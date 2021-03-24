#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"ureg.h"
#include	"../port/error.h"
#include	"audit.h"

#define waslo(sr) (!((sr) & (PsrDirq|PsrDfiq)))

typedef struct IrqEntry {
		void	(*r)(Ureg*, void*);
		void	*a;
		int	v;
} IrqEntry;

enum {
	NumIRQbits = MaxIRQbit+1,

	MinGpioIRQbit = 11,
	NumGpioIRQbits = MaxGPIObit-MinGpioIRQbit+1,
	GpioIRQmask = ((1<<NumGpioIRQbits)-1)<<MinGpioIRQbit,
};

static IrqEntry Irq[NumIRQbits];
static IrqEntry GPIOIrq[NumGpioIRQbits];

Instr BREAK = 0xE6BAD010;

int (*breakhandler)(Ureg*, Proc*);
int (*catchdbg)(Ureg *, uint);

extern void (*serwrite)(char *, int);

void dumperrstk(void);
/*
 * Interrupt sources not masked by splhi() -- these are special
 *  interrupt handlers (e.g. profiler or watchdog), not allowed
 *  to share regular kernel data structures.  All interrupts are
 *  masked by splfhi(), which should only be used herein.
 */
enum {
	IRQ_NONMASK = ((1 << OSTimerbit(3)) | (1 << OSTimerbit(2))),
};
int splfhi(void);	/* disable all */
int splflo(void);	/* enable FIQ */

static int actIrq = -1;	/* Active Irq handler, 0-31, or -1 if none */
static int wasIrq = -1;	/* Interrupted Irq handler */

static Proc *iup;	/* Interrupted kproc */

void
intrenable(int v, void (*f)(Ureg*, void*), void* a, int tbdf)
{
	int x;

	switch(tbdf) {
	case BusGPIO:
		if(v < 0 || v > MaxGPIObit)
			panic("intrenable: gpio source %d out of range\n", v);
		if(v >= MinGpioIRQbit) {
			GPIOIrq[v-MinGpioIRQbit].r = f;
			GPIOIrq[v-MinGpioIRQbit].a = a;
			*GPDR &= ~(1<<v);
			return;
		}
		/*FALLTHROUGH for GPIO sources 0-10 */
	case BUSUNKNOWN:
	case BusCPU:
		if(v < 0 || v > MaxIRQbit)
			panic("intrenable: irq source %d out of range\n", v);
		Irq[v].r = f;
		Irq[v].a = a;

		x = splfhi();
		/* Enable the interrupt by setting the mask bit */
		INTRREG->icmr |= (1 << v);
		splx(x);
		break;
	default:
		panic("intrenable: unknown irq bus %d\n", tbdf);
	}
}

ulong fiqstack[4];
ulong irqstack[4];
ulong abtstack[4];
ulong undstack[4];

static void
safeintr(Ureg *, void *a)
{
	int v = (int)a;
	int x;

	/* No handler - clear the mask so we don't loop */
	x = splfhi();
	INTRREG->icmr &= ~(1 << v);
	splx(x);
	iprint("SPURIOUS INTERRUPT %d\n", v);
}

static void
safegpiointr(Ureg *, void *a)
{
	int i = (1 << (int)a);

	/* No handler - zap the edge detects so we don't loop */
	*GEDR = i;
	*GRER &= ~i;
	*GFER &= ~i;
	iprint("spurious GPIO intr %d\n", (int)a);
}

static void
gpiointr(Ureg *ur, void*)
{
	while (1) {
		IrqEntry *cur;
		ulong ibits;

		ibits = *GEDR & GpioIRQmask;
		if (!ibits)
			break;
		ibits <<= 31-MaxGPIObit;
		cur = GPIOIrq + NumGpioIRQbits - 1;
		/*
		 * Find highest-order bit set
		 */
		if (!(ibits & 0xff000000)) {
			cur -= 8;
			ibits <<= 8;
		}
		if (!(ibits & 0xf0000000)) {
			cur -= 4;
			ibits <<= 4;
		}
		while ((int)ibits > 0) {
			cur--;
			ibits <<= 1;
		}
		cur->r(ur, cur->a);
	}
}

static void
trapv(int off, void (*f)(void))
{
	ulong *vloc;
	int offset;

	if (conf.remaplo)
		off += ALT_IVEC;
	vloc = (ulong *)off;
	offset = (((ulong *) f) - vloc)-2;
	*vloc = (0xea << 24) | offset;
}

void
trapinit(void)
{
	int v;
	IntrReg *intr = INTRREG;

	intr->icmr = 0;
	intr->iclr = IRQ_NONMASK;

	/* set up stacks for various exceptions */
	setr13(PsrMfiq, fiqstack+nelem(fiqstack));
	setr13(PsrMirq, irqstack+nelem(irqstack));
	setr13(PsrMabt, abtstack+nelem(abtstack));
	setr13(PsrMund, undstack+nelem(undstack));

	for (v = 0; v < nelem(Irq); v++) {
		Irq[v].r = safeintr;
		Irq[v].a = (void *)v;
		Irq[v].v = v;
	}
	for (v = 0; v < nelem(GPIOIrq); v++) {
		GPIOIrq[v].r = safegpiointr;
		GPIOIrq[v].a = (void *)(v+MinGpioIRQbit);
		GPIOIrq[v].v = v+MinGpioIRQbit;
	}

	mmuregw(CpDAC, 3);	/* 'manager' has write permissions */
	if (conf.remaplo)
		remaplomem();	/* remap memory for use of ALT_IVEC */

	trapv(0x0, _vsvccall);
	trapv(0x4, _vundcall);
	trapv(0xc, _vpabcall);
	trapv(0x10, _vdabcall);
	trapv(0x18, _virqcall);
	trapv(0x1c, _vfiqcall);
	trapv(0x8, _vsvccall);

	/* Turn on system mode and alternate ivec first */
	if (conf.remaplo)
		mmuctlregw(mmuctlregr() | CpCsystem | CpCaltivec);
	else
		mmuctlregw((mmuctlregr() | CpCsystem) & ~CpCaltivec);

	/* Then change domain control to 'client' */
	mmuregw(CpDAC, 1);

	/* paranoia? */
	flushIcache();
	writeBackDC();
	flushDcache();
	flushIcache();
	drainWBuffer();

	intrenable(MinGpioIRQbit, gpiointr, nil, BusCPU);
}

static char *_trap_str[PsrMask+1] = {
	[ PsrMfiq ] "Fiq interrupt",
	[ PsrMirq ] "Mirq interrupt",
	[ PsrMsvc ] "SVC/SWI Exception",
	[ PsrMabt ] "Prefetch Abort/Data Abort",
	[ PsrMabt+1 ] "Data Abort",
	[ PsrMund ] "Undefined instruction",
	[ PsrMsys ] "Sys trap"
};

static char *
trap_str(int psr)
{
	char *str = _trap_str[psr & PsrMask];
	if (!str)
		str = "Undefined trap";
	return(str);
}

static void
sys_trap_error(int type)
{
	char errbuf[ERRLEN];
	sprint(errbuf, "sys: trap: %s\n", trap_str(type));
	error(errbuf);
}

void
dflt(Ureg *ureg, ulong far)
{
	char buf[ERRLEN];

	dumpregs(ureg);
	sprint(buf, "trap: fault pc=%N addr=0x%lux", (ulong)ureg->pc, far);
	disfault(ureg, buf);
}

/*
 *  All traps come here.  It is slower to have all traps call trap()
 *  rather than directly vectoring the handler.
 *  However, this avoids
 *  a lot of code dup and possible bugs.
 *  trap is called splfhi().
 */

void
trap(Ureg* ureg)
{
	 //
	 // This is here to make sure that a clock interrupt doesn't
	 // cause the process we just returned into to get scheduled
	 // before it single stepped to the next instruction.
	 //
	static struct {int callsched;} c = {1};
	int itype;
	/*
	 * All interrupts/exceptions should be resumed at ureg->pc-4,
	 * except for Data Abort which resumes at ureg->pc-8.
	 */

	ureg->pc -= 4;
	ureg->sp = (ulong)(ureg+1);
	itype = ureg->type;
	if (itype == PsrMirq || itype == PsrMfiq) {	/* Interrupt Request */

		Proc *saveup;
		int t;

		SET(t);
		SET(saveup);

		if (itype == PsrMirq) {
			splflo();	/* Allow nonmasked interrupts */
			if (saveup = up) {
				t = m->ticks;	/* CPU time per proc */
				saveup->pc = ureg->pc;	/* debug info */
				saveup->dbgreg = ureg;
			}
		} else {
					 /* for profiler(wasbusy()): */
			wasIrq = actIrq; /* Save ID of interrupted handler */
			iup = up;	 /* Save ID of interrupted proc */
		}

		while (1) {		/* Use up all the active interrupts */
			ulong ibits;
			IrqEntry *curIrq;
			IntrReg *intr = INTRREG;

			if (itype == PsrMirq)
				ibits = intr->icip;	/* screened by icmr */
			else
				ibits = intr->icfp;	/* screened by icmr */
			if (!ibits)
				break;
			/*
			 * Find highest-order bit set
			 */
			curIrq = Irq + NumIRQbits - 1;
			if (!(ibits & 0xffff0000)) {
				curIrq -= 16;
				ibits <<= 16;
			}
			if (!(ibits & 0xff000000)) {
				curIrq -= 8;
				ibits <<= 8;
			}
			if (!(ibits & 0xf0000000)) {
				curIrq -= 4;
				ibits <<= 4;
			}
			/* Loop until high bit set */
			while ((int)ibits > 0) {
				curIrq--;
				ibits <<= 1;
			}
			actIrq = curIrq->v; /* show active interrupt handler */
			up = 0;		/* Make interrupted process invisible */
			curIrq->r(ureg, curIrq->a);	/* Call handler */
		}
		if (itype == PsrMirq) {
			up = saveup;	/* Make interrupted process visible */
			actIrq = -1;	/* No more interrupt handler running */
			if (saveup) {
				if (saveup->state == Running) {
					if (strcmp(saveup->kstack+500, "stackmark") != 0) {
						setpanic();
						dumpregs(ureg);
						panic("Stack overflow");
					}
					t = m->ticks - t;	/* See if timer advanced */
					if ((rdypri < saveup->pri) || (t && (rdypri == saveup->pri))) {
						if(c.callsched)
							sched();
					}
				}
				saveup->dbgreg = nil;
			}
		} else {
			actIrq = wasIrq;
			up = iup;
		}
		return;
	}

	/* All other traps */

	if (ureg->psr & PsrDfiq)
		goto faultpanic;
	if (up)
		up->dbgreg = ureg;
	switch(itype) {

	case PsrMund:				/* Undefined instruction */
		if(*(ulong*)ureg->pc == BREAK && breakhandler) {
			int s;
			Proc *p;

			p = up;
			/* if (!waslo(ureg->psr) || (ureg->pc >= (ulong)splhi && ureg->pc < (ulong)islo))
				p = 0; */
			s = breakhandler(ureg, p);
			if(s == BrkSched) {
				c.callsched = 1;
				sched();
			} else if(s == BrkNoSched) {
				c.callsched = 0;
				if(up)
					up->dbgreg = 0;
				return;
			}
			break;
		}
		if (!up)
			goto faultpanic;
		spllo();
		if (waserror()) {
			if(waslo(ureg->psr) && (up->type == Interp))
				disfault(ureg, up->env->error);
			setpanic();
			dumpregs(ureg);
			panic("%s", up->env->error);
		}
		if (!fpiarm(ureg)) {
			dumpregs(ureg);
			sys_trap_error(ureg->type);
		}
		poperror();
		break;

	case PsrMsvc:				/* Jump through 0 or SWI */
		if (waslo(ureg->psr) && up && (up->type == Interp)) {
			spllo();
			dumpregs(ureg);
			sys_trap_error(ureg->type);
		}
		goto faultpanic;

	case PsrMabt:				/* Prefetch abort */
		if (catchdbg && catchdbg(ureg, 0))
			break;
		ureg->pc -= 4;

	case PsrMabt+1:	{			/* Data abort */
		uint far;
		uint fsr;

		fsr = mmuregr(CpFSR);
		far = mmuregr(CpFAR);
		if (fsr & (1<<9)) {
			mmuregw(CpFSR, fsr & ~(1<<9));
			if (catchdbg && catchdbg(ureg, fsr))
				break;
			print("Debug/");
		}
		if (waslo(ureg->psr) && up && (up->type == Interp)) {
			spllo();
			dflt(ureg, far);
		}
		goto faultpanic;
	}
	default:				/* ??? */
faultpanic:
		setpanic();
		dumpregs(ureg);
		panic("exception %uX %s\n", ureg->type, trap_str(ureg->type));
		break;
	}

	splhi();
	if(up)
		up->dbgreg = 0;		/* becomes invalid after return from trap */
}

static void
serputc(int c)
{
	if (!c)
		return;
	if (c == '\n')
		serputc('\r');
	while(!(*(ulong*)0x80050020 & 0x04))
		;
	*(ulong*)0x80050014 = c;
	if (c == '\n')
		while((*(ulong*)0x80050020 & 0x01))	/* flush xmit fifo */
			;
}

static void
_serwrite(char *data, int len)
{
	int x;

	clockpoll();
	x = splfhi();
	while (len--) {
		serputc(*data++);
	}
	splx(x);
}

static int
_serread(char *data, int len)
{
	clockcheck();
	while(len--) {
		while(!(*(ulong*)0x80050020 & 0x02))
			clockcheck();
		*data++ = *(ulong*)0x80050014;
	}
	return 0;
}

int
iprint(char *fmt, ...)
{
	int n;
	va_list arg;
	char buf[PRINTSIZE];

	va_start(arg, fmt);
	n = doprint(buf, buf+sizeof(buf), fmt, arg) - buf;
	va_end(arg);
	_serwrite(buf, n);

	return n;
}

void
setpanic(void)
{
	extern void screenon(int);
	extern int consoleprint;

	if (breakhandler != 0)	/* don't mess up debugger */
		return;
	INTRREG->icmr = 0;
	spllo();
	screenon(!consoleprint);
	consoleprint = 1;
	serwrite = _serwrite;
}

int
isvalid_pc(ulong v)
{
	extern char etext[];
	extern void _startup(void);

	return(((ulong)_startup <= v && v < (ulong)etext) && !(v & 3));
}

int
isvalid_wa(void *v)
{
	return((ulong)v >= 0x8000 && (ulong)v < conf.topofmem && !((ulong)v & 3));
}

int
isvalid_va(void *v)
{
	return((ulong)v >= 0x8000 && (ulong)v < conf.topofmem);
}

void
dumplongs(char *msg, ulong *v, int n)
{
	int	ii;
	int	ll;

	ll = print("%s at %ulx: ", msg, v);
	for (ii = 0; ii < n; ii++)
	{
		if (ll >= 60)
		{
			print("\n");
			ll = print("    %ulx: ", v);
		}
		if (isvalid_va(v))
			ll += print(" %ulx", *v++);
		else
		{
			ll += print(" invalid");
			break;
		}
	}
	print("\n");
	USED(ll);
}

void
dumpregs(Ureg* ureg)
{
	Proc *p;

	print("TRAP: %s", trap_str(ureg->type));
	if ((ureg->psr & PsrMask) != PsrMsvc)
		print(" in %s", trap_str(ureg->psr));
	if ((ureg->type == PsrMabt) || (ureg->type == PsrMabt + 1))
		print(" FSR %8.8luX FAR %8.8luX\n", mmuregr(CpFSR), mmuregr(CpFAR));
	print("\n");
	print("PSR %8.8uX type %2.2uX PC %8.8uX LINK %8.8uX\n",
		ureg->psr, ureg->type, ureg->pc, ureg->link);
	print("R14 %8.8uX R13 %8.8uX R12 %8.8uX R11 %8.8uX R10 %8.8uX\n",
		ureg->r14, ureg->r13, ureg->r12, ureg->r11, ureg->r10);
	print("R9  %8.8uX R8  %8.8uX R7  %8.8uX R6  %8.8uX R5  %8.8uX\n",
		ureg->r9, ureg->r8, ureg->r7, ureg->r6, ureg->r5);
	print("R4  %8.8uX R3  %8.8uX R2  %8.8uX R1  %8.8uX R0  %8.8uX\n",
		ureg->r4, ureg->r3, ureg->r2, ureg->r1, ureg->r0);
	print("Stack is at: %8.8luX\n",ureg);
	print("CPSR %8.8uX SPSR %8.8uX ", cpsrr(), spsrr());
	print("PC %N LINK %N\n", (ulong)ureg->pc, (ulong)ureg->link);

	p = (actIrq >= 0) ? iup : up;
	if (p != nil)
		print("Process stack:  %lux-%lux\n",
			p->kstack, p->kstack+KSTACK-4);
	else
		print("System stack: %lux-%lux\n",
			(ulong)(m+1), (ulong)m+KSTACK-4);
	dumplongs("stk", (ulong *)(ureg + 1), 16);
	print("bl's: ");
	dumpstk((ulong *)(ureg + 1));
	if (isvalid_wa((void *)ureg->pc))
		dumplongs("code", (ulong *)ureg->pc - 5, 12);

#define AUDIT(s,r) if (isvalid_va((void *)ureg->r)) auditmemloc(s, (void *)ureg->r);
	if (auditmemloc) {
		AUDIT("R0", r0);
		AUDIT("R1", r1);
		AUDIT("R2", r2);
		AUDIT("R3", r3);
		AUDIT("R4", r4);
		AUDIT("R5", r5);
		AUDIT("R6", r6);
		AUDIT("R7", r7);
		AUDIT("R8", r8);
		AUDIT("R9", r9);
		AUDIT("R10", r10);
		AUDIT("R11", r11);
		AUDIT("SP", sp);
	}
	dumperrstk();
}

void
dumpstack(void)
{
	ulong l;

	if (breakhandler != 0)
		dumpstk(&l);
}

void
dumpstk(ulong *l)
{
	ulong *v, i;
	ulong inst;
	ulong *estk;
	uint len;

	len = KSTACK/sizeof *l;
	if (up == 0)
		len -= l - (ulong *)m;
	else
		len -= l - (ulong *)up->kstack;

	if (len > KSTACK/sizeof *l)
		len = KSTACK/sizeof *l;
	else if (len < 0)
		len = 50;

	i = 0;
	for(estk = l + len; l<estk; l++) {
		if (!isvalid_wa(l)) {
			i += print("invalid(%lux)", l);
			break;
		}
		v = (ulong *)*l;
		if (isvalid_wa(v)) {
			inst = *(v - 1);
			if (	(
					((inst & 0x0ff0f000) == 0x0280f000)
					&&
					((*(v-2) & 0x0ffff000) == 0x028fe000)
				)
				||
				((inst & 0x0f000000) == 0x0b000000)
			) {
				i += print("%N ", v);
			}
		}
		if (i >= 60) {
			print("\n");
			i = print("    ");
		}
	}
	if (i)
		print("\n");
}

void
dumperrstk(void)
{
	int ii, ll;

	if (!up)
		return;

	ll = print("err stk: ");
	for (ii = 0; ii < NERR; ii++) {
		if (ii == up->nerrlab)
			ll += print("* ");
		if (up->errlab[ii].pc) {
			ll += print(" %lux/%N",
				up->errlab[ii].sp, up->errlab[ii].pc);
			if (ll >= 60) {
				print("\n");
				ll = 0;
			}
		}
	}
	if (ll)
		print("\n");
}

void
trapspecial(int (*f)(Ureg *, uint))
{
	catchdbg = f;
}

int
wasbusy(int idlepri)
{
	return	wasIrq >= 0
		||
		(nrdy > 0 && rdypri < idlepri)
		||
		(iup && iup->type != IdleGC && iup->pri < idlepri)
		||
		idlepri > Nrq;
}
