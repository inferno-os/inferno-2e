#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"

#include <image.h>
#include <memimage.h>
#include <memlayer.h>

#include "screen.h"
#include "vmode.h"
#include "vctlr.h"

// NOTE:  Nothing in this file should need to be modified for 
// specific video controllers.  

extern Vmode default_vmode;

Vdisplay *vd = nil;

static Vctlr *vctlrs;
static Vctlr *vctlr;

extern uchar *font;

int text_fg = 0x00;		/* white */
int text_bg = 0xc9;		/* dark blue */
int text_cursor = 0xc0;		/* greenish */
int text_rows, text_cols;
int text_x, text_y;
int text_wid = 1;
int text_hgt = 1;
int vd_wid;
int vd_hgt;
uchar *vd_fb;


/* luminance table from Rob Pike: */
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

static uchar lum16[65536];

static void
dosetcolor(ulong p, ulong r, ulong g, ulong b)
{
	long nr,ng,nb;
	long contrast = vd->contrast>>1;
	long brightness = (vd->brightness-0x8000)*2;

	// if(vd->vctlr->setbrightness)
	//	brightness = 0;
	if(vd->vctlr->setcontrast)
		contrast = (MAX_VCONTRAST/2+1)>>1;

	nr = (((r>>16)*contrast)>>14) + brightness;
	if(nr < 0) nr = 0;
	else if(nr > 0xffff) nr = 0xffff;
	ng = (((g>>16)*contrast)>>14) + brightness;
	if(ng < 0) ng = 0;
	else if(ng > 0xffff) ng = 0xffff;
	nb = (((b>>16)*contrast)>>14) + brightness;
	if(nb < 0) nb = 0;
	else if(nb > 0xffff) nb = 0xffff;
	vd->vctlr->setcolor(p, nr<<16, ng<<16, nb<<16);
}

int
setcolor(ulong p, ulong r, ulong g, ulong b)
{
	if(!vd->vctlr->setcolor)
		return 0;
	vd->colormap[p][0] = r;
	vd->colormap[p][1] = g;
	vd->colormap[p][2] = b;
	dosetcolor(p, r, g, b);
	return ~0;	/* but WHY? what does it MEAN? */
}


static void
refreshpalette(void)
{
	int i;
	// if(vd->vctlr->setbrightness && vd->vctlr->setcontrast)
	//	return;
	for(i=0; i<256; i++) 
		dosetcolor(i,
			vd->colormap[i][0],
			vd->colormap[i][1],
			vd->colormap[i][2]);
}


void
setbrightness(ushort b)
{
	vd->brightness = b;
	// if(vd->vctlr->setbrightness)
	//	vd->vctlr->setbrightness(b);
	// else
		refreshpalette();
}
		
void
setcontrast(ushort c)
{
	vd->contrast = c;
	if(vd->vctlr->setcontrast)
		vd->vctlr->setcontrast(c);		
	else
		refreshpalette();
}

ushort
getbrightness(void) { return vd->brightness; }

ushort
getcontrast(void) { return vd->contrast; }


int
setvideohz(int hz)
{
	if(vd->vctlr->sethz) {
		vd->vctlr->sethz(hz);
		return 1;
	} else
		return 0;
}

int
getvideohz(void) { return vd->hz; }


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
			setcolor(p,
				cr*0x01010101,
				cg*0x01010101,
				cb*0x01010101);
		}
	}
}

int
setscreen(Vmode *mode)
{
	/* place an order for little endian pixel order,
	 * since it's the only thing this code can deal with:
	 */
	mode->flags |= VMODE_LILEND;

	if(vctlr)
		vd = vctlr->init(mode);
	else {
		if(!(vctlr = vctlrs)) {
			error("no vctlrs");
			return -1;
		}
		while(vctlr && !(vd = vctlr->init(mode)))
			vctlr = vctlr->link;
	}
	if(vd == nil) {
		char estr[ERRLEN];
		sprint(estr, "setscreen: no support for %dx%dx%d hz=%d f=%ux",
			mode->wid, mode->hgt, 1<<mode->d,
			mode->hz, mode->flags);
		error(estr);
		return -1;
	}

	if(lum[255] == 255) {
		int i;
		for(i=0; i<256; i++)
			lum[i] >>= 4;	/* change this for other ldepths */
		for(i=0; i<65536; i++)
			lum16[i] = lum[i&0xff]|(lum[i>>8]<<4);
	}

	vd_wid = vd->wid;
	vd_hgt = vd->hgt;
	vd->vctlr = vctlr;
	if(vd->d == 3) 
		vd_fb = vd->fb; 
	else 
		vd_fb = malloc(vd_wid*vd_hgt);
		// vd_fb = (uchar*)malloc(vd_wid*vd_hgt)+0x10000000;
	print("vd_fb=%ux fb=%ux d=%d\n", vd_fb, vd->fb, vd->d);

	if(vctlr->setbrightness) {
		vctlr->setbrightness(vd->brightness);
		vd->brightness = (vd->brightness+1)>>1;
	}
	screen_clear(-1);
	text_x = 0;
	text_y = 0;
	text_rows = vd_hgt/fonthgt - 2;
	text_cols = vd_wid/fontwid;
	graphicscmap(1);
	setbrightness(vd->brightness);
	setcontrast(vd->contrast);
	return 0;
}



static void
flush8to4(ulong *s, ulong *d, int n)
{
	n >>= 3;	// convert pixel count to final longword count
	while(n--) {
		register ulong v1 = *s++;
		register ulong v2 = *s++;
/*
		*d++ = 	 (lum[v2>>24]<<28)
			|(lum[(v2>>16)&0xff]<<24)
			|(lum[(v2>>8)&0xff]<<20)
			|(lum[v2&0xff]<<16)
			|(lum[v1>>24]<<12)
			|(lum[(v1>>16)&0xff]<<8)
			|(lum[(v1>>8)&0xff]<<4)
			|(lum[v1&0xff])
			;
*/
		*d++ = (lum16[v2>>16]<<24)
		      |(lum16[v2&0xffff]<<16)
		      |(lum16[v1>>16]<<8)
		      |(lum16[v1&0xffff]);
	}
}

void
screen_flush(void)
{
	if(vd == nil)
		return;
	if(vd->d != 3)
		flush8to4((ulong*)vd_fb, (ulong*)vd->fb, vd_wid*vd_hgt);
	if(vd->vctlr->flush)
		vd->vctlr->flush(vd->fb, vd_wid, vd->d,
			(Rectangle){(Point){0, 0}, (Point){vd_wid, vd_hgt}});
}

void
screen_copy(int sx1, int sy1, int sx2, int sy2, int dx1, int dy1)
{
	int w = sx2-sx1+1;
	int n = sy2-sy1+1;
	int k = vd_wid;
	uchar *s, *d;
	if(sy1 < dy1) {
		dy1 += (n-1);
		sy1 = sy2;
		k = -k;
	}
	s = vd_fb + sy1*vd_wid+sx1;
	d = vd_fb + dy1*vd_wid+dx1;
	delay(0);
	while(n--) {
		memmove(d, s, w);
		s += k;
		d += k;
		delay(0);
	}
}

void
screen_putchar(int x, int y, int c)
{
	uchar *a = vd_fb + vd_wid*y;
	uchar *fp = &font[((c&0x7f)-32)*fonthgt];
	int i, j, k, m;
	int cw = fontwid*text_wid;

	a += x;
	delay(0);
	for(i=0; i<fonthgt; i++, fp++) for(k=0; k<text_hgt; k++) {
		uchar d = *fp;
		for(j=0; j<fontwid; j++, d <<= 1) for(m=0; m<text_wid; m++) {
			if((d^c)&0x80)
				*a = text_fg;
			else if(*a == text_fg)
				*a = text_bg;
			a++;
		}
		a += vd_wid-cw;
	}
	delay(0);
}

void
screen_putstr(int x, int y, const char *s)
{
	while(*s) {
		screen_putchar(x, y, *s++);
		x += fontwid*text_wid;
	}		
}


int
screen_getpixel(int x, int y)
{
	uchar *a = vd_fb + vd_wid*y;
	return a[x];
}

void
screen_putpixel(int x, int y, int c)
{
	uchar *a = vd_fb + vd_wid*y;
	a[x] = c; 
}


void
screen_xhline(int x1, int x2, int y, int c, int m)
{
	uchar *a = vd_fb + vd_wid*y;
	a += x1;
	while(x1 <= x2) {
		*a = (*a & m) ^ c;
		a++;
		x1++;
	}
}

void
screen_hline(int x1, int x2, int y, int c)
{
	screen_xhline(x1, x2, y, c, 0);
}

void
screen_xfillbox(int x1, int y1, int x2, int y2, int c, int m)
{
	for(; y1 <= y2; y1++) {
		screen_xhline(x1, x2, y1, c, m);
		delay(0);
	}
}

void
screen_fillbox(int x1, int y1, int x2, int y2, int c)
{
	screen_xfillbox(x1, y1, x2, y2, c, 0);
}

void
screen_clear(int c)
{
	screen_cursor(0);
	if(c < 0)
		c = text_bg;
	memset(vd_fb, c, vd_wid*vd_hgt);
	text_x = 0;
	text_y = 0;
}


void
screen_cursor(int on)
{
	static int ison = 0;
	static int curswid;
	static int curshgt;
	if(ison != on) {
		if(on) {
			curswid = fontwid*text_wid;
			curshgt = fonthgt*text_hgt;
		}
		screen_xfillbox(text_x*fontwid, text_y*fonthgt,
			text_x*fontwid+curswid-1,
			text_y*fonthgt+curshgt-1,
			text_cursor^text_bg, 0xff);
		ison = on;
	}
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

void
addvctlrlink(Vctlr *v)
{
	v->link = vctlrs;
	vctlrs = v;
}

void*
screenalloc(ulong size)
{
	return malloc(size);
}

