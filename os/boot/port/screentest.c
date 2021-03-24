#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "screen.h"

extern int rgb2cmap(int, int, int);

int
cmd_test6(int, char **, int *)
{
	int x, y;

	for(y=0; y<16; y++)
		for(x=0; x<16; x++)
			screen_fillbox(384+x*16, 32+y*16,
				384+x*16+15, 32+y*16+15, y*16+x);
	for(x=0; x<16; x++)
		screen_fillbox(384+x*16, 0, 384+x*16+15, 15, x|(x<<4));
	for(x=0; x<256; x++)
		screen_fillbox(384+255-x, 16, 384+255-x, 31, rgb2cmap(x, x, x));
	screen_flush();
	return 0;
}

void
screentestlink()
{
	addcmd('6', cmd_test6, 0, 1, "palette");
}

