/*
 *	Et tu, Brutus.
 */
#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"io.h"
#include	"image.h"
#include	<memimage.h>
#include	"screen.h"
#include "bootparam.h"

extern int cflag;
extern int consoleprint;
extern int redirectconsole;
extern int main_pool_pcnt;
extern int heap_pool_pcnt;
extern int image_pool_pcnt;
extern int kernel_pool_pcnt;
extern char debug_keys;
extern Vmode default_vmode;

int smodem_HybridDelay = 0x23;	/* see comment in devsm.c */

enum {
	MODEMHOOK=	BIT(8),
};

void
archreset(void)
{
	/* put the hardware in a known state */
	*GFER = 0;
	*GRER = 0;
	*GEDR = *GEDR;
	*GPDR = 0;

	dmareset();

	/* init codec, set direction for modem hook */
	mcpinit();
	mcpgpiosetdir(MODEMHOOK, MODEMHOOK);
}

void
archconfinit(void)
{
	/* allow bootparam environment to override defaults */
	bpoverride("cflag", &cflag);
	bpoverride("consoleprint", &consoleprint);
	bpoverride("redirectconsole", &redirectconsole);
	bpoverride("kernel_pool_pcnt", &kernel_pool_pcnt);
	bpoverride("main_pool_pcnt", &main_pool_pcnt);
	bpoverride("heap_pool_pcnt", &heap_pool_pcnt);
	bpoverride("image_pool_pcnt", &image_pool_pcnt);
	bpoverride_uchar("vidhz", &default_vmode.hz);
	bpoverride_uchar("debugkeys", (uchar*)&debug_keys);
	bpoverride("smhybrid", &smodem_HybridDelay);

	conf.topofmem = bootparam->himem;
	conf.flashbase = bootparam->flashbase;
	conf.cpuspeed = bootparam->cpuspeed;
	conf.pagetable = bootparam->pagetable;

	conf.usebabycache = 1;
	conf.cansetbacklight = 1;
	conf.cansetcontrast = 0;
}

void
archreboot(void)
{
	bootparam->reboot(1);
}

int gpio_irq_ucb1200 = 22;

void
lights(ulong)
{
	/* XXX to do; there are some lights to play with! */
}

void
lcd_setbacklight(int on)
{
	if (on)
		*GPSR = (1<<16);
	else
		*GPCR = (1<<16);
}

void
lcd_setbrightness(ushort)
{
}

void
lcd_setcontrast(ushort)
{
}

int
archhooksw(int offhook)
{
	mcpgpiowrite(MODEMHOOK, offhook ? MODEMHOOK : 0);
	return 0;
}

void
archspeaker(int on, int vol)
{
	mcpspeaker(on, vol);
}

int
archflash12v(int /*on*/)
{
        return 1;       // umm, sure, we got 12V...
}

void
archflashwp(int wp)
{
        if(wp)
                *(ulong*)0x0e000000 &= ~0x20200000;     // disable 5V
        else
                *(ulong*)0x0e000000 |= 0x20200000;      // enable 5V
}

int	touch_read_delay = 100;	/* usec between setup and first reading */
int	touch_l2nreadings = 5;	/* log2 of number of readings to take */
int	touch_minpdelta = 40;	/* minimum pressure difference to 
			   detect a change during calibration */
int	touch_filterlevel = 900; /* -1024(off) to 1024(full): 1024*cos(angle) */


TouchCal touchcal = {
	{
		{10, 10}, {630, 10}, {630, 470}, {10, 470},
	},
	{
		{   {779, 148}, {84, 165}, {88, 845}, {779, 764}, },
		{   {876, 784}, {147, 842}, {129, 160}, {827, 161}, },
		{   {836, 148}, {129, 164}, {145, 845}, {869, 762}, },
		{   {876, 784}, {148, 842}, {129, 160}, {827, 161}, },
	},
	{
		{   -58458, 343, 1084, 44329, 46037734, -5329820 },
		{   -55618, 1467, -3509, -44108, 47320930, 39557584 },
		{   -57447, 1349, 1001, 44249, 49015133, -5258007 },
		{   -55689, 1551, -3513, -44105, 47341696, 39559572 },
	},
	{10, 21},
	{2, 2},
	144, 111,
};

/*
 *	PCMCIA support for Caesar, provided via an external register (CS3REG)
 *	and 6 GPIO pins.
 */
#define CS3REG	(ulong*)(0x18000000)

enum
{
	/* cs3reg bits */
	Cs3bits_0=	16,			/* shift position of slot's bits in cs3reg */
	Cs3bits_1=	24,
	Drven_0=		(1<<22),		/* Enable addr/ctrl fanout buffers */
	Drven_1=		(1<<30),
	Stschg_0=	(1<<10),		/* Status chg */
	Stschg_1=	(1<<14),
	NotReset_0=	(1<<23),		/* SW control of RESET */
	NotReset_1=	(1<<31),
	V12en_0=	(1<<20),		/* Enable 12v programming voltage */
	V12en_1=	(1<<28),
	V5en_0=		(1<<21),		/* Enable 5v inverter */
	V5en_1=		(1<<29),

	Vsupmsk=	0x3,			/* VCC/VPP supplied - each uses two bits */
	Vppbits_0=	16,
	Vppbits_1=	24,
		Vpp_0v=		0,
		Vpp_vcc=		1,
		Vpp_12v=	2,
	Vccbits_0=	18,
	Vccbits_1=	26,
		Vcc_0v=		0,
		Vcc_3v=		1,
		Vcc_5v=		2,

	Vccreqmsk=	0x3,			/* VCC req'd by card (low-voltage spec) */
	Vccreqbits_0=	8,
	Vccreqbits_1=	12,

	/* GPIO bits */
	GPIOeject_0=	0,	/* insert -> falling */
	GPIOeject_1=	1,
	GPIOrdy_0=	26,
	GPIOrdy_1=	27,
	GPIOstschg_0=	20,
	GPIOstschg_1=	24,
};

#define	Cs3bits(s)	((s) ? Cs3bits_1 : Cs3bits_0)
#define	Drven(s)	((s) ? Drven_1 : Drven_0)
#define	NotReset(s)	((s) ? NotReset_1 : NotReset_0)
#define	Resetmsk(s)	(~(0xff<<Cs3bits(s)))
#define	Stschg(s)	((s) ? Stschg_1 : Stschg_0)
#define	V12en(s)	((s) ? V12en_0 : V12en_1)
#define	V5en(s)	((s) ? V5en_0 : V5en_1)
#define	Vccreqbits(s)	((s) ? Vccreqbits_1 : Vccreqbits_0)
#define	Vccbits(s)	((s) ? Vccbits_1 : Vccbits_0)
#define	Vppbits(s)	((s) ? Vppbits_1 : Vppbits_0)

void
pcmsetvcc(int slotno, int vcc)
{
	int code, shift;

	shift = Vccbits(slotno);
	switch (vcc) {
	case 0:
		code = Vcc_0v;
		break;
	case 3:
		code = Vcc_3v;
		break;
	case 5:
		if(pcmpowered(slotno) == 3) {
			/* avoid a hazard which resets my brutus board -- try caesar without it */
/*			*CS3REG &= ~(Vsupmsk << shift);
			delay(300);
*/
		}
		code = Vcc_5v;
		break;
	default:
		print("illegal vcc voltage %d for slot %d\n", vcc, slotno);		/* error? */
		return;
	}

	if (vcc == 5)
		*CS3REG |= V5en(slotno);			/* Turn on 5v convertor */
	else
		*CS3REG &= ~V5en(slotno);		/* Turn off 5v */

	*CS3REG = (*CS3REG & ~(Vsupmsk << shift)) | (code << shift);
}

void
pcmsetvpp(int slotno, int vpp)	/* set power and enable device */
{
	int shift, code;

	switch (vpp) {
	case 0:
		code = Vpp_0v;
		break;
	case 12:
		code = Vpp_12v;
		break;
	default:
		if (vpp == pcmpowered(slotno)) {		/* same as Vcc? */
			code = Vpp_vcc;
			break;
		}
		print("illegal vpp voltage %d for slot %d\n", vpp, slotno);		/* error? */
		return;
	}

	shift = Vppbits(slotno);
	*CS3REG &= ~(Vsupmsk << shift);		/* Turn off Vpp first */

	if (vpp == 12)
		*CS3REG |= V12en(slotno);		/* Turn on 12v convertor */
	else
		*CS3REG &= ~V12en(slotno);		/* Turn off 12v */

	*CS3REG |= (code << shift);			/* set new value for Vpp */
	delay(300);						/* empirical (???) */
}

void
pcmpower(int slotno, int on)
{
	int vcc;

	/* first switch off power */
	*CS3REG &= Resetmsk(slotno);
	if (!on)
		return;

	/*
	 * power up and unreset, wait's are empirical (???)
	 * Vcc is set to the value requested via the voltage sense lines.
	 */
	delay(2);
	SET(vcc);
	switch ((*CS3REG >> Vccreqbits(slotno)) & Vccreqmsk) {
	case 3:		/* vs#1,#2 set -> 5v */
		vcc = 5;
		break;
	case 2:		/* vs#2 set -> 3.3v */
	case 0:		/* vs#1,#2 0 -> x.x or 3.3v */
		vcc = 3;
		break;
	case 1:		/* vs#1 set -> x.x, not supported */
		vcc = 0;
		print("unsupported voltage request for slot %d\n", slotno);
	}
	pcmsetvcc(slotno, vcc);
	delay(300);
	*CS3REG |= Drven(slotno) | NotReset(slotno);
	delay(500);
}

int
pcmpowered(int slotno)
{
	switch ((*CS3REG >> Vccbits(slotno)) & Vsupmsk) {
	case Vcc_3v:
		return 3;
	case Vcc_5v:
		return 5;
	default:
		return 0;
	}
}

int
pcmpin(int slot, int type)
{
	switch (type) {
	case PCMready:
		return (slot == 0) ? GPIOrdy_0 : GPIOrdy_1;
	case PCMeject:
		return (slot == 0) ? GPIOeject_0 : GPIOeject_1;
	case PCMstschng:
		return (slot == 0) ? GPIOstschg_0 : GPIOstschg_1;
	}
}

/*
 *	``What kind of struggle?  _What_ kind of struggle?''
 *	``A political struggle.''   - Monty Python
 */
