#include	"dat.h"
#include	"fns.h"
#include	"error.h"

enum
{
	Qdir,
	Qdev,
	Qprog,
	Qnet,
	Qchan,
	Qnvfs
};

static
Dirtab slashdir[] =
{
	"dev",		{CHDIR|Qdev},		0,	0777,
	"prog",		{CHDIR|Qprog},		0,	0777,
	"net",		{CHDIR|Qnet},		0,	0777,
	"chan",		{CHDIR|Qchan},		0,	0777,
	"nvfs",		{CHDIR|Qnvfs},		0,	0777,
};

void
rootinit(void)
{
}

Chan *
rootattach(void *spec)
{
	return devattach('/', spec);
}

Chan*
rootclone(Chan *c, Chan *nc)
{
	return devclone(c, nc);
}

int
rootwalk(Chan *c, char *name)
{
	if((c->qid.path & ~CHDIR) != Qdir){
		if(strcmp(name, "..") == 0){
			c->qid.path = Qdir|CHDIR;
			return 1;
		} else
			return 0;
	}
	return devwalk(c, name, slashdir, nelem(slashdir), devgen);
}

void
rootstat(Chan *c, char *db)
{
	devstat(c, db, slashdir, nelem(slashdir), devgen);
}

Chan *
rootopen(Chan *c, int omode)
{
	return devopen(c, omode, slashdir, nelem(slashdir), devgen);
}

void
rootclose(Chan *c)
{
	USED(c);
}

long
rootread(Chan *c, void *a, long n, ulong offset)
{
	USED(offset);
	switch(c->qid.path & ~CHDIR) {
	default:
		return 0;
	case Qdir:
		return devdirread(c, a, n, slashdir, nelem(slashdir), devgen);
	}	
}

long
rootwrite(Chan *ch, void *a, long n, ulong offset)
{
	USED(ch);
	USED(a);
	USED(n);
	USED(offset);
	error(Eperm);
	return -1;
}

Dev rootdevtab = {
	'/',
	"root",

	rootinit,
	rootattach,
	rootclone,
	rootwalk,
	rootstat,
	rootopen,
	devcreate,
	rootclose,
	rootread,
	devbread,
	rootwrite,
	devbwrite,
	devremove,
	devwstat
};
