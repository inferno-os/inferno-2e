#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

#include	"io.h"

/*
 * MPC8xx real time clock
 * FADS board option switch
 * interrupt statistics
 * lcd control
 */

enum{
	Qrtc = 1,
	Qswitch,
	Qintstat,
	Qlcdctl,

	Qporta,
	Qportb,
	Qportc,

	/* sccr */
	RTDIV=	1<<24,
	RTSEL=	1<<23,

	/* rtcsc */
	RTE=	1<<0,
	R38K=	1<<4,
};

static	QLock	rtclock;		/* mutex on clock operations */

static Dirtab rtcdir[]={
	"rtc",		{Qrtc, 0},	12,	0666,
	"switch",	{Qswitch, 0}, 0, 0444,
	"intstat",	{Qintstat, 0}, 0, 0444,
	"lcdctl",	{Qlcdctl, 0}, 0, 0666,
	"porta",	{Qporta, 0}, 0, 0666,
	"portb",	{Qportb, 0}, 0, 0666,
	"portc",	{Qportc, 0}, 0, 0666,
};
#define	NRTC	(sizeof(rtcdir)/sizeof(rtcdir[0]))

static	long	readport(int, ulong, char*, long);

static void
rtcreset(void)
{
	IMM *io;
	int n;

	io = m->iomem;
	io->rtcsck = KEEP_ALIVE_KEY;
	n = (RTClevel<<8)|RTE;
	if(m->clockgen == 5*MHz)
		n |= R38K;
	io->rtcsc = n;
	io->rtcsck = ~KEEP_ALIVE_KEY;
}

static Chan*
rtcattach(char *spec)
{
	return devattach('r', spec);
}

static int	 
rtcwalk(Chan *c, char *name)
{
	return devwalk(c, name, rtcdir, NRTC, devgen);
}

static void	 
rtcstat(Chan *c, char *dp)
{
	devstat(c, dp, rtcdir, NRTC, devgen);
}

static Chan*
rtcopen(Chan *c, int omode)
{
	omode = openmode(omode);
	switch(c->qid.path){
	case Qrtc:
		if(strcmp(up->env->user, eve)!=0 && omode!=OREAD)
			error(Eperm);
		break;
	case Qswitch:
	case Qintstat:
		if(omode!=OREAD)
			error(Eperm);
		break;
	case Qlcdctl:
	case Qporta:
	case Qportb:
	case Qportc:
		break;
	}
	return devopen(c, omode, rtcdir, NRTC, devgen);
}

static void	 
rtcclose(Chan*)
{
}

static long	 
rtcread(Chan *c, void *buf, long n, ulong offset)
{
	ulong t;
	char *b;

	if(c->qid.path & CHDIR)
		return devdirread(c, buf, n, rtcdir, NRTC, devgen);

	switch(c->qid.path){
	case Qrtc:
		t = m->iomem->rtc;
		n = readnum(offset, buf, n, t, 12);
		return n;
	case Qswitch:
		return readnum(offset, buf, n, archoptionsw(), 12);
	case Qintstat:
		b = malloc(2048);
		if(waserror()){
			free(b);
			nexterror();
		}
		intrstats(b, 2048);
		t = readstr(offset, buf, n, b);
		poperror();
		free(b);
		return t;
	case Qlcdctl:
		return 0;
	case Qporta:
	case Qportb:
	case Qportc:
		return readport(c->qid.path, offset, buf, n);
	}
	error(Egreg);
	return 0;		/* not reached */
}

static long	 
rtcwrite(Chan *c, void *buf, long n, ulong offset)
{
	ulong secs;
	char *cp, sbuf[32];
	IMM *io;

	switch(c->qid.path){
	case Qrtc:
		/*
		 *  read the time
		 */
		if(offset != 0 || n >= sizeof(sbuf)-1)
			error(Ebadarg);
		memmove(sbuf, buf, n);
		sbuf[n] = '\0';
		cp = sbuf;
		while(*cp){
			if(*cp>='0' && *cp<='9')
				break;
			cp++;
		}
		secs = strtoul(cp, 0, 0);
		/*
		 * set it
		 */
		io = ioplock();
		io->rtck = KEEP_ALIVE_KEY;
		io->rtc = secs;
		io->rtck = ~KEEP_ALIVE_KEY;
		iopunlock();
		return n;
	case Qswitch:
		return 0;
	case Qlcdctl:
		return lcdctl(buf, n);
	}
	error(Egreg);
	return 0;		/* not reached */
}

static long
readport(int p, ulong offset, char *buf, long n)
{
	long t;
	char *b;
	int v[4], i;
	IMM *io;

	io = m->iomem;
	for(i=0;i<nelem(v); i++)
		v[i] = 0;
	switch(p){
	case Qporta:
		v[0] = io->padat;
		v[1] = io->padir;
		v[2] = io->papar;
		break;
	case Qportb:
		v[0] = io->pbdat;
		v[1] = io->pbdir;
		v[2] = io->pbpar;
		break;
	case Qportc:
		v[0] = io->pcdat;
		v[1] = io->pcdir;
		v[2] = io->pcpar;
		v[3] = io->pcso;
		break;
	}
	b = malloc(READSTR);
	if(waserror()){
		free(b);
		nexterror();
	}
	t = 0;
	for(i=0; i<nelem(v); i++)
		t += snprint(b+t, READSTR-t, " %8.8ux", v[i]);
	t = readstr(offset, buf, n, b);
	poperror();
	free(b);
	return t;
}

long
rtctime(void)
{
	return m->iomem->rtc;
}

Dev rtcdevtab = {
	'r',
	"rtc",

	rtcreset,
	devinit,
	rtcattach,
	devdetach,
	devclone,
	rtcwalk,
	rtcstat,
	rtcopen,
	devcreate,
	rtcclose,
	rtcread,
	devbread,
	rtcwrite,
	devbwrite,
	devremove,
	devwstat,
};
