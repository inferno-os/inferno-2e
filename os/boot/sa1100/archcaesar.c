/*
 *	Et tu, Brutus.
 */

#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "io.h"
#include "bpi.h"

void
archconfinit(void)
{	
}


void
dbgled(int n, int v)
{
	if(n == 0) {
		*GPDR |= BIT(19);
		if(v&1)
			*GPCR = BIT(19);	// LED on
		else
			*GPSR = BIT(19);	// LED off
	}
}

int
archflash12v(int /*on*/)
{
	return 1;	// umm, sure, we got 12V...
}

int
archflashwp(int wp)
{
	if(wp)
		*(ulong*)0x0e000000 &= ~0x20200000;	// disable 5V
	else
		*(ulong*)0x0e000000 |= 0x20200000;	// enable 5V
	return 1;
}

void
lcd_setbacklight(int on)
{	
	if (on)
		*GPSR = BIT(16);
	else
		*GPCR = BIT(16);
}

void
lcd_setbrightness(ushort)
{
}

void
lcd_setcontrast(ushort)
{
}

/*
 *	``What kind of struggle?  _What_ kind of struggle?''
 *	``A political struggle.''   - Monty Python
 */
