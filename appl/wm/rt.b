implement WmRt;

include "sys.m";
	sys: Sys;
	sprint: import sys;

include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;

include "draw.m";
	draw: Draw;

include "tk.m";
	tk: Tk;
	Toplevel: import tk;

include	"wmlib.m";
	wmlib: Wmlib;

include "math.m";
	math: Math;

WmRt: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

t: ref Toplevel;
disptr: int;
disfile: string;
disobj: array of byte;

XMAGIC:		con	819248;
SMAGIC:		con	923426;
MUSTCOMPILE:	con	1<<0;
DONTCOMPILE:	con 	1<<1;

AMP:	con 16r00;	# Src/Dst op addressing
AFP:	con 16r01;
AIMM:	con 16r02;
AXXX:	con 16r03;
AIND:	con 16r04;
AMASK:	con 16r07;
AOFF:	con 16r08;
AVAL:	con 16r10;

ARM:	con 16rC0;	# Middle op addressing
AXNON:	con 16r00;
AXIMM:	con 16r40;
AXINF:	con 16r80;
AXINM:	con 16rC0;

DEFZ:	con 0;
DEFB:	con 1;		# Byte
DEFW:	con 2;		# Word
DEFS:	con 3;		# Utf-string
DEFF:	con 4;		# Real value
DEFA:	con 5;		# Array
DIND:	con 6;		# Set index
DAPOP:	con 7;		# Restore address register
DEFL:	con 8;		# BIG
DMAX:	con 1<<4;

TK:	con 1;

sign_mask:	con 16r8000000000000000;
exponent_mask:	con 16r7ff0000000000000;
mantissa_mask:	con 16r000fffffffffffff;

Inst: adt
{
	op:	int;
	addr:	int;
	mid:	int;
	src:	int;
	dst:	int;
};

Type: adt
{
	size:	int;
	np:	int;
	map:	array of byte;
};

Data: adt
{
	op:	int;
	n:	int;
	off:	int;
	i1:	int;
	i2:	int;
	bytes:	array of byte;
	words:	array of int;
	bigs:	array of big;
	reals:	array of real;
	str:	string;
};

Link: adt
{
	pc:	int;
	desc:	int;
	sig:	int;
	name:	string;
};

Mod: adt
{
	name:	string;

	magic:	int;
	rt:	int;
	ss:	int;
	isize:	int;
	dsize:	int;
	hsize:	int;
	lsize:	int;
	entry:	int;
	entryt:	int;

	inst:	array of ref Inst;
	types:	array of ref Type;
	data:	list of ref Data;
	links:	array of ref Link;

	sign:	array of byte;
};
m: Mod;
rt, ss: int;

rt_cfg := array[] of {
	"frame .m",
	"menubutton .m.open -text File -menu .file",
	"menubutton .m.prop -text Properties -menu .prop",
	"menubutton .m.view -text View -menu .view",
	"label .m.l",
	"pack .m.open .m.view .m.prop -side left",
	"pack .m.l -side right",
	"frame .b",
	"text .b.t -width 12c -height 7c -yscrollcommand {.b.s set} -bg white",
	"scrollbar .b.s -command {.b.t yview}",
	"pack .b.s -fill y -side left",
	"pack .b.t -fill both -expand 1",
	"pack .m -anchor w -fill x",
	"pack .b -fill both -expand 1",
	"pack propagate . 0",
	"update",

	"menu .prop",
	".prop add checkbutton -text {Must compile} -command {send cmd must}",
	".prop add checkbutton -text {Don't compile} -command {send cmd dont}",
	".prop add separator",
	".prop add command -text {Set stack extent} -command {send cmd stack}",
	".prop add command -text {Sign module} -command {send cmd sign}",

	"menu .view",
	".view add command -text {Header} -command {send cmd hdr}",
	".view add command -text {Code segment} -command {send cmd code}",
	".view add command -text {Data segment} -command {send cmd data}",
	".view add command -text {Type descriptors} -command {send cmd type}",
	".view add command -text {Link descriptors} -command {send cmd link}",

	"menu .file",
	".file add command -text {Open module} -command {send cmd open}",
	".file add separator",
	".file add command -text {Write .dis module} -command {send cmd save}",
	".file add command -text {Write .s file} -command {send cmd list}",
};

optab := array[] of {
	"nop",
	"alt",
	"nbalt",
	"goto",
	"call",
	"frame",
	"spawn",
	"runt",
	"load",
	"mcall",
	"mspawn",
	"mframe",
	"ret",
	"jmp",
	"case",
	"exit",
	"new",
	"newa",
	"newcb",
	"newcw",
	"newcf",
	"newcp",
	"newcm",
	"newcmp",
	"send",
	"recv",
	"consb",
	"consw",
	"consp",
	"consf",
	"consm",
	"consmp",
	"headb",
	"headw",
	"headp",
	"headf",
	"headm",
	"headmp",
	"tail",
	"lea",
	"indx",
	"movp",
	"movm",
	"movmp",
	"movb",
	"movw",
	"movf",
	"cvtbw",
	"cvtwb",
	"cvtfw",
	"cvtwf",
	"cvtca",
	"cvtac",
	"cvtwc",
	"cvtcw",
	"cvtfc",
	"cvtcf",
	"addb",
	"addw",
	"addf",
	"subb",
	"subw",
	"subf",
	"mulb",
	"mulw",
	"mulf",
	"divb",
	"divw",
	"divf",
	"modw",
	"modb",
	"andb",
	"andw",
	"orb",
	"orw",
	"xorb",
	"xorw",
	"shlb",
	"shlw",
	"shrb",
	"shrw",
	"insc",
	"indc",
	"addc",
	"lenc",
	"lena",
	"lenl",
	"beqb",
	"bneb",
	"bltb",
	"bleb",
	"bgtb",
	"bgeb",
	"beqw",
	"bnew",
	"bltw",
	"blew",
	"bgtw",
	"bgew",
	"beqf",
	"bnef",
	"bltf",
	"blef",
	"bgtf",
	"bgef",
	"beqc",
	"bnec",
	"bltc",
	"blec",
	"bgtc",
	"bgec",
	"slicea",
	"slicela",
	"slicec",
	"indw",
	"indf",
	"indb",
	"negf",
	"movl",
	"addl",
	"subl",
	"divl",
	"modl",
	"mull",
	"andl",
	"orl",
	"xorl",
	"shll",
	"shrl",
	"bnel",
	"bltl",
	"blel",
	"bgtl",
	"bgel",
	"beql",
	"cvtlf",
	"cvtfl",
	"cvtlw",
	"cvtwl",
	"cvtlc",
	"cvtcl",
	"headl",
	"consl",
	"newcl",
	"casec",
	"indl",
	"movpc",
	"tcmp",
	"mnewz",
	"cvtrf",
	"cvtfr",
	"cvtws",
	"cvtsw",
	"lsrw",
	"lsrl",
	"eclr",
	"newz",
	"newaz",
};

init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;

	wmlib->init();

	tkargs := "";
	argv = tl argv;
	if(argv != nil) {
		tkargs = hd argv;
		argv = tl argv;
	}

	menubut: chan of string;
	(t, menubut) = wmlib->titlebar(ctxt.screen, tkargs,
				"Dis Module Manager", Wmlib->Appl);

	cmd := chan of string;

	tk->namechan(t, cmd, "cmd");
	wmlib->tkcmds(t, rt_cfg);

	math = load Math Math->PATH;
	if(math == nil) {
		wmlib->dialog(t, "error -fg red", "Load Module",
				 "wmrt requires $Math",
				0, "Exit"::nil);
		return;
	}

	for(;;) alt {
	menu := <-menubut =>
		if(menu[0] == 'e')
			return;
		wmlib->titlectl(t, menu);
	s := <-cmd =>
		case s {
		"open" =>
			openfile(ctxt);
		"save" =>
			writedis();
		"list" =>
			writeasm();
		"hdr" =>
			hdr();
		"code" =>
			das(TK);
		"data" =>
			dat(TK);
		"type" =>
			desc(TK);
		"link" =>
			link(TK);
		"must" =>
			rt ^= MUSTCOMPILE;
		"dont" =>
			rt ^= DONTCOMPILE;
		"stack" =>
			spawn stack(ctxt);
		"sign" =>
			wmlib->dialog(t, "error -fg red", "Signed Modules",
				"not implemented",
				0, "Continue"::nil);
		}
	}
}

stack_cfg := array[] of {
	"scale .s -length 200 -to 32768 -resolution 128 -orient horizontal",
	"frame .f",
	"pack .s .f -pady 5 -fill x -expand 1",
	"update",
};

stack(ctxt: ref Draw->Context)
{
	(s, sbut) := wmlib->titlebar(ctxt.screen, wmlib->geom(t), "Dis Stack", 0);

	cmd := chan of string;
	tk->namechan(s, cmd, "cmd");
	wmlib->tkcmds(s, stack_cfg);
	
	for(;;) alt {
	wmctl := <-sbut =>
		if(wmctl[0] == 'e') {
			ss = int tk->cmd(s, ".s get");
			return;
		}
		wmlib->titlectl(s, wmctl);
	}	
}

openfile(ctxt: ref Draw->Context)
{
	pattern := list of {
		"*.dis (Dis VM module)",
		"* (All files)"
	};

	for(;;) {
		disfile = wmlib->filename(ctxt.screen, t, "Dis file", pattern, nil);
		if(disfile == "")
			break;

		s := loadobj();
		if(s == nil) {
			tk->cmd(t, ".m.l configure -text {"+m.name+"}");
			das(TK);
			return;
		}

		r := wmlib->dialog(t, "error -fg red", "Open Dis File",
				s,
				0, "Retry" :: "Abort" :: nil);
		if(r == 1)
			return;
	}
}

loadobj(): string
{
	fd := sys->open(disfile, sys->OREAD);
	if(fd == nil)
		return "open failed: "+sprint("%r");

	(ok, d) := sys->fstat(fd);
	if(ok < 0)
		return "stat failed: "+sprint("%r");

	disobj = array[d.length] of byte;

	if(sys->read(fd, disobj, d.length) != d.length)
		return "read failed: "+sprint("%r");

	disptr = 0;
	m.magic = operand();
	if(m.magic == SMAGIC) {
		n := operand();
		m.sign = disobj[disptr:disptr+n];
		disptr += n;
		m.magic = operand();
	}
	if(m.magic != XMAGIC)
		return "bad magic number";

	m.rt = operand();
	m.ss = operand();
	m.isize = operand();
	m.dsize = operand();
	m.hsize = operand();
	m.lsize = operand();
	m.entry = operand();
	m.entryt = operand();

	m.inst = array[m.isize] of ref Inst;
	for(i := 0; i < m.isize; i++) {
		o := ref Inst;
		o.op = int disobj[disptr++];
		o.addr = int disobj[disptr++];
		case o.addr & ARM {
		AXIMM or
		AXINF or
		AXINM =>
			o.mid = operand();
		}

		case (o.addr>>3) & 7 {
		AFP or
		AMP or
		AIMM =>
			o.src = operand();
		AIND|AFP or
		AIND|AMP =>
			o.src = operand()<<16;
			o.src |= operand();
		}

		case o.addr & 7	 {
		AFP or
		AMP or
		AIMM =>
			o.dst = operand();
		AIND|AFP or
		AIND|AMP =>
			o.dst = operand()<<16;
			o.dst |= operand();
		}
		m.inst[i] = o;
	}

	m.types = array[m.isize] of ref Type;
	for(i = 0; i < m.hsize; i++) {
		h := ref Type;
		id := operand();
		h.size = operand();
		h.np = operand();
		h.map = disobj[disptr:disptr+h.np];
		disptr += h.np;
		m.types[i] = h;
	}

	for(;;) {
		dat := ref Data;
		dat.op = int disobj[disptr++];
		if(dat.op == 0)
			break;

		n := dat.op & (DMAX-1);
		if(n == 0)
			n = operand();

		dat.n = n;
		dat.off = operand();

		case dat.op>>4 {
		DEFB =>
			dat.bytes = disobj[disptr:disptr+n];
			disptr += n;
		DEFW =>
			dat.words = array[n] of int;
			for(i = 0; i < n; i++)
				dat.words[i] = getw();
		DEFS =>
			dat.str = string disobj[disptr:disptr+n];
			disptr += n;
		DEFF =>
			dat.reals = array[n] of real;
			for(i = 0; i < n; i++)
				dat.reals[i] = math->bits64real(getl());
			break;
		DEFA =>
			dat.i1 = getw();
			dat.i2 = getw();
		DIND =>
			dat.i1 = getw();
		DAPOP =>
			break;
		DEFL =>
			dat.bigs = array[n] of big;
			for(i = 0; i < n; i++)
				dat.bigs[i] = getl();
		}

		m.data = dat :: m.data;
	}

	m.data = revdat(m.data);

	m.name = gets();

	m.links = array[m.lsize] of ref Link;
	for(i = 0; i < m.lsize; i++) {
		l := ref Link;
		l.pc = operand();
		l.desc = operand();
		l.sig = getw();
		l.name = gets();

		m.links[i] = l;
	}

	return nil;
}

writedis()
{
	if(m.magic == 0) {
		wmlib->dialog(t, "error -fg red", "Write .dis",
				"no module loaded",
				0, "Continue"::nil);
		return;
	}
	if(m.rt == rt && m.ss == ss)
		return;
	fd: ref Sys->FD;
	for(;;) {
		fd = sys->open(disfile, Sys->OWRITE);
		if(fd != nil)
			break;
		r := wmlib->dialog(t, "error -fg red", "Open Dis File",
			"open failed: "+sprint("%r"),
			0, "Retry" :: "Abort" :: nil);
		if(r == 0)
			continue;
		else
			return;
	}
	sys->seek(fd, 4, Sys->SEEKSTART);	# skip magic
	discon(fd, rt);
	discon(fd, ss);
	m.rt = rt;
	m.ss = ss;
}

discon(fd: ref Sys->FD, val: int)
{
	a: array of byte;
	if(val >= -64 && val <= 63)
		a = array[] of { byte(val & ~16r80) };
	else if(val >= -8192 && val <= 8191)
		a = array[] of { byte((val>>8) & ~16rC0 | 16r80), byte val };
	else
		a = array[] of { byte(val>>24 | 16rC0), byte(val>>16), byte(val>>8), byte val };
	sys->write(fd, a, len a);
}

fasm: ref Iobuf;

writeasm()
{
	if(m.magic == 0) {
		wmlib->dialog(t, "error -fg red", "Write .s",
				"no module loaded",
				0, "Continue"::nil);
		return;
	}

	bufio = load Bufio Bufio->PATH;
	if(bufio == nil) {
		wmlib->dialog(t, "error -fg red", "Write .s",
				"Bufio load failed: "+sprint("%r"),
				0, "Exit"::nil);
		return;
	}

	for(;;) {
		asmfile: string;
		if(len disfile > 4 && disfile[len disfile-4:] == ".dis")
			asmfile = disfile[0:len disfile-3] + "s";
		else
			asmfile = disfile + ".s";
		fasm = bufio->create(asmfile, Sys->OWRITE|Sys->OTRUNC, 8r666);
		if(fasm != nil)
			break;
		r := wmlib->dialog(t, "error -fg red", "Create .s file",
			"open failed: "+sprint("%r"),
			0, "Retry" :: "Abort" :: nil);
		if(r == 0)
			continue;
		else
			return;
	}
	das(!TK);
	fasm.puts("\tentry\t" + string m.entry + "," + string m.entryt + "\n");
	desc(!TK);
	dat(!TK);
	fasm.puts("\tmodule\t" + m.name + "\n");
	link(!TK);
	fasm.close();
}

link(flag: int)
{
	if(m.magic == 0) {
		wmlib->dialog(t, "error -fg red", "Link Descriptors",
				"no module loaded",
				0, "Continue"::nil);
		return;
	}

	if(flag == TK)
		tk->cmd(t, ".b.t delete 1.0 end");

	for(i := 0; i < m.lsize; i++) {
		l := m.links[i];
		s := sprint("	link %d,%d, 0x%ux, \"%s\"\n",
					l.desc, l.pc, l.sig, l.name);
		if(flag == TK)
			tk->cmd(t, ".b.t insert end '"+s);
		else
			fasm.puts(s);
	}
	if(flag == TK)
		tk->cmd(t, ".b.t see 1.0; update");
}

desc(flag: int)
{
	if(m.magic == 0) {
		wmlib->dialog(t, "error -fg red", "Type Descriptors",
				"no module loaded",
				0, "Continue"::nil);
		return;
	}

	if(flag == TK)
		tk->cmd(t, ".b.t delete 1.0 end");

	for(i := 0; i < m.hsize; i++) {
		h := m.types[i];
		s := sprint("	desc $%d, %d, \"", i, h.size);
		for(j := 0; j < h.np; j++)
			s += sprint("%.2ux", int h.map[j]);
		s += "\"\n";
		if(flag == TK)
			tk->cmd(t, ".b.t insert end '"+s);
		else
			fasm.puts(s);
	}
	if(flag == TK)
		tk->cmd(t, ".b.t see 1.0; update");
}

hdr()
{
	if(m.magic == 0) {
		wmlib->dialog(t, "error -fg red", "Header",
				"no module loaded",
				0, "Continue"::nil);
		return;
	}

	tk->cmd(t, ".b.t delete 1.0 end");

	s := sprint("%.8ux Version %d Dis VM\n", m.magic, m.magic - XMAGIC + 1);
	s += sprint("%.8ux Runtime flags %s\n", m.rt, rtflag(m.rt));
	s += sprint("%8d bytes per stack extent\n\n", m.ss);


	s += sprint("%8d instructions\n", m.isize);
	s += sprint("%8d data size\n", m.dsize);
	s += sprint("%8d heap type descriptors\n", m.hsize);
	s += sprint("%8d link directives\n", m.lsize);
	s += sprint("%8d entry pc\n", m.entry);
	s += sprint("%8d entry type descriptor\n\n", m.entryt);

	if(m.sign == nil)
		s += "Module is Insecure\n";

	tk->cmd(t, ".b.t insert end '"+s);
	tk->cmd(t, ".b.t see 1.0; update");
}

rtflag(flag: int): string
{
	if(flag == 0)
		return "";

	s := "[";

	if(flag & MUSTCOMPILE)
		s += "MustCompile";
	if(flag & DONTCOMPILE) {
		if(flag & MUSTCOMPILE)
			s += "|";
		s += "DontCompile";
	}
	s[len s] = ']';

	return s;
}

das(flag: int)
{
	if(m.magic == 0) {
		wmlib->dialog(t, "error -fg red", "Assembly",
				"no module loaded",
				0, "Continue"::nil);
		return;
	}

	if(flag == TK)
		tk->cmd(t, ".b.t delete 1.0 end");

	fi := 0;
	si := 0;
	for(i := 0; i < m.isize; i++) {
		o := m.inst[i];
		prefix := "";
		if(flag == TK)
			prefix = sprint(".b.t insert end '%4d   ", i);
		else {
			if(i % 10 == 0)
				fasm.puts("#" + string i + "\n");
			prefix = sprint("\t");
		}
		s := prefix + sprint("%-10s", optab[o.op]);
		src := "";
		dst := "";
		mid := "";
		case (o.addr>>3) & 7 {
		AFP =>
			src = sprint("%d(fp)", o.src);
		AMP =>
			src = sprint("%d(mp)", o.src);
		AIMM =>
			src = sprint("$%d", o.src);
		AIND|AFP =>
			fi = (o.src>>16) & 16rFFFF;
			si = o.src & 16rFFFF;
			src = sprint("%d(%d(fp))", si, fi);
		AIND|AMP =>
			fi = (o.src>>16) & 16rFFFF;
			si = o.src & 16rFFFF;
			src = sprint("%d(%d(fp))", si, fi);
		}

		case o.addr & ARM {
		AXIMM =>
			mid = sprint("$%d", o.mid);
		AXINF =>
			mid = sprint("%d(fp)", o.mid);
		AXINM =>
			mid = sprint("%d(mp)", o.mid);
		}

		case o.addr & 7 {
		AFP =>
			dst = sprint("%d(fp)", o.dst);
		AMP =>
			dst = sprint("%d(mp)", o.dst);
		AIMM =>
			dst = sprint("$%d", o.dst);
		AIND|AFP =>
			fi = (o.dst>>16) & 16rFFFF;
			si = o.dst & 16rFFFF;
			dst = sprint("%d(%d(fp))", si, fi);
		AIND|AMP =>
			fi = (o.dst>>16) & 16rFFFF;
			si = o.dst & 16rFFFF;
			dst = sprint("%d(%d(fp))", si, fi);
		}
		if(mid == "") {
			if(src == "")
				s += sprint("%s\n", dst);
			else
				s += sprint("%s, %s\n", src, dst);
		}
		else
			s += sprint("%s, %s, %s\n", src, mid, dst);

		if(flag == TK)
			tk->cmd(t, s);
		else
			fasm.puts(s);
	}
	if(flag == TK)
		tk->cmd(t, ".b.t see 1.0; update");
}

dat(flag: int)
{
	if(m.magic == 0) {
		wmlib->dialog(t, "error -fg red", "Module Data",
				"no module loaded",
				0, "Continue"::nil);
		return;
	}
	s := sprint("	var @mp, %d\n", m.types[0].size);
	if(flag == TK) {
		tk->cmd(t, ".b.t delete 1.0 end");
		tk->cmd(t, ".b.t insert end '"+s);
	} else
		fasm.puts(s);

	s = "";
	for(d := m.data; d != nil; d = tl d) {
		dat := hd d;
		case dat.op>>4 {
		DEFB =>
			s = sprint("\tbyte @mp+%d", dat.off);
			for(n := 0; n < dat.n; n++)
				s += sprint(",%d", int dat.bytes[n]);
		DEFW =>
			s = sprint("\tword @mp+%d", dat.off);
			for(n := 0; n < dat.n; n++)
				s += sprint(",%d", dat.words[n]);
		DEFS =>
			s = sprint("\tstring @mp+%d, \"%s\"", dat.off, mapstr(dat.str));
		DEFF =>
			s = sprint("\treal @mp+%d", dat.off);
			for(n := 0; n < dat.n; n++)
				s += sprint(", %g", dat.reals[n]);
			break;
		DEFA =>
			s = sprint("\tarray @mp+%d,$%d,%d", dat.off, dat.i1, dat.i2);
		DIND =>
			s = sprint("\tindir @mp+%d,%d", dat.off, dat.i1);
		DAPOP =>
			s = "\tapop";
			break;
		DEFL =>
			s = sprint("\tlong @mp+%d", dat.off);
			for(n := 0; n < dat.n; n++)
				s += sprint(", %bd", dat.bigs[n]);
		}
		if(flag == TK)
			tk->cmd(t, ".b.t insert end '"+s+"\n");
		else
			fasm.puts(s+"\n");
	}

	if(flag == TK)
		tk->cmd(t, ".b.t see 1.0; update");
}

mapstr(s: string): string
{
	for(i := 0; i < len s; i++) {
		if(s[i] == '\n')
			s = s[0:i] + "\\n" + s[i+1:];
	}
	return s;
}

operand(): int
{
	b := int disobj[disptr++];

	case b & 16rC0 {
	16r00 =>
		return b;
	16r40 =>
		return b | ~16r7F;
	16r80 =>
		if(b & 16r20)
			b |= ~16r3F;
		else
			b &= 16r3F;
		return (b<<8) | int disobj[disptr++];
	16rC0 =>
		if(b & 16r20)
			b |= ~16r3F;
		else
			b &= 16r3F;
		b = b<<24 |
			(int disobj[disptr]<<16) |
		    	(int disobj[disptr+1]<<8)|
		    	int disobj[disptr+2];
		disptr += 3;
		return b;
	}
	return 0;
}

getw(): int
{
	i := (int disobj[disptr+0]<<24) |
	     (int disobj[disptr+1]<<16) |
	     (int disobj[disptr+2]<<8) |
	      int disobj[disptr+3];

	disptr += 4;
	return i;
}

getl(): big
{
	i := (big disobj[disptr+0]<<56) |
	     (big disobj[disptr+1]<<48) |
	     (big disobj[disptr+2]<<40) |
	     (big disobj[disptr+3]<<32) |
	     (big disobj[disptr+4]<<24) |
	     (big disobj[disptr+5]<<16) |
	     (big disobj[disptr+6]<<8) |
	      big disobj[disptr+7];

	disptr += 8;
	return i;
}

gets(): string
{
	s := disptr;
	while(disobj[disptr] != byte 0)
		disptr++;

	v := string disobj[s:disptr];
	disptr++;
	return v;
}

revdat(d: list of ref Data): list of ref Data
{
	t: list of ref Data;

	while(d != nil) {
		t = hd d :: t;
		d = tl d;
	}
	return t;
}
