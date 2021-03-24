#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"

int
cmd_reset(int, char **, int *nargv)
{
	reboot(nargv[1]);
	return -1;
}


int
cmd_watchdog(int, char **, int *nargv)
{
	timer_setwatchdog(nargv[1]*TIMER_HZ);
	return 0;
}


//int
//getflashbase(void) { return conf.flashbase; }

//void
//setflashbase(int b) { conf.flashbase = b; }

int
cmd_va2pa(int, char **, int *nargv)
{
	print("%ux -> %ux\n", nargv[1], va2pa((void*)nargv[1]));
	return 0;
}

static int
cmd_bpi(int, char **, int *)
{
	printbootinfo();
	return 0;
}

void
cmdmisclink()
{
	addcmd('r', cmd_reset, 0, 1, "reset");
	addcmd('W', cmd_watchdog, 1, 1, "watchdog");
	addcmd('V', cmd_va2pa, 1, 1, "va->pa");
	addcmd('B', cmd_bpi, 0, 0, "bpi");
	//nbindenv("flash", getflashbase, setflashbase); 
}

