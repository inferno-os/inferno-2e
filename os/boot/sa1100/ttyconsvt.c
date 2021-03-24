#include <lib9.h>
#include <keyboard.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "../port/ttystream.h"
#include "../port/screen.h"
#include "../port/vmode.h"
#include "../port/error.h"
#include "kbd.h"

extern void *vd;
extern int rgb2cmap(int, int, int);

#define VTPARAM	void
#define VTPARAM_C
#define VTARG
#define VTARG_C
#include "../port/vt.h"

static struct vtstate vt;
int nlcr = 1;

#define VT_PUTCHAR(vt,x,y,ch)	screen_putchar(x*fontwid, y*fonthgt, ch)
#define VT_SCROLL_UP(vt,x1,y1,x2,y2,n) scrup(x1,y1,x2,y2,n)
#define VT_SCROLL_DOWN(vt,x1,y1,x2,y2,n) scrdn(x1,y1,x2,y2,n)
#define VT_SCROLL_RIGHT(vt,x1,y1,x2,y2,n) scrrt(x1,y1,x2,y2,n)
#define VT_SCROLL_LEFT(vt,x1,y1,x2,y2,n) scrlt(x1,y1,x2,y2,n)
#define VT_CLEAR(vt,x1,y1,x2,y2) sclear(x1,y1,x2,y2)
#define VT_SET_COLOR(vt) scolor()
#define VT_SET_CURSOR(vt,x,y)
#define VT_BEEP(vt) sbeep()
#define VT_TYPE(vt,b,n) kbdput((uchar*)b,n)

#define VT_WID	text_cols
#define VT_HGT	text_rows
#define VT_X	text_x
#define VT_Y	text_y
#define VT_DX	text_wid
#define VT_DY	text_hgt
#define VTS	vt
#define VT_NLCR	nlcr

static int vtc[16];

static void
sbeep(void)
{
	screen_xfillbox(0,0,vd_wid-1,vd_hgt-1,0xff,0xff);
	delay(10);
	screen_xfillbox(0,0,vd_wid-1,vd_hgt-1,0xff,0xff);
}

static void
scolor(void)
{
	int fg = VTS.fg&0xf;
	int bg = VTS.bg&0xf;
	if(VTS.attr&BIT(7)) {
		int t = fg;
		fg = bg;
		bg = t;
	}
	if(VTS.attr&BIT(1)) 
		fg |= 8;
	text_fg = vtc[fg];
	text_bg = vtc[bg];
}

static void
scrup(int x1, int y1, int x2, int y2, int n)
{
	x1 *= fontwid;
	x2 = (x2+1)*fontwid-1;
	y1 *= fonthgt;
	y2 = (y2+1)*fonthgt-1;
	n *= fonthgt;
	screen_copy(x1, y1+n, x2, y2, x1, y1);
	screen_fillbox(x1, y2-n+1, x2, y2, text_bg);
}

static void
scrdn(int x1, int y1, int x2, int y2, int n)
{
	x1 *= fontwid;
	x2 = (x2+1)*fontwid-1;
	y1 *= fonthgt;
	y2 = (y2+1)*fonthgt-1;
	n *= fonthgt;
	screen_copy(x1, y1, x2, y2-n, x1, y1+n);
	screen_fillbox(x1, y1, x2, y1+n-1, text_bg);
}

static void
scrlt(int x1, int y1, int x2, int y2, int n)
{
	x1 *= fontwid;
	x2 = (x2+1)*fontwid-1;
	y1 *= fonthgt;
	y2 = (y2+1)*fonthgt-1;
	n *= fontwid;
	screen_copy(x1+n, y1, x2, y2, x1, y1);
	screen_fillbox(x2-n+1, y1, x2, y2, text_bg);
}

static void
scrrt(int x1, int y1, int x2, int y2, int n)
{
	x1 *= fontwid;
	x2 = (x2+1)*fontwid-1;
	y1 *= fonthgt;
	y2 = (y2+1)*fonthgt-1;
	n *= fontwid;
	screen_copy(x1, y1, x2-n, y2, x1+n, y1);
	screen_fillbox(x1, y1, x1+n-1, y2, text_bg);
}

static void
sclear(int x1, int y1, int x2, int y2)
{
	screen_fillbox(x1*fontwid, y1*fonthgt,
			(x2+1)*fontwid-1, (y2+1)*fonthgt-1, text_bg);
}

uchar kbuf[128];
int kin = 0;
int kout = 0;

static void
kbdput(uchar *b, int n)
{
	while(--n >= 0) {
		kbuf[kin++] = *b++;
		if(kin >= sizeof kbuf)
			kin = 0;
	}
}

static void
kbdcheck(void)
{
	if(kbd_charav()) {
		uchar c = kbd_getc();
		char *cp = (char*)&c;
		switch(c) {
		case '\n':
			cp = "\r";
			break;	
		case '\r':
			cp = "\n";
			break;
		case Del:
			cp = "\177";
			break;
		case Up:
			cp = "\033[A";
			break;
		case Down:
			cp = "\033[B";
			break;
		case Right:
			cp = "\033[C";
			break;
		case Left:
			cp = "\033[D";
			break;
		case Ins:
			cp = "\033[2~";
			break;
		case Pgup:
			cp = "\033[5~";
			break;
		case Pgdown:
			cp = "\033[6~";
			break;
		case Home:
			cp = "\033[H";
			break;
		case End:
			cp = "\033[F";
			break;
		default:
			kbdput(&c, 1);
			return;
		}
		kbdput((uchar*)cp, strlen(cp));
	}
}

static int
kbdget(void)
{
	int c;
	kbdcheck();
	if(kin != kout) {
		c = kbuf[kout++];
		if(kout >= sizeof kbuf)
			kout = 0;
		return c;
	}
	return -1;
}

vidcon_write(Ostream *, void *va, int len)
{
	char *s = (char*)va;
	if(!vd)
		return 0;
	screen_cursor(0);
	kbdcheck();
	vt_write(s, len);
	kbdcheck();
	screen_cursor(1);
	screen_flush();
	return len;
}


static Ostream _vidcon_ostream = { vidcon_write, 0, 0 };


static int
kbd_read(Istream *, void *va, int len)
{
	uchar *a = (uchar*)va;
	int c;
	if(len == 0)
		return 0;
	if(len < 0)
		len = -len;
	else {
		while((c = kbdget()) < 0)
			delay(0);
		*a++ = c;
		--len;
	}
	while(--len >= 0) {
		if((c = kbdget()) < 0)
			break;
		*a++ = c;
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
ttyconsvtlink()
{
	extern Vmode default_vmode;
	int i;

	kbd_init();
	if(tty_openi(&_tty_istream, rconin, conout, TTYSTREAM_ECHO) >= 0)
		conin = &_tty_istream;

	if(setscreen(&default_vmode) < 0) 
		print("setscreen: %r\n");

	for(i=0; i<16; i++) {
		int v = (i&8) ? 255 : 192;
		vtc[i] = rgb2cmap((i&1) ? v : 0, (i&2) ? v : 0, (i&4) ? v : 0);
	}
	vt_init();

	stdin = conin;
	stdout = conout;
}

#define sprintf sprint
#include "../port/vt.c"

