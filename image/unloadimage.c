#include <lib9.h>
#include <image.h>
#include <kernel.h>
#include <interp.h>

int
unloadimage(Image *i, Rectangle r, uchar *data, int ndata)
{
	int bpl;
	uchar *a;
	Display *d;

	if(!rectinrect(r, i->r)){
		kwerrstr("unloadimage: bad rectangle");
		return -1;
	}
	bpl = bytesperline(r, i->ldepth);
	if(ndata < bpl*Dy(r)){
		kwerrstr("unloadimage: buffer too small");
		return -1;
	}

	d = i->display;
	flushimage(d, 0);	/* make sure subsequent flush is for us only */
	a = bufimage(i->display, 1+4+4*4);
	if(a == 0){
		kwerrstr("unloadimage: %r");
		return -1;
	}
	a[0] = 'r';
	BPLONG(a+1, i->id);
	BPLONG(a+5, r.min.x);
	BPLONG(a+9, r.min.y);
	BPLONG(a+13, r.max.x);
	BPLONG(a+17, r.max.y);
	if(flushimage(i->display, 0) < 0)
		return -1;
	if(d->local == 0)
		release();
	ndata = kchanio(d->datachan, data, ndata, OREAD);
	if(d->local == 0)
		acquire();
	return ndata;
}
