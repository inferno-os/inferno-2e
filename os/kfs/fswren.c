#include "all.h"

enum
{
	MAXWREN = 7,
};

#define WMAGIC	"kfs wren device\n"

typedef struct Wren	Wren;
struct Wren
{
	QLock;
	Device	dev;
	ulong	size;
	Chan*	io;
	int	usable;
};

static Wren*	wrens;
static int	maxwren;
static QLock	wrenlock;
int		nwren;

static Wren *
wren(Device dev)
{
	int i;

	for(i = 0; i < maxwren; i++) {
		if(devcmp(dev, wrens[i].dev) == 0)
			return &wrens[i];
	}

	panic("can't find wren for %Z", dev);
	return 0;
}

/*
 * find out the length of a file
 * given the mesg version of a stat buffer
 * we call this because fsconvM2D is different
 * for the file system than in the os
 */
ulong
statlen(char *ap)
{
	uchar *p;

	p = (uchar*)ap;
	p += 3*NAMELEN+5*4;
	return p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24);
}

char *
wreninit(Device dev, char *name, int magicok)
{
#define BFSZ (8*1024)
	char *buf, d[DIRREC];
	Wren *w;
	int fd, i, bsize;

	if(wrens == 0)
		wrens = fsalloc(MAXWREN * sizeof *wrens);
	qlock(&wrenlock);
	if(waserror()){
		qunlock(&wrenlock);
		return up->env->error;
	}
	w = &wrens[maxwren];
	fd = kopen(name, ORDWR);
	if(fd < 0)
		error("can't open");

	if(kfstat(fd, d) < 0)
		error("can't stat");

	kseek(fd, 0, 0);
	buf = malloc(BFSZ);
	if(buf == nil)
		nexterror();
	if(waserror()){
		free(buf);
		nexterror();
	}
	i = kread(fd, buf, 8*1024);
	if(i < BFSZ)
	{
		if (!magicok)
			error("can't read config block");
		i = 1;
		SET(bsize);
	}
	else
	{
		i = strncmp(buf+256, WMAGIC, strlen(WMAGIC));
		bsize = strtol(buf+256+strlen(WMAGIC), 0, 0);
	}
	free(buf);
	poperror();
	if(i == 0) {
		if(bsize % 512) {
			cprint("kfs: bad buffer size in config block (%d)\n", bsize);
			error("bad block size in config block");
		}
		if(bsize != RBUFSIZE){
			cprint("kfs: %s: block size %d not system's %d\n", bsize, RBUFSIZE);
			error("block size differs from system's");
		}
		w->usable = 1;
	}else{
		if(!magicok)
			error("bad magic");
		w->usable = 0;
	}
	w->dev = dev;
	w->size = statlen(d);
	w->io = fdtochan(up->env->fgrp, fd, ORDWR, 0, 1);
	kclose(fd);
	maxwren++;
cprint("kfs %s: opened: dev %Z size %d bsize %d ok %d\n", name, dev, w->size, RBUFSIZE, w->usable);
	qunlock(&wrenlock);
	poperror();
	return nil;
}

void
wrenream(Device dev)
{
	char *buf;

	if(RBUFSIZE % 512)
		panic("kfs: bad buffersize(%d): restart a multiple of 512\n", RBUFSIZE);
	cprint("kfs: reaming the file system using %d byte blocks\n", RBUFSIZE);
	buf = malloc(RBUFSIZE);
	if(buf == 0)
		panic("wrenream: no buf");
	memset(buf, 0, RBUFSIZE);
	sprint(buf+256, "%s%d\n", WMAGIC, RBUFSIZE);
	if(wrenwrite(dev, 0, buf))
		panic("can't ream disk");
}

int
wrentag(char *p, int tag, long qpath)
{
	Tag *t;

	t = (Tag*)(p+BUFSIZE);
	return t->tag != tag || (qpath&~QPDIR) != t->path;
}

char*
wrencheck(Device dev)
{
	char *buf;

	if(!wren(dev)->usable)
		return "bad config block";
	buf = malloc(RBUFSIZE);
	if(buf == 0)
		panic("wrencheck: no buf");
	if(wrenread(dev, wrensuper(dev), buf) || wrentag(buf, Tsuper, QPSUPER)
	|| wrenread(dev, wrenroot(dev), buf) || wrentag(buf, Tdir, QPROOT)){
		free(buf);
		return "super/root tag invalid";
	}
	if(((Dentry *)buf)[0].mode & DALLOC){
		free(buf);
		return nil;
	}
	free(buf);
	return "root not alloc";
}

long
wrensize(Device dev)
{
	return wren(dev)->size / RBUFSIZE;
}

long
wrensuper(Device dev)
{
	USED(dev);
	return 1;
}

long
wrenroot(Device dev)
{
	USED(dev);
	return 2;
}

int
wrenread(Device dev, long addr, void *b)
{
	Wren *w;
	int i;

	w = wren(dev);
	qlock(w);
	w->io->offset = addr*RBUFSIZE;
	i = kchanio(w->io, b, RBUFSIZE, OREAD) != RBUFSIZE;
	qunlock(w);
	return i;
}

int
wrenwrite(Device dev, long addr, void *b)
{
	Wren *w;
	int i;

	w = wren(dev);
	qlock(w);
	w->io->offset = addr*RBUFSIZE;
	i = kchanio(w->io, b, RBUFSIZE, OWRITE) != RBUFSIZE;
	qunlock(w);
	return i;
}
