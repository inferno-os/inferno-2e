/*
 * Fake MS-DOS.COM startup. The -H3 loader option makes a header which fools
 * MS-DOS enough to load the programme into memory.
 * If a -T option is used, the address should be of the form 0xXXXXX100.
 * Although MS-DOS will perhaps load a .COM file larger than 64Kb in its
 * entirety, the SP is set just below the 64Kb boundary and any interrupts
 * before things are set up will corrupt whatever code/data is there.
 * Note that the processor is in 'real' mode on entry, so care must be taken
 * as the assembler assumes protected mode, hence the sometimes weird-looking
 * code to assure 16-bit instructions are issued.
 */
#include "x16.h"

/*
 * Start.
 * Check the video mode and force it to mode3 if necessary. Win95 throws
 * a spanner in the works here, it doesn't report the mode correctly when
 * booting; it's best to disable the Win95 boot logo by placing
 *	Logo=0
 * in MSDOS.SYS. Also, if MSDOS.SYS contains the line
 *	BootGUI=0
 * Win95 will come up in MS-DOS mode and will not boot the window system.
 */
TEXT _start(SB), $0
	CLI

	MFSR(rCS, rAX)
	MTSR(rAX, rDS)			/* ensure DS is correct */

	LWI(0x0F00, rAX)		/* get current video mode in AL */
	SYSCALL(0x10)
	ANDI(0x007F, rAX)
	SUBI(0x0003, rAX)		/* is it mode 3? */
	JEQ	_relocate

	LWI(0x0003, rAX)		/* turn on text mode 3 */
	SYSCALL(0x10)

/*
 * Copy the programme image to 0x80100, in 64Kb chunks. The original BX contains the
 * upper 16-bits of the programme size, so this +1 can be used as a loop count for the
 * number of 64Kb segments to copy.
 */
_relocate:
	MFSR(rCS, rAX)
	MTSR(rAX, rDS)			/* starting source segment */

	LWI(0x8000, rDX)
	MTSR(rDX, rES)			/* starting destination segment (0x8000) */

	CLR(rDX)
	MW(rDX, rSI)			/* offset into source segment */
	MW(rDX, rDI)			/* offset into destination segment */

	LWI(1, rBX)			/* number of 64Kb chunks to copy */

	CLD
	LWI(0x1000, rDX)		/* segment increment per loop */
_cscopy:
	LWI(0x8000, rCX)		/* words in 64Kb (0x8000) */

	REP
	MOVSL				/* MOV DS:[(E)SI] -> ES:[(E)DI] */

	DEC(rBX)
	JEQ	_pspcopy		/* all chunks copied? */

	MFSR(rDS, rAX)
	ADD(rDX, rAX)
	MTSR(rAX, rDS)			/* increment source segment */

	MFSR(rES, rAX)
	ADD(rDX, rAX)
	MTSR(rAX, rES)			/* increment destination segment */

	JMP	_cscopy

/*
 * Copy the PSP to 0x80000. SI and DI should be 0. When all is done,
 * jump to the copied image.
 */
_pspcopy:
	LWI(0x8000, rDX)
	MTSR(rDX, rES)			/* destination segment (0x8000) */

	MFSR(rFS, rAX)
	MTSR(rAX, rDS)			/* restore original DS */

	LWI(0x0080, rCX)		/* words in PSP (0x0080) */

	REP				/* copy PSP to 0x80000 */
	MOVSL				/* MOV DS:[(E)SI] -> ES:[(E)DI] */

	FARJUMP16(0x8000, _start801XX(SB))

/*
 * Now running in the copied image at 0x80100+something, so fix up DS to the
 * correct segment (copy from CS).
 * If the processor is already in protected mode, have to get back to real mode
 * before trying any privileged operations (like going into protected mode...).
 * Try to reset with a restart vector.
 */
TEXT _start801XX(SB), $0
	MFSR(rCS, rAX)			/* fix up DS (0x8000) */
	MTSR(rAX, rDS)

_BIOSputs:				/* output a cheery wee message */
	LWI(_hello(SB), rSI)
	CLR(rBX)
_BIOSputsloop:
	LODSB
	ORB(rAL, rAL)
	JEQ	_BIOSputsret

	LBI(0x0E,rAH)
	SYSCALL(0x10)
	JMP	_BIOSputsloop
_BIOSputsret:

	MFCR(rCR0, rAX)
	ANDI(0x0001, rAX)		/* protected mode? */
	JEQ	_real

_reset0x0A:				/* need to reset out of protected mode */
	CLR(rBX)
	MTSR(rBX, rES)			/* BIOS data-area (BDA) segment */

	LWI(0x0467, rBX)		/* reset entry point */
	LWI(_start801XX(SB), rAX)	/* offset within segment */
	POKEW				/* MOVW	AX, ES:[BX] */
	LWI(0x0469, rBX)
	MFSR(rCS, rAX)			/* segment */
	POKEW				/* MOVW	AX, ES:[BX] */

	OUTPORTB(0x70, 0x8F)		/* set shutdown status in CMOS cell 0x0F */
	OUTPORTB(0x71, 0x0A)		/* code 0x0A is reset code */

	FARJUMP16(0xFFFF, 0x0000)	/* reset */

/*
 * Really in real mode, can now go to protected mode and stop all this nonsense.
 * Load a basic GDT to map 4Gb, turn on the protected mode bit in CR0, set all the
 * segments to point to the new GDT then jump to the 32-bit code.
 */
_real:
	LGDT(_gdtptr16(SB))		/* load a basic gdt */

	MFCR(rCR0, rAX)
	ORI(1, rAX)
	MTCR(rAX, rCR0)			/* turn on protected mode */
	DELAY				/* JMP .+2 */

	LWI(SELECTOR(1, SELGDT, 0), rAX)/* set all segments */
	MTSR(rAX, rDS)
	MTSR(rAX, rES)
	MTSR(rAX, rFS)
	MTSR(rAX, rGS)
	MTSR(rAX, rSS)

	FARJUMP32(SELECTOR(2, SELGDT, 0), _start32-KZERO(SB))

/*
 * There's no way with 8[al] to make this into local data, hence
 * the TEXT definitions. Also, it should be in the same segment as
 * the LGDT instruction.
 */
TEXT _gdt16(SB), $0
	LONG $0x0000; LONG $0
	LONG $0xFFFF; LONG $(SEGG|SEGB|(0xF<<16)|SEGP|SEGPL(0)|SEGDATA|SEGW)
	LONG $0xFFFF; LONG $(SEGG|SEGD|(0xF<<16)|SEGP|SEGPL(0)|SEGEXEC|SEGR)
TEXT _gdtptr16(SB), $0
	WORD	$(3*8)
	LONG	$_gdt16-KZERO(SB)

TEXT _hello(SB), $0
	BYTE $0x50; BYTE $0x72; BYTE $0x6F; BYTE $0x74;	/* Prot */
	BYTE $0x65; BYTE $0x63; BYTE $0x74; BYTE $0x65;	/* ecte */
	BYTE $0x64; BYTE $0x2D; BYTE $0x6D; BYTE $0x6F;	/* d-mo */
	BYTE $0x64; BYTE $0x65; BYTE $0x20; BYTE $0x62;	/* de b */
	BYTE $0x6F; BYTE $0x6F; BYTE $0x74; BYTE $0x73;	/* oots */
	BYTE $0x74; BYTE $0x72; BYTE $0x61; BYTE $0x70;	/* trap */
	BYTE $0x0D; BYTE $0x0A; BYTE $0x00; BYTE $0x00;	/* \r\n\z\z */
