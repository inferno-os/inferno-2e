#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

/*
 * Note: this ramdisk driver assumes that memory has been reserved by
 * the bootloader prior to running the kernel.  Currently, this is
 * only done by the SOLO bootloader.  The top of memory used by the
 * kernel also needs to be lowered.
 * i.e. -- say that the real top of memory is 0x2000000, then the bootloader
 * needs to detect this and subtract an appropriate amount (let's say 4MB)
 * and then set the following config variables:
 *	*maxmem=0x1c00000
 *	ramdiskbase=0x1c00000
 *	ramdisksize=0x0400000
 * (presumably, the bootloader also preloads the ramdisk with useful data,
 * like a previously created KFS filesystem that it uncompressed from
 * a floppy disk)
 */

enum{
	Qramdisk = 1,
};

Dirtab ramdiskdir[]={
	"ramdisk",			{Qramdisk,    0},	0,		0666,
};

static uchar *base;

void
ramdiskreset(void)
{
	char *p;
	if((p = getconf("ramdiskbase"))) {
		ulong phys = strtoul(p, 0, 0);
		ulong size;
		p = getconf("ramdisksize");
		size = strtoul(p, 0, 0);
		base = (uchar*)upamalloc(phys, size, 0);
		print("ramdisk: mapped %ux at %ux to %ux\n", size, phys, (ulong)base);
		ramdiskdir[0].length = size;
	}
}

static Chan*
ramdiskattach(char* spec)
{
	return devattach('R', spec);
}

static int	 
ramdiskwalk(Chan* c, char* name)
{
	return devwalk(c, name, ramdiskdir, nelem(ramdiskdir), devgen);
}

static void	 
ramdiskstat(Chan* c, char* dp)
{
	devstat(c, dp, ramdiskdir, nelem(ramdiskdir), devgen);
}

static Chan*
ramdiskopen(Chan* c, int omode)
{
	omode = openmode(omode);
	return devopen(c, omode, ramdiskdir, nelem(ramdiskdir), devgen);
}

static void	 
ramdiskclose(Chan*)
{
}

static long	 
ramdiskread(Chan* c, void* buf, long n, ulong offset)
{
	USED(c);
	if(c->qid.path & CHDIR)
		return devdirread(c, buf, n, ramdiskdir, nelem(ramdiskdir), devgen);
	if(offset+n > ramdiskdir[0].length)
		n = ramdiskdir[0].length-offset;
	if(n <= 0)
		return n;
	memmove( buf, base+offset, n);
	return n;
}



static long	 
ramdiskwrite(Chan* c, void* buf, long n, ulong offset)
{
	USED(c);

	if(offset+n > ramdiskdir[0].length)
		n = ramdiskdir[0].length-offset;
	if(n <= 0)
		return n;
	memmove( base+offset, buf, n );
	return n;
}

Dev ramdiskdevtab = {
	'R',
	"ramdisk",

	ramdiskreset,
	devinit,
	ramdiskattach,
	devdetach,
	devclone,
	ramdiskwalk,
	ramdiskstat,
	ramdiskopen,
	devcreate,
	ramdiskclose,
	ramdiskread,
	devbread,
	ramdiskwrite,
	devbwrite,
	devremove,
	devwstat,
};

