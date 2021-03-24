#include	"u.h"
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"ureg.h"

/*
 *  8259 interrupt controllers
 */
enum
{
	Int0ctl=	0x20,		/* control port (ICW1, OCW2, OCW3) */
	Int0aux=	0x21,		/* everything else (ICW2, ICW3, ICW4, OCW1) */
	Int1ctl=	0xA0,		/* control port */
	Int1aux=	0xA1,		/* everything else (ICW2, ICW3, ICW4, OCW1) */

	Icw1=		0x10,		/* select bit in ctl register */
	Ocw2=		0x00,
	Ocw3=		0x08,

	EOI=		0x20,		/* non-specific end of interrupt */
};

int	int0mask = 0xff;	/* interrupts enabled for first 8259 */
int	int1mask = 0xff;	/* interrupts enabled for second 8259 */

typedef struct Handler Handler;
typedef struct Handler {
	void		(*r)(Ureg*, void*);	/* handler to call */
	void*		a;			/* argument to call it with */
	Handler*	l;			/* link to next handler */
} Handler;

static struct {
	Handler*	h[256];			/* handlers on this vector */
} handler;

void
setvec(int v, void (*r)(Ureg*, void*), void* a)
{
	Handler *hp;

	if((hp = malloc(sizeof(Handler))) == 0){
		print("setvec: too many links\n");
		return;
	}

	hp->r = r;
	hp->a = a;
	hp->l = handler.h[v];
	handler.h[v] = hp;

	/*
	 *  enable corresponding interrupt in 8259
	 */
	if((v&~0x7) == Int0vec){
		int0mask &= ~(1<<(v&7));
		outb(Int0aux, int0mask);
	} else if((v&~0x7) == Int1vec){
		int1mask &= ~(1<<(v&7));
		outb(Int1aux, int1mask);
	}
}

void
maskirq(int irq)
{
	if(irq < 8){
		int0mask |= 1<<irq;
		outb(Int0aux, int0mask);
	}
	else{
		int1mask |= 1<<(irq-8);
		outb(Int1aux, int1mask);
	}
}

void
intr8259init(void)
{
	int0mask = 0xFF;
	int1mask = 0xFF;

	/*
	 *  Set up the first 8259 interrupt processor.
	 *  Make 8259 interrupts start at CPU vector Int0vec.
	 *  Set the 8259 as master with edge triggered
	 *  input with fully nested interrupts.
	 */
	outb(Int0ctl, (1<<4)|(0<<3)|(1<<0));	/* ICW1 - master, edge triggered,
					  	   ICW4 will be sent */
	outb(Int0aux, Int0vec);			/* ICW2 - interrupt vector offset */
	outb(Int0aux, 0x04);			/* ICW3 - have slave on level 2 */
	outb(Int0aux, 0x01);			/* ICW4 - 8086 mode, not buffered */

	/*
	 *  Set up the second 8259 interrupt processor.
	 *  Make 8259 interrupts start at CPU vector IRQvBase+8.
	 *  Set the 8259 as slave with level triggered
	 *  input with fully nested interrupts.
	 */
	outb(Int1ctl, (1<<4)|(0<<3)|(1<<0));	/* ICW1 - master, edge triggered,
					  	   ICW4 will be sent */
	outb(Int1aux, Int0vec+8);		/* ICW2 - interrupt vector offset */
	outb(Int1aux, 0x02);			/* ICW3 - I am a slave on level 2 */
	outb(Int1aux, 0x01);			/* ICW4 - 8086 mode, not buffered */
	outb(Int1aux, int1mask);

	/*
	 *  pass #2 8259 interrupts to #1
	 */
	int0mask &= ~0x04;
	outb(Int0aux, int0mask);

	/*
	 * Set Ocw3 to return the ISR when ctl read.
	 * After initialisation status read is set to IRR.
	 * Read IRR first to possibly deassert an outstanding
	 * interrupt.
	 */
	inb(Int0ctl);
	outb(Int0ctl, Ocw3|0x03);
	inb(Int1ctl);
	outb(Int1ctl, Ocw3|0x03);
}

/*
 *  set up the interrupt/trap gates
 */
void
trapinit(void)
{
	int v, pri;
	ulong vaddr, x;
	Segdesc *idt;
	ushort ptr[3];

	idt = (Segdesc*)IDTADDR;
	vaddr = (ulong)vectortable;
	pri = 0;
	for(v = 0; v < 256; v++){
		idt[v].d0 = (vaddr & 0xFFFF)|(KESEL<<16);
		idt[v].d1 = (vaddr & 0xFFFF0000)|SEGP|SEGPL(pri)|SEGIG;
		vaddr += 6;
	}

	/*
	 *  tell the hardware where the table is (and how long)
	 */
	ptr[0] = sizeof(Segdesc)*256;
	x = IDTADDR;
	ptr[1] = x & 0xFFFF;
	ptr[2] = (x>>16) & 0xFFFF;
	lidt(ptr);

	intr8259init();
}

/*
 *  dump registers
 */
static void
dumpregs(Ureg *ur)
{
	print("FLAGS=%lux TRAP=%lux ECODE=%lux PC=%lux\n",
		ur->flags, ur->trap, ur->ecode, ur->pc);
	print("  AX %8.8lux  BX %8.8lux  CX %8.8lux  DX %8.8lux\n",
		ur->ax, ur->bx, ur->cx, ur->dx);
	print("  SI %8.8lux  DI %8.8lux  BP %8.8lux\n",
		ur->si, ur->di, ur->bp);
	print("  CS %4.4ux DS %4.4ux  ES %4.4ux  FS %4.4ux  GS %4.4ux\n",
		ur->cs & 0xFF, ur->ds & 0xFFFF, ur->es & 0xFFFF, ur->fs & 0xFFFF, ur->gs & 0xFFFF);
	print("  CR0 %8.8lux CR2 %8.8lux CR3 %8.8lux\n",
		getcr0(), getcr2(), getcr3());
}

/*
 *  All traps
 */
void
trap(Ureg *ur)
{
	int v;
	int c;
	Handler *hp;
	ushort isr;
	static int spurious;

	v = ur->trap;
	/*
	 *  tell the 8259 that we're done with the
	 *  highest level interrupt (interrupts are still
	 *  off at this point)
	 */
	c = v&~0x7;
	isr = 0;
	if(c==Int0vec || c==Int1vec){
		isr = inb(Int0ctl);
		outb(Int0ctl, EOI);
		if(c == Int1vec){
			isr |= inb(Int1ctl)<<8;
			outb(Int1ctl, EOI);
		}
	}

	hp = handler.h[v];
	if(v>=256 || hp == 0){
		if(v >= Int0vec && v < Int0vec+16){
			v -= Int0vec;
			/*
			 * Check for a default IRQ7. This can happen when
			 * the IRQ input goes away before the acknowledge.
			 * In this case, a 'default IRQ7' is generated, but
			 * the corresponding bit in the ISR isn't set.
			 * In fact, just ignore all such interrupts.
			 */
			if(isr & (1<<v)){
				print("unknown interrupt %d pc=0x%lux\n", v, ur->pc);
				outb(Int0ctl, EOI);
				if(c == Int1vec)
					outb(Int1ctl, EOI);
			}
			return;
		}

		switch(v){

		case 0x02:				/* NMI */
			print("NMI: nmisc=0x%2.2ux, nmiertc=0x%2.2ux, nmiesc=0x%2.2ux\n",
				inb(0x61), inb(0x70), inb(0x461));
			return;

		default:
			print("exception/interrupt %d\n", v);
			dumpregs(ur);
			spllo();
			print("^P to reset\n");
			for(;;);
		}
	}

	/*
	 *  call the trap routines
	 */
	do {
		(*hp->r)(ur, hp->a);
		hp = hp->l;
	} while(hp);
}
