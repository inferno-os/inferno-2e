#include <lib9.h>
#include <kernel.h>
#include <image.h>

#define	CHUNK	8000

Image*
readimage(Display *d, int fd, int dolock)
{
	char hdr[5*12+1];
	int dy;
	uint l, n;
	int m;
	int miny, maxy;
	Rectangle r;
	int ld, locked;
	uchar *tmp;
	Image *i;

	if(libreadn(fd, hdr, 11) != 11)
		return nil;
	if(memcmp(hdr, "compressed\n", 11) == 0)
		return creadimage(d, fd, dolock);
	if(libreadn(fd, hdr+11, 5*12-11) != 5*12-11)
		return nil;
	locked = 0;
	/* check format of ldepth as a kind of magic number test */
	for(m=0; m<10; m++)
		if(hdr[m] != ' '){
			kwerrstr("readimage: bad format");
			return nil;
		}
	if(hdr[11] != ' ')
		return nil;
	ld = ((int)hdr[10])-'0';
	if(ld<0 || ld>3){
		kwerrstr("readimage: bad ldepth %d", ld);
		return nil;
	}
	r.min.x = atoi(hdr+1*12);
	r.min.y = atoi(hdr+2*12);
	r.max.x = atoi(hdr+3*12);
	r.max.y = atoi(hdr+4*12);
	if(r.min.x>r.max.x || r.min.y>r.max.y){
		kwerrstr("readimage: bad rectangle", ld);
		return nil;
	}

	miny = r.min.y;
	maxy = r.max.y;

	l = bytesperline(r, ld);
	if(dolock)
		locked = lockdisplay(d, 0);
	i = allocimage(d, r, ld, 0, 0);
	if(locked)
		unlockdisplay(d);
	if(i == nil)
		return nil;
	tmp = malloc(CHUNK);
	if(tmp == nil)
		goto Err;
	while(maxy > miny){
		dy = maxy - miny;
		if(dy*l > CHUNK)
			dy = CHUNK/l;
		n = dy*l;
		m = libreadn(fd, tmp, n);
		if(m != n){
			kwerrstr("readimage: read count %d not %d: %r", m, n);
   Err:
			if(dolock)
				locked = lockdisplay(d, 0);
   Err1:
 			freeimage(i);
			if(locked)
				unlockdisplay(d);
			free(tmp);
			return nil;
		}
		if(dolock)
			locked = lockdisplay(d, 0);
		if(loadimage(i, Rect(r.min.x, miny, r.max.x, miny+dy), tmp, CHUNK) <= 0)
			goto Err1;
		if(locked)
			unlockdisplay(d);
		miny += dy;
	}
	free(tmp);
	return i;
}
