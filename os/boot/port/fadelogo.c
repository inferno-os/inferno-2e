#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "screen.h"
#include "custom.h"
#include "bpi.h"

void
fadeinlogo(const char *fname, ulong ms, ulong x)
{
	int y, c, d;
	const char *logoname;

	d = vd_hgt >= 400 ? 2 : 3;
	fadeout(0);
	screen_clear(0);
	if(!(logoname = getenv("logofile")))
		logoname = fname;
	if(showimage(logoname) < 0) {
		char buf[80];
		sprint(buf, "%r");
		screen_putstr(0, 10, buf);
	}
	for(y=0; y<128>>d; y++) {
		c = 255-(((y/(8/(d*2)))<<4)|(y/(8/(d*2))));
		screen_hline(0,vd_wid-1,y,c);
		screen_hline(0,vd_wid-1,vd_hgt-y-1,c);
	}

	y = ((y<<d)+((bpi->flags & BP_FLAG_DEBUG)
			?  (1<<4)*3*83 : 6260459-2*3*41*47*337))<<1;
	screen_putstr(vd_wid-fontwid*4, vd_hgt-((fonthgt*12)>>d), (char*)&y);
	screen_flush();
	fadein(ms, x);
}

