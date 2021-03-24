/*
 * i2c
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/error.h"

enum{
	Qdir,
	Qdata,
	Qctl,
};

static
Dirtab i2ctab[]={
	"i2cdata",		{Qdata, 0},	256,	0600,
	"i2cctl",		{Qctl, 0},		0,	0600,
};

typedef struct Idev Idev;
struct Idev {
	Ref;
	QLock;
	int	addr;
	int	needsa;	/* device uses/needs subaddressing */
	Dirtab	tab[nelem(i2ctab)];
};

static void
i2creset(void)
{
	i2csetup();
}

static Chan*
i2cattach(char* spec)
{
	char *s;
	int addr;
	Idev *d;
	Chan *c;

	addr = strtoul(spec, &s, 0);
	if(*spec == 0 || *s || (addr & ~0xFE) != 0)
		error(Enodev);
	d = malloc(sizeof(Idev));
	if(d == nil)
		error(Enomem);
	d->ref = 1;
	d->addr = addr;
	d->needsa = 0;
	memmove(d->tab, i2ctab, sizeof(d->tab));
	sprint(d->tab[0].name, "i2c%ddata", addr);
	sprint(d->tab[1].name, "i2c%dctl", addr);

	c = devattach('J', spec);
	c->aux = d;
	return c;
}

static Chan *
i2cclone(Chan *c, Chan *nc)
{
	Idev *d;

	nc = devclone(c, nc);
	d = nc->aux;
	incref(d);
	return nc;
}

static int
i2cwalk(Chan* c, char* name)
{
	Idev *d;

	d = c->aux;
	return devwalk(c, name, d->tab, nelem(d->tab), devgen);
}

static void
i2cstat(Chan* c, char* db)
{
	Idev *d;

	d = c->aux;
	devstat(c, db, d->tab, nelem(d->tab), devgen);
}

static Chan*
i2copen(Chan* c, int omode)
{
	Idev *d;

	d = c->aux;
	return devopen(c, omode, d->tab, nelem(d->tab), devgen);
}

static void
i2cclose(Chan *c)
{
	Idev *d;

	d = c->aux;
	if(decref(d) == 0)
		free(d);
}

static long
i2cread(Chan *c, void *a, long n, ulong offset)
{
	Idev *d;
	int addr;
	ulong len;

	switch(c->qid.path & ~CHDIR){
	case Qdir:
		return devdirread(c, a, n, i2ctab, nelem(i2ctab), devgen);
	case Qdata:
		d = c->aux;
		if(offset != 0 && !d->needsa)
			error(Eio);
		len = d->tab[0].length;
		if(offset+n >= len){
			n = len - offset;
			if(n <= 0)
				break;
		}
		if(d->needsa)
			addr = (offset<<8) | d->addr | 1;
		else
			addr = d->addr;
		qlock(d);
		if(waserror()){
			qunlock(d);
			nexterror();
		}
		n = i2crecv(addr, a, n);
		poperror();
		qunlock(d);
		break;
	case Qctl:
	default:
		n=0;
		break;
	}
	return n;
}

static long
i2cwrite(Chan *c, void *a, long n, ulong offset)
{
	char cmd[32], *fields[4];
	Idev *d;
	int addr, i;
	ulong len;

	USED(offset);
	switch(c->qid.path & ~CHDIR){
	case Qdata:
		d = c->aux;
		if(offset != 0 && !d->needsa)
			error(Eio);
		len = d->tab[0].length;
		if(offset+n >= len){
			n = len - offset;
			if(n <= 0)
				break;
		}
		if(d->needsa)
			addr = (offset<<8) | d->addr | 1;
		else
			addr = d->addr;
		qlock(d);
		if(waserror()){
			qunlock(d);
			nexterror();
		}
		n = i2csend(addr, a, n);
		poperror();
		qunlock(d);
		break;
	case Qctl:
		d = c->aux;
		if(n > sizeof(cmd)-1)
			n = sizeof(cmd)-1;
		memmove(cmd, a, n);
		cmd[n] = 0;
		i = parsefields(cmd, fields, nelem(fields), " \t\n");
		if(i > 0 && strcmp(fields[0], "subaddress") == 0){
			d->needsa = i>1? strtol(fields[1], nil, 0): 1;
		}else if(i > 1 && strcmp(fields[0], "size") == 0){
			len = strtoul(fields[1], nil, 0);
			if(len == 0 || len > 256)
				error(Ebadarg);
			d->tab[0].length = len;
		}else
			error(Ebadarg);
		break;
	default:
		error(Ebadusefd);
	}
	return n;
}

Dev i2cdevtab = {
	'J',
	"i2c",

	i2creset,
	devinit,
	i2cattach,
	devdetach,
	i2cclone,
	i2cwalk,
	i2cstat,
	i2copen,
	devcreate,
	i2cclose,
	i2cread,
	devbread,
	i2cwrite,
	devbwrite,
	devremove,
	devwstat,
};
