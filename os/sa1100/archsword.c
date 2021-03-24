#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"io.h"
#include	"image.h"
#include	<memimage.h>
#include	"screen.h"
#include "bootparam.h"

enum {
	/* U20, PCF8574 @i2c addr 0x40 */
	I2C_MUTE_SPKR =		0,
	I2C_MUTE_HEADSET =	1,
	I2C_VCC_5EN =		2,
	I2C_LCD_EN =			3,
	I2C_TAD_FL_WP =		4,		/* negative logic */
	I2C_RS232_ON_OFF =	5,

	/* U21, PCF8574 @i2c addr 0x42 */
	I2C_LCD_BACK_DIS =	10,
	I2C_LCD_YEE_DIS =		11,
	I2C_CHRG1 =			12,
	I2C_CHRG2 =			13,

	/*
	 *	GPIO asignments.  Not all used in this file, but gathered here for
	 *	documental purposes.
	 */
	gpio_switch =		0,		/* in, S7 */
	gpio_handset =	1,		/* in, S9, handset off hook */
	gpio_cid =		2,		/* out, enable DTAD DAA caller ID */
	gpio_batt_thrm =	3,		/* in, some smartbat thingy */
	gpio_ring =		4,		/* in, DTAD DAA ring indicator */
	gpio_hook =		5,		/* out, DTAD DAA off hook */
	gpio_i2c_sda =		6,		/* in/out, as per i2c protocol */
	gpio_i2c_scl =		7,		/* in/out, as per i2c protocol */
	gpio_tpad_clk =	8,		/* out, clock for touchpad connector */
	gpio_tpad_data =	9,		/* in, data in for touchpad connector */
	gpio_mosi =		10,		/* out, spi (to kbd interface) */
	gpio_miso =		11,		/* in, spi (to kbd interface) */
	gpio_sclk =		12,		/* out, spi (to kbd interface) */
	gpio_sfrm =		13,		/* out, spi (to kbd interface) */
	gpio_flashwp =		14,		/* out, negative logic */
	gpio_irq_dtad =	15,		/* in, ack- line from dtad */
	gpio_kpad_c1 =	16,		/* in, keypad column 1 */
	gpio_kpad_c2 =	17,		/* in, keypad column 1 */
	gpio_kpad_c3 =	18,		/* in, keypad column 1 */
	gpio_kpad_c4 =	19,		/* in, keypad column 1 */
	gpio_debug =		20,		/* in, S5, debug/normal switch (polarity???) */
	gpio_irda_sd =		21,		/* out, purpose unclear (XXX TFDS6000 data sheet?) */

	gpio_wkup =		23,		/* out, to kbd interface -- negative logic */
	gpio_wuko =		24,		/* in???, to kbd interface */
	gpio_atn =		25,		/* in, spi (to kbd interface) -- negative logic */
	gpio_led1 =		26,		/* out */
	gpio_led2 =		27,		/* out */

	/* CODEC GPIO's, via MCP */
	MODEMHOOK=	BIT(8),		/* out, UCB DAA off hook */
	RINGIND=	BIT(7),			/* in, UCB DAA ring indicator */
};

int gpio_irq_ucb1200 = 22;

extern int cflag;
extern int consoleprint;
extern int redirectconsole;
extern int main_pool_pcnt;
extern int heap_pool_pcnt;
extern int image_pool_pcnt;
extern int kernel_pool_pcnt;
extern char debug_keys;
extern Vmode default_vmode;

int smodem_HybridDelay = 0x23;	/* see comment in devsm.c */

int i2c_reset(void);
int i2c_setpin(int b);
int i2c_clrpin(int b);

void
archreset(void)
{
	/* put the hardware in a known state */
	*GFER = 0;
	*GRER = 0;
	*GEDR = *GEDR;
	*GPDR = (1<<gpio_flashwp)|(1<<gpio_led1)|(1<<gpio_led2);

	dmareset();
	i2c_reset();

	/* init codec, put modem on hook */
	mcpinit();
	mcpgpiosetdir(MODEMHOOK, MODEMHOOK);
}

void
archconfinit(void)
{
	/* allow bootparam environment to override defaults */
	bpoverride("cflag", &cflag);
	bpoverride("consoleprint", &consoleprint);
	bpoverride("redirectconsole", &redirectconsole);
	bpoverride("kernel_pool_pcnt", &kernel_pool_pcnt);
	bpoverride("main_pool_pcnt", &main_pool_pcnt);
	bpoverride("heap_pool_pcnt", &heap_pool_pcnt);
	bpoverride("image_pool_pcnt", &image_pool_pcnt);
	bpoverride_uchar("vidhz", &default_vmode.hz);
	bpoverride_uchar("debugkeys", (uchar*)&debug_keys);
	bpoverride("smhybrid", &smodem_HybridDelay);
	bpoverride("textwrite", &conf.textwrite);

	conf.topofmem = bootparam->himem;
	conf.flashbase = bootparam->flashbase;
	conf.cpuspeed = bootparam->cpuspeed;
	conf.pagetable = bootparam->pagetable;

	conf.usebabycache = 1;
	conf.cansetbacklight = 1;
	conf.cansetcontrast = 0;
	conf.remaplo = 1;
}

void
archreboot(void)
{
	bootparam->reboot(1);
}

void
lights(ulong x)
{
	if (x&1)
		*GPSR = (1<<gpio_led1);
	else
		*GPCR = (1<<gpio_led1);
	if (x&2)
		*GPSR = (1<<gpio_led2);
	else
		*GPCR = (1<<gpio_led2);
}

void
lcd_setbacklight(int on)
{
	if (on)
		i2c_clrpin(I2C_LCD_BACK_DIS);
	else
		i2c_setpin(I2C_LCD_BACK_DIS);
}

void
lcd_setbrightness(ushort)
{
}

void
lcd_setcontrast(ushort)
{
}

int
archflash12v(int /*on*/)
{
        return 1;	/* whatever */
}

void
archflashwp(int wp)
{
	if (wp)
		*GPCR = (1<<gpio_flashwp);	/* negative logic (says schematic) */
	else
		*GPSR = (1<<gpio_flashwp);
}

int	touch_read_delay = 100;	/* usec between setup and first reading */
int	touch_l2nreadings = 5;	/* log2 of number of readings to take */
int	touch_minpdelta = 40;	/* minimum pressure difference to 
			   detect a change during calibration */
int	touch_filterlevel = 900; /* -1024(off) to 1024(full): 1024*cos(angle) */


TouchCal touchcal = {
	{
		{10, 10}, {630, 10}, {630, 470}, {10, 470},
	},
	{
		{   {779, 148}, {84, 165}, {88, 845}, {779, 764}, },
		{   {876, 784}, {147, 842}, {129, 160}, {827, 161}, },
		{   {836, 148}, {129, 164}, {145, 845}, {869, 762}, },
		{   {876, 784}, {148, 842}, {129, 160}, {827, 161}, },
	},
	{
		{   -58458, 343, 1084, 44329, 46037734, -5329820 },
		{   -55618, 1467, -3509, -44108, 47320930, 39557584 },
		{   -57447, 1349, 1001, 44249, 49015133, -5258007 },
		{   -55689, 1551, -3513, -44105, 47341696, 39559572 },
	},
	{10, 21},
	{2, 2},
	144, 111,
};

int
archhooksw(int offhook)
{
	mcpgpiowrite(MODEMHOOK, offhook ? MODEMHOOK : 0);
	return 0;
}

void
archspeaker(int, int)
{
}

/*
 *	SWoRD I2C hardware: SDA on GPIO 6, SCL on GPIO 7.
 *	Two latches.  Smart battery optional.  Maybe something else.
 *
 *	This is so SWoRD dependant, that it probably belongs here
 *	rather than in a separate i2c.c file...
 *
 *	We don't implement the read operation.
 *
 *	I'm not happy with the interface.  Tempted to make
 *	some or all of it call error() instead of returning -1.
 */
static uchar i2c_iactl[2] = { 0xff, 0xf3 };

static int
i2c_set(int pin)
{
	ulong b = 1<<pin;

	*GPDR &= ~b;
	timer_delay(US2TMR(1) >> 1);
	return (*GPLR & b) == 0 ? -1 : 1;
}

static int
i2c_clear(int pin)
{
	ulong b = 1<<pin;

	*GPCR = b;
	*GPDR |= b;
	timer_delay(US2TMR(2));
	return (*GPLR & b) != 0 ? -1 : 1;
}

static int
i2c_getack(void)
{
	timer_delay(US2TMR(2));
	if(i2c_set(gpio_i2c_sda) > 0)
		return -1;
	i2c_set(gpio_i2c_scl);
	timer_delay(US2TMR(1));
	i2c_clear(gpio_i2c_scl);
	timer_delay(US2TMR(5));
	return 1;
}

static void
i2c_putbyte(int b)
{
	int m;

	for(m=0x80; m; m >>= 1) {
		if(b&m)
			i2c_set(gpio_i2c_sda);
		else
			i2c_clear(gpio_i2c_sda);
		i2c_clear(gpio_i2c_scl);
		timer_delay(US2TMR(1));
		i2c_set(gpio_i2c_scl);
		timer_delay(US2TMR(1));
		i2c_clear(gpio_i2c_scl);
	}
}

static int
i2c_start(void)
{
	if((*GPLR & (1<<gpio_i2c_sda)) == 0)
		return -1;
	if((*GPLR & (1<<gpio_i2c_scl)) == 0)
		return -1;
	if(i2c_clear(gpio_i2c_sda) < 0)
		return -1;
	timer_delay(MS2TMR(5));
	if(i2c_clear(gpio_i2c_scl) < 0)
		return -1;
	timer_delay(MS2TMR(1));
	return 1;
}
	
static int
i2c_stop(void)
{
	if(i2c_clear(gpio_i2c_sda) < 0)
		return -1;
	timer_delay(MS2TMR(1));
	if(i2c_set(gpio_i2c_scl) < 0)
		return -1;
	timer_delay(MS2TMR(1));
	if(i2c_set(gpio_i2c_sda) < 0)
		return -1;
	timer_delay(MS2TMR(1));
	return 1;
}

static int
i2c_write(int i)
{
	if(i2c_start() < 0)
		return -1;
	i2c_putbyte(0x40 + (i << 1));
	if(i2c_getack() < 0)
		return -2;
	i2c_putbyte(i2c_iactl[i]);
	if(i2c_getack() < 0)
		return -3;
	return i2c_stop() < 0 ? -4 : 1;
}

int
i2c_setpin(int b)
{
	int i = b>>3;

	i2c_iactl[i] |= (1 << (b&7));
	return i2c_write(i);
}

int
i2c_clrpin(int b)
{
	int i = b>>3;

	i2c_iactl[i] &= ~(1 << (b&7));
	return i2c_write(i);
}

int
i2c_reset(void)
{
	int i;

	if(i2c_set(gpio_i2c_sda) < 0)
		return -1;
	if(i2c_clear(gpio_i2c_scl) < 0)
		return -1;
	for(i=0; i<3; i++)
		if(i2c_stop() < 0)
			return -1;
	i2c_write(0);
	i2c_write(1);
	return 0;
}

static void
archdtadspeaker(int on)
{
	if (on)
		i2c_clrpin( I2C_MUTE_SPKR );
	else
		i2c_setpin( I2C_MUTE_SPKR );
}

static void
archdtadhandset(int on)
{
	if (on)
		i2c_clrpin( I2C_MUTE_HEADSET );
	else
		i2c_setpin( I2C_MUTE_HEADSET );
}

/*
static void
archdtadflwp(int on)
{
	if (on)
		i2c_clrpin( I2C_TAD_FL_WP );
	else
		i2c_setpin( I2C_TAD_FL_WP );
}
*/