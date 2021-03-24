#include <lib9.h>
#include <kernel.h>
#include <image.h>

Image *
creadimage(Display *d, int fd, int dolock)
{
	char hdr[5*12+1];
	Rectangle r;
	int ld, nb, miny, maxy;
	uchar *buf, *a;
	Image *i;
	int locked;

	if(libreadn(fd, hdr, 5*12) != 5*12)
		return nil;
	locked = 0;
	ld=atoi(hdr+0*12);
	r.min.x=atoi(hdr+1*12);
	r.min.y=atoi(hdr+2*12);
	r.max.x=atoi(hdr+3*12);
	r.max.y=atoi(hdr+4*12);
	if(ld<0 || ld>3){
		kwerrstr("rdbitmapfile: bad ldepth %d", ld);
		return nil;
	}
	if(r.min.x>r.max.x || r.min.y>r.max.y){
		kwerrstr("rdbitmapfile: bad rectangle", ld);
		return nil;
	}

	if(dolock)
		locked = lockdisplay(d, 0);
	i = allocimage(d, r, ld, 0, 0);
	if(locked)
		unlockdisplay(d);
	if(i == nil)
		return nil;
	buf = malloc(NCBLOCK);
	if(buf == nil)
		goto Errout;
	miny = r.min.y;
	while(miny != r.max.y){
		if(libreadn(fd, hdr, 2*12) != 2*12){
		Errout:
			if(dolock)
				locked = lockdisplay(d, 0);
			freeimage(i);
			if(locked)
				unlockdisplay(d);
			free(buf);
			return nil;
		}
		maxy = atoi(hdr+0*12);
		nb = atoi(hdr+1*12);
		if(maxy<=miny || r.max.y<maxy){
			kwerrstr("readimage: bad maxy %d", maxy);
			goto Errout;
		}
		if(nb<=0 || NCBLOCK<nb){
			kwerrstr("readimage: bad count %d", nb);
			goto Errout;
		}
		if(libreadn(fd, buf, nb)!=nb)
			goto Errout;
		if(dolock)
			locked = lockdisplay(d, 0);
		a = bufimage(i->display, 21+nb);
		a[0] = 'W';
		BPLONG(a+1, i->id);
		BPLONG(a+5, r.min.x);
		BPLONG(a+9, miny);
		BPLONG(a+13, r.max.x);
		BPLONG(a+17, maxy);
		memmove(a+21, buf, nb);
		if(locked)
			unlockdisplay(d);
		miny = maxy;
	}
	free(buf);
	return i;
}
