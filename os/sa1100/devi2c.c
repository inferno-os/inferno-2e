/*
 * devi2c -
 *
 * Simple namespace interface for i2c bus debugging.
 */
#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"io.h"
#include	"i2c.h"

enum {
	Qi2c = 1,
	Qi2cctl,
	Qi2cstat,
};

Dirtab i2cdir[]= {
	{ "i2c",	{ Qi2c, 0 },		0,	0664 },		/* raw binary I2C interface */
	{ "i2cctl",	{ Qi2cctl, 0 },		0,	0222 },		/* pin set/clear interface */
	{ "i2cstat", 	{ Qi2cstat, 0 },	0,	0444 }
};

static void
i2creset(void)
{
}

static Chan*
i2cattach(char* spec)
{
	return devattach('J', spec);
}

static int	 
i2cwalk(Chan* c, char* name)
{
	return devwalk(c, name, i2cdir, nelem(i2cdir), devgen);
}

static void	 
i2cstat(Chan* c, char* dp)
{
	devstat(c, dp, i2cdir, nelem(i2cdir), devgen);
}

static Chan*
i2copen(Chan* c, int omode)
{
	return devopen(c, omode, i2cdir, nelem(i2cdir), devgen);
}

static void	 
i2cclose(Chan*)
{
}

static long	 
i2cread(Chan* c, void* buf, long n, ulong offset)
{
	char regbuf[64];
	int rc;
	extern unsigned char i2c_iactl[];

	if(c->qid.path & CHDIR)
		return devdirread(c, buf, n, i2cdir, nelem(i2cdir), devgen);
	
	switch(c->qid.path){
	case Qi2c:
		if (n != 1)                             /* can only read a byte at a time */
                        error(Ebadusefd);

                if ((offset > 0xff) | (offset & 0x01))  /* addresses 0 - 0xff with bit 0 = 0 */
                        error(Ebadusefd);

                rc = i2c_read_byte(offset);

		if (rc < 0)
			error(Eio);

		*(unsigned char *)buf = (unsigned char)buf;

		return n;
		break;

	case Qi2cstat:
		sprint(regbuf, "[0x40] = %2.2ux => %2.2ux", i2c_iactl[0], i2c_read_byte(0x40));
		break;

	default:
		error(Ebadarg);
		return 0;
	}
	return readstr(offset, buf, n, regbuf);
}

static long	 
i2cwrite(Chan* c, void* buf, long n, ulong offset)
{
	char tmpbuf[64];
	char *field[2];
	int  nf, rc;


	switch(c->qid.path & ~CHDIR) {

	case Qi2c:
		if (n != 1)				/* can only write a byte at a time */
			error(Ebadusefd);

		if ((offset > 0xff) | (offset & 0x01))  /* addresses 0 - 0xff with bit 0 = 0 */
			error(Ebadusefd);

		i2c_write_byte(offset, *(unsigned char *)buf);
		break;

	case Qi2cctl:
		if(offset!=0)
			error(Ebadarg);

		if(n >= sizeof(tmpbuf))			/* make a copy of the buffer */
			n = sizeof(tmpbuf)-1;
		strncpy(tmpbuf, buf, n);
		tmpbuf[n] = 0;

		nf = parsefields(tmpbuf, field, 2, " \t\n");
		if (nf != 2)
			error(Ebadarg);

		if (strcmp(field[0], "set") == 0) 
			i2c_setpin(strtol(field[1], 0, 10));
		else if (strcmp(field[0], "clear") == 0) 
			i2c_clrpin(strtol(field[1], 0, 10));
		else 
			error(Ebadarg);
		break;

	default:
		error(Ebadusefd);
		return 0;
	}
	return n;
}


Dev i2cdevtab = {
	'J',
	"i2c",

	i2creset,
	devinit,
	i2cattach,
	devdetach,
	devclone,
	i2cwalk,
	i2cstat,
	i2copen,
	devcreate,
	i2cclose,
	i2cread,
	devbread,
	i2cwrite,
	devbwrite,
	devremove,
	devwstat,
};


