/*************************************************************/
/* Keyboard Manufacturer: Sejin Electron Inc.				 */
/* Keyboard Model #:      SWK-8630							 */
/*************************************************************/

#include <lib9.h>
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include <keyboard.h>

static uchar shiftbitmap;

static uchar keybitmap[128/8];
static uchar CapsLock;

#define KEYHIT(k)	keybitmap[(k)>>3] |= (1<<((k)&7))
#define KEYRELEASE(k)	keybitmap[(k)>>3] &= ~(1<<((k)&7))
#define KEYISHIT(k)	(keybitmap[(k)>>3] & (1<<((k)&7)))

#define REPEAT_SPEED	5

/*************Keyboard specific scan codes*********************/


/**************	Keyboard-specific scancode tables ************/
/* KeyMatrix Valid For:	Sejin Electron Inc. SWK-8630		*/
/*  P/N's: SPR-8695WRA, SPR-8696W/WT/WS, SPR-8638W/WT,		*/
/*		   SPR-8698W/WT, SPR-8697WT			*/
/*  Production run for Shannon phones				*/
/*  (engineering sample has different mappings, which can	*/
/*   be found in spr8695wra.h in pre-inf2.3 trees)		*/
/*************************************************************/

/* Don't use this keyboard mapping elsewhere... this is a
 * partial mapping that is just good enough for the boot loader
 */

enum {
	LeftShiftKey=	0061,
	RightShiftKey=	0062,
	NumLockKey= 	0122,
	FnKey=		0000,
	LeftCtrlKey=	0034,
	RightCtrlKey=	0124,
	LeftAltKey=	0053,
	RightAltKey=	0053,
	LatinKey=	0050,
	CapsLockKey=	0011,
};

uchar kbmap[128] = {
     No,     No,     No,     No,    Del,     No,     No,     No, 
     No,     No,    'x',   KF|5,   KF|1,    '2',    's',    'w', 
     No,   KF|3,    'c',   KF|4,   KF|2,    '3',    'd',    'e', 
     No,   '\t',    'z',    Esc,     No,    '1',    'a',    'q', 
    'b',    't',    'v',    'g',    '5',    '4',    'f',    'r',
  Latin,     No,     No,     No,     No,  SysRq,     No, Scroll, 
     No,     No,     No,     No,     No,     No,     No,     No, 
    ' ',   '\b',   '\n',   KF|6,   KF|9,  KF|10,   '\\',     No, 
     No,     No,     No,     No,     No,     No,     No,     No, 
   Left,     No,  Break,     Up,   Home,     No,   Pgup,     No,
   Down,     No,     No,     No,     No,  KF|11, Pgdown,     No,
  Right,     No,     No,    End,    '`',  KF|12,     No,     No, 
     No,   KF|7,    '.',     No,   KF|8,    '9',    'l',    'o', 
    '/',    '[',     No,   '\'',    '-',    '0',    ';',    'p',
     No,    ']',    ',',     No,    '=',    '8',    'k',    'i',
    'n',    'y',    'm',    'h',    '6',    '7',    'j',    'u', 
};

uchar kbmap_shift[128] = {
     No,     No,     No,     No,    Del,     No,     No,     No, 
     No,     No,    'X',   KF|5,   KF|1,    '@',    'S',    'W', 
     No,   KF|3,    'C',   KF|4,   KF|2,    '#',    'D',    'E', 
     No,   '\t',    'Z',    Esc,     No,    '!',    'A',    'Q', 
    'B',    'T',    'V',    'G',    '%',    '$',    'F',    'R',
  Latin,     No,     No,       No,     No,     No,     No, Scroll, 
     No,     No,     No,     No,     No,     No,     No,     No, 
    ' ',   '\b',   '\n',   KF|6,   KF|9,  KF|10,    '|',     No, 
     No,     No,     No,     No,     No,     No,     No,     No, 
   Left,     No,   No,     Up,   Home,     No,   Pgup,     No,
   Down,     No,     No,     No,     No,  KF|11, Pgdown,     No,
  Right,     No,     No,    End,    '~',  KF|12,     No,     No, 
     No,   KF|7,    '>',     No,   KF|8,    '(',    'L',    'O', 
    '?',    '{',     No,   '\"',    '_',    ')',    ':',    'P',
     No,    '}',    '<',     No,    '+',    '*',    'K',    'I',
    'N',    'Y',    'M',    'H',    '^',    '&',    'J',    'U', 
};


/*******************Interface-specific I/O********************/

static long
kbd_getscancode(void)
{
	UartReg *uart = UARTREG(2);
	static	uchar	key;
	static	uchar	pos = 0;
	uchar	ch;
	long	stat1;

	while ( (stat1 = uart->utsr1) & 0x0002) {

		if(stat1 & 0x0038) {
			ch = uart->utdr;;
			USED(ch);
			continue;
		}

		ch = uart->utdr;
		switch ( pos ) {
		case 0 :	/* Packet 1 */
			key = (ch&0x40)<<1;	/* press = 0, release = 0x80 */
			if((ch&0xbf) == 0x94)	/* check for press/release */
				pos++;
			break;
		default :	/* Packet 2 */
			pos = 0;
			if ( ch == 0x0 ) 	// all keys released:
				memset(keybitmap, 0, sizeof(keybitmap));
			else
				return ch | key;
		}
	}
	return -1;					/* no scancode */
}

void	
kbd_init(void)
{
	UartReg *uart = UARTREG(2);
	HsspReg *hssp = HSSPREG;

	uart->utcr0 = UTCR0_PE|UTCR0_DSS;	/* 8 Data, Odd Parity, 1 Stop*/
	uart->utcr1 = 0x0000;		/* 1280 Baud, hi             */
	uart->utcr2 = 0x00B3;		/* lo */
	uart->utcr3 = UTCR3_RXE | UTCR3_TXE;	/* Rx enabled.       */
	uart->utcr4 = 0x0000;		/* IR port is Normal UART    */

	hssp->hscr0 = 0x0000;		/* HSC turned off.           */
	hssp->hscr1 = 0x0000;		/* Address field = 0.        */

	memset(keybitmap, 0, sizeof(keybitmap));
	shiftbitmap = 0;			/* Initialize shift bitmap	*/
}

/*******************Generic keyboard processing***************/

static int kbd_char = -1;


static int
kbd_charav_now(void)
{
	static int repeat_count = 0;
	int c;

	if(kbd_char >= 0)
		return kbd_char;

	while ((c = kbd_getscancode()) >= 0) {
		if (c&0x80) 	/* key release */ {
			c &= 0x7f;
			KEYRELEASE(c);
			continue;
		} else {
			if(KEYISHIT(c)) {	/* key repeat */
				if(repeat_count++ < REPEAT_SPEED)
					continue;
			} else
				repeat_count = 0;
			KEYHIT(c);		
		}
		if(c == CapsLockKey) {
			CapsLock ^= 1;
			continue;
		}
		if (KEYISHIT(LeftShiftKey) || KEYISHIT(RightShiftKey)) 
			c = kbmap_shift[c];
		else
			c = kbmap[c];

		if(c == (uchar)No)
			continue;

		if(CapsLock && c >= 'a' && c <= 'z')
			c += 'A' - 'a';
		if(KEYISHIT(LeftCtrlKey) || KEYISHIT(RightCtrlKey)) 
			c &= 0x1f;
		/*
		if(KEYISHIT(LeftAltKey) || KEYISHIT(RightAltKey)) 
			c = ( c & 0x1f ) | APP;
		*/
		kbd_char = c;
		return c;
	}
	return -1;
}

int
kbd_charav(void)
{
	int c;
	ulong t = timer_start();
	do {
		if((c = kbd_charav_now()) >= 0)
			return c ? c : 0x100;
		delay(0);
	} while(timer_ticks(t) < MS2TMR(5));
	return 0;
}

int
kbd_getc(void)
{
	int c;
	while( (c = kbd_charav_now()) < 0)
		;
	kbd_char = -1;
	kbd_charav();			/* to hopefully get key release... */
	return c;
}

