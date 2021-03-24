#include        "dat.h"
#include        "fns.h"
#include        "error.h"
#include        "kernel.h"

int
openmode(ulong o)
{
	if(o >= (OTRUNC|OCEXEC|ORCLOSE|OEXEC))
		error(Ebadarg);
	o &= ~(OTRUNC|OCEXEC|ORCLOSE);
	if(o > OEXEC)
		error(Ebadarg);
	if(o == OEXEC)
		return OREAD;
	return o;
}

Chan*
fdtochan(Fgrp *f, int fd, int mode, int chkmnt, int iref)
{
	Chan *c;

	c = 0;
	lock(&f->r.l);
	if(fd<0 || f->maxfd<fd || (c = f->fd[fd])==0) {
		unlock(&f->r.l);
		error(Ebadfd);
	}
	if(iref)
		incref(&c->r);
	unlock(&f->r.l);

	if(chkmnt && (c->flag&CMSG))
		goto bad;
	if(mode<0 || c->mode==ORDWR)
		return c;
	if((mode&OTRUNC) && c->mode==OREAD)
		goto bad;
	if((mode&~OTRUNC) != c->mode)
		goto bad;
	return c;
bad:
	if(iref)
		cclose(c);
	error(Ebadusefd);
	return nil;
}

static void
fdclose(Fgrp *f, int fd, int flag)
{
	int i;
	Chan *c;

	lock(&f->r.l);
	c = f->fd[fd];
	if(c == 0) {
		unlock(&f->r.l);
		return;
	}
	if(flag) {
		if(c==0 || !(c->flag&flag)) {
			unlock(&f->r.l);
			return;
		}
	}
	f->fd[fd] = 0;
	if(fd == f->maxfd)
		for(i=fd; --i>=0 && f->fd[i]==0; )
			f->maxfd = i;

	unlock(&f->r.l);
	cclose(c);
}

static int
newfd(Chan *c)
{
	int i;
	Fgrp *f;

	f = up->env->fgrp;
	lock(&f->r.l);
	for(i=0; i<NFD; i++)
		if(f->fd[i] == 0){
			if(i > f->maxfd)
				f->maxfd = i;
			f->fd[i] = c;
			unlock(&f->r.l);
			return i;
		}
	unlock(&f->r.l);
	exhausted("no file descriptors");
	return 0;
}

long
kchanio(void *vc, void *buf, int n, int mode)
{
	int r;
	Chan *c;

	c = vc;
	if(waserror())
		return -1;

	if(mode == OREAD)
		r = devtab[c->type]->read(c, buf, n, c->offset);
	else
		r = devtab[c->type]->write(c, buf, n, c->offset);

	lock(&c->r.l);
	c->offset += r;
	unlock(&c->r.l);
	poperror();
	return r;
}

int
kchdir(char *path)
{
	Chan *c;
	Pgrp *pg;

	if(waserror())
		return -1;

	c = namec(path, Atodir, 0, 0);
	pg = up->env->pgrp;
	cclose(pg->dot);
	pg->dot = c;
	poperror();
	return 0;
}

int
kfgrpclose(Fgrp *f, int fd)
{
	if(waserror())
		return -1;

	/*
	 * Take no reference on the chan because we don't really need the
	 * data structure, and are calling fdtochan only for error checks.
	 * fdclose takes care of processes racing through here.
	 */
	fdtochan(f, fd, -1, 0, 0);
	fdclose(f, fd, 0);
	poperror();
	return 0;
}

int
kclose(int fd)
{
	return kfgrpclose(up->env->fgrp, fd);
}

int
kcreate(char *path, int mode, ulong perm)
{
	int fd;
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()) {
		cclose(c.c);
		return -1;
	}

	openmode(mode);                 /* error check only */
	c.c = namec(path, Acreate, mode, perm);
	fd = newfd(c.c);
	poperror();
	return fd;
}

int
kdup(int old, int new)
{
	Chan *oc;
	Fgrp *f = up->env->fgrp;
	volatile struct { Chan *c; } c;

	if(waserror())
		return -1;

	c.c = fdtochan(up->env->fgrp, old, -1, 0, 1);
	if(new != -1) {
		if(new < 0 || NFD <= new) {
			cclose(c.c);
			error(Ebadfd);
		}
		lock(&f->r.l);
		if(new > f->maxfd)
			f->maxfd = new;
		oc = f->fd[new];
		f->fd[new] = c.c;
		unlock(&f->r.l);
		if(oc != 0)
			cclose(oc);
	}
	else {
		if(waserror()) {
			cclose(c.c);
			nexterror();
		}
		new = newfd(c.c);
		poperror();
	}
	poperror();
	return new;
}

int
kfstat(int fd, char *buf)
{
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()) {
		cclose(c.c);
		return -1;
	}
	c.c = fdtochan(up->env->fgrp, fd, -1, 0, 1);
	devtab[c.c->type]->stat(c.c, buf);
	poperror();
	cclose(c.c);
	return 0;
}

int
kpipe(int fd[2])
{
	Dev *d;
	Fgrp *f;
	Chan *c[2];

	f = up->env->fgrp;

	d = devtab[devno('|', 0)];
	c[0] = namec("#|", Atodir, 0, 0);
	c[1] = 0;
	fd[0] = -1;
	fd[1] = -1;
	if(waserror()) {
		cclose(c[0]);
		if(c[1])
			cclose(c[1]);
		if(fd[0] >= 0)
			f->fd[fd[0]]=0;
		if(fd[1] >= 0)
			f->fd[fd[1]]=0;
		return -1;
	}
	c[1] = cclone(c[0], 0);
	walk(c[0], "data", 1);
	walk(c[1], "data1", 1);
	c[0] = d->open(c[0], ORDWR);
	c[1] = d->open(c[1], ORDWR);
	fd[0] = newfd(c[0]);
	fd[1] = newfd(c[1]);
	poperror();
	return 0;
}

int
kfwstat(int fd, char *buf)
{
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()) {
		cclose(c.c);
		return -1;
	}
	nameok(buf);
	c.c = fdtochan(up->env->fgrp, fd, -1, 1, 1);
	devtab[c.c->type]->wstat(c.c, buf);
	poperror();
	cclose(c.c);
	return 0;
}

long
bindmount(Chan *c, char *old, int flag, char *spec)
{
	int ret;
	volatile struct { Chan *c; } c0;
	volatile struct { Chan *c; } c1;

	c0.c = c;
	if(flag>MMASK || (flag&MORDER) == (MBEFORE|MAFTER))
		error(Ebadarg);

	c1.c = namec(old, Amount, 0, 0);
	if(waserror()){
		cclose(c1.c);
		nexterror();
	}
	ret = cmount(c0.c, c1.c, flag, spec);

	poperror();
	cclose(c1.c);
	return ret;
}

int
kbind(char *new, char *old, int flags)
{
	long r;
	volatile struct { Chan *c; } c0;

	c0.c = nil;
	if(waserror()) {
		cclose(c0.c);
		return -1;
	}
	c0.c = namec(new, Aaccess, 0, 0);
	r = bindmount(c0.c, old, flags, "");
	poperror();
	cclose(c0.c);
	return r;
}

int
kmount(int fd, char *old, int flags, char *spec)
{
	long r;
	volatile struct { Chan *c; } c0;
	volatile struct { Chan *c; } bc;
	struct {
		Chan*   chan;
		char*   spec;
		int     flags;
	} mntparam;

	bc.c = nil;
	c0.c = nil;
	if(waserror()) {
		cclose(bc.c);
		cclose(c0.c);
		return -1;
	}
	bc.c = fdtochan(up->env->fgrp, fd, ORDWR, 0, 1);
	mntparam.chan = bc.c;
	mntparam.spec = spec;
	mntparam.flags = flags;
	c0.c = devtab[devno('M', 0)]->attach((char*)&mntparam);

	r = bindmount(c0.c, old, flags, spec);
	cclose(bc.c);
	cclose(c0.c);
	poperror();

	return r;
}

int
kunmount(char *old, char *new)
{
	volatile struct { Chan *c; } cmount;
	volatile struct { Chan *c; } cmounted;

	cmount.c = nil;
	cmounted.c = nil;
	if(waserror()) {
		cclose(cmount.c);
		cclose(cmounted.c);
		return -1;
	}

	cmount.c = namec(new, Amount, OREAD, 0);
	if(old != nil && old[0] != '\0') {
		cmounted.c = namec(old, Aaccess, OREAD, 0);
	}

	cunmount(cmount.c, cmounted.c);
	poperror();
	cclose(cmount.c);
	cclose(cmounted.c);
	return 0;
}

int
kopen(char *path, int mode)
{
	int fd;
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()){
		cclose(c.c);
		return -1;
	}
	openmode(mode);                         /* error check only */
	c.c = namec(path, Aopen, mode, 0);
	fd = newfd(c.c);
	poperror();
	return fd;
}

int
unionread(Chan *c, void *va, long n)
{
	uchar *dp;
	long nr, r;
	volatile struct { Pgrp *p; } pg;
	volatile struct { Chan *c; } nc;

	r = 0;
	dp = va;
	nc.c = nil;

	pg.p = up->env->pgrp;
	rlock(&pg.p->ns);

	for(;;) {
		if(waserror()) {
			runlock(&pg.p->ns);
			nexterror();
		}
		nc.c = cclone(c->mnt->to, 0);
		poperror();

		if(c->mountid != c->mnt->mountid) {
			runlock(&pg.p->ns);
			cclose(nc.c);
			return 0;
		}

		/* Error causes component of union to be skipped */
		if(waserror()) {
			cclose(nc.c);
			goto next;
		}

		nc.c = devtab[nc.c->type]->open(nc.c, OREAD);
		nc.c->offset = c->offset;

		while (r < n) {
			nr = devtab[nc.c->type]->read(nc.c, dp+r, n-r, nc.c->offset);
			if(nr == 0)
				break;
			
			nc.c->offset += nr;	/* devdirread e.g. changes it */
			r += nr;
		}

		c->offset = nc.c->offset;

		poperror();
		cclose(nc.c);
		nc.c = nil;

		/* should work on directories as n is a multiple of DIRLEN */
		if(r >= n) {
			runlock(&pg.p->ns);
			return r;
		}
		/* Advance to next element */
	next:
		c->mnt = c->mnt->next;
		if(c->mnt == 0)
			break;

		c->mountid = c->mnt->mountid;
		c->offset = 0;
	}
	runlock(&pg.p->ns);
	return r;
}

long
kread(int fd, void *va, long n)
{
	int dir;
	Lock *cl;
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()) {
		cclose(c.c);
		return -1;
	}
	c.c = fdtochan(up->env->fgrp, fd, OREAD, 1, 1);

	dir = c.c->qid.path&CHDIR;
	if(dir) {
		n -= n%DIRLEN;
		if(c.c->offset%DIRLEN || n==0){
			print("kread failed\n");
			error(Etoosmall);
		}
	}

	if(dir && c.c->mnt)
		n = unionread(c.c, va, n);
	else {
		n = devtab[c.c->type]->read(c.c, va, n, c.c->offset);
		cl = &c.c->r.l;
		lock(cl);
		c.c->offset += n;
		unlock(cl);
	}

	poperror();
	cclose(c.c);

	return n;
}


long
kreadnb(int fd, void *va, long n)
{
	int dir;
	Lock *cl;
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()) {
		cclose(c.c);
		return -1;
	}
	c.c = fdtochan(up->env->fgrp, fd, OREAD, 1, 1);

	dir = c.c->qid.path&CHDIR;
	if (dir) {
		poperror();
		cclose(c.c);
		return 0;
	}


	if (devtab[c.c->type]->dc == 'I') {
	/*	n = devtab[c.c->type]->read(c.c, va, n, c.c->offset); */
		n = ipreadnew(c.c, va, n, c.c->offset, 1); 
	}
	else {
		poperror();
		cclose(c.c);
		return 0;
	}

	cl = (Lock*)&c.c->r.l;
	lock(cl);
	c.c->offset += n;
	unlock(cl);

	poperror();
	cclose(c.c);

	return n;
}


int
kremove(char *path)
{
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()) {
		if(c.c != nil)
			c.c->type = 0;	/* see below */
		cclose(c.c);
		return -1;
	}
	c.c = namec(path, Aaccess, 0, 0);
	devtab[c.c->type]->remove(c.c);
	/*
	 * Remove clunks the fid, but we need to recover the Chan
	 * so fake it up.  rootclose() is known to be a nop.
	 */
	c.c->type = 0;
	poperror();
	cclose(c.c);
	return 0;
}

long
kseek(int fd, long off, int whence)
{
	Dir dir;
	Chan *c;
	char buf[DIRLEN];

	if(waserror())
		return -1;

	c = fdtochan(up->env->fgrp, fd, -1, 1, 1);
	if(waserror()) {
		cclose(c);
		nexterror();
	}
	if(c->qid.path & CHDIR)
		error(Eisdir);

	switch(whence) {
	case 0:
		c->offset = off;
		break;

	case 1:
		lock(&c->r.l);	/* lock for read/write update */
		c->offset += off;
		off = c->offset;
		unlock(&c->r.l);
		break;

	case 2:
		devtab[c->type]->stat(c, buf);
		convM2D(buf, &dir);
		c->offset = dir.length + off;
		off = c->offset;
		break;
	default:
		error(Ebadarg);
		break;
	}
	poperror();
	cclose(c);
	poperror();
	return off;
}

int
kstat(char *path, char *buf)
{
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()){
		cclose(c.c);
		return -1;
	}
	c.c = namec(path, Aaccess, 0, 0);
	devtab[c.c->type]->stat(c.c, buf);
	poperror();
	cclose(c.c);
	return 0;
}

long
kwrite(int fd, void *va, long n)
{
	Lock *cl;
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()) {
		cclose(c.c);
		return -1;
	}
	c.c = fdtochan(up->env->fgrp, fd, OWRITE, 1, 1);
	if(c.c->qid.path & CHDIR)
		error(Eisdir);

	n = devtab[c.c->type]->write(c.c, va, n, c.c->offset);

	cl = (Lock*)&c.c->r.l;
	lock(cl);
	c.c->offset += n;
	unlock(cl);

	poperror();
	cclose(c.c);

	return n;
}

long
kwritenb(int fd, void *va, long n)
{
	Lock *cl;
	volatile struct { Chan *c; } c;


	c.c = nil;
	if(waserror()) {
		cclose(c.c);
		return -1;
	}
	c.c = fdtochan(up->env->fgrp, fd, OWRITE, 1, 1);
	if(c.c->qid.path & CHDIR)
		error(Eisdir);

	if (devtab[c.c->type]->dc == 'I') {
	/*	n = devtab[c.c->type]->write(c.c, va, n, c.c->offset); */
		n = ipwritenew(c.c, va, n, c.c->offset, 1); 
	}
	else    {
		poperror();
		cclose(c.c);
		return 0;
	}

	cl = (Lock*)&c.c->r.l;
	lock(cl);
	c.c->offset += n;
	unlock(cl);

	poperror();
	cclose(c.c);

	return n;
}


int
kwstat(char *path, char *buf)
{
	volatile struct { Chan *c; } c;

	c.c = nil;
	if(waserror()) {
		cclose(c.c);
		return -1;
	}
	/* name is known to be first member */
	nameok(buf);
	c.c = namec(path, Aaccess, 0, 0);
	devtab[c.c->type]->wstat(c.c, buf);
	poperror();
	cclose(c.c);
	return 0;
}

int
kdirstat(char *name, Dir *dir)
{
	char buf[DIRLEN];

	if(kstat(name, buf) == -1)
		return -1;
	convM2D(buf, dir);
	return 0;
}

int
kdirfstat(int fd, Dir *dir)
{
	char buf[DIRLEN];

	if(kfstat(fd, buf) == -1)
		return -1;

	convM2D(buf, dir);
	return 0;
}

int
kdirwstat(char *name, Dir *dir)
{
	char buf[DIRLEN];

	convD2M(dir, buf);
	return kwstat(name, buf);
}

int
kdirfwstat(int fd, Dir *dir)
{
	char buf[DIRLEN];

	convD2M(dir, buf);
	return kfwstat(fd, buf);
}

long
kdirread(int fd, Dir *dbuf, long count)
{
	int c, n, i, r;
	char *b;

	if(waserror())
		return -1;
	poperror();
	n = 0;
	b = malloc(DIRLEN*50);
	if(b == 0)
		return -1;
	count = (count/sizeof(Dir)) * DIRLEN;
	while(n < count) {
		c = count - n;
		if(c > DIRLEN*50)
			c = DIRLEN*50;
		r = kread(fd, b, c);
		if(r == 0)
			break;
		if(r < 0 || r % DIRLEN){
			free(b);
			return -1;
		}
		for(i=0; i<r; i+=DIRLEN) {
			convM2D(b+i, dbuf);
			dbuf++;
		}
		n += r;
		if(r != c)
			break;
	}
	free(b);

	return (n/DIRLEN) * sizeof(Dir);
}
