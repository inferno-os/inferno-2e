/*
 *  StrongARM 1100 Architecture Specific Assembly
 *	Adapted from the sa110 ebsit code.
 *	
 */


#include "mem.h"


/*
 * Entered here from the bootp loader with
 *	supervisor mode, interrupts disabled;
 *	MMU, IDC and WB enabled.
 */
TEXT _startup(SB), $-4
	MOVW	$0x13131313, R13
	MOVW	$0x14141414, R14
/*	MOVW	$(PsrDirq|PsrMsvc), R1 
	MOVW	R1, CPSR */

	MOVW	$setR12(SB), R12		/* static base (SB) */
/*	CMP	$0, R0 */
/*	BNE	_usebpi */
_nobpi:
/*	MOVW	$0x800000, R13			/* if no BootParam, set SP */
	MOVW	$0x800000, R13			/* if no BootParam, set SP */
	SUB	$256, R13			/* just to be safe? */
	SUB	$4, R13				/* link? */
	BL	demonbpi_init(SB)
	B	_main
_usebpi:
	MOVW	R0, bpi(SB)
	MOVW	0x28(R0), R13			/* stack = bpi->himem */
	SUB	$256, R13			/* just to be safe? */
	SUB	$4, R13				/* link */
_main:
	BL	main(SB)
	MOVW	bpi(SB), R0
	MOVW	0x80(R0), PC			/* exit */


DATA bpi+0(SB)/4, $0xb00
GLOBL bpi(SB), $4


/* Demon/BootParam interface (for old DEMONs without BootParam)
 */


TEXT demon_writec(SB),$0
	WORD	$0xef000000
	RET

TEXT demon_write0(SB),$0
	WORD	$0xef000002
	RET

TEXT demon_readc(SB),$0
	WORD	$0xef000004
	RET

TEXT demon_exit(SB),$0
	WORD	$0xef000011
	RET

TEXT demon_exec(SB),$0
	MOVW	R0, R1
	MOVW	bpi(SB), R0
	WORD	$0xef000016
	B	(R1)
	RET

TEXT demon_read(SB),$0
	MOVW	adr+4(FP),R1
	MOVW	len+8(FP),R2
	WORD	$0xef00006a
	RET

TEXT demon_write(SB),$0
	MOVW	adr+4(FP),R1
	MOVW	len+8(FP),R2
	WORD	$0xef000069
	RET

TEXT demon_close(SB),$0
	WORD	$0xef000068
	RET

TEXT demon_open(SB),$0
	MOVW	mode+4(FP),R1
	WORD	$0xef000066
	RET

TEXT demon_flen(SB),$0
	WORD	$0xef00006c
	RET

TEXT demon_istty(SB),$0
	WORD	$0xef00006e
	RET

TEXT demon_seek(SB),$0
	MOVW	ofs+4(FP),R1
	WORD	$0xef00006b
	RET


TEXT mmuenable(SB), $-4
        MOVW    CPSR, R8
        WORD    $0xef000016             /* go to supervisor mode */
        MOVW    $0, R1
        MCR     CpMMU, 0, R1, C(8), C(7)
        MCR     CpMMU, 0, R1, C(7), C(7)
        MCR     CpMMU, 0, R0, C(CpControl), C(0)
        MOVW    R0, R0
        MOVW    R0, R0
        MOVW    R8, CPSR                /* restore previous mode */
        RET
 
TEXT checkcpsr(SB),$-4
        MOVW    CPSR, R0
        RET
 
/*
 * Micro Delays
 */
 
TEXT busyloop(SB), $0
        MOVW    R0, R2
        MOVW    4(FP), R1
_busyloop:
        SUB.S   $1, R0
        BGT     _busyloop
        SUB.S   $1, R1
        MOVW    R2, R0
        BGT     _busyloop
        RET

