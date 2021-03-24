#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "../port/error.h"
#include "io.h"

#include <image.h>
#include <memimage.h>
#include <memlayer.h>
#include <cursor.h>

#include "screen.h"

enum {
	Backgnd = 0xFF,	/* black */
	Foregnd =	0x00,	/* white */
};

#define	DPRINT	if(0)iprint

extern Vmode default_vmode;


ulong	consbits = (Foregnd<<24)|(Foregnd<<16)|(Foregnd<<8)|Foregnd;
Memdata consdata = {
	nil,
	&consbits
};
Memimage conscol =
{
	{ 0, 0, 1, 1 },
	{ -100000, -100000, 100000, 100000 },
	3,
	1,
	&consdata,
	0,
	1
};

ulong	onesbits = ~0;
Memdata	onesdata = {
	nil,
	&onesbits,
};
Memimage	xones =
{
	{ 0, 0, 1, 1 },
	{ -100000, -100000, 100000, 100000 },
	3,
	1,
	&onesdata,
	0,
	1
};

Memimage *memones = &xones;

ulong	zerosbits = 0;
Memdata	zerosdata = {
	nil,
	&zerosbits,
};

Memimage	xzeros =
{
	{ 0, 0, 1, 1 },
	{ -100000, -100000, 100000, 100000 },
	3,
	1,
	&zerosdata,
	0,
	1
};
Memimage *memzeros = &xzeros;

ulong	backbits = (Backgnd<<24)|(Backgnd<<16)|(Backgnd<<8)|Backgnd;
Memdata	backdata = {
	nil,
	&backbits,
};
Memimage	xback =
{
	{ 0, 0, 1, 1 },
	{ -100000, -100000, 100000, 100000 },
	3,
	1,
	&backdata,
	0,
	1
};
Memimage *back = &xback;

static Memsubfont *memdefont;
Memimage gscreen;
Memdata	gscreendata;
static Point curpos;
static Rectangle window;

typedef struct SWcursor SWcursor;

static Vdisplay *vd;
static SWcursor *swc = nil;
static int screen_ready = 0;

SWcursor* swcurs_create(ulong *, int, int, Rectangle, int);
void swcurs_destroy(SWcursor*);
void swcurs_enable(SWcursor*);
void swcurs_disable(SWcursor*);
void swcurs_hide(SWcursor*);
void swcurs_unhide(SWcursor*);
void swcurs_load(SWcursor*, Cursor*);

static char printbuf[1024];
static int printbufpos = 0;

static Cursor arrow = {
	{ -1, -1 },
	{ 0xFF, 0xFF, 0x80, 0x01, 0x80, 0x02, 0x80, 0x0C,
	  0x80, 0x10, 0x80, 0x10, 0x80, 0x08, 0x80, 0x04,
	  0x80, 0x02, 0x80, 0x01, 0x80, 0x02, 0x8C, 0x04,
	  0x92, 0x08, 0x91, 0x10, 0xA0, 0xA0, 0xC0, 0x40,
	},
	{ 0x00, 0x00, 0x7F, 0xFE, 0x7F, 0xFC, 0x7F, 0xF0,
	  0x7F, 0xE0, 0x7F, 0xE0, 0x7F, 0xF0, 0x7F, 0xF8,
	  0x7F, 0xFC, 0x7F, 0xFE, 0x7F, 0xFC, 0x73, 0xF8,
	  0x61, 0xF0, 0x60, 0xE0, 0x40, 0x40, 0x00, 0x00,
	},
};

static Cursor crosshair_black = {
	{ 4, 4 },
	{ 0x1c,0x00, 0x14,0x00, 0x14,0x00, 0xf7,0x80,
	  0x80,0x80, 0xf7,0x80, 0x14,0x00, 0x14,0x00,
	  0x1c,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	  0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	},
	{ 0x00,0x00, 0x08,0x00, 0x08,0x00, 0x08,0x00,
	  0x77,0x00, 0x08,0x00, 0x08,0x00, 0x08,0x00,
	  0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	  0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	},
};

static Cursor crosshair_white = {
	{ 4, 4 },
	{ 0x00,0x00, 0x08,0x00, 0x08,0x00, 0x08,0x00,
	  0x77,0x00, 0x08,0x00, 0x08,0x00, 0x08,0x00,
	  0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	  0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	},
	{ 0x1c,0x00, 0x14,0x00, 0x14,0x00, 0xf7,0x80,
	  0x80,0x80, 0xf7,0x80, 0x14,0x00, 0x14,0x00,
	  0x1c,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	  0x00,0x00, 0x00,0x00, 0x00,0x00, 0x00,0x00,
	},
};

void	lcdchkstatus(void);

static void
dosetcolor(ulong p, ulong r, ulong g, ulong b)
{
	long nr,ng,nb;
	long contrast = vd->contrast>>1;
	long brightness = (vd->brightness-0x8000)*2;

	if(conf.cansetcontrast) {
		brightness = 0;
		contrast = 0x4000;
	}

	nr = (((r>>16)*contrast)>>14) + brightness;
	if(nr < 0) nr = 0;
	else if(nr > 0xffff) nr = 0xffff;
	ng = (((g>>16)*contrast)>>14) + brightness;
	if(ng < 0) ng = 0;
	else if(ng > 0xffff) ng = 0xffff;
	nb = (((b>>16)*contrast)>>14) + brightness;
	if(nb < 0) nb = 0;
	else if(nb > 0xffff) nb = 0xffff;

	lcd_setcolor(p, nr<<16, ng<<16, nb<<16);
}

int
setcolor(ulong p, ulong r, ulong g, ulong b)
{
	if(vd->d == 3)
		p &= 0xff;
	else
		p &= 0xf;
	vd->colormap[p][0] = r;
	vd->colormap[p][1] = g;
	vd->colormap[p][2] = b;
	dosetcolor(p, r, g, b);
	return ~0;
}

void
getcolor(ulong p, ulong *pr, ulong *pg, ulong *pb)
{
	if(vd->d == 3)
		p = (p&0xff)^0xff;
	else
		p = (p&0xf)^0xf;
	*pr = vd->colormap[p][0];
	*pg = vd->colormap[p][1];
	*pb = vd->colormap[p][2];
}

static void
refreshpalette(void)
{
	int i;
	if(conf.cansetcontrast)
		return;
	for(i=0; i<256; i++)
		dosetcolor(i,
			vd->colormap[i][0],
			vd->colormap[i][1],
			vd->colormap[i][2]);
}

void
setbrightness(int b)
{
	vd->brightness = b;
	if(conf.cansetcontrast)
		lcd_setbrightness(b);
	else
		refreshpalette();
}

void
setcontrast(int c)
{
	vd->contrast = c;
	if(conf.cansetcontrast)
		lcd_setcontrast(c);
	else
		refreshpalette();
}

int
getbrightness(void)
{
	return vd->brightness;
}

int
getcontrast(void)
{
	return vd->contrast;
}

void
graphicscmap(int invert)
{
	int num, den, i, j;
	int r, g, b, cr, cg, cb, v, p;

	for(r=0,i=0;r!=4;r++) for(v=0;v!=4;v++,i+=16){
		for(g=0,j=v-r;g!=4;g++) for(b=0;b!=4;b++,j++){
			den=r;
			if(g>den) den=g;
			if(b>den) den=b;
			if(den==0)	/* divide check -- pick grey shades */
				cr=cg=cb=v*17;
			else{
				num=17*(4*den+v);
				cr=r*num/den;
				cg=g*num/den;
				cb=b*num/den;
			}
			p = (i+(j&15))^(invert ? 0xff : 0);
			if(vd->d == 2) {
				if((p&0xf) != (p>>4))
					continue;
				p &= 0xf;
			}
			setcolor(p,
				cr*0x01010101,
				cg*0x01010101,
				cb*0x01010101);
		}
	}
}

static uchar lum[256]={
  0,   7,  15,  23,  39,  47,  55,  63,  79,  87,  95, 103, 119, 127, 135, 143,
154,  17,   9,  17,  25,  49,  59,  62,  68,  89,  98, 107, 111, 129, 138, 146,
157, 166,  34,  11,  19,  27,  59,  71,  69,  73,  99, 109, 119, 119, 139, 148,
159, 169, 178,  51,  13,  21,  29,  69,  83,  75,  78, 109, 120, 131, 128, 149,
 28,  35,  43,  60,  68,  75,  83, 100, 107, 115, 123, 140, 147, 155, 163,  20,
 25,  35,  40,  47,  75,  85,  84,  89, 112, 121, 129, 133, 151, 159, 168, 176,
190,  30,  42,  44,  50,  90, 102,  94,  97, 125, 134, 144, 143, 163, 172, 181,
194, 204,  35,  49,  49,  54, 105, 119, 103, 104, 137, 148, 158, 154, 175, 184,
 56,  63,  80,  88,  96, 103, 120, 128, 136, 143, 160, 168, 175, 183,  40,  48,
 54,  63,  69,  90,  99, 107, 111, 135, 144, 153, 155, 173, 182, 190, 198,  45,
 50,  60,  70,  74, 100, 110, 120, 120, 150, 160, 170, 167, 186, 195, 204, 214,
229,  55,  66,  77,  79, 110, 121, 131, 129, 165, 176, 187, 179, 200, 210, 219,
 84, 100, 108, 116, 124, 140, 148, 156, 164, 180, 188, 196, 204,  60,  68,  76,
 82,  91, 108, 117, 125, 134, 152, 160, 169, 177, 195, 204, 212, 221,  66,  74,
 80,  89,  98, 117, 126, 135, 144, 163, 172, 181, 191, 210, 219, 228, 238,  71,
 76,  85,  95, 105, 126, 135, 145, 155, 176, 185, 195, 205, 225, 235, 245, 255,
};

void flushmemscreen(Rectangle r);

/*
 * BUG: calls to this can be replaced by calls to xspanalloc once the virtual-physical mapping is redesigned:
 * KADDR should not be the identity map but invert the mmu mapping (in effect only)
 * so that the x* routines use true physical addresses as intended and pool*
 * routines allocate virtual addresses
 */
void*
screenalloc(ulong size)
{
	void *va, *tries[5];
	int i, n;

	for(n=0; n < nelem(tries); n++){
		tries[n] = va = xalloc(size);
		if(PADDR(va)+size-4 == PADDR((ulong)va+size-4)) {
			for(i=0; i<n; i++)
				xfree(tries[i]);
			segflush(va, size);
			return va;
		}
	}
	panic("xscreenalloc");
	return nil;
}

void
screenclear(void)
{
	int width;

	width = gscreen.r.max.x * (1 <<gscreen.ldepth )/BI2WD;
	memset(gscreendata.data, Backgnd, width*BY2WD*gscreen.r.max.y);
	curpos = window.min;
	flushmemscreen(gscreen.r);
}

void
setscreen(Vmode *mode)
{
	int h;

	if(swc)
		swcurs_destroy(swc);

	vd = nil;
	if(!(mode->flags & (VMODE_BIGEND|VMODE_LILEND)))
		mode->flags |= VMODE_BIGEND;	/* we can only deal with
			big-endian pixel layout, (because of the devdraw code),
			so we try to force it into that mode, but still allow
		 	the mode parameters to override it, in case someone
			has a good reason to do so, like a mis-wired LCD
			display or something) */

	vd = lcd_init(mode);

	if(vd == nil)
		panic("unsupported vmode");
		// panic(Ebadarg);

	if(lum[255] == 255) {
		int i;
		for(i=0; i<256; i++)
			lum[i] >>= 4;	/* could support depths other than 4 */
	}

	gscreen.r.min = Pt(0, 0);
	gscreen.r.max = Pt(vd->wid, vd->hgt);
	gscreen.clipr = gscreen.r;
	gscreen.ldepth = vd->d < 3 ? 3 : vd->d;
	gscreen.repl = 0;
	gscreen.width = ((vd->wid << gscreen.ldepth)+31) >> 5;
	if(!vd->fb) {
		if(!(vd->fb = screenalloc(((vd->wid*(1<<vd->d))>>5)*vd->hgt*BY2WD)))
			panic("can't alloc vidmem");
		if(conf.usebabycache)
			*(ulong*)&(vd->fb) |= 0x10000000;	/* baby cache */
	}
	if(vd->d == 2) {	/* use 8 to 4 bit fakeout for speed */
		if(!(gscreendata.data = poolalloc(imagmem,
					gscreen.width*vd->hgt*BY2WD)))
			panic("can't alloc vidmem");
		if(conf.usebabycache)
			*(ulong*)&gscreendata.data |= 0x10000000; /* baby cache */
	} else
		gscreendata.data = (ulong*)vd->fb;

	DPRINT("vd->wid=%d bwid=%d gscreen.width=%d fb=%ux data=%ux\n",
		vd->wid, vd->bwid, gscreen.width, vd->fb, gscreendata.data);
	h = memdefont->height;
	gscreen.data = &gscreendata;
	graphicscmap(1);
	setbrightness(vd->brightness);
	setcontrast(vd->contrast);
	window = gscreen.r;
	window.max.x = vd->wid;
	window.max.y = (vd->hgt/h) * h;
	screenclear();

	swc = swcurs_create(gscreendata.data, gscreen.width, gscreen.ldepth, gscreen.r, 1);

	drawcursor(nil);
}

void
screenon(int clr)
{
	if (!screen_ready)
		return;
	if (clr)
		screenclear();
	setbrightness(MAX_VBRIGHTNESS);
}

void
screeninit(void)
{
	lcd_setbacklight(1);
	memdefont = getmemdefont();
	setscreen(&default_vmode);
	screen_ready = 1;
	if(printbufpos)
		screenputs("", 0);
	lcdchkstatus();
/*
	DPRINT("font height: %d width: %d %d %d %d\n",
		memdefont->height,
		memsubfontwidth(memdefont, " ").x,
		memsubfontwidth(memdefont, "0").x,
		memsubfontwidth(memdefont, "i").x,
		memsubfontwidth(memdefont, "w").x);
	DPRINT("Screen size: %dx%d (%dx%d)\n",
		gscreen.r.max.x, gscreen.r.max.y,
		gscreen.r.max.x/memsubfontwidth(memdefont, "X").x,
		gscreen.r.max.y/memdefont->height);
*/
}

ulong*
attachscreen(Rectangle *r, int *ld, int *width, int *softscreen)
{
	*r = gscreen.r;
	*ld = gscreen.ldepth;
	*width = gscreen.width;
	*softscreen = (gscreendata.data != (ulong*)vd->fb);

	return gscreendata.data;
}

static void
flush8to4(Rectangle r, ulong *s, int sw, ulong *d, int dw)
{
	int i, h, w;

	/*
	print("1) s=%ux sw=%d d=%ux dw=%d r=(%d,%d)(%d,%d)\n",
		s, sw, d, dw, r.min.x, r.min.y, r.max.x, r.max.y);
	*/
	r.min.x &= ~7;
	r.max.x = (r.max.x + 7) & ~7;
	s += (r.min.y*sw)+(r.min.x>>2);
	d += (r.min.y*dw)+(r.min.x>>3);
	h = Dy(r);
	w = Dx(r) >> 3;
	sw -= w*2;
	dw -= w;

	while(h--) {
		for(i=w; i; i--) {
			register ulong v1 = *s++;
			register ulong v2 = *s++;
			*d++ = 	 (lum[v2>>24]<<28)
				|(lum[(v2>>16)&0xff]<<24)
				|(lum[(v2>>8)&0xff]<<20)
				|(lum[v2&0xff]<<16)
				|(lum[v1>>24]<<12)
				|(lum[(v1>>16)&0xff]<<8)
				|(lum[(v1>>8)&0xff]<<4)
				|(lum[v1&0xff])
				;
		}
		s += sw;
		d += dw;
	}
 }

void
flushmemscreen(Rectangle r)
{
	if(rectclip(&r, gscreen.r) == 0)
		return;
	if(r.min.x == r.max.x || r.min.y == r.max.y)
		return;
	if(gscreendata.data != (ulong*)vd->fb)
		flush8to4(r, gscreendata.data, gscreen.width, (ulong*)vd->fb, vd->bwid >> 2);
	lcd_flush();
}

static void
scroll(void)
{
	int o;
	Point p;
	Rectangle r;

	o = 4*memdefont->height;
	r = Rpt(window.min, Pt(window.max.x, window.max.y-o));
	p = Pt(window.min.x, window.min.y+o);
	memdraw(&gscreen, r, &gscreen, p, memones, p);
	r = Rpt(Pt(window.min.x, window.max.y-o), window.max);
	memdraw(&gscreen, r, back, memzeros->r.min, memones, memzeros->r.min);

	curpos.y -= o;
}

static void
clearline(void)
{
	Rectangle r;
	int yloc = curpos.y;

	r = Rpt(Pt(window.min.x, window.min.y + yloc),
		Pt(window.max.x, window.min.y+yloc+memdefont->height));
	memdraw(&gscreen, r, back, memzeros->r.min, memones, memzeros->r.min);
}

static void
screenputc(char *buf)
{
	Point p;
	int h, w, pos;
	Rectangle r;
	static int *xp;
	static int xbuf[256];

	h = memdefont->height;
	if(xp < xbuf || xp >= &xbuf[sizeof(xbuf)])
		xp = xbuf;

	switch(buf[0]) {
	case '\n':
		if(curpos.y+h >= window.max.y)
			scroll();
		curpos.y += h;
		/* fall through */
	case '\r':
		xp = xbuf;
		curpos.x = window.min.x;
		break;
	case '\t':
		if(curpos.x == window.min.x)
			clearline();
		p = memsubfontwidth(memdefont, " ");
		w = p.x;
		*xp++ = curpos.x;
		pos = (curpos.x-window.min.x)/w;
		pos = 8-(pos%8);
		curpos.x += pos*w;
		break;
	case '\b':
		if(xp <= xbuf)
			break;
		xp--;
		r = Rpt(Pt(*xp, curpos.y), Pt(curpos.x, curpos.y + h));
		memdraw(&gscreen, r, back, back->r.min, memones, back->r.min);
		curpos.x = *xp;
		break;
	case '\0':
		/*
		 * printing NUL doesn't work with memimagestring(), which is string-based.
		 * silently ignore it for now; should probably try to print a substitute
		 */
		break;
	default:
		p = memsubfontwidth(memdefont, buf);
		w = p.x;

		if(curpos.x >= window.max.x-w)
			screenputc("\n");

		if(curpos.x == window.min.x)
			clearline();
		if(xp < xbuf+nelem(xbuf))
			*xp++ = curpos.x;
		memimagestring(&gscreen, curpos, &conscol, memdefont, buf);
		curpos.x += w;
	}
}

static void
screenpbuf(char *s, int n)
{
	if(printbufpos+n > sizeof(printbuf))
		n = sizeof(printbuf)-printbufpos;
	if(n > 0) {
		memmove(&printbuf[printbufpos], s, n);
		printbufpos += n;
	}
}

static void
screendoputs(char *s, int n)
{
	int i;
	Rune r;
	char buf[4];

	while(n > 0) {
		i = chartorune(&r, s);
		if(i == 0){
			s++;
			--n;
			continue;
		}
		memmove(buf, s, i);
		buf[i] = 0;
		n -= i;
		s += i;
		screenputc(buf);
	}
}

void
screenflush(void)
{
	int j = 0;
	int k;

	for (k = printbufpos; j < k; k = printbufpos) {
		screendoputs(printbuf + j, k - j);
		j = k;
	}
	printbufpos = 0;
}

void
screenputs(char *s, int n)
{
	static Proc *me;

	if(!screen_ready) {
		screenpbuf(s, n);
		return;
	}

	if(!canlock(&vd->lock)) {
		/* don't deadlock trying to print in interrupt */
		/* don't deadlock trying to print while in print */
		if((islo() == 0) || (up && (up == me)))
		{
			/* save it for later... */
			/* In some cases this allows seeing a panic message
			  that would be locked out forever */
			screenpbuf(s, n);
			return;
		}
		lock(&vd->lock);
	}

	me = up;
	if (printbufpos)
		screenflush();
	screendoputs(s, n);
	if (printbufpos)
		screenflush();
	me = nil;

	/* should be updated to keep track of the minimum bounding
	   rectangle for the last puts: */
	flushmemscreen(gscreen.r);
	unlock(&vd->lock);
}

int
vmodematch(Vmode *v1, Vmode *v2)
{
	if(v1->wid && v2->wid && v1->wid != v2->wid)
		return 0;
	if(v1->hgt && v2->hgt && v1->hgt != v2->hgt)
		return 0;
	if(v1->d && v2->d && v1->d != v2->d)
		return 0;
	if(v1->hz && v2->hz && v1->hz != v2->hz)
		return 0;
	if((v1->flags&v2->flags) != v2->flags)
		return 0;
	return 1;
}

/*
 *	Software cursor code.  Interim version (for baseline).
 *	we may want to replace code here by memdraw primitives.
 */
 
typedef struct SWcursor {
	ulong	*fb;	/* screen frame buffer */
	Rectangle r;
	int	d;	/* ldepth of screen */
	int 	width;	/* width of screen in ulongs */
	int	x;
	int	y;
	int	hotx;
	int	hoty;
	uchar	cbwid;	/* cursor byte width */
	uchar	f;	/* flags */
	uchar	cwid;
	uchar	chgt;
	int	hidecount;
	uchar	data[CURSWID*CURSHGT];
	uchar	mask[CURSWID*CURSHGT];
	uchar	save[CURSWID*CURSHGT];
} SWcursor;

enum {
	CUR_ENA = 0x01,		/* cursor is enabled */
	CUR_DRW = 0x02,		/* cursor is currently drawn */
	CUR_SWP = 0x10,		/* bit swap */
};

static Rectangle cursoroffrect;
static int	cursorisoff;

static void swcursorflush(int, int);
static void	swcurs_draw_or_undraw(SWcursor *);

static void
cursorupdate0(void)
{
	int inrect, x, y;

	x = mouse.x - swc->hotx;
	y = mouse.y - swc->hoty;
	inrect = (x >= cursoroffrect.min.x && x < cursoroffrect.max.x
		&& y >= cursoroffrect.min.y && y < cursoroffrect.max.y);
	if (cursorisoff == inrect)
		return;
	cursorisoff = inrect;
	if (inrect)
		swcurs_hide(swc);
	else {
		swc->hidecount = 0;
		swcurs_draw_or_undraw(swc);
	}
	swcursorflush(mouse.x, mouse.y);
}

void
cursorupdate(Rectangle r)
{
	lock(&vd->lock);
	r.min.x -= 16;
	r.min.y -= 16;
	cursoroffrect = r;
	if (swc)
		cursorupdate0();
	unlock(&vd->lock);
}

void
cursorenable(void)
{
	lock(&vd->lock);
	if(swc) {
		swcurs_enable(swc);
		swcursorflush(mouse.x, mouse.y);
	}
	unlock(&vd->lock);
}

void
cursordisable(void)
{
	lock(&vd->lock);
	if(swc) {
		swcurs_disable(swc);
		swcursorflush(mouse.x, mouse.y);
	}
	unlock(&vd->lock);
}

void
mousetrack(int b, int dx, int dy)
{
	ulong tick;
	int d, x, y;
	static ulong lastt;
	int oldx, oldy;

	x = mouse.x + dx;
	if(x < gscreen.r.min.x)
		x = gscreen.r.min.x;
	if(x >= gscreen.r.max.x)
		x = gscreen.r.max.x;
	y = mouse.y + dy;
	if(y < gscreen.r.min.y)
		y = gscreen.r.min.y;
	if(y >= gscreen.r.max.y)
		y = gscreen.r.max.y;

	tick = MACHP(0)->ticks;
	d = mouse.b ^ b;
	if(d && (d & mouse.b)) {
		if(tick - lastt < MS2TK(300))
			b |= (1<<4);
		lastt = tick;
	}

	oldx = mouse.x;
	oldy = mouse.y;
	mouse.x = x;
	mouse.y = y;
	mouse.b = b;
	mouse.modify = 1;

	if(!canlock(&vd->lock))
		return;		/* if can't lock, don't wake up stuff */

	if(swc) {
		swcurs_hide(swc);
		swc->x = x;
		swc->y = y;
		cursorupdate0();
		swcurs_unhide(swc);
		swcursorflush(oldx, oldy);
		swcursorflush(x, y);
	}

	unlock(&vd->lock);
	wakeup(&mouse.r);
}

void
drawcursor(Drawcursor* c)
{
	Point p;
	Cursor curs, *cp;
	int j, i, h, bpl;
	uchar *bc, *bs, *cclr, *cset;

	if(!swc)
		return;

	/* Set the default system cursor */
	if(!c || c->data == nil)
		cp = &arrow /*&crosshair_black*/;
	else {
		cp = &curs;
		p.x = c->hotx;
		p.y = c->hoty;
		cp->offset = p;
		bpl = bytesperline(Rect(c->minx, c->miny, c->maxx, c->maxy), 0);

		h = (c->maxy-c->miny)/2;
		if(h > 16)
			h = 16;

		bc = c->data;
		bs = c->data + h*bpl;

		cclr = cp->clr;
		cset = cp->set;
		for(i = 0; i < h; i++) {
			for(j = 0; j < 2; j++) {
				cclr[j] = bc[j];
				cset[j] = bs[j];
			}
			bc += bpl;
			bs += bpl;
			cclr += 2;
			cset += 2;
		}
	}

	if(swc) {
		swcurs_load(swc, cp);
		swcursorflush(mouse.x, mouse.y);
	}
}

SWcursor*
swcurs_create(ulong *fb, int width, int ldepth, Rectangle r, int bitswap)
{
	SWcursor *swc = (SWcursor*)malloc(sizeof(SWcursor));
	swc->fb = fb;
	swc->r = r;
	swc->d = ldepth;
	swc->width = width;
	swc->f = bitswap ? CUR_SWP : 0;
	swc->x = swc->y = 0;
	swc->hotx = swc->hoty = 0;
	swc->hidecount = 0;
	return swc;
}

void
swcurs_destroy(SWcursor *swc)
{
	swcurs_disable(swc);
	free(swc);
}

static void
swcursorflush(int x, int y)
{
	Rectangle r;

	/* XXX a little too paranoid here */
	r.min.x = x-16;
	r.min.y = y-16;
	r.max.x = x+17;
	r.max.y = y+17;
	flushmemscreen(r);
}

static void
swcurs_draw_or_undraw(SWcursor *swc)
{
	uchar *p;
	uchar *cs;
	int w, vw;
	int x1 = swc->r.min.x;
	int y1 = swc->r.min.y;
	int x2 = swc->r.max.x;
	int y2 = swc->r.max.y; 
	int xp = swc->x - swc->hotx;
	int yp = swc->y - swc->hoty;
	int ofs;

	if(((swc->f & CUR_ENA) && (swc->hidecount <= 0))
			 == ((swc->f & CUR_DRW) != 0))
		return;
	w = swc->cbwid*BI2BY/(1 << swc->d);
	x1 = xp < x1 ? x1 : xp;
	y1 = yp < y1 ? y1 : yp;
	x2 = xp+w >= x2 ? x2 : xp+w;
	y2 = yp+swc->chgt >= y2 ? y2 : yp+swc->chgt;
	if(x2 <= x1 || y2 <= y1)
		return;
	p = (uchar*)(swc->fb + swc->width*y1)
		+ x1*(1 << swc->d)/BI2BY;
	y2 -= y1;
	x2 = (x2-x1)*(1 << swc->d)/BI2BY;
	vw = swc->width*BY2WD - x2;
	w = swc->cbwid - x2;
	ofs = swc->cbwid*(y1-yp)+(x1-xp);
	cs = swc->save + ofs;
	if((swc->f ^= CUR_DRW) & CUR_DRW) {
		uchar *cm = swc->mask + ofs; 
		uchar *cd = swc->data + ofs;
		while(y2--) {
			x1 = x2;
			while(x1--) {
				*p = ((*cs++ = *p) & *cm++) ^ *cd++;
				p++;
			}
			cs += w;
			cm += w;
			cd += w;
			p += vw;
		}
	} else {
		while(y2--) {
			x1 = x2;
			while(x1--) 
				*p++ = *cs++;
			cs += w;
			p += vw;
		}
	}
}

void
swcurs_hide(SWcursor *swc)
{
	++swc->hidecount;
	swcurs_draw_or_undraw(swc);
}

void
swcurs_unhide(SWcursor *swc)
{
	if (--swc->hidecount < 0)
		swc->hidecount = 0;
	swcurs_draw_or_undraw(swc);
}

void
swcurs_enable(SWcursor *swc)
{
	swc->f |= CUR_ENA;
	swcurs_draw_or_undraw(swc);
}

void
swcurs_disable(SWcursor *swc)
{
	swc->f &= ~CUR_ENA;
	swcurs_draw_or_undraw(swc);
}

void
swcurs_load(SWcursor *swc, Cursor *c)
{
	int i, k;
	uchar *bc, *bs, *cd, *cm;
	static uchar bdv[4] = {0,Backgnd,Foregnd,0xff};
	static uchar bmv[4] = {0xff,0,0,0xff};
	int bits = 1<<swc->d;
	uchar mask = (1<<bits)-1;
	int bswp = (swc->f&CUR_SWP) ? 8-bits : 0;

	bc = c->clr;
	bs = c->set;

	swcurs_hide(swc);
	cd = swc->data;
	cm = swc->mask;
	swc->hotx = c->offset.x;
	swc->hoty = c->offset.y;
	swc->chgt = CURSHGT;
	swc->cwid = CURSWID;
	swc->cbwid = CURSWID*(1<<swc->d)/BI2BY;
	for(i = 0; i < CURSWID/BI2BY*CURSHGT; i++) {
		uchar bcb = *bc++;
		uchar bsb = *bs++;
		for(k=0; k<BI2BY;) {
			uchar cdv = 0;
			uchar cmv = 0;
			int z;
			for(z=0; z<BI2BY; z += bits) {
				int n = ((bsb&(0x80))|((bcb&(0x80))<<1))>>7;
				int s = z^bswp;
				cdv |= (bdv[n]&mask) << s;
				cmv |= (bmv[n]&mask) << s;
				bcb <<= 1;
				bsb <<= 1;
				k++;
			}
			*cd++ = cdv;
			*cm++ = cmv;
		}
	}
	swcurs_unhide(swc);
}
