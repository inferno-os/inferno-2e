/*
 * basic Flash Translation Layer driver
 *	see for instance the Intel technical paper
 *	``Understanding the Flash Translation Layer (FTL) Specification''
 *	Order number 297816-001 (online at www.intel.com)
 *
 * a public driver by David Hinds, dhinds@allegro.stanford.edu
 * further helps with some details.
 *
 * this driver uses the common simplification of never storing
 * the VBM on the medium (a waste of precious flash!) but
 * rather building it on the fly as the block maps are read.
 *
 * Plan 9 driver (c) 1997 by C H Forsyth (forsyth@caldo.demon.co.uk)
 *	This driver may be used or adapted by anyone for any non-commercial purpose.
 *
 * adapted for Inferno 1998 by C H Forsyth, Vita Nuova Limited, York, England (charles@vitanuova.com)
 *
 * C H Forsyth and Vita Nuova Limited expressly allow Lucent Technologies
 * to use this driver freely for any Inferno-related purposes whatever,
 * including commercial applications.
 *
 * TO DO:
 *	check error handling details for get/put flash
 *	bad block handling
 *	reserved space in formatted size
 *	possibly block size as parameter
 *	fetch parameters from header on init
 */

#include "u.h"
#include "../port/lib.h"
#include "../port/error.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"

#include "kernel.h"
#include "flashif.h"

#ifndef offsetof
#define offsetof(T,X) ((ulong)&(((T*)0)->X))
#endif

typedef struct Ftl Ftl;
typedef struct Merase Merase;
typedef struct Terase Terase;

enum {
	Eshift = 18,	/* 2^18=256k; log2(eraseunit) */
	Flashseg = 1<<Eshift,
	Bshift = 9,		/* 2^9=512 */
	Bsize = 1<<Bshift,
	BAMoffset = 0x100,
	Nolimit = ~0,
	USABLEPCT = 95,	/* release only this % to client */

	FTLDEBUG = 0
};

/* erase unit header (defined by FTL specification) */
struct Merase {
	uchar	linktuple[5];
	uchar	orgtuple[10];
	uchar	nxfer;
	uchar	nerase[4];
	uchar	id[2];
	uchar	bshift;
	uchar	eshift;
	uchar	pstart[2];
	uchar	nunits[2];
	uchar	psize[4];
	uchar	vbmbase[4];
	uchar	nvbm[2];
	uchar	flags;
	uchar	code;
	uchar	serial[4];
	uchar	altoffset[4];
	uchar	bamoffset[4];
	uchar	rsv2[12];
};
#define	ERASEHDRLEN	64

enum {
	/* special unit IDs */
	XferID = 0xffff,
	XferBusy = 0x7fff,

	/* special BAM addresses */
	Bfree = 0xffffffff,
	Bwriting = 0xfffffffe,
	Bdeleted = 0,

	/* block types */
	TypeShift = 7,
	BlockType = (1<<TypeShift)-1,
	ControlBlock = 0x30,
	DataBlock = 0x40,
	ReplacePage = 0x60,
	BadBlock = 0x70,
};

#define	BTYPE(b)	((b) & BlockType)
#define	BADDR(b)	((b) & ~BlockType)
#define	BNO(va)	(((ulong)(va))>>Bshift)
#define	MKBAM(b,t)	(((b)<<Bshift)|(t))

struct Terase {
	int	x;
	int	id;
	ulong	offset;
	ulong	bamoffset;
	ulong	nbam;
	ulong*	bam;
	ulong	bamx;
	ulong	nfree;
	ulong	nused;
	ulong	ndead;
	ulong	nbad;
	ulong	nerase;
};

struct Ftl {
	QLock;
	Ref;

	Chan*	flash;
	Chan*	flashctl;
	ulong	base;	/* base of flash region */
	ulong	size;	/* size of flash region */
	ulong	segsize;	/* size of flash segment (erase unit) */
	int	eshift;	/* log2(erase-unit-size) */
	int	bshift;	/* log2(bsize) */
	int	bsize;
	int	nunit;	/* number of segments (erase units) */
	Terase**	unit;
	int	lastx;	/* index in unit of last allocation */
	int	xfer;		/* index in unit of current transfer unit (-1 if none) */
	ulong	nfree;	/* total free space in blocks */
	ulong	nblock;	/* total space in blocks */
	ulong	rwlimit;	/* user-visible block limit (`formatted size') */
	ulong*	vbm;		/* virtual block map */
	ulong	fstart;	/* address of first block of data in a segment */
	int	trace;	/* (debugging) trace of read/write actions */
	int	detach;	/* free Ftl on last close */

	/* scavenging variables */
	QLock	wantq;
	Rendez	wantr;
	Rendez	workr;
	int	needspace;
	int	hasproc;
};

enum {
	/* Ftl.detach */
	Detached = 1,	/* detach on close */
	Deferred	/* scavenger must free it */
};

/* little endian */
#define	GET2(p)	(((p)[1]<<8)|(p)[0])
#define	GET4(p)	(((((((p)[3]<<8)|(p)[2])<<8)|(p)[1])<<8)|(p)[0])
#define	PUT2(p,v)	(((p)[1]=(v)>>8),((p)[0]=(v)))
#define	PUT4(p,v)	(((p)[3]=(v)>>24),((p)[2]=(v)>>16),((p)[1]=(v)>>8),((p)[0]=(v)))

static	Lock	ftllock;
static	Ftl	*ftls;

static	ulong	allocblk(Ftl*);
static	void	eraseflash(Ftl*, ulong);
static	void	erasefree(Terase*);
static	void	eraseinit(Ftl*, ulong, int, int);
static	Terase*	eraseload(Ftl*, int, ulong);
static	void	ftlfree(Ftl*);
static	void	getflash(Ftl*, void*, ulong, long);
static	int	mapblk(Ftl*, ulong, Terase**, ulong*);
static	Ftl*	mkftl(char*, ulong, ulong, int, char*);
static	void	putbam(Ftl*, Terase*, int, ulong);
static	void	putflash(Ftl*, ulong, void*, long);
static	int	scavenge(Ftl*);

enum {
	Qdir,
	Qdata,
	Qctl,
};

static	Dirtab	ftldir[] = {
	"ftldata",	{Qdata, 0},	0,	0666,
	"ftlctl",	{Qctl, 0},	0,	0666,
};
#define	NFTL	(sizeof(ftldir)/sizeof(ftldir[0]))

static Ftl *
ftlget(void)
{
	Ftl *ftl;

	lock(&ftllock);
	ftl = ftls;
	if(ftl != nil)
		incref(ftl);
	unlock(&ftllock);
	return ftl;
}

static void
ftlput(Ftl *ftl)
{
	if(ftl != nil){
		lock(&ftllock);
		if(decref(ftl) == 0 && ftl->detach == Detached){
			ftls = nil;
			if(ftl->hasproc){	/* no lock needed: can't change if ftl->ref==0 */
				ftl->detach = Deferred;
				wakeup(&ftl->workr);
			}else
				ftlfree(ftl);
		}
		unlock(&ftllock);
	}
}

static Chan *
ftlattach(char *spec)
{
	return devattach('X', spec);
}

static int	 
ftlwalk(Chan *c, char *name)
{
	return devwalk(c, name, ftldir, NFTL, devgen);
}

static void	 
ftlstat(Chan *c, char *dp)
{
	if(ftls)
		ftldir[Qdata-1].length = ftls->rwlimit*Bsize;
	devstat(c, dp, ftldir, NFTL, devgen);
}

static Chan*
ftlopen(Chan *c, int omode)
{
	Ftl *ftl;

	omode = openmode(omode);
	switch(c->qid.path){
	case Qdata:
		ftl = ftls;
		if(ftl == nil)
			error(Enodev);
		if(strcmp(up->env->user, eve)!=0)
			error(Eperm);
		break;
	case Qctl:
		if(strcmp(up->env->user, eve)!=0)
			error(Eperm);
		break;
	}
	c = devopen(c, omode, ftldir, NFTL, devgen);
	if(c->qid.path == Qdata){
		c->aux = ftlget();
		if(c->aux == nil)
			error(Enodev);
	}
	return c;
}

static Chan*
ftlclone(Chan *c, Chan *nc)
{
	nc = devclone(c, nc);
	if(c->qid.path == Qdata)
		nc->aux = ftlget();
	return nc;
}

static void	 
ftlclose(Chan *c)
{
	if(c->qid.path == Qdata && (c->flag&COPEN) != 0)
		ftlput((Ftl*)c->aux);
}

static long	 
ftlread(Chan *c, void *buf, long n, ulong offset)
{
	Ftl *ftl;
	Terase *e;
	int nb;
	uchar *a;
	ulong pb;

	if(c->qid.path & CHDIR)
		return devdirread(c, buf, n, ftldir, NFTL, devgen);

	switch(c->qid.path){
	case Qctl:
		return 0;
	case Qdata:
		if(n <= 0 || n%Bsize || offset%Bsize)
			error(Eio);
		ftl = c->aux;
		nb = n/Bsize;
		offset /= Bsize;
		if(offset >= ftl->rwlimit)
			return 0;
		if(offset+nb > ftl->rwlimit)
			nb = ftl->rwlimit - offset;
		a = buf;
		for(n = 0; n < nb; n++){
			qlock(ftl);
			if(waserror()){
				qunlock(ftl);
				nexterror();
			}
			if(mapblk(ftl, offset+n, &e, &pb))
				getflash(ftl, a, e->offset + pb*Bsize, Bsize);
			else
				memset(a, 0, Bsize);
			poperror();
			qunlock(ftl);
			a += Bsize;
		}
		return a-(uchar*)buf;
	}
	error(Egreg);
	return 0;		/* not reached */
}

static long	 
ftlwrite(Chan *c, void *buf, long n, ulong offset)
{
	char cmd[64], *fields[6];
	int ns, i, k, nb;
	uchar *a;
	Terase *e, *oe;
	ulong ob, v, base, size, segsize;
	Ftl *ftl;

	if(n <= 0)
		return 0;
	switch(c->qid.path){
	case Qdata:
		ftl = c->aux;
		if(n <= 0 || n%Bsize || offset%Bsize)
			error(Eio);
		nb = n/Bsize;
		offset /= Bsize;
		if(offset >= ftl->rwlimit)
			return 0;
		if(offset+nb > ftl->rwlimit)
			nb = ftl->rwlimit - offset;
		a = buf;
		for(n = 0; n < nb; n++){
			ns = 0;
			while((v = allocblk(ftl)) == 0)
				if(!scavenge(ftl) || ++ns > 3){
					print("ftl: flash memory full\n");
					error("flash memory full");
				}
			qlock(ftl);
			if(waserror()){
				qunlock(ftl);
				nexterror();
			}
			if(!mapblk(ftl, offset+n, &oe, &ob))
				oe = nil;
			e = ftl->unit[v>>16];
			v &= 0xffff;
			putflash(ftl, e->offset + v*Bsize, a, Bsize);
			putbam(ftl, e, v, MKBAM(offset+n, DataBlock));
			/* both old and new block references exist in this window (can't be closed?) */
			ftl->vbm[offset+n] = (e->x<<16) | v;
			if(oe != nil){
				putbam(ftl, oe, ob, Bdeleted);
				oe->ndead++;
			}
			poperror();
			qunlock(ftl);
			a += Bsize;
		}
		return a-(uchar*)buf;
	case Qctl:
		if(n > sizeof(cmd)-1)
			n = sizeof(cmd)-1;
		memmove(cmd, buf, n);
		cmd[n] = 0;
		i = parsefields(cmd, fields, 6, " \t\n");
		if(i <= 0)
			error(Ebadarg);
		if(i >= 2 && (strcmp(fields[0], "init") == 0 || strcmp(fields[0], "format") == 0)){
			if(i > 2)
				base = strtoul(fields[2], nil, 0);
			else
				base = 1024*1024;	/* TO DO: hunt for signature */
			if(i > 3)
				size = strtoul(fields[3], nil, 0);
			else
				size = Nolimit;
			if(i > 4)
				segsize = strtoul(fields[4], nil, 0);
			else
				segsize = Flashseg;
			/* segsize must be non-zero power of two and size and base must be multiples of it */
			if(segsize == 0 || segsize > size || segsize&(segsize-1) || base&(segsize-1) || size == 0 || size != Nolimit && size&(segsize-1))
				error(Ebadarg);
			for(k=0; k<32 && (1<<k) != segsize; k++)
				;
			if(ftls != nil)
				error(Einuse);
			ftls = mkftl(fields[1], base, size, k, fields[0]);
		}else if(strcmp(fields[0], "scavenge") == 0){
			if(ftls != nil)
				print("scavenge %d\n", scavenge(ftls));
		}else if(strcmp(fields[0], "trace") == 0){
			if(ftls != nil)
				ftls->trace = i>1? strtol(fields[1], nil, 0): 1;
		}else if(strcmp(fields[0], "detach") == 0){
			if((ftl = ftlget()) != nil){
				if(ftl->ref > 1){
					ftlput(ftl);
					error(Einuse);
				}
				ftl->detach = Detached;
				ftlput(ftl);
			}else
				error(Enodev);
		}else
			error(Ebadarg);
		return n;
	}
	error(Egreg);
	return 0;		/* not reached */
}

static Chan *
ftlkopen(char *name, char *suffix)
{
	Chan *c;
	char *fn;
	int fd;

	if(suffix != nil && *suffix){
		fn = smalloc(strlen(name)+strlen(suffix)+1);
		if(fn == nil)
			return nil;
		strcpy(fn, name);
		strcat(fn, suffix);
		fd = kopen(fn, ORDWR);
		free(fn);
	}else
		fd = kopen(name, ORDWR);
	if(fd < 0)
		return nil;
	c = fdtochan(up->env->fgrp, fd, ORDWR, 0, 1);
	kclose(fd);
	return c;
}

static ulong
ftlfsize(Chan *c)
{
	char dbuf[DIRLEN];
	Dir d;

	devtab[c->type]->stat(c, dbuf);
	convM2D(dbuf, &d);
	return d.length;
}

static Ftl *
mkftl(char *fname, ulong base, ulong size, int eshift, char *op)
{
	int i, j, nov, segblocks;
	ulong limit;
	Terase *e;
	Ftl *ftl;

	ftl = malloc(sizeof(*ftl));
	if(ftl == nil)
		error(Enomem);
	if(waserror()){
		ftlfree(ftl);
		nexterror();
	}
	ftl->flash = ftlkopen(fname, "");
	if(ftl->flash == nil)
		error(up->env->error);
	ftl->flashctl = ftlkopen(fname, "ctl");
	if(ftl->flashctl == nil)
		error(up->env->error);
	limit = ftlfsize(ftl->flash);
	if(limit == 0)
		error("no space for flash translation");
	if(size == Nolimit)
		size = limit-base;
	if(base >= limit || size > limit || base+size > limit || eshift < 8 || (1<<eshift) > size)
		error("bad flash space parameters");
	if(FTLDEBUG)
		print("%s flash %s #%lux:#%lux limit #%lux\n", op, fname, base, size, limit);
	ftl->base = base;
	ftl->size = size;
	ftl->bshift = Bshift;
	ftl->bsize = Bsize;
	ftl->eshift = eshift;
	ftl->segsize = 1<<eshift;
	ftl->nunit = size>>eshift;
	nov = ((ftl->segsize/Bsize)*4 + BAMoffset + Bsize - 1)/Bsize;	/* number of overhead blocks per segment (header, and BAM itself) */
	ftl->fstart = nov;
	segblocks = ftl->segsize/Bsize - nov;
	ftl->nblock = ftl->nunit*segblocks;
	if(ftl->nblock >= 0x10000)
		ftl->nblock = 0x10000;
	ftl->vbm = malloc(ftl->nblock*sizeof(*ftl->vbm));
	ftl->unit = malloc(ftl->nunit*sizeof(*ftl->unit));
	if(ftl->vbm == nil || ftl->unit == nil)
		error(Enomem);
	if(strcmp(op, "format") == 0){
		for(i=0; i<ftl->nunit-1; i++)
			eraseinit(ftl, i*ftl->segsize, i, 1);
		eraseinit(ftl, i*ftl->segsize, XferID, 1);
	}
	ftl->xfer = -1;
	for(i=0; i<ftl->nunit; i++){
		e = eraseload(ftl, i, i*ftl->segsize);
		if(e == nil){
			print("ftl: logical segment %d: bad format\n", i);
			continue;
		}
		if(e->id == XferBusy){
			e->nerase++;
			eraseinit(ftl, e->offset, XferID, e->nerase);
			e->id = XferID;
		}
		for(j=0; j<ftl->nunit; j++)
			if(ftl->unit[j] != nil && ftl->unit[j]->id == e->id){
				print("ftl: duplicate erase unit #%x\n", e->id);
				erasefree(e);
				e = nil;
				break;
			}
		if(e){
			ftl->unit[e->x] = e;
			if(e->id == XferID)
				ftl->xfer = e->x;
			print("ftl: unit %d:#%x used %lud free %lud dead %lud bad %lud nerase %lud\n",
				e->x, e->id, e->nused, e->nfree, e->ndead, e->nbad, e->nerase);
		}
	}
	if(ftl->xfer < 0 && ftl->nunit <= 0 || ftl->xfer >= 0 && ftl->nunit <= 1)
		error("no valid flash data units");
	if(ftl->xfer < 0)
		print("ftl: no transfer unit: device is WORM\n");
	else
		ftl->nblock -= segblocks;	/* discount transfer segment */
	if(ftl->nblock >= 1000)
		ftl->rwlimit = ftl->nblock-100;	/* TO DO: variable reserve */
	else
		ftl->rwlimit = ftl->nblock*USABLEPCT/100;
	poperror();
	return ftl;
}

static void
ftlfree(Ftl *ftl)
{
	if(ftl != nil){
		if(ftl->flashctl != nil)
			cclose(ftl->flashctl);
		if(ftl->flash != nil)
			cclose(ftl->flash);
		free(ftl->unit);
		free(ftl->vbm);
		free(ftl);
	}
}

/*
 * this simple greedy algorithm weighted by nerase does seem to lead
 * to even wear of erase units (cf. the eNVy file system)
 */
static Terase *
bestcopy(Ftl *ftl)
{
	Terase *e, *be;
	int i;

	be = nil;
	for(i=0; i<ftl->nunit; i++)
		if((e = ftl->unit[i]) != nil && e->id != XferID && e->id != XferBusy && e->ndead+e->nbad &&
		    (be == nil || e->nerase <= be->nerase && e->ndead >= be->ndead))
			be = e;
	return be;
}

static int
copyunit(Ftl *ftl, Terase *from, Terase *to)
{
	int i, nb;
	uchar id[2];
	ulong *bam;
	uchar *buf;
	ulong v, bno;

	if(FTLDEBUG || ftl->trace)
		print("ftl: copying %d (#%lux) to #%lux\n", from->id, from->offset, to->offset);
	to->nbam = 0;
	free(to->bam);
	to->bam = nil;
	bam = nil;
	buf = malloc(Bsize);
	if(buf == nil)
		return 0;
	if(waserror()){
		free(buf);
		free(bam);
		return 0;
	}
	PUT2(id, XferBusy);
	putflash(ftl, to->offset+offsetof(Merase,id[0]), id, 2);
	/* make new BAM */
	nb = from->nbam*sizeof(*to->bam);
	bam = malloc(nb);
	if(bam == nil)
		error(Enomem);
	memmove(bam, from->bam, nb);
	to->nused = 0;
	to->nbad = 0;
	to->nfree = 0;
	to->ndead = 0;
	for(i = 0; i < from->nbam; i++)
		switch(bam[i]){
		case Bwriting:
		case Bdeleted:
		case Bfree:
			bam[i] = Bfree;
			to->nfree++;
			break;
		default:
			switch(bam[i]&BlockType){
			default:
			case BadBlock:	/* it isn't necessarily bad in this unit */
				to->nfree++;
				bam[i] = Bfree;
				break;
			case DataBlock:
			case ReplacePage:
				v = bam[i];
				bno = BNO(v & ~BlockType);
				if(i < ftl->fstart || bno >= ftl->nblock){
					print("ftl: unit %d:#%x bad bam[%d]=#%lux\n", from->x, from->id, i, v);
					to->nfree++;
					bam[i] = Bfree;
					break;
				}
				getflash(ftl, buf, from->offset+i*Bsize, Bsize);
				putflash(ftl, to->offset+i*Bsize, buf, Bsize);
				to->nused++;
				break;
			case ControlBlock:
				to->nused++;
				break;
			}
		}
	for(i=0; i<from->nbam; i++){
		uchar *p = (uchar*)&bam[i];
		v = bam[i];
		if(v != Bfree && ftl->trace > 1)
			print("to[%d]=#%lux\n", i, v);
		PUT4(p, v);
	}
	putflash(ftl, to->bamoffset, bam, nb);	/* BUG: PUT4 */
	for(i=0; i<from->nbam; i++){
		uchar *p = (uchar*)&bam[i];
		v = bam[i];
		PUT4(p, v);
	}
	to->id = from->id;
	PUT2(id, to->id);
	putflash(ftl, to->offset+offsetof(Merase,id[0]), id, 2);
	to->nbam = from->nbam;
	to->bam = bam;
	ftl->nfree += to->nfree - from->nfree;
	poperror();
	free(buf);
	return 1;
}

static int
mustscavenge(void *a)
{
	return ((Ftl*)a)->needspace || ((Ftl*)a)->detach == Deferred;
}

static int
donescavenge(void *a)
{
	return ((Ftl*)a)->needspace == 0;
}

static void
scavengeproc(void *arg)
{
	Ftl *ftl;
	int i;
	Terase *e, *ne;

	ftl = arg;
	if(waserror()){
		print("ftl: kproc noted\n");
		pexit("ftldeath", 0);
	}
	for(;;){
		sleep(&ftl->workr, mustscavenge, ftl);
		if(ftl->detach == Deferred){
			ftlfree(ftl);
			pexit("", 0);
		}
		if(FTLDEBUG || ftl->trace)
			print("ftl: scavenge %ld\n", ftl->nfree);
		qlock(ftl);
		if(waserror()){
			qunlock(ftl);
			nexterror();
		}
		e = bestcopy(ftl);
		if(e == nil || ftl->xfer < 0 || (ne = ftl->unit[ftl->xfer]) == nil || ne->id != XferID || e == ne)
			goto Fail;
		if(copyunit(ftl, e, ne)){
			i = ne->x; ne->x = e->x; e->x = i;
			ftl->unit[ne->x] = ne;
			ftl->unit[e->x] = e;
			ftl->xfer = e->x;
			e->id = XferID;
			e->nbam = 0;
			free(e->bam);
			e->bam = nil;
			e->bamx = 0;
			e->nerase++;
			eraseinit(ftl, e->offset, XferID, e->nerase);
		}
	Fail:
		if(FTLDEBUG || ftl->trace)
			print("ftl: end scavenge %ld\n", ftl->nfree);
		ftl->needspace = 0;
		wakeup(&ftl->wantr);
		poperror();
		qunlock(ftl);
	}
}

static int
scavenge(Ftl *ftl)
{
	if(ftl->xfer < 0 || bestcopy(ftl) == nil)
		return 0;	/* you worm! */

	qlock(ftl);
	if(waserror()){
		qunlock(ftl);
		return 0;
	}
	if(!ftl->hasproc){
		ftl->hasproc = 1;
		kproc("ftl.scavenge", scavengeproc, ftl);
	}
	ftl->needspace = 1;
	wakeup(&ftl->workr);
	poperror();
	qunlock(ftl);

	qlock(&ftl->wantq);
	if(waserror()){
		qunlock(&ftl->wantq);
		nexterror();
	}
	while(ftl->needspace)
		sleep(&ftl->wantr, donescavenge, ftl);
	poperror();
	qunlock(&ftl->wantq);

	return ftl->nfree;
}

static void
putbam(Ftl *ftl, Terase *e, int n, ulong entry)
{
	uchar b[4];

	e->bam[n] = entry;
	PUT4(b, entry);
	putflash(ftl, e->bamoffset + n*4, b, 4);
}

static ulong
allocblk(Ftl *ftl)
{
	Terase *e;
	int i, j;

	qlock(ftl);
	i = ftl->lastx;
	do{
		e = ftl->unit[i];
		if(e != nil && e->id != XferID && e->nfree){
			ftl->lastx = i;
			for(j=e->bamx; j<e->nbam; j++)
				if(e->bam[j] == Bfree){
					putbam(ftl, e, j, Bwriting);
					ftl->nfree--;
					e->nfree--;
					e->bamx = j+1;
					qunlock(ftl);
					return (e->x<<16) | j;
				}
			e->nfree = 0;
			qunlock(ftl);
			print("ftl: unit %d:#%x nfree %ld but not free in BAM\n", e->x, e->id, e->nfree);
			qlock(ftl);
		}
		if(++i >= ftl->nunit)
			i = 0;
	}while(i != ftl->lastx);
	qunlock(ftl);
	return 0;
}

static int
mapblk(Ftl *ftl, ulong bno, Terase **ep, ulong *bp)
{
	ulong v;
	int x;

	if(bno < ftl->nblock){
		v = ftl->vbm[bno];
		if(v == 0 || v == ~0)
			return 0;
		x = v>>16;
		if(x >= ftl->nunit || x == ftl->xfer || ftl->unit[x] == nil){
			print("ftl: corrupt format: bad block mapping %lud -> unit #%x\n", bno, x);
			return 0;
		}
		*ep = ftl->unit[x];
		*bp = v & 0xFFFF;
		return 1;
	}
	return 0;
}

static void
eraseinit(Ftl *ftl, ulong offset, int id, int nerase)
{
	union {
		Merase;
		uchar	block[ERASEHDRLEN];
	} *m;
	uchar *bam, *p;
	int i, nov;

	nov = ((ftl->segsize/Bsize)*4 + BAMoffset + Bsize - 1)/Bsize;	/* number of overhead blocks (header, and BAM itself) */
	if(nov*Bsize >= ftl->segsize)
		error("ftl -- too small for files");
	eraseflash(ftl, offset);
	m = malloc(sizeof(*m));
	if(m == nil)
		error(Enomem);
	memset(m, 0xFF, sizeof(*m));
	m->linktuple[0] = 0x13;
	m->linktuple[1] = 0x3;
	memmove(m->linktuple+2, "CIS", 3);
	m->orgtuple[0] = 0x46;
	m->orgtuple[1] = 0x57;
	m->orgtuple[2] = 0x00;
	memmove(m->orgtuple+3, "FTL100", 7);
	m->nxfer = 1;
	PUT4(m->nerase, nerase);
	PUT2(m->id, id);
	m->bshift = ftl->bshift;
	m->eshift = ftl->eshift;
	PUT2(m->pstart, 0);
	PUT2(m->nunits, ftl->nunit);
	PUT4(m->psize, ftl->size - nov*Bsize);
	PUT4(m->vbmbase, 0xffffffff);	/* we always calculate the VBM */
	PUT2(m->nvbm, 0);
	m->flags = 0;
	m->code = 0xFF;
	memmove(m->serial, "Inf1", 4);
	PUT4(m->altoffset, 0);
	PUT4(m->bamoffset, BAMoffset);
	putflash(ftl, offset, m, ERASEHDRLEN);
	free(m);
	if(id == XferID)
		return;
	nov *= 4;	/* now bytes of BAM */
	bam = malloc(nov);
	if(bam == nil)
		error(Enomem);
	for(i=0; i<nov; i += 4){
		p = bam+i;
		PUT4(p, ControlBlock);	/* reserve them */
	}
	putflash(ftl, offset+BAMoffset, bam, nov);
	free(bam);
}

static Terase *
eraseload(Ftl *ftl, int x, ulong offset)
{
	union {
		Merase;
		uchar	block[ERASEHDRLEN];
	} *m;
	Terase *e;
	uchar *p;
	int i, nbam;
	ulong bno, v;

	m = malloc(sizeof(*m));
	if(m == nil)
		error(Enomem);
	getflash(ftl, m, offset, ERASEHDRLEN);
	if(memcmp(m->orgtuple+3, "FTL100", 7) != 0 ||
	   memcmp(m->serial, "Inf1", 4) != 0){
		free(m);
		return nil;
	}
	e = malloc(sizeof(*e));
	if(e == nil){
		free(m);
		error(Enomem);
	}
	e->x = x;
	e->id = GET2(m->id);
	e->offset = offset;
	e->bamoffset = GET4(m->bamoffset);
	e->nerase = GET4(m->nerase);
	free(m);
	if(e->bamoffset != BAMoffset){
		free(e);
		return nil;
	}
	e->bamoffset += offset;
	if(e->id == XferID || e->id == XferBusy){
		e->bam = nil;
		e->nbam = 0;
		return e;
	}
	nbam = ftl->segsize/Bsize;
	e->bam = malloc(nbam*sizeof(*e->bam));
	e->nbam = nbam;
	getflash(ftl, e->bam, e->bamoffset, nbam*4);
	/* scan BAM to build VBM */
	e->bamx = 0;
	for(i=0; i<nbam; i++){
		p = (uchar*)&e->bam[i];
		e->bam[i] = v = GET4(p);
		if(v == Bwriting || v == Bdeleted)
			e->ndead++;
		else if(v == Bfree){
			if(e->bamx == 0)
				e->bamx = i;
			e->nfree++;
			ftl->nfree++;
		}else{
			switch(v & BlockType){
			case ControlBlock:
				break;
			case DataBlock:
				/* add to VBM */
				if(v & (1<<31))
					break;	/* negative => VBM page, ignored */
				bno = BNO(v & ~BlockType);
				if(i < ftl->fstart || bno >= ftl->nblock){
					print("ftl: unit %d:#%x bad bam[%d]=#%lux\n", e->x, e->id, i, v);
					e->nbad++;
					break;
				}
				ftl->vbm[bno] = (e->x<<16) | i;
				e->nused++;
				break;
			case ReplacePage:
				/* replacement VBM page; ignored */
				break;
			default:
				print("ftl: unit %d:#%x bad bam[%d]=%lux\n", e->x, e->id, i, v);
			case BadBlock:
				e->nbad++;
				break;
			}
		}
	}
	return e;
}

static void
erasefree(Terase *e)
{
	free(e->bam);
	free(e);
}

static void
eraseflash(Ftl *ftl, ulong offset)
{
	char cmd[40];

	offset += ftl->base;
	if(FTLDEBUG || ftl->trace)
		print("ftl: erase seg @#%lux\n", offset);
	snprint(cmd, sizeof(cmd), "erase 0x%8.8lux", offset);
	if(kchanio(ftl->flashctl, cmd, strlen(cmd), OWRITE) <= 0){
		print("ftl: erase failed: %s\n", up->env->error);
		error(up->env->error);
	}
}

static void
putflash(Ftl *ftl, ulong offset, void *buf, long n)
{
	offset += ftl->base;
	if(ftl->trace)
		print("ftl: write(#%lux, %ld)\n", offset, n);
	ftl->flash->offset = offset;
	if(kchanio(ftl->flash, buf, n, OWRITE) != n){
		print("ftl: flash write error: %s\n", up->env->error);
		error(up->env->error);
	}
}

static void
getflash(Ftl *ftl, void *buf, ulong offset, long n)
{
	offset += ftl->base;
	if(ftl->trace)
		print("ftl: read(#%lux, %ld)\n", offset, n);
	ftl->flash->offset = offset;
	if(kchanio(ftl->flash, buf, n, OREAD) != n){
		print("ftl: flash read error %s\n", up->env->error);
		error(up->env->error);
	}
}

Dev ftldevtab = {
	'X',	/* TO DO */
	"ftl",

	devreset,
	devinit,
	ftlattach,
	devdetach,
	ftlclone,
	ftlwalk,
	ftlstat,
	ftlopen,
	devcreate,
	ftlclose,
	ftlread,
	devbread,
	ftlwrite,
	devbwrite,
	devremove,
	devwstat,
};
