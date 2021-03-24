/*
 *  StrongARM 1100 Architecture Specific Assembly
 *	for mmu registers and caches
 *	
 */

#define SYSMODE		\
	MOVW	CPSR, R9; \
	TST	$2, R9; \
	WORD	$0x0f000016	/* if user mode, go to supervisor mode */ 

#define OLDMODE \
	MOVW	R9, CPSR

#include "mem.h"

/*
 * MMU register Operations
 */

TEXT mmuctlregr(SB), $-4
	SYSMODE
	MRC		CpMMU, 0, R0, C(CpControl), C(0)
	OLDMODE
	RET
 
TEXT mmuctlregw(SB), $-4
	SYSMODE
	MCR		CpMMU, 0, R0, C(CpControl), C(0)
	MOVW	R0, R0
	MOVW	R0, R0
	OLDMODE
	RET


/*
 * Cache operations
 */

TEXT writeBackBDC(SB), $-4
	SYSMODE
 	MOVW    $0xE4000000, R0
 	MOVW    $0x200, R1
	ADD     R0, R1
wbbflush:
 	MOVW    (R0), R2
	ADD     $32, R0
	CMP     R1,R0
	BNE     wbbflush
	MCR     CpMMU, 0, R0, C(CpCacheCtl), C(10), 4	/* Drain a write buffer 
*/
 	MOVW    R0,R0							/* Four No-ops */
	MOVW    R0,R0
	MOVW    R0,R0
	MOVW    R0,R0
	OLDMODE
	RET


TEXT _flushIcache(SB), $-4
	SYSMODE
	MCR	CpMMU, 0, R0, C(CpCacheCtl), C(5), 0
	MOVW	R0,R0
	MOVW	R0,R0
	MOVW	R0,R0
	MOVW	R0,R0
	MOVW	$(PsrDirq|PsrMusr), R1	/* return to user mode */
	OLDMODE
	RET

TEXT _drainWBuffer(SB), $-4
	SYSMODE
	MCR	CpMMU, 0, R0, C(CpCacheCtl), C(10), 4
	MOVW	$(PsrDirq|PsrMusr), R1	/* return to user mode */
	OLDMODE
	RET

TEXT _writeBackDC(SB), $-4
	MOVW	$0xe0000000, R0
	MOVW	$8192, R1
	ADD	R0, R1
wbflush:
	MOVW	(R0), R2
	ADD	$32, R0
	CMP	R1, R0
	BNE	wbflush
	RET


TEXT flushIDC(SB), $0
	BL	_writeBackDC(SB)
	BL	_drainWBuffer(SB)
	BL	_flushIcache(SB)
	RET

