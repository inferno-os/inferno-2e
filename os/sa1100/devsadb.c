#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"ureg.h"
#include	"../port/error.h"

#define DPRINT if(0)print

enum{
	Qdir,
	Qdref,
	Qiref,
	Qdval,
};

Dirtab sadbdir[]={
	"dref",		{Qdref},	0,	0666,
	"iref",		{Qiref},	0,	0666,
	"dval",		{Qdval},	0,	0666,
};

#define	NDBG	(sizeof sadbdir/sizeof(Dirtab))

Ureg	Dur, *Dsp;
Ureg	Iur, *Isp;
Ureg	Vur, *Vsp;
void catchDref(char *, void *);
void catchDval(char *, ulong, ulong);
void catchIref(char *, void *);
static int caughtDBG(Ureg *ur, uint);

static void
sadbreset(void)
{
	wDBCR(0);
	wIBCR(0);
}

static void
sadbinit(void)
{
}

static Chan*
sadbattach(char *spec)
{
	return devattach('Z', spec);
}

static int
sadbwalk(Chan *c, char *name)
{
	return devwalk(c, name, sadbdir, nelem(sadbdir), devgen);
}

static void
sadbstat(Chan *c, char *dp)
{
	devstat(c, dp, sadbdir, nelem(sadbdir), devgen);
}

static Chan*
sadbopen(Chan *c, int omode)
{
	return devopen(c, omode, sadbdir, nelem(sadbdir), devgen);
}

static void
sadbcreate(Chan *c, char *name, int omode, ulong perm)
{
	USED(c, name, omode, perm);
	error(Eperm);
}

static void
sadbclose(Chan *c)
{
	USED(c);
}

static char nameDref[20];
static char nameDval[20];
static char nameIref[20];

static long
sadbread(Chan *c, void *va, long n, ulong offset)
{
	Ureg *sp;
	ulong *up;
	char *nm;
	int ii;
	char *cp, buf[2048];

	SET(sp,up,nm);
	switch(c->qid.path & ~CHDIR){

	case Qdir:
		return devdirread(c, va, n, sadbdir, nelem(sadbdir), devgen);

	case Qdref:
		up = (ulong *)&Dur;
		sp = Dsp;
		nm = nameDref;
		break;
	case Qdval:
		up = (ulong *)&Vur;
		sp = Vsp;
		nm = nameDval;
		break;
	case Qiref:
		up = (ulong *)&Iur;
		sp = Isp;
		nm = nameIref;
		break;
	default:
		error(Ebadusefd);
	}
	cp = buf;
	cp += sprint(cp, "%s", nm);
	if (sp)
	{
		cp += sprint(cp, "%.8ulx", sp);
		for (ii = 0; ii < sizeof(Ureg) / sizeof *up; ii++, up++)
			cp += sprint(cp, "\n%.8ulx", *up);
	}
	*cp = 0;
	return readstr(offset, va, n, buf);
}

static Block*
sadbbread(Chan *c, long n, ulong offset)
{
	return devbread(c, n, offset);
}

static long
sadbwrite(Chan *c, void *va, long n, ulong)
{
#define Ncmd 5
	char buf[255], *field[Ncmd];
	ulong arg1, arg2;
	char *lbl;
	int nf;

	switch(c->qid.path){
	case Qdref:
	case Qdval:
	case Qiref:
		trapspecial(caughtDBG);

		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, va, n);
		buf[n] = '\0';

		lbl = 0;
		arg1 = arg2 = 0;
		nf = parsefields(buf, field, Ncmd, " \t\n");
		if (nf > 0)
		{
			lbl = field[0];
			if (strcmp(lbl, "testsadb") == 0)
			{
				testsadb();
				return n;
			}
		}
		if (nf > 1)
			arg1 = strtol(field[1], 0, 16);
		if (nf > 2)
			arg2 = strtol(field[2], 0, 16);

		switch(c->qid.path) {
		case Qiref:
			catchIref(lbl, (void *)arg1);
			break;
		case Qdref:
			catchDref(lbl, (void *)arg1);
			break;
		case Qdval:
			catchDval(lbl, arg1, arg2);
			break;
		}
		return n;
	}
	error(Ebadusefd);
	return 0;	/* not reached */
}

static long
sadbbwrite(Chan *c, Block *bp, ulong offset)
{
	return devbwrite(c, bp, offset);
}

static void
sadbremove(Chan *c)
{
	USED(c);
	error(Eperm);
}

static void
sadbwstat(Chan *c, char *dp)
{
	USED(c, dp);
	error(Eperm);
}

Dev sadbdevtab = {					/* defaults in dev.c */
	'Z',
	"sadb",

	sadbreset,					/* devreset */
	sadbinit,					/* devinit */
	sadbattach,
	devdetach,
	devclone,					/* devclone */
	sadbwalk,
	sadbstat,
	sadbopen,
	sadbcreate,					/* devcreate */
	sadbclose,
	sadbread,
	sadbbread,					/* devbread */
	sadbwrite,
	sadbbwrite,					/* devbwrite */
	sadbremove,					/* devremove */
	sadbwstat,					/* devwstat */
};

ulong lastDBAR;
ulong lastDBVR;
ulong lastDBMR;
ulong lastDBCR;
ulong lastIBCR;

void
catchDref(char *s, void *v)
{
	lastDBCR &= ~2;
	nameDref[0] = 0;
	Dsp = 0;
	wDBCR(lastDBCR);
	if (s)
	{
		DPRINT("catchDref(%s, %ux)\n", s, v);
		strncpy(nameDref, s, sizeof nameDref-1);
		lastDBAR = (ulong)v;
		wDBAR(lastDBAR);
		lastDBCR |= 2;
		wDBCR(lastDBCR);
	}
}

void
catchDval(char *s, ulong v, ulong m)
{
	lastDBCR &= ~4;
	nameDval[0] = 0;
	Vsp = 0;
	wDBCR(lastDBCR);
	if (s)
	{
		DPRINT("catchDval(%s, %ux/%ux)\n", s, v, m);
		strncpy(nameDval, s, sizeof nameDval-1);
		lastDBVR = v;
		lastDBMR = m;
		wDBVR(lastDBVR);
		wDBMR(lastDBMR);
		lastDBCR |= 4;
		if (!(lastDBCR & 2))
		{
			lastDBAR=-1;
			wDBAR(lastDBAR);
			lastDBCR |= 2;
		}
		wDBCR(lastDBCR);
	}
}

void
catchIref(char *s, void *a)
{
	ulong	v = (ulong)a;

	v &= ~3;
	nameIref[0] = 0;
	Isp = 0;
	if (s)
	{
		DPRINT("catchIref(%s, %ux)\n", s, v);
		strncpy(nameIref, s, sizeof nameIref-1);
		lastIBCR = v | 1;
	}
	else
		lastIBCR = 0;
	wIBCR(lastIBCR);
}

static int
caughtDBG(Ureg *ur, uint fsr)
{
	int ret = 0;

	DPRINT("IBCR %ux PC %ux DBCR %ux DBAR %ux\n",
		lastIBCR, ur->pc, lastDBCR, lastDBAR);
	if (fsr & (1 << 9))
	{
		ret = 1;
		Dur = *ur;
		Dsp = ur;
		wDBCR(lastDBCR = 0);
		print("Dref caught: %s\n", nameDref);
		ur->pc -= 4;	/* Must re-execute previous instruction */
		dumpregs(ur);
	}
	if ((ur->pc | 1) == lastIBCR)
	{
		ret = 1;
		Iur = *ur;
		Isp = ur;
		wIBCR(lastIBCR = 0);
		print("Iref caught: %s\n", nameIref);
		dumpregs(ur);
	}
	return(ret);
}
