#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

int
newfd(Chan *c)
{
	int i;
	Fgrp *f = up->env->fgrp;

	lock(f);
	for(i=0; i<NFD; i++)
		if(f->fd[i] == 0){
			if(i > f->maxfd)
				f->maxfd = i;
			f->fd[i] = c;
			unlock(f);
			return i;
		}
	unlock(f);
	exhausted("file descriptors");
	return 0;
}

Chan*
fdtochan(Fgrp *f, int fd, int mode, int chkmnt, int iref)
{
	Chan *c;

	c = 0;

	lock(f);
	if(fd<0 || f->maxfd<fd || (c = f->fd[fd])==0) {
		unlock(f);
		error(Ebadfd);
	}
	if(iref)
		incref(c);
	unlock(f);

	if(chkmnt && (c->flag&CMSG)) {
		if(iref)
			cclose(c);
		error(Ebadusefd);
	}

	if(mode<0 || c->mode==ORDWR)
		return c;

	if((mode&OTRUNC) && c->mode==OREAD) {
		if(iref)
			cclose(c);
		error(Ebadusefd);
	}

	if((mode&~OTRUNC) != c->mode) {
		if(iref)
			cclose(c);
		error(Ebadusefd);
	}

	return c;
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

	lock(c);
	c->offset += r;
	unlock(c);
	poperror();
	return r;
}

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

long
kdup(int old, int new)
{
	int fd;
	Chan *c, *oc;
	Fgrp *f = up->env->fgrp;

	if(waserror())
		return -1;

	/*
	 * Close after dup'ing, so date > #d/1 works
	 */
	c = fdtochan(up->env->fgrp, old, -1, 0, 1);
	fd = new;
	if(fd != -1){
		if(fd<0 || NFD<=fd) {
			cclose(c);
			error(Ebadfd);
		}
		lock(f);
		if(fd > f->maxfd)
			f->maxfd = fd;

		oc = f->fd[fd];
		f->fd[fd] = c;
		unlock(f);
		if(oc)
			cclose(oc);
	}else{
		if(waserror()) {
			cclose(c);
			nexterror();
		}
		fd = newfd(c);
		poperror();
	}
	poperror();
	return fd;
}

long
kopen(char *path, int mode)
{
	int fd;
	Chan *c = 0;

	if(waserror()) {
		cclose(c);
		return -1;
	}
	openmode(mode);
	c = namec(path, Aopen, mode, 0);

        /*    check to see if open succeed;
         *    this is used to deal with ipopen timeout
         */
        if (c->flag & COPEN)
                fd = newfd(c);
        else    fd = -1;

	poperror();
	return fd;
}

void
fdclose(Fgrp *f, int fd, int flag)
{
	int i;
	Chan *c;

	lock(f);
	c = f->fd[fd];
	if(c == 0){
		/* can happen for users with shared fd tables */
		unlock(f);
		return;
	}
	if(flag){
		if(c==0 || !(c->flag&flag)){
			unlock(f);
			return;
		}
	}
	f->fd[fd] = 0;
	if(fd == f->maxfd)
		for(i=fd; --i>=0 && f->fd[i]==0; )
			f->maxfd = i;

	unlock(f);
	cclose(c);
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

long
kclose(int fd)
{
	return kfgrpclose(up->env->fgrp, fd);
}

long
unionread(Chan *c, void *va, long n)
{
	Pgrp *pg;
	Chan *nc;
	uchar *dp;
	long nr, r;

	r = 0;
	dp = va;

	pg = up->env->pgrp;
	rlock(&pg->ns);

	for(;;) {
		if(waserror()) {
			runlock(&pg->ns);
			nexterror();
		}
		nc = cclone(c->mnt->to, 0);
		poperror();

		if(c->mountid != c->mnt->mountid) {
			runlock(&pg->ns);
			cclose(nc);
			return 0;
		}

		/* Error causes component of union to be skipped */
		if(waserror()) {	
			cclose(nc);
			goto next;
		}

		nc = devtab[nc->type]->open(nc, OREAD);
		nc->offset = c->offset;

		while (r < n) {
			nr = devtab[nc->type]->read(nc, dp+r, n-r, nc->offset);
			if(nr == 0)
				break;
			
			nc->offset += nr;	/* devdirread e.g. changes it */
			r += nr;
		}

		c->offset = nc->offset;

		poperror();
		cclose(nc);
		nc = nil;
		USED(nc);	/* Because of error longjmp */

 		/* should work on directories as n is a multiple of DIRLEN */
		if(r >= n) {
			runlock(&pg->ns);
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
	runlock(&pg->ns);
	return r;
}

long
kread(int fd, void *va, int n)
{
	int dir;
	Chan *c;

	if(waserror())
		return -1;

	c = fdtochan(up->env->fgrp, fd, OREAD, 1, 1);
	if(waserror()) {
		cclose(c);
		nexterror();
	}

	dir = c->qid.path&CHDIR;
	if(dir) {
		n -= n%DIRLEN;
		if(c->offset%DIRLEN || n==0)
			error(Etoosmall);
	}

	if(dir && c->mnt)
		n = unionread(c, va, n);
	else {
		n = devtab[c->type]->read(c, va, n, c->offset);
		lock(c);
		c->offset += n;
		unlock(c);
	}
	poperror();
	cclose(c);

	poperror();
	return n;
}

long
kwrite(int fd, void *va, int n)
{
	Chan *c;

	if(waserror())
		return -1;

	c = fdtochan(up->env->fgrp, fd, OWRITE, 1, 1);
	if(waserror()) {
		cclose(c);
		nexterror();
	}

	if(c->qid.path & CHDIR)
		error(Eisdir);

	n = devtab[c->type]->write(c, va, n, c->offset);

	lock(c);
	c->offset += n;
	unlock(c);

	poperror();
	cclose(c);

	poperror();
	return n;
}

long
kseek(int fd, long offset, int whence)
{
	Chan *c;
	char buf[DIRLEN];
	Dir dir;
	long off;

	if(waserror())
		return -1;

	c = fdtochan(up->env->fgrp, fd, -1, 1, 1);
	if(waserror()) {
		cclose(c);
		nexterror();
	}
	if(c->qid.path & CHDIR)
		error(Eisdir);

	off = 0;
	switch(whence) {
	case 0:
		lock(c);	/* lock for read/write update */
		off = c->offset = offset;
		unlock(c);
		break;
	case 1:
		lock(c);	/* lock for read/write update */
		c->offset += offset;
		off = c->offset;
		unlock(c);
		break;
	case 2:
		devtab[c->type]->stat(c, buf);
		convM2D(buf, &dir);
		lock(c);	/* lock for read/write update */
		off = c->offset = dir.length + offset;
		unlock(c);
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

long
kfstat(int fd, char *buf)
{
	Chan *c;

	if(waserror())
		return -1;

	c = fdtochan(up->env->fgrp, fd, -1, 0, 1);
	if(waserror()) {
		cclose(c);
		nexterror();
	}
	devtab[c->type]->stat(c, buf);
	poperror();
	cclose(c);
	poperror();
	return 0;
}

long
kstat(char *path, char *buf)
{
	Chan *c;

	if(waserror())
		return -1;

	c = namec(path, Aaccess, 0, 0);
	if(waserror()){
		cclose(c);
		nexterror();
	}
	devtab[c->type]->stat(c, buf);

	poperror();
	cclose(c);

	poperror();
	return 0;
}

long
kchdir(char *path)
{
	Chan *c;
	Osenv *o;

	if(waserror())
		return -1;

	c = namec(path, Atodir, 0, 0);
	o = up->env;
	cclose(o->pgrp->dot);
	o->pgrp->dot = c;

	poperror();
	return 0;
}

long
bindmount(Chan *c, char *old, int flag, char *spec)
{
	long ret;
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
		Chan*	chan;
		char*	spec;
		int	flags;
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

long
kunmount(char *old, char *new)
{
	Chan *cmount, *cmounted;

	cmount = 0;
	cmounted = 0;

	if(waserror()) {
		cclose(cmount);
		cclose(cmounted);
		return -1;
	}

	cmount = namec(new, Amount, 0, 0);

	if(old != nil && old[0] != '\0') {
		cmounted = namec(old, Aopen, OREAD, 0);
	}

	cunmount(cmount, cmounted);
	cclose(cmount);
	cclose(cmounted);

	poperror();
	return 0;
}

long
kcreate(char *path, int mode, int perm)
{
	int fd;
	Chan *c = 0;

	if(waserror()) {
		cclose(c);
		return -1;
	}
	openmode(mode);
	c = namec(path, Acreate, mode, perm);
	fd = newfd(c);
	poperror();
	return fd;
}

long
kremove(char *path)
{
	Chan *c;

	if(waserror())
		return -1;

	c = namec(path, Aaccess, 0, 0);
	if(waserror()){
		c->type = 0;		/* see below */
		cclose(c);
		nexterror();
	}
	devtab[c->type]->remove(c);
	/*
	 * Remove clunks the fid, but we need to recover the Chan
	 * so fake it up.  rootclose() is known to be a nop.
	 */
	c->type = 0;
	poperror();
	cclose(c);
	poperror();
	return 0;
}

long
kwstat(char *path, char *buf)
{
	Chan *c;

	c = 0;
	if(waserror()) {
		cclose(c);
		return -1;
	}

	/* name is known to be first member */
	nameok(buf);
	c = namec(path, Aaccess, 0, 0);
	devtab[c->type]->wstat(c, buf);
	cclose(c);
	poperror();
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

long
kfwstat(int fd, char *buf)
{
	Chan *c;

	c = 0;
	if(waserror()) {
		cclose(c);
		nexterror();
	}
	nameok(buf);
	c = fdtochan(up->env->fgrp, fd, -1, 1, 1);
	devtab[c->type]->wstat(c, buf);
	cclose(c);
	poperror();
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
