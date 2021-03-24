#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"version.h"
#include	"libcrypt.h"
#include	"keyboard.h"

enum
{
	Qdir,
	Qcons,
	Qsysctl,
	Qconsctl,
	Qemuargs,
	Qkeyboard,
	Qscancode,
	Qmemory,
	Qnull,
	Qpin,
	Qpointer,
	Qprofile,
	Qrandom,
	Qnotquiterandom,
	Qsysname,
	Quser,
	Qtime,
	Qdrivers
};

Dirtab contab[] =
{
	"cons",		{Qcons},	0,	0666,
	"sysctl",	{Qsysctl},	0,	0644,
	"consctl",	{Qconsctl},	0,	0222,
	"emuargs",	{Qemuargs},	0,	0444,
	"keyboard",	{Qkeyboard},	0,	0666,
	"scancode",	{Qscancode},	0,	0444,
	"memory",	{Qmemory},	0,	0444,
	"notquiterandom",	{Qnotquiterandom},	0,	0444,
	"null",		{Qnull},	0,	0666,
	"pin",		{Qpin},		0,	0666,
	"pointer",	{Qpointer},	0,	0444,
	"profile",	{Qprofile},	0,	0666,
	"random",	{Qrandom},	0,	0444,
	"sysname",	{Qsysname},	0,	0644,
	"user",		{Quser},	0,	0644,
	"time",		{Qtime},	0,	0644,
	"drivers",	{Qdrivers},	0,	0644,
};

enum
{
	MBS	=	1024
};

Queue*	ptrq;			/* Graphics mouse events */
Queue*	gkscanq;		/* Graphics keyboard raw scancodes */
char*	gkscanid;		/* name of raw scan format (if defined) */
Queue*	gkbdq;			/* Graphics keyboard unprocessed input */
Queue*	kbdq;			/* Console window unprocessed keyboard input */
Queue*	lineq;			/* processed console input */
char	ossysname[3*NAMELEN] = "inferno";

vlong	timeoffset;

static ulong	randomread(void *xp, ulong n);
static void	randominit(void);

extern int	dflag;

static int	sysconwrite(void*, ulong);
static int	argsread(ulong, void*, ulong);
extern int	rebootargc;
extern char**	rebootargv;
static char*	haltarg = "halt";
static char*	rebootarg = "reboot";

static struct
{
	QLock	q;
	QLock	gq;		/* separate lock for the graphical input */

	int	raw;		/* true if we shouldn't process input */
	Ref	ctl;		/* number of opens to the control file */
	Ref	ptr;		/* number of opens to the ptr file */
	int	scan;		/* true if reading raw scancodes */
	int	x;		/* index into line */
	char	line[1024];	/* current input line */

	Rune	c;
	int	count;
} kbd;

void
kbdslave(void *a)
{
	char b;

	USED(a);
	for(;;) {
		b = readkbd();
		if(kbd.raw == 0)
			write(1, &b, 1);
		qproduce(kbdq, &b, 1);
	}
	pexit("kbdslave", 0);
}

void
gkbdputc(Queue *q, int ch)
{
	int n;
	Rune r;
	static uchar kc[5*UTFmax];
	static int nk, collecting = 0;
	char buf[UTFmax];

	r = ch;
	if(r == Latin) {
		collecting = 1;
		nk = 0;
		return;
	}
	if(collecting) {
		int c;
		nk += runetochar((char*)&kc[nk], &r);
		c = latin1(kc, nk);
		if(c < -1)	/* need more keystrokes */
			return;
		collecting = 0;
		if(c == -1) {	/* invalid sequence */
			qproduce(q, kc, nk);
			return;
		}
		r = (Rune)c;
	}
	n = runetochar(buf, &r);
	if(n == 0)
		return;
	/* if(!isdbgkey(r)) */ 
		qproduce(q, buf, n);
}

/* default %N (convert address to name), just say %uX */

static int
Nconv(va_list *arg, Fconv *fp)
{
	ulong v;
	char s[9];
	int ii;

	v = va_arg(*arg, ulong);
	s[8] = 0;
	ii = 7;
	do {
		s[ii] = "0123456789ABCDEF"[v & 0xf];
		v >>= 4;
	} while (ii-- >= 0 && v);
	strconv(s+ii+1, fp);
	return 0;
}

void
coninit(void)
{
	kbdq = qopen(512, 0, 0, 0);
	if(kbdq == 0)
		panic("no memory");
	lineq = qopen(512, 0, 0, 0);
	if(lineq == 0)
		panic("no memory");
	gkbdq = qopen(512, 0, 0, 0);
	if(gkbdq == 0)
		panic("no memory");
	ptrq = qopen(512, 0, 0, 0);
	if(ptrq == 0)
		panic("no memory");
	randominit();
	fmtinstall('N', Nconv);
}

Chan*
conattach(void *spec)
{
	static int kp;

	if (kp == 0 && !dflag) {
		kproc("kbd", kbdslave, 0);
		kp = 1;
	}
	return devattach('c', spec);
}

Chan*
conclone(Chan *c, Chan *nc)
{
	return devclone(c, nc);
}

int
conwalk(Chan *c, char *name)
{
	return devwalk(c, name, contab, nelem(contab), devgen);
}

void
constat(Chan *c, char *db)
{
	devstat(c, db, contab, nelem(contab), devgen);
}

Chan*
conopen(Chan *c, int omode)
{
	if(c->qid.path & ~CHDIR == Qdir) {
		if(omode != OREAD)
			error(Eisdir);
		c->mode = omode;
		c->flag |= COPEN;
		c->offset = 0;
		return c;
	}

	c = devopen(c, omode, contab, nelem(contab), devgen);

	switch(c->qid.path & ~CHDIR) {
	case Qconsctl:
		incref(&kbd.ctl);
		break;
	case Qpointer:
		if(incref(&kbd.ptr) != 1){
			decref(&kbd.ptr);
			c->flag &= ~COPEN;
			error(Einuse);
		}
		break;
	case Qscancode:
		qlock(&kbd.gq);
		if(gkscanq || !gkscanid) {
			qunlock(&kbd.q);
			c->flag &= ~COPEN;
			if(gkscanq)
				error(Einuse);
			else
				error(Ebadarg);
		}
		gkscanq = qopen(256, 0, nil, nil);
		qunlock(&kbd.gq);
		break;
	}
	return c;
}

void
conclose(Chan *c)
{
	if((c->flag & COPEN) == 0)
		return;

	switch(c->qid.path) {
	case Qconsctl:
		if(decref(&kbd.ctl) == 0)
			kbd.raw = 0;
		break;
	case Qpointer:
		decref(&kbd.ptr);
		break;
	case Qscancode:
		qlock(&kbd.gq);
		if(gkscanq) {
			qfree(gkscanq);
			gkscanq = 0;
		}
		qunlock(&kbd.gq);
		break;
	}
}

static int
changed(void *a)
{
	Pointer *p = a;
	return p->modify == 1;
}

long
conread(Chan *c, void *va, long count, ulong offset)
{
	int i, n, ch, eol;
	Pointer m;
	char *p, buf[64];

	if(c->qid.path & CHDIR)
		return devdirread(c, va, count, contab, nelem(contab), devgen);

	switch(c->qid.path) {
	default:
		error(Egreg);
	case Qsysctl:
		return readstr(offset, va, count, VERSION);
	case Qsysname:
		return readstr(offset, va, count, ossysname);
	case Qrandom:
		return randomread(va, count);
	case Qnotquiterandom:
		pseudoRandomBytes(va, count);
		return count;
	case Qemuargs:
		return argsread(offset, va, count);
	case Qpin:
		p = "pin set";
		if(up->env->pgrp->pin == Nopin)
			p = "no pin";
		return readstr(offset, va, count, p);
	case Quser:
		return readstr(offset, va, count, up->env->user);
	case Qtime:
		snprint(buf, sizeof(buf), "%.lld", timeoffset + osusectime());
		return readstr(offset, va, count, buf);
	case Qdrivers:
		p = malloc(MBS);
		if(p == nil)
			error(Enomem);
		n = 0;
		for(i = 0; devtab[i] != nil; i++)
			n += snprint(p+n, MBS-n, "#%C %s\n", devtab[i]->dc,  devtab[i]->name);
		n = readstr(offset, va, count, p);
		free(p);
		return n;
	case Qmemory:
		return poolread(va, count, offset);

	case Qprofile:
		profread(buf, sizeof(buf));
		return readstr(offset, va, count, buf);

	case Qnull:
		return 0;
	case Qcons:
		qlock(&kbd.q);
		if(waserror()){
			qunlock(&kbd.q);
			nexterror();
		}

		if(dflag)
			error(Enonexist);

		while(!qcanread(lineq)) {
			qread(kbdq, &kbd.line[kbd.x], 1);
			ch = kbd.line[kbd.x];
			if(kbd.raw){
				qiwrite(lineq, &kbd.line[kbd.x], 1);
				continue;
			}
			eol = 0;
			switch(ch) {
			case '\b':
				if(kbd.x)
					kbd.x--;
				break;
			case 0x15:
				kbd.x = 0;
				break;
			case '\n':
			case 0x04:
				eol = 1;
			default:
				kbd.line[kbd.x++] = ch;
				break;
			}
			if(kbd.x == sizeof(kbd.line) || eol){
				if(ch == 0x04)
					kbd.x--;
				qwrite(lineq, kbd.line, kbd.x);
				kbd.x = 0;
			}
		}
		n = qread(lineq, va, count);
		qunlock(&kbd.q);
		poperror();
		return n;
	case Qscancode:
		if(offset == 0)
			return readstr(0, va, count, gkscanid);
		else
			return qread(gkscanq, va, count);
	case Qkeyboard:
		return qread(gkbdq, va, count);
	case Qpointer:
		n = qread(ptrq, &m, sizeof(m));

		if(n == sizeof(m)) {
			n = sprint(buf, "m%11d %11d %11d ", m.x, m.y, m.b);
			if(count < n)
				n = count;
			memmove(va, buf, n);
		}
		return n;
	}
}

long
conwrite(Chan *c, void *va, long count, ulong offset)
{
	char buf[128];
	int len;
	int x;

	USED(offset);

	if(c->qid.path & CHDIR)
		error(Eperm);

	switch(c->qid.path) {
	default:
		error(Egreg);
	case Qcons:
		if (!dflag) 
			return write(1, va, count);
		if (count >= sizeof(buf))
			count = sizeof(buf)-1;
		memcpy(buf, va, count);
		buf[count] = '\0';
		logmsg(1, buf);
		return count;
	case Qsysctl:
		return sysconwrite(va, count);
	case Qprofile:
		return profwrite(va, count);
	case Qconsctl:
		if(count >= sizeof(buf))
			count = sizeof(buf)-1;
		strncpy(buf, va, count);
		buf[count] = 0;
		if(strncmp(buf, "rawon", 5) == 0) {
			kbd.raw = 1;
			return count;
		}
		else
		if(strncmp(buf, "rawoff", 6) == 0) {
			kbd.raw = 0;
			return count;
		}
		error(Ebadctl);
	case Qkeyboard:
		for(x=0; x<count; ) {
			Rune r;
			x += chartorune(&r, &((char*)va)[x]);
			gkbdputc(gkbdq, r);
		}
		return count;
	case Qnull:
		return count;
	case Qpin:
		if(up->env->pgrp->pin != Nopin)
			error("pin already set");
		if(count >= sizeof(buf))
			count = sizeof(buf)-1;
		strncpy(buf, va, count);
		buf[count] = '\0';
		up->env->pgrp->pin = atoi(buf);
		return count;
	case Qtime:
		if(count >= sizeof(buf))
			count = sizeof(buf)-1;
		strncpy(buf, va, count);
		buf[count] = '\0';
		timeoffset = strtoll(buf, 0, 0)-osusectime();
		return count;
	case Quser:
		if(count >= sizeof(buf))
			count = sizeof(buf)-1;
		strncpy(buf, va, count);
		buf[count] = '\0';
		if(strcmp(up->env->user, eve) != 0)
			error(Eperm);
		setid(buf);
		return count;
	case Qsysname:
		if(count >= sizeof(buf))
			count = sizeof(buf)-1;
		strncpy(buf, va, count);
		buf[count] = '\0';
		strncpy(ossysname, buf, sizeof(ossysname));
		return count;
	}
	return 0;
}

static Rb *rp;

int
rbnotfull(void *v)
{
	int i;

	USED(v);
	i = rp->wp - rp->rp;
	if(i < 0)
		i += sizeof(rp->buf);
	return i < rp->target;
}

static int
rbnotempty(void *v)
{
	USED(v);
	return rp->wp != rp->rp;
}

/*
 *  spin counting up
 */
void
genrandom(void *v)
{
	USED(v);

	/* cuz we're not interested in our environment */
	closefgrp(up->env->fgrp);
	up->env->fgrp = nil;
	closepgrp(up->env->pgrp);
	up->env->fgrp = nil;
	osspin(&rp->producer);
}

/*
 *  produce random bits in a circular buffer
 */
static void
randomclock(void *v)
{
	uchar *p;

	USED(v);

	/* cuz we're not interested in our environment */
	closefgrp(up->env->fgrp);
	up->env->fgrp = nil;
	closepgrp(up->env->pgrp);
	up->env->fgrp = nil;

	for(;; osmillisleep(20)){
		while(!rbnotfull(0)){
			rp->filled = 1;
			Sleep(&rp->clock, rbnotfull, 0);
		}

		if(rp->randomcount == 0)
			continue;
		rp->bits = (rp->bits<<2) ^ (rp->randomcount&3);
		rp->randomcount = 0;
		rp->next += 2;
		if(rp->next != 8)
			continue;

		rp->next = 0;
		*rp->wp ^= rp->bits ^ *rp->rp;
		p = rp->wp+1;
		if(p == rp->ep)
			p = rp->buf;
		rp->wp = p;

		if(rp->wakeme)
			Wakeup(&rp->consumer);
	}
}

static void
randominit(void)
{
	rp=osraninit();
	rp->target = 16;
	rp->ep = rp->buf + sizeof(rp->buf);
	rp->rp = rp->wp = rp->buf;
}

/*
 *  consume random bytes from a circular buffer
 */
static ulong
randomread(void *xp, ulong n)
{
	int i, sofar;
	uchar *e, *p;

	p = xp;

	if(waserror()){
		qunlock(&rp->l);
		nexterror();
	}

	qlock(&rp->l);
	if(!rp->kprocstarted){
		rp->kprocstarted = 1;
		kproc("genrand", genrandom, 0);
		kproc("randomclock", randomclock, 0);
	}

	for(sofar = 0; sofar < n;){
		if(!rbnotempty(0)){
			rp->wakeme = 1;
			Wakeup(&rp->clock);
			oswakeupproducer(&rp->producer);
			Sleep(&rp->consumer, rbnotempty, 0);
			rp->wakeme = 0;
			continue;
		}
		*(p+(sofar++)) = *rp->rp;
		e = rp->rp + 1;
		if(e == rp->ep)
			e = rp->buf;
		rp->rp = e;
	}
	if(rp->filled && rp->wp == rp->rp){
		i = 2*rp->target;
		if(i > sizeof(rp->buf) - 1)
			i = sizeof(rp->buf) - 1;
		rp->target = i;
		rp->filled = 0;
	}
	qunlock(&rp->l);
	poperror();

	Wakeup(&rp->clock);
	oswakeupproducer(&rp->producer);

	return n;
}

Dev condevtab = {
	'c',
	"con",

	coninit,
	conattach,
	conclone,
	conwalk,
	constat,
	conopen,
	devcreate,
	conclose,
	conread,
	devbread,
	conwrite,
	devbwrite,
	devremove,
	devwstat
};

int	rebootok = 0;

static int	
sysconwrite(void *va, ulong count)
{
	int len;
	char *arg = (char*) va;

	len = strlen(rebootarg);
	if((count>=len) && (strncmp(arg, rebootarg, len) == 0)) {
		if (rebootok) {
			termrestore();
			execvp(rebootargv[0], rebootargv);
			panic("reboot failure");
		} else {
			error("reboot option not supported on this system");
		}
	}

	len = strlen(haltarg);
	if((count>=len) && (strncmp(arg, haltarg, len) == 0))  {
		cleanexit(0);
	}

	return 0;
} 

int
argsread(ulong offset, void *va, ulong count)
{
	int i, len;
	char *p, *q, **arg;

	p = va;
	arg = rebootargv;
	for (i = 0; i < rebootargc; i++) {
		q = *arg++;
		len = strlen(q);
		if (offset >= len+1)
			offset -= len+1;
		else {
			q += offset;
			len -= offset;
			offset = 0;
			if (count < len+1) {
				memmove(p, q, count);
				p += count;
				break;
			}
			memmove(p, q, len);
			p += len;
			*p++ = '\001';
			count -= len+1;
		}
	}
	return p-(char*)va;
}

int profile=0;
profwrite(void *va, ulong count)
{
	char buf[32];

	strncpy(buf, va, count);
	buf[count] = 0;

	if(strncmp(buf, "clear", 5) == 0){
		profclear();
		return count;
	}

	if(strncmp(buf, "start", 5) == 0){
		profile=1;
		profstart();
		return count;
        }

	if(strncmp(buf, "stop", 4) == 0){
                profile=0;
                return count;
        }
}

/* re-initialize your data here */
profclear(){}

/* resume your collection here */
profstart(){}

/* put here what you want to output */
profread(char *buf, int count){
	snprint(buf, count, "\n");
}
