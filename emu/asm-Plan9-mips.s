#define	NOOP		WORD	$0x27
#define COP3		WORD	$(023<<26)

/*
 *	magnum user level tas, emulated in system
 */
	TEXT	m3kcanlock(SB),$0
	MOVW	R1, R21
btas:
	MOVW	R21, R1
	MOVB	R0, 1(R1)
	NOOP
	COP3
	BLTZ	R1, btas
	BEQ	R1, yes
	MOVW	$0, R1
	RET
yes:
	MOVW	$1, R1
	RET

	TEXT	lockinit(SB),$0
	MOVW	R1, 0(FP)
	MOVW	FCR0, R7
	MOVW	R7, R8
	SUB	$0x330, R7	/* Magnum */
	BEQ		R7, magnum
	MOVW	R8, R7
	SUB	$0x340, R7
	BEQ		R7, magnum	/* Magnum II */
	MOVW	R8, R7
	SUB	$0x320, R7
	BEQ		R7, power	/* SGI Power Series */
	MOVW	$0, (R0)	/* UNKNOWN */
	RET

magnum:
	MOVW	$m3kcanlock(SB), R2
	MOVW	R2, (R1)
	RET

power:
	JAL	powerlockinit(SB)
	BNE	R1, ispower
	MOVW	0(FP), R1
	BEQ	R0, magnum

ispower:
	MOVW	$powercanlock(SB), R2
	MOVW	0(FP), R1
	MOVW	R2, (R1)
	RET

	TEXT	tramp(SB), 1, $0
	ADDU	$-8, R1, R3		/* new stack */
	MOVW	4(FP), R2		/* func to exec */
	MOVW	8(FP), R1		/* arg to reg */
	MOVW	R3, R29			/* new stack */
	JAL	(R2)
	MOVW	R0, R1
	JMP	_exits(SB)		/* Leaks the stack in R29 */

	TEXT	getcallerpc(SB), 1, $0
	MOVW	0(SP), R1
	RET

	TEXT	vstack(SB), 1, $0	/* Passes &targ through R1 */
	MOVW	ustack(SB), R29
	JMP	exectramp(SB)
	RET

	TEXT	FPsave(SB), 1, $0
	MOVW	FCR31, R2
	MOVW	R2, 0(R1)
	RET

	TEXT	FPrestore(SB), 1, $0
	MOVW	0(R1), R2
	MOVW	R2, FCR31
	RET
