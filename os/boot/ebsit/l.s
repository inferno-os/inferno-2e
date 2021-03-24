/*
 *  StrongARM 1100 Architecture Specific Assembly
 *	
 */

#include "mem.h"

/* These constants may need to change if the Bpi structure is altered: */
#define Bpi__himem	0x28
#define Bpi__exit	0x80

/*
 * Entered here from the boot loader with
 *	supervisor mode, interrupts disabled;
 *	MMU, IDC and WB enabled.
 */
TEXT _startup(SB), $-4
	MOVW	$setR12(SB), R12		/* static base (SB) */
	MOVW	R0, bpi(SB)
	/* MOVW	$0x13131313, R13	*/	
	/* MOVW	$0x14141414, R14	*/
	/* MOVW	$(PsrDirq|PsrDfiq|PsrMsvc), R1 */
	/* MOVW	R1, CPSR		*/
_main:
	MOVW	bpi(SB), R0
	MOVW	Bpi__himem(R0), R13		/* stack = bpi->himem */
	MOVW	savehimem(SB), R1
	SUB	R1, R13				/* reserve some space */
	SUB	$4, R13				/* link */
	BL	main(SB)
	MOVW	bpi(SB), R1
	MOVW	Bpi__exit(R1), PC		/* exit(R0) */

/* extra code to make sure stubs get linked in from the library: */
	BL	_div(SB)


DATA bpi+0(SB)/4, $0
GLOBL bpi(SB), $4

