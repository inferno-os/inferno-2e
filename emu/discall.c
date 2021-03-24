#include "dat.h"
#include "fns.h"
#include <interp.h>

void*
libqlowner(void *l)
{
	QLock *q;

	q = l;
	return q->owner;
}

void
libqlock(void *l)
{
	Prog *p;
	QLock *q;

	q = l;
	p = currun();
	if(p == nil)
		abort();

	if(!canqlock(q)) {
		release();
		qlock(q);
		acquire();
	}
	q->owner = p;
}

void
libqunlock(void *l)
{
	Prog *p;
	QLock *q;

	q = l;
	p = currun();
	if(p == nil)
		abort();
	if(q->owner != p)
		abort();

	q->owner = nil;
	qunlock(q);
}

void*
libqlalloc(void)
{
	QLock *q;

	q = malloc(sizeof(QLock));
	return q;
}

void
libqlfree(void *l)
{
	free(l);
}

int
libread(int fd, void *buf, int n)
{
	release();
	n = kread(fd, buf, n);
	acquire();
	return n;
}

int
libreadn(int fd, void *av, long n)
{
	char *a;
	long m, t;

	a = av;
	t = 0;
	release();
	while(t < n){
		m = kread(fd, a+t, n-t);
		if(m <= 0){
			if(t == 0){
				acquire();
				return m;
			}
			break;
		}
		t += m;
	}
	acquire();
	return t;
}

int
libwrite(int fd, void *buf, int n)
{
	release();
	n = kwrite(fd, buf, n);
	acquire();
	return n;
}

int
libopen(char *name, int omode)
{
	int fd;

	release();
	fd = kopen(name, omode);
	acquire();
	return fd;
}

int
libclose(int fd)
{
	release();
	fd = kclose(fd);
	acquire();
	return fd;
}

int
libdirfstat(int fd, Dir *dir)
{
	int n;

	release();
	n = kdirfstat(fd, dir);
	acquire();
	return n;
}

int
libbind(char *s, char *t, int f)
{
	int n;

	release();
	n = kbind(s, t, f);
	acquire();
	return n;
}

void
libchanclose(void *chan)
{
	release();
	cclose(chan);
	acquire();
}

void*
libfdtochan(int fd, int mode)
{
	Chan *c;

	release();
	if(waserror()) {
		acquire();
		return nil;
	}
	c = fdtochan(up->env->fgrp, fd, mode, 0, 1);
	poperror();
	acquire();
	return c;
}
