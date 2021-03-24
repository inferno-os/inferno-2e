#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"styx.h"

struct Mntrpc
{
	Chan*	c;		/* Channel for whom we are working */
	Mntrpc*	list;		/* Free/pending list */
	Fcall	request;	/* Outgoing file system protocol message */
	Fcall	reply;		/* Incoming reply */
	Mnt*	m;		/* Mount device during rpc */
	Rendez	r;		/* Place to hang out */
	char*	rpc;		/* I/O Data buffer */
	char	done;		/* Rpc completed */
	char	flushed;	/* Flush was sent */
	ushort	flushtag;	/* Tag flush sent on */
	char	flush[MAXMSG];	/* Somewhere to build flush */
};

struct Mntalloc
{
	Lock	l;
	Mnt*	list;		/* Mount devices in use */
	Mnt*	mntfree;	/* Free list */
	Mntrpc*	rpcfree;
	ulong	id;
	int	rpctag;
}mntalloc;

#define MAXRPC		(MAXFDATA+MAXMSG)
#define limit(n, max)	(n > max ? max : n)

void	mattach(Mnt*, Chan*, char *spec);
void	mntauth(Mnt *, Mntrpc *, char *, ushort);
Mnt*	mntchk(Chan*);
void	mntdirfix(uchar*, Chan*);
int	mntflush(Mnt*, Mntrpc*);
void	mntfree(Mntrpc*);
void	mntgate(Mnt*);
void	mntpntfree(Mnt*);
void	mntqrm(Mnt*, Mntrpc*);
Mntrpc*	mntralloc(Chan*);
long	mntrdwr(int , Chan*, void*,long , ulong);
long	mnt9prdwr(int , Chan*, void*,long , ulong);
void	mntrpcread(Mnt*, Mntrpc*);
void	mountio(Mnt*, Mntrpc*);
void	mountmux(Mnt*, Mntrpc*);
void	mountrpc(Mnt*, Mntrpc*);
int	rpcattn(void*);
void	mclose(Mnt*, Chan*);
Chan*	mntchan(void);

int	defmaxmsg = MAXFDATA;
int	mntdebug;

enum
{
	Tagspace	= 1,
	Tagfls		= 0x8000,
	Tagend		= 0xfffe
};

void
mntinit(void)
{
	mntalloc.id = 1;
	mntalloc.rpctag = Tagspace;

	fmtinstall('F', fcallconv);
}

Chan*
mntattach(void *muxattach)
{
	Mnt *m;
	Chan *mc;
	volatile struct { Chan *c; } c;
	char buf[NAMELEN];
	struct {
		Chan	*chan;
		char	*spec;
		int	flags;
	} mntparam;

	memmove(&mntparam, muxattach, sizeof(mntparam));
	c.c = mntparam.chan;

	lock(&mntalloc.l);
	for(m = mntalloc.list; m; m = m->list) {
		if(m->c == c.c && m->id) {
			lock(&m->r.l);
			if(m->id && m->r.ref > 0 && m->c == c.c) {
				unlock(&mntalloc.l);
				m->r.ref++;
				unlock(&m->r.l);
				c.c = mntchan();
				if(waserror()) {
					chanfree(c.c);
					nexterror();
				}
				mattach(m, c.c, mntparam.spec);
				poperror();
				return c.c;
			}
			unlock(&m->r.l);	
		}
	}

	m = mntalloc.mntfree;
	if(m != 0)
		mntalloc.mntfree = m->list;	
	else {
		m = malloc(sizeof(Mnt)+MAXRPC);
		if(m == 0) {
			unlock(&mntalloc.l);
			exhausted("no mount devices");
		}
		m->flushbase = Tagfls;
		m->flushtag = Tagfls;
	}
	m->list = mntalloc.list;
	mntalloc.list = m;
	m->id = mntalloc.id++;
	lock(&m->r.l);
	unlock(&mntalloc.l);

	m->r.ref = 1;
	m->queue = 0;
	m->rip = 0;
	m->c = c.c;
	m->c->flag |= CMSG;
	m->blocksize = defmaxmsg;
	m->flags = mntparam.flags;

	incref(&m->c->r);

	sprint(buf, "#M%d", m->id);
	m->tree.root = ptenter(&m->tree, 0, buf);

	unlock(&m->r.l);

	c.c = mntchan();
	if(waserror()) {
		mclose(m, c.c);
		/* Close must not be called since it will
		 * call mnt recursively
		 */
		chanfree(c.c);
		nexterror();
	}

	mattach(m, c.c, mntparam.spec);
	poperror();

	/*
	 * Detect a recursive mount for a mount point served by export.
	 * If CHDIR is clear in the returned qid, the foreign server is
	 * requesting the mount point be folded into the connection
	 * to the exportfs. In this case the remote mount driver does
	 * the multiplexing.
	 */
	mc = m->c;
	if(mc->type == devno('M', 0) && (c.c->qid.path&CHDIR) == 0) {
		mclose(m, c.c);
		c.c->qid.path |= CHDIR;
		c.c->u.mntptr = mc->u.mntptr;
		c.c->mchan = c.c->u.mntptr->c;
		c.c->mqid = c.c->qid;
		c.c->path = c.c->u.mntptr->tree.root;
		incref(&c.c->path->r);
		incref(&c.c->u.mntptr->r);
	}

	return c.c;
}

Chan*
mntchan(void)
{
	Chan *c;

	c = devattach('M', 0);
	lock(&mntalloc.l);
	c->dev = mntalloc.id++;
	unlock(&mntalloc.l);

	return c;
}

void
mattach(Mnt *m, Chan *c, char *spec)
{
	volatile struct { Mntrpc *r; } r;

	r.r = mntralloc(0);
	c->u.mntptr = m;

	if(waserror()) {
		mntfree(r.r);
		nexterror();
	}

	r.r->request.type = Tattach;
	r.r->request.fid = c->fid;
	memmove(r.r->request.uname, up->env->user, NAMELEN);
	strncpy(r.r->request.aname, spec, NAMELEN);

	mountrpc(m, r.r);

	c->qid = r.r->reply.qid;
	c->mchan = m->c;
	c->mqid = c->qid;
	c->path = m->tree.root;
	incref(&c->path->r);

	poperror();
	mntfree(r.r);
}

Chan*
mntclone(Chan *c, Chan *xnc)
{
	Mnt *m;
	volatile struct { Chan *nc; } nc;
	volatile struct { Mntrpc *r; } r;
	volatile int alloc = 0;

	nc.nc = xnc;
	m = mntchk(c);
	r.r = mntralloc(c);
	if(nc.nc == 0) {
		nc.nc = newchan();
		alloc = 1;
	}
	if(waserror()) {
		mntfree(r.r);
		if(alloc)
			cclose(nc.nc);
		nexterror();
	}

	r.r->request.type = Tclone;
	r.r->request.fid = c->fid;
	r.r->request.newfid = nc.nc->fid;
	mountrpc(m, r.r);

	devclone(c, nc.nc);
	nc.nc->mqid = c->qid;
	incref(&m->r);

	USED(alloc);
	poperror();
	mntfree(r.r);
	return nc.nc;
}

int	 
mntwalk(Chan *c, char *name)
{
	Mnt *m;
	Path *op;
	volatile struct { Mntrpc *r; } r;

	m = mntchk(c);
	r.r = mntralloc(c);
	if(waserror()) {
		mntfree(r.r);
		return 0;
	}
	r.r->request.type = Twalk;
	r.r->request.fid = c->fid;
	strncpy(r.r->request.name, name, NAMELEN);
	mountrpc(m, r.r);

	c->qid = r.r->reply.qid;
	op = c->path;
	c->path = ptenter(&m->tree, op, name);

	decref(&op->r);

	poperror();
	mntfree(r.r);
	return 1;
}

void	 
mntstat(Chan *c, char *dp)
{
	Mnt *m;
	volatile struct { Mntrpc *r; } r;

	m = mntchk(c);
	r.r = mntralloc(c);
	if(waserror()) {
		mntfree(r.r);
		nexterror();
	}
	r.r->request.type = Tstat;
	r.r->request.fid = c->fid;
	mountrpc(m, r.r);

	memmove(dp, r.r->reply.stat, DIRLEN);
	mntdirfix((uchar*)dp, c);
	poperror();
	mntfree(r.r);
}

Chan*
mntopen(Chan *c, int omode)
{
	Mnt *m;
	volatile struct { Mntrpc *r; } r;

	m = mntchk(c);
	r.r = mntralloc(c);
	if(waserror()) {
		mntfree(r.r);
		nexterror();
	}
	r.r->request.type = Topen;
	r.r->request.fid = c->fid;
	r.r->request.mode = omode;
	mountrpc(m, r.r);

	c->qid = r.r->reply.qid;
	c->offset = 0;
	c->mode = openmode(omode);
	c->flag |= COPEN;
	poperror();
	mntfree(r.r);

	return c;
}

void	 
mntcreate(Chan *c, char *name, int omode, ulong perm)
{
	Mnt *m;
	volatile struct { Mntrpc *r; } r;

	m = mntchk(c);
	r.r = mntralloc(c);
	if(waserror()) {
		mntfree(r.r);
		nexterror();
	}
	r.r->request.type = Tcreate;
	r.r->request.fid = c->fid;
	r.r->request.mode = omode;
	r.r->request.perm = perm;
	strncpy(r.r->request.name, name, NAMELEN);
	mountrpc(m, r.r);

	c->qid = r.r->reply.qid;
	c->flag |= COPEN;
	c->mode = openmode(omode);
	poperror();
	mntfree(r.r);
}

void	 
mntclunk(Chan *xc, int t)
{
	Mnt *m;
	volatile struct { Chan *c; } c;
	volatile struct { Mntrpc *r; } r;

	c.c = xc;	
	m = mntchk(c.c);
	r.r = mntralloc(c.c);
	if(waserror()){
		mntfree(r.r);
		mclose(m, c.c);
		nexterror();
	}

	r.r->request.type = t;
	r.r->request.fid = c.c->fid;
	mountrpc(m, r.r);
	mntfree(r.r);
	mclose(m, c.c);
	poperror();
}

void
mclose(Mnt *m, Chan *c)
{
	Mntrpc *q, *r;

	if(decref(&m->r) != 0)
		return;

	c->path = 0;
	ptclose(&m->tree);

	for(q = m->queue; q; q = r) {
		r = q->list;
		q->flushed = 0;
		mntfree(q);
	}
	m->id = 0;
	cclose(m->c);
	mntpntfree(m);
}

void
mntpntfree(Mnt *m)
{
	Mnt *f, **l;

	lock(&mntalloc.l);
	l = &mntalloc.list;
	for(f = *l; f; f = f->list) {
		if(f == m) {
			*l = m->list;
			break;
		}
		l = &f->list;
	}

	m->list = mntalloc.mntfree;
	mntalloc.mntfree = m;
	unlock(&mntalloc.l);
}

void
mntclose(Chan *c)
{
	mntclunk(c, Tclunk);
}

void	 
mntremove(Chan *c)
{
	mntclunk(c, Tremove);
}

void
mntwstat(Chan *c, char *dp)
{
	Mnt *m;
	volatile struct { Mntrpc *r; } r;

	m = mntchk(c);
	r.r = mntralloc(c);
	if(waserror()) {
		mntfree(r.r);
		nexterror();
	}
	r.r->request.type = Twstat;
	r.r->request.fid = c->fid;
	memmove(r.r->request.stat, dp, DIRLEN);
	mountrpc(m, r.r);
	poperror();
	mntfree(r.r);
}

long	 
mntread9p(Chan *c, void *buf, long n, ulong offset)
{
	return mnt9prdwr(Tread, c, buf, n, offset);
}

long	 
mntread(Chan *c, void *buf, long n, ulong offset)
{
	int isdir;
	uchar *p, *e;

	isdir = 0;
	if(c->qid.path & CHDIR)
		isdir = 1;

	p = buf;
	n = mntrdwr(Tread, c, buf, n, offset);
	if(isdir) {
		for(e = &p[n]; p < e; p += DIRLEN)
			mntdirfix(p, c);
	}
	return n;
}

long	 
mntwrite9p(Chan *c, void *buf, long n, ulong offset)
{
	return mnt9prdwr(Twrite, c, buf, n, offset);
}

long	 
mntwrite(Chan *c, void *buf, long n, ulong offset)
{
	return mntrdwr(Twrite, c, buf, n, offset);
}

long
mnt9prdwr(int type, Chan *c, void *buf, long n, ulong offset)
{
	Mnt *m;
 	ulong nr;
	volatile struct { Mntrpc *r; } r;

	if(n > MAXRPC-32) {
		if(type == Twrite)
			error("write9p too long");
		n = MAXRPC-32;
	}

	m = mntchk(c);
	r.r = mntralloc(c);
	if(waserror()) {
		mntfree(r.r);
		nexterror();
	}
	r.r->request.type = type;
	r.r->request.fid = c->fid;
	r.r->request.offset = offset;
	r.r->request.data = buf;
	r.r->request.count = n;
	mountrpc(m, r.r);
	nr = r.r->reply.count;
	if(nr > r.r->request.count)
		nr = r.r->request.count;

	if(type == Tread)
		memmove(buf, r.r->reply.data, nr);

	poperror();
	mntfree(r.r);
	return nr;
}

long
mntrdwr(int type, Chan *c, void *buf, long n, ulong offset)
{
	Mnt *m;
	char *uba;
	ulong cnt, nr;
 	volatile struct { Mntrpc *r; } r;

	m = mntchk(c);
	uba = buf;
	cnt = 0;
	for(;;) {
		r.r = mntralloc(c);
		if(waserror()) {
			mntfree(r.r);
			nexterror();
		}
		r.r->request.type = type;
		r.r->request.fid = c->fid;
		r.r->request.offset = offset;
		r.r->request.data = uba;
		r.r->request.count = limit(n, m->blocksize);
		mountrpc(m, r.r);
		nr = r.r->reply.count;
		if(nr > r.r->request.count)
			nr = r.r->request.count;

		if(type == Tread)
			memmove(uba, r.r->reply.data, nr);

		poperror();
		mntfree(r.r);
		offset += nr;
		uba += nr;
		cnt += nr;
		n -= nr;
		if(nr != r.r->request.count || n == 0)
			break;
	}
	return cnt;
}

void
mountrpc(Mnt *m, Mntrpc *r)
{
	int t;

	r->reply.tag = 0;
	r->reply.type = 4;

	mountio(m, r);

	t = r->reply.type;
	switch(t) {
	case Rerror:
		error(r->reply.ename);
	case Rflush:
		error(Eintr);
	default:
		if(t == r->request.type+1)
			break;
		print("mnt: mismatch rep 0x%lux T%d R%d rq %d fls %d rp %d\n",
			r, r->request.type, r->reply.type, r->request.tag, 
			r->flushtag, r->reply.tag);
		error(Emountrpc);
	}
}

void
mountio(Mnt *xm, Mntrpc *xr)
{
	int n;
	volatile struct { Mnt *m; } m;
	volatile struct { Mntrpc *r; } r;

	m.m = xm;
	r.r = xr;

	lock(&m.m->r.l);
	r.r->flushed = 0;
	r.r->m = m.m;
	r.r->list = m.m->queue;
	m.m->queue = r.r;
	unlock(&m.m->r.l);

	/* Transmit a file system rpc */
	n = convS2M(&r.r->request, r.r->rpc);
	if(n < 0)
		panic("bad message type in mountio");
	if(mntdebug)
		print("mnt: <- %F\n", &r.r->request);
	if(waserror()) {
		if(mntflush(m.m, r.r) == 0)
			nexterror();
	}
	else {
		if(devtab[m.m->c->type]->dc == L'M'){
			if(mnt9prdwr(Twrite, m.m->c, r.r->rpc, n, 0) != n)
				error(Emountrpc);
		}
		else
		if(devtab[m.m->c->type]->write(m.m->c, r.r->rpc, n, 0) != n)
				error(Emountrpc);
		poperror();
	}

	/* Gate readers onto the mount point one at a time */
	for(;;) {
		lock(&m.m->r.l);
		if(m.m->rip == 0)
			break;
		unlock(&m.m->r.l);
		if(waserror()) {
			if(mntflush(m.m, r.r) == 0)
				nexterror();
			continue;
		}
		Sleep(&r.r->r, rpcattn, r.r);
		poperror();
		if(r.r->done)
			return;
	}
	m.m->rip = up;
	unlock(&m.m->r.l);
	while(r.r->done == 0) {
		mntrpcread(m.m, r.r);
		mountmux(m.m, r.r);
	}
	mntgate(m.m);
}

void
mntrpcread(Mnt *xm, Mntrpc *xr)
{
	char *buf;
	int n, cn, len, eof;
	volatile struct { Mnt *m; } m;
	volatile struct { Mntrpc *r; } r;

	m.m = xm;
	r.r = xr;

	buf = r.r->rpc;
	len = MAXRPC;
	n = m.m->npart;
	eof = 0;
	if(n > 0){
		memmove(buf, m.m->part, n);
		buf += n;
		len -= n;
		m.m->npart = 0;
		goto chk;
	}

	for(;;) {
		if(waserror()) {
			if(mntflush(m.m, r.r) == 0) {
				mntgate(m.m);
				nexterror();
			}
			continue;
		}

		r.r->reply.tag = 0;
		r.r->reply.type = 0;

		if(devtab[m.m->c->type]->dc == L'M')
			n = mnt9prdwr(Tread, m.m->c, buf, len, 0);
		else
			n = devtab[m.m->c->type]->read(m.m->c, buf, len, 0);

		poperror();

		if(n == 0) {
			if(eof) {
				mntqrm(m.m, r.r);
				mntgate(m.m);
				error(Ehungup);
			}
			eof = 1;
			continue;
		}
		eof = 0;
		buf += n;
		len -= n;
	chk:
		n = buf - r.r->rpc;

		/* convM2S returns size of correctly decoded message */
		cn = convM2S(r.r->rpc, &r.r->reply, n);
		if(cn < 0) {
			mntqrm(m.m, r.r);
			mntgate(m.m);
			error("bad message type in devmnt");
		}
		if(cn > 0) {
			n -= cn;
			if(n < 0)
				panic("negative size in devmnt");
			m.m->npart = n;
			if(n != 0)
				memmove(m.m->part, r.r->rpc+cn, n);
			if(mntdebug)
				print("mnt: %s: <- %F\n", up->env->user, &r.r->reply);
			return;
		}
	}
}

void
mntgate(Mnt *m)
{
	Mntrpc *q;

	lock(&m->r.l);
	m->rip = 0;
	for(q = m->queue; q; q = q->list) {
		if(q->done == 0) {
			lock(&q->r.l);
			if(q->r.p) {
				unlock(&q->r.l);
				unlock(&m->r.l);
				Wakeup(&q->r);
				return;
			}
			unlock(&q->r.l);
		}
	}
	unlock(&m->r.l);
}

void
mountmux(Mnt *m, Mntrpc *r)
{
	char *dp;
	Mntrpc **l, *q;

	lock(&m->r.l);
	l = &m->queue;
	for(q = *l; q; q = q->list) {
		if(q->request.tag == r->reply.tag
		|| q->flushed && q->flushtag == r->reply.tag) {
			*l = q->list;
			unlock(&m->r.l);
			if(q != r) {		/* Completed someone else */
				dp = q->rpc;
				q->rpc = r->rpc;
				r->rpc = dp;
				q->reply = r->reply;
				q->done = 1;
				Wakeup(&q->r);
			}
			else
				q->done = 1;
			return;
		}
		l = &q->list;
	}
	unlock(&m->r.l);
}

int
mntflush(Mnt *xm, Mntrpc *xr)
{
	int n, l;
	Fcall flush;
	volatile struct { Mnt *m; } m;
	volatile struct { Mntrpc *r; } r;

	m.m = xm;
	r.r = xr;

	lock(&m.m->r.l);
	r.r->flushtag = m.m->flushtag++;
	if(m.m->flushtag == Tagend)
		m.m->flushtag = m.m->flushbase;
	r.r->flushed = 1;
	unlock(&m.m->r.l);

	flush.type = Tflush;
	flush.tag = r.r->flushtag;
	flush.oldtag = r.r->request.tag;
	n = convS2M(&flush, r.r->flush);
	if(n < 0)
		panic("bad message type in mntflush");

	if(waserror()) {
		if(strcmp(up->env->error, Eintr) == 0)
			return 1;
		mntqrm(m.m, r.r);
		return 0;
	}
	l = devtab[m.m->c->type]->write(m.m->c, r.r->flush, n, 0);
	if(l != n)
		error(Ehungup);
	poperror();
	return 1;
}

Mntrpc *
mntralloc(Chan *c)
{
	Mntrpc *new;

	lock(&mntalloc.l);
	new = mntalloc.rpcfree;
	if(new != 0)
		mntalloc.rpcfree = new->list;
	else {
		new = malloc(sizeof(Mntrpc)+MAXRPC);
		if(new == 0) {
			unlock(&mntalloc.l);
			error(Enovmem);
		}
		new->rpc = (char*)new+sizeof(Mntrpc);
		new->request.tag = mntalloc.rpctag++;
	}
	unlock(&mntalloc.l);
	new->c = c;
	new->done = 0;
	new->flushed = 0;
	return new;
}

void
mntfree(Mntrpc *r)
{
	lock(&mntalloc.l);
	r->list = mntalloc.rpcfree;
	mntalloc.rpcfree = r;
	unlock(&mntalloc.l);
}

void
mntqrm(Mnt *m, Mntrpc *r)
{
	Mntrpc **l, *f;

	lock(&m->r.l);
	r->done = 1;
	r->flushed = 0;

	l = &m->queue;
	for(f = *l; f; f = f->list) {
		if(f == r) {
			*l = r->list;
			break;
		}
		l = &f->list;
	}
	unlock(&m->r.l);
}

Mnt*
mntchk(Chan *c)
{
	Mnt *m;

	m = c->u.mntptr;

	/*
	 * Was it closed and reused
	 */
	if(m->id == 0 || m->id >= c->dev)
		error(Eshutdown);

	return m;
}

void
mntdirfix(uchar *dirbuf, Chan *c)
{
	int r;

	r = devtab[c->type]->dc;
	dirbuf[DIRLEN-4] = r>>0;
	dirbuf[DIRLEN-3] = r>>8;
	dirbuf[DIRLEN-2] = c->dev;
	dirbuf[DIRLEN-1] = c->dev>>8;
}

int
rpcattn(void *a)
{
	Mntrpc *r = a;
	return r->done || r->m->rip == 0;
}

Dev mntdevtab = {
	'M',
	"mnt",

	mntinit,
	mntattach,
	mntclone,
	mntwalk,
	mntstat,
	mntopen,
	mntcreate,
	mntclose,
	mntread,
	devbread,
	mntwrite,
	devbwrite,
	mntremove,
	mntwstat
};
