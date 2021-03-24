#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "../port/ttystream.h"
#include "../port/screen.h"
#include "../port/vmode.h"
#include "../port/error.h"
#include "kbd.h"

extern void *vd;

int
vidcon_write(Ostream *, void *va, int len)
{
	char *s = (char*)va;
	int i;
	if(!vd)
		return 0;
	if(len == 0) {
		screen_flush();
		return 0;
	}
	screen_cursor(0);
	for(i=0; i<len; i++) {
		switch(s[i]) {
		case '\b':
			if(text_x > 0)
				text_x -= text_wid;
			break;
		case '\t':
			text_x = (text_x&~7)+8;
			break;
		case '\r':
			text_x = 0;
			break;
		default:
			screen_putchar(text_x*fontwid,
					text_y*fonthgt, s[i]);
			if((text_x += text_wid)+text_wid <= text_cols)
				break;
			/* fall through */
		case '\n':
			text_x = 0;	/* nl=cr+nl */
			if((text_y += text_hgt)+text_hgt <= text_rows)
				break;
			screen_copy(0,fonthgt*8,
				text_cols*fontwid,text_rows*fonthgt,0,0);
			screen_fillbox(0,(text_rows-8)*fonthgt-1,
				text_cols*fontwid,text_rows*fonthgt-1,text_bg);
			text_y -= 8;
		}
	}
	screen_cursor(1);
	return len;
}


static Ostream _vidcon_ostream = { vidcon_write, 0, 0 };

static int
kbd_read(Istream *, void *va, int len)
{
	uchar *a = (uchar*)va;
	if(len < 0) {
		if(!kbd_charav())
			return 0;
		len = -len;
	}
	while(--len >= 0) {
		*a++ = kbd_getc();
		if(!kbd_charav())
			break;
	}
	return a-(uchar*)va;
}

static Istream _kbd_istream = { kbd_read, 0, 0 };
static TtyIstream _tty_istream;

Istream *rconin = &_kbd_istream;
Istream *conin = &_kbd_istream;
Ostream *conout = &_vidcon_ostream;


void
status(const char *msg, int sofar, int outof)
{
	status_bar(msg, sofar, outof);
}

void
statusclear(void)
{
	statusbar_erase();
}

int
interrupt(void)
{
	return kbd_charav();
}


void
ttyconslink()
{
	extern Vmode default_vmode;

	kbd_init();
	if(tty_openi(&_tty_istream, rconin, conout, TTYSTREAM_ECHO) >= 0)
		conin = &_tty_istream;

	if(setscreen(&default_vmode) < 0) 
		print("setscreen: %r\n");
	stdin = conin;
	stdout = conout;
}

