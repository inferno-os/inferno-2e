#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "kbd.h"

void
wait_keyrelease(void)
{
	delay(1000);
	while(kbd_charav()) {
		kbd_getc();
		print("Please release all keys\r");
		delay(500);
	}
	print("                       \r");
}

