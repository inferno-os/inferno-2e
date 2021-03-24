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

/*
 * ATI Mach64CT.
 */
enum {					/* I/O select */
	HTotalDisp	= 0x00,
	HSyncStrtWid	= 0x01,
	VTotalDisp	= 0x02,
	VSyncStrtWid	= 0x03,
	VlineCrntVline	= 0x04,
	OffPitch	= 0x05,
	IntCntl		= 0x06,
	CrtcGenCntl	= 0x07,

	OvrClr		= 0x08,
	OvrWidLR	= 0x09,
	OvrWidTB	= 0x0A,

	CurClr0		= 0x0B,
	CurClr1		= 0x0C,
	CurOffset	= 0x0D,
	CurHVposn	= 0x0E,
	CurHVoff	= 0x0F,

	ScratchReg0	= 0x10,
	ScratchReg1	= 0x11,		/* Scratch Register (BIOS info) */
	ClockCntl	= 0x12,
	BusCntl		= 0x13,
	MemCntl		= 0x14,		/* Memory control */
	MemVgaWpSel	= 0x15,
	MemVgaRpSel	= 0x16,
	DacRegs		= 0x17,
	DacCntl		= 0x18,
	GenTestCntl	= 0x19,
	ConfigCntl	= 0x1A,		/* Configuration control */
	ConfigChipId	= 0x1B,
	ConfigStat0	= 0x1C,		/* Configuration status 0 */
	ConfigStat1	= 0x1D,		/* Configuration status 0 */

	Nreg		= 0x1E,
};

enum {
	PLLm		= 0x02,
	PLLp		= 0x06,
	PLLn2		= 0x09,

	Npll		= 16,
};

typedef struct {
	ulong	io;

	ulong	reg[Nreg];
	uchar	pll[Npll];
} Mach64ct;

static Mach64ct mp640x480x8 = {
	0x2EC,

	{
	0x004F0063,			/* HTotalDisp */
	0x000C0052,			/* HSyncStrtWid */
	0x01DF020C,			/* VTotalDisp */
	0x000201EA,			/* VSyncStrtWid */
	0x016A03FF,			/* VlineCrntVline */
	0x14000000,			/* OffPitch */
	0x00000000,			/* IntCntl */
	0x030B0200,			/* CrtcGenCntl */
	0x00000000,			/* OvrClr */
	0x00000000,			/* OvrWidLR */
	0x00000000,			/* OvrWidTB */
	0xFFFFFFFF,			/* CurClr0 */
	0x00000000,			/* CurClr1 */
	0x00009600,			/* CurOffset */
	0x0007009B,			/* CurHVposn */
	0x00300030,			/* CurHVoff */
	0x04100400,			/* ScratchReg0 */
	0x00000000,			/* ScratchReg1 */
	0x00000002,			/* ClockCntl */
	0x600000F9,			/* BusCntl */
	0x00000402,			/* MemCntl */
	0x00010000,			/* MemVgaWpSel */
	0x00010000,			/* MemVgaRpSel */
	0x00FF0001,			/* DacRegs */
	0x07006000,			/* DacCntl */
	0x00000188,			/* GenTestCntl */
	0x00000000,			/* ConfigCntl */
	0x09004354,			/* ConfigChipId */
	0x00000009,			/* ConfigStat0 */
	0x00000000,			/* ConfigStat1 */
	},

	{
	0x00, 0x60, 0x2D, 0x14,		/* PLL */
	0x9D, 0x0B, 0xEA, 0x9D,
	0xB2, 0x9E, 0x81, 0x00,
	0x00, 0x00, 0x00, 0x00,
	},
};
static Mach64ct mp1024x768x8 = {
	0x2EC,

	{
	0x007F00A5,			/* HTotalDisp */
	0x00340085,			/* HSyncStrtWid */
	0x02FF0325,			/* VTotalDisp */
	0x00260302,			/* VSyncStrtWid */
	0x00D403FF,			/* VlineCrntVline */
	0x20000000,			/* OffPitch */
	0x00000000,			/* IntCntl */
	0x030B0200,			/* CrtcGenCntl */
	0x00000000,			/* OvrClr */
	0x00000000,			/* OvrWidLR */
	0x00000000,			/* OvrWidTB */
	0xFFFFFFFF,			/* CurClr0 */
	0x00000000,			/* CurClr1 */
	0x00018000,			/* CurOffset */
	0x00720132,			/* CurHVposn */
	0x00300030,			/* CurHVoff */
	0x04100400,			/* ScratchReg0 */
	0x00000000,			/* ScratchReg1 */
	0x00000002,			/* ClockCntl */
	0x600000F9,			/* BusCntl */
	0x00000402,			/* MemCntl */
	0x00010000,			/* MemVgaWpSel */
	0x00010000,			/* MemVgaRpSel */
	0x00FF0001,			/* DacRegs */
	0x070060F0,			/* DacCntl */
	0x00000188,			/* GenTestCntl */
	0x00000000,			/* ConfigCntl */
	0x09004354,			/* ConfigChipId */
	0x00000009,			/* ConfigStat0 */
	0x00000000,			/* ConfigStat1 */
	},

	{
	0x00, 0x60, 0x2D, 0x14,		/* PLL */
	0x9D, 0x0B, 0xDA, 0x9D,
	0xB2, 0xEC, 0x81, 0x00,
	0x00, 0x00, 0x00, 0x00,
	},
};

static	Pcidev*		pcidev;
static	Mach64ct*	mp;
static	ulong		aperture;
static	ulong		storage;
static	ulong		storage64;
static	Point		hotpoint;
extern	Memimage	gscreen;

static void
pllw(int r, uchar b)
{
	int io;

	io = (ClockCntl<<10)+mp->io;
	outb(io+1, (r<<2)|0x02);
	outb(io+2, b);
}

static ulong
ior32(int r)
{
	return inl((r<<10)+mp->io);
}

static void
iow32(int r, ulong l)
{
	outl(((r)<<10)+mp->io, l);
}

static void
mach64ctinit(Mode* mode)
{
	if(mode->x == 640 && mode->y == 480 && mode->d == 3)
		mp = &mp640x480x8;
	else if(mode->x == 1024 && mode->y == 768 && mode->d == 3)
		mp = &mp1024x768x8;
	else
		return;

	/*
	 * Set linear aperture, recommended 8Mb on a 16Mb boundary.
	 * Should set mode->apsize to the real memory size. Mustn't
	 * be set to 8Mb as the registers are mapped at the top and
	 * setscreen() tries to clear it all.
	 */
	if(aperture == 0){
		if((aperture = upamalloc(0, 8*1024*1024, 16*1024*1024)) == 0)
			return;
	
		pcicfgw32(pcidev, PciBAR0, aperture);
		iow32(ConfigCntl, ((aperture/(4*1024*1024))<<4)|0x02);
	}

	mode->aperture = aperture;
	mode->apsize = mode->x*((1<<mode->d)/BI2BY)*mode->y;
	for(mode->apshift = 0; mode->apshift < 31; mode->apshift++){
		if((1<<mode->apshift) >= mode->apsize)
			break;
	}
	mode->apsize = 1<<mode->apshift;

	/*
	 * Other relevant accelerator registers.
	 */
	iow32(GenTestCntl, 0);
	iow32(GenTestCntl, 0x100);

	iow32(MemCntl, mp->reg[MemCntl] & ~0x70000);
	iow32(HTotalDisp, mp->reg[HTotalDisp]);
	iow32(HSyncStrtWid, mp->reg[HSyncStrtWid]);
	iow32(VTotalDisp, mp->reg[VTotalDisp]);
	iow32(VSyncStrtWid, mp->reg[VSyncStrtWid]);
	iow32(IntCntl, mp->reg[IntCntl]);
	iow32(OffPitch, mp->reg[OffPitch]);
	iow32(CrtcGenCntl, mp->reg[CrtcGenCntl]);
	iow32(OvrClr, mp->reg[OvrClr]);

	/*
	 * Set the clock.
	 */
	pllw(PLLn2, mp->pll[PLLn2]);
	pllw(PLLp, mp->pll[PLLp]);
	iow32(ClockCntl, mp->reg[ClockCntl]);
	iow32(ClockCntl, 0x40|mp->reg[ClockCntl]);
}

static int
mach64ctident(void)
{
	if(pcidev == 0 && (pcidev = pcimatch(0, 0x1002, 0x4354)) == 0)
		return 0;

	/*
	 * should grab the i/o base address here.
	 */
	return 1;
}

static void
mach64ctpage(int)
{
}

static	void	enable(void);
static	void	disable(void);
static	void	move(int, int);
static	void	load(Cursor*);

Vgac vgamach64ct = {
	"mach64ct",
	mach64ctpage,
	mach64ctinit,
	mach64ctident,
	enable,
	disable,
	move,
	load,
};

static void
disable(void)
{
	ulong r;

	r = ior32(GenTestCntl);
	iow32(GenTestCntl, r & ~0x80);
}

static void
enable(void)
{
	ulong r;

	r = ior32(GenTestCntl);
	iow32(GenTestCntl, r & ~0x80);

	iow32(CurClr0, (Pwhite<<24)|(Pwhite<<16)|(Pwhite<<8)|Pwhite);
	iow32(CurClr1, (Pblack<<24)|(Pblack<<16)|(Pblack<<8)|Pblack);

	/*
	 * Find a place for the cursor data in display memory.
	 * Must be 64-bit aligned.
	 */
	storage64 = (gscreen.width*BY2WD*gscreen.r.max.y+7)/8;
	iow32(CurOffset, storage64);
	storage = storage64*8;

	/*
	 * Cursor goes in the top right corner of the 64x64 array
	 * so the horizontal and vertical presets are 64-16.
	 */
	iow32(CurHVposn, (0<<16)|0);
	iow32(CurHVoff, ((64-16)<<16)|(64-16));

	iow32(GenTestCntl, 0x80|r);
}

static void
move(int cx, int cy)
{
	int x, xo, y, yo;

	/*
	 * Mustn't position the cursor offscreen even partially,
	 * or it disappears. Therefore, if x or y is -ve, adjust the
	 * cursor presets instead. If y is negative also have to
	 * adjust the starting offset.
	 */
	if((x = cx+hotpoint.x) < 0){
		xo = x;
		x = 0;
	}
	else
		xo = 0;
	if((y = cy+hotpoint.y) < 0){
		yo = y;
		y = 0;
	}
	else
		yo = 0;

	iow32(CurHVoff, ((64-16-yo)<<16)|(64-16-xo));
	iow32(CurOffset, storage64 + (-yo*2));
	iow32(CurHVposn, (y<<16)|x);
}

static void
load(Cursor *c)
{
	ulong r;
	ushort p;
	int i, x, y;
	uchar p0, p1, *mem;

	r = ior32(GenTestCntl);

	/*
	 * Disable the cursor.
	 */
	iow32(GenTestCntl, r & ~0x80);

	mem = ((uchar*)gscreen.data->data) + storage;

	/*
	 * Initialise the 64x64 cursor RAM array.
	 * The cursor mode gives the following truth table:
	 *	p1 p0	colour
	 *	 0  0	Cursor Colour 0
	 *	 0  1	Cursor Colour 1
	 *	 1  0	Transparent
	 *	 1  1	Complement
	 * Put the cursor into the top-right of the 64x64 array.
	 */
	for(y = 0; y < 64; y++){
		for(x = 0; x < 64/8; x++){
			if(x >= (64-16)/8 && y < 16){
				p0 = c->clr[(x-(64-16)/8)+y*2];
				p1 = c->set[(x-(64-16)/8)+y*2];

				p = 0x0000;
				for(i = 0; i < 8; i++){
					if(p1 & (1<<(7-i)))
						p |= 0x01<<(2*i);
					else if(p0 & (1<<(7-i)))
						;
					else
						p |= 0x02<<(2*i);
				}
				*mem++ = p & 0xFF;
				*mem++ = (p>>8) & 0xFF;
			}
			else {
				*mem++ = 0xAA;
				*mem++ = 0xAA;
			}
		}
	}

	/*
	 * Set the cursor hotpoint and enable the cursor.
	 */
	hotpoint = c->offset;
	iow32(GenTestCntl, 0x80|r);
}
