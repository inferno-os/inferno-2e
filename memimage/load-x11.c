#include <lib9.h>
#include <image.h>
#include <memimage.h>

#include "xmem.h"

static int
_loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	int y, l, lpart, rpart, mx, m, mr;
	uchar *q;

	l = bytesperline(r, i->ldepth);
	if(ndata < l*Dy(r))
		return -1;
	ndata = l*Dy(r);
	q = byteaddr(i, r.min);
	mx = 7>>i->ldepth;
	lpart = (r.min.x & mx) << i->ldepth;
	rpart = (r.max.x & mx) << i->ldepth;
	m = 0xFF >> lpart;
	/* may need to do bit insertion on edges */
	if(l == 1){	/* all in one byte */
		if(rpart)
			m ^= 0xFF >> rpart;
		for(y=r.min.y; y<r.max.y; y++){
			*q ^= (*data^*q) & m;
			q += i->width*sizeof(ulong);
			data++;
		}
		return ndata;
	}
	if(lpart==0 && rpart==0){	/* easy case */
		for(y=r.min.y; y<r.max.y; y++){
			memmove(q, data, l);
			q += i->width*sizeof(ulong);
			data += l;
		}
		return ndata;
	}
	mr = 0xFF ^ (0xFF >> rpart);
	if(lpart!=0 && rpart==0){
		for(y=r.min.y; y<r.max.y; y++){
			*q ^= (*data^*q) & m;
			if(l > 1)
				memmove(q+1, data+1, l-1);
			q += i->width*sizeof(ulong);
			data += l;
		}
		return ndata;
	}
	if(lpart==0 && rpart!=0){
		for(y=r.min.y; y<r.max.y; y++){
			if(l > 1)
				memmove(q, data, l-1);
			q[l-1] ^= (data[l-1]^q[l-1]) & mr;
			q += i->width*sizeof(ulong);
			data += l;
		}
		return ndata;
	}
	for(y=r.min.y; y<r.max.y; y++){
		*q ^= (*data^*q) & m;
		if(l > 2)
			memmove(q+1, data+1, l-2);
		q[l-1] ^= (data[l-1]^q[l-1]) & mr;
		q += i->width*sizeof(ulong);
		data += l;
	}
	return ndata;
}

int
loadmemimage(Memimage *i, Rectangle r, uchar *data, int ndata)
{
	XImage *x;
	Xmem *xm;
	int n;

	if(!rectinrect(r, i->r))
		return -1;
	
	xm = i->X;

	x = getXdata(i, Rpt(r.min, r.min));
	n = _loadmemimage(i, r, data, ndata);
	
	if(x != nil){
		putXdata(i, x, r);
		XDestroyImage(x);
		i->data->data = xm->wordp;
		i->data->base = nil;
	}
	xdirtied(i);
	return n;
}
