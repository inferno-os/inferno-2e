#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "bootparam.h"

/* note: returns 0 for invalid addresses */

ulong
va2pa(void *v)
{
	int idx;
	ulong pte, *ttb;

	idx = MmuL1x((ulong)v);
	ttb = (ulong*)conf.pagetable;
	pte = ttb[idx];
	if((pte & MmuL1type) != MmuL1section)
		return 0;
	return (pte & 0xfff00000)|((ulong)v & 0x000fffff);
}

enum {
	MmuSectionSh = 20,
	MmuSmallPageSh = 12,
	SectionPages = MmuSection/MmuSmallPage,
	PtAlign = (1<<10)
};

#define ALT_BOOTP	0xF1000000

static void
relocv(ulong lomem, void **a)
{
	if ((*a > (void *)0x40) && (*a < (void*)lomem))
		*a = (void *)((ulong)*a + ALT_BOOTP);
}

static void
reloclist(ulong lomem, char **l)
{
	while (*l)
		relocv(lomem, l++);
}

/*
 * This will make an interrupt vector container at ALT_IVEC (ffff0000)
 *  and move bootparam from 0-7fff to ALT_BOOTP.
 * This leaves us a space that is inaccessible in the range
 *   ffff1000 to 00007fff.  So offsets from '0' pointers and limbo
 *   references via 'H' are trapped by hardware.
 * The 32K originally at 0 is already filled with pagetables and
 *  bootparams, so is not wasted.
 */
void
remaplomem(void)
{
	int ii, ero, ap;
	ulong *ptable, *stable, mem0, lomem;

	/*
	 * L1 table, typically sections, not page tables.
	 */
	stable = (ulong*)conf.pagetable;
	mem0 = va2pa((void *)0);	/* reuse pages currently at 0 */

	/*
	 * Map ALT_BOOTP to copy of first 1Meg
	 */
	stable[MmuL1x(ALT_BOOTP)] = stable[MmuL1x(0)];

	/*
	 * Relocate the addresses in bootparam
	 */
	if (bootparam == nil) {
		lomem = 0x8000;
		relocv(lomem, (void**)&conf.pagetable);
	}
	else {
		lomem = (ulong)bootparam->lomem;
		reloclist(lomem, bootparam->argv);
		relocv(lomem, &bootparam->argv);
		reloclist(lomem, bootparam->envp);
		relocv(lomem, &bootparam->envp);
		relocv(lomem, &bootparam->bootname);
		relocv(lomem, (void**)&conf.pagetable);
		relocv(lomem, &bootparam);
	}
	flushTLB();		/* invalidate previous mapping */

	/*
	 * Build a page table for ALT_IVEC, all invalid (0) except for
	 *  ivec page, mapped read-only.
	 */
	ptable = xspanalloc(sizeof *ptable * SectionPages, PtAlign, 0);
	ptable[MmuL2x(ALT_IVEC)] = mem0 | MmuL2AP(MmuAPsro) | MmuWB | MmuIDC | MmuL2small;
	stable[MmuL1x(ALT_IVEC)] = va2pa(ptable) | MmuL1page;

	/*
	 * Build a new page table for the 1 Meg section at '0'.
	 * Leave the 1st 32K invalid (0), then duplicate the map
	 *  from 32K -> 1Meg, making pages below etext read-only.
	 */
	ptable = xspanalloc(sizeof *ptable * SectionPages, PtAlign, 0);
	ero = (ulong)etext >> 12;
	
	for (ii = MmuL2x(lomem); ii < SectionPages; ii++) {
		if (ii < ero && !conf.textwrite)
			ap = MmuL2AP(MmuAPsro);
		else
			ap = MmuL2AP(MmuAPsrw);
		ptable[ii] = (mem0 + ii*MmuSmallPage) | ap | MmuWB | MmuIDC | MmuL2small;
	}
	stable[MmuL1x(0)] = va2pa(ptable) | MmuL1page;
	flushTLB();		/* invalidate previous mapping */
}
