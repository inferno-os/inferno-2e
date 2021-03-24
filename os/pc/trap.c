#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"ureg.h"
#include	"../port/error.h"

#define	DISTRAP	1

void	noted(Ureg*, ulong);

static void debugbpt(Ureg*, void*);
static void fault386(Ureg*, void*);

static Lock irqctllock;
static Irqctl *irqctl[256];

int (*breakhandler)(Ureg *ur, Proc*);

void
intrenable(int v, void (*f)(Ureg*, void*), void* a, int tbdf)
{
	Irq *irq;
	Irqctl *ctl;

	lock(&irqctllock);
	if(irqctl[v] == 0)
		irqctl[v] = xalloc(sizeof(Irqctl));
	ctl = irqctl[v];

	if(v >= VectorLAPIC && arch->intrenable(v, tbdf, ctl) == -1){
		if(ctl->irq == nil){
			irqctl[v] = nil;
			xfree(ctl);
		}
		unlock(&irqctllock);
		print("intrenable: didn't find v %d, tbdf 0x%uX\n", v, tbdf);
		return;
	}

	irq = xalloc(sizeof(Irq));
	irq->f = f;
	irq->a = a;
	irq->next = ctl->irq;
	ctl->irq = irq;
	unlock(&irqctllock);
}

void
trapinit(void)
{
	int v, pri;
	ulong vaddr;
	Segdesc *idt;

	idt = (Segdesc*)IDTADDR;
	vaddr = (ulong)vectortable;
	for(v = 0; v < 256; v++){
		if(v == VectorBPT)
			pri = 3;
		else
			pri = 0;
		idt[v].d0 = (vaddr & 0xFFFF)|(KESEL<<16);
		idt[v].d1 = (vaddr & 0xFFFF0000)|SEGP|SEGPL(pri)|SEGIG;
		vaddr += 6;
	}

	intrenable(VectorBPT, debugbpt, 0, BUSUNKNOWN);
	intrenable(VectorPF, fault386, 0, BUSUNKNOWN);
}

char *excname[] = {
	[0]	"divide error",
	[1]	"debug exception",
	[2]	"nonmaskable interrupt",
	[3]	"breakpoint",
	[4]	"overflow",
	[5]	"bounds check",
	[6]	"invalid opcode",
	[7]	"coprocessor not available",
	[8]	"double fault",
	[9]	"coprocessor segment overrun",
	[10]	"invalid TSS",
	[11]	"segment not present",
	[12]	"stack exception",
	[13]	"general protection violation",
	[14]	"page fault",
	[15]	"15 (reserved)",
	[16]	"coprocessor error",
	[17]	"alignment check",
	[18]	"machine check",
};

static int nspuriousintr;

/*
 *  All traps come here.  It is slower to have all traps
 *  call trap() rather than directly vectoring the handler.
 *  However, this avoids a lot of code duplication and possible
 *  bugs.  trap is called splhi().
 */
void
trap(Ureg* ureg)
{
	Irq *irq;
	int v;
	Irqctl *ctl;
	char buf[ERRLEN];

	if(up) {
		/*
		 * Compute actual sp at time of trap. The SP is
		 * saved last when the Ureg is built on the stack.
		 * so, must add the entries in the Ureg alreadly
		 * pushed.
		 */
		ureg->sp = ureg->sp + sizeof(Ureg) - sizeof(ulong);
		up->dbgreg = ureg;
	}

	v = ureg->trap;

	if(ctl = irqctl[v]){
		if(ctl->isr)
			ctl->isr(v);

		for(irq = ctl->irq; irq; irq = irq->next)
			irq->f(ureg, irq->a);

		if(ctl->eoi)
			ctl->eoi(v);
	}
	else if(v <= 16 && up->type == Interp){
		sprint(buf, "sys: trap: %s", excname[v]);
		error(buf);
	}
	else if(v >= VectorPIC && v <= MaxVectorPIC){
		/*
		 * An unknown interrupt.
		 * Check for a default IRQ7. This can happen when
		 * the IRQ input goes away before the acknowledge.
		 * In this case, a 'default IRQ7' is generated, but
		 * the corresponding bit in the ISR isn't set.
		 * In fact, just ignore all such interrupts.
		 */
		nspuriousintr++;
		print("%d: spurious interrupt %d\n", nspuriousintr, v-VectorPIC);
		return;
	}
	else {
		dumpregs(ureg);
		if(v < nelem(excname))
			panic("%s", excname[v]);
		panic("unknown trap/intr: %d\n", v);
	}

	if(up && up->state == Running && rdypri < up->pri)
		sched();

	if (up)
		up->dbgreg = 0;
	splhi();
}

/*
 *  dump registers
 */
void
dumpregs2(Ureg* ureg)
{
	ureg->cs &= 0xFFFF;
	ureg->ds &= 0xFFFF;
	ureg->es &= 0xFFFF;
	ureg->fs &= 0xFFFF;
	ureg->gs &= 0xFFFF;

	if(up)
		print("cpu%d: registers for %s %d\n", m->machno, up->text, up->pid);
	else
		print("cpu%d: registers for kernel\n", m->machno);

	print("FLAGS=%luX TRAP=%luX ECODE=%luX PC=%luX", ureg->flags, ureg->trap,
		ureg->ecode, ureg->pc);
	print(" SS=%4.4luX USP=%luX\n", ureg->ss & 0xFFFF, ureg->usp);
	print("  AX %8.8luX  BX %8.8luX  CX %8.8luX  DX %8.8luX\n",
		ureg->ax, ureg->bx, ureg->cx, ureg->dx);
	print("  SI %8.8luX  DI %8.8luX  BP %8.8luX\n",
		ureg->si, ureg->di, ureg->bp);
	print("  CS %4.4uX  DS %4.4uX  ES %4.4uX  FS %4.4uX  GS %4.4uX\n",
		ureg->cs, ureg->ds, ureg->es, ureg->fs, ureg->gs);
}

void
dumpregs(Ureg* ureg)
{
	extern ulong etext;
	ulong mca[2], mct[2];

	dumpregs2(ureg);

	/*
	 * Processor control registers.
	 * If machine check exception, time stamp counter, page size extensions or
	 * enhanced virtual 8086 mode extensions are supported, there is a CR4.
	 * If there is a CR4 and machine check extensions, read the machine check
	 * address and machine check type registers if RDMSR supported.
	 */
	print("  CR0 %8.8lux CR2 %8.8lux CR3 %8.8lux", getcr0(), getcr2(), getcr3());
	if(m->cpuiddx & 0x9A){
		print(" CR4 %8.8luX", getcr4());
		if((m->cpuiddx & 0xA0) == 0xA0){
			rdmsr(0x00, &mca[1], &mca[0]);
			rdmsr(0x01, &mct[1], &mct[0]);
			print("\n  MCA %8.8luX:%8.8luX MCT %8.8luX",
				mca[1], mca[0], mct[0]);
		}
	}
	print("\n  ur %luX up %luX\n", ureg, up);
}

int
isvalid_pc(ulong v)
{
	extern char etext[];
	extern void _start0x00100020(void);

	return(((ulong)_start0x00100020 <= v && v < (ulong)etext));
}

void
dumpstack(void)
{
	ulong l, v, i;
	uchar *p;
	extern ulong etext;

	if(up == 0)
		return;

	i = 0;
	for(l=(ulong)&l; l<(ulong)(up->kstack+KSTACK); l+=4){
		v = *(ulong*)l;
		if(KTZERO < v && v < (ulong)&etext){
			p = (uchar*)v;
			if(*(p-5) == 0xE8){
				print("%lux ", p-5);
				i++;
			}
		}
		if(i == 8){
			i = 0;
			print("\n");
		}
	}
}

static void
debugbpt(Ureg* ureg, void*)
{
	
	if (breakhandler) {
		breakhandler(ureg, up);
	} else {
		char buf[ERRLEN];
	
		if(up == 0)
			panic("kernel bpt");
		/* restore pc to instruction that caused the trap */
		ureg->pc--;
		sprint(buf, "sys: breakpoint");
		error(buf);
	}
}

#include <isa.h>
#include <interp.h>
#include <kernel.h>

Type *DESTROY;		// debug

static void
fault386(Ureg* ureg, void*)
{
	int read;
	ulong addr;
	char buf[ERRLEN];

	up->dbgreg = ureg;		/* For remote ACID */

	addr = getcr2();
	read = !(ureg->ecode & 2);
	spllo();
	sprint(buf, "trap: fault %s pc=0x%lux addr=0x%lux",
			read ? "read" : "write", ureg->pc, addr);

	if(DISTRAP && up->type == Interp)
		disfault(ureg, buf);

	dumpregs(ureg);
	dumpstack();
for(;;);
	panic("fault: %s\n", buf);
}

static void
linkproc(void)
{
	spllo();
	up->kpfun(up->arg);
}

void
kprocchild(Proc* p, void (*func)(void*), void* arg)
{
	p->sched.pc = (ulong)linkproc;
	p->sched.sp = (ulong)p->kstack+KSTACK;

	p->kpfun = func;
	p->arg = arg;
}
