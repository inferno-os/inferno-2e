#include <lib9.h>
#include <image.h>
#include <memimage.h>

#include "xmem.h"

static
int
_unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	int y, l;
	uchar *q;

	if(!rectinrect(r, i->r))
		return -1;
	l = bytesperline(r, i->ldepth);
	if(ndata < l*Dy(r))
		return -1;
	ndata = l*Dy(r);
	q = byteaddr(i, r.min);
	for(y=r.min.y; y<r.max.y; y++){
		memmove(data, q, l);
		q += i->width*sizeof(ulong);
		data += l;
	}
	return ndata;
}

int
unloadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	XImage *x;
	Xmem *xm;
	int n;

	if(!rectinrect(r, i->r))
		return -1;
	
	xm = i->X;
	x = getXdata(i, r);
	n = _unloadmemimage(i, r, data, ndata);

	if(x != nil)
		XDestroyImage(x);
	i->data->data = xm->wordp;
	i->data->base = nil;
	return n;
}
