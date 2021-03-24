#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"

#include	<image.h>
#include	<memimage.h>
#include	<cursor.h> 
#include	"screen.h"

#include	"archpaq.h"

/*
 * board-specific support for the 821/823 PowerPAQ
 */

enum {
	/* sccr */
	RTSEL = IBIT(8),	/* =0, select main oscillator (OSCM); =1, select external crystal (EXTCLK) */
	RTDIV = IBIT(7),	/* =0, divide by 4; =1, divide by 512 */
	CRQEN = IBIT(9),	/* =1, switch to high frequency when CPM active */
	PRQEN = IBIT(10),	/* =1, switch to high frequency when interrupt pending */

	/* plprcr */
	CSRC = IBIT(21),	/* =0, clock is DFNH; =1, clock is DFNL */
};

void (*coherence)(void) = eieio;

/*
 * called early in main.c, after machinit:
 * using board and architecture specific registers, initialise
 * 8xx registers that need it and complete initialisation of the Mach structure.
 */
void
archinit(void)
{
	IMM *io;
	int mf;

	m->bcsr = nil;	/* no BCSR space */
	io = m->iomem;	/* run by reset code: no need to lock */
	m->clockgen = 32768;	/* crystal frequency */
	m->oscclk = m->clockgen/MHz;	/* TO DO: 32k clock */
	io->plprcrk = KEEP_ALIVE_KEY;
	io->plprcr &= ~CSRC;	/* general system clock is DFNH */
	mf = (io->plprcr >> 20)+1;	/* use timing set by bootstrap */
	io->plprcrk = ~KEEP_ALIVE_KEY;
	io->sccrk = KEEP_ALIVE_KEY;
	io->sccr &= ~RTDIV;	/* divide 32k by 4 */
	io->sccr |= CRQEN | PRQEN;
	io->sccr |= RTSEL;	/* select EXTCLK */
	io->sccrk = ~KEEP_ALIVE_KEY;
	m->cpuhz = m->clockgen*mf;
	m->speed = m->cpuhz/MHz;
}

static ulong
banksize(int x, ulong *pa)
{
	IMM *io;

	io = m->iomem;
	if((io->memc[x].base & 1) == 0)
		return 0;	/* bank not valid */
	*pa = io->memc[x].base & ~0x7FFF;
	return -(io->memc[x].option&~0x7FFF);
}

/*
 * initialise the kernel's memory configuration:
 * there are two banks (base0, npage0) and (base1, npage1).
 * initialise any other values in conf that are board-specific.
 */
void
archconfinit(void)
{
	ulong nbytes, pa, ktop;

	conf.nscc = 2;

	conf.npage0 = 0;
	nbytes = banksize(DRAM1, &pa);
	if(nbytes){
		conf.npage0 = nbytes/BY2PG;
		conf.base0 = pa;
	}

	conf.npage1 = 0;

	/* the following assumes the kernel text and/or data is in bank 0 */
	ktop = PGROUND((ulong)end);
	ktop = PADDR(ktop) - conf.base0;
	conf.npage0 -= ktop/BY2PG;
	conf.base0 += ktop;
}

void
cpuidprint(void)
{
	ulong v;

	print("PVR: ");
	switch(m->cputype){
	case 0x01:	print("MPC601"); break;
	case 0x03:	print("MPC603"); break;
	case 0x04:	print("MPC604"); break;
	case 0x06:	print("MPC603e"); break;
	case 0x07:	print("MPC603e-v7"); break;
	case 0x50:	print("MPC8xx"); break;
	default:	print("PowerPC version #%x", m->cputype); break;
	}
	print(", revision #%lux\n", getpvr()&0xffff);
	print("IMMR: ");
	v = getimmr() & 0xFFFF;
	switch(v>>8){
	case 0x00:	print("MPC860/821"); break;
	case 0x20:	print("MPC823"); break;
	case 0x21:	print("MPC823A"); break;
	default:	print("Type #%lux", v>>8); break;
	}
	print(", mask #%lux\n", v&0xFF);
	print("plprcr=%8.8lux sccr=%8.8lux\n", m->iomem->plprcr, m->iomem->sccr);
	print("%lud MHz system\n", m->cpuhz/MHz);
	print("\n");
}

/*
 * provide value for #r/switch (devrtc.c)
 */
int
archoptionsw(void)
{
	return 1<<0;
}

/*
 * invoked by clock.c:/^clockintr
 */
static void
twinkle(void)
{
	/* no easy-to-use LED on PAQ (they use i2c) */
}

void	(*archclocktick)(void) = twinkle;

/*
 * invoked by ../port/taslock.c:/^ilock:
 * reset watchdog timer here, if there is one and it is enabled
 * (qboot currently disables it on the FADS board)
 */
void
clockcheck(void)
{
}

/*
 * for devflash.c:/^flashreset
 * retrieve flash type, virtual base and length and return 0;
 * return -1 on error (no flash)
 */
int
archflashreset(char *type, void **addr, long *length)
{
	strcpy(type, "AMD29F0x0");	/* BUG; it isn't: it's 16-bit flash */
	*addr = KADDR(FLASHMEM);
	*length = 4*1024*1024;
	return 0;
}

/*
 * enable the clocks for the given SCC ether and reveal them to the caller.
 * do anything else required to prepare the transceiver (eg, set full-duplex, reset loopback).
 */
int
archetherenable(int cpmid, int *rcs, int *tcs)
{
	USED(cpmid, rcs, tcs);
	return -1;	/* there isn't an ether on the PAQs */
}

/*
 * do anything extra required to enable the UART on the given CPM port
 */
void
archenableuart(int id, int irda)
{
	IMM *io;

	USED(irda);
	switch(id){
	case CPsmc1:
		io = ioplock();
		io->pbodr &= ~0xc0;
		io->pbdat |= 0xc0;
		io->pcdat |= 0x400;
		io->pcpar &= ~0x400;
		io->pcdir |= 0x400;
		io->pcdat &= ~0x400;	/* enable SMC RS232 buffer */
		iopunlock();
		break;
	case CPscc2:
		break;
	default:
		/* nothing special */
		break;
	}
}

/*
 * do anything extra required to disable the UART on the given CPM port
 */
void
archdisableuart(int id)
{
	IMM *io;

	switch(id){
	case CPsmc1:
		io = ioplock();
		io->pcdat |= 0x400;	/* disable SMC RS232 buffer */
		iopunlock();
		break;
	case CPscc2:
		break;
	default:
		/* nothing special */
		break;
	}
}

/*
 * enable the external USB transceiver
 *	speed is 12MHz if highspeed is non-zero; 1.5MHz if zero
 *	master is non-zero if the node is acting as USB Host and should provide power
 */
void
archenableusb(int highspeed, int master)
{
	USED(highspeed, master);
}

/*
 * shut down the USB transceiver
 */
void
archdisableusb(void)
{
}

/*
 * set the external infrared transceiver to the given speed
 */
void
archsetirxcvr(int highspeed)
{
	USED(highspeed);
}

/*
 * force hardware reset/reboot
 */
void
archreboot(void)
{
	IMM *io;

for(;;);	/* for debugging */
	io = m->iomem;
	io->plprcrk = KEEP_ALIVE_KEY;
	io->plprcr |= 1<<7;	/* checkstop reset enable */
	io->plprcrk = ~KEEP_ALIVE_KEY;
	eieio();
	io->sdcr = 1;
	eieio();
	io->lccr = 0;	/* switch LCD off */
	eieio();
	firmware(0);
}

/*
 * board-specific PCMCIA support: assumes slot B on 82xFADS
 */

void
pcmenable(void)
{
	ioplock();
	iopunlock();
}

int
pcmpowered(int)
{
	return 0;
}

void
pcmsetvcc(int, int v)
{
	ioplock();
	USED(v);
	iopunlock();
}

void
pcmsetvpp(int, int v)
{
	ioplock();
	USED(v);
	iopunlock();
}

void
pcmpower(int slotno, int on)
{
	if(!on){
		pcmsetvcc(slotno, 0);	/* turn off card power */
		pcmsetvpp(slotno, -1);	/* turn off programming voltage (Hi-Z) */
	}else
		pcmsetvcc(slotno, 5);
}

/*
 * enable/disable the LCD panel's backlight
 */
void
archbacklight(int on)
{
	uchar	msg;
	IMM *io;

	io = ioplock();
	io->pbpar &= ~EnableLCD;
	io->pbodr &= ~EnableLCD;
	io->pbdir |= EnableLCD;
	if(on)
		io->pbdat |= EnableLCD;
	else
		io->pbdat &= ~EnableLCD;
	iopunlock();
//	if(on) {
//		msg = ~(DisablePanelVCC5|DisableTFT);
//		i2csend(PanelI2C, &msg, 1);
//	} else {
//		msg = ~0;
//		i2csend(PanelI2C, &msg, 1);
//	}
}

/*
 * set parameters to describe the screen
 */
int
archlcdmode(Mode *m)
{
	m->x = 640;
	m->y = 480;
	m->d = 3;
	m->lcd.freq = 25000000;
	m->lcd.ac = 0;
	m->lcd.vpw = 1;
	m->lcd.wbf = 33;
	m->lcd.wbl = 228;
	m->lcd.flags = IsColour | IsTFT | OELow | VsyncLow | ClockLow;
	return 0;
}

enum {
	KD=	SIBIT(9),	/* active low; can interrupt */
	KR=	IBIT(22),	/* active low */
	KH=	IBIT(18),	/* active low */
};

/*
 * reset the ps/2 keyboard port
 */
void
archkbdinit(void)
{
	IMM *io;

	io = ioplock();
	io->pbpar &= ~(KH|KR);
	io->pbdat |= KH|KR;
	delay(1);
	io->pbdat &= ~(KH|KR);
	iopunlock();
}
