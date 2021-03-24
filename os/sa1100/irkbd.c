/*
 *  Shannon IR keyboard
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "io.h"
#include "fns.h"
#include  	"keyboard.h"

static int	irkbdputc(Queue *, int);
static void	sprkbdputc(int);

/*
 *  KeyMatrix Valid For:	Sejin Electron Inc. SWK-8630
 *  P/N's: SPR-8695WRA, SPR-8696W/WT/WS, SPR-8638W/WT,
 *		   SPR-8698W/WT, SPR-8697WT
 */

char *kscanid = "swk8630";

Rune kbtab[] = 
{	
[0x00]	No,	No,	No,	No,	Del,	No,	No,	No,	
[0x08]	No,	Caps,	'x',	KF|5,	KF|1,	'2',	's',	'w',	
[0x10]	No,	KF|3,	'c',	KF|4,	KF|2,	'3',	'd',	'e',	
[0x18]	LAlt,	'\t',	'z',	Esc,	LCtrl,	'1',	'a',	'q',	
[0x20]	'b',	't',	'v',	'g',	'5',	'4',	'f',	'r',
[0x28]	RAlt,	Ins,	No,	Latin,	No,	SysRq,	No,	No,	
[0x30]	No,	LShift,	RShift,	No,	No,	No,	No,	No,	
[0x38]	' ',	'\b',	'\n',	KF|6,	KF|9,	KF|10,	'\\',	No,	
[0x40]	No,	No,	No,	No,	No,	No,	No,	No,	
[0x48]	Left,	No,	Break,	Up,	Home,	No,	Pgup,	No,
[0x50]	Down,	No,	KF|13,	No,	RCtrl,	KF|11,	Pgdown,	No,
[0x58]	Right,	No,	No,	End,	'`',	KF|12,	No,	No,	
[0x60]	No,	KF|7,	'.',	No,	KF|8,	'9',	'l',	'o',	
[0x68]	'/',	'[',	No,	'\'',	'-',	'0',	';',	'p',
[0x70]	No,	']',	',',	No,	'=',	'8',	'k',	'i',
[0x78]	'n',	'y',	'm',	'h',	'6',	'7',	'j',	'u',	
};

Rune kbtabshift[] =
{	
[0x00]	No,	No,	No,	No,	KF|15,	No,	No,	No,	
[0x08]	No,	Caps,	'X',	KF|5,	KF|1,	'@',	'S',	'W',	
[0x10]	No,	KF|3,	'C',	KF|4,	KF|2,	'#',	'D',	'E',	
[0x18]	LAlt,	BackTab,'Z',	Esc,	LCtrl,	'!',	'A',	'Q',	
[0x20]	'B',	'T',	'V',	'G',	'%',	'$',	'F',	'R',
[0x28]	RAlt, KF|14,	No,	Latin,	No,	Print,	No,	No,	
[0x30]	No,	LShift,	RShift,	No,	No,	No,	No,	No,	
[0x38]	' ',	'\b',	'\n',	KF|6,	KF|9,	KF|10,	'|',	No,	
[0x40]	No,	No,	No,	No,	No,	No,	No,	No,	
[0x48]	Left,	No,	Pause,	Up,	Home,	No,	Pgup,	No,
[0x50]	Down,	No,	KF|13,	No,	RCtrl,	KF|11,	Pgdown,	No,
[0x58]	Right,	No,	No,	End,	'~',	KF|12,	No,	No,	
[0x60]	No,	KF|7,	'>',	No,	KF|8,	'(',	'L',	'O',	
[0x68]	'?',	'{',	No,	'\"',	'_',	')',	':',	'P',
[0x70]	No,	'}',	'<',	No,	'+',	'*',	'K',	'I',
[0x78]	'N',	'Y',	'M',	'H',	'^',	'&',	'J',	'U',	
};


int pressed[NShifts];
int toggle[NShifts] = { [Caps-Shift]1, [Num-Shift]1, [Meta-Shift]1 };


void
kbdinit(void)
{
	HsspReg *hssp = HSSPREG;
	UartReg *uart = UARTREG(2);

	hssp->hscr0 = 0;	/* HSC turned off */
	hssp->hscr1 = 0;	/* clear address field */
	uart->utcr4 = 0;	/* IR port is normal UART */
	uartspecial(1, 1280, 'o', nil, nil, irkbdputc);  /* eia1 -> uart2 */
	if(kbdq == nil)
		kbdq = qopen(1024, 0, 0, 0);
}

static int
irkbdputc(Queue *, int c)
{
	static int pos, key, last, cnt;

	if (pos == 0) {
		key = (c&0x40)<<1;	/* press = 0, release = 0x80 */
		if((c&0xbf) == 0x94)	/* check for press/release */
			pos++;
		return 0;
	}

	pos = 0;
	if (c == 0x0) {		/* All keys up */
		last = cnt = 0;
		return 0;
	}

	if (key == 0) {
		if(c == last) {
			if(++cnt < 5)
				return 0;
		} else {
			cnt = 0;
			last = c;
		}
	}
	else if (c == last)
		last = cnt = 0;

	if(kscanq) {
		uchar ch = c|key;
		qproduce(kscanq, &ch, 1);
		return 0;
	}

	sprkbdputc(c|key);
	return 0;
}

static void
sprkbdputc(int c)
{
	int keyup = c&0x80;
	c &= 0x7f;

	if(pressed[LShift-Shift] || pressed[RShift-Shift])
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
