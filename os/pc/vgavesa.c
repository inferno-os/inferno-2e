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

int swcursor = 1;

extern Memimage gscreen;

static ulong storage;
static Point hotpoint;
static SWcursor *swc = nil;

typedef struct VesaInfo VesaInfo;
struct VesaInfo {
	uchar	attr;
	uchar	_[15];
	ushort	span;
	ushort	width;
	ushort	height;
	uchar	_[3];
	uchar	depth;
	uchar	_[3];
	uchar	pages;
	uchar	_[10];
	ulong	base;
};

static void
vesapage(int)
{
}

static void
vesainit(Mode* mode)
{
	VesaInfo *vi = (VesaInfo*)APBOOTSTRAP;	/* for now */
	ulong aperture;

	mode->x = vi->width;
	mode->y = vi->height;
	mode->d = 3;
	aperture = vi->base;

	mode->apsize = mode->x*((1<<mode->d)/BI2BY)*mode->y;
	mode->apshift = 0;
	aperture = upamalloc(aperture, mode->apsize, 0);
	if(aperture == 0)
		return;
	mode->aperture = aperture;

	if(swc)
		swcurs_destroy(swc);
	swc = swcurs_create(KADDR(aperture), vi->span>>2, mode->d, 
		(Rectangle){(Point){0,0}, (Point){mode->x, mode->y}}, 1);
}

static int
vesaident(void)
{
	VesaInfo *vi = (VesaInfo*)APBOOTSTRAP;	/* for now */
	return (vi->depth == 8 && (vi->attr & 0x99) == 0x99 && vi->span == vi->width);
}

static void
disable(void)
{
	swcurs_disable(swc);
}

static void
enable(void)
{
	swcurs_enable(swc);
}

static void
load(Cursor* c)
{
	swcurs_load(swc, c);
}

static void
move(int cx, int cy)
{
	swcurs_move(swc, cx, cy);
}

static void
update(Rectangle r)
{
	swcurs_update(swc, r);
}

Vgac vgavesa = {
	"vesa",
	vesapage,
	vesainit,
	vesaident,

	enable,
	disable,
	move,
	load,
	update,
	1,
};
