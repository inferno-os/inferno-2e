#define	LL(base, rt)	WORD	$((060<<26)|((base)<<21)|((rt)<<16))
#define	SC(base, rt)	WORD	$((070<<26)|((base)<<21)|((rt)<<16))
#define	NOOP		WORD	$0x27

	TEXT	canlock(SB), 1, $0
	MOVW	R1, R2			/* address of key */
loop:
	MOVW	$1, R3
	LL(2, 1)
	NOOP
	SC(2, 3)
	NOOP
	BEQ	R3, loop
	XOR	$1, R1
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
