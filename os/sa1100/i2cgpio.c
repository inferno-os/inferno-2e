/*
 *	I2C master emulation using GPIO pins.
 *	7 bit addressing only.
 */
#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"io.h"
#include	"i2c.h"

/* GPIO bitmasks */
static unsigned int sda;
static unsigned int scl;


/* set pin level high by disabling output drive and allowing pull-up to work */
static void
i2c_set(int pin)
{
	*GPDR &= ~pin;	/* configure pin as input */
}

/* set pin level low with output drive */
static void
i2c_clear(int pin)
{
	*GPCR = pin;	/* set pin output low */
	*GPDR |= pin;	/* configure pin as output */
}

static int
i2c_getack(void)
{
	/* scl is low, sda is not defined */

	i2c_set(sda);		/* set data high */
	timer_delay(US2TMR(3));

	i2c_set(scl);		/* raise clock */
	timer_delay(US2TMR(5));

	/* check for ack from slave! */
	if (*GPLR & sda)
		print("I2C: Warning did not get ack!\n");

	i2c_clear(sda);		/* lower data */
	i2c_clear(scl);		/* lower clock */
	timer_delay(US2TMR(3));

	/* scl is low, sda is low */
	return 1;
}


static void
i2c_putack(void)
{
	/* scl is low, sda is not defined */

	timer_delay(US2TMR(3));		/* lower data */
	i2c_clear(sda);

	i2c_set(scl);			/* pulse clock */
	timer_delay(US2TMR(5));

	i2c_clear(scl);			/* lower clock */
	timer_delay(US2TMR(3));

	/* scl is low, sda is low */
}


static void
i2c_putbyte(unsigned char b)
{
	/* start condition has been sent */
	/* scl is low, sda is low */

	unsigned char m;
	for(m=0x80; m; m >>= 1) {
		
		/* set data bit */
		if(b&m)
			i2c_set(sda);
		else
			i2c_clear(sda);

		/* pulse clock */
		timer_delay(US2TMR(3));
		i2c_set(scl);
		timer_delay(US2TMR(5));
		i2c_clear(scl);
		timer_delay(US2TMR(3));
	}

	i2c_clear(sda);
	/* scl is low, sda is low */
}


static unsigned char
i2c_getbyte(void)
{
	/* start condition, address and ack been done */
	/* scl is low, sda is high */
	unsigned char data = 0x00;
	int i;

	i2c_set(sda);
	for (i=7; i >= 0; i--) {

		timer_delay(US2TMR(3));

                /* raise clock */
                i2c_set(scl);
                timer_delay(US2TMR(5));

		/* sample data */
		data |= ((*GPLR & sda) ? 1 : 0) << i;

		/* lower clock */
		i2c_clear(scl);
		timer_delay(US2TMR(3));
	}

	i2c_clear(sda);
	return data;
}

/* generate I2C start condition */
static int
i2c_start(void)
{
	/* check that both scl and sda are high */
	if ((*GPLR & (sda | scl)) != (sda | scl)) 
		print("I2C: Bus not clear when attempting start condition\n");

	i2c_clear(sda);			/* lower sda */
	timer_delay(US2TMR(5));

	i2c_clear(scl);			/* lower scl */
	timer_delay(US2TMR(3));

	return 1;
}

/* generate I2C stop condition */	
static int
i2c_stop(void)
{
	/* clock is low, data is low */
	timer_delay(US2TMR(3));

	i2c_set(scl);
	timer_delay(US2TMR(5));

	i2c_set(sda);

	timer_delay(MS2TMR(1));		/* ensure separation between commands */

	return 1;
}

/********************************************************************************/
/*
 * external I2C interface
 */

/* write a byte over the i2c bus */
int
i2c_write_byte(unsigned char addr, unsigned char data)
{
	int rc = 0;

	if(i2c_start() < 0)			/* start condition */
		rc = -1;

	i2c_putbyte(addr & 0xfe);		/* address byte (LSB = 0 -> write) */

	if (i2c_getack() < 0)			/* get ack */
		rc = -2;

	i2c_putbyte(data);			/* data byte */

	if (i2c_getack() < 0)			/* get ack */
		rc = -3;

	if (i2c_stop() < 0)
		rc = -4;			/* stop condition */

	return rc;
}

/* read a byte over the i2c bus */
int
i2c_read_byte(unsigned char addr)
{
	unsigned char data;
	int rc = 0;

	if(i2c_start() < 0)			/* start condition */
		rc = -1;

	i2c_putbyte(addr | 0x01);		/* address byte (LSB = 1 -> read) */

	if(i2c_getack() < 0)			/* get ack */
		rc = -2;

	data = i2c_getbyte();			/* data byte */

	i2c_putack();				/* put ack */

	if (i2c_stop() < 0) 			/* stop condition */
		rc = -4;

	return rc;
}

void
i2c_reset(void)
{
	/* initialise bitmasks */
	sda = (1 << gpio_i2c_sda);
	scl = (1 << gpio_i2c_scl);
	
	/* ensure that both clock and data are high */
	i2c_set(sda);
	i2c_set(scl);
	timer_delay(MS2TMR(5));
}


/*********************************************************************************/
/*
 * external pin set/clear interface
 */
uchar i2c_iactl[2] = { 0xff, 0xff };		/* defaults overridden in arch?????.c */

int
i2c_setpin(int b)
{
	int i = b>>3;

	i2c_iactl[i] |= (1 << (b&7));
	return i2c_write_byte(0x40 | (i << 1), i2c_iactl[i]);
}

int
i2c_clrpin(int b)
{
	int i = b>>3;

	i2c_iactl[i] &= ~(1 << (b&7));
	return i2c_write_byte(0x40 | (i << 1), i2c_iactl[i]);
}

