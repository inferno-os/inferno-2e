#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "portfns.h"
#include "ureg.h"
#include "../port/error.h"

//
// These bits used to be in port/devdbg but were removed in
// order to allow for using hardware debug features on certain
// architectures
//

extern void breakset(Brk *b);
extern void breakrestore(Brk *b);
extern Brk* breakclear(int id);
extern void breaknotify(Brk *b, Proc *p);
extern int breakmatch(BrkCond *cond, Ureg *ur, Proc *p);

void	skipfree(Brk *b);
Brk*newskip(ulong addr, Brk *skipb, Proc *skipp);
Brk *skipalloc;
extern Brk	*breakpoints;
typedef struct SkipArg SkipArg;
struct SkipArg
{
	Brk *b;
	Proc *p;
};

 void
skiphandler(Brk *b)
{
	SkipArg *a = b->aux;
	Brk *l;

	if(breakclear(b->id) == nil)
		panic("skiphandler: breakclear() failed");
	breakrestore(a->b);
	l = a->b->link;
	while(l != nil) {
		breakrestore(l);
		l = l->link;
	}
	skipfree(b);
	a->p->dbgstop = 0;		// Whoo!
}

 Brk*
newskip(ulong addr, Brk *skipb, Proc *skipp)
{
	Brk *b;
	SkipArg *a;

	b = skipalloc;
	if(b == nil)
		panic("newskip(): no free skips\n");
	skipalloc = b->next;

	b->addr = addr;
	b->conditions->val = addr;
	b->link = nil;
	a = b->aux;
	a->b = skipb;
	a->p = skipp;

	return b;
}

 void
skipfree(Brk *b)
{
	b->next = skipalloc;
	skipalloc = b;
}

//
// Called from the exception handler when a breakpoint instruction has been
// hit.  This cannot not be called unless at least one breakpoint with this
// address is in the list of breakpoints.  (All breakpoint notifications must
// previously have been set via setbreak())
//
//	foreach breakpoint in list
//		if breakpoint matches conditions
//			notify the break handler
//	if no breakpoints matched the conditions
//		pick a random breakpoint set to this address
//
//		set a breakpoint at the next instruction to be executed,
//		and pass the current breakpoint to the "skiphandler"
//
//		clear the current breakpoint
//
//		Tell the scheduler to stop scheduling, so the caller is
//		guaranteed to execute the instruction, followed by the
//		added breakpoint.
//
//
int
breakhit(Ureg *ur, Proc *p)
{
	Brk *b;
	int nmatched;
	Brk *skip;

	nmatched = 0;
	for(b = breakpoints; b != nil; b = b->next) {
		if(breakmatch(b->conditions, ur, p)) {
			breaknotify(b, p);
			++nmatched;
		}
	}

	if(nmatched)
		return BrkSched;

	skip = nil;
	for(b = breakpoints; b != nil;  b = b->next) {
		if(b->addr == ur->pc) {
			if(breakclear(b->id) == nil)
				panic("breakhit: breakclear() failed");

			if(skip == nil)
				skip = newskip(machnextaddr(ur), b, p);
			else {
				b->link = skip->link;
				skip->link = b;
			}
		}
	}
	if(skip == nil)
		return BrkSched;
	breakset(skip);
	return BrkNoSched;
}

void
portbreakinit(void)
{
	Brk *b;
	int i;

	skipalloc = mallocz(conf.nproc*(sizeof(Brk)+sizeof(BrkCond)+sizeof(SkipArg)), 1);
	if(skipalloc == nil)
		error(Enomem);

	b = skipalloc;
	for(i=0; i < conf.nproc-1; i++) {
		b->id = -(i+1);
		b->conditions = (BrkCond*)((uchar*)b + sizeof(Brk));
		b->conditions->op = 'b';
		b->handler = skiphandler;
		b->aux = (SkipArg*)((uchar*)b+sizeof(Brk)+sizeof(BrkCond));
		b->next = (Brk*)((uchar*)b+sizeof(Brk)+sizeof(BrkCond)+sizeof(SkipArg));
		b = b->next;
	}
	b->next = nil;
}
