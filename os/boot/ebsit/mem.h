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
 * More accurate (???) time (need better system than FIQ overflow counter...)
 */
#define TIMER_HZ	80
#define MS2TMR(t)	(ulong)(((uvlong)(t)*TIMER_HZ+500)/1000)
#define US2TMR(t)	(ulong)(((uvlong)(t)*TIMER_HZ+500000)/1000000)

/*
 *  Address spaces
 *
*/

#define KZERO		0x0


#define ActiveDebugger	((ulong*)0xa90)


#include "armv4.h"
