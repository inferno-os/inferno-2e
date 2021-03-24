#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"audit.h"

Ref	pidalloc;

struct
{
	Lock;
	Proc*	arena;
	Proc*	free;
}procalloc;

typedef struct
{
	Lock;
	Proc*	head;
	Proc*	tail;
}Schedq;

Schedq	runq[Nrq];
int	nrdy;
int	rdypri;

char *statename[] =
{			/* BUG: generate automatically */
	"Dead",
	"Moribund",
	"Ready",
	"Scheding",
	"Running",
	"Queueing",
	"Wakeme",
	"Broken",
	"Stopped",
	"Rendez",
};

/*
 * Always splhi()'ed.
 */
void
schedinit(void)		/* never returns */
{
	setlabel(&m->sched);
	if(up) {
		m->proc = 0;
		switch(up->state) {
		case Running:
			ready(up);
			break;
		case Moribund:
			up->state = Dead;
			/*
			 * Holding locks from pexit:
			 * 	procalloc
			 */
			up->qnext = procalloc.free;
			procalloc.free = up;
			unlock(&procalloc);
			break;
		}
		up->mach = 0;
		up = 0;
	}
	sched();
}

void
sched(void)
{
	if(up) {
		splhi();
		procsave(up);
		if(setlabel(&up->sched)) {
			spllo();
			return;
		}
		gotolabel(&m->sched);
	}
	up = runproc();
	up->state = Running;
	up->mach = m;
	m->proc = up;
	gotolabel(&up->sched);
}

void
ready(Proc *p)
{
	int s;
	Schedq *rq;

	s = splhi();

	if(p->pri < rdypri)
		rdypri = p->pri;
	rq = &runq[p->pri];

	lock(rq);
	p->rnext = 0;
	if(rq->tail)
		rq->tail->rnext = p;
	else
		rq->head = p;
	rq->tail = p;

	nrdy++;
	p->state = Ready;
	unlock(rq);
	splx(s);
}

Proc*
runproc(void)
{
	Proc *p;
	Schedq *rq, *erq;

	rdypri = Nrq;
	erq = runq + Nrq - 1;
loop:
	spllo();
	for(rq = runq; rq < erq && rq->head == 0; rq++)
		;
	splhi();

	for(rq = runq; rq < erq && rq->head == 0; rq++)
		;

dbg:
	lock(rq);
	if(rq->head == 0) {
		unlock(rq);
		goto loop;
	}

	p = rq->head;
	/* p->mach==0 only when process state is saved */
	if(p == 0 || p->mach) {
		unlock(rq);
		goto loop;
	}
	if(p->dbgstop) {
		Proc *prev;

		prev = p;
		while((p = p->rnext) != nil) {
			if(p->dbgstop) {
				prev = p;
				continue;
			}
			if(p->state != Ready)
				print("runproc %s %lud %s\n", p->text, p->pid,
					statename[p->state]);
			prev->rnext = p->rnext;
			nrdy--;
			rdypri = p->pri;
			if(p->rnext == 0) {
				rdypri = p->pri + 1;
				rq->tail = prev;
			}
			unlock(rq);
			p->state = Scheding;
			return p;
		}
		unlock(rq);
		for(++rq; rq < erq && rq->head == 0; rq++)
			;
		goto dbg;
	}
	rdypri = p->pri;
	if(p->rnext == 0)
	{
		rdypri = p->pri + 1;
		rq->tail = 0;
	}
	rq->head = p->rnext;
	nrdy--;
	if(p->state != Ready)
		print("runproc %s %lud %s\n", p->text, p->pid, statename[p->state]);
	unlock(rq);
	p->state = Scheding;
	return p;
}

int
setpri(int pri)
{
	int p;

	p = up->pri;
	up->pri = pri;
	if(up->state == Running)
		sched();
	return p;
}

Proc*
newproc(void)
{
	Proc *p;

	lock(&procalloc);
	for(;;) {
		if(p = procalloc.free)
			break;

		unlock(&procalloc);
		resrcwait("no procs");
		lock(&procalloc);
	}
	procalloc.free = p->qnext;
	unlock(&procalloc);

	p->type = Unknown;
	p->state = Scheding;
	p->pri = PriNormal;
	p->psstate = "New";
	p->mach = 0;
	p->qnext = 0;
	p->fpstate = FPINIT;
	p->kp = 0;
	p->swipend = 0;
	p->mp = 0;
	p->env = &p->defenv;
	p->dbgreg = 0;

	p->pid = incref(&pidalloc);
	if(p->pid == 0)
		panic("pidalloc");
	if(p->kstack == 0)
		p->kstack = smalloc(KSTACK);
	strcpy(p->kstack+500, "stackmark");
	addprog(p);

	return p;
}

void
procinit(void)
{
	Proc *p;
	int i;

	procalloc.free = xalloc(conf.nproc*sizeof(Proc));
	procalloc.arena = procalloc.free;

	p = procalloc.free;
	for(i=0; i<conf.nproc-1; i++,p++)
		p->qnext = p+1;
	p->qnext = 0;

	debugkey('p', "processes", procdump, 0);
}

void
sleep1(Rendez *r, int (*f)(void*), void *arg)
{
	int s;

	if (!up)
		panic("sleep() not in process (%lux)", getcallerpc(&r));
	/*
	 * spl is to allow lock to be called
	 * at interrupt time. lock is mutual exclusion
	 */
	s = splhi();
	up->r = r;	/* early so postnote knows */
	lock(r);

	/*
	 * if condition happened, never mind
	 */
	if(f(arg)){
		up->r = 0;
		unlock(r);
		splx(s);
		return;
	}

	/*
	 * now we are committed to
	 * change state and call scheduler
	 */
	if(r->p != nil) {
		print("double sleep %lud %lud r=%lux\n", r->p->pid, up->pid, r);
		dumpstack();
		panic("sleep");
	}
	up->state = Wakeme;
	r->p = up;
	unlock(r);
}

void
sleep(Rendez *r, int (*f)(void*), void *arg)
{
	int s;

	sleep1(r, f, arg);
	if(up->swipend == 0)
		sched();	/* swipend set during sleep */

	if(up->swipend) {
		up->swipend = 0;
		up->twhen = 0;	/* because tsleep may not get the chance */
		s = splhi();
		lock(r);
		if(r->p == up)
			r->p = 0;
		unlock(r);
		splx(s);
		error(Eintr);
	}
}

int
tfn(void *arg)
{
	return MACHP(0)->ticks >= up->twhen || (*up->tfn)(arg);
}

/* this function returns the time left before timeout;
   if timeout happens, it returns 0;
*/
int
tsleep(Rendez *r, int (*fn)(void*), void *arg, int ms)
{
	ulong when;
	Proc *f, **l;

	if (!up)
		panic("tsleep() not in process (%lux)", getcallerpc(&r));

	when = MS2TK(ms)+MACHP(0)->ticks;

	lock(&talarm);
	/* take out of list if checkalarm didn't */
	if(up->trend) {
		l = &talarm.list;
		for(f = *l; f; f = f->tlink) {
			if(f == up) {
				*l = up->tlink;
				break;
			}
			l = &f->tlink;
		}
	}
	/* insert in increasing time order */
	l = &talarm.list;
	for(f = *l; f; f = f->tlink) {
		if(f->twhen >= when)
			break;
		l = &f->tlink;
	}
	up->trend = r;
	up->twhen = when;
	up->tfn = fn;
	up->tlink = *l;
	*l = up;
	unlock(&talarm);

	sleep(r, tfn, arg);
	up->twhen = 0;

        /* return the time left before timeout */
        return TK2MS(when - MACHP(0)->ticks);
}

void
wakeup(Rendez *r)
{
	Proc *p;
	int s;

	s = splhi();
	lock(r);
	p = r->p;
	if(p){
		r->p = 0;
		if(p->state != Wakeme)
			panic("wakeup: state");
		p->r = 0;
		ready(p);
	}
	unlock(r);
	splx(s);
}

void
swiproc(Proc *p)
{
	ulong s;
	Rendez *r;

	if(p == nil)
		return;

	s = splhi();
	r = p->r;
	if(r != nil) {
		lock(r);
		if(p->r == r) {
			p->swipend = 1;
			r->p = nil;
			p->r = nil;
			ready(p);
		}
		unlock(r);
	}
	splx(s);
}


int
chkpstk(Proc *p, int *pmax, ulong *pmaxpc)
{
	int used;
	ulong *sp;
	int ii;

	sp = (ulong *)p->kstack;
	ii = 600/4;
	sp += ii;
	for (; ii < KSTACK/4; sp++, ii++)
		if (*sp != 0)
			break;
	used = KSTACK - 4*ii;
	if (used > *pmax)
	{
		*pmax = used;
		for (; ii < KSTACK/4; sp++, ii++)
		{
			if (isvalid_pc(*sp))
			{
				*pmaxpc = *sp;
				break;
			}
		}
	}
	return(used);
}

void
pexit(char*, int)
{
	Osenv *o;

	up->alarm = 0;

	o = up->env;

	if (o != nil) {
		closefgrp(o->fgrp);
		closepgrp(o->pgrp);
	}
	{
		int max = 0;
		ulong maxpc;
		int used;

		used = chkpstk(up, &max, &maxpc);
		print("%s: stack used: %d(%N)\n", up->text, used, maxpc);
	}
	/* Sched must not loop for this lock */
	lock(&procalloc);

	up->state = Moribund;
	sched();
	panic("pexit");
}

Proc*
proctab(int i)
{
	return &procalloc.arena[i];
}


void
procdump(void)
{
	int i;
	char *s;
	int max = 0;
	ulong maxpc;
	int used;
	Proc *p;

	for(i=0; i<conf.nproc; i++) {
		p = &procalloc.arena[i];
		if(p->state == Dead)
			continue;

		s = p->psstate;
		if(s == 0)
			s = "kproc";
		print("%6lux:%3lud:%14.14s %12.12N %s/%s",
			p, p->pid, p->text, p->pc, s, statename[p->state]);

		switch(p->state)
		{
		case Wakeme:
			print("/%lux", p->r);
			break;
		default:
			print("       ");
			break;
		}
		used = chkpstk(p, &max, &maxpc);
		print(" pri %d s %d\n", p->pri, used);
	}
	print("Max stack: %d, %N\n", max, maxpc);
}

void
kproc(char *name, void (*func)(void *), void *arg)
{
	Proc *p;
	Pgrp *pg;
	Fgrp *fg;

	p = newproc();
	p->psstate = 0;
	p->kp = 1;

	p->fpsave = up->fpsave;
	p->scallnr = up->scallnr;
	p->nerrlab = 0;

	memmove(p->env->user, up->env->user, NAMELEN);
	pg = up->env->pgrp;
	fg = up->env->fgrp;
	incref(pg);
	incref(fg);
	p->env->pgrp = pg;
	p->env->fgrp = fg;

	kprocchild(p, func, arg);

	strcpy(p->text, name);

	ready(p);
}

void
error(char *err)
{
	if (!up)
		panic("Error %s not running a process\n", err);
	spllo();
	strncpy(up->env->error, err, ERRLEN);
	nexterror();
}

#include "errstr.h"

/* Set kernel error string */
void
kerrstr(char *err)
{
	char *s;
	char buf[ERRLEN];

	s = buf;
	if(up != nil)
		s = up->env->error;
	strncpy(s, err, ERRLEN);
}

/* Get kernel error string */
void
kgerrstr(char *err)
{
	char *s;

	s = "<no-up>";
	if(up != nil)
		s = up->env->error;
	strncpy(err, s, ERRLEN);
}

/* Set kernel error string, using formatted print */
void
kwerrstr(char *fmt, ...)
{
	va_list arg;
	char buf[ERRLEN];

	va_start(arg, fmt);
	doprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	strncpy(up->env->error, buf, ERRLEN);
}

void
nexterror(void)
{
	gotolabel(&up->errlab[--up->nerrlab]);
}

void
exhausted(char *resource)
{
	char buf[ERRLEN];

	snprint(buf, sizeof(buf), "no free %s", resource);
	error(buf);
}

/*
 *  change ownership to 'new' of all processes owned by 'old'.  Used when
 *  eve changes.
 */
void
renameuser(char *old, char *new)
{
	char *u;
	Proc *p, *ep;

	ep = procalloc.arena+conf.nproc;
	for(p = procalloc.arena; p < ep; p++) {
		u = p->env->user;
		if(strcmp(old, u) == 0)
			memmove(u, new, NAMELEN);
	}
}

int
return0(void*)
{
	return 0;
}

void
setid(char *name)
{
	USED(name);
}
