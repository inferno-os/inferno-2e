#include <lib9.h>
#include "dat.h"
#include "fns.h"

/*
 *	PCMCIA support for Brutus, provided via an external register (CS3REG)
 *	and 6 GPIO pins.
 */
#define CS3REG	(ulong*)(0x0e000000)

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
	V12en_0=	(1<<20),		/* Enable 12v inverter */
	V12en_1=	(1<<28),

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
	GPIOeject_0=	4,	/* insert -> falling */
	GPIOeject_1=	7,
	GPIOrdy_0=	3,
	GPIOrdy_1=	6,
	GPIOstschg_0=	2,
	GPIOstschg_1=	5,
};

#define	Cs3bits(s)	((s) ? Cs3bits_1 : Cs3bits_0)
#define	Drven(s)	((s) ? Drven_1 : Drven_0)
#define	NotReset(s)	((s) ? NotReset_1 : NotReset_0)
#define	Resetmsk(s)	(~(0xff<<Cs3bits(s)))
#define	Stschg(s)	((s) ? Stschg_1 : Stschg_0)
#define	V12en(s)	((s) ? V12en_0 : V12en_1)
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
		if(pcmpowered(slotno) == 5) {
			/* avoid a hazard which resets the system! */
			*CS3REG &= ~(Vsupmsk << shift);
			delay(100);
		}
		code = Vcc_3v;
		break;
	case 5:
		if(pcmpowered(slotno) == 3) {
			/* avoid a hazard which resets the system! */
			*CS3REG &= ~(Vsupmsk << shift);
			delay(100);
		}
		code = Vcc_5v;
		break;
	default:
		print("illegal vcc voltage %d for slot %d\n", vcc, slotno);		/* error? */
		return;
	}
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
	delay(100);						/* empirical (???) */
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
	delay(100);
	*CS3REG |= Drven(slotno) | NotReset(slotno);
	delay(200);
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

