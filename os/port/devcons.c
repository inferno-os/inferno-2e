#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	<version.h>
#include	"keyboard.h"

void	(*serwrite)(const char *, int);

Queue*	kscanq;			/* keyboard raw scancodes (when needed) */
char*	kscanid;		/* name of raw scan format (if defined) */
Queue*	kbdq;			/* unprocessed console input */
Queue*	lineq;			/* processed console input */
Queue*	printq;			/* console output */
Pointer	mouse;

static struct
{
	Ref	readers;
	Queue	*printq;
} rcons;

static struct
{
	QLock;

	int	raw;		/* true if we shouldn't process input */
	int	ctl;		/* number of opens to the control file */
	int	kbdr;		/* number of open reads to the keyboard */
	int	scan;		/* true if reading raw scancodes */
	int	x;		/* index into line */
	char	line[1024];	/* current input line */

	char	c;
	int	count;
	int	repeat;
} kbd;


	char	sysname[NAMELEN] = "inferno";
	char	eve[NAMELEN] = "inferno";

static	ulong	randomread(uchar*, ulong);
static	void	randominit(void);
static	void	seednrand(void);
static	ulong	sysctlwrite(void *buf);

void	pseudoRandomBytes(uchar*, int);	/* stop yer whining */

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
printinit(void)
{
	lineq = qopen(2*1024, 0, 0, 0);
	qnoblock(lineq, 1);
	fmtinstall('N', Nconv);
}

/*
 *  return true if current user is eve
 */
int
iseve(void)
{
	Osenv *o;

	o = up->env;
	return strcmp(eve, o->user) == 0;
}

static int
consactive(void)
{
	if(printq)
		return qlen(printq) > 0;
	return 0;
}

static void
prflush(void)
{
	while(!serwrite && consactive())
		;
}

extern int consoleprint;
static int consolepush = 1;
/*
 *   Print a string on the console.  Convert \n to \r\n for serial
 *   line consoles.  Locking of the queues is left up to the screen
 *   or uart code.  Multi-line messages to serial consoles may get
 *   interspersed with other messages.
 */
static void
putstrn0(char *str, int n, int usewrite)
{
	int m;
	char *t;
	char buf[PRINTSIZE+2];

	/*
	 *  rcons file overrides default console, but not
	 *  the panic-time polled serial write
	 */
	if(rcons.printq != nil && serwrite == nil) {
		if(usewrite)
			qwrite(rcons.printq, str, n);
		else
			qiwrite(rcons.printq, str, n);
		return;
	}

	/*
	 *  if there's an attached bit mapped display,
	 *  put the message there.  screenputs is defined
	 *  as a null macro for systems that have no such
	 *  display.
	 */
	if (consoleprint)
		screenputs(str, n);

	/*
	 *  if there's a serial line being used as a console,
	 *  put the message there.
	 */
	if (serwrite) {
		serwrite(str, n);
		return;
	}

	if(printq == 0)
		return;

	while(n > 0) {
		t = memchr(str, '\n', n);
		if(t) {
			m = t - str;
			memmove(buf, str, m);
			buf[m] = '\r';
			buf[m+1] = '\n';
			if(usewrite)
				qwrite(printq, buf, m+2);
			else
				qiwrite(printq, buf, m+2);
			str = t + 1;
			n -= m + 1;
		} else {
			if(usewrite)
				qwrite(printq, str, n);
			else 
				qiwrite(printq, str, n);
			break;
		}
	}
}

void
putstrn(char *str, int n)
{
	putstrn0(str, n, 0);
}

int
snprint(char *s, int n, char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	n = doprint(s, s+n, fmt, arg) - s;
	va_end(arg);

	return n;
}

int
sprint(char *s, char *fmt, ...)
{
	int n;
	va_list arg;

	va_start(arg, fmt);
	n = doprint(s, s+PRINTSIZE, fmt, arg) - s;
	va_end(arg);

	return n;
}

int
print(char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[PRINTSIZE];

	va_start(arg, fmt);
	n = doprint(buf, buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);
	putstrn(buf, n);

	return n;
}

int
fprint(int fd, char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[PRINTSIZE];

	USED(fd);
	va_start(arg, fmt);
	n = doprint(buf, buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);
	putstrn(buf, n);

	return n;
}

/* these come from the kernel config file */
int panicreset;
char *resetmsg;
char *noresetmsg;

void
panic(char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[PRINTSIZE];
	char *msg;

	setpanic();
	if (panicreset)
		msg = resetmsg;
	else
		msg = noresetmsg;
	if (msg) {
		putstrn(msg, strlen(msg));
		strcpy(buf, "diagnosis: ");
	}
	else
		strcpy(buf, "panic: ");

	va_start(arg, fmt);
	n = doprint(buf+strlen(buf), buf+sizeof(buf)-1, fmt, arg) - buf;
	va_end(arg);
	buf[n] = '\n';
	putstrn(buf, n+1);
	spllo();
	dumpstack();
	if (panicreset) {
		splhi();
		while(1);
	}
	exit(1);
}

int
pprint(char *fmt, ...)
{
	int n;
	Chan *c;
	Osenv *o;
	va_list arg;
	char buf[2*PRINTSIZE];

	n = sprint(buf, "%s %ld: ", up->text, up->pid);
	va_start(arg, fmt);
	n = doprint(buf+n, buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);

	o = up->env;
	if(o->fgrp == 0) {
		print("%s", buf);
		return 0;
	}
	c = o->fgrp->fd[2];
	if(c==0 || (c->mode!=OWRITE && c->mode!=ORDWR)) {
		print("%s", buf);
		return 0;
	}

	if(waserror()) {
		print("%s", buf);
		return 0;
	}
	devtab[c->type]->write(c, buf, n, c->offset);
	poperror();

	lock(c);
	c->offset += n;
	unlock(c);

	return n;
}

void
echo(Rune r, char *buf, int n)
{
	if(kbd.raw)
		return;

	if(r == '\n'){
		if(printq)
			qiwrite(printq, "\r", 1);
	} else if(r == 0x15){
		buf = "^U\n";
		n = 3;
	}
	if (consoleprint)
		screenputs(buf, n);
	if(printq)
		qiwrite(printq, buf, n);
}

/*
 *	New debug key support.  Allows other parts of the kernel to register debug
 *	key handlers, instead of devcons.c having to know whatever's out there.
 *	A kproc is used to invoke most handlers, rather than tying up the CPU at
 *	high IPL, which can choke some device drivers (eg softmodem).
 */
typedef struct {
	Rune	r;
	char	*m;
	void	(*f)(Rune);
	int	i;	/* function called at interrupt time */
} Dbgkey;

static struct {
	Rendez;
	Dbgkey	*work;
	Dbgkey	keys[50];
	int	nkeys;
	int	on;
} dbg;

static Dbgkey *
finddbgkey(Rune r)
{
	int i;
	Dbgkey *dp;

	for (dp = dbg.keys, i = 0; i < dbg.nkeys; i++, dp++)
		if (dp->r == r)
			return dp;
	return nil;
}

static int
dbgwork(void *)
{
	return dbg.work != 0;
}

static void
dbgproc(void *)
{
	Dbgkey *dp;

	setpri(PriRealtime);
	for (;;) {
		do {
			sleep(&dbg, dbgwork, 0);
			dp = dbg.work;
		} while (dp == nil);
		dp->f(dp->r);
		dbg.work = nil;
	}
}

void
debugkey(Rune r, char *msg, void (*fcn)(), int iflag)
{
	Dbgkey *dp;

	if (dbg.nkeys >= nelem(dbg.keys))
		return;
	if (finddbgkey(r) != nil)
		return;
	for (dp = &dbg.keys[dbg.nkeys++] - 1; dp >= dbg.keys; dp--) {
		if (strcmp(dp->m, msg) < 0)
			break;
		dp[1] = dp[0];
	}
	dp++;
	dp->r = r;
	dp->m = msg;
	dp->f = fcn;
	dp->i = iflag;
}

static int
isdbgkey(Rune r)
{
	static int ctrlt;
	Dbgkey *dp;
	int echoctrlt = ctrlt;

	/*
	 * ^t hack BUG
	 */
	if (dbg.on || (ctrlt >= 2)) {
		if (r == 0x14) {
			ctrlt++;
			return 0;
		}
		if (dp = finddbgkey(r)) {
			if (dp->i || ctrlt > 2)
				dp->f(r);
			else {
				dbg.work = dp;
				wakeup(&dbg);
			}
			ctrlt = 0;
			return 1;
		}
		ctrlt = 0;
	}
	else if(r == 0x14){
		ctrlt++;
		return 1;
	}
	else
		ctrlt = 0;
	if(echoctrlt){
		char buf[3];

		buf[0] = 0x14;
		while(--echoctrlt >= 0){
			echo(buf[0], buf, 1);
			qproduce(kbdq, buf, 1);
		}
	}
	return 0;
}

static void
dbgtoggle(Rune)
{
	dbg.on = !dbg.on;
	print("Debug keys %s\n", dbg.on ? "HOT" : "COLD");
}

static void
dbghelp(void)
{
	int i;
	Dbgkey *dp;
	Dbgkey *dp2;
	static char fmt[] = "%c: %-22s";

	dp = dbg.keys;
	dp2 = dp + (dbg.nkeys + 1)/2;
	for (i = dbg.nkeys; i > 1; i -= 2, dp++, dp2++) {
		print(fmt, dp->r, dp->m);
		print(fmt, dp2->r, dp2->m);
		print("\n");
	}
	if (i)
		print(fmt, dp->r, dp->m);
	print("\n");
}

static void
debuginit(void)
{
	kproc("consdbg", dbgproc, 0);
	debugkey('|', "HOT|COLD keys", dbgtoggle, 0);
	debugkey('?', "help", dbghelp, 0);
}

/*
 *  Called by a uart interrupt for console input.
 *
 *  turn '\r' into '\n' before putting it into the queue.
 */
int
kbdcr2nl(Queue *q, int ch)
{
	if(ch == '\r')
		ch = '\n';
	return kbdputc(q, ch);
}

/*
 *  Put character, possibly a rune, into read queue at interrupt time.
 *  Performs translation for compose sequences
 *  Called at interrupt time to process a character.
 */
int
kbdputc(Queue *q, int ch)
{
	int n;
	char buf[3];
	Rune r;
	static uchar kc[15];
	static int nk, collecting = 0;

	r = ch;
	if(r == Latin) {
		collecting = 1;
		nk = 0;
		return 0;
	}
	if(collecting) {
		int c;
		nk += runetochar((char*)&kc[nk], &r);
		c = latin1(kc, nk);
		if(c < -1)	/* need more keystrokes */
			return 0;
		collecting = 0;
		if(c == -1) {	/* invalid sequence */
			echo(kc[0], (char*)kc, nk);
			qproduce(q, kc, nk);
			return 0;
		}
		r = (Rune)c;
	}
	n = runetochar(buf, &r);
	if(n == 0)
		return 0;
	if (!isdbgkey(r)) {
		echo(r, buf, n);
		qproduce(q, buf, n);
	}
	return 0;
}

void
kbdrepeat(int rep)
{
	kbd.repeat = rep;
	kbd.count = 0;
}

void
kbdclock(void)
{
	if(kbd.repeat == 0)
		return;
	if(kbd.repeat==1 && ++kbd.count>HZ){
		kbd.repeat = 2;
		kbd.count = 0;
		return;
	}
	if(++kbd.count&1)
		kbdputc(kbdq, kbd.c);
}

enum{
	Qdir,
	Qcons,
	Qsysctl,
	Qconsctl,
	Qdrivers,
	Qkeyboard,
	Qscancode,
	Qmemory,
	Qmsec,
	Qnull,
	Qpgrpid,
	Qpin,
	Qpointer,
	Qrandom,
	Qrcons,
	Qnotquiterandom,
	Qsysenv,
	Qsysname,
	Qtime,
	Quser,
};

static Dirtab consdir[]=
{
	"cons",		{Qcons},	0,		0660,
	"sysctl",	{Qsysctl},	0,		0644,
	"consctl",	{Qconsctl},	0,		0220,
	"drivers",	{Qdrivers},	0,		0444,
	"keyboard",	{Qkeyboard},	0,		0666,
	"scancode",	{Qscancode},	0,		0444,
	"memory",	{Qmemory},	0,		0444,
	"msec",		{Qmsec},	NUMSIZE,	0444,
	"null",		{Qnull},	0,		0666,
	"pin",		{Qpin},		0,		0666,
	"pointer",	{Qpointer},	0,		0666,
	"pgrpid",	{Qpgrpid},	NUMSIZE,	0444,
	"random",	{Qrandom},	0,		0444,
	"rcons",	{Qrcons},	0,		0666,
	"notquiterandom", {Qnotquiterandom}, 0,	0444,
	"sysenv",	{Qsysenv},	0,		0444,
	"sysname",	{Qsysname},	0,		0664,
	"time",		{Qtime},	0,		0664,
	"user",		{Quser},	NAMELEN,	0666,
};

int     inferno_time_has_changed = 0;
                // To let external processes know time has been updated

ulong	boottime;		/* seconds since epoch at boot */

long
seconds(void)
{
	return boottime + TK2SEC(MACHP(0)->ticks);
}

vlong
mseconds(void)
{
	return ((vlong)boottime*1000)+((vlong)(TK2MS(MACHP(0)->ticks)));
}

vlong
osusectime(void)
{
	return (((vlong)boottime*1000)+((vlong)(TK2MS(MACHP(0)->ticks)))*1000);
}

int
readnum(ulong off, char *buf, ulong n, ulong val, int size)
{
	char tmp[64];

	if(size > 64) size = 64;

	snprint(tmp, sizeof(tmp), "%*.0lud ", size, val);
	if(off >= size)
		return 0;
	if(off+n > size)
		n = size-off;
	memmove(buf, tmp+off, n);
	return n;
}

int
readstr(ulong off, char *buf, ulong n, char *str)
{
	int size;

	size = strlen(str);
	if(off >= size)
		return 0;
	if(off+n > size)
		n = size-off;
	memmove(buf, str+off, n);
	return n;
}

void
fddump()
{
	Proc *p;
	Osenv *o;
	int i;
	char ptb[128];

	p = proctab(6);
	o = p->env;
	for(i = 0; i < o->fgrp->maxfd; i++) {
		if(o->fgrp->fd[i] == 0)
			continue;
		ptpath(o->fgrp->fd[i]->path, ptb, sizeof(ptb));
		print("%d: %s\n", i, ptb);
	}
}

static void
qpanic(Rune)
{
	panic("User requested panic.");
}

static void
rexit(Rune)
{
	exit(0);
}

static void
consinit(void)
{
	randominit();
	debuginit();
	debugkey('f', "files/6", fddump, 0);
	debugkey('q', "panic", qpanic, 1);
	debugkey('r', "exit", rexit, 1);
}

static Chan*
consattach(char *spec)
{
	return devattach('c', spec);
}

static int
conswalk(Chan *c, char *name)
{
	return devwalk(c, name, consdir, nelem(consdir), devgen);
}

static void
consstat(Chan *c, char *dp)
{
	devstat(c, dp, consdir, nelem(consdir), devgen);
}

static void
flushkbdline(Queue *q)
{
	if(kbd.x){
		qwrite(q, kbd.line, kbd.x);
		kbd.x = 0;
	}
}

static Chan*
consopen(Chan *c, int omode)
{
	c->aux = 0;
	switch(c->qid.path){
	case Qconsctl:
		if(!iseve())
			error(Eperm);
		qlock(&kbd);
		kbd.ctl++;
		qunlock(&kbd);
		break;
	case Qkeyboard:
		if((omode & 3) != OWRITE) {
			qlock(&kbd);
			kbd.kbdr++;
			flushkbdline(kbdq);
			kbd.raw = 1;
			qunlock(&kbd);
		}
		break;
	case Qscancode:
		qlock(&kbd);
		if(kscanq || !kscanid) {
			qunlock(&kbd);
			c->flag &= ~COPEN;
			if(kscanq)
				error(Einuse);
			else
				error(Ebadarg);
		}
		kscanq = qopen(256, 0, nil, nil);
		qunlock(&kbd);
		break;
	case Qpointer:
		if(incref(&mouse.ref) == 1)
			cursorenable();
		break;
	case Qrcons:
		if((omode & 3) != OWRITE) {
			if(incref(&rcons.readers) != 1){
				decref(&rcons.readers);
				error(Einuse);
			}
			rcons.printq = qopen(4096, 0, 0, 0);
			if(rcons.printq == 0) {
				decref(&rcons.readers);
				error("no memory");
			}
		}
	}
	return devopen(c, omode, consdir, nelem(consdir), devgen);
}

static void
consclose(Chan *c)
{
	if((c->flag&COPEN) == 0)
		return;

	switch(c->qid.path){
	case Qconsctl:
		/* last close of control file turns off raw */
		qlock(&kbd);
		if(--kbd.ctl == 0)
			kbd.raw = 0;
		qunlock(&kbd);
		break;
	case Qkeyboard:
		if(c->mode != OWRITE) {
			qlock(&kbd);
			--kbd.kbdr;
			qunlock(&kbd);
		}
		break;
	case Qscancode:
		qlock(&kbd);
		if(kscanq) {
			qfree(kscanq);
			kscanq = 0;
		}
		qunlock(&kbd);
		break;
	case Qpointer:
		if(decref(&mouse.ref) == 0)
			cursordisable();
		break;
	case Qrcons:
		if(c->mode != OWRITE && decref(&rcons.readers) == 0) {
			qfree(rcons.printq);
			rcons.printq = nil;
		}
		break;
	}
}

static int
changed(void *a)
{
	Pointer *p = a;
	return p->modify == 1;
}

static long
readsysenv(ulong offset, void *buf, long n)
{
	char eb[1024];
	int i = 0;
	const char *cp;
	char *bp;

	bp = eb;
	while((cp = bpenumenv(i++))) 
		bp += sprint(bp, "%s\n", cp);
	*bp = '\0';
	USED(i);	/* in case bpenumenv() is a null macro */
	return readstr(offset, buf, n, eb);
}

static long
consread(Chan *c, void *buf, long n, ulong offset)
{
	int l;
	Osenv *o;
	Pointer mt;
	int ch, eol, i;
	char *p, tmp[128];

	if(n <= 0)
		return n;
	o = up->env;
	switch(c->qid.path & ~CHDIR){
	case Qdir:
		return devdirread(c, buf, n, consdir, nelem(consdir), devgen);
	case Qsysctl:
		return readstr(offset, buf, n, VERSION);
	case Qcons:
	case Qkeyboard:
		qlock(&kbd);
		if(waserror()) {
			qunlock(&kbd);
			nexterror();
		}
		while(!qcanread(lineq)) {
			qread(kbdq, &kbd.line[kbd.x], 1);
			ch = kbd.line[kbd.x];
			if(kbd.raw || kbd.kbdr) {
				qiwrite(lineq, &kbd.line[kbd.x], 1);
				continue;
			}
			eol = 0;
			switch(ch){
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
			if(kbd.x == sizeof(kbd.line) || eol) {
				if(ch == 0x04)
					kbd.x--;

	// NOTE: sending EOF is relying on some potentially
	// incorrect behavior of qwrite. Basically, a write of 0
	// bytes will still cause Qstarve to be cleared, causing
	// the next qread to wake up with a read of 0 bytes.
	// However, if there's already something in the queue,
	// the EOF concept will get lost, so no attempt is made
	// to store that EOF concept.

				qwrite(lineq, kbd.line, kbd.x);
				kbd.x = 0;
			}
		}
		n = qread(lineq, buf, n);
		qunlock(&kbd);
		poperror();
		return n;
	case Qscancode:
		if(offset == 0)
			return readstr(0, buf, n, kscanid);
		else
			return qread(kscanq, buf, n);
	case Qpin:
		p = "pin set";
		if(up->env->pgrp->pin == Nopin)
			p = "no pin";
		return readstr(offset, buf, n, p);
	case Qpointer:
		qlock(&mouse.q);
		if(waserror()) {
			qunlock(&mouse.q);
			nexterror();
		}
		sleep(&mouse.r, changed, &mouse);
		mt = mouse;
		mouse.modify = 0;
		l = sprint(tmp, "m%11d %11d %11d ", mt.x, mt.y, mt.b);
		if(n < l)
			l = n;
		memmove(buf, tmp, l);
		qunlock(&mouse.q);
		poperror();
		return l;
	case Qpgrpid:
		return readnum(offset, buf, n, o->pgrp->pgrpid, NUMSIZE);
	case Qtime:
		snprint(tmp, sizeof(tmp), "%.lld", (vlong)mseconds()*1000);
		return readstr(offset, buf, n, tmp);
	case Quser:
		return readstr(offset, buf, n, o->user);
	case Qnull:
		return 0;
	case Qmsec:
		return readnum(offset, buf, n, TK2MS(MACHP(0)->ticks), NUMSIZE);
	case Qsysname:
		return readstr(offset, buf, n, sysname);
	case Qsysenv:
		return readsysenv(offset, buf, n);
	case Qnotquiterandom:
		pseudoRandomBytes(buf, n);
		return n;
	case Qrandom:
		return randomread(buf, n);
	case Qmemory:
		return poolread(buf, n, offset);
	case Qdrivers:
		p = malloc(READSTR);
		if(p == nil)
			error(Enomem);
		n = 0;
		for(i = 0; devtab[i] != nil; i++)
			n += snprint(p+n, READSTR-n, "#%C %s\n", devtab[i]->dc,  devtab[i]->name);
		n = readstr(offset, buf, n, p);
		free(p);
		return n;
	case Qrcons:
		if(rcons.printq == nil)
			error(Egreg);
		return qread(rcons.printq, buf, n);
	default:
		print("consread %lud\n", c->qid.path);
		error(Egreg);
	}
	return -1;		/* never reached */
}

static long
conswrite(Chan *c, void *va, long n, ulong offset)
{
	vlong t;
	Osenv *o;
	long l, bp;
	char *a = va;
	char buf[256];
	int b, x, y;

	switch(c->qid.path){
	case Qcons:
		/*
		 * Can't page fault in putstrn, so copy the data locally.
		 */
		l = n;
		while(l > 0){
			bp = l;
			if(bp > sizeof buf)
				bp = sizeof buf;
			memmove(buf, a, bp);
			putstrn0(a, bp, 1);
			a += bp;
			l -= bp;
		}
		break;
	case Qsysctl:
		if(n >= sizeof(buf))
			n = sizeof(buf)-1;
		strncpy(buf, a, n);
		buf[n] = 0;
		if (!sysctlwrite(buf))
			return 0;
		break;
	case Qpin:
		if(up->env->pgrp->pin != Nopin)
			error("pin already set");
		if(n >= sizeof(buf))
			n = sizeof(buf)-1;
		strncpy(buf, va, n);
		buf[n] = '\0';
		up->env->pgrp->pin = atoi(buf);
		return n;
	case Qconsctl:
		if(n >= sizeof(buf))
			n = sizeof(buf)-1;
		strncpy(buf, a, n);
		buf[n] = 0;
		for(a = buf; a;){
			if(strncmp(a, "rawon", 5) == 0){
				qlock(&kbd);
				flushkbdline(kbdq);
				kbd.raw = 1;
				qunlock(&kbd);
			} else if(strncmp(a, "rawoff", 6) == 0){
				qlock(&kbd);
				kbd.raw = 0;
				qunlock(&kbd);
			}
			if(a = strchr(a, ' '))
				a++;
		}
		break;
	case Qkeyboard:
		for(x=0; x<n; ) {
			Rune r;
			x += chartorune(&r, &a[x]);
			kbdputc(kbdq, r);
		}
		break;
	
	case Qtime:
		if(n >= sizeof(buf))
			n = sizeof(buf)-1;
		strncpy(buf, a, n);
		buf[n] = 0;
		t = strtoll(buf, 0, 0)/1000000;
		boottime = t - TK2SEC(MACHP(0)->ticks);
		inferno_time_has_changed = 1;
		break;

	case Quser:
		if(!iseve())
			error(Eperm);
		if(offset != 0)
			error(Ebadarg);
		if(n <= 0 || n >= NAMELEN)
			error(Ebadarg);
		o = up->env;
		strncpy(o->user, a, n);
		o->user[n] = 0;
		if(o->user[n-1] == '\n')
			o->user[n-1] = 0;
		break;

	case Qnull:
		break;

	case Qsysname:
		if(offset != 0)
			error(Ebadarg);
		if(n <= 0 || n >= NAMELEN)
			error(Ebadarg);
		strncpy(sysname, a, n);
		sysname[n] = 0;
		if(sysname[n-1] == '\n')
			sysname[n-1] = 0;
		break;

	case Qpointer:
		if(n > sizeof buf-1)
			n = sizeof buf -1;
		memmove(buf, va, n);
		buf[n] = 0;
		a = 0;
		x = strtoul(buf+1, &a, 0);
		if(a == 0)
			error(Eshort);
		y = strtoul(a, &a, 0);
		if(a == 0)
			error(Eshort);
		b = strtoul(a, 0, 0);
		mousetrack(b, x-mouse.x, y-mouse.y);
		return n;

	case Qrcons:
		return qwrite(kbdq, va, n);

	default:
		print("conswrite: %lud\n", c->qid.path);
		error(Egreg);
	}
	return n;
}

Dev consdevtab = {
	'c',
	"cons",

	devreset,
	consinit,
	consattach,
	devdetach,
	devclone,
	conswalk,
	consstat,
	consopen,
	devcreate,
	consclose,
	consread,
	devbread,
	conswrite,
	devbwrite,
	devremove,
	devwstat,
};

static struct
{
	QLock;
	Rendez	producer;
	Rendez	consumer;
	ulong	randomcount;
	uchar	buf[1024];
	uchar	*ep;
	uchar	*rp;
	uchar	*wp;
	uchar	next;
	uchar	bits;
	uchar	wakeme;
	uchar	filled;
	int	target;
	int	kprocstarted;
	ulong	randn;
} rb;

static void
seednrand(void)
{
	randomread((void*)&rb.randn, sizeof(rb.randn));
}

int
nrand(int n)
{
	rb.randn = rb.randn*1103515245 + 12345 + MACHP(0)->ticks;
	return (rb.randn>>16) % n;
}

static int
rbnotfull(void*)
{
	int i;

	i = rb.wp - rb.rp;
	if(i < 0)
		i += sizeof(rb.buf);
	return i < rb.target;
}

static int
rbnotempty(void*)
{
	return rb.wp != rb.rp;
}

void
genrandom(void*)
{
	setpri(PriBackground);

	for(;;) {
		if(!rbnotfull(0))
			sleep(&rb.producer, rbnotfull, 0);
		rb.randomcount++;
	}
}

/*
 *  produce random bits in a circular buffer
 */
static void
randomclock(void)
{
	uchar *p;

	if(rb.randomcount == 0)
		return;

	if(!rbnotfull(0)) {
		rb.filled = 1;
		return;
	}

	rb.bits = (rb.bits<<2) ^ (rb.randomcount&3);
	rb.randomcount = 0;

	rb.next += 2;
	if(rb.next != 8)
		return;

	rb.next = 0;
	*rb.wp ^= rb.bits ^ *rb.rp;
	p = rb.wp+1;
	if(p == rb.ep)
		p = rb.buf;
	rb.wp = p;

	if(rb.wakeme)
		wakeup(&rb.consumer);
}

static void
randominit(void)
{
	addclock0link(randomclock);
	rb.target = 16;
	rb.ep = rb.buf + sizeof(rb.buf);
	rb.rp = rb.wp = rb.buf;
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
		qunlock(&rb);
		nexterror();
	}

	qlock(&rb);
	if(!rb.kprocstarted){
		rb.kprocstarted = 1;
		kproc("genrand", genrandom, 0);
	}

	for(sofar = 0; sofar < n; sofar += i){
		i = rb.wp - rb.rp;
		if(i == 0){
			rb.wakeme = 1;
			wakeup(&rb.producer);
			sleep(&rb.consumer, rbnotempty, 0);
			rb.wakeme = 0;
			continue;
		}
		if(i < 0)
			i = rb.ep - rb.rp;
		if((i+sofar) > n)
			i = n - sofar;
		memmove(p + sofar, rb.rp, i);
		e = rb.rp + i;
		if(e == rb.ep)
			e = rb.buf;
		rb.rp = e;
	}
	if(rb.filled && rb.wp == rb.rp){
		i = 2*rb.target;
		if(i > sizeof(rb.buf) - 1)
			i = sizeof(rb.buf) - 1;
		rb.target = i;
		rb.filled = 0;
	}
	qunlock(&rb);
	poperror();

	wakeup(&rb.producer);

	return n;
}

static int
console_cmd(char *msg)
{
	if (strcmp("print", msg) == 0) {
		if (consolepush < (1<<29))
			consolepush = (consolepush << 1) | consoleprint;
		consoleprint = 1;
	} else if (strcmp("noprint", msg) == 0) {
		if (consolepush < (1<<29))
			consolepush = (consolepush << 1) | consoleprint;
		consoleprint = 0;
	} else if (strcmp("restore", msg) == 0) {
		if (consolepush != 1)
		{
			consoleprint = consolepush & 1;
			consolepush >>= 1;
		}
	} else
		return 0;
	return 1;
}

static ulong
sysctlwrite(char *msg)
{
	char *fields[3];
	int nf;

	nf = parsefields(msg, fields, nelem(fields), " \n");
	if (nf >= 1)
	{
		if (strcmp(fields[0], "reboot") == 0) {
			reboot();
			panic("reboot failure");
		}

		if (strcmp(fields[0], "halt") == 0) {
			halt();
			panic("halt failure");
		}

		if ((nf == 2) && (strcmp(fields[0], "console") == 0))
			return console_cmd(fields[1]);
	}
	return 0;
}
