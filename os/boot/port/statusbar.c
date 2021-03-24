#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "screen.h"
#include "custom.h"

extern void *vd;	// XXX - hack

void
status_bar(const char *name, int pos, int total)
{
	if(!vd) {
		print("%s: %d/%d\r", name, pos, total);
		if(pos == total)
			print("\n");
		else
			soflush(stdout);
	} else {
		int savetxtwid = text_wid;
		int savetxtfg = text_fg;
		int i, c;
		int b1=vd_hgt-((fonthgt*text_hgt*3) >> 1);
		int b2=vd_hgt-((fonthgt*text_hgt) >> 1);
		char buf[40];
		int xp1=vd_wid*3/8;
		int xp2=vd_wid-fontwid*10;
		int xp=total==0?(vd_wid >> 1):((vd_wid*3) >> 3)
				+((vd_wid >> 1)*pos/total);
		text_wid = 1;
		text_fg = 0xff;
		for(i=vd_hgt-fonthgt*text_hgt*2; i<vd_hgt; i++) {
			c = (i-((b2+b1) >> 1))*16/(fonthgt*text_hgt);
			if(i>=b1 && i<=b2) {
				c = (c < 0) ? 1-c : c;
				screen_hline(0, xp1, i, 0xe2);
				screen_hline(xp2, vd_wid-1, i, 0xe2);
				screen_hline(xp1, xp, i, c|(c<<4));
				screen_hline(xp, xp2, i, (15-c)|((15-c)<<4));
			} else {
				c = (c < 0) ? -1-c : c;
				c = 0xee-((c < 12) ? 3 : 15-c)*4;
				screen_hline(0, vd_wid-1, i, c);
			}
		}
		screen_putstr(fontwid, b1, name);
		sprint(buf, "%d", pos);
		screen_putstr(xp1-(strlen(buf)+1)*fontwid, b1, buf);
		sprint(buf, "%d", total);
		screen_putstr(xp2+fontwid, b1, buf);
		screen_flush();
		text_wid = savetxtwid;
		text_fg = savetxtfg;
	}
}


void
statusbar_erase(void)
{
	if(vd)
		screen_fillbox(0, vd_hgt-fonthgt*text_hgt*2,
			vd_wid-1, vd_hgt-1,
			screen_getpixel(vd_wid-1, vd_hgt-fonthgt*text_hgt*2-1));
}

