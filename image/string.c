#include <lib9.h>
#include <image.h>

enum
{
	Max = 100
};

Point
string(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s)
{
	return _string(dst, pt, src, sp, f, s, nil, 1<<24, dst->clipr);
}

Point
stringn(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, int len)
{
	return _string(dst, pt, src, sp, f, s, nil, len, dst->clipr);
}

Point
runestring(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r)
{
	return _string(dst, pt, src, sp, f, nil, r, 1<<24, dst->clipr);
}

Point
runestringn(Image *dst, Point pt, Image *src, Point sp, Font *f, Rune *r, int len)
{
	return _string(dst, pt, src, sp, f, nil, r, len, dst->clipr);
}

Point
_string(Image *dst, Point pt, Image *src, Point sp, Font *f, char *s, Rune *r, int len, Rectangle clipr)
{
	int n, wid, max;
	ushort cbuf[Max], *c, *ec;
	uchar *b;
	char *subfontname;
	char **sptr;
	Rune **rptr;
	Font *def;

	if(s == nil){
		s = "";
		sptr = nil;
	}else
		sptr = &s;
	if(r == nil){
		r = (Rune*) L"";
		rptr = nil;
	}else
		rptr = &r;
	while((*s || *r) && len){
		max = Max;
		if(len < max)
			max = len;
		n = cachechars(f, sptr, rptr, cbuf, max, &wid, &subfontname);
		if(n > 0){
			b = bufimage(dst->display, 47+2*n);
			if(b == 0){
				fprint(2, "string: %r\n");
				break;
			}
			b[0] = 's';
			BPLONG(b+1, dst->id);
			BPLONG(b+5, src->id);
			BPLONG(b+9, f->cacheimage->id);
			BPLONG(b+13, pt.x);
			BPLONG(b+17, pt.y+f->ascent);
			BPLONG(b+21, clipr.min.x);
			BPLONG(b+25, clipr.min.y);
			BPLONG(b+29, clipr.max.x);
			BPLONG(b+33, clipr.max.y);
			BPLONG(b+37, sp.x);
			BPLONG(b+41, sp.y);
			BPSHORT(b+45, n);
			b += 47;
			ec = &cbuf[n];
			for(c=cbuf; c<ec; c++, b+=2)
				BPSHORT(b, *c);
			pt.x += wid;
			agefont(f);
			len -= n;
		}
		if(subfontname){
			if(getsubfont(f->display, subfontname) == 0){
				def = f->display->defaultfont;
				if(def && f!=def)
					f = def;
				else
					break;
			}
		}
	}
	return pt;
}
