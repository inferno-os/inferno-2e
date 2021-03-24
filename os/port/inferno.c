#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "isa.h"
#include "interp.h"
#include "runt.h"
#include "kernel.h"

/*
 * here because Sys_FileIO is not public
 */
extern	int	srvf2c(char*, char*, Sys_FileIO*);

/*
 * System types connected to gc
 */
uchar	FDmap[] = Sys_FD_map;
uchar	FileIOmap[] = Sys_FileIO_map;
void	freeFD(Heap*, int);
void	freeFileIO(Heap*, int);
Type*	TFD;
Type*	TFileIO;

static	uchar	rmap[] = Sys_FileIO_read_map;
static	uchar	wmap[] = Sys_FileIO_write_map;
static	Type*	FioTread;
static	Type*	FioTwrite;

typedef struct FD FD;
struct FD
{
	Sys_FD	fd;
	Fgrp*	grp;
};

void
sysinit(void)
{
	TFD = dtype(freeFD, sizeof(FD), FDmap, sizeof(FDmap));
	TFileIO = dtype(freeFileIO, Sys_FileIO_size, FileIOmap, sizeof(FileIOmap));

	FioTread = dtype(freeheap, Sys_FileIO_read_size, rmap, sizeof(rmap));
	FioTwrite = dtype(freeheap, Sys_FileIO_write_size, wmap, sizeof(wmap));
}

void
freeFD(Heap *h, int swept)
{
	FD *handle;

	USED(swept);

	handle = H2D(FD*, h);

	release();
	kfgrpclose(handle->grp, handle->fd.fd);
	closefgrp(handle->grp);
	acquire();
}

void
freeFileIO(Heap *h, int swept)
{
	Sys_FileIO *fio;

	if(swept)
		return;

	fio = H2D(Sys_FileIO*, h);
	destroy(fio->read);
	destroy(fio->write);
}

Sys_FD*
mkfd(int fd)
{
	Heap *h;
	Fgrp *fg;
	FD *handle;

	h = heap(TFD);
	handle = H2D(FD*, h);
	handle->fd.fd = fd;
	fg = up->env->fgrp;
	handle->grp = fg;
	incref(fg);
	return (Sys_FD*)handle;
}
#define fdchk(x)	((x) == (Sys_FD*)H ? -1 : (x)->fd)

char*
syserr(char *s, char *es, Prog *p)
{
	Osenv *o;

	o = p->osenv;
	strncpy(s, o->error, es - s);
	return s + strlen(o->error);
}

void
Sys_millisec(void *fp)
{
	F_Sys_millisec *f;

	f = fp;
	*f->ret = TK2MS(MACHP(0)->ticks);
}

void
Sys_open(void *fp)
{
	int fd;
	F_Sys_open *f;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;
	release();
	fd = kopen(string2c(f->s), f->mode);
	acquire();
	if(fd == -1)
		return;

	*f->ret = mkfd(fd);
}

void
Sys_pipe(void *fp)
{
	Array *a;
	int fd[2];
	Sys_FD **sfd;
	F_Sys_pipe *f;

	f = fp;
	*f->ret = -1;

	a = f->fds;
	if(a->len < 2)
		return;
	if(kpipe(fd) < 0)
		return;

	sfd = (Sys_FD**)a->data;
	destroy(sfd[0]);
	destroy(sfd[1]);
	sfd[0] = H;
	sfd[1] = H;
	sfd[0] = mkfd(fd[0]);
	sfd[1] = mkfd(fd[1]);
	*f->ret = 0;
}

void
Sys_fildes(void *fp)
{
	F_Sys_fildes *f;
	int fd;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;
	release();
	fd = kdup(f->fd, -1);
	acquire();
	if(fd == -1)
		return;
	*f->ret = mkfd(fd);
}

void
Sys_dup(void *fp)
{
	F_Sys_dup *f;

	f = fp;
	release();
	*f->ret = kdup(f->old, f->new);	
	acquire();
}

void
Sys_create(void *fp)
{
	int fd;
	F_Sys_create *f;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;
	release();
	fd = kcreate(string2c(f->s), f->mode, f->perm);
	acquire();
	if(fd == -1)
		return;

	*f->ret = mkfd(fd);
}

void
Sys_remove(void *fp)
{
	F_Sys_remove *f;

	f = fp;
	release();
	*f->ret = kremove(string2c(f->s));
	acquire();
}

void
Sys_seek(void *fp)
{
	F_Sys_seek *f;

	f = fp;
	release();
	*f->ret = kseek(fdchk(f->fd), f->off, f->start);
	acquire();
}

void
Sys_unmount(void *fp)
{
	F_Sys_unmount *f;

	f = fp;
	release();
	*f->ret = kunmount(string2c(f->s1), string2c(f->s2));
	acquire();
}

void
Sys_read(void *fp)
{
	int n;
	F_Sys_read *f;

	f = fp;
	n = f->n;
	if(f->buf == (Array*)H) {
		*f->ret = 0;
		return;		
	}
	if(n > f->buf->len)
		n = f->buf->len;

	release();
	*f->ret = kread(fdchk(f->fd), f->buf->data, n);
	acquire();
}

void
Sys_chdir(void *fp)
{
	F_Sys_chdir *f;

	f = fp;
	release();
	*f->ret = kchdir(string2c(f->path));
	acquire();
}

void
Sys_write(void *fp)
{
	int n;
	F_Sys_write *f;

	f = fp;
	n = f->n;
	if(f->buf == (Array*)H) {
		*f->ret = 0;
		return;		
	}
	if(n > f->buf->len)
		n = f->buf->len;

	release();
	*f->ret = kwrite(fdchk(f->fd), f->buf->data, n);
	acquire();
}

static void
unpackdir(Dir *d, Sys_Dir *sd)
{
	retstr(d->name, &sd->name);
	retstr(d->uid, &sd->uid);
	retstr(d->gid, &sd->gid);
	sd->qid.path = d->qid.path;
	sd->qid.vers = d->qid.vers;
	sd->mode = d->mode;
	sd->atime = d->atime;
	sd->mtime = d->mtime;
	sd->length = d->length;
	sd->dtype = d->type;
	sd->dev = d->dev;
}

static void
packdir(Dir *d, Sys_Dir *sd)
{
	strncpy(d->name, string2c(sd->name), sizeof(d->name));
	strncpy(d->uid, string2c(sd->uid), sizeof(d->uid));
	strncpy(d->gid, string2c(sd->gid), sizeof(d->gid));
	d->qid.path = sd->qid.path;
	d->qid.vers = sd->qid.vers;
	d->mode = sd->mode;
	d->atime = sd->atime;
	d->mtime = sd->mtime;
	d->length = sd->length;
	d->type = sd->dtype;
	d->dev = sd->dev;
}

void
Sys_fstat(void *fp)
{
	Dir d;
	F_Sys_fstat *f;

	f = fp;
	release();
	f->ret->t0 = kdirfstat(fdchk(f->fd), &d);
	acquire();
	if(f->ret->t0 < 0)
		return;
	unpackdir(&d, &f->ret->t1);
}

void
Sys_stat(void *fp)
{
	Dir d;
	F_Sys_stat *f;

	f = fp;
	release();
	f->ret->t0 = kdirstat(string2c(f->s), &d);
	acquire();
	if(f->ret->t0 < 0)
		return;
	unpackdir(&d, &f->ret->t1);
}

void
Sys_mount(void *fp)
{
	F_Sys_mount *f;

	f = fp;
	release();
	*f->ret = kmount(fdchk(f->fd), string2c(f->on), f->flags, string2c(f->spec));
	acquire();
}

void
Sys_bind(void *fp)
{
	F_Sys_bind *f;

	f = fp;
	release();
	*f->ret = kbind(string2c(f->s), string2c(f->on), f->flags);
	acquire();
}

void
Sys_wstat(void *fp)
{
	Dir d;
	F_Sys_wstat *f;

	f = fp;
	packdir(&d, &f->d);
	release();
	*f->ret = kdirwstat(string2c(f->s), &d);
	acquire();
}

void
Sys_fwstat(void *fp)
{
	Dir d;
	F_Sys_fwstat *f;

	f = fp;
	packdir(&d, &f->d);
	release();
	*f->ret = kdirfwstat(fdchk(f->fd), &d);
	acquire();
}

void
Sys_print(void *fp)
{
	int n;
	Prog *p;
	Chan *c;
	char buf[1024];
	F_Sys_print *f;
	f = fp;
	c = up->env->fgrp->fd[1];
	if(c == nil)
		return;
	p = currun();

	release();
	n = xprint(p, f, &f->vargs, f->s, buf, sizeof(buf));
	*f->ret = kwrite(1, buf, n);
	acquire();
}

void
Sys_fprint(void *fp)
{
	int n;
	Prog *p;
	char buf[1024];
	F_Sys_fprint *f;

	f = fp;
	p = currun();
	release();
	n = xprint(p, f, &f->vargs, f->s, buf, sizeof(buf));
	*f->ret = kwrite(fdchk(f->fd), buf, n);
	acquire();
}

void
Sys_dial(void *fp)
{
	int cfd;
	char dir[40], *a, *l;
	F_Sys_dial *f;

	f = fp;
	a = string2c(f->addr);
	l = string2c(f->local);
	release();
	f->ret->t0 = kdial(a, l, dir, &cfd);
	acquire();
	destroy(f->ret->t1.dfd);
	f->ret->t1.dfd = H;
	destroy(f->ret->t1.cfd);
	f->ret->t1.cfd = H;
	if(f->ret->t0 == -1)
		return;

	f->ret->t1.dfd = mkfd(f->ret->t0);
	f->ret->t1.cfd = mkfd(cfd);
	retstr(dir, &f->ret->t1.dir);
}

void
Sys_announce(void *fp)
{
	char dir[40], *a;
	F_Sys_announce *f;

	f = fp;
	a = string2c(f->addr);
	release();
	f->ret->t0 = kannounce(a, dir);
	acquire();
	destroy(f->ret->t1.dfd);
	f->ret->t1.dfd = H;
	destroy(f->ret->t1.cfd);
	f->ret->t1.cfd = H;
	if(f->ret->t0 == -1)
		return;

	f->ret->t1.cfd = mkfd(f->ret->t0);
	retstr(dir, &f->ret->t1.dir);
}

void
Sys_listen(void *fp)
{
	F_Sys_listen *f;
	char dir[40], *d;

	f = fp;
	d = string2c(f->c.dir);
	release();
	f->ret->t0 = klisten(d, dir);
	acquire();

	destroy(f->ret->t1.dfd);
	f->ret->t1.dfd = H;
	destroy(f->ret->t1.cfd);
	f->ret->t1.cfd = H;
	if(f->ret->t0 == -1)
		return;

	f->ret->t1.cfd = mkfd(f->ret->t0);
	retstr(dir, &f->ret->t1.dir);
}

void
Sys_sleep(void *fp)
{
	F_Sys_sleep *f;

	f = fp;
	release();
	if(f->period > 0){
		tsleep(&up->sleep, return0, 0, f->period);
	}
	acquire();
	*f->ret = 0;
}

void
Sys_stream(void *fp)
{
	Prog *p;
	uchar *buf;
	int src, dst;
	F_Sys_stream *f;
	int nbytes, t, n;

	f = fp;
	buf = malloc(f->bufsiz);
	if(buf == nil) {
		*f->ret = -1;
		return;
	}

	src = fdchk(f->src);
	dst = fdchk(f->dst);

	p = currun();

	release();
	t = 0;
	nbytes = 0;
	while(p->kill == nil) {
		n = kread(src, buf+t, f->bufsiz-t);
		if(n <= 0)
			break;
		t += n;
		if(t >= f->bufsiz) {
			if(kwrite(dst, buf, t) != t) {
				t = 0;
				break;
			}

			nbytes += t;
			t = 0;
		}
	}
	if(t != 0) {
		kwrite(dst, buf, t);
		nbytes += t;
	}
	acquire();
	free(buf);
	*f->ret = nbytes;
}

void
Sys_export(void *fp)
{
	F_Sys_export *f;

	f = fp;
	release();
	*f->ret = export(fdchk(f->c), f->flag&Sys_EXPASYNC, f->flag&Sys_EXPEXCL);
	acquire();
}

void
Sys_file2chan(void *fp)
{
	int r;
	Heap *h;
	Channel *c;
	Sys_FileIO *fio;
	F_Sys_file2chan *f;
	void *sv;

	h = heap(TFileIO);

	fio = H2D(Sys_FileIO*, h);

	c = cnewc(movtmp);
	c->mid.t = FioTread;
	FioTread->ref++;
	fio->read = c;

	c = cnewc(movtmp);
	c->mid.t = FioTwrite;
	FioTwrite->ref++;
	fio->write = c;

	f = fp;
	sv = *f->ret;
	*f->ret = fio;
	destroy(sv);

	release();
	r = srvf2c(string2c(f->dir), string2c(f->file), fio);
	acquire();
	if(r == -1) {
		*f->ret = H;
		destroy(fio);
	}
}

void
Sys_pctl(void *fp)
{
	int fd;
	Prog *p;
	List *l;
	Chan *c;
	Pgrp *np;
	Chan *dot;
	Osenv *o;
	F_Sys_pctl *f;
	Fgrp *fg, *ofg, *nfg;

	f = fp;

	p = currun();
	if (f->flags & (Sys_NEWFD|Sys_FORKFD|Sys_NEWNS|Sys_FORKNS))
	release();
	o = p->osenv;
	if(f->flags & Sys_NEWFD) {
		ofg = o->fgrp;
		nfg = newfgrp();
		/* file descriptors to preserve */
		for(l = f->movefd; l != H; l = l->tail) {
			fd = *(int*)l->data;
			if(fd >= 0 || fd < ofg->maxfd) {
				c = ofg->fd[fd];
				incref(c);
				nfg->fd[fd] = c;
				if(nfg->maxfd < fd)
					nfg->maxfd = fd;
			}
		}
		closefgrp(ofg);
		o->fgrp = nfg;
	}
	else
	if(f->flags & Sys_FORKFD) {
		fg = dupfgrp(o->fgrp);
		/* file descriptors to close */
		for(l = f->movefd; l != H; l = l->tail)
			kclose(*(int*)l->data);
		closefgrp(o->fgrp);
		o->fgrp = fg;
	}

	if(f->flags & Sys_NEWNS) {
		np = newpgrp();
		dot = o->pgrp->dot;
		np->dot = cclone(dot, nil);
		np->slash = cclone(dot, nil);
		np->pin = o->pgrp->pin;		/* pin is ALWAYS inherited */
		closepgrp(o->pgrp);
		o->pgrp = np;
	}
	else
	if(f->flags & Sys_FORKNS) {
		np = newpgrp();
		pgrpcpy(np, o->pgrp);
		closepgrp(o->pgrp);
		o->pgrp = np;
	}

	if (f->flags & (Sys_NEWFD|Sys_FORKFD|Sys_NEWNS|Sys_FORKNS))
		acquire();

	if(f->flags & Sys_NEWPGRP)
		p->grp = p->pid;

	if(f->flags & Sys_NODEVS)
		o->pgrp->nodevs = 1;

	*f->ret = p->pid;
}

void
Sys_dirread(void *fp)
{
	Dir *b;
	uchar *d;
	int i, n;
	F_Sys_dirread *f;

	f = fp;
	b = malloc(sizeof(Dir)*f->dir->len);
	if(b == nil) {
		kwerrstr(Enomem);
		*f->ret = -1;
		return;
	}
	release();
	n = kdirread(fdchk(f->fd), b, sizeof(Dir)*f->dir->len);
	acquire();
	if(n <= 0) {
		*f->ret = n;
		free(b);
		return;
	}
	n /= sizeof(Dir);
	if(f->dir != H) {
		d = f->dir->data;
		for(i = 0; i < n; i++) {
			unpackdir(b+i, (Sys_Dir*)d);
			d += Sys_Dir_size;
		}
	}		
	free(b);
	*f->ret = n;	
}

void
ccom(Prog **cl, Prog *p)
{
	Prog *f, **v;
	volatile struct {Prog **cl;} vcl;

	p->comm = nil;
	v = cl;
	for(f = *v; f != nil; f = f->comm)
		v = &f->comm;
	*v = p;
	vcl.cl = cl;
	if(waserror()) {
		if(p->ptr != nil) {	/* no killcomm */
			v = vcl.cl;
			for(f = *v; f; f = f->comm) {
				if(f == p) {
					*v = p->comm;
					p->ptr = nil;
					break;
				}
				v = &f->comm;
			}
		}
		nexterror();
	}
	cblock(p);
	poperror();
}

void
crecv(Channel *c, void *ip)
{
	Prog *p;
	REG rsav;

	if(c->send == nil && c->sendalt == nil && c->sendq == H) {
		p = currun();
		p->ptr = ip;
		ccom(&c->recv, p);
		return;
	}
	rsav = R;
	R.s = &c;
	R.d = ip;
	irecv();
	R = rsav;
}
