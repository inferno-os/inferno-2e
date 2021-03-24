#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

extern Rootdata rootdata[];
extern Dirtab roottab[];
extern int	rootmaxq;

static Chan*
rootattach(char *spec)
{
	int i;

	for (i = 0; i < rootmaxq; i++)
		if (rootdata[i].sizep)
			rootdata[i].size = roottab[i].length = *rootdata[i].sizep;

	return devattach('/', spec);
}

static int	 
rootwalk(Chan *c, char *name)
{
	ulong p;

	p = c->qid.path & ~CHDIR;
	if(strcmp(name, "..") == 0) {
		c->qid.path = rootdata[p].dotdot | CHDIR;
		return 1;
	}
	if(c->qid.path & CHDIR)
		return devwalk(c, name, rootdata[p].ptr, rootdata[p].size, devgen);
	else
		return 0;
}

static void
rootstat(Chan *c, char *dp)
{
	int p;

	p = rootdata[c->qid.path & ~CHDIR].dotdot;
	devstat(c, dp, rootdata[p].ptr, rootdata[p].size, devgen);
}

static Chan*
rootopen(Chan *c, int omode)
{
	int p;

	p = rootdata[c->qid.path & ~CHDIR].dotdot;
	return devopen(c, omode, rootdata[p].ptr, rootdata[p].size, devgen);
}

/*
 * sysremove() knows this is a nop
 */
static void	 
rootclose(Chan*)
{
}

static long	 
rootread(Chan *c, void *buf, long n, ulong offset)
{
	ulong p;
	ulong len;
	uchar *data;

	p = c->qid.path & ~CHDIR;
	if(c->qid.path & CHDIR)
		return devdirread(c, buf, n, rootdata[p].ptr, rootdata[p].size, devgen);
	data = rootdata[p].ptr;
	len = rootdata[p].size;
	if(offset >= len)
		return 0;
	if(offset+n > len)
		n = len - offset;
	memmove(buf, data+offset, n);
	return n;
}

static long	 
rootwrite(Chan*, void*, long, ulong)
{
	error(Eperm);
	return 0;
}

Dev rootdevtab = {					/* defaults in dev.c */
	'/',
	"root",

	devreset,					/* devreset */
	devinit,					/* devinit */
	rootattach,
	devdetach,
	devclone,					/* devclone */
	rootwalk,
	rootstat,
	rootopen,
	devcreate,					/* devcreate */
	rootclose,
	rootread,
	devbread,					/* devbread */
	rootwrite,
	devbwrite,					/* devbwrite */
	devremove,					/* devremove */
	devwstat,					/* devwstat */
};
