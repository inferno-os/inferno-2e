#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"interp.h"
#include	<isa.h>
#include	"runt.h"

typedef struct SrvFile SrvFile;
struct SrvFile
{
	char		name[NAMELEN];
	char		user[NAMELEN];
	ulong		perm;
	Qid		qid;
	int		ref;
	int		opens;
	int		flags;
	Channel*	read;
	Channel*	write;
	SrvFile*	entry;
	SrvFile*	dir;
};

enum
{
	SORCLOSE	= (1<<0),
	SRDCLOSE	= (1<<1),
	SWRCLOSE	= (1<<2),
	SREMOVED	= (1<<3),
};

typedef struct SrvDev SrvDev;
struct SrvDev
{
	Type*		Rread;
	Type*		Rwrite;
	QLock		l;
	Qid		qid;
};

static SrvDev dev;

void	freechan(Heap*, int);
void	freerdchan(Heap*, int);
void	freewrchan(Heap*, int);

Type	*Trdchan;
Type	*Twrchan;

int
srvgen(Chan *c, Dirtab *tab, int ntab, int s, Dir *dp)
{
	SrvFile *f;

	USED(tab);
	USED(ntab);

	f = c->u.aux;
	if((c->qid.path & CHDIR) == 0) {
		if(s > 0)
			return -1;
		devdir(c, f->qid, f->name, 0, f->user, f->perm, dp);
		return 1;
	}

	for(f = f->entry; f != nil; f = f->entry) {
		if(s-- == 0)
			break;
	}
	if(f == nil)
		return -1;

	devdir(c, f->qid, f->name, 0, f->user, f->perm, dp);
	return 1;
}

void
srvinit(void)
{
	static uchar rmap[] = Sys_Rread_map;
	static uchar wmap[] = Sys_Rwrite_map;

	Trdchan = dtype(freerdchan, sizeof(Channel), Tchannel.map, Tchannel.np);
	Twrchan = dtype(freewrchan, sizeof(Channel), Tchannel.map, Tchannel.np);

	dev.qid.path = 1;
	dev.Rread = dtype(freeheap, Sys_Rread_size, rmap, sizeof(rmap));
	dev.Rwrite = dtype(freeheap, Sys_Rwrite_size, wmap, sizeof(wmap));
}

Chan*
srvattach(void *spec)
{
	Chan *c;
	SrvFile *d;

	d = malloc(sizeof(SrvFile));
	if(d == nil)
		error(Enomem);

	c = devattach('s', spec);

	qlock(&dev.l);
	d->ref = 1;
	snprint(d->name, NAMELEN, "srv%d", up->env->pgrp->pgrpid);
	strncpy(d->user, up->env->user, NAMELEN);
	d->perm = 0700;
	d->qid = dev.qid;
	dev.qid.path++;
	qunlock(&dev.l);

	c->u.aux = d;
	d->qid.path |= CHDIR;
	c->qid = d->qid;

	return c;
}

Chan*
srvclone(Chan *c, Chan *nc)
{
	SrvFile *d;

	d = c->u.aux;
	c = devclone(c, nc);
	qlock(&dev.l);
	d->ref++;
	qunlock(&dev.l);
	
	return c;
}

int
srvwalk(Chan *c, char *name)
{
	SrvFile *d, *pd;

	pd = c->u.aux;
	if(devwalk(c, name, 0, 0, srvgen) == 0)
		return 0;

	qlock(&dev.l);
	pd->ref--;
	for(d = pd->entry; d != nil; d = d->entry) {
		if(d->qid.path == c->qid.path) {
			c->u.aux = d;
			d->ref++;
			qunlock(&dev.l);
			return 1;
		}
	}

	panic("srvwalk");
	return 0;
}

void
srvstat(Chan *c, char *db)
{
	devstat(c, db, 0, 0, srvgen);
}

Chan*
srvopen(Chan *c, int omode)
{
	SrvFile *sf;

	if(c->qid.path & CHDIR) {
		if(omode != OREAD)
			error(Eisdir);
		c->mode = omode;
		c->flag |= COPEN;
		c->offset = 0;
		return c;
	}

	sf = c->u.aux;
	if(omode&ORCLOSE)
		sf->flags |= SORCLOSE;

	qlock(&dev.l);
	sf->opens++;
	qunlock(&dev.l);

	c->offset = 0;
	c->flag |= COPEN;
	c->mode = openmode(omode);

	return c;
}

void
srvwstat(Chan *c, char *dp)
{
	Dir d;
	SrvFile *sf;

	sf = c->u.aux;
	if(strcmp(up->env->user, sf->user) != 0)
		error(Eperm);

	convM2D(dp, &d);
	strncpy(sf->name, d.name, sizeof(sf->name));
	d.mode &= 0777;
	sf->perm = d.mode;
}

static void
srvunblock(SrvFile *sf, int fid)
{
	Channel *d;
	Sys_FileIO_read rreq;
	Sys_FileIO_write wreq;

	acquire();
	d = sf->read;
	if(d != H) {
		rreq.t0 = 0;
		rreq.t1 = 0;
		rreq.t2 = fid;
		rreq.t3 = H;
		csendq(d, &rreq, d->mid.t, -1);
	}

	d = sf->write;
	if(d != H) {
		wreq.t0 = 0;
		wreq.t1 = H;
		wreq.t2 = fid;
		wreq.t3 = H;
		csendq(d, &wreq, d->mid.t, -1);
	}
	release();
}

void
srvdecr(SrvFile *sf, int remove)
{
	SrvFile *f, **l;

	if(remove) {
		l = &sf->dir->entry;
		for(f = *l; f != nil; f = f->entry) {
			if(sf == f) {
				*l = f->entry;
				break;
			}
			l = &f->entry;
		}
		sf->ref--;
		sf->flags |= SREMOVED;
	}

	if(sf->ref == 0) {
		if(sf->dir != nil && sf->dir->ref-- == 1)
			free(sf->dir);
		free(sf);
	}
}

void
srvfree(SrvFile *sf, int flag)
{
	sf->flags |= flag;
	if((sf->flags & (SRDCLOSE | SWRCLOSE)) == (SRDCLOSE | SWRCLOSE)) {
		sf->ref--;
		srvdecr(sf, (sf->flags & SREMOVED) == 0);
	}
}

void
freerdchan(Heap *h, int swept)
{
	SrvFile *sf;

	release();
	qlock(&dev.l);
	sf = H2D(Channel*, h)->aux;
	sf->read = H;
	srvfree(sf, SRDCLOSE);
	qunlock(&dev.l);
	acquire();
	freechan(h, swept);
}

void
freewrchan(Heap *h, int swept)
{
	SrvFile *sf;

	release();
	qlock(&dev.l);
	sf = H2D(Channel*, h)->aux;
	sf->write = H;
	srvfree(sf, SWRCLOSE);
	qunlock(&dev.l);
	acquire();
	freechan(h, swept);
}

void
srvclunk(Chan *c, int remove)
{
	int opens;
	SrvFile *sf;

	sf = c->u.aux;
	qlock(&dev.l);
	sf->ref--;
	if(c->qid.path & CHDIR) {
		if(sf->ref == 0)
			free(sf);
		qunlock(&dev.l);
		return;
	}

	opens = 0;
	if(c->flag & COPEN)
		opens = sf->opens--;

	if(opens == 1) {
		if (sf->read != H || sf->write != H)
			srvunblock(sf, c->fid);
		if((sf->flags & (SORCLOSE | SREMOVED)) == SORCLOSE)
			remove = 1;
	}

	srvdecr(sf, remove);
	qunlock(&dev.l);
}

void
srvclose(Chan *c)
{
	srvclunk(c, 0);
}

void
srvremove(Chan *c)
{
	srvclunk(c, 1);
}

long
srvread(Chan *c, void *va, long count, ulong offset)
{
	int l;
	volatile struct {Heap *h;} h;
	Array *a;
	SrvFile *sp;
	Channel *rc;
	Channel *rd;
	volatile struct {Sys_Rread *rep;} r;
	char buf[ERRLEN];
	Sys_FileIO_read req;

	if(c->qid.path & CHDIR)
		return devdirread(c, va, count, 0, 0, srvgen);

	sp = c->u.aux;

	acquire();
	rd = sp->read;
	if(rd == H) {
		release();
		error(Eshutdown);
	}

	rc = cnewc(movtmp);
	rc->mid.t = dev.Rread;
	dev.Rread->ref++;

	req.t0 = offset;
	req.t1 = count;
	req.t2 = c->fid;
	req.t3 = rc;

	csendq(rd, &req, rd->mid.t, -1);

	h.h = heap(dev.Rread);
	ptradd(h.h);
	r.rep = H2D(Sys_Rread *, h.h);
	if (waserror()) {
		ptrdel(h.h);
		destroy(r.rep);
		release();
		nexterror();
	}
	crecv(rc, r.rep);

	if(r.rep->t1 != H) {
		strncpy(buf, string2c(r.rep->t1), sizeof(buf));
		error(buf);
	}
	poperror();
	ptrdel(h.h);
	a = r.rep->t0;
	l = 0;
	if(a != H) {
		l = a->len;
		if(l > count)
			l = count;
		memmove(va, a->data, l);
	}
	destroy(r.rep);
	release();
	return l;
}

long
srvwrite(Chan *c, void *va, long count, ulong offset)
{
	long l;
	volatile struct {Heap *h;} h;
	SrvFile *sp;
	Channel *wc;
	Channel *wr;
	volatile struct {Sys_Rwrite *rep;} w;
	char buf[ERRLEN];
	Sys_FileIO_write req;

	if(c->qid.path & CHDIR)
		error(Eperm);

	sp = c->u.aux;

	acquire();
	wr = sp->write;
	if(wr == H) {
		release();
		error(Eshutdown);
	}

	wc = cnewc(movtmp);
	wc->mid.t = dev.Rwrite;
	dev.Rwrite->ref++;
	
	req.t0 = offset;
	req.t1 = mem2array(va, count);
	req.t2 = c->fid;
	req.t3 = wc;

	csendq(wr, &req, wr->mid.t, -1);

	h.h = heap(dev.Rwrite);
	ptradd(h.h);
	w.rep = H2D(Sys_Rwrite *, h.h);
	if (waserror()) {
		ptrdel(h.h);
		destroy(w.rep);
		release();
		nexterror();
	}
	crecv(wc, w.rep);

	if(w.rep->t1 != H) {
		strncpy(buf, string2c(w.rep->t1), sizeof(buf));
		error(buf);
	}
	poperror();
	ptrdel(h.h);
	l = w.rep->t0;
	destroy(w.rep);
	release();
	return l;
}

void
srvretype(Channel *c, SrvFile *f, Type *t)
{
	Heap *h;

	h = D2H(c);
	h->t->ref--;
	h->t = t;
	t->ref++;
	c->aux = f;
}

int
srvf2c(char *dir, char *file, Sys_FileIO *io)
{
	SrvFile *s, *f;
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()) {
		cclose(c.c);
		return -1;
	}

	c.c = namec(dir, Aaccess, 0, 0);
	if((c.c->qid.path&CHDIR) == 0 || devtab[c.c->type]->dc != 's')
		error("directory not a srv device");

	s = c.c->u.aux;

	qlock(&dev.l);
	for(f = s->entry; f != nil; f = f->entry) {
		if(strcmp(f->name, file) == 0) {
			qunlock(&dev.l);
			error("file exists");
		}
	}

	f = malloc(sizeof(SrvFile));
	if(f == nil) {
		qunlock(&dev.l);
		error(Enomem);
	}

	srvretype(io->read, f, Trdchan);
	srvretype(io->write, f, Twrchan);
	f->read = io->read;
	f->write = io->write;

	strncpy(f->name, file, NAMELEN);
	strncpy(f->user, up->env->user, NAMELEN);
	f->perm = 0600;
	f->ref = 2;
	f->qid = dev.qid;
	dev.qid.path++;

	f->entry = s->entry;
	s->entry = f;
	s->ref++;
	f->dir = s;
	qunlock(&dev.l);

	cclose(c.c);
	poperror();

	return 0;
}

Dev srvdevtab = {
	's',
	"srv",

	srvinit,
	srvattach,
	srvclone,
	srvwalk,
	srvstat,
	srvopen,
	devcreate,
	srvclose,
	srvread,
	devbread,
	srvwrite,
	devbwrite,
	srvremove,
	srvwstat
};
