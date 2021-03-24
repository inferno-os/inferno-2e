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

static ulong storage;
static Point hotpoint;

static void
s3page(int)
{
}

static void
s3init(Mode* mode)
{
	uchar x;
	ulong aperture;
	int size;
	Pcidev *p;

	if(p = pcimatch(nil, 0x5333, 0)){
		aperture = p->mem[0].bar & ~0x0F;
		size = p->mem[0].size;
	}
	else{
		aperture = 0;
		size = 2*1024*1024;
	}
	aperture = upamalloc(aperture, size, 0);
	if(aperture == 0)
		return;

	mode->aperture = aperture;
	mode->apsize = mode->x*((1<<mode->d)/BI2BY)*mode->y;
	for(mode->apshift = 0; mode->apshift < 31; mode->apshift++){
		if((1<<mode->apshift) >= mode->apsize)
			break;
	}
	mode->apsize = 1<<mode->apshift;

	vgaxo(Crtx, 0x31, 0x85|0x08);
	x = vgaxi(Crtx, 0x32);
	vgaxo(Crtx, 0x32, x|0x40);
	x = vgaxi(Crtx, 0x33);
	vgaxo(Crtx, 0x33, x|0x20);
	vgaxo(Crtx, 0x34, 0x10);
	vgaxo(Crtx, 0x35, 0x00);
	x = vgaxi(Crtx, 0x3A);
	vgaxo(Crtx, 0x3A, x|0x10);

	x = vgaxi(Crtx, 0x40);
	vgaxo(Crtx, 0x40, x|0x01);

	/*
	 * Now set the aperture registers.
	 */
	aperture = aperture>>16;
	vgaxo(Crtx, 0x59, (aperture>>8) & 0xFF);
	vgaxo(Crtx, 0x5A, aperture & 0xFF);
	vgaxo(Crtx, 0x58, 0x98|0x02);
}

static int
s3ident(void)
{
	uchar crt30;

	/*
	 * 0xA5 ensures Crt36 and Crt37 are also unlocked
	 * (0xA0 unlocks everything else).
	 */
	vgaxo(Crtx, 0x38, 0x48);
	vgaxo(Crtx, 0x39, 0xA5);

	crt30 = vgaxi(Crtx, 0x30);
	if(crt30 == 0xE1){					/* Trio64 and later */
		//crt2E = vgaxi(Crtx, 0x2E);
		//if(crt2E == 0x10 || crt2E == 0x11)
			return 1;
	}
	else if(crt30 == 0xA0)					/* S3801 */
		return 1;

	return 0;
}

static void
vsyncactive(void)
{
	/*
	 * Hardware cursor information is fetched from display memory
	 * during the horizontal blank active time. The 80x chips may hang
	 * if the cursor is turned on or off during this period.
	 */
	while((vgai(Status1) & 0x08) == 0)
		;
}

static void
disable(void)
{
	uchar crt45;

	/*
	 * Turn cursor off.
	 */
	crt45 = vgaxi(Crtx, 0x45) & 0xFE;
	vsyncactive();
	vgaxo(Crtx, 0x45, crt45);
}

static void
enable(void)
{
	int i;

	disable();

	/*
	 * Cursor colours. Set both the CR0[EF] and the colour
	 * stack in case we are using a 16-bit RAMDAC.
	 * Why are these colours reversed?
	 */
	vgaxo(Crtx, 0x0E, Pwhite);
	vgaxo(Crtx, 0x0F, Pblack);
	vgaxi(Crtx, 0x45);
	for(i = 0; i < 4; i++)
		vgaxo(Crtx, 0x4A, Pwhite);
	vgaxi(Crtx, 0x45);
	for(i = 0; i < 4; i++)
		vgaxo(Crtx, 0x4B, Pblack);

	/*
	 * Find a place for the cursor data in display memory.
	 * Must be on a 1024-byte boundary.
	 */
	storage = (gscreen.width*BY2WD*gscreen.r.max.y+1023)/1024;
	vgaxo(Crtx, 0x4C, (storage>>8) & 0x0F);
	vgaxo(Crtx, 0x4D, storage & 0xFF);
	storage *= 1024;

	/*
	 * Enable the cursor in X11 mode.
	 */
	vgaxo(Crtx, 0x55, vgaxi(Crtx, 0x55)|0x10);
	vsyncactive();
	vgaxo(Crtx, 0x45, 0x01);
}

static void
load(Cursor* c)
{
	uchar *p;
	int x, y;

	/*
	 * Disable the cursor and
	 * set the pointer to the two planes.
	 */
	disable();

	p = ((uchar*)gscreen.data->data) + storage;

	/*
	 * The cursor is set in X11 mode which gives the following
	 * truth table:
	 *	and xor	colour
	 *	 0   0	underlying pixel colour
	 *	 0   1	underlying pixel colour
	 *	 1   0	background colour
	 *	 1   1	foreground colour
	 * Put the cursor into the top-left of the 64x64 array.
	 *
	 * The cursor pattern in memory is interleaved words of
	 * AND and XOR patterns.
	 */
	for(y = 0; y < 64; y++){
		for(x = 0; x < 64/8; x += 2){
			if(x < 16/8 && y < 16){
				*p++ = c->clr[2*y + x]|c->set[2*y + x];
				*p++ = c->clr[2*y + x+1]|c->set[2*y + x+1];
				*p++ = c->set[2*y + x];
				*p++ = c->set[2*y + x+1];
			}
			else {
				*p++ = 0x00;
				*p++ = 0x00;
				*p++ = 0x00;
				*p++ = 0x00;
			}
		}
	}

	/*
	 * Set the cursor hotpoint and enable the cursor.
	 */
	hotpoint = c->offset;
	vsyncactive();
	vgaxo(Crtx, 0x45, 0x01);
}

static void
move(int cx, int cy)
{
	int x, xo, y, yo;

	/*
	 * Mustn't position the cursor offscreen even partially,
	 * or it disappears. Therefore, if x or y is -ve, adjust the
	 * cursor offset instead.
	 * There seems to be a bug in that if the offset is 1, the
	 * cursor doesn't disappear off the left edge properly, so
	 * round it up to be even.
	 */
	if((x = cx+hotpoint.x) < 0){
		xo = -x;
		xo = ((xo+1)/2)*2;
		x = 0;
	}
	else
		xo = 0;
	if((y = cy+hotpoint.y) < 0){
		yo = -y;
		y = 0;
	}
	else
		yo = 0;

	vgaxo(Crtx, 0x46, (x>>8) & 0x07);
	vgaxo(Crtx, 0x47, x & 0xFF);
	vgaxo(Crtx, 0x49, y & 0xFF);
	vgaxo(Crtx, 0x4E, xo);
	vgaxo(Crtx, 0x4F, yo);
	vgaxo(Crtx, 0x48, (y>>8) & 0x07);
}

Vgac vgas3 = {
	"s3",
	s3page,
	s3init,
	s3ident,

	enable,
	disable,
	move,
	load,

	0,
};
