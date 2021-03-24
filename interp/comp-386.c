#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"

#define DOT			((ulong)code)

/*
 *	Cannot use enum's with the same name as the board register names
 *	as defined in utils/libmach/8.c. This is because when the acid
 *	file for the kernel is created it includes the definitions from
 *	this file, causing the register offsets to become the value of the
 *	enums. If you want to use the register names as enums, they must have
 *	the same values as the offsets in utils/libmach/8.c.
 */
enum
{
	RAXenum	= 0,
	RAH	= 4,
	RCXenum	= 1,
	RDXenum	= 2,
	RBXenum	= 3,
	RSPenum	= 4,
	RBPenum	= 5,
	RSIenum	= 6,
	RDIenum	= 7,

	RFP	= RSIenum,
	RMP	= RDIenum,
	RTA	= RDXenum,
	RTMP	= RBXenum,

	Omovzxb	= 0xb6,
	Omovzxw	= 0xb7,
	Osal	= 0xd1,
	Oaddf	= 0xdc,
	Ocall	= 0xe8,
	Ocallrm	= 0xff,
	Ocdq	= 0x99,
	Ocld	= 0xfc,
	Ocmpb	= 0x38,
	Ocmpw	= 0x39,
	Ocmpi	= 0x83,
	Odecrm	= 0xff,
	Oincr	= 0x40,
	Oincrm	= 0xff,
	Ojccl	= 0x83,
	Ojcsl	= 0x82,
	Ojeqb	= 0x74,
	Ojeql	= 0x84,
	Ojgel	= 0x8d,
	Ojgtl	= 0x8f,
	Ojhil	= 0x87,
	Ojlel	= 0x8e,
	Ojlsl	= 0x86,
	Ojltl	= 0x8c,
	Ojol	= 0x80,
	Ojnol	= 0x81,
	Ojbl	= 0x82,
	Ojael	= 0x83,
	Ojal	= 0x87,
	Ojnel	= 0x85,
	Ojbel	= 0x86,
	Ojneb	= 0x75,
	Ojgtb	= 0x7f,
	Ojgeb	= 0x7d,
	Ojleb	= 0x7e,
	Ojltb	= 0x7c,
	Ojmp	= 0xe9,
	Ojmpb	= 0xeb,
	Ojmprm	= 0xff,
	Oldb	= 0x8a,
	Olds	= 0x89,
	Oldw	= 0x8b,
	Olea	= 0x8d,
	Otestib	= 0xf6,
	Oshld	= 0xa5,
	Oshrd	= 0xad,
	Osar	= 0xd3,
	Osarimm = 0xc1,
	Omov	= 0xc7,
	Omovf	= 0xdd,
	Omovimm	= 0xb8,
	Omovsb	= 0xa4,
	Orep	= 0xf3,
	Oret	= 0xc3,
	Oshl	= 0xd3,
	Oshr	= 0xd1,
	Ostb	= 0x88,
	Ostw	= 0x89,
	Osubf	= 0xdc,
	Oxchg	= 0x87,
	OxchgAX	= 0x90,
	Oxor	= 0x31,
	Opopl	= 0x58,
	Opushl	= 0x50,
	Opushrm	= 0xff,
	Oneg	= 0xf7,

	SRCOP	= (1<<0),
	DSTOP	= (1<<1),
	WRTPC	= (1<<2),
	TCHECK	= (1<<3),
	NEWPC	= (1<<4),
	DBRAN	= (1<<5),
	THREOP	= (1<<6),

	ANDAND	= 1,
	OROR	= 2,
	EQAND	= 3,

	MacFRP	= 0,
	MacRET	= 1,
	MacCASE	= 2,
	MacCOLR	= 3,
	MacMCAL	= 4,
	MacFRAM	= 5,
	MacMFRA	= 6,
	MacECLR	= 7,
	NMACRO
};

static	uchar*	code;
static	uchar*	base;
static	ulong*	patch;
static	int	pass;
static	Module*	mod;
static	uchar*	tinit;
static	ulong*	litpool;
static	int	nlit;
static	void	macfrp(void);
static	void	macret(void);
static	void	maccase(void);
static	void	maccolr(void);
static	void	macmcal(void);
static	void	macfram(void);
static	void	macmfra(void);
static	void	maceclr(void);
static	ulong	macro[NMACRO];
	void	(*comvec)(void);
extern	void	das(uchar*, int);

#define T(r)	*((void**)(R.r))

struct
{
	int	idx;
	void	(*gen)(void);
} mactab[] =
{
	MacFRP,		macfrp,		/* decrement and free pointer */
	MacRET,		macret,		/* return instruction */
	MacCASE,	maccase,	/* case instruction */
	MacCOLR,	maccolr,	/* increment and color pointer */
	MacMCAL,	macmcal,	/* mcall bottom half */
	MacFRAM,	macfram,	/* frame instruction */
	MacMFRA,	macmfra,	/* punt mframe because t->initialize==0 */
	MacECLR,	maceclr,	/* exception patcher */
};

static void
rdestroy(void)
{
	destroy(R.s);
}

static void
rmcall(void)
{
	Prog *p;
	Frame *f;
	Type *t;

	if((void*)R.dt == H)
		error(exModule);

	f = (Frame*)R.FP;
	if(f == H)
		error(exModule);

	f->mr = nil;
	((void(*)(Frame*))R.dt)(f);
	R.SP = (uchar*)f;
	R.FP = f->fp;
	t = f->t;
	if(t == nil) {
		unextend(f);
		return;
	}
	if (t->np)
		freeptrs(f, t);
	p = currun();
	if(p->kill != nil)
		error(p->kill);
}

static void
rmfram(void)
{
	Type *t;
	Frame *f;
	uchar *nsp;

	t = (Type*)R.s;
	if(t == H)
		error(exModule);

	nsp = R.SP + t->size;
	if(nsp >= R.TS) {
		R.s = t;
		extend();
		T(d) = R.s;
		return;
	}
	f = (Frame*)R.SP;
	R.SP = nsp;
	f->t = t;
	f->mr = nil;
	if (t->np)
		initmem(t, f);
	T(d) = f;
}

static int
bc(int o)
{
	if(o < 127 && o > -128)
		return 1;
	return 0;
}

static void
urk(void)
{
	error(exCompile);
}

static void
genb(uchar o)
{
	*code++ = o;
}

static void
gen2(uchar o1, uchar o2)
{
	code[0] = o1;
	code[1] = o2;
	code += 2;
}

static void
genw(ulong o)
{
	*(ulong*)code = o;
	code += 4;
}

static void
modrm(int inst, ulong disp, int rm, int r)
{
	*code++ = inst;
	if(disp == 0) {
		*code++ = (0<<6)|(r<<3)|rm;
		return;
	}
	if(bc(disp)) {
		code[0] = (1<<6)|(r<<3)|rm;
		code[1] = disp;
		code += 2;
		return;
	}
	*code++ = (2<<6)|(r<<3)|rm;
	*(ulong*)code = disp;
	code += 4;
}

static void
con(ulong o, int r)
{
	if(o == 0) {
		gen2(Oxor, (3<<6)|(r<<3)|r);
		return;
	}
	genb(Omovimm+r);
	genw(o);
}

static void
opwld(Inst *i, int mi, int r)
{
	int ir, rta;

	switch(UXSRC(i->add)) {
	default:
		print("%D\n", i);
		urk();
	case SRC(AFP):
		modrm(mi, i->s.ind, RFP, r);
		return;
	case SRC(AMP):
		modrm(mi, i->s.ind, RMP, r);
		return;
	case SRC(AIMM):
		con(i->s.imm, r);
		return;
	case SRC(AIND|AFP):
		ir = RFP;
		break;
	case SRC(AIND|AMP):
		ir = RMP;
		break;
	}
	rta = RTA;
	if(mi == Olea)
		rta = r;
	modrm(Oldw, i->s.i.f, ir, rta);
	modrm(mi, i->s.i.s, rta, r);
}

static void
opwst(Inst *i, int mi, int r)
{
	int ir, rta;

	switch(UXDST(i->add)) {
	default:
		print("%D\n", i);
		urk();
	case DST(AIMM):
		con(i->d.imm, r);
		return;
	case DST(AFP):
		modrm(mi, i->d.ind, RFP, r);
		return;
	case DST(AMP):
		modrm(mi, i->d.ind, RMP, r);
		return;
	case DST(AIND|AFP):
		ir = RFP;
		break;
	case DST(AIND|AMP):
		ir = RMP;
		break;
	}
	rta = RTA;
	if(mi == Olea)
		rta = r;
	modrm(Oldw, i->d.i.f, ir, rta);
	modrm(mi, i->d.i.s, rta, r);
}

static void
bra(ulong dst, int op)
{
	dst -= (DOT+5);
	genb(op);
	genw(dst);
}

static void
rbra(ulong dst, int op)
{
	dst += (ulong)base;
	dst -= DOT+5;
	genb(op);
	genw(dst);
}

static void
literal(ulong imm, int roff)
{
	nlit++;

	genb(Omovimm+RAXenum);
	genw((ulong)litpool);
	modrm(Ostw, roff, RTMP, RAXenum);

	if(pass == 0)
		return;

	*litpool = imm;
	litpool++;	
}

static void
punt(Inst *i, int m, void (*fn)(void))
{
	ulong pc;

	con((ulong)&R, RTMP);

	if(m & SRCOP) {
		if(UXSRC(i->add) == SRC(AIMM))
			literal(i->s.imm, O(REG, s));
		else {
			opwld(i, Olea, RAXenum);
			modrm(Ostw, O(REG, s), RTMP, RAXenum);
		}
	}

	if(m & DSTOP) {
		opwst(i, Olea, 0);
		modrm(Ostw, O(REG, d), RTMP, RAXenum);
	}
	if(m & WRTPC) {
		modrm(Omov, O(REG, PC), RTMP, 0);
		pc = patch[i-mod->prog+1];
		genw((ulong)base + pc);
	}
	if(m & DBRAN) {
		pc = patch[(Inst*)i->d.imm-mod->prog];
		literal((ulong)base+pc, O(REG, d));
	}

	switch(i->add&ARM) {
	case AXNON:
		if(m & THREOP) {
			modrm(Oldw, O(REG, d), RTMP, RAXenum);
			modrm(Ostw, O(REG, m), RTMP, RAXenum);
		}
		break;
	case AXIMM:
		literal((short)i->reg, O(REG, m));
		break;
	case AXINF:
		modrm(Olea, i->reg, RFP, RAXenum);
		modrm(Ostw, O(REG, m), RTMP, RAXenum);
		break;
	case AXINM:
		modrm(Olea, i->reg, RMP, RAXenum);
		modrm(Ostw, O(REG, m), RTMP, RAXenum);
		break;
	}
	modrm(Ostw, O(REG, FP), RTMP, RFP);

	bra((ulong)fn, Ocall);

	con((ulong)&R, RTMP);
	if(m & TCHECK) {
		modrm(Ocmpi, O(REG, t), RTMP, 7);// CMPL $0, R.t
		genb(0x00);
		gen2(Ojeqb, 0x06);		// JEQ	.+6
		genb(Opopl+RDIenum);
		genb(Opopl+RSIenum);
		genb(Opopl+RDXenum);
		genb(Opopl+RCXenum);
		genb(Opopl+RBXenum);
		genb(Oret);
	}

	modrm(Oldw, O(REG, FP), RTMP, RFP);
	modrm(Oldw, O(REG, MP), RTMP, RMP);

	if(m & NEWPC) {
		modrm(Oldw, O(REG, PC), RTMP, RAXenum);
		gen2(Ojmprm, (3<<6)|(4<<3)|RAXenum);
	}
}

static void
mid(Inst *i, uchar mi, int r)
{
	int ir;

	switch(i->add&ARM) {
	default:
		opwst(i, mi, r);
		return;
	case AXIMM:
		con((short)i->reg, r);
		return;
	case AXINF:
		ir = RFP;
		break;
	case AXINM:
		ir = RMP;
		break;
	}
	modrm(mi, i->reg, ir, r);
}

static void
arith(Inst *i, int op2, int rm)
{
	if(UXSRC(i->add) != SRC(AIMM)) {
		if(i->add&ARM) {
			mid(i, Oldw, RAXenum);
			opwld(i, op2|2, 0);
			opwst(i, Ostw, 0);
			return;
		}
		opwld(i, Oldw, RAXenum);
		opwst(i, op2, 0);
		return;
	}
	if(i->add&ARM) {
		mid(i, Oldw, RAXenum);
		if(bc(i->s.imm)) {
			gen2(0x83, (3<<6)|(rm<<3)|RAXenum);
			genb(i->s.imm);
		}
		else {
			gen2(0x81, (3<<6)|(rm<<3)|RAXenum);
			genw(i->s.imm);
		}
		opwst(i, Ostw, RAXenum);
		return;
	}
	if(bc(i->s.imm)) {
		opwst(i, 0x83, rm);
		genb(i->s.imm);
		return;
	}
	opwst(i, 0x81, rm);
	genw(i->s.imm);
}

static void
arithb(Inst *i, int op2)
{
	if(UXSRC(i->add) == SRC(AIMM))
		urk();

	if(i->add&ARM) {
		mid(i, Oldb, RAXenum);
		opwld(i, op2|2, 0);
		opwst(i, Ostb, 0);
		return;
	}
	opwld(i, Oldb, RAXenum);
	opwst(i, op2, RAXenum);
}

static void
shift(Inst *i, int ld, int st, int op, int r)
{
	mid(i, ld, RAXenum);
	opwld(i, Oldw, RCXenum);
	gen2(op, (3<<6)|(r<<3)|RAXenum);
	opwst(i, st, RAXenum);
}

static void
arithf(Inst *i, int op)
{
	opwld(i, Omovf, 0);
	mid(i, 0xdc, op);
	opwst(i, Omovf, 3);
}

static void
cmpl(int r, ulong v)
{
	if(bc(v)) {
		gen2(0x83, (3<<6)|(7<<3)|r);
		genb(v);
		return;
	}
	gen2(0x81, (3<<6)|(7<<3)|r);
	genw(v);
}

static int
swapbraop(int b)
{
	switch(b) {
	case Ojgel:
		return Ojlel;
	case Ojlel:
		return Ojgel;
	case Ojgtl:
		return Ojltl;
	case Ojltl:
		return Ojgtl;
	}
	return b;
}

static void
cbra(Inst *i, int jmp)
{
	mid(i, Oldw, RAXenum);
	if(UXSRC(i->add) == SRC(AIMM)) {
		cmpl(RAXenum, i->s.imm);
		jmp = swapbraop(jmp);
	}
	else
		opwld(i, Ocmpw, RAXenum);
	genb(0x0f);
	rbra(patch[i->d.ins-mod->prog], jmp);
}

static void
cbral(Inst *i, int jmsw, int jlsw, int mode)
{
	ulong dst;
	uchar *label;

	opwld(i, Olea, RTMP);
	mid(i, Olea, RTA);
	modrm(Oldw, 4, RTA, RAXenum);
	modrm(Ocmpw, 4, RTMP, RAXenum);
	label = 0;
	dst = patch[i->d.ins-mod->prog];
	switch(mode) {
	case ANDAND:
		gen2(jmsw, 0);
		label = code-1;
		break;
	case OROR:
		genb(0x0f);
		rbra(dst, jmsw);
		break;
	case EQAND:
		genb(0x0f);
		rbra(dst, jmsw);
		gen2(Ojneb, 0);
		label = code-1;
		break;
	}
	modrm(Oldw, 0, RTA, RAXenum);
	modrm(Ocmpw, 0, RTMP, RAXenum);
	genb(0x0f);
	rbra(dst, jlsw);
	if(label != nil)
		*label = code-label-1;
}

static void
cbrab(Inst *i, int jmp)
{
	mid(i, Oldb, RAXenum);
	if(UXSRC(i->add) == SRC(AIMM))
		urk();

	opwld(i, Ocmpb, RAXenum);
	genb(0x0f);
	rbra(patch[i->d.ins-mod->prog], jmp);
}

static void
cbraf(Inst *i, int jmp)
{
	opwld(i, Omovf, 0);
	mid(i, 0xdc, 3);	// FCOMP
	genb(0x9b);		// FWAIT
	gen2(0xdf, 0xe0);	// FSTSW AX
	genb(0x9e);		// SAHF

	genb(0x0f);
	rbra(patch[i->d.ins-mod->prog], jmp);
}

static void
comcase(Inst *i, int w)
{
	int l;
	WORD *t, *e;

	if(w != 0) {
		opwld(i, Oldw, RAXenum);		// v
		genb(Opushl+RSIenum);
		opwst(i, Olea, RSIenum);		// table
		rbra(macro[MacCASE], Ojmp);
	}

	t = (WORD*)(mod->origmp+i->d.ind+4);
	l = t[-1];
	if(pass == 0) {
		if(l > 0)
			t[-1] = -l;	/* Mark it not done */
		return;
	}
	if(l >= 0)			/* Check pass 2 done */
		return;
	t[-1] = -l;			/* Set real count */
	e = t + t[-1]*3;
	while(t < e) {
		t[2] = (ulong)base + patch[t[2]];
		t += 3;
	}
	t[0] = (ulong)base + patch[t[0]];
}

static void
commframe(Inst *i)
{
	int o;
	uchar *punt, *mlnil;

	opwld(i, Oldw, RAXenum);
	cmpl(RAXenum, (ulong)H);
	gen2(Ojeqb, 0);
	mlnil = code - 1;
	if((i->add&ARM) == AXIMM) {
		o = OA(Modlink, links)+i->reg*sizeof(Modl)+O(Modl, frame);
		modrm(Oldw, o, RAXenum, RTA);
	} else {
		gen2(Oldw, (3<<6)|(RTMP<<3)|RAXenum);	// MOVL	AX, RTMP
		mid(i, Oldw, RCXenum);			// index
		gen2(Olea, (0<<6)|(0<<3)|4);		// lea	(AX)(RCXenum*8)
		genb((3<<6)|(RCXenum<<3)|RAXenum);		// assumes sizeof(Modl) == 8 hence 3
		o = OA(Modlink, links)+O(Modl, frame);
		modrm(Oldw, o, RAXenum, RTA);		// frame
		genb(OxchgAX+RTMP);			// get old AX back
	}
	modrm(0x83, O(Type, initialize), RTA, 7);
	genb(0);
	gen2(Ojneb, 0);
	punt = code - 1;
	genb(OxchgAX+RTA);
	opwst(i, Olea, RTA);
	*mlnil = code-mlnil-1;
	rbra(macro[MacMFRA], Ocall);
	rbra(patch[i-mod->prog+1], Ojmp);

	*punt = code-punt-1;
	rbra(macro[MacFRAM], Ocall);
	opwst(i, Ostw, RCXenum);
}

static void
commcall(Inst *i)
{
	uchar *mlnil;

	con((ulong)&R, RTMP);			// MOVL	$R, RTMP
	opwld(i, Oldw, RCXenum);
	modrm(Omov, O(Frame, lr), RCXenum, 0);	// MOVL $.+1, lr(CX)	f->lr = R.PC
	genw((ulong)base+patch[i-mod->prog+1]);
	modrm(Ostw, O(Frame, fp), RCXenum, RFP); 	// MOVL RFP, fp(CX)	f->fp = R.FP
	modrm(Oldw, O(REG, M), RTMP, RTA);	// MOVL R.M, RTA
	modrm(Ostw, O(Frame, mr), RCXenum, RTA);	// MOVL RTA, mr(CX) 	f->mr = R.M
	opwst(i, Oldw, RTA);			// MOVL ml, RTA
	cmpl(RTA, (ulong)H);
	gen2(Ojeqb, 0);
	mlnil = code - 1;
	if((i->add&ARM) == AXIMM)
		modrm(Oldw, OA(Modlink, links)+i->reg*sizeof(Modl)+O(Modl, u.pc), RTA, RAXenum);
	else {
		genb(Opushl+RCXenum);
		mid(i, Oldw, RCXenum);		// index
		gen2(Olea, (0<<6)|(0<<3)|4);	// lea	(RTA)(RCXenum*8)
		genb((3<<6)|(RCXenum<<3)|RTA);	// assumes sizeof(Modl) == 8 hence 3
		modrm(Oldw, OA(Modlink, links)+O(Modl, u.pc), RAXenum, RAXenum);
		genb(Opopl+RCXenum);
	}
	*mlnil = code-mlnil-1;
	rbra(macro[MacMCAL], Ocall);
}

static void
larith(Inst *i, int op, int opc)
{
	opwld(i, Olea, RTMP);
	mid(i, Olea, RTA);
	modrm(Oldw, 0, RTA, RAXenum);	// MOVL	0(RTA), AX
	modrm(op, 0, RTMP, RAXenum);	// ADDL 0(RTMP), AX
	modrm(Oldw, 4, RTA, RCXenum);	// MOVL 4(RTA), CX
	modrm(opc, 4, RTMP, RCXenum);	// ADCL 4(RTMP), CX
	if((i->add&ARM) != AXNON)
		opwst(i, Olea, RTA);
	modrm(Ostw, 0, RTA, RAXenum);
	modrm(Ostw, 4, RTA, RCXenum);
}

static void
shll(Inst *i)
{
	uchar *label, *label1;

	opwld(i, Oldw, RCXenum);
	mid(i, Olea, RTA);
	gen2(Otestib, (3<<6)|(0<<3)|RCXenum);
	genb(0x20);
	gen2(Ojneb, 0);
	label = code-1;
	modrm(Oldw, 0, RTA, RAXenum);
	modrm(Oldw, 4, RTA, RBXenum);
	genb(0x0f);
	gen2(Oshld, (3<<6)|(RAXenum<<3)|RBXenum);
	gen2(Oshl, (3<<6)|(4<<3)|RAXenum);
	gen2(Ojmpb, 0);
	label1 = code-1;
	*label = code-label-1;
	modrm(Oldw, 0, RTA, RBXenum);
	con(0, RAXenum);
	gen2(Oshl, (3<<6)|(4<<3)|RBXenum);
	*label1 = code-label1-1;
	opwst(i, Olea, RTA);
	modrm(Ostw, 0, RTA, RAXenum);
	modrm(Ostw, 4, RTA, RBXenum);
}

static void
shrl(Inst *i)
{
	uchar *label, *label1;

	opwld(i, Oldw, RCXenum);
	mid(i, Olea, RTA);
	gen2(Otestib, (3<<6)|(0<<3)|RCXenum);
	genb(0x20);
	gen2(Ojneb, 0);
	label = code-1;
	modrm(Oldw, 0, RTA, RAXenum);
	modrm(Oldw, 4, RTA, RBXenum);
	genb(0x0f);
	gen2(Oshrd, (3<<6)|(RBXenum<<3)|RAXenum);
	gen2(Osar, (3<<6)|(7<<3)|RBXenum);
	gen2(Ojmpb, 0);
	label1 = code-1;
	*label = code-label-1;
	modrm(Oldw, 4, RTA, RBXenum);
	gen2(Oldw, (3<<6)|(RAXenum<<3)|RBXenum);
	gen2(Osarimm, (3<<6)|(7<<3)|RBXenum);
	genb(0x1f);
	gen2(Osar, (3<<6)|(7<<3)|RAXenum);
	*label1 = code-label1-1;
	opwst(i, Olea, RTA);
	modrm(Ostw, 0, RTA, RAXenum);
	modrm(Ostw, 4, RTA, RBXenum);
}

static
void
compdbg(void)
{
	print("%s:%d@%.8lux\n", R.M->m->name, *(ulong*)R.m, *(ulong*)R.s);
}

static void
comp(Inst *i)
{
	int r;
	WORD *t, *e;
	char buf[ERRLEN];

	if(0) {
		Inst xx;
		xx.add = AXIMM|SRC(AIMM);
		xx.s.imm = (ulong)code;
		xx.reg = i-mod->prog;
		punt(&xx, SRCOP, compdbg);
	}

	switch(i->op) {
	default:
		snprint(buf, sizeof buf, "%s compile, no '%D'", mod->name, i);
		error(buf);
		break;
	case IMCALL:
		commcall(i);
		break;
	case ISEND:
	case IRECV:
	case IALT:
		punt(i, SRCOP|DSTOP|TCHECK|WRTPC, optab[i->op]);
		break;
	case ISPAWN:
		punt(i, SRCOP|DBRAN, optab[i->op]);
		break;
	case IBNEC:
	case IBEQC:
	case IBLTC:
	case IBLEC:
	case IBGTC:
	case IBGEC:
		punt(i, SRCOP|DBRAN|NEWPC|WRTPC, optab[i->op]);
		break;
	case ICASEC:
		comcase(i, 0);
		punt(i, SRCOP|DSTOP|NEWPC, optab[i->op]);
		break;
	case IADDC:
	case IMULL:
	case IDIVL:
	case IMODL:
	case IMNEWZ:
	case ILSRW:
	case ILSRL:
		punt(i, SRCOP|DSTOP|THREOP, optab[i->op]);
		break;
	case ILOAD:
	case INEWA:
	case INEWAZ:
	case INEW:
	case INEWZ:
	case ISLICEA:
	case ISLICELA:
	case ICONSB:
	case ICONSW:
	case ICONSL:
	case ICONSF:
	case ICONSM:
	case ICONSMP:
	case ICONSP:
	case IMOVMP:
	case IHEADMP:
	case IHEADL:
	case IINSC:
	case ICVTAC:
	case ICVTCW:
	case ICVTWC:
	case ICVTLC:
	case ICVTCL:
	case ICVTFC:
	case ICVTCF:
	case ICVTRF:
	case ICVTFR:
	case ICVTWS:
	case ICVTSW:
	case IMSPAWN:
	case ICVTCA:
	case ISLICEC:
	case INEWCM:
	case INEWCMP:
	case INBALT:
		punt(i, SRCOP|DSTOP, optab[i->op]);
		break;
	case IMFRAME:
		commframe(i);
		break;
	case INEWCB:
	case INEWCW:
	case INEWCF:
	case INEWCP:
	case INEWCL:
		punt(i, DSTOP, optab[i->op]);
		break;
	case IEXIT:
		punt(i, 0, optab[i->op]);
		break;
	case ICVTBW:
		opwld(i, Oldb, RAXenum);
		genb(0x0f);
		gen2(0xb6, (3<<6)|(RAXenum<<3)|RAXenum);
		opwst(i, Ostw, RAXenum);
		break;
	case ICVTWB:
		opwld(i, Oldw, RAXenum);
		opwst(i, Ostb, RAXenum);
		break;
	case ICVTFW:
		opwld(i, Omovf, 0);
		opwst(i, 0xdb, 3);
		break;
	case ICVTWF:
		opwld(i, 0xdb, 0);
		opwst(i, Omovf, 3);
		break;
	case ICVTLF:
		opwld(i, 0xdf, 5);
		opwst(i, Omovf, 3);
		break;
	case ICVTFL:
		opwld(i, Omovf, 0);
		opwst(i, 0xdf, 7);
		break;
	case IHEADM:
		opwld(i, Oldw, RAXenum);
		modrm(Olea, OA(List, data), RAXenum, RAXenum);
		goto movm;
	case IMOVM:
		opwld(i, Olea, RAXenum);
	movm:
		opwst(i, Olea, RBXenum);
		mid(i, Oldw, RCXenum);
		genb(OxchgAX+RSIenum);
		gen2(Oxchg, (3<<6)|(RDIenum<<3)|RBXenum);
		genb(Ocld);
		gen2(Orep, Omovsb);
		genb(OxchgAX+RSIenum);
		gen2(Oxchg, (3<<6)|(RDIenum<<3)|RBXenum);
		break;
	case IRET:
		rbra(macro[MacRET], Ojmp);
		break;
	case IFRAME:
		if(UXSRC(i->add) != SRC(AIMM)) {
			punt(i, SRCOP|DSTOP, optab[i->op]);
			break;
		}
		tinit[i->s.imm] = 1;
		con((ulong)mod->type[i->s.imm], RTA);
		rbra(macro[MacFRAM], Ocall);
		opwst(i, Ostw, RCXenum);
		break;
	case ILEA:
		if(UXSRC(i->add) == SRC(AIMM)) {
			gen2(Ojmpb, 4);
			genw(i->s.imm);
			con((ulong)(code-4), RAXenum);
		}
		else
			opwld(i, Olea, RAXenum);
		opwst(i, Ostw, RAXenum);
		break;
	case IHEADW:
		opwld(i, Oldw, RAXenum);
		modrm(Oldw, OA(List, data), RAXenum, RAXenum);
		opwst(i, Ostw, RAXenum);
		break;
	case IHEADF:
		opwld(i, Oldw, RAXenum);
		modrm(Omovf, OA(List, data), RAXenum, 0);
		opwst(i, Omovf, 3);
		break;
	case IHEADB:
		opwld(i, Oldw, RAXenum);
		modrm(Oldb, OA(List, data), RAXenum, RAXenum);
		opwst(i, Ostb, RAXenum);
		break;
	case ITAIL:
		opwld(i, Oldw, RAXenum);
		modrm(Oldw, O(List, tail), RAXenum, RBXenum);
		goto movp;
	case IMOVP:
	case IHEADP:
		opwld(i, Oldw, RBXenum);
		if(i->op == IHEADP)
			modrm(Oldw, OA(List, data), RBXenum, RBXenum);
	movp:
		cmpl(RBXenum, (ulong)H);
		gen2(Ojeqb, 0x05);
		rbra(macro[MacCOLR], Ocall);
		opwst(i, Oldw, RAXenum);
		opwst(i, Ostw, RBXenum);
		rbra(macro[MacFRP], Ocall);
		break;
	case ILENA:
		opwld(i, Oldw, RBXenum);
		con(0, RAXenum);
		cmpl(RBXenum, (ulong)H);
		gen2(Ojeqb, 0x02);
		modrm(Oldw, O(Array, len), RBXenum, RAXenum);
		opwst(i, Ostw, RAXenum);
		break;
	case ILENC:
		opwld(i, Oldw, RBXenum);
		con(0, RAXenum);
		cmpl(RBXenum, (ulong)H);
		gen2(Ojeqb, 0x09);
		modrm(Oldw, O(String, len), RBXenum, RAXenum);
		cmpl(RAXenum, 0);
		gen2(Ojgeb, 0x02);
		gen2(Oneg, (3<<6)|(3<<3)|RAXenum);
		opwst(i, Ostw, RAXenum);
		break;
	case ILENL:
		con(0, RAXenum);
		opwld(i, Oldw, RBXenum);
		cmpl(RBXenum, (ulong)H);
		gen2(Ojeqb, 0x05);
		modrm(Oldw, O(List, tail), RBXenum, RBXenum);
		genb(Oincr+RAXenum);
		gen2(Ojmpb, 0xf6);
		opwst(i, Ostw, RAXenum);
		break;
	case IBEQF:
		cbraf(i, Ojeql);
		break;
	case IBNEF:
		cbraf(i, Ojnel);
		break;
	case IBLEF:
		cbraf(i, Ojlsl);
		break;
	case IBLTF:
		cbraf(i, Ojcsl);
		break;
	case IBGEF:
		cbraf(i, Ojccl);
		break;
	case IBGTF:
		cbraf(i, Ojhil);
		break;
	case IBEQW:
		cbra(i, Ojeql);
		break;
	case IBLEW:
		cbra(i, Ojlel);
		break;
	case IBNEW:
		cbra(i, Ojnel);
		break;
	case IBGTW:
		cbra(i, Ojgtl);
		break;
	case IBLTW:
		cbra(i, Ojltl);
		break;
	case IBGEW:
		cbra(i, Ojgel);
		break;
	case IBEQB:
		cbrab(i, Ojeql);
		break;
	case IBLEB:
		cbrab(i, Ojlsl);
		break;
	case IBNEB:
		cbrab(i, Ojnel);
		break;
	case IBGTB:
		cbrab(i, Ojhil);
		break;
	case IBLTB:
		cbrab(i, Ojbl);
		break;
	case IBGEB:
		cbrab(i, Ojael);
		break;
	case ISUBW:
		arith(i, 0x29, 5);
		break;
	case ISUBB:
		arithb(i, 0x28);
		break;
	case ISUBF:
		arithf(i, 5);
		break;
	case IADDW:
		arith(i, 0x01, 0);
		break;
	case IADDB:
		arithb(i, 0x00);
		break;
	case IADDF:
		arithf(i, 0);
		break;
	case IORW:
		arith(i, 0x09, 1);
		break;
	case IORB:
		arithb(i, 0x08);
		break;
	case IANDW:
		arith(i, 0x21, 4);
		break;
	case IANDB:
		arithb(i, 0x20);
		break;
	case IXORW:
		arith(i, Oxor, 6);
		break;
	case IXORB:
		arithb(i, 0x30);
		break;
	case ISHLW:
		shift(i, Oldw, Ostw, 0xd3, 4);
		break;
	case ISHLB:
		shift(i, Oldb, Ostb, 0xd2, 4);
		break;
	case ISHRW:
		shift(i, Oldw, Ostw, 0xd3, 7);
		break;
	case ISHRB:
		shift(i, Oldb, Ostb, 0xd2, 5);
		break;
	case IMOVF:
		opwld(i, Omovf, 0);
		opwst(i, Omovf, 3);
		break;
	case INEGF:
		opwld(i, Omovf, 0);
		genb(0xd9);
		genb(0xe0);
		opwst(i, Omovf, 3);
		break;
	case IMOVB:
		opwld(i, Oldb, RAXenum);
		opwst(i, Ostb, RAXenum);
		break;
	case IMOVW:
	case ICVTLW:			// Little endian
		if(UXSRC(i->add) == SRC(AIMM)) {
			opwst(i, Omov, RAXenum);
			genw(i->s.imm);
			break;
		}
		opwld(i, Oldw, RAXenum);
		opwst(i, Ostw, RAXenum);
		break;
	case ICVTWL:
		opwst(i, Olea, RTMP);
		opwld(i, Oldw, RAXenum);
		modrm(Ostw, 0, RTMP, RAXenum);
		genb(0x99);
		modrm(Ostw, 4, RTMP, RDXenum);
		break;
	case ICALL:
		opwld(i, Oldw, RAXenum);
		modrm(Omov, O(Frame, lr), RAXenum, 0);	// MOVL $.+1, lr(AX)
		genw((ulong)base+patch[i-mod->prog+1]);
		modrm(Ostw, O(Frame, fp), RAXenum, RFP); 	// MOVL RFP, fp(AX)
		gen2(Oldw, (3<<6)|(RFP<<3)|RAXenum);	// MOVL AX,RFP
		/* no break */
	case IJMP:
		rbra(patch[i->d.ins-mod->prog], Ojmp);
		break;
	case IGOTO:
		opwst(i, Olea, RBXenum);
		opwld(i, Oldw, RAXenum);
		gen2(Ojmprm, (0<<6)|(4<<3)|4);
		genb((2<<6)|(RAXenum<<3)|RBXenum);

		if(pass == 0)
			break;

		t = (WORD*)(mod->origmp+i->d.ind);
		e = t + t[-1];
		t[-1] = 0;
		while(t < e) {
			t[0] = (ulong)base + patch[t[0]];
			t++;
		}
		break;
	case IMULF:
		arithf(i, 1);
		break;
	case IDIVF:
		arithf(i, 7);
		break;
	case IMODW:
	case IDIVW:
	case IMULW:
		mid(i, Oldw, RAXenum);
		opwld(i, Oldw, RTMP);
		if(i->op == IMULW)
			gen2(0xf7, (3<<6)|(4<<3)|RTMP);
		else {
			genb(Ocdq);
			gen2(0xf7, (3<<6)|(7<<3)|RTMP);	// IDIV AX, RTMP
			if(i->op == IMODW)
				genb(0x90+RDXenum);		// XCHG	AX, DX
		}
		opwst(i, Ostw, RAXenum);
		break;
	case IMODB:
	case IDIVB:
	case IMULB:
		mid(i, Oldb, RAXenum);
		opwld(i, Oldb, RTMP);
		if(i->op == IMULB)
			gen2(0xf6, (3<<6)|(4<<3)|RTMP);
		else {
			genb(Ocdq);
			gen2(0xf6, (3<<6)|(7<<3)|RTMP);	// IDIV AX, RTMP
			if(i->op == IMODB)
				genb(0x90+RDXenum);		// XCHG	AX, DX
		}
		opwst(i, Ostb, RAXenum);
		break;
	case IINDX:
		opwld(i, Oldw, RTMP);			// MOVW	xx(s), BX
		modrm(Oldw, O(Array, t), RTMP, RAXenum);	// MOVW	t(BX), AX
		modrm(Oldw, O(Type, size), RAXenum, RAXenum);	// MOVW size(AX), AX
		if(UXDST(i->add) == DST(AIMM)) {
			gen2(0x69, (3<<6)|(RAXenum<<3)|0);
			genw(i->d.imm);
		}
		else
			opwst(i, 0xf7, 5);		// IMULL AX,xx(d)

		modrm(0x03, O(Array, data), RBXenum, RAXenum);	// ADDL data(BX), AX
		r = RMP;
		if((i->add&ARM) == AXINF)
			r = RFP;
		modrm(Ostw, i->reg, r, RAXenum);
		break;
	case IINDB:
		r = 0;
		goto idx;
	case IINDF:
	case IINDL:
		r = 3;
		goto idx;
	case IINDW:
		r = 2;
	idx:
		opwld(i, Oldw, RAXenum);
		opwst(i, Oldw, RTMP);
		modrm(Oldw, O(Array, data), RAXenum, RAXenum);
		gen2(Olea, (0<<6)|(0<<3)|4);		/* lea	(AX)(RTMP*r) */
		genb((r<<6)|(RTMP<<3)|RAXenum);
		r = RMP;
		if((i->add&ARM) == AXINF)
			r = RFP;
		modrm(Ostw, i->reg, r, RAXenum);
		break;
	case IINDC:
		opwld(i, Oldw, RAXenum);			// string
		mid(i, Oldw, RBXenum);			// index
		modrm(Ocmpi, O(String, len), RAXenum, 7);
		genb(0);
		gen2(Ojltb, 7);
		genb(0x0f);
		gen2(Omovzxb, (1<<6)|(0<<3)|4);		/* movzbx 12(AX)(RBXenum*1), RAXenum */
		gen2((0<<6)|(RBXenum<<3)|RAXenum, O(String, data));
		gen2(Ojmpb, 5);
		genb(0x0f);
		gen2(Omovzxw, (1<<6)|(0<<3)|4);		/* movzwx 12(AX)(RBXenum*4), RAXenum */
		gen2((1<<6)|(RBXenum<<3)|RAXenum, O(String, data));
		opwst(i, Ostw, RAXenum);
		break;
	case ICASE:
		comcase(i, 1);
		break;
	case IMOVL:
		opwld(i, Olea, RTA);
		opwst(i, Olea, RTMP);
		modrm(Oldw, 0, RTA, RAXenum);
		modrm(Ostw, 0, RTMP, RAXenum);
		modrm(Oldw, 4, RTA, RAXenum);
		modrm(Ostw, 4, RTMP, RAXenum);
		break;
	case IADDL:
		larith(i, 0x03, 0x13);
		break;
	case ISUBL:
		larith(i, 0x2b, 0x1b);
		break;
	case IORL:
		larith(i, 0x0b, 0x0b);
		break;
	case IANDL:
		larith(i, 0x23, 0x23);
		break;
	case IXORL:
		larith(i, 0x33, 0x33);
		break;
	case IBEQL:
		cbral(i, Ojneb, Ojeql, ANDAND);
		break;
	case IBNEL:
		cbral(i, Ojnel, Ojnel, OROR);
		break;
	case IBLEL:
		cbral(i, Ojltl, Ojbel, EQAND);
		break;
	case IBGTL:
		cbral(i, Ojgtl, Ojal, EQAND);
		break;
	case IBLTL:
		cbral(i, Ojltl, Ojbl, EQAND);
		break;
	case IBGEL:
		cbral(i, Ojgtl, Ojael, EQAND);
		break;
	case ISHLL:
		shll(i);
		break;
	case ISHRL:
		shrl(i);
		break;
	}
}

static int
preamble(void)
{
	if(comvec)
		return 0;

	comvec = malloc(32);
	if(comvec == nil)
		return -1;
	code = (uchar*)comvec;

	genb(Opushl+RBXenum);
	genb(Opushl+RCXenum);
	genb(Opushl+RDXenum);
	genb(Opushl+RSIenum);
	genb(Opushl+RDIenum);
	con((ulong)&R, RTMP);
	modrm(Oldw, O(REG, FP), RTMP, RFP);
	modrm(Oldw, O(REG, MP), RTMP, RMP);
	modrm(Ojmprm, O(REG, PC), RTMP, 4);

	return 0;
}

static void
maccase(void)
{
	uchar *loop, *def, *lab1;

	modrm(Oldw, 0, RSIenum, RDXenum);		// n = t[0]
	modrm(Olea, 4, RSIenum, RSIenum);		// t = &t[1]
	gen2(Oldw, (3<<6)|(RBXenum<<3)|RDXenum);	// MOVL	DX, BX
	gen2(Oshr, (3<<6)|(4<<3)|RBXenum);		// SHL	BX,1
	gen2(0x01, (3<<6)|(RDXenum<<3)|RBXenum);	// ADDL	DX, BX	BX = n*3
	gen2(Opushrm, (0<<6)|(6<<3)|4);
	genb((2<<6)|(RBXenum<<3)|RSIenum);		// PUSHL 0(SI)(BX*4)
	loop = code;
	cmpl(RDXenum, 0);
	gen2(Ojleb, 0);
	def = code-1;
	gen2(Oldw, (3<<6)|(RCXenum<<3)|RDXenum);	// MOVL	DX, CX	n2 = n
	gen2(Oshr, (3<<6)|(5<<3)|RCXenum);		// SHR	CX,1	n2 = n2>>1
	gen2(Oldw, (3<<6)|(RBXenum<<3)|RCXenum);	// MOVL	CX, BX
	gen2(Oshr, (3<<6)|(4<<3)|RBXenum);		// SHL	BX,1
	gen2(0x01, (3<<6)|(RCXenum<<3)|RBXenum);	// ADDL	CX, BX	BX = n2*3
	gen2(0x3b, (0<<6)|(RAXenum<<3)|4);
	genb((2<<6)|(RBXenum<<3)|RSIenum);		// CMPL AX, 0(SI)(BX*4)
	gen2(Ojgeb, 0);				// JGE	lab1
	lab1 = code-1;
	gen2(Oldw, (3<<6)|(RDXenum<<3)|RCXenum);
	gen2(Ojmpb, loop-code-2);
	*lab1 = code-lab1-1;			// lab1:
	gen2(0x3b, (1<<6)|(RAXenum<<3)|4);
	gen2((2<<6)|(RBXenum<<3)|RSIenum, 4);		// CMPL AX, 4(SI)(BX*4)
	gen2(Ojltb, 0);
	lab1 = code-1;
	gen2(Olea, (1<<6)|(RSIenum<<3)|4);
	gen2((2<<6)|(RBXenum<<3)|RSIenum, 12);		// LEA	12(SI)(RBX*4), RSI
	gen2(0x2b, (3<<6)|(RDXenum<<3)|RCXenum);	// SUBL	CX, DX		n -= n2
	gen2(Odecrm, (3<<6)|(1<<3)|RDXenum);	// DECL	DX		n -= 1
	gen2(Ojmpb, loop-code-2);
	*lab1 = code-lab1-1;			// lab1:
	gen2(Oldw, (1<<6)|(RAXenum<<3)|4);
	gen2((2<<6)|(RBXenum<<3)|RSIenum, 8);		// MOVL 8(SI)(BX*4), AX
	genb(Opopl+RSIenum);			// ditch default
	genb(Opopl+RSIenum);
	gen2(Ojmprm, (3<<6)|(4<<3)|RAXenum);	// JMP*L AX
	*def = code-def-1;			// def:
	genb(Opopl+RAXenum);			// ditch default
	genb(Opopl+RSIenum);
	gen2(Ojmprm, (3<<6)|(4<<3)|RAXenum);
}

static void
macfrp(void)
{
	cmpl(RAXenum, (ulong)H);			// CMPL AX, $H
	gen2(Ojneb, 0x01);			// JNE	.+1
	genb(Oret);				// RET
	modrm(0x83, O(Heap, ref)-sizeof(Heap), RAXenum, 7);
	genb(0x01);				// CMP	AX.ref, $1
	gen2(Ojeqb, 0x04);			// JNE	.+4
	modrm(Odecrm, O(Heap, ref)-sizeof(Heap), RAXenum, 1);
	genb(Oret);				// DEC	AX.ref
						// RET
	con((ulong)&R, RTMP);			// MOV  $R, RTMP
	modrm(Ostw, O(REG, FP), RTMP, RFP);	// MOVL	RFP, R.FP
	modrm(Ostw, O(REG, s), RTMP, RAXenum);	// MOVL	RAX, R.s
	bra((ulong)rdestroy, Ocall);		// CALL rdestroy
	con((ulong)&R, RTMP);			// MOVL	$R, RTMP
	modrm(Oldw, O(REG, FP), RTMP, RFP);	// MOVL	R.MP, RMP
	modrm(Oldw, O(REG, MP), RTMP, RMP);	// MOVL R.FP, RFP
	genb(Oret);
}

static void
macret(void)
{
	Inst i;
	uchar *s;
	static ulong lpunt, lnomr, lfrmr, linterp;

	s = code;

	lpunt -= 2;
	lnomr -= 2;
	lfrmr -= 2;
	linterp -= 2;

	con(0, RBXenum);				// MOVL  $0, RBX
	modrm(Oldw, O(Frame, t), RFP, RAXenum);	// MOVL  t(FP), RAX
	gen2(Ocmpw, (3<<6)|(RAXenum<<3)|RBXenum);	// CMPL  RAX, RBX
	gen2(Ojeqb, lpunt-(code-s));		// JEQ	 lpunt
	modrm(Oldw, O(Type, destroy), RAXenum, RAXenum);// MOVL  destroy(RAX), RAX
	gen2(Ocmpw, (3<<6)|(RAXenum<<3)|RBXenum);	// CMPL	 RAX, RBX
	gen2(Ojeqb, lpunt-(code-s));		// JEQ	 lpunt
	modrm(Ocmpw, O(Frame, fp), RFP, RBXenum);	// CMPL	 fp(FP), RBX
	gen2(Ojeqb, lpunt-(code-s));		// JEQ	 lpunt
	modrm(Ocmpw, O(Frame, mr), RFP, RBXenum);	// CMPL	 mr(FP), RBX
	gen2(Ojeqb, lnomr-(code-s));		// JEQ	 lnomr
	con((ulong)&R, RTMP);			// MOVL	 $R, RTMP
	modrm(Oldw, O(REG, M), RTMP, RTA);	// MOVL	 R.M, RTA
	modrm(Odecrm, O(Heap, ref)-sizeof(Heap), RTA, 1);
	gen2(Ojneb, lfrmr-(code-s));		// JNE	 lfrmr
	modrm(Oincrm, O(Heap, ref)-sizeof(Heap), RTA, 0);
	gen2(Ojmpb, lpunt-(code-s));		// JMP	 lpunt
	lfrmr = code - s;
	modrm(Oldw, O(Frame, mr), RFP, RTA);	// MOVL	 mr(FP), RTA
	modrm(Ostw, O(REG, M), RTMP, RTA);	// MOVL	 RTA, R.M
	modrm(Oldw, O(Modlink, MP), RTA, RMP);	// MOVL	 MP(RTA), RMP
	modrm(Ostw, O(REG, MP), RTMP, RMP);	// MOVL	 RMP, R.MP
	modrm(Ocmpi, O(Modlink, compiled), RTA, 7);// CMPL $0, M.compiled
	genb(0x00);
	gen2(Ojeqb, linterp-(code-s));		// JEQ	linterp
	lnomr = code - s;
	gen2(Ocallrm, (3<<6)|(2<<3)|RAXenum);	// CALL* AX
	con((ulong)&R, RTMP);			// MOVL	 $R, RTMP
	modrm(Ostw, O(REG, SP), RTMP, RFP);	// MOVL  RFP, R.SP
	modrm(Oldw, O(Frame, lr), RFP, RAXenum);	// MOVL  lr(RFP), RAX
	modrm(Oldw, O(Frame, fp), RFP, RFP);	// MOVL  fp(RFP), RFP
	modrm(Ostw, O(REG, FP), RTMP, RFP);	// MOVL  RFP, R.FP
	gen2(Ojmprm, (3<<6)|(4<<3)|RAXenum);	// JMP*L AX

	linterp = code - s;			// return to uncompiled code
	gen2(Ocallrm, (3<<6)|(2<<3)|RAXenum);	// CALL* AX
	con((ulong)&R, RTMP);			// MOVL	 $R, RTMP
	modrm(Ostw, O(REG, SP), RTMP, RFP);	// MOVL  RFP, R.SP
	modrm(Oldw, O(Frame, lr), RFP, RAXenum);	// MOVL  lr(RFP), RAX
	modrm(Ostw, O(REG, PC), RTMP, RAXenum);	// MOVL  RAX, R.PC
	modrm(Oldw, O(Frame, fp), RFP, RFP);	// MOVL  fp(RFP), RFP
	modrm(Ostw, O(REG, FP), RTMP, RFP);	// MOVL  RFP, R.FP
	genb(Opopl+RDIenum);			// return to uncompiled code
	genb(Opopl+RSIenum);
	genb(Opopl+RDXenum);
	genb(Opopl+RCXenum);
	genb(Opopl+RBXenum);
	genb(Oret);
						// label:
	lpunt = code - s;

	i.add = AXNON;
	punt(&i, TCHECK|NEWPC, optab[IRET]);
}

static void
maccolr(void)
{
	modrm(Oincrm, O(Heap, ref)-sizeof(Heap), RBXenum, 0);
	gen2(Oldw, (0<<6)|(RAXenum<<3)|5);		// INCL	ref(BX)
	genw((ulong)&mutator);			// MOVL	mutator, RAXenum
	modrm(Ocmpw, O(Heap, color)-sizeof(Heap), RBXenum, RAXenum);
	gen2(Ojneb, 0x01);			// CMPL	color(BX), RAXenum
	genb(Oret);				// MOVL $propagator,RTMP
	con(propagator, RAXenum);			// MOVL	RTMP, color(BX)
	modrm(Ostw, O(Heap, color)-sizeof(Heap), RBXenum, RAXenum);
	gen2(Ostw, (0<<6)|(RAXenum<<3)|5);		// can be any !0 value
	genw((ulong)&nprop);			// MOVL	RBXenum, nprop
	genb(Oret);
}

static void
macmcal(void)
{
	uchar *label, *mlnil, *interp;

	cmpl(RAXenum, (ulong)H);
	gen2(Ojeqb, 0);
	mlnil = code - 1;
	modrm(0x83, O(Modlink, prog), RTA, 7);	// CMPL $0, ml->prog
	genb(0x00);
	gen2(Ojneb, 0);				// JNE	patch
	label = code-1;
	*mlnil = code-mlnil-1;
	modrm(Ostw, O(REG, FP), RTMP, RCXenum);
	modrm(Ostw, O(REG, dt), RTMP, RAXenum);
	bra((ulong)rmcall, Ocall);		// CALL rmcall
	con((ulong)&R, RTMP);			// MOVL	$R, RTMP
	modrm(Oldw, O(REG, FP), RTMP, RFP);
	modrm(Oldw, O(REG, MP), RTMP, RMP);
	genb(Oret);				// RET
	*label = code-label-1;			// patch:
	gen2(Oldw, (3<<6)|(RFP<<3)|RCXenum);	// MOVL CX, RFP		R.FP = f
	modrm(Ostw, O(REG, M), RTMP, RTA);	// MOVL RTA, R.M
	modrm(Oincrm, O(Heap, ref)-sizeof(Heap), RTA, 0);
	modrm(Oldw, O(Modlink, MP), RTA, RMP);	// MOVL R.M->mp, RMP
	modrm(Ostw, O(REG, MP), RTMP, RMP);	// MOVL RMP, R.MP	R.MP = ml->MP
	modrm(Ocmpi, O(Modlink, compiled), RTA, 7);// CMPL $0, M.compiled
	genb(0x00);
	genb(Opopl+RTA);			// balance call
	gen2(Ojeqb, 0);				// JEQ	interp
	interp = code-1;
	gen2(Ojmprm, (3<<6)|(4<<3)|RAXenum);	// JMP*L AX
	*interp = code-interp-1;		// interp:
	modrm(Ostw, O(REG, FP), RTMP, RFP);	// MOVL FP, R.FP
	modrm(Ostw, O(REG, PC), RTMP, RAXenum);	// MOVL PC, R.PC
	genb(Opopl+RDIenum);			// call to uncompiled code
	genb(Opopl+RSIenum);
	genb(Opopl+RDXenum);
	genb(Opopl+RCXenum);
	genb(Opopl+RBXenum);
	genb(Oret);
}

static void
macfram(void)
{
	uchar *label;

	con((ulong)&R, RTMP);			// MOVL	$R, RTMP
	modrm(Oldw, O(REG, SP), RTMP, RAXenum);	// MOVL	R.SP, AX
	modrm(0x03, O(Type, size), RTA, RAXenum);	// ADDL size(RCXenum), RAX
	modrm(0x3b, O(REG, TS), RTMP, RAXenum);	// CMPL	AX, R.TS
	gen2(0x7c, 0x00);			// JL	.+(patch)
	label = code-1;

	modrm(Ostw, O(REG, s), RTMP, RTA);
	modrm(Ostw, O(REG, FP), RTMP, RFP);	// MOVL	RFP, R.FP
	bra((ulong)extend, Ocall);		// CALL	extend
	con((ulong)&R, RTMP);
	modrm(Oldw, O(REG, FP), RTMP, RFP);	// MOVL	R.MP, RMP
	modrm(Oldw, O(REG, MP), RTMP, RMP);	// MOVL R.FP, RFP
	modrm(Oldw, O(REG, s), RTMP, RCXenum);	// MOVL	R.s, *R.d
	genb(Oret);				// RET
	*label = code-label-1;
	modrm(Oldw, O(REG, SP), RTMP, RCXenum);	// MOVL	R.SP, CX
	modrm(Ostw, O(REG, SP), RTMP, RAXenum);	// MOVL	AX, R.SP

	modrm(Ostw, O(Frame, t), RCXenum, RTA);	// MOVL	RTA, t(CX) f->t = t
	modrm(Omov, REGMOD*4, RCXenum, 0);     	// MOVL $0, mr(CX) f->mr
	genw(0);
	modrm(Oldw, O(Type, initialize), RTA, RTA);
	gen2(Ojmprm, (3<<6)|(4<<3)|RTA);	// JMP*L RTA
	genb(Oret);				// RET
}

static void
macmfra(void)
{
	con((ulong)&R, RTMP);			// MOVL	$R, RTMP
	modrm(Ostw, O(REG, FP), RTMP, RFP);
	modrm(Ostw, O(REG, s), RTMP, RAXenum);	// Save type
	modrm(Ostw, O(REG, d), RTMP, RTA);	// Save destination
	bra((ulong)rmfram, Ocall);		// CALL rmfram
	con((ulong)&R, RTMP);			// MOVL	$R, RTMP
	modrm(Oldw, O(REG, FP), RTMP, RFP);
	modrm(Oldw, O(REG, MP), RTMP, RMP);
	genb(Oret);				// RET
}

static void
maceclr(void)
{
	Inst *i;
	static Inst ieclr = { IECLR, SRC(AXXX)|DST(AXXX) };

	i = &ieclr;
	punt(i, NEWPC, optab[i->op]);
}

void
comd(Type *t)
{
	int i, j, m, c;

	for(i = 0; i < t->np; i++) {
		c = t->map[i];
		j = i<<5;
		for(m = 0x80; m != 0; m >>= 1) {
			if(c & m) {
				modrm(Oldw, j, RFP, RAXenum);
				rbra(macro[MacFRP], Ocall);
			}
			j += sizeof(WORD*);
		}
	}
	genb(Oret);
}

void
comi(Type *t)
{
	int i, j, m, c;

	con((ulong)H, RAXenum);
	for(i = 0; i < t->np; i++) {
		c = t->map[i];
		j = i<<5;
		for(m = 0x80; m != 0; m >>= 1) {
			if(c & m)
				modrm(Ostw, j, RCXenum, RAXenum);
			j += sizeof(WORD*);
		}
	}
	genb(Oret);
}

void
typecom(Type *t)
{
	int n;
	uchar *tmp;

	if(t == nil || t->initialize != 0)
		return;

	tmp = mallocz(4096*sizeof(uchar), 0);
	if(tmp == nil)
		error(exNomem);

	code = tmp;
	comi(t);
	n = code - tmp;
	code = tmp;
	comd(t);
	n += code - tmp;
	free(tmp);

	code = mallocz(n, 0);
	if(code == nil)
		return;

	t->initialize = code;
	comi(t);
	t->destroy = code;
	comd(t);

	if(cflag > 1)
		print("typ= %.8lux %4d i %.8lux d %.8lux asm=%d\n",
			t, t->size, t->initialize, t->destroy, n);
}

int
compile(Module *m, int size, Modlink *ml)
{
	ulong v;
	Modl *e;
	Link *l;
	int i, n;
	uchar *s, *tmp;

	base = nil;
	patch = mallocz(size*sizeof(*patch), 0);
	tinit = malloc(m->ntype*sizeof(*tinit));
	tmp = mallocz(4096*sizeof(uchar),0);
	if(tinit == nil || patch == nil || tmp == nil || preamble() < 0)
		goto bad;

	mod = m;
	n = 0;
	pass = 0;
	nlit = 0;

	for(i = 0; i < size; i++) {
		code = tmp;
		comp(&m->prog[i]);
		patch[i] = n;
		n += code - tmp;
	}

	for(i = 0; i < nelem(mactab); i++) {
		code = tmp;
		mactab[i].gen();
		macro[mactab[i].idx] = n;
		n += code - tmp;
	}

	n = (n+3)&~3;

	nlit *= sizeof(ulong);
	base = mallocz(n + nlit, 0);
	if(base == nil)
		goto bad;

	if(cflag > 1)
		print("dis=%5d %5d 386=%5d asm=%.8lux lit=%d: %s\n",
			size, size*sizeof(Inst), n, base, nlit, m->name);

	pass++;
	nlit = 0;
	litpool = (ulong*)(base+n);
	code = base;

	for(i = 0; i < size; i++) {
		s = code;
		comp(&m->prog[i]);
		if(cflag > 2) {
			print("%D\n", &m->prog[i]);
			das(s, code-s);
		}
	}

	for(i = 0; i < nelem(mactab); i++)
		mactab[i].gen();

	v = (ulong)base;
	for(l = m->ext; l; l = l->next) {
		l->u.pc = (Inst*)(v+patch[l->u.pc-m->prog]);
		typecom(l->frame);
	}
	if(ml != nil) {
		e = &ml->links[0];
		for(i = 0; i < ml->nlinks; i++) {
			e->u.pc = (Inst*)(v+patch[e->u.pc-m->prog]);
			typecom(e->frame);
			e++;
		}
	}
	for(i = 0; i < m->ntype; i++) {
		if(tinit[i] != 0)
			typecom(m->type[i]);
	}

	m->entry = (Inst*)(v+patch[mod->entry-mod->prog]);
	free(patch);
	free(tinit);
	free(tmp);
	free(m->prog);
	m->prog = (Inst*)base;
	m->compiled = 1;
	m->eclr = (Inst*)(base+macro[MacECLR]);
	return 1;
bad:
	free(patch);
	free(tinit);
	free(tmp);
	return 0;
}
