#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"io.h"

enum{
	Qgpioset = 1,
	Qgpioclear,
	Qgpioedge,
	Qgpioctl,
};

Dirtab gpiodir[]={
	"gpioset",			{Qgpioset, 0},		0,	0664,
	"gpioclear",		{Qgpioclear, 0},		0,	0664,
	"gpioedge",		{Qgpioedge, 0},		0,	0664,
	"gpioctl",			{Qgpioctl,0},		0,	0664,
};

static void
gpioreset( void )
{
}

static Chan*
gpioattach(char* spec)
{
	return devattach('G', spec);
}

static int	 
gpiowalk(Chan* c, char* name)
{
	return devwalk(c, name, gpiodir, nelem(gpiodir), devgen);
}

static void	 
gpiostat(Chan* c, char* dp)
{
	devstat(c, dp, gpiodir, nelem(gpiodir), devgen);
}

static Chan*
gpioopen(Chan* c, int omode)
{
	return devopen(c, omode, gpiodir, nelem(gpiodir), devgen);
}

static void	 
gpioclose(Chan*)
{
}

static long	 
gpioread(Chan* c, void* buf, long n, ulong offset)
{
	char regbuf[255];
	
	if(c->qid.path & CHDIR)
		return devdirread(c, buf, n, gpiodir, nelem(gpiodir), devgen);
	
	switch(c->qid.path){
	case Qgpioset:
		sprint( regbuf, "%8.8lux", *GPLR);
		break;
	case Qgpioclear:
		sprint( regbuf, "%8.8lux", *GPLR);
		break;
	case Qgpioedge:
		sprint( regbuf, "%8.8lux", *GEDR);
		break;
	case Qgpioctl:
		sprint( regbuf, "GPDR:%8.8lux\nGRER:%8.8lux\nGFER:%8.8lux\nGAFR:%8.8lux\nGPLR:%8.8lux\n", *GPDR,*GRER,*GFER,*GAFR,*GPLR);
		break;
	default:
		error(Ebadarg);
		return 0;
	}
	return readstr( offset, buf, n, regbuf);
	error(Ebadarg);
	return 0;
}

static long	 
gpiowrite(Chan* c, void* buf, long n, ulong offset)
{
	char tmpbuf[256];
	char *field[3];
	char cmd;
	ulong pin, set;

	USED(c,buf,n,offset);

	if(offset!=0)
		error(Ebadarg);
	switch(c->qid.path & ~CHDIR){
	case Qgpioset:
		*GPSR = strtol( buf, 0, 16 );
		break;
	case Qgpioclear:
		*GPCR = strtol( buf, 0, 16 );
		break;
	case Qgpioedge:
		*GEDR = strtol( buf, 0, 16 );
		break;
	case Qgpioctl:
		if(n >= sizeof(tmpbuf))
			n = sizeof(tmpbuf)-1;
		strncpy(tmpbuf, buf, n);
		tmpbuf[n] = 0;	
		if (parsefields( tmpbuf, field, 3, " ") == 3) {
			cmd = *field[0];
			pin = strtol( field[1], 0, 16);
			set = strtol( field[2], 0, 2);
			switch(cmd) {
			case 'd':
				if ( set )
					*GPDR |= 1 << pin;
				else
					*GPDR &= ~(1 << pin);
				return n;
			case 'r':
				if ( set )
					*GRER |= 1 << pin;
				else
					*GRER &= ~(1 << pin);
				return n;
			case 'f':
				if ( set )
					*GFER |= 1 << pin;
				else
					*GRER &= ~(1 << pin);
				return n;
			case 'a':
				if ( set )
					*GAFR |= 1 << pin;
				else
					*GAFR &= ~(1 << pin);
				return n;
			default:
				error(Ebadusefd);
				return 0;
			}
		}
		break;
	default:
		error(Ebadusefd);
		return 0;
	}
	return n;
}




Dev gpiodevtab = {
	'G',
	"gpio",

	gpioreset,
	devinit,
	gpioattach,
	devdetach,
	devclone,
	gpiowalk,
	gpiostat,
	gpioopen,
	devcreate,
	gpioclose,
	gpioread,
	devbread,
	gpiowrite,
	devbwrite,
	devremove,
	devwstat,
};


