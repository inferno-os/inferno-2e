#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "io.h"
#include "bpi.h"
#include "mcp.h"

enum {
	/*
	 *	GPIO asignments.  Not all used in this file, but gathered here for
	 *	documental purposes.
	 */
	gpio_irq_codec =	22,		/* in, irq from ucb1200 */
};


void
archreset(void)
{
	GpioReg *gpio = GPIOREG;
	ulong b;

	/* put the hardware in a known state */
	gpio->gfer = 0;
	gpio->grer = 0;
	gpio->gedr = gpio->gedr;	/* why? */
	gpio->gpdr = 0;

	mcpinit();
}

void
archconfinit(void)
{	
}

void
dbgled(int n, int v)
{
	GpioReg *gpio = GPIOREG;
	if(n == 0) {
		gpio->gpdr |= BIT(8)|BIT(9);
		if(v&1)
			gpio->gpcr = BIT(8);	// LED on
		else
			gpio->gpsr = BIT(8);	// LED off
		if(v&2)
			gpio->gpcr = BIT(9);	// LED on
		else
			gpio->gpsr = BIT(9);	// LED off
	}
}

static int backlight = 0;
static int brightness = 0;
static int contrast = 0x20;

static void
updatecodecio(void)
{
}

void
lcd_setbacklight(int on)
{
}

void
lcd_setbrightness(ushort level)
{
}

void
lcd_setcontrast(ushort level)
{
}

int
archflash12v(int on)
{
	return 1;
}

int
archflashwp(int /*wp*/)
{
	return 1;
}

