#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "screen.h"
#include "vmode.h"
#include "image.h"
#include "vctlr.h"
#include "kbd.h"

extern Vdisplay *vd;

void
lineartrace(void)
{
	int x[4], y[4], dx[4], dy[4], c[4];
	char ch;
	uchar *p1, *p2;
	int i, n, r, s = 0;

	SET(r);
	do {
		screen_clear(0xaa);
		screen_fillbox(4, 4, vd_wid-5, vd_hgt-5, 0xff);
		sprint((char*)x, "%d", s++);
		text_fg = 0;
		screen_putstr(8, 8, (char*)x);
		for(i=1; i<4; i++) {
			x[i] = vd_wid*i/5; y[i] = vd_hgt >> 3;
			dx[i] = 0; dy[i] = 1;
			c[i] = 0x92+i;
		}
		x[0] = vd_wid >> 1; y[0] = (vd_hgt*7) >> 3;
		dx[0] = 0; dy[0] = -1;
		c[0] = 0;
		n = 3;
		while(n && (dx[0] || dy[0])) {
			for(i=0; i<4; i++) {
				int odx = dx[i], ody = dy[i];
				int z;
				if(!odx && !ody)
					continue;
				p1 = &vd_fb[vd_wid*y[i]+x[i]];
				p2 = &vd_fb[vd_wid*(y[i]+dx[i])
							+(x[i]+dy[i])];
				if((*p1&*p2) != 0xff) {
					dx[i] = 0; dy[i] = 0; --n;
					continue;
				} else 
					*p1 = *p2 = c[i];
				if(!i && kbd_charav()) {
					ch = kbd_getc();
					if(ch >= 'h' && ch <= 'l') {
						dx[i] = (ch=='l')-(ch=='h');
						dy[i] = (ch=='j')-(ch=='k');
					} else if(ch == 'q') 
						return;
				}
				for(z=s < 20 ? 20-s : 1; z; z--) {
					r = (r*17335977+12345+s);
					if(i && (vd_fb[vd_wid*(y[i]+dy[i]*z)
						+(x[i]+dx[i]*z)] != 0xff
							|| (r&0xffe)==0xffe)) {
						dx[i] = ody*(1-(r&2));
						dy[i] = odx*(1-(r&2));
					}
				}
                               	ody += dx[i];
                                x[i] += ((ody < 0 ? ody+1 : ody) >> 1)+dx[i];
                                odx += dy[i];
                                y[i] += ((odx < 0 ? odx+1 : odx) >> 1)+dy[i];
			}
			screen_flush();
			if(s<10)
				delay(10-s);
		}
	} while(dx[0] || dy[0]);
}

