#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "image.h"
#include "vmode.h"
#include "vctlr.h"
#include "putimage.h"
#include "screen.h"


int imagewid(uchar *image)
{
	uchar *bp = image+11;
	return atoi((char*)bp+3*12)-atoi((char*)bp+1*12);
}

int imagehgt(uchar *image)
{
	uchar *bp = image+11;
	return atoi((char*)bp+4*12)-atoi((char*)bp+2*12);
}


void putimage(int px, int py, uchar *image, int scale)
{
  	int x, y;
	uchar *bp, *dp, *sp;
	// int ldepth;
	int rminx, rminy, rmaxx, rmaxy, lasty, csize, wid, hgt;
	int i, n;
	uchar *buf;
	uchar *fb = vd_fb + py*vd_wid;
	int xscale = scale;
	int yscale = scale;

	fb += px;
	bp = image+11;
	// ldepth = atoi((char*)bp+0*12);
	rminx = atoi((char*)bp+1*12);
	rminy = atoi((char*)bp+2*12);
	rmaxx = atoi((char*)bp+3*12);
	rmaxy = atoi((char*)bp+4*12);
	bp += 5*12;
	wid = rmaxx - rminx;
	hgt = rmaxy - rminy;
	buf = (uchar*)malloc(wid*hgt);
	dp = buf;
	do {
		lasty = atoi((char*)bp+0*12);
		csize = atoi((char*)bp+1*12);
		bp += 2*12;
		for(i=0; i<csize; i++) {
			if(*bp & 0x80) {
				n=(*bp++&0x7f)+1;
				sp = bp;
				bp += n;
				i += n;
			} else {
				n = ((*bp >> 2) & 0x1f)+3;
				sp = dp-((((bp[0]&3)<<8)|bp[1])+1);
				bp += 2;
				i++;
			}
			while(n--)
				*dp++ = *sp++;
		}
	} while(lasty < rmaxy);
	bp = buf;

	for(y=0; y<hgt; y++) {
		uchar *xfb = fb;
		for(x=0; x<wid; x++) {
			uchar *sfb = xfb;
			uchar c = *bp++;
			int i, j;
			for(j=0; j<yscale; j++) {
				for(i=0; i<xscale; i++)
					*sfb++ = c;
				sfb += vd_wid-xscale;
			}
			xfb += xscale;
		}
		fb += vd_wid*yscale;
	}	
	free(buf);
}

