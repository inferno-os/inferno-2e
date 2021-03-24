#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "io.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "version.h"

Mach *m = (Mach*)MACHADDR;
Proc *up = 0;
Conf conf;

extern ulong kerndate;
extern int cflag;
extern int consoleprint;
extern int redirectconsole;
extern int main_pool_pcnt;
extern int heap_pool_pcnt;
extern int image_pool_pcnt;
extern int kernel_pool_pcnt;

int
segflush(void *p, ulong l)
{
	int x;
	USED(p, l);
	x = splhi();
	writeBackDC();	
	flushDcache();
	drainWBuffer();
	flushIcache();
	splx(x);
	return 1;
}

static void
poolsizeinit(void)
{
	ulong nb;

	nb = conf.npage*BY2PG;
	poolsize(mainmem, (nb*main_pool_pcnt)/100, 0);
	poolsize(heapmem, (nb*heap_pool_pcnt)/100, 0);
	poolsize(imagmem, (nb*image_pool_pcnt)/100, 1);
}

void
reboot(void)
{
	exit(0);
}

void
halt(void)
{
	spllo();
	print("cpu halted\n");
	while(1);
}

void
confinit(void)
{
	ulong base;

	archconfinit();

	base = PGROUND((ulong)end);
	conf.base0 = base;

	conf.base1 = 0;
	conf.npage1 = 0;

	conf.npage0 = (conf.topofmem - base)/BY2PG;

	conf.npage = conf.npage0 + conf.npage1;
	conf.ialloc = (((conf.npage*(main_pool_pcnt))/100)/2)*BY2PG;


	conf.nproc = 100 + ((conf.npage*BY2PG)/MB)*5;
	conf.nmach = 1;
	conf.interps = 5;

	strcpy(eve, "inferno");
}

void
machinit(void)
{
	memset(m, 0, sizeof(Mach));	/* clear the mach struct */
}

void
redirect(void)
{
	int speed, secs;

	speed = 38400;
	secs = 10;
	bpoverride("redirectspeed", &speed);
	bpoverride("redirectdelay", &secs);
	print("Pausing %d seconds...\n", secs);
	print("Please switch serial line to terminal...\n");
	delay(secs*1000);
	print("Redirecting console (%dbps)...\n", speed); /* */
	uartspecial(0, speed, 'n', &kbdq, &printq, kbdcr2nl);
}

void
main(void)
{
	memset(edata, 0, end-edata);		/* clear the BSS */

	machinit();
	archreset();
	confinit();
	links();
	xinit();
	poolinit();
	poolsizeinit();
	trapinit(); 
	mmuctlregw(mmuctlregr() | CpCDcache | CpCwb | CpCi32 | CpCd32 | CpCIcache);
	clockinit(); 
	printinit();
	screeninit();
	procinit();
	chandevreset(0);

	if (redirectconsole)
		redirect();
//	else
		kbdinit();

	print("\nInferno Network Operating System\n");
	print("%s-%s (%lud)\n\n",VERSION, conffile, kerndate);
	print("JIT Compilation Mode = %d\n",cflag);

	userinit();
	schedinit();
}

void
init0(void)
{
	Osenv *o;

	up->nerrlab = 0;

	spllo();

	if(waserror())
		panic("init0 %r");
	/*
	 * These are o.k. because rootinit is null.
	 * Then early kproc's will have a root and dot.
	 */
	o = up->env;
	o->pgrp->slash = namec("#/", Atodir, 0, 0);
	o->pgrp->dot = cclone(o->pgrp->slash, 0);

	chandevinit();
	poperror();

	disinit("/osinit.dis");
}

void
userinit()
{
	Proc *p;
	Osenv *o;

	p = newproc();
	o = p->env;

	o->fgrp = newfgrp();

	o->pgrp = newpgrp();
	strcpy(o->user, eve);

	strcpy(p->text, "interp");

	p->fpstate = FPINIT;

	/*
	 * Kernel Stack
	 *
	 * N.B. The -12 for the stack pointer is important.
	 *	4 bytes for gotolabel's return PC
	 */
	p->sched.pc = (ulong)init0;
	p->sched.sp = (ulong)p->kstack+KSTACK-8;

	ready(p);
}

void
exit(int inpanic)
{
	up = 0;

	/* Shutdown running devices */
	chandevreset(1);

	if(inpanic){
		print("Hit the reset button\n");
		for(;;)clockpoll();
	}
	archreboot();
}

static void
linkproc(void)
{
	spllo();
	if (waserror())
		print("error() underflow: %r\n");
	else
		(*up->kpfun)(up->arg);
	pexit("end proc", 1);
}

void
kprocchild(Proc *p, void (*func)(void*), void *arg)
{
	p->sched.pc = (ulong)linkproc;
	p->sched.sp = (ulong)p->kstack+KSTACK-8;

	p->kpfun = func;
	p->arg = arg;
}

/* stubs */
void
setfsr(ulong x) {
USED(x);
}

ulong
getfsr(){
return 0;
}

void
setfcr(ulong x) {
USED(x);
}

ulong
getfcr(){
return 0;
}

void
fpinit(void)
{
}

void
FPsave(void*)
{
}

void
FPrestore(void*)
{
}

