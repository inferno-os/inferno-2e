
	TEXT	realcanlock(SB), 1, $0

	TAS	(R7), R7		/* LDSTUB, thank you ken */
	CMP	R7, $0
	BE	gotit
	MOVW	$0, R7
	RETURN
gotit:
	MOVW	$1, R7
	RETURN

	TEXT	lockinit(SB), $0
	MOVW	$realcanlock(SB), R4
	MOVW	R4, (R7)
	RETURN

	TEXT	tramp(SB), 1, $0
	ADD	$-8, R7, R3		/* new stack */
	MOVW	4(FP), R4		/* func to exec */
	MOVW	8(FP), R7		/* arg to reg */
	MOVW	R3, R1			/* new stack */
	JMPL	(R4)
	MOVW	R0, R7
	JMPL	_exits(SB)		/* Leaks the stack in R29 */

	TEXT	vstack(SB), 1, $0	/* Passes &targ through R1 */
	MOVW	ustack(SB), R1
	MOVW	$exectramp(SB), R3
	JMP	(R3)
	RETURN

	TEXT	getcallerpc(SB), $0
	MOVW	0(R1), R7
	RETURN

	TEXT	FPsave(SB), 1, $0
	MOVW	FSR, 0(R7)
	RETURN

	TEXT	FPrestore(SB), 1, $0
	MOVW	0(R7), FSR
	RETURN

	TEXT	__rem64(SB), $0
	MOVW	$0, (R0)
	RETURN

	TEXT	__mul64(SB), $0
	MOVW	$0, (R0)
	RETURN

	TEXT	__div64(SB), $0
	MOVW	$0, (R0)
	RETURN

