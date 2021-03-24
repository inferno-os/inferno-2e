#include <lib9.h>
#include <image.h>

void
cursor(Point hotspot, Image *bits)
{
	uchar *a;

	a = bufimage(bits->display, 1+4+2*4);
	if(a == 0){
		fprint(2, "image cursor: %r\n");
		return;
	}
	a[0] = 'C';
	BPLONG(a+1, bits->id);
	BPLONG(a+5, hotspot.x);
	BPLONG(a+9, hotspot.y);
	flushimage(bits->display, 0);
}
