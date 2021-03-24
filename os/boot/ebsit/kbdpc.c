#include <lib9.h>
#include "io.h"

static uchar keybitmap[128/8];

#define KEYHIT(k)	keybitmap[(k)>>3] |= (1<<((k)&7))
#define KEYRELEASE(k)	keybitmap[(k)>>3] &= ~(1<<((k)&7))
#define KEYISHIT(k)	(keybitmap[(k)>>3] & (1<<((k)&7)))

#define  No 0

#define KEY_FN		0x21
#define KEY_LSHFT	0x2a
#define KEY_RSHFT	0x36
#define KEY_LCTRL	0x1d
#define KEY_RCTRL	0x1d
#define KEY_ALT		0x38
#define KEY_CAPS	0x3a
#define KEY_CPLK	0x70	/* needs to be unused scancode */

uchar kbmap[128] = {
[0x00]	No,	0x1b,	'1',	'2',	'3',	'4',	'5',	'6',
[0x08]	'7',	'8',	'9',	'0',	'-',	'=',	'\b',	'\t',
[0x10]	'q',	'w',	'e',	'r',	't',	'y',	'u',	'i',
[0x18]	'o',	'p',	'[',	']',	'\n',	No,	'a',	's',
[0x20]	'd',	'f',	'g',	'h',	'j',	'k',	'l',	';',
[0x28]	'\'',	'`',	No,	'\\',	'z',	'x',	'c',	'v',
[0x30]	'b',	'n',	'm',	',',	'.',	'/',	No,	'*',
[0x38]	No,	' ',	No,	No,	No,	No,	No,	No,
[0x40]	No,	No,	No,	No,	No,	No,	No,	'7',
[0x48]	'8',	'9',	'-',	'4',	'5',	'6',	'+',	'1',
[0x50]	'2',	'3',	'0',	'.',	No,	No,	No,	No,
};

uchar kbmap_shft[128] = {
[0x00]	No,	0x1b,	'!',	'@',	'#',	'$',	'%',	'^',
[0x08]	'&',	'*',	'(',	')',	'_',	'+',	'\b',	'\t',
[0x10]	'Q',	'W',	'E',	'R',	'T',	'Y',	'U',	'I',
[0x18]	'O',	'P',	'{',	'}',	'\n',	No,	'A',	'S',
[0x20]	'D',	'F',	'G',	'H',	'J',	'K',	'L',	':',
[0x28]	'"',	'~',	No,	'|',	'Z',	'X',	'C',	'V',
[0x30]	'B',	'N',	'M',	'<',	'>',	'?',	No,	'*',
[0x38]	No,	' ',	No,	No,	No,	No,	No,	No,
[0x40]	No,	No,	No,	No,	No,	No,	No,	'7',
[0x48]	'8',	'9',	'-',	'4',	'5',	'6',	'+',	'1',
[0x50]	'2',	'3',	'0',	'.',	No,	No,	No,	No,
};


/* 82C106 */
#define KBD_STAT	0x22
#define KBD_CMD		0x22
#define KBD_DATA	0x20

/* status */
#define	KBD_OBF		0x01
#define	KBD_IBF		0x02
#define	KBD_KBEN	0x10
#define	KBD_ODS		0x20
#define	KBD_GTO		0x40
#define	KBD_PERR	0x80

/* kbd ctl */
#define	KBD_CTL_TEST1	0xAA
#define	KBD_CTL_TEST2	0xAB
#define	KBD_CTL_ENABLE	0xAE
#define	KBD_CTL_RDOUT	0xD0
#define	KBD_CTL_WROUT	0xD1
#define KBD_CTL_WRTMODE 0x60
#define KBD_CTL_RDMODE  0x20

#define KBD_EKI 0x01	/* enable keyboard interrupt */
#define KBD_EMI	0x02	/* enable mouse intterupt */
#define KBD_SYS 0x04	/* system flag */
#define KBD_DKB 0x10	/* disable keyboard */
#define KBD_DMS 0x20	/* disable mouse */
#define KBD_KCC 0x40	/* scan code set 1 */


/* kbd */
#define	KBD_RESET	0xFF
#define	KBD_MAKEBREAK	0xFC
#define	KBD_SETLEDS	0xED
#define	KBD_SELECTCODE	0xF0
#define	KBD_ENABLE	0xF4
#define	KBD_ACK		0xFA
#define	KBD_RESEND	0xFE
#define KBD_SETDEFAULT  0xF6


static void
kbd_cmd(int c)
{
	while(inb(KBD_STAT)&KBD_IBF)
		;
	outb(KBD_CMD, c);
}

static void
kbd_output(int c)
{
	while(inb(KBD_STAT)&KBD_IBF)
		;
	outb(KBD_DATA, c);
}

static int
kbd_input(void)
{
	while(!(inb(KBD_STAT)&KBD_OBF))
		;
	return inb(KBD_DATA);
}

static void
kbd_setled(int on)
{
	kbd_output(KBD_SETLEDS);
	if(on)
		kbd_output(0x04);
	else
		kbd_output(0x00);
}


static char kbd_char = 0; 


static int
kbd_charav_now(void)
{
	int s, c;
	if(kbd_char)
		return kbd_char;
	while((c = inb(KBD_STAT))&KBD_OBF) {
		s = inb(KBD_DATA);
		if(c&(KBD_PERR|KBD_GTO|KBD_ODS))
			continue;
		c = 0;
		if(s&0x80) 	/* key up */
			KEYRELEASE(s&0x7f);
		else {	/* key down */
			KEYHIT(s);
			if(s == KEY_CAPS) {
				if(KEYISHIT(KEY_CPLK)) {
					KEYRELEASE(KEY_CPLK);
					kbd_setled(0);
				} else {
					KEYHIT(KEY_CPLK);
					kbd_setled(1);
				}
			} else if(KEYISHIT(KEY_LSHFT) || KEYISHIT(KEY_RSHFT)) {
				if(s < sizeof kbmap_shft)
				c = kbmap_shft[s];
			} else {
				if(s < sizeof kbmap)
					c = kbmap[s];
			}
			if(KEYISHIT(KEY_CPLK) && c >= 'a' && c <= 'z')
				c += 'A' - 'a';
			if(KEYISHIT(KEY_LCTRL) || KEYISHIT(KEY_RCTRL))
				c &= 0x1f;
			if(c) {
				kbd_char = c;
				return c;
			}
		}
	}
	return 0;
}

int
kbd_charav(void)
{
	int c, i;
	for(i=0; i<16; i++) {
		if((c = kbd_charav_now()))
			return c;
		delay(1);
	}
	return 0;
}

int
kbd_getc(void)
{
	int c;
	while(!kbd_charav_now())
		;
	c = kbd_char;
	kbd_char = 0;
	return c;
}


void
system_reset(void)
{
	int i, x;
	print("My mind is going Dave...  ");
	kbd_cmd(0xfe);
	x = 0xdf;
	for(i=0; i < 5; i++) {
		x ^= 1;
		kbd_cmd(0xd1);
		kbd_output(x);
		delay(100);
	}
	print("I'm feeling much better now.");
}

void
kbd_init(void)
{
	int c;
	kbd_cmd(KBD_CTL_RDMODE);
	c = kbd_input();
	c &= ~(KBD_EKI | KBD_DKB | KBD_EMI);
	c |= KBD_DMS | KBD_KCC | KBD_SYS;
	kbd_cmd(KBD_CTL_WRTMODE);
	kbd_output(c);
	memset(keybitmap, 0, sizeof keybitmap);	
	kbd_setled(0);
}

