#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "ureg.h"
#include "../port/error.h"
#include	"rdbg.h"

#include	<kernel.h>
#include	<interp.h>
#include	<audit.h>

/*
 *	The following should be set in the config file to override
 *	the defaults.
 */
int	dbgstart = 1;
char	*dbgdata;
char	*dbgctl;
char	*dbgctlstart;
char	*dbgctlstop;
char	*dbgctlflush;

//
// Error messages sent to the remote debugger
//
static	uchar	Ereset[9] = { 'r', 'e', 's', 'e', 't' };
static	uchar	Ecount[9] = { 'c', 'o', 'u', 'n', 't' };
static	uchar	Eunk[9] = { 'u', 'n', 'k' };
static	uchar	Einval[9] = { 'i', 'n', 'v', 'a', 'l' };
static	uchar	Ebadpid[9] = {'p', 'i', 'd'};
static	uchar	Eunsup[9] = { 'u', 'n', 's', 'u', 'p' };
static	uchar	Enotstop[9] = { 'n', 'o', 't', 's', 't', 'o', 'p' };
 
//
// Error messages raised via call to error()
//
static	char	Erunning[] = "Not allowed while debugger is running";
static	char	Enumarg[] = "Not enough args";
static	char	Ebadcmd[] = "Unknown command";

static	int	PROCREG;
static	struct {
	Rendez;
	Brk *b;
} brk;

static	Queue	*log;

static	int	chatty = 1;

typedef struct Debugger Debugger;
struct Debugger {
	RWlock;
	int	running;
	char	data[PRINTSIZE];
	char	ctl[PRINTSIZE];
	char	ctlstart[PRINTSIZE];
	char	ctlstop[PRINTSIZE];
	char	ctlflush[PRINTSIZE];
};

static Debugger debugger = {
	.data		"#t/eia0",
	.ctl		"#t/eia0ctl",
	.ctlstart	"B19200",
	.ctlstop	"h",
	.ctlflush	"f",
};

enum {
	BrkStackSize=	256,
};

typedef struct SkipArg SkipArg;
struct SkipArg
{
	Brk *b;
	Proc *p;
};

Brk	*breakpoints;
void	freecondlist(BrkCond *l);


static int
getbreaks(ulong addr, Brk **a, int nb)
{
	Brk *b;
	int n;

	n = 0;
	for(b = breakpoints; b != nil; b = b->next) {
		if(b->addr == addr) {
			a[n++] = b;
			if(n == nb)
				break;
		}
	}
	return n;
}

Brk*
newbreak(int id, ulong addr, BrkCond *conds, void(*handler)(Brk*), void *aux)
{
	Brk *b;

	b = malloc(sizeof(*b));
	if(b == nil)
		error(Enomem);

	b->id = id;
	b->conditions = conds;
	b->addr = addr;
	b->handler = handler;
	b->aux = aux;
	b->next = nil;

	return b;
}

void
freebreak(Brk *b)
{
	freecondlist(b->conditions);
	free(b);
}

BrkCond*
newcondition(uchar cmd, ulong val)
{
	BrkCond *c;

	c = mallocz(sizeof(*c), 0);
	if(c == nil)
		error(Enomem);

	c->op = cmd;
	c->val = val;
	c->next = nil;

	return c;
}

void
freecondlist(BrkCond *l)
{
	BrkCond *next;

	while(l != nil) {
		next = l->next;
		free(l);
		l = next;
	}
}


void
breakset(Brk *b)
{
	Brk *e[1];

	if(getbreaks(b->addr, e, 1) != 0) {
		b->instr = e[0]->instr;
	} else {
		b->instr = machinstr(b->addr);
		machbreakset(b->addr);
	}

	b->next = breakpoints;
	breakpoints = b;
}

void
breakrestore(Brk *b)
{
	b->next = breakpoints;
	breakpoints = b;
	machbreakset(b->addr);
}

Brk*
breakclear(int id)
{
	Brk *b, *e, *p;

	for(b=breakpoints, p=nil; b != nil && b->id != id; p = b, b = b->next)
		;

	if(b != nil) {
		if(p == nil)
			breakpoints = b->next;
		else
			p->next = b->next;

		if(getbreaks(b->addr, &e, 1) == 0)
			machbreakclear(b->addr, b->instr);
	}

	return b;
}

void
breaknotify(Brk *b, Proc *p)
{
	p->dbgstop = 1;		// stop running this process.
	b->handler(b);
}

int
breakmatch(BrkCond *cond, Ureg *ur, Proc *p)
{
	ulong a, b;
	int pos;
	ulong s[BrkStackSize];

	memset(s, 0, sizeof(s));
	pos = 0;

	for(;cond != nil; cond = cond->next) {
		switch(cond->op) {
		default:
			panic("breakmatch: unknown operator %c", cond->op);
			break;
		case 'k':
			if(p == nil || p->pid != cond->val)
				return 0;
			s[pos++] = 1;
			break;
		case 'b':
			if(ur->pc != cond->val)
				return 0;
			s[pos++] = 1;
			break;
		case 'p': s[pos++] = cond->val; break;
		case '*': a = *(ulong*)s[--pos]; s[pos++] = a; break;
		case '&': a = s[--pos]; b = s[--pos]; s[pos++] = a & b; break;
		case '=': a = s[--pos]; b = s[--pos]; s[pos++] = a == b; break;
		case '!': a = s[--pos]; b = s[--pos]; s[pos++] = a != b; break;
		case 'a': a = s[--pos]; b = s[--pos]; s[pos++] = a && b; break;
		case 'o': a = s[--pos]; b = s[--pos]; s[pos++] = a || b; break;
		}
	}

	if(pos && s[pos-1])
		return 1;
	return 0;
}

void
breakinit(void)
{
	machbreakinit();
}

static void
dbglog(char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[PRINTSIZE];

	va_start(arg, fmt);
	n = doprint(buf, buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);
	qwrite(log, buf, n);
}

static int
get(int dbgfd, uchar *b)
{
	int i;
	uchar c;

	if(kread(dbgfd, &c, 1) < 0)
		error(Eio);
	for(i=0; i<9; i++) {
		if(kread(dbgfd, b++, 1) < 0)
			error(Eio);
	}
	return c;
}

static void
mesg(int dbgfd, int m, uchar *buf)
{
	int i;
	uchar c;

	c = m;
	if(kwrite(dbgfd, &c, 1) < 0)
		error(Eio);
	for(i=0; i<9; i++) {
		if(kwrite(dbgfd, buf+i, 1) < 0)
			error(Eio);
	}
}

static ulong
dbglong(uchar *s)
{
	return (s[0]<<24)|(s[1]<<16)|(s[2]<<8)|(s[3]<<0);
}

static Proc *
dbgproc(ulong pid, int dbgok)
{
	int i;
	Proc *p;

	if(!dbgok && pid == up->pid)
		return 0;
	p = proctab(0);
	for(i = 0; i < conf.nproc; i++) {
		if(p->pid == pid)
			return p;
		p++;
	}
	return 0;
}

static void*
addr(uchar *s)
{
	ulong a;
	Proc *p;
	static Ureg ureg;

	a = ((s[0]<<24)|(s[1]<<16)|(s[2]<<8)|(s[3]<<0));
	if(a < sizeof(Ureg)) {
		p = dbgproc(PROCREG, 0);
		if(p == 0) {
			dbglog("dbg: invalid pid\n");
			return 0;
		}
		if(p->dbgreg) {
			/* in trap(), registers are all on stack */
			memmove(&ureg, p->dbgreg, sizeof(ureg));
		}
		else {
			/* not in trap, only pc and sp are available */
			memset(&ureg, 0, sizeof(ureg));
			ureg.sp = p->sched.sp;
			ureg.pc = p->sched.pc;
		}
		return (uchar*)&ureg+a;
	}
	return (void*)a;
}


static void
dumpcmd(uchar cmd, uchar *min)
{
	char *s;
	int n;

	switch(cmd) {
	case Terr:		s = "Terr"; break;
	case Tmget:		s = "Tmget"; break;
	case Tmput:		s = "Tmput"; break;
	case Tspid:		s = "Tspid"; break;
	case Tproc:		s = "Tproc"; break;
	case Tstatus:		s = "Tstatus"; break;
	case Trnote:		s = "Trnote"; break;
	case Tstartstop:	s = "Tstartstop"; break;
	case Twaitstop:		s = "Twaitstop"; break;
	case Tstart:		s = "Tstart"; break;
	case Tstop:		s = "Tstop"; break;
	case Tkill:		s = "Tkill"; break;
	case Tcondbreak:	s = "Tcondbreak"; break;
	default:		s = "<Unknown>"; break;
	}
	dbglog("%s: [%2.2ux]: ", s, cmd);
	for(n = 0; n < 9; n++)
		dbglog("%2.2ux", min[n]);
	dbglog("\n");
}

static int
brkpending(Proc *p)
{
	if(brk.b != nil) return 1;

	p->dbgstop = 0;			// atomic

	return 0;
}

static void
gotbreak(Brk *b)
{
	Brk *cur, *prev;

	b->link = nil;

	for(prev = nil, cur = brk.b; cur != nil; prev = cur, cur = cur->link)
		;
	if(prev == nil)
		brk.b = b;
	else
		prev->link = b;

	wakeup(&brk);
}

static int
startstop(Proc *p)
{
	int id;
	int s;
	Brk *b;

	sleep(&brk, brkpending, p);

	s = splhi();
	b = brk.b;
	brk.b = b->link;
	splx(s);

	id = b->id;

	return id;
}

static int
condbreak(char cmd, ulong val)
{
	BrkCond *c;
	static BrkCond *head = nil;
	static BrkCond *tail = nil;
	static Proc *p = nil;
	static int id = -1;
	int s;

	if(waserror()) {
		dbglog(up->env->error);
		freecondlist(head);
		head = tail = nil;
		p = nil;
		id = -1;
		return 0;
	}

	switch(cmd) {
		case 'b': case 'p':
		case '*': case '&': case '=':
		case '!': case 'a': case 'o':
			break;
		case 'n':
			id = val;
			poperror();
			return 1;
		case 'k':
			p = dbgproc(val, 0);
			if(p == nil)
				error("k: unknown pid");
			break;
		case 'd': {
			Brk *b;

			s = splhi();
			b = breakclear(val);
			if(b != nil) {
				Brk *cur, *prev;

				prev = nil;
				cur = brk.b;
				while(cur != nil) {
					if(cur->id == b->id) {
						if(prev == nil)
							brk.b = cur->link;
						else
							prev->link = cur->link;
						break;
					}
					cur = cur->link;
				}
				freebreak(b);
			}
			splx(s);
			poperror();
			return 1;
		}
		default:
			dbglog("condbreak(): unknown op %c %lux\n", cmd, val);
			error("unknown op");
	}

	c = newcondition(cmd, val);

	 //
	 // the 'b' command comes last, (so we know we have reached the end
	 // of the condition list), but it should be the first thing
	 // checked, so put it at the head.
	 //
	if(cmd == 'b') {
		if(p == nil) error("no pid");
		if(id == -1) error("no id");

		c->next = head;
		s = splhi();
		breakset(newbreak(id, val, c, gotbreak, p));
		splx(s);
		head = tail = nil;
		p = nil;
		id = -1;
	} else if(tail != nil) {
		tail->next = c;
		tail = c;
	} else
		head = tail = c;

	poperror();

	return 1;
}

static void
dbg(void*)
{
	Proc *p;
	ulong val;
	int n;
	struct {int dbgcfd; } dc;
	struct {int dbgfd; } d;
	uchar cmd, *a, min[9], mout[9];

	rlock(&debugger);

	setpri(PriRealtime);

	closefgrp(up->env->fgrp);
	up->env->fgrp = newfgrp();

	d.dbgfd = -1;
	dc.dbgcfd = -1;
	if(waserror()) {
		if(dc.dbgcfd != -1) {
			kwrite(dc.dbgcfd, debugger.ctlflush,
				strlen(debugger.ctlflush));
			kclose(dc.dbgcfd);
		}
		if(d.dbgfd != -1)
			kclose(d.dbgfd);
		runlock(&debugger);
		wlock(&debugger);
		debugger.running = 0;
		wunlock(&debugger);
		pexit("", 0);
	}

	dc.dbgcfd = kopen(debugger.ctl, ORDWR);
	if(dc.dbgcfd < 0) {
		dbglog("dbg: %s: %r\n", debugger.ctl);
		error(Eio);
	}
	if(kwrite(dc.dbgcfd, debugger.ctlstart, strlen(debugger.ctlstart)) < 0){
		dbglog("dbg: kwrite(%s): %r\n", debugger.ctl);
		error(Eio);
	}

	d.dbgfd = kopen(debugger.data, ORDWR);
	if(d.dbgfd < 0) {
		dbglog("dbg: %s: %r\n",debugger.data);
		error(Eio);
	}

	mesg(d.dbgfd, Rerr, Ereset);

	for(;;){
		memset(mout, 0, sizeof(mout));
		cmd = get(d.dbgfd, min);
		if(chatty)
			dumpcmd(cmd, min);
		switch(cmd){
		case Tmget:
			n = min[4];
			if(n > 9){
				mesg(d.dbgfd, Rerr, Ecount);
				break;
			}
			a = addr(min+0);
			if(!isvalid_va(a)) {
				mesg(d.dbgfd, Rerr, Einval);
				break;
			}
			memmove(mout, a, n);
			mesg(d.dbgfd, Rmget, mout);
			break;
		case Tmput:
			n = min[4];
			if(n > 4){
				mesg(d.dbgfd, Rerr, Ecount);
				break;
			}
			a = addr(min+0);
			if(!isvalid_va(a)) {
				mesg(d.dbgfd, Rerr, Einval);
				break;
			}
			memmove(a, min+5, n);
			segflush(a, n);
			mesg(d.dbgfd, Rmput, mout);
			break;
		case Tproc:
			p = dbgproc(dbglong(min+0), 0);
			if(p == 0) {
				mesg(d.dbgfd, Rerr, Ebadpid);
				break;
			}
			PROCREG = p->pid;	/* try this instead of Tspid */
			sprint((char*)mout, "%8.8lux", p);
			mesg(d.dbgfd, Rproc, mout);
			break;
		case Tstatus:
			p = dbgproc(dbglong(min+0), 1);
			if(p == 0) {
				mesg(d.dbgfd, Rerr, Ebadpid);
				break;
			}
			if(p->state > Rendezvous || p->state < Dead) {
				sprint((char*)mout, "%8.8x", p->state);
			} else if(p->dbgstop == 1) {
				strncpy((char*)mout, statename[Stopped],
					sizeof(mout));
			} else {
				strncpy((char*)mout, statename[p->state],
					sizeof(mout));
			}
			mesg(d.dbgfd, Rstatus, mout);
			break;
		case Trnote:
			p = dbgproc(dbglong(min+0), 0);
			if(p == 0) {
				mesg(d.dbgfd, Rerr, Ebadpid);
				break;
			}
			mout[0] = 0;	/* should be trap status, if any */
			mesg(d.dbgfd, Rrnote, mout);
			break;
		case Tstop:
			p = dbgproc(dbglong(min+0), 0);
			if(p == 0) {
				mesg(d.dbgfd, Rerr, Ebadpid);
				break;
			}
			p->dbgstop = 1;			// atomic
			mout[0] = 0;
			mesg(d.dbgfd, Rstop, mout);
			break;
		case Tstart:
			p = dbgproc(dbglong(min+0), 0);
			if(p == 0) {
				mesg(d.dbgfd, Rerr, Ebadpid);
				break;
			}
			p->dbgstop = 0;			// atomic
			mout[0] = 0;
			mesg(d.dbgfd, Rstart, mout);
			break;
		case Tstartstop:
			p = dbgproc(dbglong(min+0), 0);
			if(p == 0) {
				mesg(d.dbgfd, Rerr, Ebadpid);
				break;
			}
			if(!p->dbgstop) {
				mesg(d.dbgfd, Rerr, Enotstop);
				break;
			}

			mout[0] = startstop(p);
			mesg(d.dbgfd, Rstartstop, mout);
			break;
		case Tcondbreak:
			val = dbglong(min+0);
			if(!condbreak(min[4], val)) {
				mesg(d.dbgfd, Rerr, Eunk);
				break;
			}
			mout[0] = 0;
			mesg(d.dbgfd, Rcondbreak, mout);
			break;
		default:
			dumpcmd(cmd, min);
			mesg(d.dbgfd, Rerr, Eunk);
			break;
		}
	}
}

static void
dbgnote(Proc *p, Ureg *ur)
{
	if(p) {
		p->dbgreg = ur;
		PROCREG = p->pid;	/* acid can get the trap info from regs */
	}
}

enum {
	Qdir,
	Qdbgctl,
	Qdbglog,

	DBGrun = 1,
	DBGstop = 2,

	Loglimit = 4096,
};

static Dirtab dbgdir[]=
{
	"dbgctl",	{Qdbgctl},	0,		0660,
	"dbglog",	{Qdbglog},	0,		0440,
};

static void
start_debugger(void)
{
	breakinit();
	dbglog("starting debugger\n");
	debugger.running++;
	kproc("dbg", dbg, 0);
}

static void
_dbginit(void)
{

	log = qopen(Loglimit, 0, 0, 0);
	if(log == nil)
		error(Enomem);
	qnoblock(log, 1);

	wlock(&debugger);
	if(waserror()) {
		wunlock(&debugger);
		qfree(log);
		log = nil;
		nexterror();
	}

	if (dbgdata) {
		strncpy(debugger.data, dbgdata, sizeof(debugger.data));
		debugger.data[sizeof(debugger.data)-1] = 0;
	}

	if (dbgctl) {
		strncpy(debugger.ctl, dbgctl, sizeof(debugger.ctl));
		debugger.ctl[sizeof(debugger.ctl)-1] = 0;
	}

	if (dbgctlstart) {
		strncpy(debugger.ctlstart, dbgctlstart, sizeof(debugger.ctlstart));
		debugger.ctlstart[sizeof(debugger.ctlstart)-1] = 0;
	}

	if (dbgctlstop) {
		strncpy(debugger.ctlstop, dbgctlstop, sizeof(debugger.ctlstop));
		debugger.ctlstop[sizeof(debugger.ctlstop)-1] = 0;
	}

	if (dbgctlflush) {
		strncpy(debugger.ctlflush, dbgctlflush, sizeof(debugger.ctlflush));
		debugger.ctlflush[sizeof(debugger.ctlflush)-1] = 0;
	}

	if(dbgstart)
		start_debugger();

	poperror();
	wunlock(&debugger);
}

static Chan*
dbgattach(char *spec)
{
	return devattach('b', spec);
}

static int
dbgwalk(Chan *c, char *name)
{
	return devwalk(c, name, dbgdir, nelem(dbgdir), devgen);
}

static void
dbgstat(Chan *c, char *dp)
{
	devstat(c, dp, dbgdir, nelem(dbgdir), devgen);
}

static Chan*
dbgopen(Chan *c, int omode)
{
	return devopen(c, omode, dbgdir, nelem(dbgdir), devgen);
}

static long
dbgread(Chan *c, void *buf, long n, ulong offset)
{
	char ctlstate[PRINTSIZE];

	switch(c->qid.path & ~CHDIR) {
	case Qdir:
		return devdirread(c, buf, n, dbgdir, nelem(dbgdir), devgen);
	case Qdbgctl:
		rlock(&debugger);
		sprint(ctlstate, "%s data %s ctl %s ctlstart %s ctlstop %s ctlflush %s\n",
			debugger.running ? "running" : "stopped",
			debugger.data, debugger.ctl, 
			debugger.ctlstart, debugger.ctlstop, debugger.ctlflush);
		n = readstr(offset, buf, n, ctlstate);
		runlock(&debugger);
		return n;
	case Qdbglog:
		return qread(log, buf, n);
	default:
		error(Egreg);
	}
	return -1;		/* never reached */
}

static void
ctl(char *buf)
{
	Debugger d;
	int dbgstate = 0;
	char *fields[8];
	int i;
	int nf;
	char *df;
	int dfsize;
	int setval;

	nf = parsefields(buf, fields, nelem(fields), " \t");
	memset(&d, 0, sizeof(d));
	for(i=0; nf ; i++, nf--) {
		setval = 0;
		df = nil;
		dfsize = 0;
		switch(fields[i][0]) {
			case 'd':
				df = d.data;
				dfsize = sizeof(d.data);
				setval=1;
				break;
			case 'c':
				df = d.ctl;
				dfsize = sizeof(d.ctl);
				setval=1;
				break;
			case 'i':
				df = d.ctlstart;
				dfsize = sizeof(d.ctlstart);
				setval=1;
				break;
			case 'h':
				df = d.ctlstop;
				dfsize = sizeof(d.ctlstop);
				setval=1;
				break;
			case 'f':
				df = d.ctlflush;
				dfsize = sizeof(d.ctlflush);
				setval=1;
				break;
			case 'r':
				dbgstate = DBGrun;
				break;
			case 's':
				dbgstate = DBGstop;
				break;
			default:
				error(Ebadcmd);
		}
		if(setval) {
			if(nf < 2)
				error(Enumarg);
			strncpy(df, fields[i+1], dfsize-1);
			df[dfsize-1] = 0;
			++d.running;
			++i;
			--nf;
		}
	}

	if(d.running) {
		wlock(&debugger);
		if(debugger.running) {
			wunlock(&debugger);
			error(Erunning);
		}
		if(d.data[0] != 0) {
			strcpy(debugger.data, d.data);
			dbglog("data %s\n",debugger.data);
		}
		if(d.ctl[0] != 0) {
			strcpy(debugger.ctl, d.ctl);
			dbglog("ctl %s\n",debugger.ctl);
		}
		if(d.ctlstart[0] != 0) {
			strcpy(debugger.ctlstart, d.ctlstart);
			dbglog("ctlstart %s\n",debugger.ctlstart);
		}
		if(d.ctlstop[0] != 0) {
			strcpy(debugger.ctlstop, d.ctlstop);
			dbglog("ctlstop %s\n",debugger.ctlstop);
		}
		wunlock(&debugger);
	}

	if(dbgstate == DBGrun) {
		if(!debugger.running) {
			wlock(&debugger);
			if(waserror()) {
				wunlock(&debugger);
				nexterror();
			}
			if(!debugger.running)
				start_debugger();
			else
				dbglog("debugger already running\n");
			poperror();
			wunlock(&debugger);
		} else
			dbglog("debugger already running\n");
	} else if(dbgstate == DBGstop) {
		if(debugger.running) {
			int cfd;
			cfd = kopen(debugger.ctl, OWRITE);
			if(cfd == -1)
				error(Eio);
			dbglog("stopping debugger\n");
			if(kwrite(cfd, debugger.ctlstop,
				  strlen(debugger.ctlstop)) == -1) {
				error(Eio);
			}
			kclose(cfd);
		} else
			dbglog("debugger not running\n");
	}
}

static long
dbgwrite(Chan *c, void *va, long n, ulong)
{
	char buf[PRINTSIZE];

	switch(c->qid.path & ~CHDIR) {
	default:
		error(Egreg);
		break;
	case Qdbgctl:
		if(n >= sizeof(buf))
			n = sizeof(buf)-1;
		strncpy(buf, (char*)va, n);
		buf[n] = 0;
		if(buf[n-1] == '\n')
			buf[n-1] = 0;
		ctl(buf);
		break;
	}
	return n;
}

static void
dbgclose(Chan*)
{
}

Dev dbgdevtab = {
	'b',
	"dbg",

	devreset,
	_dbginit,
	dbgattach,
	devdetach,
	devclone,
	dbgwalk,
	dbgstat,
	dbgopen,
	devcreate,
	dbgclose,
	dbgread,
	devbread,
	dbgwrite,
	devbwrite,
	devremove,
	devwstat,
};
