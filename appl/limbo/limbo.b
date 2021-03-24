implement Limbo;

#line	2	"limbo.y"
include "limbo.m";
include "draw.m";

Limbo: module {

	init:		fn(ctxt: ref Draw->Context, argv: list of string);

	YYSTYPE: adt{
		tok:	Tok;
		ids:	ref Decl;
		node:	ref Node;
		ty:	ref Type;
	};
Landeq: con	57346;
Loreq: con	57347;
Lxoreq: con	57348;
Llsheq: con	57349;
Lrsheq: con	57350;
Laddeq: con	57351;
Lsubeq: con	57352;
Lmuleq: con	57353;
Ldiveq: con	57354;
Lmodeq: con	57355;
Ldeclas: con	57356;
Lload: con	57357;
Loror: con	57358;
Landand: con	57359;
Lcons: con	57360;
Leq: con	57361;
Lneq: con	57362;
Lleq: con	57363;
Lgeq: con	57364;
Llsh: con	57365;
Lrsh: con	57366;
Lcomm: con	57367;
Linc: con	57368;
Ldec: con	57369;
Lof: con	57370;
Lref: con	57371;
Lif: con	57372;
Lelse: con	57373;
Lfn: con	57374;
Lmdot: con	57375;
Lto: con	57376;
Lor: con	57377;
Lrconst: con	57378;
Lconst: con	57379;
Lid: con	57380;
Ltid: con	57381;
Lsconst: con	57382;
Llabs: con	57383;
Lnil: con	57384;
Llen: con	57385;
Lhd: con	57386;
Ltl: con	57387;
Ltagof: con	57388;
Limplement: con	57389;
Limport: con	57390;
Linclude: con	57391;
Lcon: con	57392;
Ltype: con	57393;
Lmodule: con	57394;
Lcyclic: con	57395;
Ladt: con	57396;
Larray: con	57397;
Llist: con	57398;
Lchan: con	57399;
Lself: con	57400;
Ldo: con	57401;
Lwhile: con	57402;
Lfor: con	57403;
Lbreak: con	57404;
Lalt: con	57405;
Lcase: con	57406;
Lpick: con	57407;
Lcont: con	57408;
Lreturn: con	57409;
Lexit: con	57410;
Lspawn: con	57411;

};

#line	20	"limbo.y"
	#
	# lex.b
	#
	signdump:	string;			# name of function for sig debugging
	superwarn:	int;
	debug:		array of int;
	noline:		Line;
	nosrc:		Src;
	arrayz:		int;
	emitcode:	string;			# emit stub routines for system module functions
	emitsbl:	string;			# emit symbol file for sysm modules
	emitstub:	int;			# emit type and call frames for system modules
	emittab:	string;			# emit table of runtime functions for this module
	errors:		int;
	mustcompile:	int;
	dontcompile:	int;
	asmsym:		int;			# generate symbols in assembly language?
	bout:		ref Bufio->Iobuf;	# output file
	bsym:		ref Bufio->Iobuf;	# symbol output file; nil => no sym out
	gendis:		int;			# generate dis or asm?
	fixss:		int;

	#
	# decls.b
	#
	scope:		int;
	impmod:		ref Sym;		# name of implementation module
	nildecl:	ref Decl;		# declaration for limbo's nil

	#
	# types.b
	#
	tany:		ref Type;
	tbig:		ref Type;
	tbyte:		ref Type;
	terror:		ref Type;
	tint:		ref Type;
	tnone:		ref Type;
	treal:		ref Type;
	tstring:	ref Type;
	tunknown:	ref Type;
	descriptors:	ref Desc;		# list of all possible descriptors
	tattr:		array of Tattr;

	#
	# nodes.b
	#
	opcommute:	array of int;
	oprelinvert:	array of int;
	isused:		array of int;
	casttab:	array of array of int;	# instruction to cast from [1] to [2]

	nfns:		int;			# functions defined
	nfnexp:		int;
	fns:		array of ref Decl;	# decls for fns defined
	tree:		ref Node;		# root of parse tree

	parset:		int;			# time to parse
	checkt:		int;			# time to typecheck
	gent:		int;			# time to generate code
	writet:		int;			# time to write out code
	symt:		int;			# time to write out symbols
YYEOFCODE: con 1;
YYERRCODE: con 2;
YYMAXDEPTH: con 150;
yyval: YYSTYPE;

#line	1272	"limbo.y"


include "keyring.m";

sys:	Sys;
	print, fprint, sprint: import sys;

bufio:	Bufio;
	Iobuf: import bufio;

str:		String;

keyring:Keyring;
	md5: import keyring;

math:	Math;

yylval:		YYSTYPE;

canonnan: real;

debug	= array[256] of {* => 0};

noline	= -1;
nosrc	= Src(-1, -1);

# front end
include "arg.m";
include "lex.b";
include "types.b";
include "nodes.b";
include "decls.b";

include "typecheck.b";

# back end
include "gen.b";
include "ecom.b";
include "asm.b";
include "dis.b";
include "sbl.b";
include "stubs.b";
include "com.b";

init(nil: ref Draw->Context, argv: list of string)
{
	s: string;

	sys = load Sys Sys->PATH;
	keyring = load Keyring Keyring->PATH;

	bufio = load Bufio Bufio->PATH;
	if(bufio == nil){
		sys->print("can't load %s: %r\n", Bufio->PATH);
		exit;
	}
	str = load String String->PATH;
	if(str == nil){
		sys->print("can't load %s: %r\n", String->PATH);
		exit;
	}

	stderr = sys->fildes(2);

	# if we do not have the math library(floating point) ... skip the floating point
	# initialisation so that we can still compile non floating point limbo programs
	math = load Math Math->PATH;
	if (math != nil) {
		math->FPcontrol(0, Math->INVAL|Math->ZDIV|Math->OVFL|Math->UNFL|Math->INEX);

		na := array[1] of {0.};
		math->import_real(array[8] of {byte 16r7f, * => byte 16rff}, na);
		canonnan = na[0];
		if(!math->isnan(canonnan))
			fatal("bad canonical NaN");
	}

	lexinit();
	typeinit();
	optabinit();

	gendis = 1;
	asmsym = 0;
	maxerr = 20;
	ofile := "";
	ext := "";

	arg := Arg.init(argv);
	while(c := arg.opt()){
		case c{
		'A' =>
			emitsbl = arg.arg();
			if(emitsbl == nil)
				usage();
		'C' =>
			dontcompile = 1;
		'D' =>
			#
			# debug flags:
			#
			# a	alt compilation
			# A	array constructor compilation
			# b	boolean and branch compilation
			# c	case compilation
			# d	function declaration
			# D	descriptor generation
			# e	expression compilation
			# E	addressable expression compilation
			# f	print arguments for compiled functions
			# F	constant folding
			# g	print out globals
			# m	module declaration and type checking
			# n	nil references
			# s	print sizes of output file sections
			# S	type signing
			# t	type checking function bodies
			# T	timing
			# v	global var and constant compilation
			# x	adt verification
			# Y	tuple compilation
			# z Z	bug fixes
			#
			s = arg.arg();
			for(i := 0; i < len s; i++){
				c = s[i];
				if(c < len debug)
					debug[c] = 1;
			}
		'I' =>
			s = arg.arg();
			if(s == "")
				usage();
			addinclude(s);
		'G' =>
			asmsym = 1;
		'S' =>
			gendis = 0;
		'a' =>
			emitstub = 1;
		'c' =>
			mustcompile = 1;
		'e' =>
			maxerr = 1000;
		'f' =>
			fabort = 1;
		'g' =>
			dosym = 1;
		'o' =>
			ofile = arg.arg();
		's' =>
			s = arg.arg();
			if(s != nil)
				fixss = int s;
		't' =>
			emittab = arg.arg();
			if(emittab == nil)
				usage();
		'T' =>
			emitcode = arg.arg();
			if(emitcode == nil)
				usage();
		'w' =>
			superwarn = dowarn;
			dowarn = 1;
		'x' =>
			ext = arg.arg();
		'X' =>
			signdump = arg.arg();
		'z' =>
			arrayz = 1;
		* =>
			usage();
		}
	}

	addinclude("/module");

	argv = arg.argv;
	arg = nil;

	if(argv == nil){
		usage();
	}else if(ofile != nil){
		if(len argv != 1)
			usage();
		translate(hd argv, ofile, mkfileext(ofile, ".dis", ".sbl"));
	}else{
		pr := len argv != 1;
		if(ext == ""){
			ext = ".s";
			if(gendis)
				ext = ".dis";
		}
		for(; argv != nil; argv = tl argv){
			file := hd argv;
			(nil, s) = str->splitr(file, "/");
			if(pr)
				print("%s:\n", s);
			out := mkfileext(s, ".b", ext);
			translate(file, out, mkfileext(out, ".dis", ".sbl"));
		}
	}
}

usage()
{
	fprint(stderr, "usage: limbo [-GSagwe] [-I incdir] [-o outfile] [-{T|t} module] [-D debug] file ...\n");
	exit;
}

mkfileext(file, oldext, ext: string): string
{
	n := len file;
	n2 := len oldext;
	if(n >= n2 && file[n-n2:] == oldext)
		file = file[:n-n2];
	return file + ext;
}

translate(in, out, dbg: string)
{
	outfile = out;
	errors = 0;
	bins[0] = bufio->open(in, Bufio->OREAD);
	if(bins[0] == nil){
		fprint(stderr, "can't open %s: %r\n", in);
		toterrors++;
		return;
	}
	doemit := emitcode != "" || emitstub || emittab != "" || emitsbl != "";
	if(!doemit){
		bout = bufio->create(out, Bufio->OWRITE, 8r666);
		if(bout == nil){
			fprint(stderr, "can't open %s: %r\n", out);
			toterrors++;
			bins[0].close();
			return;
		}
		if(dosym){
			bsym = bufio->create(dbg, Bufio->OWRITE, 8r666);
			if(bsym == nil)
				fprint(stderr, "can't open %s: %r\n", dbg);
		}
	}

	lexstart(in);

	popscopes();
	typestart();
	declstart();
	nfnexp = 0;

	parset = sys->millisec();
	yyparse();
	parset = sys->millisec() - parset;

	checkt = sys->millisec();
	entry := typecheck(!doemit);
	checkt = sys->millisec() - checkt;

	modcom(entry);

	fns = nil;
	nfns = 0;
	descriptors = nil;

	if(debug['T'])
		print("times: parse=%d type=%d: gen=%d write=%d symbols=%d\n",
			parset, checkt, gent, writet, symt);

	if(bout != nil)
		bout.close();
	if(bsym != nil)
		bsym.close();
	toterrors += errors;
	if(errors && bout != nil)
		sys->remove(out);
	if(errors && bsym != nil)
		sys->remove(dbg);
}
yyexca := array[] of {-1, 1,
	1, -1,
	-2, 0,
-1, 3,
	1, 3,
	-2, 0,
-1, 16,
	37, 93,
	48, 55,
	50, 93,
	92, 55,
	-2, 210,
-1, 189,
	55, 26,
	67, 26,
	-2, 0,
-1, 190,
	55, 34,
	67, 34,
	87, 34,
	-2, 0,
-1, 191,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 203,
	1, 2,
	-2, 0,
-1, 284,
	48, 55,
	92, 55,
	-2, 210,
-1, 285,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 323,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 330,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 353,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 369,
	48, 98,
	92, 98,
	-2, 198,
-1, 377,
	67, 225,
	92, 225,
	-2, 127,
-1, 388,
	55, 40,
	67, 40,
	-2, 0,
-1, 397,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 410,
	67, 222,
	-2, 0,
-1, 429,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 433,
	67, 124,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 448,
	52, 52,
	58, 52,
	-2, 55,
-1, 452,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 459,
	68, 140,
	-2, 127,
-1, 477,
	67, 132,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 480,
	68, 139,
	81, 120,
	82, 120,
	83, 120,
	85, 120,
	86, 120,
	87, 120,
	-2, 0,
-1, 483,
	48, 55,
	52, 135,
	58, 135,
	92, 55,
	-2, 210,
};
YYNPROD: con 227;
YYPRIVATE: con 57344;
yytoknames: array of string;
yystates: array of string;
yydebug: con 0;
YYLAST:	con 2084;
yyact := array[] of {
  44, 283, 470, 269, 378, 191, 390, 376, 315, 374,
 167,  86, 274, 403,  90, 334,   8,   4, 289,  46,
 425,  21,  98, 369, 325, 255,  12,  43,  70,  71,
  72, 248, 199, 410,  29, 249, 200,   3, 270,  14,
 105,  73,  14,  64,  10, 348, 197,  10, 149, 150,
 151, 152, 153, 154, 155, 156, 157, 158, 159, 160,
 161,  11,  39, 100, 166, 268,   6, 168, 324,   6,
 249,  41, 400, 475, 249, 465,  30, 249,  30, 256,
  38, 322, 321, 320,  33, 249, 476, 185, 177, 188,
 180, 428,  82, 367, 106, 366, 184, 365, 102, 330,
 329, 328, 350, 332, 331, 333, 194, 339, 326, 193,
 192, 396, 204, 205, 206, 207, 208, 209, 210, 211,
 212, 213, 214,  83, 216, 217, 218, 219, 220, 221,
 222, 223, 224, 225, 226, 227, 228, 229, 230, 231,
 232, 233, 234, 235, 236, 168, 203,  14, 241, 312,
 202, 169,  10,  42, 238, 187, 176, 175, 312, 242,
  24, 313, 456, 174, 243, 240, 183,  23, 201,   5,
 308, 436,   5,  22,   6, 395, 251, 247, 176, 175,
 432, 318, 420, 409, 176, 175,  92, 176, 175, 261,
 254, 363, 352, 257, 258,  78, 347, 444, 246, 285,
 176, 175,  76, 441,  18,  81, 419,  18, 176, 175,
 288, 291, 190,  75,  74, 189, 245, 295, 292, 186,
 287,  21,  16, 463, 100,  16,  17, 173,  85,  17,
  87,  84,  88, 461,  89,  79,  80,  77,  13,   2,
 423,  13,  14, 176, 175,  18, 301,  10, 168, 479,
  91, 303, 176, 175, 293, 487, 473, 302, 294, 102,
 450,  31, 414,  40, 479, 304, 412,  17, 306,   6,
 478, 259, 467, 286, 389, 392, 448,  18, 323,  78,
 311,  96, 338, 370, 168,  18,  76, 472, 364,  81,
 412, 335, 337, 340, 336, 103, 434,  75,  74, 104,
 253, 344, 310, 103, 317, 252,  78, 104, 239, 471,
 142, 345, 343,  76, 383, 382,  81, 412,  89,  79,
  80,  77, 107, 413,  75,  74,  93, 392, 353, 357,
  20, 424,  36, 362, 360, 176, 175, 356, 144,  85,
 462,  87,  84, 452, 401,  34,  79,  80,  77, 377,
 351, 349, 319, 298, 198,  94,  32, 168, 372, 346,
 162, 381, 300, 386, 163, 406, 394, 182, 405, 361,
 181, 398, 399, 178, 165, 377, 164, 371, 480, 466,
 153,  78, 430, 408, 429, 416, 417, 342,  76, 296,
 196,  81, 195, 359, 415, 358, 355, 418, 426,  75,
  74, 427, 327,  36, 406,  18, 121, 405,  27, 435,
 433, 407, 377, 438, 406, 440, 431, 439, 437,  28,
  25,  79,  80,  77, 290, 215, 422, 266, 446, 449,
 445,  26, 264, 454, 250, 459, 361, 273, 457, 108,
 143, 453, 146, 455, 147, 148,   1, 421,  78, 316,
 388, 387, 145, 144, 469,  76, 464, 468,  81,  15,
 447, 265, 314, 271, 263, 309,  75,  74, 361, 123,
 124, 125, 121,   9, 386, 477, 404,  45, 474,   7,
 481, 375, 485, 384, 486, 153, 385,  37,  79,  80,
  77, 361,  47,  48,  51, 237, 391,  54, 282, 276,
  97,  95,  52,  53, 179,  58, 275, 101,  66,  99,
  19,  35,   0,   0,  69,  68, 284,  63,  67, 361,
  17,  49,  50,  57,  55,  56,  59, 272, 393, 271,
   0,   0,  13,   0,   0,   0,   0,  78,  60,  61,
  62,   0,   0,  45,  76, 277,   0,  81,   0, 278,
 279, 281, 280,   0,   0,  75,  74,   0,  47,  48,
  51,   0,   0,  54, 282,   0,   0,   0,  52,  53,
   0,  58, 275,   0,  66, 385,   0,  79,  80,  77,
  69,  68, 284,  63,  67,   0,  17,  49,  50,  57,
  55,  56,  59, 272, 354, 271,   0,   0,  13,   0,
   0,   0,   0,  78,  60,  61,  62,   0,   0,  45,
  76, 277,   0,  81,   0, 278, 279, 281, 280,   0,
   0,  75,  74,   0,  47,  48,  51,   0,   0,  54,
 282,   0, 373,   0,  52,  53,   0,  58, 275,   0,
  66,   0,   0,  79,  80,  77,  69,  68, 284,  63,
  67,   0,  17,  49,  50,  57,  55,  56,  59, 272,
 341, 271,   0,   0,  13,   0,   0,   0,   0,  78,
  60,  61,  62,   0,   0,  45,  76, 277,   0,  81,
   0, 278, 279, 281, 280,   0,   0,  75,  74,   0,
  47,  48,  51,   0,   0,  54, 282,   0,   0,   0,
  52,  53,   0,  58, 275,   0,  66,   0,   0,  79,
  80,  77,  69,  68, 284,  63,  67,   0,  17,  49,
  50,  57,  55,  56,  59, 272, 267, 482,   0,   0,
  13,   0,   0,   0,   0,   0,  60,  61,  62,   0,
   0,  45,   0, 277,   0,   0,   0, 278, 279, 281,
 280,   0,   0,   0,   0,   0,  47,  48, 484,   0,
   0,  54, 282,   0,   0,   0,  52,  53,   0,  58,
 275,   0,  66,   0,   0,   0,   0,   0,  69,  68,
 483,  63,  67,   0,  17,  49,  50,  57,  55,  56,
  59, 272, 458,   0,   0,   0,  13,   0,   0,   0,
   0,   0,  60,  61,  62,   0,  45,   0,   0, 277,
   0,   0,   0, 278, 279, 281, 280,   0,   0,   0,
   0,  47,  48, 379,   0,   0,  54, 282,   0,   0,
   0,  52,  53,   0,  58, 275,   0,  66,   0,   0,
   0,   0,   0,  69,  68, 284,  63,  67,   0,  17,
  49,  50,  57,  55,  56,  59, 272, 271,   0,   0,
   0,  13,   0,   0,   0,   0,   0,  60,  61,  62,
   0,  45,   0,   0, 277,   0,   0,   0, 278, 279,
 281, 280,   0,   0,   0,   0,  47,  48,  51,   0,
   0,  54, 282,   0,   0,   0,  52,  53,   0,  58,
 275,   0,  66,   0,   0,   0,   0,   0,  69,  68,
 284,  63,  67,   0,  17,  49,  50,  57,  55,  56,
  59, 272, 380, 129, 128, 126, 127, 123, 124, 125,
 121,   0,  60,  61,  62,   0,  45,   0,   0, 277,
   0,   0,   0, 278, 279, 281, 280,   0,   0,   0,
   0,  47,  48, 379,   0,   0,  54,  65,   0,   0,
   0,  52,  53,   0,  58,   0,   0,  66,   0,   0,
   0,   0,   0,  69,  68,  40,  63,  67,   0,  17,
  49,  50,  57,  55,  56,  59,  45, 126, 127, 123,
 124, 125, 121,   0,   0,   0,   0,  60,  61,  62,
   0,  47,  48,  51,   0,   0,  54,  65,   0,   0,
 244,  52,  53,   0,  58,   0,   0,  66,   0,   0,
   0,   0,   0,  69,  68,  40,  63,  67,   0,  17,
  49,  50,  57,  55,  56,  59,  45,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,  60,  61,  62,
   0,  47,  48,  51,   0,   0,  54,  65,   0,   0,
   0,  52,  53,   0,  58,   0,   0,  66,   0,   0,
   0,   0,   0,  69,  68,  40,  63,  67,   0,  17,
  49,  50,  57,  55,  56,  59,   0,   0,   0,   0,
  47,  48,  51,   0,   0,  54,  65,  60,  61,  62,
  52,  53,   0,  58,   0,   0,  66,   0,   0,   0,
   0,   0,  69,  68,  40,  63,  67,   0,  17,  49,
  50,  57,  55,  56,  59,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,  60,  61,  62, 110,
 111, 112, 113, 114, 115, 116, 117, 118, 119, 120,
 122,   0, 141, 140, 139, 138, 137, 136, 134, 135,
 130, 131, 132, 133, 129, 128, 126, 127, 123, 124,
 125, 121, 140, 139, 138, 137, 136, 134, 135, 130,
 131, 132, 133, 129, 128, 126, 127, 123, 124, 125,
 121, 110, 111, 112, 113, 114, 115, 116, 117, 118,
 119, 120, 122, 451, 141, 140, 139, 138, 137, 136,
 134, 135, 130, 131, 132, 133, 129, 128, 126, 127,
 123, 124, 125, 121, 139, 138, 137, 136, 134, 135,
 130, 131, 132, 133, 129, 128, 126, 127, 123, 124,
 125, 121,   0, 110, 111, 112, 113, 114, 115, 116,
 117, 118, 119, 120, 122, 443, 141, 140, 139, 138,
 137, 136, 134, 135, 130, 131, 132, 133, 129, 128,
 126, 127, 123, 124, 125, 121, 137, 136, 134, 135,
 130, 131, 132, 133, 129, 128, 126, 127, 123, 124,
 125, 121,   0,   0,   0, 110, 111, 112, 113, 114,
 115, 116, 117, 118, 119, 120, 122, 442, 141, 140,
 139, 138, 137, 136, 134, 135, 130, 131, 132, 133,
 129, 128, 126, 127, 123, 124, 125, 121, 136, 134,
 135, 130, 131, 132, 133, 129, 128, 126, 127, 123,
 124, 125, 121,   0,   0,   0,   0, 110, 111, 112,
 113, 114, 115, 116, 117, 118, 119, 120, 122, 368,
 141, 140, 139, 138, 137, 136, 134, 135, 130, 131,
 132, 133, 129, 128, 126, 127, 123, 124, 125, 121,
 134, 135, 130, 131, 132, 133, 129, 128, 126, 127,
 123, 124, 125, 121,   0,   0,   0,   0,   0, 110,
 111, 112, 113, 114, 115, 116, 117, 118, 119, 120,
 122, 307, 141, 140, 139, 138, 137, 136, 134, 135,
 130, 131, 132, 133, 129, 128, 126, 127, 123, 124,
 125, 121, 130, 131, 132, 133, 129, 128, 126, 127,
 123, 124, 125, 121,   0,   0,   0,   0,   0,   0,
   0, 110, 111, 112, 113, 114, 115, 116, 117, 118,
 119, 120, 122, 305, 141, 140, 139, 138, 137, 136,
 134, 135, 130, 131, 132, 133, 129, 128, 126, 127,
 123, 124, 125, 121,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 110, 111, 112, 113, 114, 115, 116,
 117, 118, 119, 120, 122, 262, 141, 140, 139, 138,
 137, 136, 134, 135, 130, 131, 132, 133, 129, 128,
 126, 127, 123, 124, 125, 121,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0, 110, 111, 112, 113, 114,
 115, 116, 117, 118, 119, 120, 122, 260, 141, 140,
 139, 138, 137, 136, 134, 135, 130, 131, 132, 133,
 129, 128, 126, 127, 123, 124, 125, 121,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0, 110, 111, 112,
 113, 114, 115, 116, 117, 118, 119, 120, 122, 172,
 141, 140, 139, 138, 137, 136, 134, 135, 130, 131,
 132, 133, 129, 128, 126, 127, 123, 124, 125, 121,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0, 110,
 111, 112, 113, 114, 115, 116, 117, 118, 119, 120,
 122, 171, 141, 140, 139, 138, 137, 136, 134, 135,
 130, 131, 132, 133, 129, 128, 126, 127, 123, 124,
 125, 121,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0, 110, 111, 112, 113, 114, 115, 116, 117, 118,
 119, 120, 122, 170, 141, 140, 139, 138, 137, 136,
 134, 135, 130, 131, 132, 133, 129, 128, 126, 127,
 123, 124, 125, 121,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 110, 111, 112, 113, 114, 115, 116,
 117, 118, 119, 120, 122, 109, 141, 140, 139, 138,
 137, 136, 134, 135, 130, 131, 132, 133, 129, 128,
 126, 127, 123, 124, 125, 121,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 110, 111, 112, 113, 114, 115, 116,
 117, 118, 119, 120, 122, 460, 141, 140, 139, 138,
 137, 136, 134, 135, 130, 131, 132, 133, 129, 128,
 126, 127, 123, 124, 125, 121,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 110, 111, 112, 113, 114, 115, 116,
 117, 118, 119, 120, 122, 402, 141, 140, 139, 138,
 137, 136, 134, 135, 130, 131, 132, 133, 129, 128,
 126, 127, 123, 124, 125, 121,   0,   0, 110, 111,
 112, 113, 114, 115, 116, 117, 118, 119, 120, 122,
 411, 141, 140, 139, 138, 137, 136, 134, 135, 130,
 131, 132, 133, 129, 128, 126, 127, 123, 124, 125,
 121,   0,   0,   0, 299, 110, 111, 112, 113, 114,
 115, 116, 117, 118, 119, 120, 122,   0, 141, 140,
 139, 138, 137, 136, 134, 135, 130, 131, 132, 133,
 129, 128, 126, 127, 123, 124, 125, 121,   0,   0,
   0, 297, 110, 111, 112, 113, 114, 115, 116, 117,
 118, 119, 120, 122,   0, 141, 140, 139, 138, 137,
 136, 134, 135, 130, 131, 132, 133, 129, 128, 126,
 127, 123, 124, 125, 121,   0, 397, 110, 111, 112,
 113, 114, 115, 116, 117, 118, 119, 120, 122,   0,
 141, 140, 139, 138, 137, 136, 134, 135, 130, 131,
 132, 133, 129, 128, 126, 127, 123, 124, 125, 121,
   0,  65,   0,   0,   0,   0,   0,   0,   0,   0,
   0,  66,   0,   0,   0,   0,   0,  69,  68,  40,
   0,  67,   0,  17, 141, 140, 139, 138, 137, 136,
 134, 135, 130, 131, 132, 133, 129, 128, 126, 127,
 123, 124, 125, 121,
};
yypact := array[] of {
 170,-1000, 275, 167,-1000, 105,-1000,-1000,  99,  92,
 416, 404, -14, 204, 308, 295,-1000,-1000, 208, -21,
  85,-1000,-1000,-1000,-1000,1020,1020,1020,1020, 632,
 368,  55, 158, 184, 271, 307, 248,   2,-1000,-1000,
-1000, 267,-1000,1707,-1000, 255, 403,1059,1059,1059,
1059,1059,1059,1059,1059,1059,1059,1059,1059,1059,
 321, 333, 331,1059,-1000,1020, 366,-1000,-1000,-1000,
1655,1603,1551, 159,-1000,-1000, 632, 330, 632, 327,
 324, 366,-1000,-1000, 632,1020, 151,1020, 149, 146,
-1000,-1000,  43,-1000, 632, 354, 352, -46,-1000, 306,
 -16, -56,-1000,-1000,-1000,-1000, 208,-1000, 167,-1000,
1020,1020,1020,1020,1020,1020,1020,1020,1020,1020,
1020, 421,1020,1020,1020,1020,1020,1020,1020,1020,
1020,1020,1020,1020,1020,1020,1020,1020,1020,1020,
1020,1020,1020,1020, 253,2004,1020,-1000,-1000,-1000,
-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,-1000,
-1000,-1000, 970, 160, 132, 632,-1000,  -7,2003,-1000,
-1000,-1000,-1000,-1000,1020, 250, 245, 286, 632, -13,
 286, 632, 632,-1000, 203,1499,-1000,1020,1447, 430,
 425, 659,-1000,-1000, 286,-1000,-1000, 240, 344, 344,
 199,-1000,-1000, 167,2003,2003,2003,2003,2003,2003,
2003,2003,2003,2003,2003,1020,2003, 370, 370, 370,
 436, 436, 956, 956, 894, 894, 894, 894,1407,1407,
1357,1306,1255,1205,1205,1154,2047, 351, -57,-1000,
 288,1931, 305,1894, 319,1059,1020, 286,-1000,1020,
 184,1395,-1000,-1000, 286,-1000, 632, 286, 286,-1000,
-1000,1343,-1000, 103,-1000,  94,-1000,-1000,-1000,-1000,
 304,  15,-1000, -24,  40, 365,  18, 236, 236,1020,
1020,  39,1020,2003,-1000, 593, 349,-1000, 286,-1000,
 257, 286,-1000,-1000,-1000,2003,-1000,-1000,1020, 316,
 130,-1000, -22,2003,-1000,-1000, 286,-1000,-1000,-1000,
 303,  34,-1000,-1000,-1000,-1000,-1000, 302, 126, 269,
-1000,-1000,-1000, 527, 359, 632,-1000,1020, 358, 356,
 855,1020, 125, 233,  29,-1000,  27,  25,1291,-1000,
 -15,-1000,-1000,-1000, 228, 337, 566, 920,-1000, 242,
-1000, 411, 272, 461,-1000,1020, 107,1968,1020,1020,
 -10, 296,1809, 920, 396,-1000,-1000,-1000,-1000,-1000,
-1000,-1000, 286, 920, 116, -59,-1000,1859, 265,1059,
-1000, 194, 632,1020,1020, 632, 138, 115, 424,-1000,
 182, 279,-1000,-1000, -18,-1000,1020, 855,  23, 346,
 345,-1000, 920, 113,-1000, 238,1859,1020, 104,-1000,
 920,1020, 920,1020,-1000, 135,1239,1187, 129,-1000,
-1000, 221, 220,-1000, 205,-1000,1135, 297,1020, 855,
1020,  95,-1000, 790,-1000,1759,-1000,-1000,2003,-1000,
2003,-1000,-1000,-1000,-1000,-1000, 175, 292,-1000, 165,
-1000,-1000, 855,   7,-1000, 341,-1000, 214,  15,1859,
 254,-1000, 500,-1000,-1000,1020,   5,-1000,  19,-1000,
 212,-1000,-1000,-1000, 340,-1000,-1000, 725,-1000, 254,
 855, 197,  15,-1000,1059,-1000,-1000,-1000,
};
yypgo := array[] of {
   0,  11, 511,  84,  18,  38, 510, 509, 507, 504,
 501, 500,  22, 499,  15,   6, 496,  12,   1,   0,
  19,  10, 495,  43,  26,  61, 487,   9, 481,   7,
   4,  65,  37,  17, 479,  14,   3,   5,  13, 476,
 473,  16, 465, 464, 462, 461, 459, 457, 454,   2,
 451, 450, 449,   8, 447, 446, 439, 437, 434,
};
yyr1 := array[] of {
   0,  56,  55,  55,  32,  32,  33,  33,  33,  33,
  33,  33,  33,  33,  33,  33,  33,  24,  24,  31,
  31,  31,  31,  31,  31,  40,  43,  43,  43,  42,
  42,  42,  42,  41,  45,  45,  45,  44,  44,  44,
  54,  54,  53,  53,  52,  50,  50,  50,  51,  51,
  51,  15,  16,  16,   5,   6,   6,   1,   1,   1,
   1,   1,   1,   1,   1,   1,   1,   9,   9,   2,
   2,   2,   3,   3,  10,  10,  11,  11,  12,  12,
  12,  12,   7,   8,   8,   8,   8,   4,   4,  34,
  35,  35,  35,  46,  46,  37,  37,  37,  57,  57,
  36,  36,  36,  36,  36,  36,  36,  36,  36,  36,
  36,  36,  36,  36,  36,  36,  36,  36,  36,  36,
  13,  13,  14,  14,  38,  39,  39,  30,  30,  30,
  30,  30,  47,  48,  48,  49,  49,  49,  49,  17,
  17,  18,  18,  18,  18,  18,  18,  18,  18,  18,
  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,
  18,  18,  18,  18,  18,  18,  18,  18,  18,  18,
  18,  18,  18,  18,  18,  19,  19,  19,  19,  19,
  19,  19,  19,  19,  19,  19,  19,  19,  19,  19,
  19,  19,  19,  19,  19,  19,  20,  20,  20,  58,
  20,  20,  20,  20,  20,  20,  20,  20,  20,  20,
  23,  23,  25,  26,  26,  26,  26,  22,  22,  21,
  21,  27,  27,  28,  28,  29,  29,
};
yyr2 := array[] of {
   0,   0,   5,   1,   1,   2,   2,   1,   1,   2,
   2,   4,   4,   4,   4,   4,   6,   1,   3,   3,
   5,   5,   4,   6,   5,   6,   0,   2,   1,   4,
   2,   5,   5,   6,   0,   2,   1,   1,   1,   5,
   0,   2,   5,   4,   4,   2,   2,   1,   2,   4,
   4,   1,   1,   3,   1,   1,   3,   1,   1,   3,
   3,   2,   3,   3,   3,   3,   2,   1,   3,   3,
   3,   5,   1,   3,   0,   1,   1,   3,   3,   3,
   3,   3,   1,   1,   1,   3,   3,   2,   3,   3,
   3,   2,   4,   1,   3,   0,   2,   2,   3,   5,
   2,   2,   4,   3,   4,   6,   2,   5,   7,  10,
   6,   8,   3,   3,   3,   3,   6,   5,   8,   2,
   0,   2,   0,   1,   2,   2,   4,   1,   3,   1,
   3,   1,   2,   2,   4,   1,   1,   3,   1,   0,
   1,   1,   3,   3,   3,   3,   3,   3,   3,   3,
   3,   3,   3,   4,   3,   3,   3,   3,   3,   3,
   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,
   3,   3,   3,   3,   3,   1,   2,   2,   2,   2,
   2,   2,   2,   2,   2,   2,   2,   2,   2,   6,
   8,   7,   5,   3,   4,   2,   1,   4,   3,   0,
   4,   3,   3,   4,   6,   2,   2,   1,   1,   1,
   1,   1,   3,   1,   1,   3,   3,   0,   1,   1,
   3,   1,   2,   1,   3,   1,   3,
};
yychk := array[] of {
-1000, -55,  69, -32, -33,   2, -31, -34, -41, -40,
 -23, -25, -24,  71,  -5, -46,  55,  59,  37,  -6,
  55, -33,  68,  68,  68,   4,  15,   4,  15,  48,
  92,  57,  48,  -3,  50,  -2,  37, -26, -25, -23,
  55,  92,  68, -18, -19,  16, -20,  31,  32,  60,
  61,  33,  41,  42,  36,  63,  64,  62,  44,  65,
  77,  78,  79,  56, -23,  37,  47,  57,  54,  53,
 -18, -18, -18,  -1,  56,  55,  44,  79,  37,  77,
  78,  47, -25,  68,  73,  70,  -1,  72,  74,  76,
 -35,  66,   2,  55,  48, -10,  33, -11, -12,  -7,
 -24,  -8, -25,  55,  59,  38,  92,  55, -56,  68,
   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,
  14,  36,  15,  33,  34,  35,  31,  32,  30,  29,
  25,  26,  27,  28,  23,  24,  22,  21,  20,  19,
  18,  17,  55,  37,  50,  49,  39,  41,  42, -19,
 -19, -19, -19, -19, -19, -19, -19, -19, -19, -19,
 -19, -19,  39,  43,  43,  43, -19, -21, -18,  -3,
  68,  68,  68,  68,   4,  50,  49,  -1,  43,  -9,
  -1,  43,  43,  -3,  -1, -18,  68,   4, -18,  66,
  66, -37,  67,  66,  -1,  38,  38,  92,  48,  48,
  92, -25, -23, -32, -18, -18, -18, -18, -18, -18,
 -18, -18, -18, -18, -18,   4, -18, -18, -18, -18,
 -18, -18, -18, -18, -18, -18, -18, -18, -18, -18,
 -18, -18, -18, -18, -18, -18, -18, -22, -21,  55,
 -20, -18, -17, -18,  40,  56,  66,  -1,  38,  92,
 -58, -18,  55,  55,  -1,  38,  92,  -1,  -1,  68,
  68, -18,  68, -43,   2, -45,   2,  67, -31, -36,
  -5,   2,  66, -57, -17,  45, -13,  84,  88,  89,
  91,  90,  37, -18,  55, -37,  33, -12,  -1,  -4,
  80,  -1,  -4,  55,  59, -18,  38,  40,  48,  40,
  43, -19, -21, -18, -35,  68,  -1,  68,  67, -42,
  -5, -41,  55,  67, -44, -53, -52,  -5,  87,  48,
  68,  67,  66, -37,  92,  48,  68,  37,  83,  82,
  81,  86,  85,  87, -14,  55, -14, -17, -18,  68,
 -21,  67,  38,  55,  44, -17,  43,  66,  67,  48,
  68,  48,  66, -37,  67,  37,  -1, -18,  37,  37,
 -36,  -5, -18,  66,  55,  68,  68,  68,  68,  38,
  55,  40,  -1,  66, -27, -28, -29, -18, -30,  33,
   2,  -1,  73,  72,  72,  75,  -1, -50, -51,   2,
 -15, -16,  55,  67, -21,  68,   4,  38, -17, -17,
  82,  48,  66, -38, -39, -30, -18,  15, -27,  67,
  92,  51,  52,  58,  68,  -1, -18, -18,  -1,  68,
  67, -54,   2,  58,  52,  38, -18, -36,  68,  38,
  37, -38,  67, -37,  58, -18,  67, -29, -18, -30,
 -18,  68,  68,  68,  68, -53, -15,  -5,  55, -15,
  55,  68,  46, -17, -36, -17,  67, -30,   2, -18,
  66,  58,  48,  58, -36,  68,  38,  58, -47, -48,
 -49,  55,  33,   2, -17,  68,  67, -37,  58,  52,
  38, -49,   2,  55,  33, -49, -36,  58,
};
yydef := array[] of {
   0,  -2,   0,  -2,   4,   0,   7,   8,   0,   0,
   0,  17,   0,   0,   0,   0,  -2, 211,   0,  54,
   0,   5,   6,   9,  10,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,  72,  74,   0, 213, 214,
 210,   0,   1,   0, 141,   0, 175,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0, 196,   0,   0, 207, 208, 209,
   0,   0,   0,   0,  57,  58,   0,   0,   0,   0,
   0,   0,  18,  19,   0,   0,   0,   0,   0,   0,
  89,  95,   0,  94,   0,   0,   0,  75,  76,   0,
   0,  82,  17,  83,  84, 212,   0,  56,   0,  11,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
   0,   0,   0, 217,   0,   0, 139, 205, 206, 176,
 177, 178, 179, 180, 181, 182, 183, 184, 185, 186,
 187, 188,   0,   0,   0,   0, 195,   0, 219, 199,
  13,  12,  14,  15,   0,   0,   0,  61,   0,   0,
  67,   0,   0,  66,   0,   0,  22,   0,   0,  -2,
  -2,  -2,  91,  95,  73,  69,  70,   0,   0,   0,
   0, 215, 216,  -2, 142, 143, 144, 145, 146, 147,
 148, 149, 150, 151, 152,   0, 154, 156, 157, 158,
 159, 160, 161, 162, 163, 164, 165, 166, 167, 168,
 169, 170, 171, 172, 173, 174, 155,   0, 218, 201,
 202, 140,   0,   0,   0,   0,   0, 193, 198,   0,
   0,   0,  59,  60,  62,  63,   0,  64,  65,  20,
  21,   0,  24,   0,  28,   0,  36,  90,  96,  97,
   0,   0,  95,   0,   0,   0,   0, 122, 122, 139,
   0,   0,   0, 140,  -2,  -2,   0,  77,  78,  79,
   0,  80,  81,  85,  86, 153, 197, 203, 139,   0,
   0, 194,   0, 220, 200,  16,  68,  23,  25,  27,
   0,   0,  55,  33,  35,  37,  38,   0,   0, 121,
 100, 101,  95,  -2,   0,   0, 106,   0,   0,   0,
  -2,   0,   0,   0,   0, 123,   0,   0,   0, 119,
   0,  92,  71,  87,   0,   0,   0,   0, 192,   0,
  30,   0,   0,  -2, 103,   0,   0,   0, 139, 139,
   0,   0,   0,   0,   0, 112, 113, 114, 115,  -2,
  88, 204, 189,   0,   0, 221, 223,  -2,   0, 129,
 131,   0,   0,   0,   0,   0,   0,   0,  -2,  47,
   0,  51,  52, 102,   0, 104,   0,  -2,   0,   0,
   0, 121,   0,   0,  95,   0, 127,   0,   0, 191,
  -2,   0,   0,   0,  29,   0,   0,   0,   0,  43,
  44,  45,  46,  48,   0,  99,   0, 107, 139,  -2,
 139,   0, 117,  -2, 125,   0, 190, 224, 128, 130,
 226,  31,  32,  39,  42,  41,   0,   0,  -2,   0,
  53, 105,  -2,   0, 110,   0, 116,   0, 131,  -2,
   0,  49,   0,  50, 108, 139,   0, 126,   0,  95,
   0, 135, 136, 138,   0, 111, 118,  -2, 133,   0,
  -2,   0, 138,  -2, 136, 137, 109, 134,
};
yytok1 := array[] of {
   1,   3,   3,   3,   3,   3,   3,   3,   3,   3,
   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,
   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,
   3,   3,   3,  60,   3,   3,   3,  35,  22,   3,
  37,  38,  33,  31,  92,  32,  50,  34,   3,   3,
   3,   3,   3,   3,   3,   3,   3,   3,  48,  68,
  25,   4,  26,   3,   3,   3,   3,   3,   3,   3,
   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,
   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,
   3,  39,   3,  40,  21,   3,   3,   3,   3,   3,
   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,
   3,   3,   3,   3,   3,   3,   3,   3,   3,   3,
   3,   3,   3,  66,  20,  67,  61,
};
yytok2 := array[] of {
   2,   3,   5,   6,   7,   8,   9,  10,  11,  12,
  13,  14,  15,  16,  17,  18,  19,  23,  24,  27,
  28,  29,  30,  36,  41,  42,  43,  44,  45,  46,
  47,  49,  51,  52,  53,  54,  55,  56,  57,  58,
  59,  62,  63,  64,  65,  69,  70,  71,  72,  73,
  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,
  84,  85,  86,  87,  88,  89,  90,  91,
};
yytok3 := array[] of {
   0
};

YYSys: module
{
	FD: adt
	{
		fd:	int;
	};
	fildes:		fn(fd: int): ref FD;
	fprint:		fn(fd: ref FD, s: string, *): int;
};

yysys: YYSys;
yystderr: ref YYSys->FD;

YYFLAG: con -1000;

# parser for yacc output
yynerrs := 0;		# number of errors
yyerrflag := 0;		# error recovery flag

yytokname(yyc: int): string
{
	if(yyc > 0 && yyc <= len yytoknames && yytoknames[yyc-1] != nil)
		return yytoknames[yyc-1];
	return "<"+string yyc+">";
}

yystatname(yys: int): string
{
	if(yys >= 0 && yys < len yystates && yystates[yys] != nil)
		return yystates[yys];
	return "<"+string yys+">\n";
}

yylex1(): int
{
	c : int;
	yychar := yylex();
	if(yychar <= 0)
		c = yytok1[0];
	else if(yychar < len yytok1)
		c = yytok1[yychar];
	else if(yychar >= YYPRIVATE && yychar < YYPRIVATE+len yytok2)
		c = yytok2[yychar-YYPRIVATE];
	else{
		n := len yytok3;
		c = 0;
		for(i := 0; i < n; i+=2) {
			if(yytok3[i+0] == yychar) {
				c = yytok3[i+1];
				break;
			}
		}
		if(c == 0)
			c = yytok2[1];	# unknown char
	}
	if(yydebug >= 3)
		yysys->fprint(yystderr, "lex %.4ux %s\n", yychar, yytokname(c));
	return c;
}

YYS: adt
{
	yyv: YYSTYPE;
	yys: int;
};

yyparse(): int
{
	if(yysys == nil) {
		yysys = load YYSys "$Sys";
		yystderr = yysys->fildes(2);
	}

	yys := array[YYMAXDEPTH] of YYS;

	save1 := yylval;
	save2 := yyval;
	save3 := yynerrs;
	save4 := yyerrflag;

	yystate := 0;
	yychar := -1;
	yynerrs = 0;
	yyerrflag = 0;
	yyp := -1;
	yyn := 0;

yystack:
	for(;;){
		# put a state and value onto the stack
		if(yydebug >= 4)
			yysys->fprint(yystderr, "char %s in %s", yytokname(yychar), yystatname(yystate));

		yyp++;
		if(yyp >= YYMAXDEPTH) {
			yyerror("yacc stack overflow");
			yyn = 1;
			break yystack;
		}
		yys[yyp].yys = yystate;
		yys[yyp].yyv = yyval;

		for(;;){
			yyn = yypact[yystate];
			if(yyn > YYFLAG) {	# simple state
				if(yychar < 0)
					yychar = yylex1();
				yyn += yychar;
				if(yyn >= 0 && yyn < YYLAST) {
					yyn = yyact[yyn];
					if(yychk[yyn] == yychar) { # valid shift
						yychar = -1;
						yyp++;
						if(yyp >= YYMAXDEPTH) {
							yyerror("yacc stack overflow");
							yyn = 1;
							break yystack;
						}
						yystate = yyn;
						yys[yyp].yys = yystate;
						yys[yyp].yyv = yylval;
						if(yyerrflag > 0)
							yyerrflag--;
						if(yydebug >= 4)
							yysys->fprint(yystderr, "char %s in %s", yytokname(yychar), yystatname(yystate));
						continue;
					}
				}
			}
		
			# default state action
			yyn = yydef[yystate];
			if(yyn == -2) {
				if(yychar < 0)
					yychar = yylex1();
		
				# look through exception table
				for(yyxi:=0;; yyxi+=2)
					if(yyexca[yyxi] == -1 && yyexca[yyxi+1] == yystate)
						break;
				for(yyxi += 2;; yyxi += 2) {
					yyn = yyexca[yyxi];
					if(yyn < 0 || yyn == yychar)
						break;
				}
				yyn = yyexca[yyxi+1];
				if(yyn < 0){
					yyn = 0;
					break yystack;
				}
			}

			if(yyn != 0)
				break;

			# error ... attempt to resume parsing
			if(yyerrflag == 0) { # brand new error
				yyerror("syntax error");
				yynerrs++;
				if(yydebug >= 1) {
					yysys->fprint(yystderr, "%s", yystatname(yystate));
					yysys->fprint(yystderr, "saw %s\n", yytokname(yychar));
				}
			}

			if(yyerrflag != 3) { # incompletely recovered error ... try again
				yyerrflag = 3;
	
				# find a state where "error" is a legal shift action
				while(yyp >= 0) {
					yyn = yypact[yys[yyp].yys] + YYERRCODE;
					if(yyn >= 0 && yyn < YYLAST) {
						yystate = yyact[yyn];  # simulate a shift of "error"
						if(yychk[yystate] == YYERRCODE)
							continue yystack;
					}
	
					# the current yyp has no shift onn "error", pop stack
					if(yydebug >= 2)
						yysys->fprint(yystderr, "error recovery pops state %d, uncovers %d\n",
							yys[yyp].yys, yys[yyp-1].yys );
					yyp--;
				}
				# there is no state on the stack with an error shift ... abort
				yyn = 1;
				break yystack;
			}

			# no shift yet; clobber input char
			if(yydebug >= 2)
				yysys->fprint(yystderr, "error recovery discards %s\n", yytokname(yychar));
			if(yychar == YYEOFCODE) {
				yyn = 1;
				break yystack;
			}
			yychar = -1;
			# try again in the same state
		}
	
		# reduction by production yyn
		if(yydebug >= 2)
			yysys->fprint(yystderr, "reduce %d in:\n\t%s", yyn, yystatname(yystate));
	
		yypt := yyp;
		yyp -= yyr2[yyn];
#		yyval = yys[yyp+1].yyv;
		yym := yyn;
	
		# consult goto table to find next state
		yyn = yyr1[yyn];
		yyg := yypgo[yyn];
		yyj := yyg + yys[yyp].yys + 1;
	
		if(yyj >= YYLAST || yychk[yystate=yyact[yyj]] != -yyn)
			yystate = yyact[yyg];
		case yym {
			
1=>
#line	133	"limbo.y"
{
		impmod = yys[yypt-1].yyv.tok.v.idval;
	}
2=>
#line	136	"limbo.y"
{
		tree = rotater(yys[yypt-0].yyv.node);
	}
3=>
#line	140	"limbo.y"
{
		impmod = nil;
		tree = rotater(yys[yypt-0].yyv.node);
	}
4=>
yyval.node = yys[yyp+1].yyv.node;
5=>
#line	148	"limbo.y"
{
		if(yys[yypt-1].yyv.node == nil)
			yyval.node = yys[yypt-0].yyv.node;
		else if(yys[yypt-0].yyv.node == nil)
			yyval.node = yys[yypt-1].yyv.node;
		else
			yyval.node = mkbin(Oseq, yys[yypt-1].yyv.node, yys[yypt-0].yyv.node);
	}
6=>
#line	159	"limbo.y"
{
		yyval.node = nil;
	}
7=>
yyval.node = yys[yyp+1].yyv.node;
8=>
yyval.node = yys[yyp+1].yyv.node;
9=>
yyval.node = yys[yyp+1].yyv.node;
10=>
yyval.node = yys[yyp+1].yyv.node;
11=>
#line	167	"limbo.y"
{
		yyval.node = mkbin(Oas, yys[yypt-3].yyv.node, yys[yypt-1].yyv.node);
	}
12=>
#line	171	"limbo.y"
{
		yyval.node = mkbin(Oas, yys[yypt-3].yyv.node, yys[yypt-1].yyv.node);
	}
13=>
#line	175	"limbo.y"
{
		yyval.node = mkbin(Odas, yys[yypt-3].yyv.node, yys[yypt-1].yyv.node);
	}
14=>
#line	179	"limbo.y"
{
		yyval.node = mkbin(Odas, yys[yypt-3].yyv.node, yys[yypt-1].yyv.node);
	}
15=>
#line	183	"limbo.y"
{
		yyerror("illegal declaration");
		yyval.node = nil;
	}
16=>
#line	188	"limbo.y"
{
		yyerror("illegal declaration");
		yyval.node = nil;
	}
17=>
yyval.node = yys[yyp+1].yyv.node;
18=>
#line	196	"limbo.y"
{
		yyval.node = mkbin(Oseq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
19=>
#line	202	"limbo.y"
{
		includef(yys[yypt-1].yyv.tok.v.idval);
		yyval.node = nil;
	}
20=>
#line	207	"limbo.y"
{
		yyval.node = typedecl(yys[yypt-4].yyv.ids, yys[yypt-1].yyv.ty);
	}
21=>
#line	211	"limbo.y"
{
		yyval.node = importdecl(yys[yypt-1].yyv.node, yys[yypt-4].yyv.ids);
		yyval.node.src.start = yys[yypt-4].yyv.ids.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
22=>
#line	217	"limbo.y"
{
		yyval.node = vardecl(yys[yypt-3].yyv.ids, yys[yypt-1].yyv.ty);
	}
23=>
#line	221	"limbo.y"
{
		yyval.node = mkbin(Ovardecli, vardecl(yys[yypt-5].yyv.ids, yys[yypt-3].yyv.ty), varinit(yys[yypt-5].yyv.ids, yys[yypt-1].yyv.node));
	}
24=>
#line	225	"limbo.y"
{
		yyval.node = condecl(yys[yypt-4].yyv.ids, yys[yypt-1].yyv.node);
	}
25=>
#line	231	"limbo.y"
{
		yys[yypt-5].yyv.ids.src.stop = yys[yypt-0].yyv.tok.src.stop;
		yyval.node = moddecl(yys[yypt-5].yyv.ids, rotater(yys[yypt-1].yyv.node));
	}
26=>
#line	238	"limbo.y"
{
		yyval.node = nil;
	}
27=>
#line	242	"limbo.y"
{
		if(yys[yypt-1].yyv.node == nil)
			yyval.node = yys[yypt-0].yyv.node;
		else if(yys[yypt-0].yyv.node == nil)
			yyval.node = yys[yypt-1].yyv.node;
		else
			yyval.node = mkn(Oseq, yys[yypt-1].yyv.node, yys[yypt-0].yyv.node);
	}
28=>
#line	251	"limbo.y"
{
		yyval.node = nil;
	}
29=>
#line	257	"limbo.y"
{
		yyval.node = fielddecl(Dglobal, typeids(yys[yypt-3].yyv.ids, yys[yypt-1].yyv.ty));
	}
30=>
yyval.node = yys[yyp+1].yyv.node;
31=>
#line	262	"limbo.y"
{
		yyval.node = typedecl(yys[yypt-4].yyv.ids, yys[yypt-1].yyv.ty);
	}
32=>
#line	266	"limbo.y"
{
		yyval.node = condecl(yys[yypt-4].yyv.ids, yys[yypt-1].yyv.node);
	}
33=>
#line	272	"limbo.y"
{
		yys[yypt-5].yyv.ids.src.stop = yys[yypt-0].yyv.tok.src.stop;
		yyval.node = adtdecl(yys[yypt-5].yyv.ids, rotater(yys[yypt-1].yyv.node));
	}
34=>
#line	279	"limbo.y"
{
		yyval.node = nil;
	}
35=>
#line	283	"limbo.y"
{
		if(yys[yypt-1].yyv.node == nil)
			yyval.node = yys[yypt-0].yyv.node;
		else if(yys[yypt-0].yyv.node == nil)
			yyval.node = yys[yypt-1].yyv.node;
		else
			yyval.node = mkn(Oseq, yys[yypt-1].yyv.node, yys[yypt-0].yyv.node);
	}
36=>
#line	292	"limbo.y"
{
		yyval.node = nil;
	}
37=>
yyval.node = yys[yyp+1].yyv.node;
38=>
yyval.node = yys[yyp+1].yyv.node;
39=>
#line	300	"limbo.y"
{
		yyval.node = condecl(yys[yypt-4].yyv.ids, yys[yypt-1].yyv.node);
	}
40=>
#line	306	"limbo.y"
{
		yyval.node = nil;
	}
41=>
#line	310	"limbo.y"
{
		if(yys[yypt-1].yyv.node == nil)
			yyval.node = yys[yypt-0].yyv.node;
		else if(yys[yypt-0].yyv.node == nil)
			yyval.node = yys[yypt-1].yyv.node;
		else
			yyval.node = mkn(Oseq, yys[yypt-1].yyv.node, yys[yypt-0].yyv.node);
	}
42=>
#line	321	"limbo.y"
{
		for(d := yys[yypt-4].yyv.ids; d != nil; d = d.next)
			d.cyc = byte 1;
		yyval.node = fielddecl(Dfield, typeids(yys[yypt-4].yyv.ids, yys[yypt-1].yyv.ty));
	}
43=>
#line	327	"limbo.y"
{
		yyval.node = fielddecl(Dfield, typeids(yys[yypt-3].yyv.ids, yys[yypt-1].yyv.ty));
	}
44=>
#line	333	"limbo.y"
{
		yyval.node = yys[yypt-1].yyv.node;
	}
45=>
#line	339	"limbo.y"
{
		yys[yypt-1].yyv.node.right.right = yys[yypt-0].yyv.node;
		yyval.node = yys[yypt-1].yyv.node;
	}
46=>
#line	344	"limbo.y"
{
		yyval.node = nil;
	}
47=>
#line	348	"limbo.y"
{
		yyval.node = nil;
	}
48=>
#line	354	"limbo.y"
{
		yyval.node = mkn(Opickdecl, nil, mkn(Oseq, fielddecl(Dtag, yys[yypt-1].yyv.ids), nil));
		typeids(yys[yypt-1].yyv.ids, mktype(yys[yypt-1].yyv.ids.src.start, yys[yypt-1].yyv.ids.src.stop, Tadtpick, nil, nil));
	}
49=>
#line	359	"limbo.y"
{
		yys[yypt-3].yyv.node.right.right = yys[yypt-2].yyv.node;
		yyval.node = mkn(Opickdecl, yys[yypt-3].yyv.node, mkn(Oseq, fielddecl(Dtag, yys[yypt-1].yyv.ids), nil));
		typeids(yys[yypt-1].yyv.ids, mktype(yys[yypt-1].yyv.ids.src.start, yys[yypt-1].yyv.ids.src.stop, Tadtpick, nil, nil));
	}
50=>
#line	365	"limbo.y"
{
		yyval.node = mkn(Opickdecl, nil, mkn(Oseq, fielddecl(Dtag, yys[yypt-1].yyv.ids), nil));
		typeids(yys[yypt-1].yyv.ids, mktype(yys[yypt-1].yyv.ids.src.start, yys[yypt-1].yyv.ids.src.stop, Tadtpick, nil, nil));
	}
51=>
#line	372	"limbo.y"
{
		yyval.ids = revids(yys[yypt-0].yyv.ids);
	}
52=>
#line	378	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval, nil, nil);
	}
53=>
#line	382	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval, nil, yys[yypt-2].yyv.ids);
	}
54=>
#line	388	"limbo.y"
{
		yyval.ids = revids(yys[yypt-0].yyv.ids);
	}
55=>
#line	394	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval, nil, nil);
	}
56=>
#line	398	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval, nil, yys[yypt-2].yyv.ids);
	}
57=>
#line	404	"limbo.y"
{
		yyval.ty = mkidtype(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval);
	}
58=>
#line	408	"limbo.y"
{
		yyval.ty = mkidtype(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval);
	}
59=>
#line	412	"limbo.y"
{
		yyval.ty = mkdottype(yys[yypt-2].yyv.ty.src.start, yys[yypt-0].yyv.tok.src.stop, yys[yypt-2].yyv.ty, yys[yypt-0].yyv.tok.v.idval);
	}
60=>
#line	416	"limbo.y"
{
		yyval.ty = mkarrowtype(yys[yypt-2].yyv.ty.src.start, yys[yypt-0].yyv.tok.src.stop, yys[yypt-2].yyv.ty, yys[yypt-0].yyv.tok.v.idval);
	}
61=>
#line	420	"limbo.y"
{
		yyval.ty = mktype(yys[yypt-1].yyv.tok.src.start, yys[yypt-0].yyv.ty.src.stop, Tref, yys[yypt-0].yyv.ty, nil);
	}
62=>
#line	424	"limbo.y"
{
		yyval.ty = mktype(yys[yypt-2].yyv.tok.src.start, yys[yypt-0].yyv.ty.src.stop, Tchan, yys[yypt-0].yyv.ty, nil);
	}
63=>
#line	428	"limbo.y"
{
		if(yys[yypt-1].yyv.ids.next == nil)
			yyval.ty = yys[yypt-1].yyv.ids.ty;
		else
			yyval.ty = mktype(yys[yypt-2].yyv.tok.src.start, yys[yypt-0].yyv.tok.src.stop, Ttuple, nil, revids(yys[yypt-1].yyv.ids));
	}
64=>
#line	435	"limbo.y"
{
		yyval.ty = mktype(yys[yypt-2].yyv.tok.src.start, yys[yypt-0].yyv.ty.src.stop, Tarray, yys[yypt-0].yyv.ty, nil);
	}
65=>
#line	439	"limbo.y"
{
		yyval.ty = mktype(yys[yypt-2].yyv.tok.src.start, yys[yypt-0].yyv.ty.src.stop, Tlist, yys[yypt-0].yyv.ty, nil);
	}
66=>
#line	443	"limbo.y"
{
		yys[yypt-0].yyv.ty.src.start = yys[yypt-1].yyv.tok.src.start;
		yyval.ty = yys[yypt-0].yyv.ty;
	}
67=>
#line	450	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-0].yyv.ty.src, nil, yys[yypt-0].yyv.ty, nil);
	}
68=>
#line	454	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-2].yyv.ids.src, nil, yys[yypt-0].yyv.ty, yys[yypt-2].yyv.ids);
	}
69=>
#line	460	"limbo.y"
{
		yyval.ty = mktype(yys[yypt-2].yyv.tok.src.start, yys[yypt-0].yyv.tok.src.stop, Tfn, tnone, yys[yypt-1].yyv.ids);
	}
70=>
#line	464	"limbo.y"
{
		yyval.ty = mktype(yys[yypt-2].yyv.tok.src.start, yys[yypt-0].yyv.tok.src.stop, Tfn, tnone, nil);
		yyval.ty.varargs = byte 1;
	}
71=>
#line	469	"limbo.y"
{
		yyval.ty = mktype(yys[yypt-4].yyv.tok.src.start, yys[yypt-0].yyv.tok.src.stop, Tfn, tnone, yys[yypt-3].yyv.ids);
		yyval.ty.varargs = byte 1;
	}
72=>
yyval.ty = yys[yyp+1].yyv.ty;
73=>
#line	477	"limbo.y"
{
		yys[yypt-2].yyv.ty.tof = yys[yypt-0].yyv.ty;
		yys[yypt-2].yyv.ty.src.stop = yys[yypt-0].yyv.ty.src.stop;
		yyval.ty = yys[yypt-2].yyv.ty;
	}
74=>
#line	485	"limbo.y"
{
		yyval.ids = nil;
	}
75=>
yyval.ids = yys[yyp+1].yyv.ids;
76=>
yyval.ids = yys[yyp+1].yyv.ids;
77=>
#line	493	"limbo.y"
{
		yyval.ids = appdecls(yys[yypt-2].yyv.ids, yys[yypt-0].yyv.ids);
	}
78=>
#line	499	"limbo.y"
{
		yyval.ids = typeids(yys[yypt-2].yyv.ids, yys[yypt-0].yyv.ty);
	}
79=>
#line	503	"limbo.y"
{
		yyval.ids = typeids(yys[yypt-2].yyv.ids, yys[yypt-0].yyv.ty);
		for(d := yyval.ids; d != nil; d = d.next)
			d.implicit = byte 1;
	}
80=>
#line	509	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-2].yyv.node.src, enter("junk", 0), yys[yypt-0].yyv.ty, nil);
		yyval.ids.store = Darg;
		yyerror("illegal argument declaraion");
	}
81=>
#line	515	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-2].yyv.node.src, enter("junk", 0), yys[yypt-0].yyv.ty, nil);
		yyval.ids.store = Darg;
		yyerror("illegal argument declaraion");
	}
82=>
#line	523	"limbo.y"
{
		yyval.ids = revids(yys[yypt-0].yyv.ids);
	}
83=>
#line	529	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval, nil, nil);
		yyval.ids.store = Darg;
	}
84=>
#line	534	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-0].yyv.tok.src, nil, nil, nil);
		yyval.ids.store = Darg;
	}
85=>
#line	539	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval, nil, yys[yypt-2].yyv.ids);
		yyval.ids.store = Darg;
	}
86=>
#line	544	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-0].yyv.tok.src, nil, nil, yys[yypt-2].yyv.ids);
		yyval.ids.store = Darg;
	}
87=>
#line	551	"limbo.y"
{
		yyval.ty = mkidtype(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval);
	}
88=>
#line	555	"limbo.y"
{
		yyval.ty = mktype(yys[yypt-1].yyv.tok.src.start, yys[yypt-0].yyv.tok.src.stop, Tref, mkidtype(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval), nil);
	}
89=>
#line	561	"limbo.y"
{
		yyval.node = fndecl(yys[yypt-2].yyv.node, yys[yypt-1].yyv.ty, yys[yypt-0].yyv.node);
		nfns++;
		yyval.node.src = yys[yypt-2].yyv.node.src;
	}
90=>
#line	569	"limbo.y"
{
		if(yys[yypt-1].yyv.node == nil){
			yys[yypt-1].yyv.node = mkn(Onothing, nil, nil);
			yys[yypt-1].yyv.node.src.start = curline();
			yys[yypt-1].yyv.node.src.stop = yys[yypt-1].yyv.node.src.start;
		}
		yyval.node = rotater(yys[yypt-1].yyv.node);
		yyval.node.src.start = yys[yypt-2].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
91=>
#line	580	"limbo.y"
{
		yyval.node = mkn(Onothing, nil, nil);
	}
92=>
#line	584	"limbo.y"
{
		yyval.node = mkn(Onothing, nil, nil);
	}
93=>
#line	590	"limbo.y"
{
		yyval.node = mkname(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval);
	}
94=>
#line	594	"limbo.y"
{
		yyval.node = mkbin(Odot, yys[yypt-2].yyv.node, mkname(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval));
	}
95=>
#line	600	"limbo.y"
{
		yyval.node = nil;
	}
96=>
#line	604	"limbo.y"
{
		if(yys[yypt-1].yyv.node == nil)
			yyval.node = yys[yypt-0].yyv.node;
		else if(yys[yypt-0].yyv.node == nil)
			yyval.node = yys[yypt-1].yyv.node;
		else
			yyval.node = mkbin(Oseq, yys[yypt-1].yyv.node, yys[yypt-0].yyv.node);
	}
97=>
#line	613	"limbo.y"
{
		if(yys[yypt-1].yyv.node == nil)
			yyval.node = yys[yypt-0].yyv.node;
		else
			yyval.node = mkbin(Oseq, yys[yypt-1].yyv.node, yys[yypt-0].yyv.node);
	}
100=>
#line	626	"limbo.y"
{
		yyval.node = mkn(Onothing, nil, nil);
		yyval.node.src.start = curline();
		yyval.node.src.stop = yyval.node.src.start;
	}
101=>
#line	632	"limbo.y"
{
		yyval.node = mkn(Onothing, nil, nil);
		yyval.node.src.start = curline();
		yyval.node.src.stop = yyval.node.src.start;
	}
102=>
#line	638	"limbo.y"
{
		yyval.node = mkn(Onothing, nil, nil);
		yyval.node.src.start = curline();
		yyval.node.src.stop = yyval.node.src.start;
	}
103=>
#line	644	"limbo.y"
{
		if(yys[yypt-1].yyv.node == nil){
			yys[yypt-1].yyv.node = mkn(Onothing, nil, nil);
			yys[yypt-1].yyv.node.src.start = curline();
			yys[yypt-1].yyv.node.src.stop = yys[yypt-1].yyv.node.src.start;
		}
		yyval.node = mkscope(rotater(yys[yypt-1].yyv.node));
	}
104=>
#line	653	"limbo.y"
{
		yyerror("illegal declaration");
		yyval.node = mkn(Onothing, nil, nil);
		yyval.node.src.start = curline();
		yyval.node.src.stop = yyval.node.src.start;
	}
105=>
#line	660	"limbo.y"
{
		yyerror("illegal declaration");
		yyval.node = mkn(Onothing, nil, nil);
		yyval.node.src.start = curline();
		yyval.node.src.stop = yyval.node.src.start;
	}
106=>
#line	667	"limbo.y"
{
		yyval.node = yys[yypt-1].yyv.node;
	}
107=>
#line	671	"limbo.y"
{
		yyval.node = mkn(Oif, yys[yypt-2].yyv.node, mkunary(Oseq, yys[yypt-0].yyv.node));
		yyval.node.src.start = yys[yypt-4].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.node.src.stop;
	}
108=>
#line	677	"limbo.y"
{
		yyval.node = mkn(Oif, yys[yypt-4].yyv.node, mkbin(Oseq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node));
		yyval.node.src.start = yys[yypt-6].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.node.src.stop;
	}
109=>
#line	683	"limbo.y"
{
		yyval.node = mkunary(Oseq, yys[yypt-0].yyv.node);
		if(yys[yypt-2].yyv.node.op != Onothing)
			yyval.node.right = yys[yypt-2].yyv.node;
		yyval.node = mkbin(Ofor, yys[yypt-4].yyv.node, yyval.node);
		yyval.node.decl = yys[yypt-9].yyv.ids;
		if(yys[yypt-6].yyv.node.op != Onothing)
			yyval.node = mkbin(Oseq, yys[yypt-6].yyv.node, yyval.node);
	}
110=>
#line	693	"limbo.y"
{
		yyval.node = mkn(Ofor, yys[yypt-2].yyv.node, mkunary(Oseq, yys[yypt-0].yyv.node));
		yyval.node.src.start = yys[yypt-4].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.node.src.stop;
		yyval.node.decl = yys[yypt-5].yyv.ids;
	}
111=>
#line	700	"limbo.y"
{
		yyval.node = mkn(Odo, yys[yypt-2].yyv.node, yys[yypt-5].yyv.node);
		yyval.node.src.start = yys[yypt-6].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-1].yyv.tok.src.stop;
		yyval.node.decl = yys[yypt-7].yyv.ids;
	}
112=>
#line	707	"limbo.y"
{
		yyval.node = mkn(Obreak, nil, nil);
		yyval.node.decl = yys[yypt-1].yyv.ids;
		yyval.node.src = yys[yypt-2].yyv.tok.src;
	}
113=>
#line	713	"limbo.y"
{
		yyval.node = mkn(Ocont, nil, nil);
		yyval.node.decl = yys[yypt-1].yyv.ids;
		yyval.node.src = yys[yypt-2].yyv.tok.src;
	}
114=>
#line	719	"limbo.y"
{
		yyval.node = mkn(Oret, yys[yypt-1].yyv.node, nil);
		yyval.node.src = yys[yypt-2].yyv.tok.src;
		if(yys[yypt-1].yyv.node.op == Onothing)
			yyval.node.left = nil;
		else
			yyval.node.src.stop = yys[yypt-1].yyv.node.src.stop;
	}
115=>
#line	728	"limbo.y"
{
		yyval.node = mkn(Ospawn, yys[yypt-1].yyv.node, nil);
		yyval.node.src.start = yys[yypt-2].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-1].yyv.node.src.stop;
	}
116=>
#line	734	"limbo.y"
{
		yyval.node = mkn(Ocase, yys[yypt-3].yyv.node, caselist(yys[yypt-1].yyv.node, nil));
		yyval.node.src = yys[yypt-3].yyv.node.src;
		yyval.node.decl = yys[yypt-5].yyv.ids;
	}
117=>
#line	740	"limbo.y"
{
		yyval.node = mkn(Oalt, caselist(yys[yypt-1].yyv.node, nil), nil);
		yyval.node.src = yys[yypt-3].yyv.tok.src;
		yyval.node.decl = yys[yypt-4].yyv.ids;
	}
118=>
#line	746	"limbo.y"
{
		yyval.node = mkn(Opick, mkbin(Odas, mkname(yys[yypt-5].yyv.tok.src, yys[yypt-5].yyv.tok.v.idval), yys[yypt-3].yyv.node), caselist(yys[yypt-1].yyv.node, nil));
		yyval.node.src.start = yys[yypt-5].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-3].yyv.node.src.stop;
		yyval.node.decl = yys[yypt-7].yyv.ids;
	}
119=>
#line	753	"limbo.y"
{
		yyval.node = mkn(Oexit, nil, nil);
		yyval.node.src = yys[yypt-1].yyv.tok.src;
	}
120=>
#line	760	"limbo.y"
{
		yyval.ids = nil;
	}
121=>
#line	764	"limbo.y"
{
		if(yys[yypt-1].yyv.ids.next != nil)
			yyerror("only one identifier allowed in a label");
		yyval.ids = yys[yypt-1].yyv.ids;
	}
122=>
#line	772	"limbo.y"
{
		yyval.ids = nil;
	}
123=>
#line	776	"limbo.y"
{
		yyval.ids = mkids(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval, nil, nil);
	}
124=>
#line	782	"limbo.y"
{
		yys[yypt-1].yyv.node.left.right = yys[yypt-0].yyv.node;
		yyval.node = yys[yypt-1].yyv.node;
	}
125=>
#line	789	"limbo.y"
{
		yyval.node = mkunary(Oseq, mkunary(Olabel, rotater(yys[yypt-1].yyv.node)));
	}
126=>
#line	793	"limbo.y"
{
		yys[yypt-3].yyv.node.left.right = yys[yypt-2].yyv.node;
		yyval.node = mkbin(Oseq, mkunary(Olabel, rotater(yys[yypt-1].yyv.node)), yys[yypt-3].yyv.node);
	}
127=>
yyval.node = yys[yyp+1].yyv.node;
128=>
#line	801	"limbo.y"
{
		yyval.node = mkbin(Orange, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
129=>
#line	805	"limbo.y"
{
		yyval.node = mkn(Owild, nil, nil);
		yyval.node.src = yys[yypt-0].yyv.tok.src;
	}
130=>
#line	810	"limbo.y"
{
		yyval.node = mkbin(Oseq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
131=>
#line	814	"limbo.y"
{
		yyval.node = mkn(Onothing, nil, nil);
		yyval.node.src.start = curline();
		yyval.node.src.stop = yyval.node.src.start;
	}
132=>
#line	822	"limbo.y"
{
		yys[yypt-1].yyv.node.left.right = mkscope(yys[yypt-0].yyv.node);
		yyval.node = yys[yypt-1].yyv.node;
	}
133=>
#line	829	"limbo.y"
{
		yyval.node = mkunary(Oseq, mkunary(Olabel, rotater(yys[yypt-1].yyv.node)));
	}
134=>
#line	833	"limbo.y"
{
		yys[yypt-3].yyv.node.left.right = mkscope(yys[yypt-2].yyv.node);
		yyval.node = mkbin(Oseq, mkunary(Olabel, rotater(yys[yypt-1].yyv.node)), yys[yypt-3].yyv.node);
	}
135=>
#line	840	"limbo.y"
{
		yyval.node = mkname(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval);
	}
136=>
#line	844	"limbo.y"
{
		yyval.node = mkn(Owild, nil, nil);
		yyval.node.src = yys[yypt-0].yyv.tok.src;
	}
137=>
#line	849	"limbo.y"
{
		yyval.node = mkbin(Oseq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
138=>
#line	853	"limbo.y"
{
		yyval.node = mkn(Onothing, nil, nil);
		yyval.node.src.start = curline();
		yyval.node.src.stop = yyval.node.src.start;
	}
139=>
#line	861	"limbo.y"
{
		yyval.node = mkn(Onothing, nil, nil);
		yyval.node.src.start = curline();
		yyval.node.src.stop = yyval.node.src.start;
	}
140=>
yyval.node = yys[yyp+1].yyv.node;
141=>
yyval.node = yys[yyp+1].yyv.node;
142=>
#line	871	"limbo.y"
{
		yyval.node = mkbin(Oas, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
143=>
#line	875	"limbo.y"
{
		yyval.node = mkbin(Oandas, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
144=>
#line	879	"limbo.y"
{
		yyval.node = mkbin(Ooras, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
145=>
#line	883	"limbo.y"
{
		yyval.node = mkbin(Oxoras, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
146=>
#line	887	"limbo.y"
{
		yyval.node = mkbin(Olshas, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
147=>
#line	891	"limbo.y"
{
		yyval.node = mkbin(Orshas, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
148=>
#line	895	"limbo.y"
{
		yyval.node = mkbin(Oaddas, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
149=>
#line	899	"limbo.y"
{
		yyval.node = mkbin(Osubas, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
150=>
#line	903	"limbo.y"
{
		yyval.node = mkbin(Omulas, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
151=>
#line	907	"limbo.y"
{
		yyval.node = mkbin(Odivas, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
152=>
#line	911	"limbo.y"
{
		yyval.node = mkbin(Omodas, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
153=>
#line	915	"limbo.y"
{
		yyval.node = mkbin(Osnd, yys[yypt-3].yyv.node, yys[yypt-0].yyv.node);
	}
154=>
#line	919	"limbo.y"
{
		yyval.node = mkbin(Odas, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
155=>
#line	923	"limbo.y"
{
		yyval.node = mkn(Oload, yys[yypt-0].yyv.node, nil);
		yyval.node.src.start = yys[yypt-2].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.node.src.stop;
		yyval.node.ty = mkidtype(yys[yypt-1].yyv.tok.src, yys[yypt-1].yyv.tok.v.idval);
	}
156=>
#line	930	"limbo.y"
{
		yyval.node = mkbin(Omul, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
157=>
#line	934	"limbo.y"
{
		yyval.node = mkbin(Odiv, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
158=>
#line	938	"limbo.y"
{
		yyval.node = mkbin(Omod, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
159=>
#line	942	"limbo.y"
{
		yyval.node = mkbin(Oadd, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
160=>
#line	946	"limbo.y"
{
		yyval.node = mkbin(Osub, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
161=>
#line	950	"limbo.y"
{
		yyval.node = mkbin(Orsh, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
162=>
#line	954	"limbo.y"
{
		yyval.node = mkbin(Olsh, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
163=>
#line	958	"limbo.y"
{
		yyval.node = mkbin(Olt, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
164=>
#line	962	"limbo.y"
{
		yyval.node = mkbin(Ogt, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
165=>
#line	966	"limbo.y"
{
		yyval.node = mkbin(Oleq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
166=>
#line	970	"limbo.y"
{
		yyval.node = mkbin(Ogeq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
167=>
#line	974	"limbo.y"
{
		yyval.node = mkbin(Oeq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
168=>
#line	978	"limbo.y"
{
		yyval.node = mkbin(Oneq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
169=>
#line	982	"limbo.y"
{
		yyval.node = mkbin(Oand, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
170=>
#line	986	"limbo.y"
{
		yyval.node = mkbin(Oxor, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
171=>
#line	990	"limbo.y"
{
		yyval.node = mkbin(Oor, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
172=>
#line	994	"limbo.y"
{
		yyval.node = mkbin(Ocons, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
173=>
#line	998	"limbo.y"
{
		yyval.node = mkbin(Oandand, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
174=>
#line	1002	"limbo.y"
{
		yyval.node = mkbin(Ooror, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
175=>
yyval.node = yys[yyp+1].yyv.node;
176=>
#line	1009	"limbo.y"
{
		yys[yypt-0].yyv.node.src.start = yys[yypt-1].yyv.tok.src.start;
		yyval.node = yys[yypt-0].yyv.node;
	}
177=>
#line	1014	"limbo.y"
{
		yyval.node = mkunary(Oneg, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
178=>
#line	1019	"limbo.y"
{
		yyval.node = mkunary(Onot, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
179=>
#line	1024	"limbo.y"
{
		yyval.node = mkunary(Ocomp, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
180=>
#line	1029	"limbo.y"
{
		yyval.node = mkunary(Oind, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
181=>
#line	1034	"limbo.y"
{
		yyval.node = mkunary(Opreinc, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
182=>
#line	1039	"limbo.y"
{
		yyval.node = mkunary(Opredec, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
183=>
#line	1044	"limbo.y"
{
		yyval.node = mkunary(Orcv, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
184=>
#line	1049	"limbo.y"
{
		yyval.node = mkunary(Ohd, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
185=>
#line	1054	"limbo.y"
{
		yyval.node = mkunary(Otl, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
186=>
#line	1059	"limbo.y"
{
		yyval.node = mkunary(Olen, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
187=>
#line	1064	"limbo.y"
{
		yyval.node = mkunary(Oref, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
188=>
#line	1069	"limbo.y"
{
		yyval.node = mkunary(Otagof, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
	}
189=>
#line	1074	"limbo.y"
{
		yyval.node = mkn(Oarray, yys[yypt-3].yyv.node, nil);
		yyval.node.ty = mktype(yys[yypt-5].yyv.tok.src.start, yys[yypt-0].yyv.ty.src.stop, Tarray, yys[yypt-0].yyv.ty, nil);
		yyval.node.src = yyval.node.ty.src;
	}
190=>
#line	1080	"limbo.y"
{
		yyval.node = mkn(Oarray, yys[yypt-5].yyv.node, yys[yypt-1].yyv.node);
		yyval.node.src.start = yys[yypt-7].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
191=>
#line	1086	"limbo.y"
{
		yyval.node = mkn(Onothing, nil, nil);
		yyval.node.src.start = yys[yypt-5].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-4].yyv.tok.src.stop;
		yyval.node = mkn(Oarray, yyval.node, yys[yypt-1].yyv.node);
		yyval.node.src.start = yys[yypt-6].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
192=>
#line	1095	"limbo.y"
{
		yyval.node = etolist(yys[yypt-1].yyv.node);
		yyval.node.src.start = yys[yypt-4].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
193=>
#line	1101	"limbo.y"
{
		yyval.node = mkn(Ochan, nil, nil);
		yyval.node.ty = mktype(yys[yypt-2].yyv.tok.src.start, yys[yypt-0].yyv.ty.src.stop, Tchan, yys[yypt-0].yyv.ty, nil);
		yyval.node.src = yyval.node.ty.src;
	}
194=>
#line	1107	"limbo.y"
{
		yyval.node = mkunary(Ocast, yys[yypt-0].yyv.node);
		yyval.node.ty = mktype(yys[yypt-3].yyv.tok.src.start, yys[yypt-0].yyv.node.src.stop, Tarray, mkidtype(yys[yypt-1].yyv.tok.src, yys[yypt-1].yyv.tok.v.idval), nil);
		yyval.node.src = yyval.node.ty.src;
	}
195=>
#line	1113	"limbo.y"
{
		yyval.node = mkunary(Ocast, yys[yypt-0].yyv.node);
		yyval.node.src.start = yys[yypt-1].yyv.tok.src.start;
		yyval.node.ty = mkidtype(yyval.node.src, yys[yypt-1].yyv.tok.v.idval);
	}
196=>
yyval.node = yys[yyp+1].yyv.node;
197=>
#line	1122	"limbo.y"
{
		yyval.node = mkn(Ocall, yys[yypt-3].yyv.node, yys[yypt-1].yyv.node);
		yyval.node.src.start = yys[yypt-3].yyv.node.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
198=>
#line	1128	"limbo.y"
{
		yyval.node = yys[yypt-1].yyv.node;
		if(yys[yypt-1].yyv.node.op == Oseq)
			yyval.node = mkn(Otuple, rotater(yys[yypt-1].yyv.node), nil);
		else
			yyval.node.parens = byte 1;
		yyval.node.src.start = yys[yypt-2].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
199=>
#line	1138	"limbo.y"
{
#		n := mkdeclname($1, mkids($1, enter(".fn"+string nfnexp++, 0), nil, nil));
#		$<node>$ = fndef(n, $2);
#		nfns++;
	}
200=>
#line	1143	"limbo.y"
{
#		$$ = fnfinishdef($<node>3, $4);
#		$$ = mkdeclname($1, $$.left.decl);
		yyerror("urt unk");
		yyval.node = nil;
	}
201=>
#line	1150	"limbo.y"
{
		yyval.node = mkbin(Odot, yys[yypt-2].yyv.node, mkname(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval));
	}
202=>
#line	1154	"limbo.y"
{
		yyval.node = mkbin(Omdot, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
203=>
#line	1158	"limbo.y"
{
		yyval.node = mkbin(Oindex, yys[yypt-3].yyv.node, yys[yypt-1].yyv.node);
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
204=>
#line	1163	"limbo.y"
{
		if(yys[yypt-3].yyv.node.op == Onothing)
			yys[yypt-3].yyv.node.src = yys[yypt-2].yyv.tok.src;
		if(yys[yypt-1].yyv.node.op == Onothing)
			yys[yypt-1].yyv.node.src = yys[yypt-2].yyv.tok.src;
		yyval.node = mkbin(Oslice, yys[yypt-5].yyv.node, mkbin(Oseq, yys[yypt-3].yyv.node, yys[yypt-1].yyv.node));
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
205=>
#line	1172	"limbo.y"
{
		yyval.node = mkunary(Oinc, yys[yypt-1].yyv.node);
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
206=>
#line	1177	"limbo.y"
{
		yyval.node = mkunary(Odec, yys[yypt-1].yyv.node);
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
207=>
#line	1182	"limbo.y"
{
		yyval.node = mksconst(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval);
	}
208=>
#line	1186	"limbo.y"
{
		yyval.node = mkconst(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.ival);
		if(yys[yypt-0].yyv.tok.v.ival > big 16r7fffffff || yys[yypt-0].yyv.tok.v.ival < big -16r7fffffff)
			yyval.node.ty = tbig;
	}
209=>
#line	1192	"limbo.y"
{
		yyval.node = mkrconst(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.rval);
	}
210=>
#line	1198	"limbo.y"
{
		yyval.node = mkname(yys[yypt-0].yyv.tok.src, yys[yypt-0].yyv.tok.v.idval);
	}
211=>
#line	1202	"limbo.y"
{
		yyval.node = mknil(yys[yypt-0].yyv.tok.src);
	}
212=>
#line	1208	"limbo.y"
{
		yyval.node = mkn(Otuple, rotater(yys[yypt-1].yyv.node), nil);
		yyval.node.src.start = yys[yypt-2].yyv.tok.src.start;
		yyval.node.src.stop = yys[yypt-0].yyv.tok.src.stop;
	}
213=>
yyval.node = yys[yyp+1].yyv.node;
214=>
yyval.node = yys[yyp+1].yyv.node;
215=>
#line	1218	"limbo.y"
{
		yyval.node = mkbin(Oseq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
216=>
#line	1222	"limbo.y"
{
		yyval.node = mkbin(Oseq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
217=>
#line	1228	"limbo.y"
{
		yyval.node = nil;
	}
218=>
#line	1232	"limbo.y"
{
		yyval.node = rotater(yys[yypt-0].yyv.node);
	}
219=>
yyval.node = yys[yyp+1].yyv.node;
220=>
#line	1239	"limbo.y"
{
		yyval.node = mkbin(Oseq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
221=>
#line	1245	"limbo.y"
{
		yyval.node = rotater(yys[yypt-0].yyv.node);
	}
222=>
#line	1249	"limbo.y"
{
		yyval.node = rotater(yys[yypt-1].yyv.node);
	}
223=>
yyval.node = yys[yyp+1].yyv.node;
224=>
#line	1256	"limbo.y"
{
		yyval.node = mkbin(Oseq, yys[yypt-2].yyv.node, yys[yypt-0].yyv.node);
	}
225=>
#line	1262	"limbo.y"
{
		yyval.node = mkn(Oelem, nil, yys[yypt-0].yyv.node);
		yyval.node.src = yys[yypt-0].yyv.node.src;
	}
226=>
#line	1267	"limbo.y"
{
		yyval.node = mkbin(Oelem, rotater(yys[yypt-2].yyv.node), yys[yypt-0].yyv.node);
	}
		}
	}

	yylval = save1;
	yyval = save2;
	yynerrs = save3;
	yyerrflag = save4;
	return yyn;
}
