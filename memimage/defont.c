#include <lib9.h>
#include <image.h>
#include <memimage.h>

Memsubfont*
getmemdefont(void)
{
	char *hdr, *p;
	int n;
	ulong ws, l;
	Fontchar *fc;
	Memsubfont *f;
	int ld;
	Rectangle r;
	Memdata *md;
	Memimage *i;

	/*
	 * make sure data is word-aligned.  this is true with Plan 9 compilers
	 * but not in general.  the byte order is right because the data is
	 * declared as char*, not ulong*.
	 */
	p = (char*)defontdata;
	n = (ulong)p & 3;
	if(n != 0){
		memmove(p+(4-n), p, sizeofdefont-n);
		p += 4-n;
	}
	ld = atoi(p+0*12);
	r.min.x = atoi(p+1*12);
	r.min.y = atoi(p+2*12);
	r.max.x = atoi(p+3*12);
	r.max.y = atoi(p+4*12);
	/* build image by hand, using existing data. */
	i = malloc(sizeof(Memimage));
	if(i == nil)
		return nil;
	md = malloc(sizeof(Memdata));
	if(md == nil){
		free(i);
		return nil;
	}
	
	p += 5*12;

	ws = (8*sizeof(ulong))>>ld;
	l = wordsperline(r, ld);
	md->data = (ulong*)p;	/* ick */
	i->data = md;
	i->zero = l*r.min.y;
	if(r.min.x >= 0)
		i->zero += r.min.x/ws;
	else
		i->zero -= (-r.min.x+ws-1)/ws;
	i->zero = -i->zero;
	i->width = l;
	i->ldepth = ld;
	i->r = r;
	i->clipr = r;
	i->repl = 0;

	hdr = p+Dy(r)*l*sizeof(ulong);
	n = atoi(hdr);
	p = hdr+3*12;
	fc = malloc(sizeof(Fontchar)*(n+1));
	if(fc == 0){
		freememimage(i);
		return 0;
	}
	_unpackinfo(fc, (uchar*)p, n);
	f = allocmemsubfont("*default*", n, atoi(hdr+12), atoi(hdr+24), fc, i);
	if(f == 0){
		freememimage(i);
		free(fc);
		return 0;
	}
	return f;
}
