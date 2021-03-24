#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mcp.h"
#include "../port/screen.h"
#include "ucbtouch.h"

extern int touch_read_delay;

/* this can be uncommented and used to update the default touchcal table
static void
printtouchcal()
{
	TouchCal *tc = &touchcal;
	int i,n;
	print("TouchCal touchcal = {\n\t{\n\t    ");
	for(i=0; i<4; i++)
		print("{%d, %d}, ", tc->p[i].x, tc->p[i].y);
	print("\n\t},\n\t{\n");
	for(n=0; n<4; n++) {
		print("\t    {\t");
		for(i=0; i<4; i++)
			print("{%d, %d}, ", tc->r[n][i].x, tc->r[n][i].y);
		print("},\n");
	}
	print("\t},\n\t{\n");
	for(n=0; n<4; n++) 
		print("\t    {\t%d, %d, %d, %d, %d, %d },\n",
			tc->t[n].xxm, tc->t[n].xym,
			tc->t[n].yxm, tc->t[n].yym,
			tc->t[n].xa, tc->t[n].ya);
	print("\t},\n\t{%d, %d},\n",
			tc->err.x, tc->err.y);
	print("\t{%d, %d},\n",
			tc->var.x, tc->var.y);
	print("\t%d, %d,\n",
			tc->ptp, tc->ptr);
	print("};\n");
}
*/

static void
docal(int p, int x, int y)
{
	screen_fillbox(x-5, y-5, x+5, y+5, 0xff);
	screen_hline(x-3, x+3, y, 0);
	screen_fillbox(x, y-3, x, y+3, 0);
	screen_flush();
	touchrawcal(p, x, y);
	screen_fillbox(x-5, y-5, x+5, y+5, 0xff);
	screen_flush();
}


int
touchtest(int a, int b)
{
	int i;
	int c = 0xff;
	int px=0, py=0;

	if(b) {
		touchcal.ptp = b;
		touchcal.ptr = b*2/3;
	}
	if(a==2) {
		do {
			docal(0, 10, 10);
			docal(1, vd_wid-10, 10);
			docal(2, vd_wid-10, vd_hgt-10);
			docal(3, 10, vd_hgt-10);
		} while(!touchcalibrate());
		// printtouchcal();
		return 0;
	}
	if(a==1) {
		for(;;) {
			int i;
			int x;
			int y = vd_hgt >> 4;
			int xs = (vd_wid < 512) ? 2
				 : (vd_wid < 1024) ? 1
				 : 0;
			int xp = (vd_wid-(1024>>xs)) >>2;
			for(i=TOUCH_READ_X1; i<=TOUCH_READ_RY2; i++) {
				mcptouchsetup(i);
				x = xp + (mcpadcread(i) >> xs);
				screen_fillbox(xp, y, x, y+3, 0x00);
				screen_fillbox(x+1, y, vd_wid+1-xp, y+3, 0xff);
				y += vd_hgt >> 4;
			}
		}
	}

	for(i=0; i<vd_wid; i++)
		screen_fillbox(i, vd_hgt-6, i, vd_hgt-1, i*256/vd_wid);

	for(;;) {	/* Loop forever */
		int x, y, pc;

/*
		if(kbd_charav()) {
			switch(kbd_getc()) {
			case '0':  print("%d  \r", --touch_read_delay); break;
			case '1':  print("%d  \r", ++touch_read_delay); break;
			case '2':  print("%d  \r", touch_read_delay -= 10); break;
			case '3':  print("%d  \r", touch_read_delay += 10); break;
			case '4':  print("%d  \r", touch_read_delay -= 100); break;
			case '5':  print("%d  \r", touch_read_delay += 100); break;
			}
		}
*/
		if(!touchpressed())
			continue;
		pc = 0;
		do {
			int i;
			for(i=0; i<3; i++)
				if(touchreadxy(&x, &y)) {
					if(y < 0) {
						if(x < 0)
							return 0;
					} else if(y >= vd_hgt-6)
						c = x*256/vd_wid;
					else {
						if(pc > 0) {
							px = (px+x)>>1;
							py = (py+y)>>1;
							screen_fillbox(px, py,
								px+1, py+1, c);
						}
						screen_fillbox(x, y,
							x+1, y+1, c);
						screen_flush();
						px = x; py = y;
						pc++;
					}
					break;
				}
			delay(1);
		} while(!touchreleased());
	}
	return 0;
}


int
cmd_touchtest(int, char**, int *nargv)
{
	return touchtest(nargv[1], nargv[2]);
}

void
cmdtouchlink()
{
	addcmd('t', cmd_touchtest, 0, 2, "touchtest");
}

