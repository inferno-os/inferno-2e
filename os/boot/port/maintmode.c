#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "screen.h"
#include "bpi.h"

extern int localcons;

void
maintenance_mode(void)
{
	stdout = conout;
	stdin = conin;
	text_bg = 0xc9;		// dark blue
	text_fg = 0x00;		// white
	screen_clear(-1);
	text_wid = 2;
	text_hgt = 2;
	print("\nEntering maintenance mode\n\n");
	print("For authorized personnel only!\n");
	text_wid = 1;
	text_hgt = 1;	
	bpi->flags |=  BP_FLAG_DEBUG;
	printbootinfo();
	print_title();
	localcons = 1;
	ioloop();
}

