#include "mem.h"

/*
 * 		Entered from the boot loader with
 *		supervisor mode, interrupts disabled;
 *		MMU, IDC and WB enabled.
 */

TEXT _startup(SB), $-4
	MOVW		$setR12(SB), R12 	/* static base (SB) */
	MOVW		$Mach0(SB), R13
	ADD		$(KSTACK-4), R13	/* leave 4 bytes for link */
	MOVW		R0, bootparam(SB)

	MOVW		$(PsrDirq|PsrDfiq|PsrMsvc), R1	/* Switch to SVC mode */
	MOVW		R1, CPSR

	BL		main(SB)		/* jump to kernel */

dead:
	B	dead

GLOBL 		Mach0(SB), $KSTACK
DATA 		bootparam+0(SB)/4, $0xb00	/* Default BootParam address */
GLOBL 		bootparam(SB), $4

TEXT mmuregr(SB), $-4
	CMP		$CpCPUID, R0
	BNE		_fsrr
	MRC		CpMMU, 0, R0, C(CpCPUID), C(0)
	RET

_fsrr:
	CMP		$CpFSR, R0
	BNE		_farr
	MRC		CpMMU, 0, R0, C(CpFSR), C(0)
	RET

_farr:
	CMP		$CpFAR, R0
	BNE		_domr
	MRC		CpMMU, 0, R0, C(CpFAR), C(0)
	RET

_domr:
	CMP		$CpDAC, R0
	BNE		_ttbr
	MRC		CpMMU, 0, R0, C(CpDAC), C(0)
	RET

_ttbr:
	CMP		$CpTTB, R0
	BNE		_noner
	MRC		CpMMU, 0, R0, C(CpTTB), C(0)
	RET

_noner:
	MOVW		$-1, R0
	RET

TEXT mmuregw(SB), $-4
	MOVW		4(FP), R1
	CMP		$CpFSR, R0
	BNE		_domw
	MCR		CpMMU, 0, R1, C(CpFSR), C(0)
	RET

_domw:
	CMP		$CpDAC, R0
	BNE		_nonew
	MCR		CpMMU, 0, R1, C(CpDAC), C(0)
	RET

_nonew:
	RET

TEXT flushTLB(SB), $-4
	MCR		CpMMU, 0, R0, C(CpTLBops), C(7)
	RET

TEXT rDBAR(SB), $-4
	MRC		CpMMU, 0, R0, C(CpDebug), C(CpDBAR)
	RET

TEXT rDBVR(SB), $-4
	MRC		CpMMU, 0, R0, C(CpDebug), C(CpDBVR)
	RET

TEXT rDBMR(SB), $-4
	MRC		CpMMU, 0, R0, C(CpDebug), C(CpDBMR)
	RET

TEXT rDBCR(SB), $-4
	MRC		CpMMU, 0, R0, C(CpDebug), C(CpDBCR)
	RET

TEXT wDBAR(SB), $-4
	MCR		CpMMU, 0, R0, C(CpDebug), C(CpDBAR)
	RET

TEXT wDBVR(SB), $-4
	MCR		CpMMU, 0, R0, C(CpDebug), C(CpDBVR)
	RET

TEXT wDBMR(SB), $-4
	MCR		CpMMU, 0, R0, C(CpDebug), C(CpDBMR)
	RET

TEXT wDBCR(SB), $-4
	MCR		CpMMU, 0, R0, C(CpDebug), C(CpDBCR)
	RET

TEXT wIBCR(SB), $-4
	MCR		CpMMU, 0, R0, C(CpDebug), C(CpIBCR)
	RET

TEXT setr13(SB), $-4
	MOVW		4(FP), R1

	MOVW		CPSR, R2
	BIC		$PsrMask, R2, R3
	ORR		R0, R3
	MOVW		R3, CPSR

	MOVW		R13, R0
	MOVW		R1, R13

	MOVW		R2, CPSR
	RET

TEXT _vundcall(SB), $-4			
_vund:
	MOVM.DB		[R0-R3], (R13)
	MOVW		$PsrMund, R0
	B		_vswitch

TEXT _vsvccall(SB), $-4				
_vsvc:
	MOVW.W		R14, -4(R13)
	MOVW		CPSR, R14
	MOVW.W		R14, -4(R13)
	BIC		$PsrMask, R14
	ORR		$(PsrDirq|PsrDfiq|PsrMsvc), R14
	MOVW		R14, CPSR
	MOVW		$PsrMsvc, R14
	MOVW.W		R14, -4(R13)
	B		_vsaveu

TEXT _vpabcall(SB), $-4			
_vpab:
	MOVM.DB		[R0-R3], (R13)
	MOVW		$PsrMabt, R0
	B		_vswitch

TEXT _vdabcall(SB), $-4	
_vdab:
	MOVM.DB		[R0-R3], (R13)
	MOVW		$(PsrMabt+1), R0
	B		_vswitch

TEXT _vfiqcall(SB), $-4				/* IRQ */
_vfiq:		/* FIQ */
	MOVM.DB		[R0-R3], (R13)
	MOVW		$PsrMfiq, R0
	B		_vswitch

TEXT _virqcall(SB), $-4				/* IRQ */
_virq:
	MOVM.DB		[R0-R3], (R13)
	MOVW		$PsrMirq, R0

_vswitch:					/* switch to svc mode */
	MOVW		SPSR, R1
	MOVW		R14, R2
	MOVW		R13, R3

	MOVW		CPSR, R14
	BIC		$PsrMask, R14
	ORR		$(PsrDirq|PsrDfiq|PsrMsvc), R14
	MOVW		R14, CPSR

	MOVM.DB.W 	[R0-R2], (R13)
	MOVM.DB	  	(R3), [R0-R3]

_vsaveu:						/* Save Registers */
	MOVW.W		R14, -4(R13)			/* save link */
	MCR		CpMMU, 0, R0, C(0), C(0), 0	

	SUB		$8, R13
	MOVM.DB.W 	[R0-R12], (R13)

	MOVW		R0, R0				/* gratuitous noop */

	MOVW		$setR12(SB), R12		/* static base (SB) */
	MOVW		R13, R0				/* argument is ureg */
	SUB		$8, R13				/* space for arg+lnk*/
	BL		trap(SB)


_vrfe:							/* Restore Regs */
	MOVW		CPSR, R0			/* splhi on return */
	ORR		$(PsrDirq|PsrDfiq), R0, R1
	MOVW		R1, CPSR
	ADD		$(8+4*15), R13		/* [r0-R14]+argument+link */
	MOVW		(R13), R14			/* restore link */
	MOVW		8(R13), R0
	MOVW		R0, SPSR
	MOVM.DB.S 	(R13), [R0-R14]		/* restore user registers */
	MOVW		R0, R0				/* gratuitous nop */
	ADD		$12, R13		/* skip saved link+type+SPSR*/
	RFE					/* MOVM.IA.S.W (R13), [R15] */
	
TEXT splhi(SB), $-4					
	MOVW		CPSR, R0
	ORR		$(PsrDirq), R0, R1
	MOVW		R1, CPSR
	RET

TEXT spllo(SB), $-4
	MOVW		CPSR, R0
	BIC		$(PsrDirq|PsrDfiq), R0, R1
	MOVW		R1, CPSR
	RET

TEXT splx(SB), $-4
	MOVW		R0, R1
	MOVW		CPSR, R0
	MOVW		R1, CPSR
	RET

TEXT islo(SB), $-4
	MOVW		CPSR, R0
	AND		$(PsrDirq), R0
	EOR		$(PsrDirq), R0
	RET

TEXT splfhi(SB), $-4					
	MOVW		CPSR, R0
	ORR		$(PsrDfiq|PsrDirq), R0, R1
	MOVW		R1, CPSR
	RET

TEXT splflo(SB), $-4
	MOVW		CPSR, R0
	BIC		$(PsrDfiq), R0, R1
	MOVW		R1, CPSR
	RET

TEXT cpsrr(SB), $-4
	MOVW		CPSR, R0
	RET

TEXT spsrr(SB), $-4
	MOVW		SPSR, R0
	RET

TEXT getcallerpc(SB), $-4
	MOVW		0(R13), R0
	RET

TEXT tas(SB), $-4
	MOVW		R0, R1
	MOVW		$0xDEADDEAD, R2
	SWPW		R2, (R1), R0
	RET

TEXT setlabel(SB), $-4
	MOVW		R13, 0(R0)		/* sp */
	MOVW		R14, 4(R0)		/* pc */
	MOVW		$0, R0
	RET

TEXT gotolabel(SB), $-4
	MOVW		0(R0), R13		/* sp */
	MOVW		4(R0), R14		/* pc */
	MOVW		$1, R0
	RET

TEXT mmuctlregr(SB), $-4
	MRC		CpMMU, 0, R0, C(CpControl), C(0)
	RET	

TEXT mmuctlregw(SB), $-4
	MCR		CpMMU, 0, R0, C(CpControl), C(0)
	MOVW		R0, R0
	MOVW		R0, R0
	RET	

TEXT flushIcache(SB), $-4
	MCR	 	CpMMU, 0, R0, C(CpCacheCtl), C(5), 0	
	MOVW		R0,R0							
	MOVW		R0,R0
	MOVW		R0,R0
	MOVW		R0,R0
	RET

TEXT cleanDentry(SB), $-4
	MCR		CpMMU, 0, R0, C(CpCacheCtl), C(10), 1
	RET

TEXT flushDentry(SB), $-4
	MCR		CpMMU, 0, R0, C(CpCacheCtl), C(6), 1
	RET

TEXT drainWBuffer(SB), $-4
	MCR		CpMMU, 0, R0, C(CpCacheCtl), C(10), 4	
	RET

TEXT writeBackDC(SB), $-4
	MOVW		$0xE0000000, R0
	MOVW		$8192, R1
	ADD		R0, R1

wbflush:
	MOVW.P.W	32(R0), R2
	CMP		R1,R0
	BNE		wbflush
	RET

TEXT flushDcache(SB), $-4
	MCR		CpMMU, 0, R0, C(CpCacheCtl), C(6), 0	
	RET

TEXT writeBackBDC(SB), $-4		
	MOVW		$0xE4000000, R0
	MOVW		$0x200, R1
	ADD		R0, R1

wbbflush:
	MOVW.P.W	32(R0), R2
	CMP		R1,R0
	BNE		wbbflush
	MCR		CpMMU, 0, R0, C(CpCacheCtl), C(10), 4	
	MOVW		R0,R0								
	MOVW		R0,R0
	MOVW		R0,R0
	MOVW		R0,R0
	RET

TEXT flushIDC(SB), $-4
/*BUG*/
	BL 		drainWBuffer(SB)
	BL 		writeBackDC(SB)
	BL 		flushDcache(SB) 
	BL 		flushIcache(SB)	
	RET

TEXT _outs(SB), $-4
	MOVW	4(FP),R1
	WORD	$0xe1c010b0	/* STR H R1,[R0+0] */
	RET

TEXT _ins(SB), $-4
	WORD	$0xe1d000b0	/* LDRHU R0,[R0+0] */
	RET

/* for devboot */
TEXT	gotopc(SB), $-4
	MOVW	R0, R1
	MOVW	bootparam(SB), R0
	MOVW	R1, PC
	RET
