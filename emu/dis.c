#include	"dat.h"
#include	"fns.h"
#include	<isa.h>
#include	<interp.h>
#include	"error.h"

extern	int	Dconv(va_list*, Fconv*);
extern	int	aconv(va_list*, Fconv*);

struct
{
	Lock	l;
	Prog*	runhd;
	Prog*	runtl;
	Prog*	head;
	Prog*	tail;
	Rendez	irend;
	int	idle;
	int	yield;
	Proc*	vmq;
	Proc*	vmqt;
	Atidle*	idletasks;
} isched;

int	cflag;

int
tready(void *a)
{
	USED(a);
	return isched.runhd != nil || isched.yield != 0;
}

Prog*
progn(int n)
{
	Prog *p;

	for(p = isched.head; p && n--; p = p->next)
		;
	return p;
}

Prog*
progpid(int pid)
{
	Prog *p;

	for(p = isched.head; p; p = p->next)
		if(p->pid == pid)
			break;
	return p;
}

int
nprog(void)
{
	int n;
	Prog *p;

	n = 0;
	for(p = isched.head; p; p = p->next)
		n++;
	return n;
}

static void
execatidle(void)
{
	int done;

	if(tready(nil))
		return;

	up->type = IdleGC;
	up->iprog = nil;
	addrun(up->prog);

	done = gccolor+3;
	while(gccolor < done && gcruns()) {
		if(isched.yield != 0 || isched.runhd != isched.runtl)
			break;

		rungc(isched.head);
	}

	up->type = Interp;
	delrunq(up->prog);
}

Prog*
newprog(Prog *p, Modlink *m)
{
	Heap *h;
	Prog *n;
	Osenv *on, *op;
	static int pidnum;

	n = malloc(sizeof(Prog)+sizeof(Osenv));
	if(n == 0)
		panic("no memory");

	addrun(n);
	n->pid = ++pidnum;
	n->grp = n->pid;

	if(isched.tail != nil) {
		n->prev = isched.tail;
		isched.tail->next = n;
	}
	else {
		isched.head = n;
		n->prev = nil;
	}
	isched.tail = n;

	n->osenv = (Osenv*)((uchar*)n + sizeof(Prog));
	n->xec = xec;
	n->quanta = 2048;

	h = D2H(m);
	h->ref++;
	Setmark(h);
	n->R.M = m;
	n->R.MP = m->MP;
	if(m->MP != H)
		Setmark(D2H(m->MP));

	if(p == nil)
		return n;

	n->grp = p->grp;
	memmove(n->osenv, p->osenv, sizeof(Osenv));
	op = p->osenv;
	on = n->osenv;
	on->waitq = op->childq;
	on->childq = nil;
	on->debug = nil;
	incref(&on->pgrp->r);
	incref(&on->fgrp->r);

	return n;
}

void
delprog(Prog *p, char *msg)
{
	Osenv *o;

	/* call before being removed from prog list */
	tellsomeone(p, msg);

	if(p->prev) 
		p->prev->next = p->next;
	else
		isched.head = p->next;

	if(p->next)
		p->next->prev = p->prev;
	else
		isched.tail = p->prev;

	o = p->osenv;
	release();
	closepgrp(o->pgrp);
	closefgrp(o->fgrp);
	acquire();
	closeexp(p);

	if(p == isched.runhd) {
		isched.runhd = p->link;
		if(p->link == nil)
			isched.runtl = nil;
	}
	p->state = 0xdeadbeef;
	free(p);
}

void
tellsomeone(Prog *p, char *buf)
{
	Osenv *o;

	o = p->osenv;
	if(o->childq != nil)
		qproduce(o->childq, buf, strlen(buf));
	if(o->waitq != nil) 
		qproduce(o->waitq, buf, strlen(buf)); 
}

static void
swiprog(Prog *p)
{
	Proc *q;

	lock(&procs.l);
	for(q = procs.head; q; q = q->next) {
		if(q->iprog == p) {
			unlock(&procs.l);
			swiproc(q);
			return;
		}
	}
	unlock(&procs.l);
	print("didn't find\n");
}
int
killprog(Prog *p, char *cause)
{
	Osenv *env;
	Channel *c;
	Prog *f, **l;
	char msg[ERRLEN+2*NAMELEN];

	if(p == isched.runhd) {
		p->kill = "";
		p->state = Pexiting;
		return 0;
	}

	switch(p->state) {
	case Palt:
		altdone(p->R.s, p, nil, -1);
		break;
	case Psend:
		c = p->chan;
		l = &c->send;
	delcomm:
		for(f = *l; f; f = f->comm) {
			if(f == p) {
				*l = p->comm;
				break;
			}
			l = &f->comm;
		}
		break;
	case Precv:
		c = p->chan;
		l = &c->recv;
		goto delcomm;
	case Pready:
		delrunq(p);
		break;
	case Prelease:
		p->kill = "";
		p->state = Pexiting;
		swiprog(p);
		/* No break */
	case Pexiting:
		return 0;
	case Pbroken:
	case Pdebug:
		break;
	default:
		panic("killprog - bad state 0x%x\n", p->state);
	}

	if(p->addrun != nil) {
		p->kill = "";
		p->addrun(p);
		p->addrun = nil;
		return 0;
	}

	env = p->osenv;
	if(env->debug != nil) {
		p->state = Pbroken;
		dbgexit(p, 0, cause);
		return 0;
	}

	snprint(msg, sizeof(msg), "%d \"%s\":%s", p->pid, p->R.M->m->name, cause);

	p->state = Pexiting;
	gclock();
	destroystack(&p->R);
	delprog(p, msg);
	gcunlock();

	return 1;
}

char	changup[] = "channel hangup";

void
killcomm(Prog **pp)
{
	Prog *p;

	for (p = *pp; p != nil; p = *pp) {
		*pp = p->comm;
		p->ptr = nil;
		switch(p->state) {
		case Prelease:
			swiprog(p);
			break;
		case Psend:
		case Precv:
			p->kill = changup;
			addrun(p);
			break;
		}
	}
}

void
addprog(Proc *p)
{
	Prog *n;

	n = malloc(sizeof(Prog));
	if(n == nil)
		panic("no memory");
	p->prog = n;
	n->osenv = p->env;
}

static void
cwakeme(Prog *p)
{
	Osenv *o;

	p->addrun = nil;
	o = p->osenv;
	Wakeup(o->rend);
}

static int
cdone(void *vp)
{
	Prog *p = vp;

	return p->addrun == nil || p->kill != nil;
}

void
cblock(Prog *p)
{
	Osenv *o;

	p->addrun = cwakeme;
	o = p->osenv;
	o->rend = &up->sleep;
	release();

	/*
	 * To allow cdone(p) safely after release,
	 * p must be currun before the release.
	 * Exits in the error case with the vm acquired.
	 */
	if(waserror()) {
		acquire();
		p->addrun = nil;
		nexterror();
	}
	Sleep(o->rend, cdone, p);
	if (p->kill != nil)
		error(Eintr);
	poperror();
	acquire();
}

void
addrun(Prog *p)
{
	if(p->addrun != 0) {
		p->addrun(p);
		return;
	}
	p->state = Pready;
	p->link = nil;
	if(isched.runhd == nil)
		isched.runhd = p;
	else
		isched.runtl->link = p;

	isched.runtl = p;
}

Prog*
delrun(int state)
{
	Prog *p;

	p = isched.runhd;
	p->state = state;
	isched.runhd = p->link;
	if(p->link == nil)
		isched.runtl = nil;

	return p;
}

void
delrunq(Prog *p)
{
	Prog *prev, *f;

	prev = nil;
	for(f = isched.runhd; f; f = f->link) {
		if(f == p)
			break;
		prev = f;
	}
	if(f == nil)
		return;
	if(prev == nil)
		isched.runhd = p->link;
	else
		prev->link = p->link;
	if(p == isched.runtl)
		isched.runtl = prev;
}

Prog*
delruntail(int state)
{
	Prog *p;

	p = isched.runtl;
	delrunq(isched.runtl);
	p->state = state;
	return p;
}

Prog*
currun(void)
{
	return isched.runhd;
}

Prog*
schedmod(Module *m)
{
	Heap *h;
	Type *t;
	Prog *p;
	Modlink *ml;
	Frame f, *fp;

	ml = mklinkmod(m, 0);

	if(m->origmp != H && m->ntype > 0) {
		t = m->type[0];
		h = nheap(t->size);
		h->t = t;
		t->ref++;
		ml->MP = H2D(uchar*, h);
		newmp(ml->MP, m->origmp, t);
	}

	p = newprog(nil, ml);
	h = D2H(ml);
	h->ref--;
	p->R.PC = m->entry;
	fp = &f;
	R.s = &fp;
	f.t = m->entryt;
	newstack(p);
	initmem(m->entryt, p->R.FP);

	return p;
}
	
void
acquire(void)
{
	int n;
	Prog *p;

	lock(&isched.l);
	if(isched.idle) {
		isched.idle = 0;
		unlock(&isched.l);
	}
	else {
		n = isched.yield++;
		up->qnext = isched.vmq;
		if(isched.vmq == nil)
			isched.vmqt = up;
		isched.vmq = up;

		unlock(&isched.l);
		strcpy(up->text, "acquire");
		if(n == 0)
			Wakeup(&isched.irend);
		rendezvous(up, 0);
	}

	if(up->type == Interp) {
		p = up->iprog;
		up->iprog = nil;
		irestore(p);
	}
	else
		p = up->prog;

	p->state = Pready;
	p->link = isched.runhd;
	isched.runhd = p;
	if(p->link == nil)
		isched.runtl = p;

	strcpy(up->text, "dis");
}

void
release(void)
{
	Proc *p;

	if(up->type == Interp)
		up->iprog = isave();
	else
		delrun(Prelease);

	lock(&isched.l);
	if(isched.vmq == nil) {
		isched.idle = 1;
		unlock(&isched.l);
		kproc("dis", vmachine, nil);
		return;
	}
	p = isched.vmq;
	isched.vmq = p->qnext;
	unlock(&isched.l);

	rendezvous(p, 0);		/* wake up thread to run VM */
	strcpy(up->text, "released");
}

void
iyield(void)
{
	Proc *p;

	lock(&isched.l);
	isched.yield--;
	p = isched.vmq;
	if(p == nil) {
		unlock(&isched.l);
		return;
	}
	isched.vmq = p->qnext;

	if(isched.vmq == nil)
		isched.vmq = up;
	else
		isched.vmqt->qnext = up;
	isched.vmqt = up;
	up->qnext = nil;

	unlock(&isched.l);
	rendezvous(p, 0);		/* wake up aquiring kproc */
	strcpy(up->text, "yield");
	rendezvous(up, 0);		/* sleep */
	strcpy(up->text, "dis");
}

void
startup(void)
{
	Osenv *o;

	up->type = Interp;
	o = up->env;
	closefgrp(o->fgrp);
	closepgrp(o->pgrp);

	lock(&isched.l);
	if(isched.idle) {
		isched.idle = 0;
		unlock(&isched.l);
		return;
	}
	if(isched.vmq == nil)
		isched.vmq = up;
	else
		isched.vmqt->qnext = up;
	isched.vmqt = up;
	up->qnext = nil;

	unlock(&isched.l);
	rendezvous(up, 0);
}

void
progexit(void)
{
	Prog *r;
	Module *m;
	int broken;
	char *estr, msg[ERRLEN+2*NAMELEN];

	estr = up->env->error;
	broken = 0;
	if(estr[0] != '\0' && strcmp(estr, Eintr) != 0)
		broken = 1;

	r = up->iprog;
	if(r != nil)
		acquire();
	else
		r = currun();

	m = R.M->m;
	if(broken)
		print("[%s] Broken: \"%s\"\n", m->name, estr);

	snprint(msg, sizeof(msg), "%d \"%s\":%s", r->pid, m->name, estr);

	if(up->env->debug != nil) {
		dbgexit(r, broken, estr);
		broken = 1;
	}

	if(broken) {
		tellsomeone(r, msg);
		r = isave();
		r->state = Pbroken;
		return;
	}

	gclock();
	destroystack(&R);
	delprog(r, msg);
	gcunlock();

	if(isched.head == nil)
		cleanexit(0);
}

void
disfault(void *reg, char *msg)
{
	Prog *p;
	ulong pc;
	Osenv *o;

	USED(reg);

	if(strncmp(msg, Eintr, 6) == 0)
		exits(0);

	if(up == nil) {
		print("EMU: faults: %s\n", msg);
		cleanexit(0);
	}
	if(up->type != Interp) {
		print("SYS: process %s faults: %s\n", up->text, msg);
		cleanexit(0);
	}

	if(up->iprog != nil)
		acquire();

	p = currun();
	if(p == nil)
		panic("Interp faults with no dis prog");

	/* cause an exception on the dis prog.  As for error(), but Brazil needs reg*/
	strncpy(up->env->error, msg, ERRLEN);
	oslongjmp(reg, up->estack[--up->nerr], 1);
}

void
vmachine(void *a)
{
	Prog *r;
	Osenv *o;

	USED(a);

	startup();

	while(waserror()) {
		if(up->iprog != nil)
			acquire();
		if(handler(up->env->error) == 0)
			progexit();
	}

	for(;;) {
		if(tready(nil) == 0) {
			execatidle();
			strcpy(up->text, "idle");
			Sleep(&isched.irend, tready, 0);
			strcpy(up->text, "dis");
		}

		if(isched.yield != 0)
			iyield();

		r = isched.runhd;
		if(r != nil) {
			o = r->osenv;
			up->env = o;

			FPrestore(&o->fpu);
			r->xec(r);
			FPsave(&o->fpu);

			if(isched.runhd != nil)
			if(r == isched.runhd)
			if(isched.runhd != isched.runtl) {
				isched.runhd = r->link;
				r->link = nil;
				isched.runtl->link = r;
				isched.runtl = r;
			}
		}
		if(isched.runhd != nil)
		if(memlow()) {
			up->type = IdleGC;
			pushrun(up->prog);
			rungc(isched.head);
			up->type = Interp;
			delrunq(up->prog);
		}
	}
}

static int
econv(va_list *arg, Fconv *fp)
{
	char *s;

	USED(arg);

	s = "no up";
	if(up != 0)
		s = up->env->error;
	strconv(s, fp);
	return 0;
}

void
disinit(void *a)
{
	Prog *p;
	Osenv *o;
	Module *root;
	char *initmod = a;

	if(waserror())
		panic("disinit error: %r");

	print("Initialize Dis: %s\n", initmod);

	fmtinstall('D', Dconv);
	FPinit();
	FPsave(&up->env->fpu);

	opinit();
	modinit();

	root = readmod(initmod, nil, 0);
	if(root == 0)
		panic("loading \"%s\": %r", initmod);

	p = schedmod(root);

	memmove(p->osenv, up->env, sizeof(Osenv));
	o = p->osenv;
	incref(&o->pgrp->r);
	incref(&o->fgrp->r);

	isched.idle = 1;
	poperror();
	vmachine(nil);
}

void
pushrun(Prog *p)
{
	if(p->addrun != nil)
		panic("pushrun addrun");
	p->state = Pready;
	p->link = isched.runhd;
	isched.runhd = p;
	if(p->link == nil)
		isched.runtl = p;
}

void
releasex(void)
{
	Proc *p;

	lock(&isched.l);
	if(isched.vmq == nil) {
		isched.idle = 1;
		unlock(&isched.l);
		kproc("dis", vmachine, nil);
		return;
	}
	p = isched.vmq;
	isched.vmq = p->qnext;
	unlock(&isched.l);

	rendezvous(p, 0);
}
