/*
 *  StrongARM 110 Architecture Specific Assembly
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

TEXT mmuenable(SB), $-4
	SYSMODE 
	MOVW	$0, R1
	MCR	CpMMU, 0, R1, C(8), C(7) 
	MCR	CpMMU, 0, R1, C(7), C(7) 
        MCR     CpMMU, 0, R0, C(CpControl), C(0)
        MOVW    R0, R0
        MOVW    R0, R0
	OLDMODE
        RET

TEXT flushIDC(SB), $-4
	SYSMODE
        MOVW    $0x4000, R0
        MOVW    $32768, R1
        ADD     R0, R1
wbflush:
        MOVW    (R0), R2
        ADD     $32, R0
        CMP     R1,R0
        BNE     wbflush
        MCR     CpMMU, 0, R0, C(7), C(10), 4
        MOVW    R0,R0
        MOVW    R0,R0
        MOVW    R0,R0
        MOVW    R0,R0
        MCR     CpMMU, 0, R0, C(7), C(7), 0
        MOVW    R0,R0
        MOVW    R0,R0
        MOVW    R0,R0
        MOVW    R0,R0
	OLDMODE
        RET

TEXT disableCache(SB), $0
	SYSMODE
        MOVW    $0x1071, R0
        MCR     CpMMU, 0, R0, C(1), C(0), 0
        MOVW    R0, R0
        MOVW    R0, R0
        MOVW    R0, R0
        MOVW    R0, R0
	OLDMODE
        RET

