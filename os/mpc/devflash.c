#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

#include "flashif.h"

/*
 * flash memory
 */
static struct
{
	Flash*	card;	/* actual card type, reset for access */
	Flash*	types;	/* possible card types */
}flash;

enum{
	Qdir,
	Qflash,
	Qflashctl,
	Qflashstat,
};

static Dirtab flashdir[]={
	"flash",		{Qflash, 0}, 0, 0666,
	"flashctl",	{Qflashctl, 0}, 0, 0666,
	"flashstat", {Qflashstat, 0}, 0, 0444,
};

static	int	eraseflash(int);
static	long	readflash(void*, long, int);
static	long	writeflash(long, void*,int);

static void
flashreset(void)
{
	Flash *f;
	char type[NAMELEN];
	long length;
	void *addr;

	type[0] = 0;
	if(archflashreset(type, &addr, &length) < 0 || type[0] == 0)
		return;
	for(f = flash.types; f != nil; f = f->next)
		if(strcmp(type, f->name) == 0)
			break;
	if(f == nil){
		print("#F: no flash driver for type %s (addr 0x%lux)\n", type, addr);
		return;
	}
	f->addr = addr;
	f->size = length;
	if(f->reset(f) == 0){
		flash.card = f;
		print("#F: %s addr 0x%lux len %lud\n", f->name, (ulong)FLASHMEM, f->size);
		flashdir[0].length = f->size;	/* length of data file */
	}
}

static Chan*
flashattach(char *spec)
{
	return devattach('F', spec);
}

static int	 
flashwalk(Chan *c, char *name)
{
	return devwalk(c, name, flashdir, nelem(flashdir), devgen);
}

static void	 
flashstat(Chan *c, char *dp)
{
	devstat(c, dp, flashdir, nelem(flashdir), devgen);
}

static Chan*
flashopen(Chan *c, int omode)
{
	omode = openmode(omode);
	switch(c->qid.path){
	case Qflash:
	case Qflashctl:
		if(flash.card == nil)
			error(Enodev);
		if(strcmp(up->env->user, eve)!=0)
			error(Eperm);
		break;
	case Qflashstat:
		if(omode!=OREAD)
			error(Eperm);
		break;
	}
	return devopen(c, omode, flashdir, nelem(flashdir), devgen);
}

static void	 
flashclose(Chan*)
{
}

static long	 
flashread(Chan *c, void *buf, long n, ulong offset)
{
	Flash *f;
	char *s;

	if(c->qid.path & CHDIR)
		return devdirread(c, buf, n, flashdir, nelem(flashdir), devgen);

	switch(c->qid.path){
	case Qflash:
		if(offset >= flash.card->size)
			return 0;
		if(offset+n > flash.card->size)
			n = flash.card->size - offset;
		n = readflash(buf, offset, n);
		if(n < 0)
			error(Eio);
		return n;
	case Qflashctl:
		return 0;
	case Qflashstat:
		f = flash.card;
		if(f == nil)
			return 0;
		s = malloc(READSTR);
		if(waserror()){
			free(s);
			nexterror();
		}
		snprint(s, READSTR, "%2.2ux %2.2ux %1.1d %8.8ux %8.8lux\n",
			f->id, f->devid, f->width, f->erasesize, f->size);
		n = readstr(offset, buf, n, s);
		poperror();
		free(s);
		return n;
	}
	error(Egreg);
	return 0;		/* not reached */
}

static long	 
flashwrite(Chan *c, void *buf, long n, ulong offset)
{
	char cmd[50], *fields[5];
	char *cp;
	int i;
	ulong addr;

	switch(c->qid.path){
	case Qflash:
		if(flash.card == nil)
			error(Enodev);
		if(flash.card->write == nil)
			error(Eperm);
		if(offset >= flash.card->size)
			return 0;
		if(offset+n > flash.card->size)
			n = flash.card->size - offset;
		return writeflash(offset, buf, n);
	case Qflashctl:
		if(flash.card == nil)
			error(Enodev);
		if(n > sizeof(cmd)-1)
			n = sizeof(cmd)-1;
		memmove(cmd, buf, n);
		cmd[n] = 0;
		i = parsefields(cmd, fields, nelem(fields), " \t\n");
		if(i < 2)
			error(Ebadarg);
		if(strcmp(fields[0], "erase") == 0){
			if(strcmp(fields[1], "all") != 0){
				addr = strtoul(fields[1], &cp, 0);
				if(*cp || (addr&(flash.card->erasesize-1))!=0 || addr >= flash.card->size)
					error(Ebadarg);
				if(eraseflash(addr/flash.card->erasesize) < 0)
					error(Eio);
			}else
				if(eraseflash(-1) < 0)
					error(Eio);
		}else
			error(Ebadarg);
		return n;
	}
	error(Egreg);
	return 0;		/* not reached */
}

Dev flashdevtab = {
	'F',
	"flash",

	flashreset,
	devinit,
	flashattach,
	devdetach,
	devclone,
	flashwalk,
	flashstat,
	flashopen,
	devcreate,
	flashclose,
	flashread,
	devbread,
	flashwrite,
	devbwrite,
	devremove,
	devwstat,
};

/*
 * called by flash card types named in link section (eg, flashamd.c)
 */
void
addflashcard(char *name, int (*reset)(Flash*))
{
	Flash *f, **l;

	f = (Flash*)malloc(sizeof(*f));
	memset(f, 0, sizeof(*f));
	f->name = name;
	f->reset = reset;
	f->next = nil;
	for(l = &flash.types; *l != nil; l = &(*l)->next)
		;
	*l = f;
}

/*
 * internal interface, used by devflash but might also be needed
 * on some platforms for access to parameter flash.
 */
static long
readflash(void *buf, long offset, int n)
{
	Flash *f;

	f = flash.card;
	if(f == nil || offset < 0 || offset+n > f->size)
		return -1;
	qlock(f);
	if(waserror()){
		qunlock(f);
		nexterror();
	}
	memmove(buf, (uchar*)f->addr+offset, n);	/* assumes hardware supports byte access */
	poperror();
	qunlock(f);
	return n;
}

static long
writeflash(long offset, void *buf, int n)
{
	Flash *f;
	uchar tmp[16];
	uchar *p;
	ulong o;
	int r, width, wmask;

	f = flash.card;
	if(f == nil || f->write == nil || offset < 0 || offset+n > f->size)
		return -1;
	width = f->width;
	wmask = width-1;
	qlock(f);
	if(waserror()){
		qunlock(f);
		return -1;
	}
	p = buf;
	if(offset&wmask){
		o = offset & ~wmask;
		memmove(tmp, (uchar*)f->addr+o, width);
		for(; n > 0 && offset&wmask; n--)
			tmp[offset++&wmask] = *p++;
		if(f->write(f, o, (ulong*)tmp, width))
			error(Eio);
	}
	r = n&wmask;
	n &= ~wmask;
	if(n){
		if(f->write(flash.card, offset, (ulong*)p, n))
			error(Eio);
		offset += n;
		p += n;
	}
	if(r){
		memmove(tmp, (uchar*)f->addr+offset, width);
		memmove(tmp, p, r);
		if(f->write(flash.card, offset, (ulong*)tmp, width))
			error(Eio);
		p += r;
	}
	poperror();
	qunlock(f);
	return p-(uchar*)buf;
}

static int
eraseflash(int n)
{
	Flash *f;
	int r;

	f = flash.card;
	if(f == nil)
		return -1;
	qlock(f);
	if(waserror()){
		qunlock(f);
		return -1;
	}
	if(n < 0)
		r = f->eraseall(f);
	else
		r = f->erasezone(f, n);
	poperror();
	qunlock(f);
	return r;
}
