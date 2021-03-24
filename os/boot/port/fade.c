#include <u.h>
#include "dat.h"
#include "fns.h"
#include "screen.h"

static ushort save_contrast;
static ushort save_brightness;

void
fadeout(ulong d)
{
	int i, n;
	save_contrast = getcontrast();
	save_brightness = getbrightness();
	d /= 75;
	n = 16;
	for(i=0x1000; i>=0; i -= n, n += 1) {
		setcontrast((save_contrast*i)>>12);
		if(i <= 0x800)
			setbrightness((save_brightness*i)>>11);
		delay(d);
	}
	setbrightness(0);
	setcontrast(0);
}

void
fadein(ulong d, ulong x)
{
	int i, n;
	d /= 75;
	n = 91;
	for(i=0; i<=0x1000; i += n, n -= 1) {
		setcontrast((save_contrast*i)>>12);
		if(i <= 0x800 + x)
			setbrightness((save_brightness*i)>>11);
		delay(d);
	}
	setbrightness(save_brightness);
	setcontrast(save_contrast);
}

