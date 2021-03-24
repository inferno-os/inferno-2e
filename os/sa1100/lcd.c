#include	"u.h"
#include 	"mem.h"
#include	"../port/lib.h"
#include 	"dat.h"
#include 	"image.h"
#include	"fns.h"
#include	"io.h"
#include	<memimage.h>
#include	"screen.h"

#define	Backgnd		(0xff)
#define	DEF_HZ		60

#define	DPRINT	if(0)iprint

extern int r_gamma;
extern int g_gamma;
extern int b_gamma;


typedef struct {
	uchar	pbs;
	uchar	dual;
	uchar	mono;
	uchar	active;
	uchar	hsync_wid;
	uchar	sol_wait;
	uchar	eol_wait;
	uchar	vsync_hgt;
	uchar	sof_wait;
	uchar	eof_wait;
	uchar	lines_per_int;
	uchar	palette_delay;
	uchar	acbias_lines;
	uchar	obits;
} LCDparam;

typedef struct {
	Vmode;
	LCDparam;
} LCDmode;

typedef struct {
	Vdisplay;
	LCDparam;
	ushort*	palette;
	uchar*	upper;
	uchar*	lower;
} LCDdisplay;


/* 0 for hz or pclk will get calculated from the Vmode info */

/* These mode structures should probably be in a separate platform-specific
 * file that can be linked in with the lcd driver, for maximum flexibility.
 * That would also reduce the size of the kernel/sboot for each platform.
 */

LCDmode lcd640x480x256tft =
{
	640, 480, 3, 0,         /* wid hgt d hz */
	VMODE_TFT|VMODE_PSEUDO|VMODE_LINEAR|VMODE_PACKED
		|VMODE_BIGEND|VMODE_LILEND,
	1, 0, 0, 1,             /* pbs dual mono active */
	20, 20, 20,             /* hsync_wid sol_wait eol_wait */
	40, 33, 20,             /* vsync_hgt sof_wait eof_wait */
	0, 1, 0,                /* lines_per_int palette_delay acbias_lines */
	12,                     /* obits */
};

LCDmode lcd640x480x256 =
{
	640, 480, 3, 0,		/* wid hgt d hz */
	VMODE_COLOR|VMODE_PSEUDO|VMODE_LINEAR|VMODE_PACKED
		|VMODE_BIGEND|VMODE_LILEND,
	1, 1, 0, 0,		/* pbs dual mono active */
	2, 1, 1,		/* hsync_wid sol_wait eol_wait */
	0, 1, 0,		/* vsync_hgt sof_wait eof_wait */
	0, 1, 0,		/* lines_per_int palette_delay acbias_lines */
        16,                     /* obits */
};

LCDmode lcd640x480x16 =
{
	640, 480, 2, 0,		/* wid hgt d hz */
	VMODE_COLOR|VMODE_PSEUDO|VMODE_LINEAR|VMODE_PACKED
		|VMODE_BIGEND|VMODE_LILEND,
	0, 1, 0, 0,		/* pbs dual mono active */
	2, 1, 1,		/* hsync_wid sol_wait eol_wait */
	0, 1, 0,		/* vsync_hgt sof_wait eof_wait */
	0, 1, 0,		/* lines_per_int palette_delay acbias_lines */
        16,                     /* obits */
};

LCDmode lcd640x480x16mono =
{
	640, 480, 2, 0,		/* wid hgt d hz */
	VMODE_MONO|VMODE_PSEUDO|VMODE_LINEAR|VMODE_PACKED
		|VMODE_BIGEND|VMODE_LILEND,
	0, 1, 1, 0,		/* pbs dual mono active */
	2, 40, 100,		/* hsync_wid sol_wait eol_wait */
	0, 0, 0,		/* vsync_hgt sof_wait eof_wait */
	0, 1, 0,		/* lines_per_int palette_delay acbias_lines */
        8,                      /* obits */
};

LCDmode lcd320x240x256 =
{
	320, 240, 3, 0,		/* wid hgt d hz */
	VMODE_COLOR|VMODE_PSEUDO|VMODE_LINEAR|VMODE_PACKED
		|VMODE_BIGEND|VMODE_LILEND,
	1, 0, 0, 0,		/* pbs dual mono active */
	2, 30, 30,		/* hsync_wid sol_wait eol_wait */
	0, 0, 0,		/* vsync_hgt sof_wait eof_wait */
	0, 0, 0,		/* lines_per_int palette_delay acbias_lines */
        8,                      /* obits */
};

LCDmode lcd320x240x16 =
{
	320, 240, 2, 0,		/* wid hgt d hz */
	VMODE_COLOR|VMODE_PSEUDO|VMODE_LINEAR|VMODE_PACKED
		|VMODE_BIGEND|VMODE_LILEND,
	0, 0, 0, 0,		/* pbs dual mono active */
	2, 30, 30,		/* hsync_wid sol_wait eol_wait */
	0, 0, 0,		/* vsync_hgt sof_wait eof_wait */
	0, 0, 0,		/* lines_per_int palette_delay acbias_lines */
        8,                      /* obits */
};

LCDmode lcd320x240x16mono =
{
	320, 240, 2, 0,		/* wid hgt d hz */
	VMODE_MONO|VMODE_PSEUDO|VMODE_LINEAR|VMODE_PACKED
		|VMODE_BIGEND|VMODE_LILEND,
	0, 0, 1, 0,		/* pbs dual mono active */
	2, 30, 30,		/* hsync_wid sol_wait eol_wait */
	0, 0, 0,		/* vsync_hgt sof_wait eof_wait */
	0, 0, 0,		/* lines_per_int palette_delay acbias_lines */
        4,                      /* obits */
};


/* modes should be ordered from most specific to most generic, since
 * the first match will be used
 */
LCDmode* lcdmodes[] = {&lcd640x480x256tft,
		       &lcd640x480x256, &lcd640x480x16, &lcd640x480x16mono,
		       &lcd320x240x256, &lcd320x240x16, &lcd320x240x16mono};


static LCDdisplay	*vd;	// current active display

void
lcd_setcolor(ulong p, ulong r, ulong g, ulong b)
{
	if((vd->pbs == 0 && p > 15)
	 || vd->pbs == 1 && p > 255)
		return;
	vd->palette[p] = (vd->palette[p] & 0xf000) 
			|(r>>(32-4))<<8
			|(g>>(32-4))<<4
			|(b>>(32-4));
}

static void
setLCD(LCDdisplay *vd)
{
	LCDmode *m;
	int ppf, pclk, clockdiv;
	ulong v, c;
	LcdReg *lcd = LCDREG;
	GpioReg *gpio = GPIOREG;

	m = (LCDmode*)&vd->Vmode;
	ppf = ((((m->wid+m->sol_wait+m->eol_wait)
		       *(m->mono ? 1 : 3)) >> (3-m->mono))
			+m->hsync_wid)
		       *(m->hgt/(m->dual+1)+m->vsync_hgt
			+m->sof_wait+m->eof_wait);
	pclk = ppf*m->hz;
	clockdiv = ((conf.cpuspeed/pclk) >> 1)-2;

	// if LCD enabled, turn off and wait for current frame to end
	if(lcd->lccr0 & LCD0_M_LEN) {
		lcd->lccr0 &= ~LCD0_M_LEN;
		if (lcd->lcsr & 0x00000001)		/* broken old rev CPU -- bit always on */
			delay(50);				/* give it plenty of time to end */
		while(!(lcd->lcsr & 0x00000001))
			;
	}
	// Then make sure it gets reset
	lcd->lccr0 = 0;

	DPRINT("  pclk=%d clockdiv=%d\n", pclk, clockdiv);
	lcd->lccr3 =  (clockdiv << LCD3_V_PCD)
		| (m->acbias_lines << LCD3_V_ACB)
		| (m->lines_per_int << LCD3_V_API);
	lcd->lccr2 =  (((m->hgt/(m->dual+1))-1) << LCD2_V_LPP)
		| (m->vsync_hgt << LCD2_V_VSW)
		| (m->eof_wait << LCD2_V_EFW)
		| (m->sof_wait << LCD2_V_BFW);
	lcd->lccr1 =  ((m->wid-1) << LCD1_V_PPL)
		| (m->hsync_wid << LCD1_V_HSW)
		| (m->eol_wait << LCD1_V_ELW)
		| (m->sol_wait << LCD1_V_BLW);

	// enable LCD controller, CODEC, and lower 4/8 data bits (for tft/dual)
	v = m->obits < 12 ? 0 : m->obits < 16 ? 0x3c : 0x3fc;
	c = m->obits == 12 ? 0x3c0 : 0;
	gpio->gafr |= v;
	gpio->gpdr |= v | c;
	gpio->gpcr = c;

	lcd->dbar1 = PADDR(vd->palette);
	if(vd->dual)
		lcd->dbar2 = PADDR(vd->lower);

	// Enable LCD
	lcd->lccr0 = LCD0_M_LEN | (m->mono << LCD0_V_CMS)
		| (m->palette_delay << LCD0_V_FDD)
		| (m->dual ? LCD0_M_SDS : 0)
		| (m->active << LCD0_V_PAS);

	// recalculate actual HZ
	pclk = (conf.cpuspeed/(clockdiv+2)) >> 1;
	m->hz = pclk/ppf;
}

static void
lcdintr(Ureg *, void *)
{
	ulong x;
	LcdReg *lcd = LCDREG;

	x = lcd->lcsr;
	if (x & 0x002) {
		lcd->dbar1 = lcd->dbar1;
		lcd->dbar2 = lcd->dbar2;
	}
	if ((x&0xffc) != 0) {
		iprint("!!!*** LCSR err condition %lux\n", x);
		lcd->lcsr &= 0xffc;
	}
}

void
lcdchkstatus(void)
{
	LcdReg *lcd = LCDREG;
	/*
	 * called at the end of screeninit() to reset LCD DMA errors; apparently
	 * we are doing something wrong in the initialization code...
	 */
	for (;;) {
		delay(1000);
		if ((lcd->lcsr&0xff0) == 0)
			break;
		iprint("*** LCSR err condition %lux\n", lcd->lcsr);
		lcd->lcsr = 0xff0;
	}

	/*
	 *	Early revs of the sa1100 are broken, such that BAU and LFD are stuck
	 *	in the 1 state, and enabling this interrupt has dire consequences...
	 */
/*	intrenable(12, lcdintr, nil, BusCPU); */
}


Vdisplay*
lcd_init(Vmode* mode)
{
// the fact that this is here currently limits us to a single display...
// (along with the fact that Vctlr functions don't have any 'self' pointer
	static LCDdisplay main_display;
	LCDmode *m, **mp = lcdmodes;
	int palsize;
	int fbsize;

	while(*mp && !vmodematch(*mp, mode))
		mp++;
	if(!(m = *mp))
		return 0;

	vd = &main_display;
	vd->Vmode = *m;
	vd->LCDparam = *m;

	vd->flags &= (mode->flags & VMODE_BIGEND) ? ~VMODE_LILEND
						: ~VMODE_BIGEND;
	if(!vd->hz) {
		if(!(vd->hz = mode->hz))
			vd->hz = DEF_HZ;
	}
	DPRINT("%dx%dx%d: hz=%d f=%ux\n", vd->wid, vd->hgt, 1<<vd->d, vd->hz,
		vd->flags); /* */

	// Note: only 4-bit and 8-bit displays are currently fully supported
	palsize = vd->pbs ? 256 : 16;
	fbsize = palsize*2+(((vd->wid*vd->hgt) << vd->d) >> 3) + 15;
	if(!(vd->palette = screenalloc(fbsize)))
		panic("no vidmem, no party...");
	segflush(vd->palette, fbsize);
	writeBackBDC();

	vd->palette = (ushort*)ROUND((ulong)vd->palette, 16);

	/* make sure we're using uncached and buffered memory: */
	if (conf.usebabycache)
		vd->palette = (ushort*)((ulong)vd->palette | 0x10000000);

	vd->palette[0] = (vd->pbs<<12);
	vd->upper = (uchar*)(vd->palette + palsize);
	vd->bwid = (vd->wid << vd->pbs) >> 1;
	vd->lower = vd->upper+((vd->bwid*vd->hgt) >> 1);
	vd->fb = vd->upper;
	DPRINT("  p=%ux u=%ux l=%ux\n", vd->palette, vd->upper, vd->lower); /* */

	setLCD(vd);

	r_gamma = 1;
	b_gamma = 1;
	g_gamma = 1;
	vd->contrast = MAX_VCONTRAST/2+1;
	vd->brightness = MAX_VBRIGHTNESS/2+1;
DPRINT("  cpu rev %d\n", mmuregr(CpCPUID)&0xf);
	return vd;
}

void
lcd_flush(void)
{
	if (conf.usebabycache)
		writeBackBDC();
	else
		segflush(nil, 0);
}

void
lcd_sethz(int hz)
{
	vd->hz = hz;
	setLCD(vd);
}

void
lcdlink(void)
{
}

