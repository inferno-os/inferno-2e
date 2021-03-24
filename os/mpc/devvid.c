/*
 * mpc823 video driver (assuming 8xxFADS board)
 *
 * requires I2C interface to ADV7176 NTSC/PAL converter, as on 8xxFADS board
 */

#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/error.h"

typedef struct Vmode Vmode;

enum {
	AdvI2C=	0x54,	/* ADV7176 address on FADS i2c */

	/* vsr */
	CAS=	1<<6,	/* RAM_1 is currently active */

	/* vcmr */
	ASEL=	1<<1,	/* =0, select RAM_0 and set_0; =1, select RAM_1 and set_1 */
	BLD=		1<<0,	/* =0, display image; =1, blank display (background video) */

	/* vfcr1 */
	SFB1=	1<<31,	/* if set, frame B is not valid (frame A only) */

	/* vccr */
	DPF=	1<<13,	/* default pixel format is YCrYCb */
	BO=		1<<6,	/* big-endian */
	CSRC=	1<<1,	/* clock source is SHIFT/CLK/VCLK not LCDCLK */
	VON=	1<<0,	/* video enable */


	/* NTSC parameters */
	NLINENTSC=	240,		/* x2 for interleaved */

	/* PAL parameters */
	NLINEPAL=	288,

	BURSTLEN= 16,		/* bytes */
	NBURST=	90,
	MAXNLINE=	NLINEPAL,
};

struct Vmode {
	char*	name;
	int	nline;
	uchar*	adv;
	ulong*	vram;
	int	nvram;
};

/*
 * values from the adv7176 data sheet,
 * except for mode register 2 and timing register 0
 *
 * some of this could be done by Limbo via #v/vidctl.
 *
 * square pixel mode cannot be used on the FADS board:
 * no suitable clock is available (24.5MHz for NTSC or 29.5MHz for PAL).
 */
static uchar adv7176ntsc[] = {
	0x04, 0x02,	/* mode 0, 1 */
	0x16, 0x7c, 0xf0, 0x21,	/* subcarrier frequency */
	0x00,	/* subcarrier phase */
	0x02,	/* timing register 0 */
	0x00, 0x00, 0x00, 0x00,	/* closed captioning */
	0x00,	/* timing register 1 */
	0x08, 	/* mode 2 */
	0x00, 0x00, 0x00, 0x00,	/* pedestal */
	0x00,	/* mode 3 */
};

static uchar adv7176pal_i[] = {	/* also PAL B, D, G, H */
	0x01, 0x00,	/* mode 0, 1 */
	0xcb, 0x8a, 0x09, 0x2a,	/* subcarrier frequency */
	0x00,	/* subcarrier phase */
	0x02,	/* timing register 0 */
	0x00, 0x00, 0x00, 0x00,	/* closed captioning */
	0x00,	/* timing register 1 */
	0x08,	/* mode 2 */
	0x00, 0x00, 0x00, 0x00,	/* pedestal */
	0x00,	/* mode 3 */
};

static uchar adv7176pal_m[] = {
	0x06, 0x00,	/* mode 0, 1 */
	0xa3, 0xef, 0xe6, 0x21,	/* subcarrier frequency */
	0x00,	/* subcarrier phase */
	0x02,	/* timing register 0 */
	0x00, 0x00, 0x00, 0x00,	/* closed captioning */
	0x00,	/* timing register 1 */
	0x08,	/* mode 2 */
	0x00, 0x00, 0x00, 0x00,	/* pedestal */
	0x00,	/* mode 3 */
};

static uchar adv7176pal_n[] = {
	0x05, 0x00,	/* mode 0, 1 */
	0xcb, 0x8a, 0x09, 0x2a,	/* subcarrier frequency */
	0x00,	/* subcarrier phase */
	0x02,	/* timing register 0 */
	0x00, 0x00, 0x00, 0x00,	/* closed captioning */
	0x00,	/* timing register 1 */
	0x08,	/* mode 2 */
	0x00, 0x00, 0x00, 0x00,	/* pedestal */
	0x00,	/* mode 3 */
};

static int
i2cload(uchar *data)
{
	long n;

	print("i2c: sending to adv7176\n");
	n = i2csend(AdvI2C|1|(0<<8), data, 18);
	print("i2c: sent %ld\n", n);
	return n;
}

/*
 * video ram contents from MPC823UM (19.4.1)
 */
#define	VW(Hx,Vx,Fx,Bx,VDS,INT,LCYC,LP,LST)\
			(((Hx)<<30)|((Vx)<<28)|((Fx)<<26)|((Bx)<<24)|((VDS)<<16)|((INT)<<15)|((LCYC)<<2)|((LP)<<1)|((LST)<<0))

static	ulong	vramntsc[] = {
	VW(0,0,3,0,1,0,3,1,0),
	VW(3,0,3,0,1,0,243,0,0),
	VW(3,0,3,0,1,0,1440,0,0),
	VW(3,0,3,0,1,0,32,1,0),
	VW(0,0,0,0,1,0,18,1,0),
	VW(3,0,0,0,1,0,243,0,0),
	VW(3,0,0,0,1,0,1440, 0,0),
	VW(3,0,0,0,1,0,32,1,0),
	VW(0,0,0,0,1,0,240,1,0),
	VW(3,0,0,0,1,0,235,0,0),
	VW(3,0,0,3,1,0,8,  0,0),
	VW(3,0,0,3,0,0,1440,  0,0),
	VW(3,0,0,0,1,0,32, 1,0),
	VW(0,0,0,0,1,0,4,1,0),
	VW(3,0,0,0,1,0,243,0,0),
	VW(3,0,0,0,1,0,1440,0,0),
	VW(3,0,0,0,1,0,32,1,0),
	VW(0,0,3,0,1,0,19,1,0),
	VW(3,0,3,0,1,0,243,0,0),
	VW(3,0,3,0,1,0,1440,0,0),
	VW(3,0,3,0,1,0,32,1,0),
	VW(0,0,3,0,1,0,240,1,0),
	VW(3,0,3,0,1,0,235,0,0),
	VW(3,0,3,3,1,0,8,  0,0),
	VW(3,0,3,3,0,0,1440,  0,0),
	VW(3,0,3,0,1,0,32,  1,0),
	VW(0,0,3,0,1,0,1,1,0),
	VW(3,0,3,0,1,0,243,0,0),
	VW(3,0,3,0,1,0,1440,0,0),
	VW(3,0,3,0,1,0,32,1,1),
};

static	ulong	vrampal[] = {
	VW(0,0,0,0,1,0,22,1,0),
	VW(3,0,0,0,1,0,263,0,0),
	VW(3,0,0,0,1,0,1440,0,0),
	VW(3,0,0,0,1,0,24,1,0),
	VW(0,0,0,0,1,0,288,1,0),
	VW(3,0,0,0,1,0,255,0,0),
	VW(3,0,0,3,1,0,8,0,0),
	VW(3,0,0,3,0,0,1440,0,0),
	VW(3,0,0,0,1,0,24,1,0),
	VW(0,0,0,0,1,0,2,1,0),
	VW(3,0,0,0,1,0,263,0,0),
	VW(3,0,0,0,1,0,1440,0,0),
	VW(3,0,0,0,1,0,24,1,0),
	VW(0,0,3,0,1,0,23,1,0),
	VW(3,0,3,0,1,0,263,0,0),
	VW(3,0,3,0,1,0,1440,0,0),
	VW(3,0,3,0,1,0,24,1,0),
	VW(0,0,3,0,1,0,288,1,0),
	VW(3,0,3,0,1,0,255,0,0),
	VW(3,0,3,3,1,0,8,0,0),
	VW(3,0,3,3,0,0,1440,0,0),
	VW(3,0,3,0,1,0,24,1,0),
	VW(0,0,3,0,1,0,2,1,0),
	VW(3,0,3,0,1,0,263,0,0),
	VW(3,0,3,0,1,0,1440,0,0),
	VW(3,0,3,0,1,1,24,1,1),
};

static	Vmode	vmodes[] = {
	{"ntsc", NLINENTSC, adv7176ntsc, vramntsc, nelem(vramntsc)},	/* default */
	{"pal-i", NLINEPAL, adv7176pal_i, vrampal, nelem(vrampal)},
	{"pal-m", NLINEPAL, adv7176pal_m, vrampal, nelem(vrampal)},
	{"pal-n", NLINEPAL, adv7176pal_n, vrampal, nelem(vrampal)},
	{nil},
};

static struct {
	Lock;
	int	on;
	long	length;
	int	width;
	uchar*	framebuf;
	long	fbsize;
	Vmode*	mode;
} video;

static void
videosetup(void)
{
	IMM *io;
	ulong *vr;
	int i;

	if(video.framebuf == nil)
		error(Eio);
	io = m->iomem;
	io->vccr = DPF|BO|CSRC;
	io->vsr = ~0;

	io->vbcb = 0x80108010;	/* black background (CbYCrY) */
	io->vfcr1 = SFB1|(video.mode->nline<<19)|(NBURST<<8)|NBURST;
	io->vfcr0 = io->vfcr1;
	io->vfaa1 = PADDR(video.framebuf);	/* odd frame (2x720)x480=690k */
	io->vfaa0 = io->vfaa1;
	io->vfba1 = PADDR(video.framebuf+(NBURST*BURSTLEN));	/* even frame */
	io->vfba0 = io->vfba1;

	archenablevideo();	/* enable AFTER pdpar/pddir to avoid damage */
	io = ioplock();
	io->sdcr = (io->sdcr & ~0xF) | 0x40;	/* LAM=1, LAID=0, RAID=0 */
	iopunlock();

	print("set VCRAM...\n");
	io = m->iomem;
	vr = io->vcram;
	for(i=0; i<video.mode->nvram; i++)
		*vr++ = video.mode->vram[i];
	if(io->vsr & CAS)
		io->vcmr = 0;
	else
		io->vcmr = ASEL;
	io->vccr |= VON;

}

static void
videoup(void)
{
	IMM *io;

	i2csetup();

	print("adv7176 reset...\n");uartwait();
	lcdpanel(0);	/* must disable LCD */
	io = ioplock();
	io->vccr = 0x2042;
	io->pddir = 0;
	io->pdpar = 0x1fff;
	iopunlock();
	archresetvideo();
	if(i2cload(video.mode->adv) < 0)
		error(Eio);
	videosetup();
}

static void
videodown(void)
{
	IMM *io;

	io = ioplock();
	io->vccr &= ~VON;
	iopunlock();
	archdisablevideo();
	lcdpanel(1);
}

enum{
	Qdir,
	Qdata,
	Qctl,
	Qvcram,
};

static
Dirtab vidtab[]={
	"viddata",		{Qdata, 0},	0,	0600,
	"vidctl",		{Qctl, 0},		0,	0600,
	"vidcram",	{Qvcram, 0},	0,	0600,
};

static void
vidreset(void)
{
	video.width = NBURST*BURSTLEN;
	video.mode = &vmodes[0];
	video.length = video.width*video.mode->nline*2;
	video.fbsize = video.width*MAXNLINE*2;
}

static void
vidinit(void)
{
	ulong *v;
	int i;

	if(video.framebuf == nil)
		video.framebuf = archvideobuffer(video.fbsize);
	if(video.framebuf != nil){
		v = (ulong*)video.framebuf;
		for(i=video.fbsize/4; --i>=0;)
			*v++ = 0x80108010;	/* black */
	}
}

static Chan*
vidattach(char* spec)
{
	vidtab[0].length = video.length;
	return devattach('v', spec);
}

static int
vidwalk(Chan* c, char* name)
{
	return devwalk(c, name, vidtab, nelem(vidtab), devgen);
}

static void
vidstat(Chan* c, char* db)
{
	devstat(c, db, vidtab, nelem(vidtab), devgen);
}

static Chan*
vidopen(Chan* c, int omode)
{
	return devopen(c, omode, vidtab, nelem(vidtab), devgen);
}

static void
vidclose(Chan*)
{
}

static long
vidread(Chan *c, void *a, long n, ulong offset)
{
	IMM *io;
	uchar *s;

	switch(c->qid.path & ~CHDIR){
	case Qdir:
		return devdirread(c, a, n, vidtab, nelem(vidtab), devgen);
	case Qdata:
		if(video.framebuf == nil)
			error(Eio);
		if(offset >= (ulong)video.length){
			n=0;
			break;
		}
		if(offset+n >= video.length)
			n = video.length - offset;
		s = video.framebuf+offset;
		memmove(a, s, n);
		break;
	case Qvcram:
		if((offset|n) & 3)
			error(Eio);
		io = m->iomem;
		if(offset >= sizeof(io->vcram)){
			n=0;
			break;
		}
		if(offset+n >= sizeof(io->vcram))
			n = sizeof(io->vcram) - offset;
		memmove(a, (uchar*)io->vcram+offset, n);
		break;
	case Qctl:
	default:
		n=0;
		break;
	}
	return n;
}

static long
vidwrite(Chan *c, void *a, long n, ulong offset)
{
	uchar *d;
	char cmd[32], *fields[4];
	IMM *io;
	int i;

	switch(c->qid.path & ~CHDIR){
	case Qdata:
		if(video.framebuf == nil)
			error(Eio);
		if(offset >= (ulong)video.length){
			n=0;
			break;
		}
		if(offset+n >= video.length)
			n = video.length - offset;
		d = video.framebuf+offset;
		memmove(d, a, n);
		dcflush(d, n);
		break;
	case Qvcram:
		if((offset|n) & 3)
			error(Eio);
		io = m->iomem;
		if(offset >= sizeof(io->vcram)){
			n=0;
			break;
		}
		if(offset+n >= sizeof(io->vcram))
			n = sizeof(io->vcram) - offset;
		memmove((uchar*)io->vcram+offset, a, n);
		io->vcmr ^= ASEL;
		break;
	case Qctl:
		if(n > sizeof(cmd)-1)
			n = sizeof(cmd)-1;
		memmove(cmd, a, n);
		cmd[n] = 0;
		i = parsefields(cmd, fields, nelem(fields), " \t\n");
		if(i == 1 && strcmp(fields[0], "on") == 0){
			lock(&video);
			i = video.on;
			video.on = 1;
			unlock(&video);
			if(i == 0)
				videoup();
		}else if(i == 1 && strcmp(fields[0], "off") == 0){
			lock(&video);
			i = video.on;
			video.on = 0;
			unlock(&video);
			if(i == 1)
				videodown();
		}else if(i == 2 && strcmp(fields[0], "mode") == 0){
			Vmode *v;

			for(v = vmodes; v->name != nil; v++)
				if(strcmp(v->name, fields[1]) == 0){
					video.mode = v;
					break;
				}
			if(v->name == nil)
				error(Ebadarg);
		}else
			error(Ebadarg);
		break;
	default:
		error(Ebadusefd);
	}
	return n;
}

Dev viddevtab = {
	'v',
	"vid",

	vidreset,
	vidinit,
	vidattach,
	devdetach,
	devclone,
	vidwalk,
	vidstat,
	vidopen,
	devcreate,
	vidclose,
	vidread,
	devbread,
	vidwrite,
	devbwrite,
	devremove,
	devwstat,
};
