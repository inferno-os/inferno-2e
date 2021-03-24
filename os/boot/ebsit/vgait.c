#include	<lib9.h>
#include 	"mem.h"
#include 	"dat.h"
#include	"image.h"
#include	<memimage.h>
#include	"fns.h"
#include	"io.h"
#include	"../port/screen.h"
#include	"../port/vmode.h"
#include	"../port/vctlr.h"
#include	"bpi.h"

#define	DEF_HZ		60


#define VGA_SRX  0x03C4      /* Sequencer index register */
#define	VGA_SR0  0x03C5      /* SR0: Reset */
#define	VGA_SR1  0x03C5      /* SR1: Clocking mode */
#define	VGA_SR2  0x03C5      /* SR2: Plane mask */
#define	VGA_SR3  0x03C5      /* SR3: Character map select */
#define	VGA_SR4  0x03C5      /* SR4: Memory mode */
#define	VGA_SR6  0x03C5      /* SR6: Unlock ALL extension register */
#define VGA_SR7  0x03C5      /* SR7: Extended sequencer mode register */
#define VGA_SRF  0x03c5      /* DRAM Control Register */
#define VGA_SRW  0x03c5      /* other sequence registers */

#define VGA_CRX  0x03D4      /* CRT controller index register */
#define VGA_CRD  0x03D5      /* CRT controller register */

#define VGA_GRX  0x03CE      /* Graphics controller index register */
#define VGA_GRD  0x03CF      /* Graphics controller register */

#define	VGA_ARX 0x03C0       /* attribute controller index register */
#define	VGA_ARW 0x03C0       /* attribute controller data write register */
#define VGA_ARR 0x03C1	     /* attribute controller data read register */
#define VGA_DAC_MASK 0x03C6  /* DAC pixel mask */
#define VGA_DACW 0x03C8      /* pixel address (write mode) */
#define	VGA_DACD 0x03C9      /* pixel data */

#define VGA_STAT1 0x3DA		/* input status 1 */

#define VGA_LINEAR_SEG	0x1
#define VGA_FRAMEBUF	((uchar*)(0x03000000 + (VGA_LINEAR_SEG << 20)))



typedef struct {
	uchar misc;
	const uchar *crtc;
	const uchar *gtab;
	const uchar *attr;
} VgaParam;

typedef struct {
	Vmode;
	VgaParam;
} VgaMode;

typedef struct {
	Vdisplay;
	// nothing extra to add...
} VgaDisplay;



static const uchar crtc_1024x768[] = {
      0xA1,      /* CR0: Total number of character clocks per horizontal */
		         /*      period */
      0x80 - 1,  /* CR1: # char clocks during horizontal display time */
      0x80,      /* CR2: character count where horizontal blanking starts */
      0x80       /* CR3: make CR10 and CR11 readable */
      | 0x00     /*      display enable skew := 0 */
      | 0x04,    /*      low-order 4 bits of horizonal blanking end */
      0x84,      /* CR4: horizonal sync start */
      0x80       /* CR5: high-order bit of horizonal blanking end */
      | 0x00     /*      horizontal sync delay:  0 skew */
      | 0x12,    /*      horizontal sync end */

      0x2A,      /* CR6: low-order 8 bits of Total number of scanlines per */
	             /*      frame */
      0xFD,      /* CR7: various overflow bits */
      0x00,      /* CR8: Screen A preset row scan */
      0x00       /* CR9: CRTC scan double:  no */
      | 0x40     /*      bit 9 of CR18 */
      | 0x20     /*      bit 9 of CR15 */
      | 0x00,    /*      character height (in number of scanlines, minus one) */
      0x00,      /* CRA: text cursor start */
      0x00,      /* CRB: text cursor end */

      0x00,      /* CRC: high-order byte of Screen start address */
      0x00,      /* CRD: low-order byte of Screen start address */
      0x04,      /* CRE: high-order byte of Text cursor location address */
      0x00,      /* CRF: low-order byte of Text cursor location address */

      0x12,      /* CR10: low-order 8 bits of Vertical sync */
      0x80       /* CR11: write protect CR7-CR0 */
      | 0x00     /*       3 refresh cycles per scanline */
      | 0x09,    /*       vertical sync end */
      0xFF,      /* CR12: low-order 8 bits of Vertical display end */
      0x80,      /* CR13: low-order 8 bits of Display pitch */
      0x00,      /* CR14: underline scanline */
      0x00,      /* CR15: low-order 8 bits of Vertical blank start scanline  */
      0x2A,      /* CR16: low-order 8 bits of Vertical blank end scanline  */
      0x80       /* CR17: enable timing logic */
      | 0x60     /*       CRTC Address Counter byte mode */
      | 0x00     /*       clock Memory Address Counter with the character clock */
      | 0x00     /*       clock Scanline Counter with Horizontal Sync */
      | 0x02     /*       don't preserve Hercules compatibility */
      | 0x01,    /*       don't preserve CGA compatibility */
      0xFF,      /* CR18: low-order 8 bits of Line compare */
	  0x00,      /* CR19 */
	  0x00,      /* CR1A */
      0x20       /* CR1B: DAC blanking is controlled by Display Enable */
      | 0x02     /*       Enable extended address wrap */
};

static const uchar crtc_640x480[] = {
//	0x65, 0x4f, 0x50, 0x88, 0x55, 0x9a, 0x09, 0x3e,
//	0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//	0xe8, 0x8b, 0xdf, 0x50, 0x00, 0xe7, 0x04, 0xe3,
//	0xff, 0x00, 0x00, 0x22
	0x5f, 0x4f, 0x52, 0x9f, 0x53, 0x1f, 0x0b, 0x3e,
	0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xeb, 0x2d, 0xdf, 0x50, 0x60, 0xeb, 0xec, 0xe3,
	0xff, 0x00, 0x00, 0x22
};

static const uchar gtab_256[] = {
      0x00,   /* GR0: (unused) Set/Reset write values */
      0x00,   /* GR1: disable Set/Reset logic */
      0x00,   /* GR2: color compare value for Read mode 1 */
      0x00,   /* GR3: ignore data in latches, don't rotate data from CPU bus */
      0x00,   /* GR4: use display memory plane 0 for Read mode 0 */
      0x40    /* GR5: configure GC for 256-color mode */
      | 0x10  /*      Do not use odd/even mode */
      | 0x00  /*      Read mode 0 */
      | 0x00, /*      Write mode 0 */
      0x00    /* GR6: use the extended memory map: 128K starting at address A000:0 */
      | 0x04  /*      do not chain odd maps to even */
      | 0x01, /*      graphics mode */
      0x0F,   /* GR7: color don't care register; used only in Read mode 1 */
      0xFF    /* GR8: allow all bits to be written */
};

static const uchar attr_normal[] = {
      /* palette entries */
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,

      0x41,            /* AR10: place Attribute Controller in graphics mode */
      0xff, 	       /* AR11: border color  - bright blue */
      0x0F,            /* AR12: enable all 4 display memory planes in the choice */
                       /*       of the Attribute Controller Palette register */
      0x00,            /* AR13: don't shift pixel display data */
      0x00             /* AR14 */
};


VgaMode vga640x480x256 =
{
	640, 480, 3, 0,		/* wid hgt d hz */
	VMODE_COLOR|VMODE_PSEUDO|VMODE_LINEAR|VMODE_PACKED
		|VMODE_LILEND|VMODE_LILEND,
	0xc0|0x20|0x00|0x02|0x01,
				/* vertical size: 480 */
		  		/* page select: even */
		   		/* VCLK source: VCLK0, dflt freq: 36.082 MHz */
		   		/* enable display memory */
		   		/* color */
	crtc_640x480,
	gtab_256,
	attr_normal,
};

VgaMode vga1024x768x256 =
{
	1024, 768, 3, 0,	/* wid hgt d hz */
	VMODE_COLOR|VMODE_PSEUDO|VMODE_LINEAR|VMODE_PACKED
		|VMODE_LILEND|VMODE_LILEND,
	0xc0|0x20|0x0c|0x02|0x01,
				/* vertical size: 480 */
		  		/* page select: even */
		   		/* VCLK source: VCLK3, dflt freq: 36.082 MHz */
		   		/* enable display memory */
		   		/* color */
	crtc_1024x768,
	gtab_256,
	attr_normal,
};

/* modes should be ordered from most specific to most generic, since
 * the first match will be used
 */
VgaMode* vgamodes[] = {&vga640x480x256, &vga1024x768x256, 0};



static void
vsetcolor(ulong p, ulong r, ulong g, ulong b)
{
	outb(VGA_DAC_MASK, 0xFF);
	outb(VGA_DACW, p);
	outb(VGA_DACD, r>>(32-6));
	outb(VGA_DACD, g>>(32-6));
	outb(VGA_DACD, b>>(32-6));
}

static uchar
getVgaChipID(void)
{
	outb(VGA_SRX, 0x06);
	outb(VGA_SR6, 0x12);	/* SR6: unlock extension registers */
	outb(VGA_CRX, 0x27);
	return inb(VGA_CRD);	/* CR27: get ID 
				- should be 32 << 2 for the CL-GD5425 */
}

static void
setVgaMode(VgaMode *m)
{
	int i;

	/* global setup: */

	outb(0x46e8, 0x10);	/* enter setup mode to access POS102 */
	outb(0x0102, 0x01);	/* enable video subsystem */
	outb(0x46e8, 0x08);	/* exit setup mode, enable video subsystem */

	getVgaChipID();

	/* sequencer: */

	outb(VGA_SRX, 0x01);
	i = inb(VGA_SR1);
	outb(VGA_SRX, 0x01);
	outb(VGA_SR1, i|0x20);
	
	outb(0x03c2, m->misc);

	outb(VGA_SRX, 0x00);
	outb(VGA_SR0, 0x03);

	// outb(VGA_SRX, 0x01);
	// outb(VGA_SR1, 0x01);	/* SR1: clock every character, and
	//			   divide DCLK by 8 for character clock */
	outb(VGA_SRX, 0x02);
	outb(VGA_SR2, 0x0f);	/* SR2: enable writing all planes (0,1,2,3) */
	outb(VGA_SRX, 0x04);
	outb(VGA_SR4, 0x0e);	/* SR4: use all memory, not just 64K, and
				   put sequencer in chain-4 mode */
	outb(VGA_SRX, 0x06);
	outb(VGA_SR6, 0x12);	/* SR6: unlock extension registers */
	outb(VGA_SRX, 0x07);
	outb(VGA_SR7, (VGA_LINEAR_SEG << 4) | 0x01);
				/* SR7: use linear addressing */
				/* 1 char clock = 8 pixels */
	outb(VGA_SRX, 0x0f);
	outb(VGA_SRF, 0x10);	/* SRF: set DRAM width to 32 bits */
	outb(VGA_SRX, 0x0e);
	outb(VGA_SRW, 0x76);	/* SRE: VCLK3 numerator */
	outb(VGA_SRX, 0x1e);
	outb(VGA_SRW, 0x34);	/* SR1E: VCLK3 demoninator */

	/* CRT controller */

	outb(VGA_CRX, 0x11);
	outb(VGA_CRD, m->crtc[0x11] & ~0x80);
	// outb(VGA_CRX, 0x11);
	// outb(VGA_CRD, 0x00);  /* CR11: un-write protect CR7-CR0 */
	for(i=0; i < sizeof crtc_640x480; i++) {
		outb(VGA_CRX, i);
		outb(VGA_CRD, m->crtc[i]);
	}

	/* Graphics */

	for(i=0; i < sizeof gtab_256; i++) {
		outb(VGA_GRX, i);
		outb(VGA_GRD, m->gtab[i]);
	}
	outb(VGA_GRX, 0x09);
    	outb(VGA_GRD, 0x00);  /* GR9:  offset register 0 */

    	outb(VGA_GRX, 0x0B);
	outb(VGA_GRD, 0x00);  /* GRB: Graphics Controller
				 mode extension register */

	/* Attributes */

	outb(VGA_CRX, 0x24);
	if(inb(VGA_CRD) & 0x80)
		outb(VGA_ARW, 0x0);
	for(i=0; i < sizeof attr_normal; i++) {
		int x;
		x = inb(VGA_STAT1);
		if(i < 0x10) {
			outb(VGA_ARX, i);
			outb(VGA_ARW, m->attr[i]);
			x = inb(VGA_STAT1);
			outb(VGA_ARX, 0x20|i);
		} else {
			outb(VGA_ARX, 0x20|i);
			outb(VGA_ARW, m->attr[i]);
		}
		USED(x);
	}
	print("crtc=%ux %ux %ux\n", m->crtc[0], m->crtc[1], m->crtc[2]);

	outb(VGA_SRX, 0x01);
	outb(VGA_SR1, 0x01);	/* SR1: clock every character, and
				   divide DCLK by 8 for character clock */

	outb(VGA_ARX, 0x20);	/* enable video */
}



static Vdisplay*
vinit(Vmode* mode)
{
// the fact that this is here currently limits us to a single display... *sigh*
// (along with the fact that Vctlr functions don't have any 'self' pointer
	static VgaDisplay main_display;
	VgaDisplay *vd;

	VgaMode *m, **mp = vgamodes;

	// print("vinit(%dx%d)\n", mode->wid, mode->hgt);
	while(*mp && !vmodematch(*mp, mode))
		mp++;
	if(!(m = *mp))
		return 0;
	
	vd = &main_display;
	vd->Vmode = *m;

	vd->flags &= (mode->flags & VMODE_BIGEND) ? ~VMODE_LILEND
						: ~VMODE_BIGEND;
	if(!vd->hz) {
		if(!(vd->hz = mode->hz))
			vd->hz = DEF_HZ;
	}
	print("%dx%dx%d: hz=%d f=%ux\n", vd->wid, vd->hgt, 1<<vd->d, vd->hz,
		vd->flags);

	vd->fb = VGA_FRAMEBUF;
	setVgaMode(m);
	vd->contrast = MAX_VCONTRAST/2+1;
	vd->brightness = MAX_VBRIGHTNESS/2+1;
	return vd;
}



Vctlr vgait = {
	"vgait",
	vinit,
	vsetcolor,
	0, /* enable */
	0, /* disable */
	0, /* move */
	0, /* load */
	0, /* flush */
	0, /* setbrightness */
	0, /* setcontrast */
	0, /* sethz */
	0, /* link */
};

void
vgaitlink(void)
{
	addvctlrlink(&vgait);
}
