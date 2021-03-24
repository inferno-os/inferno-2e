#include	"mem.h"
#include	"mmap.h"

/* Physical Address Adjust for text addresses: */
#define PAA	(PhysStatic0Base-Static0Base)
/* #define PAA	(PhysStatic1Base-0x0a000000) */

#define PMBASE	0x90020000
#define PPCR			0x14
#define MDCNFG	0xA0000000


TEXT reset(SB), $-4
	BL	startup(SB)
	BL	moninfo				/* dead code removal hack */
	BL	stufftosave			/* dead code removal hack */
	WORD	$0				/* pudding */

/*
 * NOTE: moninfo MUST be at offset 0x10 from the start of the flash.
 */
moninfo:
	WORD	$0xc001babe		/* 0x10	partition magic */
	WORD	$DefPtabOfs		/* 0x14	partition offset */
	WORD	$DefPtabSize		/* 0x18	partition size */
	WORD	$0			/* 0x1C	RESERVED */
	WORD	$DefMonOfs		/* 0x20	monitor offset  */
	WORD	$DefMonSize		/* 0x24	monitor size */
	WORD	$DefAutobootOfs		/* 0x28	autoboot offset */
	WORD	$DefBootOfs		/* 0x2C	boot offset */

stufftosave:
	BL	conflist			/* dead code removal hack */
	BL	dramconflist(SB)		/* dead code removal hack */

conflist:
	WORD	$monmisc(SB)
	WORD	$reset(SB)
	WORD	$0xfeedb0ba
TEXT dramconflist(SB), $-4
	WORD	$dramconf0(SB)
	WORD	$dramconf1(SB)
	WORD	$dramconf2(SB)
	WORD	$dramconf3(SB)
	WORD	$dramconf4(SB)
	WORD	$dramconf5(SB)
	WORD	$dramconf6(SB)
	WORD	$dramconf7(SB)
	WORD	$0
	WORD	$mmapconf(SB)
	WORD	$0

/*
TEXT serinit(SB), $-4
	MOVW	$0x90000000,R1
	ORR	$0x00040000,R1
	MOVW	$0, R0
	MOVW	R0, 4(R1)
	MOVW	$0xffffffff,R0
	MOVW	R0, 12(R1)

	MOVW	$0x80000000,R1
	ORR	$0x00050000,R1
	MOVW	$0, R0
	MOVW	R0, 12(R1)
	MOVW	$0xff, R0
	MOVW	R0, 32(R1)
	MOVW	$0x08, R0
	MOVW	R0, 0(R1)
	MOVW	$0, R0
	MOVW	R0, 4(R1)
	MOVW	$0x5, R0
	MOVW	R0, 8(R1)
	MOVW	$3, R0
	MOVW	R0, 12(R1)
	RET

TEXT serputc(SB), $-4
serwait:
	MOVW	$0x80000000,R1
	ORR	$0x00050000,R1
	MOVW	32(R1),R1
	TST	$4,R1
	BEQ	serwait
	MOVW	$0x80000000,R1
	ORR	$0x00050000,R1
	MOVW	R0,20(R1) 
	RET

TEXT serbin(SB), $-4
	MOVW	R14, R4
	MOVW	R0, R3
	MOVW	$32, R2
serbinloop:
	SUB	$1, R2
	MOVW	R3 >> R2, R0
	AND	$1, R0
	ADD	$48, R0
	BL	serputc(SB)
	CMP	$0, R2
	BNE	serbinloop
	MOVW	$10, R0
	BL	serputc(SB)
	MOVW	R4, R14	
	RET
*/

TEXT startup(SB), $-4
/*
	BL	serinit(SB)
MOVW $65, R0; BL serputc(SB)
MOVW $66, R0; BL serputc(SB)
*/

	MOVW	$DBGALTBOOT_MAGIC_OFS, R2
	MOVW	(R2), R0		/* look for debug override */
	MOVW	$DBGALTBOOT_MAGIC, R1
	CMP	R0, R1
	BNE	nooverride
	CMP	$0x1000, PC		/* if we're already a copy */
	BGE	nooverride		/* skip this */
	MOVW	(DBGALTBOOT_ADDR_OFS-DBGALTBOOT_MAGIC_OFS)(R2), PC
nooverride:

	/* disable interrupts, set svc32 mode */
	MOVW	$(PsrDirq|PsrDfiq|PsrMsvc), R14
	MOVW	R14, CPSR

	/* Set CPU Speed */
	MOVW	$PMBASE, R0			/* R0 = & PLL ctrl reg */
	MOVW	$monmisc+PAA(SB), R1
	MOVW	20(R1), R1			/* R1 = cpu speed */
	MOVW	R1, PPCR(R0)			/* PPCR = cpu speed */

	/* flush and enable icache to speed up memory testing */
	MOVW	$CpCIcache, R0			/* R0 = icache ctl reg */
	MCR	CpMMU, 0, R0, C(7), C(7), 0	/* flush i&d */
	MCR	CpMMU, 0, R0, C(1), C(0), 0	/* enable icache */

	MCR	CpMMU, 0, R0, C(15), C(1), 2	/* enable clock switching */

	MOVW	$dramconflist+PAA(SB), R13
	MOVW	$0, R11			/* max DRAM size found */
	MOVW	$0, R12			/* offset of corresponding memcfg */
dramtestloop:
	MOVW.W	4(R13), R9		/* get next conf ptr */
	CMP	$0, R9
	BEQ	dramtestdone
	ADD	$PAA, R9
	BL	setupdram(SB)
	CMP	$0, R1
	BEQ	dramtestloop

	MOVW	$0,R10			/* no bitmask */
	MOVW	$0, R5			/* quick test */
	MOVW	$SCRATCH1, R0
	MOVW	R11, (R0)		/* save R11 */
	BL	mmapinit(SB)
	/* now, r6 = page table base, and r9 = total RAM size */
	MOVW	$SCRATCH1, R0
	MOVW	(R0), R11		/* restore R11 */
	CMP	R11, R9			/* is it the largest yet? */
	BLE	dramtestloop		/* if not, just loop */
	MOVW	R9, R11			/* update max size found */
	MOVW	(R13), R12		/* update corresponding memcfg table */
	B	dramtestloop
dramtestdone:
	MOVW	R12, R9
	ADD	$PAA, R9
	BL	setupdram(SB)
	B	setupdone

TEXT setupdram(SB), $-4
	MOVW	$MDCNFG, R0
	MOVM.IA	(R9), [R1-R7]		/* get params */
	MOVW	R11, R7
	CMP	$0, R1
	BEQ	dramsetupdone
	MOVW	0x10(R0), R8		/* get reset rom setup */
	AND	$4, R8			/* get 16/32 bit setting */
	ORR	R8, R5			/* merge into params */
	MOVM.IA	[R1-R7], (R0)		/* set the params */
	MOVW	$0x00000200, R0		/* give DRAM time to come up */
dramspin:
	SUB.S	$1, R0
	BNE	dramspin
dramsetupdone:
	RET

setupdone:
	MOVW	$RAMTESTED_MAGIC, R0
	MOVW	$RAMTESTED_MAGIC_OFS, R10
	MOVW	(R10), R2
	ADD	$(RAMTESTED_ADDR_OFS-RAMTESTED_MAGIC_OFS), R10 
	MOVW	(R10), R10
	CMP	R0, R2

	MOVW.NE	$1, R5			/* full test if no match */
	MOVW.NE	$RAMBITMASK_OFS, R10	/* set bitmask offset if no match */
	BL	mmapinit(SB)
	/* now, r6 = page table base, r9 = total RAM size, */
	/* and r10 = address of valid DRAM bitmask */
	/*     r11 = bitmask size (# of 1MB banks that were found, */
	/*           whether or not they passed the full test) */

	MOVW	$RAMTESTED_ADDR_OFS, R0
	MOVW	R10, (R0)
	SUB	$(RAMTESTED_ADDR_OFS-RAMTESTED_MAGIC_OFS), R0
	MOVW	$RAMTESTED_MAGIC, R10
	MOVW	R10, (R0)
	MOVW	$RAMTESTED_SIZE_OFS, R10
	MOVW	R11, (R10)

	CMP	$0, R9
	BNE	gotmem
	/* if we found no memory... */
	MOVW	$0, R5			/* quick test */
	BL	mmapinit(SB)
	MOVW	$(0x100000-0xdead0), R1	/* error flag */
	SUB	R1, R9
gotmem:
	MOVW	$mmapconf+PAA(SB), R8
	BL	mmaptable(SB)

	MOVW	R6, R1				/* get page table base */
	MCR	CpMMU, 0, R1, C(2), C(2)	/* set TTB = pg tbl base */
	MOVW	$1, R1
	MCR	CpMMU, 0, R1, C(3), C(3)	/* set domain 0 to CLIENT */

	/* disable and flush all caches and TLB's before enabling MMU */
	MOVW	$0, R0				/* disable everything */
	MCR	CpMMU, 0, R0, C(1), C(0), 0
	MCR	CpMMU, 0, R0, C(7), C(7), 0	/* Flush I&D Caches */
	MCR	CpMMU, 0, R0, C(7), C(10), 4	/* drainWBuffer */
	MCR	CpMMU, 0, R0, C(8), C(7), 0	/* Flush I&D TLB */
	MCR	CpMMU, 0, R0, C(9), C(0), 0	/* Flush Read Buffer */

	/* enable mmu & caches */
	MOVW	$aftermmu+0(SB), R1	/* addr to run at after mmu enabled */
	MOVW	$(CpCmmu|CpCIcache), R0	/* enable MMU & icache (not wbuf and Dcache) */
	MCR	CpMMU, 0, R0, C(1), C(0)	/* enable the MMU */
	MOVW	R1, PC				/* start running in remapped area */

TEXT	_swi(SB), $-4
	MOVW.W	R14, -4(R13)
	MOVW	-4(R14), R0
	AND	$0x00ffffff, R0
	BL	swi(SB)
	MOVW.P	4(R13), R14
	RET

TEXT	aftermmu(SB), $-4

	/* install SWI handler */
	MOVW	$0, R1
	MOVW	$0xe51ff004, R0
	MOVW	R0, 8(R1)
	MOVW	$_swi(SB), R0
	MOVW	R0, 12(R1)

	/* setup static base and stack for C */
	MOVW	$setR12(SB), R12		/* static base */
	SUB	$8, R9, R13			/* SP = topofmem - 4 */
	SUB	$1024, R9, R0			/* 1k for stack */

	B	main(SB)			/* whoo! */
	/* should never get here */

TEXT	segflush(SB), $-4
	MOVW	$0xE0000000, R0
	MOVW	$8192, R1
	ADD	R0, R1
wbflush:
	MOVW	(R0), R2
	ADD	$32, R0
	CMP	R1,R0
	BNE	wbflush

	MCR	CpMMU, 0, R0, C(7), C(6), 0		/* flushDcache */
	MCR	CpMMU, 0, R0, C(CpCacheCtl), C(10), 4	/* drainWBuffer */
	MCR	CpMMU, 0, R0, C(CpCacheCtl), C(5), 0	/* flushIcache */
	MCR	CpMMU, 0, R0, C(9), C(0), 0		/* Flush Read Buffer */
	MOVW	R0, R0
	MOVW	R0, R0
	MOVW	R0, R0
	MOVW	R0, R0
	RET
