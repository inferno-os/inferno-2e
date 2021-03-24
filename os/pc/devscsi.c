/*
 * Generic SCSI driver
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include 	"io.h"


enum
{
	Qdir,
	Qstatus,
	Qpage,
	Qinq,
	Qcapacity,
	Qreqsens,
	Qmodesense,
	Qmodeselect,
	Qrdbuf,
	Qread,
	Qwrite,

	Qraw,
};
Dirtab scsitab[]=
{
	"status",	{Qstatus, 0},		0,	0444,
	"page",		{Qpage, 0},		0,	0666,
	"inquire",	{Qinq, 0},		0,	0444,
	"capacity",	{Qcapacity, 0},		0,	0444,
	"reqsense",	{Qreqsens, 0},		0,	0444,
	"modesense",	{Qmodesense, 0},	0,	0444,
	"modeselect",	{Qmodeselect, 0},	0,	0222,
	"read",		{Qread, 0},		0,	0444,
	"write",	{Qwrite, 0},		0,	0222,
	"readbuffer",	{Qrdbuf, 0},		0,	0444,

	"raw",		{Qraw, 0},		0,	0666,
};

typedef struct Info Info;
struct Info
{
	Ref;
	int	lun;
	int	status;		/* Status of last command */
	int	page;		/* Page for various  */
	ulong	bsize;
	ulong	nblock;

	QLock	rawqlock;
	int	raw;
	ulong	pid;
	uchar	cmd[12];
	int	cdbs;

	Target*	t;
};

enum {
	Rawcmd,
	Rawdata,
	Rawstatus,
};

extern Target *scsiunit(int ctlr, int unit);


static Chan*
scsiattach(char *spec)
{
	Chan *c;
	Info *i;
	char *s;
	Target *t;
	int ctlr, unit, lun;

	ctlr = 0;
	unit = 0;
	lun = 0;
	s = spec;
	if(s && *s && *s >= '0' && *s <= '9')
		ctlr = *s++ - '0';
	if(s && *s && *s >= '0' && *s <= '7')
		unit = *s++ - '0';
	if(s && *s && *s >= '0' && *s <= '7')
		lun = *s - '0';

	t = scsiunit(ctlr, unit);
	if(t == 0)
		error("device not configured");

	i = malloc(sizeof(Info));
	if(i == 0)
		error(Enomem);

	i->ref = 1;
	i->lun = lun;
	i->t = t;
	i->page = 1;
	i->raw = Rawcmd;

	c = devattach('S', spec);
	c->aux = i;
	return c;
}

static Chan*
scsiclone(Chan *c, Chan *nc)
{
	Info *i;

	nc = devclone(c, nc);
	i = nc->aux;
	incref(i);
	return nc;
}

static int
scsiwalk(Chan *c, char *name)
{
	return devwalk(c, name, scsitab, nelem(scsitab), devgen);
}

static void
scsistat(Chan *c, char *db)
{
	devstat(c, db, scsitab, nelem(scsitab), devgen);
}

static Chan*
scsiopen(Chan *c, int omode)
{
	return devopen(c, omode, scsitab, nelem(scsitab), devgen);
}

static void
scsiclose(Chan *c)
{
	Info *i;

	i = c->aux;

	if((c->qid.path & ~CHDIR) == Qraw){
		if(canqlock(&i->rawqlock) || i->pid == up->pid){
			i->pid = 0;
			i->raw = Rawcmd;
			qunlock(&i->rawqlock);
		}
	}

	if(decref(i) == 0)
		free(i);
}

static int
rdcmd(Info *i, uchar *cmd, int cdbs, void *a, int n, ulong offset, int ndata)
{
	uchar *b;

	cmd[1] &= 0x1F;
	cmd[1] |= i->lun<<5;

	if(ndata >= SCSImaxxfer)
		error(Etoobig);

	b = scsialloc(ndata);
	if(b == 0)
		error(Enomem);
	if(waserror()) {
		scsifree(b);
		nexterror();
	}

	i->status = scsiexec(i->t, SCSIread, cmd, cdbs, b, &ndata);
	if(i->status != STok)
		error("scsi command failed");

	if(offset+n > ndata)
		n = ndata - offset;
	if(n <= 0)
		n = 0;
	else
		memmove(a, b+offset, n);
	scsifree(b);
	poperror();
	return n;
}

static long
scsiread(Chan *c, void *a, long n, ulong off)
{
	int nb, status;
	Info *i;
	uchar cmd[10];
	ulong offset = off;

	i = c->aux;
	memset(cmd, 0, sizeof cmd);

	switch(c->qid.path&~CHDIR) {
	case Qdir:
		return devdirread(c, a, n, scsitab, nelem(scsitab), devgen);
	case Qstatus:
		return readnum(offset, a, n, i->status, NUMSIZE);
	case Qpage:
		return readnum(offset, a, n, i->page, NUMSIZE);
	case Qinq:
		cmd[0] = 0x12;
		cmd[4] = 255;	
		return rdcmd(i, cmd, 6, a, n, offset, 255);
	case Qcapacity:
		cmd[0] = 0x25;
		return rdcmd(i, cmd, 10, a, n, offset, 8);
	case Qreqsens:
		cmd[0] = 0x03;
		cmd[4] = 160;
		return rdcmd(i, cmd, 6, a, n, offset, 160);
	case Qmodesense:
		cmd[0] = 0x5a;
		cmd[2] = i->page & 0x3f;
		cmd[7] = n>>8;
		cmd[8] = n;
		return rdcmd(i, cmd, 10, a, n, offset, n);
	case Qrdbuf:
		cmd[0] = 0x3c;
		cmd[1] = 0x02;
		cmd[2] = i->page;
		cmd[6] = n>>16;
		cmd[7] = n>>8;
		cmd[8] = n;
		return rdcmd(i, cmd, 10, a, n, offset, n);
	case Qread:
		if(i->bsize == 0) {
			if(scsicap(i->t, 0, &i->nblock, &i->bsize) != STok)
				error("getcapacity failed");
			if(i->bsize == 0 || i->nblock == 0)
				error("bad capacity info");
		}
		if(offset & (i->bsize-1))
			error(Eio);
		cmd[0] = 0x28;
		nb = off/i->bsize;
		cmd[2] = nb>>24;
		cmd[3] = nb>>16;
		cmd[4] = nb>>8;
		cmd[5] = nb;
		n = (n/i->bsize) & 0xffff;
		cmd[7] = n>>8;
		cmd[8] = n;

		nb = rdcmd(i, cmd, 10, a, n*i->bsize, 0, n*i->bsize);

		return nb;
	case Qraw:
		if(canqlock(&i->rawqlock)){
			qunlock(&i->rawqlock);
			error(Ebadusefd);
		}
		if(i->pid != up->pid)
			error(Eperm);
		if(i->raw == Rawdata){
			i->raw = Rawstatus;
			return rdcmd(i, i->cmd, i->cdbs, a, n, 0, n);
		}
		else if(i->raw == Rawstatus){
			status = i->status;
			i->pid = 0;
			i->raw = Rawcmd;
			qunlock(&i->rawqlock);
			return readnum(0, a, n, status, NUMSIZE);
		}
		break;
	default:
		n = 0;
		break;
	}
	return n;
}

static int
wrcmd(Info *i, uchar *cmd, int cdbs, void *a, int n)
{
	void *b;

	cmd[1] &= 0x1F;
	cmd[1] |= i->lun<<5;

	b = scsialloc(n);
	if(b == 0)
		error(Enomem);
	if(waserror()) {
		scsifree(b);
		nexterror();
	}
	memmove(b, a, n);
	i->status = scsiexec(i->t, SCSIwrite, cmd, cdbs, b, &n);
	if(i->status != STok)
		error("scsi command failed");

	scsifree(b);
	poperror();
	return n;
}

static long
scsiwrite(Chan *c, char *a, long n, ulong off)
{
	int nb;
	Info *i;
	uchar cmd[12];
	ulong offset = off;

	i = c->aux;
	memset(cmd, 0, sizeof cmd);

	switch(c->qid.path & ~CHDIR){
	case Qpage:
		if(n > sizeof cmd)
			n = sizeof(cmd);
		memmove(cmd, a, n);
		i->page = strtoul((char*)cmd, 0, 0);
		break;
	case Qmodeselect:
		cmd[0] = 0x55;
		cmd[7] = n>>8;
		cmd[8] = n;
		return wrcmd(i, cmd, 10, a, n);
	case Qwrite:
		if(i->bsize == 0) {
			if(scsicap(i->t, 0, &i->nblock, &i->bsize) != STok)
				error("getcapacity failed");
			if(i->bsize == 0 || i->nblock == 0)
				error("bad capacity info");
		}
		if(offset & (i->bsize-1))
			error(Eio);
		cmd[0] = 0x2A;
		nb = off/i->bsize;
		cmd[2] = nb>>24;
		cmd[3] = nb>>16;
		cmd[4] = nb>>8;
		cmd[5] = nb;
		n = (n/i->bsize) & 0xffff;
		cmd[7] = n>>8;
		cmd[8] = n;

		nb = wrcmd(i, cmd, 10, a, n*i->bsize);

		return nb;
	case Qraw:
		if(canqlock(&i->rawqlock)){
			if(i->raw != Rawcmd){
				qunlock(&i->rawqlock);
				error(Ebadusefd);
			}
			if(n < 6 || n > sizeof(i->cmd)){
				qunlock(&i->rawqlock);
				error(Ebadarg);
			}
			i->pid = up->pid;
			memmove(i->cmd, a, n);
			i->cdbs = n;
			i->status = ~0;
			i->raw = Rawdata;
		}
		else{
			if(i->pid != up->pid)
				error(Eperm);
			if(i->raw != Rawdata)
				error(Ebadusefd);
			i->raw = Rawstatus;
			return wrcmd(i, i->cmd, i->cdbs, a, n);
		}
		break;
	default:
		error(Ebadusefd);
	}
	return n;
}

extern void scsireset(void);

Dev scsidevtab = {
	'S',
	"scsi",

	scsireset,
	devinit,
	scsiattach,
	devdetach,
	scsiclone,
	scsiwalk,
	scsistat,
	scsiopen,
	devcreate,
	scsiclose,
	scsiread,
	devbread,
	scsiwrite,
	devbwrite,
	devremove,
	devwstat,
};
