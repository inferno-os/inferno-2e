#include <lib9.h>
#include <image.h>
#include <kernel.h>

#define	CHUNK	6000

int
loadimage(Image *i, Rectangle r, uchar *data, int ndata)
{
	long dy;
	int n, bpl;
	uchar *a;

	if(!rectinrect(r, i->r)){
		kwerrstr("loadimage: bad rectangle");
		return -1;
	}
	bpl = bytesperline(r, i->ldepth);
	n = bpl*Dy(r);
	if(n > ndata){
		kwerrstr("loadimage: insufficient data");
		return -1;
	}
	ndata = 0;
	while(r.max.y > r.min.y){
		dy = r.max.y - r.min.y;
		if(dy*bpl > CHUNK)
			dy = CHUNK/bpl;
		n = dy*bpl;
		a = bufimage(i->display, 21+n);
		a[0] = 'w';
		BPLONG(a+1, i->id);
		BPLONG(a+5, r.min.x);
		BPLONG(a+9, r.min.y);
		BPLONG(a+13, r.max.x);
		BPLONG(a+17, r.min.y+dy);
		memmove(a+21, data, n);
		ndata += n;
		data += n;
		r.min.y += dy;
	}
	if(flushimage(i->display, 0) < 0)
		return -1;
	return ndata;
}
