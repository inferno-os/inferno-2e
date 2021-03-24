// configuration for systems booted using BPI-compliant bootloader
#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "bpi.h"
#include "../port/flash.h"

extern void portbpidump(void);
extern FlashMap flashmap[];
extern int nflash;

void
bootconfinit(void)
{	
	ulong flashbase = bpi->flashbase;
	conf.pagetable = bpi->pagetable;
	//	conf.flashbase = bpi->flashbase;
	if(nflash > 1 && flashmap[1].base == flashbase)
		flashmap[1].base = flashmap[0].base;
	flashmap[0].base = flashbase;
	conf.cpuspeed = bpi->cpuspeed;
}

void
reboot(int x)
{
	bpi->reboot(x);
}

void
exit(int x)
{
	bpi->exit(x);
}

ulong
va2ubva(void *va)
{
	/* unbuffered, but cached */	
	return (ulong)va | 0x10000000;
}

ulong
va2ucva(void *va)
{	
	/* uncached, but buffered */
	return (ulong)va | 0x18000000;
}

ulong
va2ucubva(void *va)
{
	/* uncached, unbufferd */
	return va2pa(va);	/* correct for DRAM and most I/O,
				 * but not correct for static banks 
				 */
}

void
printbootinfo(void)
{
	portbpidump();
	print("flashbase=%ux cpuspeed=%d\n",
			bpi->flashbase,
			bpi->cpuspeed);
}

