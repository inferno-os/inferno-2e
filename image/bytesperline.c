#include <lib9.h>
#include <image.h>

static
int
unitsperline(Rectangle r, int ld, int bits)
{
	ulong ws, l, t;

	ws = bits>>ld;	/* pixels per unit */
	if(r.min.x >= 0){
		l = (r.max.x+ws-1)/ws;
		l -= r.min.x/ws;
	}else{			/* make positive before divide */
		t = (-r.min.x)+ws-1;
		t = (t/ws)*ws;
		l = (t+r.max.x+ws-1)/ws;
	}
	return l;
}

int
wordsperline(Rectangle r, int ld)
{
	return unitsperline(r, ld, 8*sizeof(ulong));
}

int
bytesperline(Rectangle r, int ld)
{
	/* speed up common case */
	if (ld == 3)
		return (r.max.x - r.min.x);

	return unitsperline(r, ld, 8);
}
