typedef struct Conf	Conf;
typedef struct FPU	FPU;
typedef struct FPenv	FPenv;
typedef struct Irq	Irq;
typedef struct Irqctl	Irqctl;
typedef struct ISAConf	ISAConf;
typedef struct Label	Label;
typedef struct Lock	Lock;
typedef struct MMU	MMU;
typedef struct Mach	Mach;
typedef struct Notsave	Notsave;
typedef struct PCArch	PCArch;
typedef struct Pcidev	Pcidev;
typedef struct PCMmap	PCMmap;
typedef struct Page	Page;
typedef struct PMMU	PMMU;
typedef struct Segdesc	Segdesc;
typedef struct Target Target;
typedef struct Ureg	Ureg;
typedef ulong Instr;

struct Lock
{
	ulong	key;
	ulong	sr;
	ulong	pc;
	ulong	pri;
};

struct Label
{
	ulong	sp;
	ulong	pc;
};

/*
 * FPenv.status
 */
enum
{
	FPINIT,
	FPACTIVE,
	FPINACTIVE,
};

/*
 * This structure must agree with FPsave and FPrestore asm routines
 */
struct FPenv
{
	ushort	control;
	ushort	r1;
	ushort	status;
	ushort	r2;
	ushort	tag;
	ushort	r3;
	ulong	pc;
	ushort	selector;
	ushort	r4;
	ulong	operand;
	ushort	oselector;
	ushort	r5;
};

/*
 * This structure must agree with fpsave and fprestore asm routines
 */
struct	FPU
{
	FPenv	env;
	uchar	regs[80];	/* floating point registers */
};

struct Conf
{
	ulong	nmach;		/* processors */
	ulong	nproc;		/* processes */
	ulong	monitor;	/* has monitor? */
	ulong	npage0;		/* total physical pages of memory */
	ulong	npage1;		/* total physical pages of memory */
	ulong	npage;		/* total physical pages of memory */
	ulong	base0;		/* base of bank 0 */
	ulong	base1;		/* base of bank 1 */
	ulong	ialloc;		/* max interrupt time allocation in bytes */
	ulong	interps;	/* number of interpreter processes */
};

#include "../port/portdat.h"

typedef struct {
	ulong	link;			/* link (old TSS selector) */
	ulong	esp0;			/* privilege level 0 stack pointer */
	ulong	ss0;			/* privilege level 0 stack selector */
	ulong	esp1;			/* privilege level 1 stack pointer */
	ulong	ss1;			/* privilege level 1 stack selector */
	ulong	esp2;			/* privilege level 2 stack pointer */
	ulong	ss2;			/* privilege level 2 stack selector */
	ulong	cr3;			/* page directory base register */
	ulong	eip;			/* instruction pointer */
	ulong	eflags;			/* flags register */
	ulong	eax;			/* general registers */
	ulong 	ecx;
	ulong	edx;
	ulong	ebx;
	ulong	esp;
	ulong	ebp;
	ulong	esi;
	ulong	edi;
	ulong	es;			/* segment selectors */
	ulong	cs;
	ulong	ss;
	ulong	ds;
	ulong	fs;
	ulong	gs;
	ulong	ldt;			/* selector for task's LDT */
	ulong	iomap;			/* I/O map base address + T-bit */
} Tss;

struct Segdesc
{
	ulong	d0;
	ulong	d1;
};

struct Mach
{
	int	machno;		/* physical id of processor (currently unused) */
	ulong	splpc;		/* pc of last caller to splhi */

	ulong*	pdb;		/* page directory base for this processor (va) */
	Tss*	tss;		/* tss for this processor */
	Segdesc	gdt[6];		/* gdt for this processor */

	Proc*	externup;	/* extern register Proc *up */

	ulong	ticks;		/* of the clock since boot time */
	Proc*	proc;		/* current process on this processor */
	Label	sched;		/* scheduler wakeup */
	Lock	alarmlock;	/* access to alarm list */
	void*	alarm;		/* alarms bound to this clock */

	int	nrdy;

	int	loopconst;

	int	cpumhz;
	int	cpuidax;
	int	cpuiddx;
	char	cpuidid[16];
	char*	cpuidtype;

	int	stack[1];
};

/*
 *  routines for things outside the PC model, like power management
 */
struct PCArch
{
	char*	id;
	int	(*ident)(void);		/* this should be in the model */
	void	(*reset)(void);		/* this should be in the model */
	int	(*serialpower)(int);	/* 1 == on, 0 == off */
	int	(*modempower)(int);	/* 1 == on, 0 == off */

	void	(*intrinit)(void);
	int	(*intrenable)(int, int, Irqctl*);

	void	(*clockenable)(void);
};

/*
 *  a parsed plan9.ini line
 */
#define ISAOPTLEN	16
#define NISAOPT		8

struct ISAConf {
	char	type[NAMELEN];
	ulong	port;
	ulong	irq;
	ulong	dma;
	ulong	mem;
	ulong	size;
	ulong	freq;

	int	nopt;
	char	opt[NISAOPT][ISAOPTLEN];
};

extern PCArch	*arch;			/* PC architecture */

/*
 * Each processor sees its own Mach structure at address MACHADDR.
 * However, the Mach structures must also be available via the per-processor
 * MMU information array machp, mainly for disambiguation and access to
 * the clock which is only maintained by the bootstrap processor (0).
 */
Mach* machp[MAXMACH];

#define	MACHP(n)	(machp[n])

extern Mach	*m;
extern Proc	*up;

typedef struct Irq {
	void	(*f)(Ureg*, void*);	/* handler to call */
	void*	a;			/* argument to call it with */

	Irq*	next;			/* link to next handler */
} Irq;

typedef struct Irqctl {
	int	(*isr)(int);		/* get isr bit for this irq */
	int	(*eoi)(int);		/* eoi */
	int	isintr;

	Irq*	irq;			/* handlers on this IRQ */
} Irqctl;

extern int swcursor;
