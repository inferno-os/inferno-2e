#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/error.h"

#include <image.h>
#include <memimage.h>
#include "vga.h"

extern Memimage gscreen;

static ulong aperture;
static ulong storage;
static Cursor curcursor;
static Lock devlock;

static uchar id[] = {
	0x88, 0x8C, 0x94, 0x80, 0x90, 0x98, 0x9C,
	0xA0, 0xA8, 0xAC, 0xB8,
	0,
};

static int
clgd542xpageset(int page)
{
	uchar gr09;
	int opage;
	
	if(vgaxi(Seqx, 0x07) & 0xF0)
		page = 0;
	gr09 = vgaxi(Grx, 0x09);
	if(vgaxi(Grx, 0x0B) & 0x20){
		vgaxo(Grx, 0x09, page<<2);
		opage = gr09>>2;
	}
	else{
		vgaxo(Grx, 0x09, page<<4);
		opage = gr09>>4;
	}

	return opage;
}

static void
clgd542xpage(int page)
{
	lock(&devlock);
	clgd542xpageset(page);
	unlock(&devlock);
}

static void
clgd542xinit(Mode* mode)
{
	int mem, size, x;
	Pcidev *p;
	uchar seq07;

	vgaxo(Seqx, 0x06, 0x12);	/* unlock */
	clgd542xpage(0);

	mem = 0;
	switch(vgaxi(Crtx, 0x27) & ~0x03){

	case 0x88:				/* CL-GD5420 */
	case 0x8C:				/* CL-GD5422 */
	case 0x94:				/* CL-GD5424 */
	case 0x80:				/* CL-GD5425 */
	case 0x90:				/* CL-GD5426 */
	case 0x98:				/* CL-GD5427 */
	case 0x9C:				/* CL-GD5429 */
		/*
		 * The BIOS leaves the memory size in Seq0A, bits 4 and 3.
		 * See Technical Reference Manual Appendix E1, Section 1.3.2.
		 *
		 * The storage area for the 64x64 cursors is the last 16Kb of
		 * display memory.
		 */
		mem = (vgaxi(Seqx, 0x0A)>>3) & 0x03;
		break;

	case 0xA0:				/* CL-GD5430 */
	case 0xA8:				/* CL-GD5434 */
	case 0xAC:				/* CL-GD5436 */
	case 0xB8:				/* CL-GD5446 */
		/*
		 * Attempt to intuit the memory size from the DRAM control
		 * register. Minimum is 512KB.
		 * If DRAM bank switching is on then there's double.
		 */
		x = vgaxi(Seqx, 0x0F);
		mem = (x>>3) & 0x03;
		if(x & 0x80)
			mem++;
		break;

	default:				/* uh, ah dunno */
		break;
	}

	storage = ((256<<mem)-16)*1024;

	seq07 = 1;			/* 256 Color mode */
	if(p = pcimatch(nil, 0x1013, 0)){
		aperture = p->mem[0].bar & ~0x0F;
		size = p->mem[0].size;
		aperture = upamalloc(aperture, size, 0);
		if(aperture){
			mode->aperture = aperture;
			mode->apsize = mode->x*((1<<mode->d)/BI2BY)*mode->y;
			for(mode->apshift = 0; mode->apshift < 31; mode->apshift++){
				if((1<<mode->apshift) >= mode->apsize)
					break;
			}
			mode->apsize = 1<<mode->apshift;

			seq07 |= 0xE0;	/* linear mode */
		}
	}
	aperture = mode->aperture;

	vgaxo(Seqx, 0x07, seq07);
	vgaxo(Crtx, 0x1B, 0x22);	/* set address wrap */
	if(storage > (1024*1024))
		vgaxo(Grx, 0x0B, 0x20);
	else
		vgaxo(Grx, 0x0B, 0x00);	/* clear extensions */

	if(mode->x == 1024 && mode->y == 768 && mode->d == 3){
		vgao(MiscW, 0xEF);
		vgaxo(Seqx, 0x0E, 0x76);
		vgaxo(Seqx, 0x1E, 0x34);

		x = vgaxi(Seqx, 0x0F);
		vgaxo(Seqx, 0x0F, 0x20|x);

		x = vgaxi(Seqx, 0x16);
		vgaxo(Seqx, 0x16, (x & 0xF0)|0x08);

		vgaxo(Crtx, 0x1A, 0xE0);
	}
	else if(mode->x == 800 && mode->y == 600 && mode->d == 3){
		vgao(MiscW, 0xEF);
		vgaxo(Seqx, 0x0E, 0x51);
		vgaxo(Seqx, 0x1E, 0x3A);

		x = vgaxi(Seqx, 0x0F);
		vgaxo(Seqx, 0x0F, x & ~0x20);

		x = vgaxi(Seqx, 0x16);
		vgaxo(Seqx, 0x16, (x & 0xF0)|0x08);

		vgaxo(Crtx, 0x1A, 0x00);
	}

	vgaxo(Seqx, 0x10, 0);		/* cursor to 0, 0 */
	vgaxo(Seqx, 0x11, 0);
}

static int
clgd542xident(void)
{
	uchar crt27;
	int i;

	crt27 = vgaxi(Crtx, 0x27) & ~0x03;
	for(i = 0; id[i]; i++){
		if(crt27 == id[i])
			return 1;
	}

	return 0;
}

static void
disable(void)
{
	uchar sr12;

	sr12 = vgaxi(Seqx, 0x12);
	vgaxo(Seqx, 0x12, sr12 & ~0x01);
}

static void
enable(void)
{
	uchar sr12;

	/*
	 * Disable the cursor.
	 */
	sr12 = vgaxi(Seqx, 0x12);
	vgaxo(Seqx, 0x12, sr12 & ~0x01);

	/*
	 * Cursor colours.  
	 */
	vgaxo(Seqx, 0x12, sr12|0x02);
	vgao(PaddrW, 0x00);
	vgao(Pdata, Pwhite);
	vgao(Pdata, Pwhite);
	vgao(Pdata, Pwhite);
	vgao(PaddrW, 0x0F);
	vgao(Pdata, Pblack);
	vgao(Pdata, Pblack);
	vgao(Pdata, Pblack);
	vgaxo(Seqx, 0x12, sr12);

	/*
	 * Set the current cursor to index 0
	 * and turn the 64x64 cursor on.
	 */
	vgaxo(Seqx, 0x13, 0);
	vgaxo(Seqx, 0x12, sr12|0x05);
}

static void
initcursor(Cursor* c, int xo, int yo, int index)
{
	uchar *p, seq07;
	uint p0, p1;
	int opage, x, y;

	/*
	 * Is linear addressing turned on? This will determine
	 * how we access the cursor storage.
	 */
	seq07 = vgaxi(Seqx, 0x07);
	opage = 0;
	p = KADDR(aperture);
	if(!(seq07 & 0xF0)){
		lock(&devlock);
		opage = clgd542xpageset(storage>>16);
		p += storage & 0xFFFF;
	}
	else
		p += storage;
	p += index*1024;

	for(y = yo; y < 16; y++){
		p0 = c->set[2*y];
		p1 = c->set[2*y+1];
		if(xo){
			p0 = (p0<<xo)|(p1>>(8-xo));
			p1 <<= xo;
		}
		*p++ = p0;
		*p++ = p1;

		for(x = 16; x < 64; x += 8)
			*p++ = 0x00;

		p0 = c->clr[2*y]|c->set[2*y];
		p1 = c->clr[2*y+1]|c->set[2*y+1];
		if(xo){
			p0 = (p0<<xo)|(p1>>(8-xo));
			p1 <<= xo;
		}
		*p++ = p0;
		*p++ = p1;

		for(x = 16; x < 64; x += 8)
			*p++ = 0x00;
	}
	while(y < 64+yo){
		for(x = 0; x < 64; x += 8){
			*p++ = 0x00;
			*p++ = 0x00;
		}
		y++;
	}

	if(!(seq07 & 0xF0)){
		clgd542xpageset(opage);
		unlock(&devlock);
	}
}

static void
load(Cursor* c)
{
	uchar sr12;

	/*
	 * Disable the cursor.
	 */
	sr12 = vgaxi(Seqx, 0x12);
	vgaxo(Seqx, 0x12, sr12 & ~0x01);

	memmove(&curcursor, c, sizeof(Cursor));
	initcursor(c, 0, 0, 0);

	/*
	 * Enable the cursor.
	 */
	vgaxo(Seqx, 0x13, 0);
	vgaxo(Seqx, 0x12, sr12|0x05);
}

static void
move(int cx, int cy)
{
	int index, x, xo, y, yo;

	/*
	 */
	index = 0;
	if((x = cx+curcursor.offset.x) < 0){
		xo = -x;
		x = 0;
	}
	else
		xo = 0;
	if((y = cy+curcursor.offset.y) < 0){
		yo = -y;
		y = 0;
	}
	else
		yo = 0;

	if(xo || yo){
		initcursor(&curcursor, xo, yo, 1);
		index = 1;
	}
	vgaxo(Seqx, 0x13, index<<2);
	
	vgaxo(Seqx, 0x10|((x & 0x07)<<5), (x>>3) & 0xFF);
	vgaxo(Seqx, 0x11|((y & 0x07)<<5), (y>>3) & 0xFF);
}

Vgac vgaclgd542x = {
	"clgd542x",
	clgd542xpage,
	clgd542xinit,
	clgd542xident,
	enable,
	disable,
	move,
	load,

	0,
};
