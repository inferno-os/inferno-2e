#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "mem.h"
#include "kbd.h"

static uchar keybitmap[128/8];

#define KEYHIT(k)	keybitmap[(k)>>3] |= (1<<((k)&7))
#define KEYRELEASE(k)	keybitmap[(k)>>3] &= ~(1<<((k)&7))
#define KEYISHIT(k)	(keybitmap[(k)>>3] & (1<<((k)&7)))


//////////////////////////// Keyboard-specific scancode tables //////////////

#define esc 27
#define tab 0x09
#define prg 0x85
#define  bs 0x08
#define  dn 0x86
#define  up 0x87
#define  rt 0x88
#define  lt 0x89
#define del 0x7f

#define KEY_FN		0x21
#define KEY_LSHFT	0x12
#define KEY_RSHFT	0x62
#define KEY_CTRL	0x19
#define KEY_ALT		0x01
#define KEY_CAPS	0x2c
#define KEY_CPLK	0x70

static char kbmap[128] = {
0,   0,   0,   0,   0,   0,   0,   0, // column 0 
0,  96,'\\', tab, 'z', 'a', 'x',   0, // column 1 
0,   0,   0,   0,   0,   0,   0,   0, // column 2 
0,   0, del,   0,   0,   0,   0,   0, // column 3 
0,   0,   0,   0,   0,   0,   0,   0, // column 4 
0, esc,0xff, 'q',   0, 's', 'c', '3', // column 5 
0, '1',   0, 'w',   0, 'd', 'v', '4', // column 6 
0, '2', 't', 'e',   0, 'f', 'b', '5', // column 7 
0, '9', 'y', 'r', 'k', 'g', 'n', '6', // column 8 
0, '0', 'u', 'o', 'l', 'h', 'm', '7', // column 9 
0, '-', 'i', 'p', ';', 'j', ',', '8', // column 10 
0, '=','\n', '[','\'', '/', '.', prg, // column 11 
0,   0,   0,   0,   0,   0,   0,   0, // column 12
0,  bs,  dn, ']',  up,  lt, ' ',  rt, // column 13
0,   0,   0,   0,   0,   0,   0,   0, // column 14 no codes
0,   0,   0,   0,   0,   0,   0,   0}; // column 15 no codes

char kbmap_shft[128] = {
0,   0,   0,   0,   0,   0,   0,   0, // column 0 
0, '~', '|', tab, 'Z', 'A', 'X',   0, // column 1 
0,   0,   0,   0,   0,   0,   0,   0, // column 2 
0,   0, del,   0,   0,   0,   0,   0, // column 3 
0,   0,   0,   0,   0,   0,   0,   0, // column 4 
0, esc,0xff, 'Q',   0, 'S', 'C', '#', // column 5 
0, '!',   0, 'W',   0, 'D', 'V', '$', // column 6 
0, '@', 'T', 'E',   0, 'F', 'B', '%', // column 7 
0, '(', 'Y', 'R', 'K', 'G', 'N', '^', // column 8 
0, ')', 'U', 'O', 'L', 'H', 'M', '&', // column 9 
0, '_', 'I', 'P', ':', 'J', '<', '*', // column 10 
0, '+','\n', '{', '"', '?', '>', prg, // column 11 
0,   0,   0,   0,   0,   0,   0,   0, // column 12
0,  bs,  dn, '}',  up,  lt, ' ',  rt, // column 13
0,   0,   0,   0,   0,   0,   0,   0, // column 14 no codes
0,   0,   0,   0,   0,   0,   0,   0}; // column 15 no codes


//////////////////////////// Interface-specific I/O ////////////////////////

static uchar
kbd_io(uchar c)
{
	uchar d;
	SspReg *ssp = SSPREG;
	ssp->sscr0 = 0;
	ssp->sscr0 = (3<<SSPCR0_V_SCR) | (1<<SSPCR0_V_SSE) | (0<<SSPCR0_V_FRF)
		| (7<<SSPCR0_V_DSS);
	while((ssp->sssr & SSPSR_M_RNE) == 1) {	/* drain recv data */
		d = ssp->ssdr;
		USED(d);
	}
	timer_devwait(&ssp->sssr, SSPSR_M_TNF, SSPSR_M_TNF, MS2TMR(100));
	ssp->ssdr = (c<<8) | c;	/* put char */
	timer_devwait(&ssp->sssr, SSPSR_M_RNE, SSPSR_M_RNE, MS2TMR(100));
	d = ssp->ssdr;
	return d;
}

static void
kbd_write(const uchar *buf, int len)
{
	int crc = 0;
	int i, x;
	GpioReg *gpio = GPIOREG;
	gpio->gpcr = 1<<23;
	for(i=0; i<3; i++) {
		x = gpio->gplr;
		USED(x);
	}
	gpio->gpsr = 1<<23;
	for(i=0; i<250*30; i++) {
		x = gpio->gplr;
		USED(x);
	}
	while(len-- > 0) {
		kbd_io(*buf);
		crc ^= *buf++;
		for(i=0; i<400; i++) {
			x = gpio->gplr;
			USED(x);
		}
	}
	if((crc &0x80) == 0x80)
		crc ^= 0xc0;
	kbd_io(crc);
	for(i=0; i<400; i++) {
		x = gpio->gplr;
		USED(x);
	}
}

static void
kbd_setled(int on)
{
	if(on)
		kbd_write((uchar*)"\033\246\000\001\000\000\000\000", 8);
	else
		kbd_write((uchar*)"\033\246\000\000\000\000\000\000", 8);
}

static int
kbd_getscancode(void)
{
	GpioReg *gpio = GPIOREG;
	if(gpio->gplr&(1<<25)) 
		return -1;	/* no new scancode */
	else {
		int x = kbd_io(0);
		timer_devwait(&gpio->gplr, 1<<25, 1<<25, MS2TMR(500));
		return x;
	}
}

void
kbd_init(void)
{
	GpioReg *gpio = GPIOREG;
	SspReg *ssp = SSPREG;
	PpcReg *ppc = PPCREG;
	gpio->gpdr = (gpio->gpdr|(0xd<<10)|(1<<23))&~((0x2<<10)|(1<<25));
	gpio->gafr = (gpio->gafr|(0xf<<10))&~((1<<23)|(1<<25));
	gpio->gpsr = 1<<23;		/* assert kbctl wakeup pin */
	ppc->ppar = 1<<PPC_V_SPR;	/* set alt func for ssp/spi interface */

	ssp->sscr0 = 0;
	ssp->sscr1 = 0;	/* no ints, no loopback */
	ssp->sssr = 0;	/* remove any rcv overrun errors */

	/* turn on SSP: */
	ssp->sscr0 = (3<<SSPCR0_V_SCR) | (1<<SSPCR0_V_SSE)
		| (0<<SSPCR0_V_FRF) | (7<<SSPCR0_V_DSS);

	memset(keybitmap, 0, sizeof(keybitmap));	
	kbd_write((uchar*)"\033\247\000\003", 4);	/* led on */
}

//////////////////////////// Generic keyboard processing ////////////////////


static int kbd_char = -1;


static int
kbd_charav_now(void)
{
	int c;
	if(kbd_char >= 0)
		return kbd_char;
	while((c = kbd_getscancode()) >= 0) {
		/* print("sc=%ux (%d %d %d %d)\n", c,
			KEYISHIT(KEY_CPLK),
			KEYISHIT(KEY_LSHFT),
			KEYISHIT(KEY_RSHFT),
			KEYISHIT(KEY_CTRL));
		*/
		if(c&0x80) 	/* key up */
			KEYRELEASE(c&0x7f);
		else {	/* key down */
			KEYHIT(c);
			if(c == KEY_CAPS) {
				if(KEYISHIT(KEY_CPLK)) {
					KEYRELEASE(KEY_CPLK);
					kbd_setled(0);
				} else {
					KEYHIT(KEY_CPLK);
					kbd_setled(1);
				}
				c = 0;
			} else if(KEYISHIT(KEY_LSHFT) || KEYISHIT(KEY_RSHFT)) 
				c = kbmap_shft[c];
			else
				c = kbmap[c];
			if(KEYISHIT(KEY_CPLK) && c >= 'a' && c <= 'z')
				c += 'A' - 'a';
				
			if(KEYISHIT(KEY_CTRL))
				c &= 0x1f;
			if(c) {
				kbd_char = c;
				return c;
			}
		}
	}
	return -1;
}

int
kbd_charav(void)
{
	int c, i;
	for(i=0; i<20; i++) {
		if((c = kbd_charav_now()) >= 0)
			return c;
		delay(1);
	}
	return 0;
}

int
kbd_getc(void)
{
	int c;
	while(kbd_charav_now() < 0)
		;
	c = kbd_char;
	kbd_char = -1;
	kbd_charav();	/* to hopefully get key release... */
	return c;
}

