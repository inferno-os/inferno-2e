#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "io.h"
#include "bpi.h"
#include "mcp.h"

enum {
	gpio_flashwp=	14,
	gpio_irq_codec=	22,		/* in, irq from ucb1200 */
	gpio_led1=	26,
	gpio_led2=	27,	
};


void
archreset(void)
{
	GpioReg *gpio = GPIOREG;
	gpio->gfer = 0;
	gpio->grer = 0;
	gpio->gedr = gpio->gedr;	/* why? */
	gpio->gpdr = (1<<gpio_flashwp)|(1<<gpio_led1)|(1<<gpio_led2);
	mcpinit();
}

void
archconfinit(void)
{	
}

void
dbgled(int /*n*/, int v)
{
	GpioReg *gpio = GPIOREG;
	if(v&1)
		gpio->gpsr = (1<<gpio_led1);
	else
		gpio->gpcr = (1<<gpio_led1);
	if(v&2)
		gpio->gpsr = (1<<gpio_led2);
	else
		gpio->gpcr = (1<<gpio_led2);
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
archflashwp(int wp)
{
	GpioReg *gpio = GPIOREG;
	if(wp)
		gpio->gpcr = (1<<gpio_flashwp);
	else
		gpio->gpsr = (1<<gpio_flashwp);
	return 1;
}

