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
#include "vga.h"

#define CURSWID	16
#define CURSHGT 16

enum {
	Backgnd = 0xFF,	/* black */
	Foregnd =	0x00,	/* white */
};


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

void
swcurs_move(SWcursor *swc, int nx, int ny)
{
	swcurs_hide(swc);
	swc->x = nx;
	swc->y = ny;
	swcurs_unhide(swc);
}

void
swcurs_update(SWcursor *swc, Rectangle r)
{
	Rectangle cr;
	cr.min.x = swc->x-swc->hotx;
	cr.min.y = swc->y-swc->hoty;
	cr.max.x = cr.min.x + swc->cwid;
	cr.max.y = cr.min.y + swc->chgt;
	if(rectclip(&cr, r))
		swcurs_hide(swc);
	else {
		swc->hidecount = 0;
		swcurs_draw_or_undraw(swc);
	}	
}

