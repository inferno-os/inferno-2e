/*
 * Memory and machine-specific definitions.  Used in C and assembler.
 */

/*
 * Sizes
 */
#define _K_		1024			/* 2^10 -> Kilo */
#define _M_		1048576			/* 2^20 -> Mega */
#define _G_		1073741824		/* 2^30 -> Giga */
#define _T_		1099511627776UL		/* 2^40 -> Tera */
#define	BI2BY		8			/* bits per byte */
#define BI2WD		32			/* bits per word */
#define	BY2WD		4			/* bytes per word */
#define	BY2V		8			/* bytes per double word */
#define	BY2PG		4096			/* bytes per page */
#define	WD2PG		(BY2PG/BY2WD)		/* words per page */
#define	PGSHIFT		12			/* log(BY2PG) */
#define ROUND(s, sz)	(((s)+(sz-1))&~(sz-1))
#define PGROUND(s)	ROUND(s, BY2PG)
#define BIT(n)		(1<<n)
#define BITS(a,b)	((1<<(b+1))-(1<<a))

#define	MAXMACH		1			/* max # cpus system can run */

/*
 * Time
 */
#define	HZ		(100)			/* clock frequency */
#define	MS2HZ		(1000/HZ)		/* millisec per clock tick */
#define	TK2SEC(t)	((t)/HZ)		/* ticks to seconds */
#define	TK2MS(t)	((t)*MS2HZ)		/* ticks to milliseconds */
#define	MS2TK(t)	((t)/MS2HZ)		/* milliseconds to ticks */

/*
 * More accurate time
 */
#define TIMER_HZ	3686400
#define MS2TMR(t)	((ulong)(((uvlong)(t)*TIMER_HZ)/1000))
#define US2TMR(t)	((ulong)(((uvlong)(t)*TIMER_HZ)/1000000))

/*
 *  Address spaces
 *
 */

#define KZERO		0x0
#define MACHADDR	((ulong)&Mach0)
/* #define MACHADDR	(KZERO+0x00002000)  /* should come from BootParam, */
					/* or be automatically allocated */
/* #define KTTB		(KZERO+0x00004000)  - comes from BootParam now */
/* #define KTZERO	(KZERO+0x00300000)  - comes from BootParam now */
#define KSTACK		8192			/* Size of kernel stack */


/*
 * various flags in special registers
 */

#define DEBUGBOOT_MAGIC		0xdeb9b007
#define DEBUGBOOT_MAGIC_OFS	0xb0000000

#define DBGALTBOOT_MAGIC	0xdb9ab007
#define DBGALTBOOT_MAGIC_OFS	0xb0000000
#define DBGALTBOOT_ADDR_OFS	0xb0000010

#define RAMTESTED_MAGIC		0xfeb6399	/* ultb 89 */
#define RAMTESTED_MAGIC_OFS	0xb00000a0	/* using DMA 5 */
#define RAMTESTED_ADDR_OFS	0xb00000b0
#define RAMTESTED_SIZE_OFS	0xb00000b4	/* only 13 bits! */

/*
 * various locations that can be used at boottime before DRAM is mapped
 */

#define SCRATCH1	0xb0000080		/* using DMA 4 */

/*
 * some offsets relative to the start of valid RAM
 */

#define RAMBITMASK_OFS		0x30
#define RAMBITMASK_SIZE 	512		/* in bits (1 MB each) */
#define SCRATCH2		0xc0000070
/* area from 0x68 to 0x77 used as temp space during memory test */
/* if the first physical 1MB of DRAM fails, then this area 
 * will not be valid for keeping temporary copies of DRAM data while doing
 * the "nondestructive" fast memory tests.  In such a case, parts
 * of memory will get damaged after a reset
 */
#define GLOBAL_OFS		0x80		/* global vars for monitor */
#define PAGETABLE_OFS		0x4000		/* for MMU */

#include "armv4.h"
