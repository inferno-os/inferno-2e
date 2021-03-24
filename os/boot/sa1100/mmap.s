#include "mem.h"
#include "mmap.h"

#define SetPTE_invalid(reg) \
	MOVW	$0, reg
#define SetPTE_sec(reg, addr, dom, ucb, acc) \
	ORR	$((acc)<<10), addr, reg; \
	ORR	$(MmuDAC(dom)|(ucb)|MmuL1section), reg

MapRAM1st:
	ADD	$PAGETABLE_OFS, R1, R6		/* page table start */
	ADD	$0x4000, R6, R4		/* page table size */
ClearLoop:		/* invalidate entire page table */
	SetPTE_invalid(R2)
	MOVW.W	R2, -4(R4)
	CMP	R6, R4
	BNE	ClearLoop
	TST	$1, R5
	BEQ	MapRAMnot1st
	CMP	$0, R10
	BEQ	MapRAMnot1st
	ADD	R1, R10
	ADD	$((RAMBITMASK_SIZE/32)<<2), R10, R2
	MOVW	$0, R0
ClearBitmask:
	SUB	$4, R2
	MOVW	R0, (R2)
	CMP	R2, R10
	BNE	ClearBitmask
	B	MapRAMnot1st 

/*
upon entry:
	r5 = 0:quick test, 1:full test
	r10 has address of bitmask, or 0 for none
at end:
	r0-r4, r7, r8 all trashed
	r6 = page table base
	r9 = total DRAM size
	r11 = total # of 1MB memory banks detected (valid or failed)
*/

TEXT	mmapinit(SB), $-4
	MOVW	$0, R6
	MOVW	$PhysDRAMBase, R7
	MOVW	$PhysDRAMSize, R8
	MOVW	$0, R9
	MOVW	$0, R11
	ADD	R8, R7, R1
	MOVW	R7, R3
FillRAMLoop:		/* store a detectable pattern */
	SUB	$MmuSection, R1
	MOVW	R1, (R1)
	CMP	R3, R1
	BHI	FillRAMLoop

	MOVW	R7, R1
MapRAMLoop:		/* map buf/unbuf RAM entries */
	MOVW	(R1), R0
	CMP	R1, R0
	BNE	MapRAMskip
	MVN	R1, R2
	MOVW	R2, (R1)	/* store the inverted address */
	MOVW	(R1), R0	/* check what was stored */
	CMP	R2, R0
	BNE	MapRAMskip
	TST	$1, R5
	BNE	MapRAMtestslow	/* if bit0=1*/
	B	MapRAMtestfast
MapRAMnext:
	ADD	$1, R11
MapRAMskip:
	ADD	$MmuSection, R1, R1
	MOVW	$(PhysDRAMBase+PhysDRAMSize),R3
	CMP	R3, R1
	BNE	MapRAMLoop
	RET

MapRAMOkay:
	CMP	$0, R9		/* is this the first DRAM found? */
	BEQ	MapRAM1st	/* if so, init page table */
MapRAMnot1st:
	TST	$1, R5
	BEQ	MapRAMsetpte
	CMP	$0, R10
	BEQ	MapRAMsetpte
	MOVW	R11>>5, R2	/* get word# for bitmask */
	ADD	R2<<2, R10, R2	/* calc word address for bitmask */
	AND	$0x1f, R11, R0	/* get shift count */
	MOVW	$1, R8
	MOVW	R8 << R0, R8
	MOVW	(R2), R0
	ORR	R8, R0
	MOVW	R0, (R2)	/* set appropriate bit in bitmask */
MapRAMsetpte:
	SetPTE_sec(R2, R1, 0, 16|MmuWB|MmuIDC, MmuAPurw)
	MOVW	R2, (R4)	/* map normal RAM */
	SetPTE_sec(R2, R1, 0, 16|MmuIDC, MmuAPurw)
	MOVW	R2, (4*((UBDRAMBase/MmuSection)&0xfff))(R4)
	SetPTE_sec(R2, R1, 0, 16|MmuWB, MmuAPurw)
	MOVW	R2, (4*((UCDRAMBase/MmuSection)&0xfff))(R4)
	ADD	$4, R4, R4	/* next PTE */
	ADD	$MmuSection, R9, R9	/* inc total RAM size */
	B	MapRAMnext

MapRAMtestfast:
	MOVW	$SCRATCH2, R3		/* area for 4 temp values */
	MOVW	$MmuSection, R8
	SUB	$4, R8, R7		/* bitmask for addr bits within page */
MapRAMtestfastloop:
	MOVW	R8 >> 1, R8
	CMP	$2, R8
	BEQ	MapRAMAlmostOkay
	/* save the spots we're about to clobber */
	MOVW	(R1), R0
	MOVW	R0, 0(R3)
	EOR	R7, R1, R2
	MOVW	(R2), R0
	MOVW	R0, 4(R3)
	EOR	R8, R2, R2
	MOVW	(R2), R0
	MOVW	R0, 8(R3)
	EOR	R7, R2, R2
	MOVW	(R2), R0
	MOVW	R0, 12(R3)
MapRAMskipsave:
	/* store 4 test patterns */
	MOVW	R1, (R1)
	EOR	R7, R1, R2
	MOVW	R2, (R2)
	EOR	R8, R2, R2
	MOVW	R2, (R2)
	EOR	R7, R2, R2
	MOVW	R2, (R2)
	/* try to read 4 test patterns */
	MOVW	(R1), R0
	CMP	R1, R0
	BNE	MapRAMnext
	EOR	R7, R1, R2
	MOVW	(R2), R0
	CMP	R2, R0
	BNE	MapRAMnext
	EOR	R8, R2, R2
	MOVW	(R2), R0
	CMP	R2, R0
	BNE	MapRAMnext
	EOR	R7, R2, R2
	MOVW	(R2), R0
	CMP	R2, R0
	BNE	MapRAMnext
	/* restore the data we nuked */
	MOVW	0(R3), R0
	MOVW	R0, (R1)
	EOR	R7, R1, R2
	MOVW	4(R3), R0
	MOVW	R0, (R2)
	EOR	R8, R2, R2
	MOVW	8(R3), R0
	MOVW	R0, (R2)
	EOR	R7, R2, R2
	MOVW	12(R3), R0
	MOVW	R0, (R2)
	B	MapRAMtestfastloop

MapRAMAlmostOkay:
	CMP	$0, R10
	BEQ	MapRAMOkay
	MOVW	R11>>5, R2	/* get word# for bitmask */
	ADD	R2<<2, R10, R2	/* calc word address for bitmask */
	AND	$0x1f, R11, R0	/* get shift count */
	MOVW	(R2), R2	/* get one word of bitmask */
	MOVW	R2 >> R0, R0	/* extract the bit */
	TST	$1, R0
	BNE	MapRAMOkay
	B	MapRAMnext

MapRAMtestslow:
MapRAMtestslowloop:
	ADD	$MmuSection, R1, R2
	MOVW	$0xffffffff, R8	 /* just use the invert pattern to save time */
MapRAMFill:		/* fill in the whole thing */
	EOR	R8, R2, R7	
	MOVW.W	R7, -4(R2)
	SWPW	R2, (R2), R0
	CMP	R7, R0
	BNE	MapRAMnext
	CMP	R1, R2
	BHI	MapRAMFill

	ADD	$MmuSection, R1, R2

MapRAMVerify:		/* and then verify it */
	MOVW.W	-4(R2), R0 
	CMP	R2, R0
	BNE	MapRAMnext
	MOVW	$0x203a2d7b, R0	/* pattern for uninitialized memory */
	MOVW	R0, 0(R2) 
	CMP	R1, R2
	BHI	MapRAMVerify
	B	MapRAMOkay


/*
upon entry:
	r6 = page table base
	
at end:
	r0-r4, r8 all trashed
	r6 = page table base
*/
TEXT mmaptable(SB), $-4
MMapTableLoop:
	MOVM.IA.W (R8),[R1-R4]	/* vbase, vsize, pbase, flags */
	CMP	$0, R2
	BEQ	MMapTableDone
	TST	$0x80000000, R4
	BNE	MMapTableLoop	/* skip any flagged with this */
	ADD	R3, R2		/* r2 = pbase+vsize */
	MOVW	R1 >> (20-2), R1	/* r1 = (vbase/MmuSection) */
	ADD	R6, R1		/* r1 += ptable base */
MMapTablePageLoop:
	ORR	R4, R3, R0	/* r0 = pbase | flags */
	MOVW.W.P R0, 4(R1)
	ADD	$MmuSection, R3
	CMP	R2, R3
	BLO	MMapTablePageLoop
	B	MMapTableLoop
MMapTableDone:
	RET

