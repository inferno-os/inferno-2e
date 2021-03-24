/*
 *  Sword keyboard -- USAR SPIcoder + Fujitsu Keyboard
 *
 * BUG: the SSP/SPI support should be broken out (and done properly!)
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "io.h"
#include "fns.h"
#include "keyboard.h"

void	fujkbdputc(Queue *, int);

Rune kbtab[] = 
{	
[0x00]	No,   LAlt,   No,   No,   No,   No,   No,   No, 
[0x08]	No,  96, '\\', '\t', 'z', 'a', 'x',   No, 
[0x10]	No,   No,   LShift,   No,   No,   No,   No,   No, 
[0x18]	No,   LCtrl, No,   No,   No,   No,   No,   No, 
[0x20]	No,   Meta,   No,   No,   No,   No,   No,   No, 
[0x28]	No, Esc,Del, 'q',  Caps, 's', 'c', '3', 
[0x30]	No, '1',   No, 'w',   No, 'd', 'v', '4', 
[0x38]	No, '2', 't', 'e',   No, 'f', 'b', '5', 
[0x40]	No, '9', 'y', 'r', 'k', 'g', 'n', '6', 
[0x48]	No, '0', 'u', 'o', 'l', 'h', 'm', '7', 
[0x50]	No, '-', 'i', 'p', ';', 'j', ',', '8', 
[0x58]	No, '=','\n', '[','\'', '/', '.', Latin, 
[0x60]	No,   No,   RShift,   No,   No,   No,   No,   No, 
[0x68]	No,  '\b',  Down, ']',  Up,  Left, ' ',  Right, 
[0x70]	No,   No,   No,   No,   No,   No,   No,   No, 
[0x78]	No,   No,   No,   No,   No,   No,   No,   No
};

Rune kbtabshift[] =
{	
[0x00]	No,   LAlt,   No,   No,   No,   No,   No,   No, 
[0x08]	No, '~', '|', BackTab, 'Z', 'A', 'X',   No, 
[0x10]	No,   No,   LShift,   No,   No,   No,   No,   No, 
[0x18]	No,   LCtrl, No,   No,   No,   No,   No,   No, 
[0x20]	No,   Meta,   No,   No,   No,   No,   No,   No, 
[0x28]	No, Esc,Del, 'Q', Caps, 'S', 'C', '#', 
[0x30]	No, '!',   No, 'W',   No, 'D', 'V', '$', 
[0x38]	No, '@', 'T', 'E',   No, 'F', 'B', '%', 
[0x40]	No, '(', 'Y', 'R', 'K', 'G', 'N', '^', 
[0x48]	No, ')', 'U', 'O', 'L', 'H', 'M', '&', 
[0x50]	No, '_', 'I', 'P', ':', 'J', '<', '*', 
[0x58]	No, '+','\n', '{', '"', '?', '>', Latin, 
[0x60]	No,   No,   RShift,   No,   No,   No,   No,   No, 
[0x68]	No,  '\b',  Down, '}',  Up,  Left, ' ',  Right, 
[0x70]	No,   No,   No,   No,   No,   No,   No,   No, 
[0x78]	No,   No,   No,   No,   No,   No,   No,   No
};

Rune kbtabmeta[] =
{	
[0x00]	No, LAlt, No, No, No, No, No, No,
[0x08]	No, No, No, No, No, No, No, No,
[0x10]	No, No, LShift, No, No, No, No, No,
[0x18]	No, LCtrl, No, No, No, No, No, No,
[0x20]	No, Meta, No, No, No, No, No, No,
[0x28]	No, No, Del, No, Caps, No, No, No,
[0x30]	No, No, No, No, No, No, No, No,
[0x38]	No, No, No, No, No, No, No, No,
[0x40]	No, No, No, No, No, No, No, No,
[0x48]	No, No, No, No, No, No, No, No,
[0x50]	No, Num, No, No, No, No, No, No,
[0x58]	No, No, No, No, No, No, No, Latin,
[0x60]	No, No, RShift, No, No, No, No, No,
[0x68]	No, No, Pgdown, No, Pgup, Home, No, End,
[0x70]	No, No, No, No, No, No, No, No,
[0x78]	No, No, No, No, No, No, No, No,
};

Rune kbtabnuml[] =
{	
[0x00]	No,   LAlt,   No,   No,   No,   No,   No,   No, 
[0x08]	No,  96, 112, '\t', 'z', 'a', 'x',   No, 
[0x10]	No,   No,   LShift,   No,   No,   No,   No,   No, 
[0x18]	No,   LCtrl, No,   No,   No,   No,   No,   No, 
[0x20]	No,   Meta,   No,   No,   No,   No,   No,   No, 
[0x28]	No, Esc,Del, 'q',  Caps, 's', 'c', '3', 
[0x30]	No, '1',   No, 'w',   No, 'd', 'v', '4', 
[0x38]	No, '2', 't', 'e',   No, 'f', 'b', '5', 
[0x40]	No, '9', 'y', 'r', '2', 'g', 'n', '6', 
[0x48]	No, '*', '4', '6', '3', 'h', '0', '7', 
[0x50]	No, '-', '5', '-', '+', '1', ',', '8', 
[0x58]	No, '=','\n', '[','\'', '/', '.', Latin, 
[0x60]	No,   No,   RShift,   No,   No,   No,   No,   No, 
[0x68]	No,  '\b',  Down, ']',  Up,  Left, ' ',  Right, 
[0x70]	No,   No,   No,   No,   No,   No,   No,   No, 
[0x78]	No,   No,   No,   No,   No,   No,   No,   No
};

int pressed[NShifts];
int toggle[NShifts] = { [Caps-Shift]1, [Num-Shift]1, [Meta-Shift]1 };


static uchar
kbd_io(uchar c)
{
        uchar d;
        SET(d);
        *SSCR0 = 0;
        *SSCR0 = (3<<SSPCR0_V_SCR) | (1<<SSPCR0_V_SSE) | (0<<SSPCR0_V_FRF)
                | (7<<SSPCR0_V_DSS);
        while((*SSSR & SSPSR_M_RNE) == 1)       /* drain recv data */
                d = *SSDR;
        while((*SSSR & SSPSR_M_TNF) == 0)       /* check for empty */
                continue;
        USED(d);
        *SSDR = (c<<8) | c;     /* put char */
        while((*SSSR & SSPSR_M_RNE) == 0)       /* wait to get char back */
                continue;
        d = *SSDR;
        return d;
}

static void
kbdfujitsuintr(Ureg *ur, void *arg)
{
	int c;

	USED(ur, arg);
	for (;;) {
		if(*GPLR&(1<<25))
			break;	/* no new scancode */
		c = kbd_io(0);
		timer_devwait(GPLR, 1<<25, 1<<25, MS2TMR(500));
		fujkbdputc(kbdq, c);
		*GEDR = (1<<25);
	}
}

void
kbdinit(void)
{
	*GPDR = (*GPDR | (0xd << 10) | (1<<23)) & ~((0x2 << 10) | (1<<25));
	*GAFR = (*GAFR | (0xf << 10)) & ~((1<<23) | (1<<25));      
		/* gp10- 13 alt func */
	// *GPDR = 0x0d807424;
	// *GAFR = 0x00003c00;
	*GPSR = (1<<23);          /* assert kbctl wakeup pin */
	*(ulong*)0x90060008 |= (1<<18);    /* set alt func for spi interface */

	*SSCR0 = 0;
	*SSCR1 = 0;     /* no ints, no loopback */
	*SSSR = 0;      /* remove any rcv overrun errors */

	/* turn on SSP: */
	*SSCR0 = (3<<SSPCR0_V_SCR) | (1<<SSPCR0_V_SSE)
		| (0<<SSPCR0_V_FRF) | (7<<SSPCR0_V_DSS);

	*GRER &= ~(1<<25);
	intrenable(25, kbdfujitsuintr, 0, BusGPIO);
	*GFER |= (1<<25);
	if (kbdq == nil)
		kbdq = qopen(4*1024, 0, 0, 0);
}

void
fujkbdputc(Queue *kbdq, int c)
{
	int keyup = c&0x80;
	c &= 0x7f;

	if (pressed[Meta-Shift])
		c = kbtabmeta[c];
	else if (pressed[Num-Shift])
		c = kbtabnuml[c];
	else if (pressed[LShift-Shift] || pressed[RShift-Shift])
		c = kbtabshift[c];
	else
		c = kbtab[c];

	if(c >= Shift && c <= Shift+NShifts) {
		pressed[c-Shift] = !keyup^(pressed[c-Shift]&toggle[c-Shift]);
		return;
	}
	if(keyup)
		return;
	if(pressed[Caps-Shift])
		c = toupper(c);
	if(pressed[LCtrl-Shift] || pressed[RCtrl-Shift])
		c &= 0x1f;
	if(pressed[LAlt-Shift] || pressed[RAlt-Shift])
		c = APP|(c&0xff);
	kbdputc(kbdq, c);
}
