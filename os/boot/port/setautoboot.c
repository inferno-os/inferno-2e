#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "screen.h"

void
setautoboot(int on)
{
	text_wid = 1;
	text_hgt = 2;
	stdout = conout;
	print("\n\nautoboot %s... ", on ? "on" : "off");
	if(system(on ? "P/a sboot" : "P/a 0") < 0)
		print("failed\n");
	else
		print("ok\n");
}

