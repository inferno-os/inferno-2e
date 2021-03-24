#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"interp.h"

void
Sleep(Rendez *r, int (*f)(void*), void *arg)
{
	up->r = r;
	lock(&r->l);
	if(f(arg)) {
             	up->r = nil;
		unlock(&r->l);
		return;
	}

	if(r->p != nil)
		panic("Sleep %s %s r=0x%lux\n", r->p->text, up->text, r);

	up->swipend = 0;
	r->p = up;
	unlock(&r->l);

	rendezvous(up, 0);

	if(up->swipend) {
		up->swipend = 0;
		error(Eintr);
	}
}

void
Wakeup(Rendez *r)
{
	Proc *p;

	lock(&r->l);
	p = r->p;
	if(p != nil) {
		r->p = nil;
		p->r = nil;
		rendezvous(p, 0);
	}
	unlock(&r->l);
}

void
swiproc(Proc *p)
{
	Rendez *r;
	
	if(p == nil)
		return;

	/*
	 * Pull out of emu Sleep
	 */
	r = p->r;
	if(r != nil) {
		lock(&r->l);
		if(p->r == r) {
			p->swipend = 1;
			r->p = nil;
			p->r = nil;
			rendezvous(p, 0);
		}
		unlock(&r->l);
		return;
	}

	/*
	 * Maybe pull out of Host OS
	 */
	lock(&p->sysio);
	if(p->syscall && p->intwait == 0) {
		p->intwait = 1;
		p->swipend = 1;
		unlock(&p->sysio);
		oshostintr(p);
		return;	
	}
	unlock(&p->sysio);
}

void
osenter(void)
{
	up->syscall = 1;
	strcpy(up->text, "syscall");
}

void
osleave(void)
{
	int r;

	lock(&up->sysio);
	r = up->swipend;
	up->swipend = 0;
	up->syscall = 0;
	unlock(&up->sysio);

	/* Cleared by the signal/note/exception handler */
	while(up->intwait)
		osyield();

	if(r != 0)
		error(Eintr);

	strcpy(up->text, "");
}
