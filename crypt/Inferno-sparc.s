#define UMUL(rs1,rs2,rd) \
	WORD    $((1<<31)|(rd<<25)|(0xA<<19)|(rs2<<14)|(rs1))

/* 	R1	n1
 * 	4(FP)	n2
 * 	8(FP)	*high 32 bits
 */

TEXT	umult(SB), $-4
	MOVW	4(FP), R5
/*	UMUL	R7, R5, R4 */
	UMUL	(7, 5, 4)
	MOVW	8(FP), R3
	MOVW	Y, R5
	MOVW	R5, (R3)
	MOVW	R4, R7
	RETURN
