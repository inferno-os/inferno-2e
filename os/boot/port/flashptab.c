#include <lib9.h>
#include "dat.h"
#include "flash.h"
#include "flashptab.h"

// Note: all of this code assumes that the monitor code resides at offset 0
// in the flash.  Newer demons can specify the start and size of their
// boot monitor (demon) code.  This should be updated to obtain such 
// information and make use of it.


static ulong
flashinfobase(FlashMap *f)
{
	int i;
	for(i=0; i<=0x400; i+=0x400)
		if(*(ulong*)(f->base+i+FLASHPTAB_MAGIC_OFS)==FLASHPTAB_MAGIC)
			return f->base+i;
	return 0;
}

static int
flashptab_getnum(FlashPTab *pt, const char *name, int np)
{
	if((int)name >= -127 && (int)name <= 127)
		return (int)name;
	else if(name[0] == '-' || (name[0] >= '0' && name[0] <= '9'))
		return strtol(name, 0, 0);
	else if(strcmp(name, "partition") == 0)
		return FLASHPTAB_PARTITION_PNUM;
	else if(strcmp(name, "all") == 0)
		return FLASHPTAB_ALL_PNUM;
	else {
		int i;
		for(i=0; i<np; i++)
			if(strcmp(name, pt[i].name) == 0)
				return i;
		return -128;
	} 
}

// call with string name (actual name or partition number as string)
// or with partition number cast to a char*
// returns partition size for valid partition
// npt can be nil and will not be copied in that case

int
flashptab_get(FlashMap *f, const char *name, FlashPTab *npt)
{
	ulong ptsize = 0;
	ulong ptofs = 0;
	int pn;
	int np = 0;
	FlashPTab *pt = 0;
	ulong fib;


	if((fib = flashinfobase(f))) {
		ptsize = *(ulong*)(fib+FLASHPTAB_SIZE_OFS);
		ptofs = *(ulong*)(fib+FLASHPTAB_OFS_OFS);
		np = ptsize / sizeof(FlashPTab);
		pt = (FlashPTab*)(f->base + ptofs);
	}

	pn = flashptab_getnum(pt, name, np);

	if(pn == FLASHPTAB_ALL_PNUM) {
		if(npt) {
			strcpy(npt->name, "all");
			npt->start = 0;
			npt->length = f->totsize;
			npt->perm = 0644; 
			npt->flags = 0;
		}
		return f->totsize;
	}
	if(pn == FLASHPTAB_PARTITION_PNUM) {
		if(npt) {
			strcpy(npt->name, "partition");
			npt->start = ptofs;
			npt->length = ptsize;
			npt->perm = 0644; 
			npt->flags = 0;
		}
		return ptsize;
	} else if(pn >= 0 && pn < np && pt[pn].name[0]) {
		if(npt)
			*npt = pt[pn];
		return pt[pn].length;
	} else
		return -1;
}
	

int
flashptab_set(FlashMap *f, const char *name, FlashPTab *npt)
{
	ulong ptsize, ptofs;
	int np, pn;
	FlashPTab *pt;
	ulong fib;

	if(!(fib = flashinfobase(f)))
		return -1;
	ptsize = *(ulong*)(fib + FLASHPTAB_SIZE_OFS);
	ptofs = *(ulong*)(fib + FLASHPTAB_OFS_OFS);
	np = ptsize / sizeof(FlashPTab);
	pt = (FlashPTab*)(f->base + ptofs);
	pn = flashptab_getnum(pt, name, np);
	if(pn == -128)
		for(pn=0; pn<np; pn++)
			if(!pt[pn].name[0])
				break;
	if(pn >= 0 && pn < np) {
		ulong data[2];
		ulong dofs, ofs;
		uchar *dptr, *buf;
		int r;
		int dsize, secsize;
		if(npt) {
			dptr = (uchar*)npt;
			dsize = sizeof *npt;
			dofs = ptofs + pn*sizeof(FlashPTab);
		} else {
			data[0] = pt[pn].start;
			data[1] = pt[pn].start;
			dptr = (uchar*)data;
			dofs = fib-f->base+FLASH_AUTOBOOT_OFS_OFS;
			dsize = sizeof(ulong) + (data[0] ? sizeof(ulong) : 0);
		}
		secsize = flash_sectorsize(f, dofs);
		ofs = flash_sectorbase(f, dofs);
		buf = malloc(secsize);
		memmove(buf, (uchar*)f->base+ofs, secsize);
		memmove(buf+dofs-ofs, dptr, dsize);
		r = flash_write_sector(f, ofs, (ulong*)buf);
		free(buf);
		return r ? 0 : -1;
	}
	return -1;
}

ulong
flashptab_getboot(FlashMap *f)
{
	return *(ulong*)(flashinfobase(f) + FLASH_AUTOBOOT_OFS_OFS);
}

int
flashptab_setboot(FlashMap *f, const char *name)
{
	return flashptab_set(f, name, 0);
}

