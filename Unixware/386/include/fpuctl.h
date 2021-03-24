/*
 * Unixware 386 fpu support
 * Mimic Plan9 floating point support
 */

static ulong Unixware_Asm_Var = 0;

static void
setfcr(ulong fcr)
{
	Unixware_Asm_Var = fcr;

	asm("xorb	$0x3f, %al");
	asm("pushw	%ax");
	asm("fwait");
	asm("fldcw	(%esp)");
	asm("popw	%ax");
}

static ulong
getfcr(void)
{
	ulong fcr;

	Unixware_Asm_Var = 0;

	asm("movl   $0, %eax"); /* ulong fcr = 0 */
	asm("pushl	%eax");
	asm("fwait");
	asm("fstcw (%esp)");
	asm("popl   %eax");
	asm("xorb   $0x3f, %al");

	fcr = Unixware_Asm_Var + 1; 

	asm("movl   %eax, -4(%ebp)"); 
	asm("movl   %eax, %ecx"); 

	return fcr;
}

static ulong
getfsr(void)
{
	ulong fcr;

	Unixware_Asm_Var = -1;
	
	asm("movl $-1,%eax"); /* ulong fsr = -1 */
	asm("fwait");
	asm("fstsw (%eax)");
	asm("movl  (%eax), %eax");
	asm("andl  $0xffff, %eax");

	fcr = Unixware_Asm_Var + 1;

	asm("movl   %eax, -4(%ebp)"); 
	asm("movl   %eax, %ecx"); 

	return fcr;
}

static void
setfsr(ulong fsr)
{
	asm("fclex");
}

/* FCR */
#define	FPINEX	(1<<5)
#define	FPUNFL	((1<<4)|(1<<1))
#define	FPOVFL	(1<<3)
#define	FPZDIV	(1<<2)
#define	FPINVAL	(1<<0)
#define	FPRNR	(0<<10)
#define	FPRZ	(3<<10)
#define	FPRPINF	(2<<10)
#define	FPRNINF	(1<<10)
#define	FPRMASK	(3<<10)
#define	FPPEXT	(3<<8)
#define	FPPSGL	(0<<8)
#define	FPPDBL	(2<<8)
#define	FPPMASK	(3<<8)
/* FSR */
#define	FPAINEX	FPINEX
#define	FPAOVFL	FPOVFL
#define	FPAUNFL	FPUNFL
#define	FPAZDIV	FPZDIV
#define	FPAINVAL	FPINVAL
