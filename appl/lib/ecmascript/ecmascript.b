implement Ecmascript;

include "sys.m";
include "math.m";
include "string.m";
include "daytime.m";
include "ecmascript.m";

include "pprint.b";
include "obj.b";
include "exec.b";
include "date.b";
include "builtin.b";

me: ESHostobj;

sys: Sys;
print, sprint: import sys;
stdout: ref Sys->FD;

math: Math;
	isnan, floor, copysign, fabs, fmod, NaN, Infinity: import math;

str: String;

daytime: Daytime;
	Tm: import daytime;

HashSize:	con 1024;

Parser: adt
{
	ex:		ref Exec;

	code:		ref Code;

	inloop:		int;		# parser state
	infunc:		int;
	lastnl:		int;		# parser state for inserting ;

	token:		int;		# lexical token
	id:		int;		# associated value
	lineno:		int;

	src:		string;		# lexical input state
	esrc:		int;
	srci:		int;

	errors: int;
};

Keywd: adt
{
	name:	string;
	token:	int;
};

#
#	lexical tokens and ops
#
	Lbase:	con 128;

	Leos,

	Landas,
	Loras,
	Lxoras,
	Llshas,
	Lrshas,
	Lrshuas,
	Laddas,
	Lsubas,
	Lmulas,
	Ldivas,
	Lmodas,
	Loror,
	Landand,
	Leq,
	Lneq,
	Lleq,
	Lgeq,
	Llsh,
	Lrsh,
	Lrshu,
	Linc,
	Ldec,
	Lnum,
	Lid,
	Lstr,
	Lthis,
	Ltypeof,
	Ldelete,
	Lvoid,
	Lwhile,
	Lfor,
	Lbreak,
	Lcontinue,
	Lwith,
	Lreturn,
	Lfunction,
	Lvar,
	Lif,
	Lelse,
	Lin,
	Lnew,

	Lpostinc,		# ops that aren't lexical tokens
	Lpostdec,
	Lpresub,
	Lpreadd,
	Lcall,
	Lnewcall,
	Lgetval,
	Las,
	Lasop,
	Lforin,
	Lforvar,
	Lforvarin,

	#
	# reserved words
	#
	Lcase,
	Lcatch,
	Lclass,
	Lconst,
	Ldebugger,
	Ldefault,
	Ldo,
	Lenum,
	Lexport,
	Lextends,
	Lfinally,
	Limport,
	Lsuper,
	Lswitch,
	Lthrow,
	Ltry:	con Lbase + iota;


#
# internals
#

Mlower, Mupper, Munder, Mdigit, Msign, Mexp, Mhex, Moct: con byte 1 << iota;
Malpha:	con Mupper|Mlower|Munder;
map :=		array[256] of 
{
	'_' or '$'		=> Munder,
	'-' or '+'		=> Msign,
	'a' to 'd' or 'f'	=> Mlower | Mhex,
	'e'			=> Mlower | Mhex | Mexp,
	'g' to 'z'		=> Mlower,
	'A' to 'D' or 'F'	=> Mupper | Mhex,
	'E'			=> Mupper | Mhex | Mexp,
	'G' to 'Z'		=> Mupper,
	'0' to '7'		=> Mdigit | Mhex | Moct,
	'8' to '9'		=> Mdigit | Mhex,
	*			=> byte 0
};

maxerr:		int;
toterrors:	int;
fabort:		int;

escmap :=	array[] of
{
	'\'' =>		byte '\'',
	'"' =>		byte '"',
	'\\' =>		byte '\\',
	'b' =>		byte '\b',
	'f' =>		byte '\u000c',
	'n' =>		byte '\n',
	'r' =>		byte '\r',
	't' =>		byte '\t',

	* =>		byte 255
};

#
# must be sorted
#
keywords := array [] of
{
	Keywd("break",		Lbreak),
	Keywd("case",		Lcase),
	Keywd("catch",		Lcatch),
	Keywd("class",		Lclass),
	Keywd("const",		Lconst),
	Keywd("continue",	Lcontinue),
	Keywd("debugger",	Ldebugger),
	Keywd("default",	Ldefault),
	Keywd("delete",		Ldelete),
	Keywd("do",		Ldo),
	Keywd("else",		Lelse),
	Keywd("enum",		Lenum),
	Keywd("export",		Lexport),
	Keywd("extends	",	Lextends),
	Keywd("finally",	Lfinally),
	Keywd("for",		Lfor),
	Keywd("function",	Lfunction),
	Keywd("if",		Lif),
	Keywd("import",		Limport),
	Keywd("in",		Lin),
	Keywd("new",		Lnew),
	Keywd("return",		Lreturn),
	Keywd("super",		Lsuper),
	Keywd("switch",		Lswitch),
	Keywd("this",		Lthis),
	Keywd("throw",		Lthrow),
	Keywd("try",		Ltry),
	Keywd("typeof",		Ltypeof),
	Keywd("var",		Lvar),
	Keywd("void",		Lvoid),
	Keywd("while",		Lwhile),
	Keywd("with",		Lwith),
};

debug = array[256] of {* => 0};

glbuiltins := array[] of
{
	Builtin("eval", "eval", array[] of {"src"}, 1),
	Builtin("parseInt", "parseInt", array[] of {"string", "radix"}, 2),
	Builtin("parseFloat", "parseFloat", array[] of {"string"}, 1),
	Builtin("escape", "escape", array[] of {"string"}, 1),
	Builtin("unescape", "unescape", array[] of {"string"}, 1),
	Builtin("isNaN", "isNaN", array[] of {"number"}, 1),
	Builtin("isFinite", "isFinite", array[] of {"number"}, 1),
};

biobj := Builtin("Object", "Object", array[] of {"value"}, 1);
biobjproto := array[] of
{
	Builtin("toString", "Object.prototype.toString", nil, 0),
	Builtin("valueOf", "Object.prototype.valueOf", nil, 0),
};

bifunc := Builtin("Function", "Function", array[] of {"body"}, 1);
bifuncproto := array[] of
{
	Builtin("toString", "Function.prototype.toString", nil, 0),
};

biarray := Builtin("Array", "Array", array[] of {"length"}, 1);
biarrayproto := array[] of
{
	Builtin("toString", "Array.prototype.toString", nil, 0),
	Builtin("join", "Array.prototype.join", array[] of {"separator"}, 1),
	Builtin("reverse", "Array.prototype.reverse", nil, 0),
	Builtin("sort", "Array.prototype.sort", array[] of {"comparefunc"}, 1),
};

bistr := Builtin("String", "String", array[] of {"value"}, 1);
bistrproto := array[] of
{
	Builtin("toString", "String.prototype.toString", nil, 0),
	Builtin("valueOf", "String.prototype.valueOf", nil, 0),
	Builtin("charAt", "String.prototype.charAt", array[] of {"pos"}, 1),
	Builtin("charCodeAt", "String.prototype.charCodeAt", array[] of {"pos"}, 1),
	Builtin("indexOf", "String.prototype.indexOf", array[] of {"string", "pos"}, 2),
	Builtin("lastIndexOf", "String.prototype.lastIndexOf", array[] of {"string", "pos"}, 2),
	Builtin("split", "String.prototype.split", array[] of {"separator"}, 1),
	Builtin("substring", "String.prototype.substring", array[] of {"start", "end"}, 2),
	Builtin("toLowerCase", "String.prototype.toLowerCase", nil, 0),
	Builtin("toUpperCase", "String.prototype.toUpperCase", nil, 0),
# JavaScript 1.0
	Builtin("anchor", "String.prototype.anchor", array[] of {"name"}, 1),
	Builtin("big", "String.prototype.big", nil, 0),
	Builtin("blink", "String.prototype.blink", nil, 0),
	Builtin("bold", "String.prototype.bold", nil, 0),
	Builtin("fixed", "String.prototype.fixed", nil, 0),
	Builtin("fontcolor", "String.prototype.fontcolor", array[] of {"color"}, 1),
	Builtin("fontsize", "String.prototype.fontsize", array[] of {"size"}, 1),
	Builtin("italics", "String.prototype.italics", nil, 0),
	Builtin("link", "String.prototype.link", array[] of {"href"}, 1),
	Builtin("small", "String.prototype.small", nil, 0),
	Builtin("strike", "String.prototype.strike", nil, 0),
	Builtin("sub", "String.prototype.sub", nil, 0),
	Builtin("sup", "String.prototype.sup", nil, 0),
};
bistrctor := Builtin("fromCharCode", "String.fromCharCode", array[] of {"characters"}, 1);

bibool := Builtin("Boolean", "Boolean", array[] of {"value"}, 1);
biboolproto := array[] of
{
	Builtin("toString", "Boolean.prototype.toString", nil, 0),
	Builtin("valueOf", "Boolean.prototype.valueOf", nil, 0),
};

binum := Builtin("Number", "Number", array[] of {"value"}, 1);
binumproto := array[] of
{
	Builtin("toString", "Number.prototype.toString", nil, 0),
	Builtin("valueOf", "Number.prototype.valueOf", nil, 0),
};

bidate := Builtin("Date", "Date", array[] of {"value"}, 1);
bidateproto := array[] of
{
	Builtin("toString", "Date.prototype.toString", nil, 0),
	Builtin("valueOf", "Date.prototype.valueOf", nil, 0),
	Builtin("getTime", "Date.prototype.getTime", nil, 0),
	Builtin("getYear", "Date.prototype.getYear", nil, 0),
	Builtin("getFullYear", "Date.prototype.getFullYear", nil, 0),
	Builtin("getUTCFullYear", "Date.prototype.getUTCFullYear", nil, 0),
	Builtin("getMonth", "Date.prototype.getMonth", nil, 0),
	Builtin("getUTCMonth", "Date.prototype.getUTCMonth", nil, 0),
	Builtin("getDate", "Date.prototype.getDate", nil, 0),
	Builtin("getUTCDate", "Date.prototype.getUTCDate", nil, 0),
	Builtin("getDay", "Date.prototype.getDay", nil, 0),
	Builtin("getUTCDay", "Date.prototype.getUTCDay", nil, 0),
	Builtin("getHours", "Date.prototype.getHours", nil, 0),
	Builtin("getUTCHours", "Date.prototype.getUTCHours", nil, 0),
	Builtin("getMinutes", "Date.prototype.getMinutes", nil, 0),
	Builtin("getUTCMinutes", "Date.prototype.getUTCMinutes", nil, 0),
	Builtin("getSeconds", "Date.prototype.getSeconds", nil, 0),
	Builtin("getUTCSeconds", "Date.prototype.getUTCSeconds", nil, 0),
	Builtin("getMilliseconds", "Date.prototype.getMilliseconds", nil, 0),
	Builtin("getUTCMilliseconds", "Date.prototype.getUTCMilliseconds", nil, 0),
	Builtin("getTimezoneOffset", "Date.prototype.getTimezoneOffset", nil, 0),
	Builtin("setTime", "Date.prototype.setTime", array[] of {"time"}, 1),
	Builtin("setMilliseconds", "Date.prototype.setMilliseconds", array[] of {"ms"}, 1),
	Builtin("setUTCMilliseconds", "Date.prototype.setUTCMilliseconds", array[] of {"ms"}, 1),
	Builtin("setSeconds", "Date.prototype.setSeconds", array[] of {"sec", "ms"}, 2),
	Builtin("setUTCSeconds", "Date.prototype.setUTCSeconds", array[] of {"sec", "ms"}, 2),
	Builtin("setMinutes", "Date.prototype.setMinutes", array[] of {"min", "sec", "ms"}, 3),
	Builtin("setUTCMinutes", "Date.prototype.setUTCMinutes", array[] of {"min", "sec", "ms"}, 3),
	Builtin("setHours", "Date.prototype.setHours", array[] of {"hour", "min", "sec", "ms"}, 4),
	Builtin("setUTCHours", "Date.prototype.setUTCHours", array[] of {"hour", "min", "sec", "ms"}, 4),
	Builtin("setDate", "Date.prototype.setDate", array[] of {"date"}, 1),
	Builtin("setUTCDate", "Date.prototype.setUTCDate", array[] of {"date"}, 1),
	Builtin("setMonth", "Date.prototype.setMonth", array[] of {"mon", "date"}, 2),
	Builtin("setUTCMonth", "Date.prototype.setUTCMonth", array[] of {"mon", "date"}, 2),
	Builtin("setFullYear", "Date.prototype.setFullYear", array[] of {"year", "mon", "date"}, 3),
	Builtin("setUTCFullYear", "Date.prototype.setUTCFullYear", array[] of {"year", "mon", "date"}, 3),
	Builtin("setYear", "Date.prototype.setYear", array[] of {"year"}, 1),
	Builtin("toLocaleString", "Date.prototype.toLocaleString", nil, 0),
	Builtin("toUTCString", "Date.prototype.toUTCString", nil, 0),
	Builtin("toGMTString", "Date.prototype.toGMTString", nil, 0),
};
bidatector := array[] of
{
	Builtin("parse", "Date.parse", array[] of {"string"}, 1),
	Builtin("UTC", "Date.UTC", array[] of {"year", "month", "date", "hours", "minutes", "seconds", "ms"}, 7),
};

bimath := array[] of
{
	Builtin("abs", "Math.abs", array[] of {"x"}, 1),
	Builtin("acos", "Math.acos", array[] of {"x"}, 1),
	Builtin("asin", "Math.asin", array[] of {"x"}, 1),
	Builtin("atan", "Math.atan", array[] of {"x"}, 1),
	Builtin("atan2", "Math.atan2", array[] of {"y", "x"}, 2),
	Builtin("ceil", "Math.ceil", array[] of {"x"}, 1),
	Builtin("cos", "Math.cos", array[] of {"x"}, 1),
	Builtin("exp", "Math.exp", array[] of {"x"}, 1),
	Builtin("floor", "Math.floor", array[] of {"x"}, 1),
	Builtin("log", "Math.log", array[] of {"x"}, 1),
	Builtin("max", "Math.max", array[] of {"x", "y"}, 2),
	Builtin("min", "Math.min", array[] of {"x", "y"}, 2),
	Builtin("pow", "Math.pow", array[] of {"x", "y"}, 2),
	Builtin("random", "Math.random", nil, 0),
	Builtin("round", "Math.round", array[] of {"x"}, 1),
	Builtin("sin", "Math.sin", array[] of {"x"}, 1),
	Builtin("sqrt", "Math.sqrt", array[] of {"x"}, 1),
	Builtin("tan", "Math.tan", array[] of {"x"}, 1),
};

init()
{
	sys = load Sys Sys->PATH;

	math = load Math Math->PATH;
	if(math == nil){
		print("can't load math functions: %r\n");
		exit;
	}

	str = load String String->PATH;
	if(str == nil){
		print("can't load string functions: %r\n");
		exit;
	}

	daytime = load Daytime Daytime->PATH;
	if(daytime == nil){
		print("can't load daytime functions: %r\n");
		exit;
	}

	me = load ESHostobj SELF;
	if(me == nil){
		print("can't load builtin functions: %r\n");
		exit;
	}

	randinit(big sys->millisec());

	stdout = sys->fildes(1);

	#
	# maximum number of syntax errors reported
	#
	maxerr = 1;

	undefined = ref Val(TUndef, 0., nil, nil);
	null =	ref Val(TNull, 0., nil, nil);
	true = ref Val(TBool, 1., nil, nil);
	false = ref Val(TBool, 0., nil, nil);
}

mkcall(ex : ref Exec, p: array of string): ref Call
{
	return ref Call(p, nil, ex);
}

mkbiobj(ex: ref Exec, meth: Builtin, proto: ref Obj): ref Obj
{
	o := biinst(ex.global, meth, ex.funcproto, me);
	o.construct = o.call;
	valinstant(o, DontEnum|DontDelete|ReadOnly, "prototype", objval(proto));
	valinstant(proto, DontEnum, "constructor", objval(o));
	return o;
}

mkexec(go: ref Obj): ref Exec
{
	o: ref Obj;
	if(go == nil)
		go = mkobj(nil, "global");
	ex := ref Exec;
	ex.this = go;
	ex.scopechain = go :: nil;
	ex.stack = array[4] of ref Ref;
	ex.sp = 0;
	ex.global = go;

	#
	# builtin object prototypes
	#
	ex.objproto = mkobj(nil, "Object");
	ex.funcproto = mkobj(ex.objproto, "Function");
	ex.arrayproto = mkobj(ex.objproto, "Array");
	ex.strproto = mkobj(ex.objproto, "String");
	ex.numproto = mkobj(ex.objproto, "Number");
	ex.boolproto = mkobj(ex.objproto, "Boolean");
	ex.dateproto = mkobj(ex.objproto, "Date");

	biminst(ex.objproto, biobjproto, ex.funcproto, me);

	biminst(ex.funcproto, bifuncproto, ex.funcproto, me);
	ex.funcproto.call = mkcall(ex, nil);
	ex.funcproto.val = strval("Function.prototype");
	valinstant(ex.funcproto, DontEnum|DontDelete|ReadOnly, "length", numval(real 0));

	biminst(ex.arrayproto, biarrayproto, ex.funcproto, me);
	valinstant(ex.arrayproto, DontEnum|DontDelete, "length", numval(real 0));

	biminst(ex.strproto, bistrproto, ex.funcproto, me);
	ex.strproto.val = strval("");
	valinstant(ex.strproto, DontEnum|DontDelete|ReadOnly, "length", numval(real 0));

	biminst(ex.boolproto, biboolproto, ex.funcproto, me);
	ex.boolproto.val = false;

	biminst(ex.numproto, binumproto, ex.funcproto, me);
	ex.numproto.val = numval(real +0);

	biminst(ex.dateproto, bidateproto, ex.funcproto, me);
	ex.dateproto.val = numval(Math->NaN);
	valinstant(ex.dateproto, DontEnum|DontDelete|ReadOnly, "length", numval(real 7));

	#
	# simple builtin functions and values
	#
	valinstant(go, DontEnum, "NaN", numval(Math->NaN));
	valinstant(go, DontEnum, "Infinity", numval(Math->Infinity));

	biminst(go, glbuiltins, ex.funcproto, me);

	#
	# builtin objects, and cross-link them to their prototypes
	#
	mkbiobj(ex, biobj, ex.objproto);
	mkbiobj(ex, bifunc, ex.funcproto);
	mkbiobj(ex, biarray, ex.arrayproto);
	o = mkbiobj(ex, bistr, ex.strproto);
	biinst(o, bistrctor, ex.funcproto, me);
	mkbiobj(ex, bibool, ex.boolproto);
	o = mkbiobj(ex, binum, ex.numproto);

	math->FPcontrol(0, Math->INVAL|Math->ZDIV|Math->OVFL|Math->UNFL|Math->INEX);

	valinstant(o, DontEnum|DontDelete|ReadOnly, "MAX_VALUE", numval(math->nextafter(Math->Infinity, 0.)));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "MIN_VALUE", numval(math->nextafter(0., 1.)));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "NaN", numval(Math->NaN));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "NEGATIVE_INFINITY", numval(-Math->Infinity));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "POSITIVE_INFINITY", numval(+Math->Infinity));
	o = mkbiobj(ex, bidate, ex.dateproto);
	biminst(o, bidatector, ex.funcproto, me);

	#
	# the math object is a little different
	#
	o = mkobj(ex.objproto, "Object");
	valinstant(go, DontEnum, "Math", objval(o));
	biminst(o, bimath, ex.funcproto, me);

	#
	# these are computed so they are consistent with numbers ecma might calculate
	#
	mathe := math->exp(1.);
	valinstant(o, DontEnum|DontDelete|ReadOnly, "E", numval(mathe));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "LN10", numval(math->log(10.)));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "LN2", numval(math->log(2.)));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "LOG2E", numval(1./math->log(2.)));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "LOG10E", numval(1./math->log(10.)));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "PI", numval(Math->Pi));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "SQRT1_2", numval(math->sqrt(1./2.)));
	valinstant(o, DontEnum|DontDelete|ReadOnly, "SQRT2", numval(math->sqrt(2.)));

	return ex;
}

mkparser(ex: ref Exec, src: string): ref Parser
{
	p := ref Parser;
	p.ex = ex;
	p.src = src;
	p.esrc = len src;
	p.srci = 0;
	p.errors = 0;
	p.lineno = 1;
	p.token = -1;
	p.lastnl = 0;
	p.inloop = 0;
	p.infunc = 0;
	p.code = mkcode();
	return p;
}

eval(ex: ref Exec, src: string): Completion
{
	p := mkparser(ex, src);

	if(debug['t'])
		parset := sys->millisec();

	prog(ex, p);

	toterrors += p.errors;

	if(p.errors)
		runtime(ex, ex.error);
	if(debug['p']){
		s := array of byte pprint(ex, p.code, "");
		if(len s)
			sys->write(stdout, s, len s);
	}

	if(debug['t'])
		xect := sys->millisec();

	globalinstant(hd ex.scopechain, p.code.vars);
	c := exec(ex, p.code);

	if(debug['t'])
		print("parse time %d exec time %d\n", xect - parset, sys->millisec() - xect);

	return c;
}

#prog	: selems
#	;
#
#selems	: selem
#	| selems selem
#	;
#selem	: stmt
#	| fundecl
#	;
prog(ex: ref Exec, p: ref Parser)
{
	while(look(p) != Leos)
		if(look(p) == Lfunction)
			fundecl(ex, p);
		else
			stmt(p);
}

#fundecl	: Lfunction Lid '(' zplist ')' '{' stmts '}'
#	;
#zplist	:
#	| plist
#	;
#
#plist	: Lid
#	| plist ',' Lid
#	;
fundecl(ex: ref Exec, p: ref Parser)
{
	c := p.code;
	mustbe(p, Lfunction);
	mustbe(p, Lid);
	jp := codevar(p, 0);
	p.code = mkcode();
	mustbe(p, '(');
	if(look(p) != ')'){
		for(;;){
			mustbe(p, Lid);
			codevar(p, 1);
			if(look(p) == ')')
				break;
			mustbe(p, ',');
		}
	}
	params := p.code.vars;
	p.code.vars = nil;
	mustbe(p, ')');
	mustbe(p, '{');
	p.infunc = 1;
	stmts(p);
	p.infunc = 0;
	mustbe(p, '}');

	#
	# override any existing value,
	# as per sec. 10.1.3 Variable instantiation
	#
	sparams := array[len params] of string;
	for(i := 0; i < len sparams; i++)
		sparams[i] = params[i].name;

	#
	# construct a function object;
	# see section 15.3.21
	o := mkobj(ex.funcproto, "Function");
	o.call = ref Call(sparams, p.code, ex);
	o.construct = o.call;
	o.val = strval(jp.name);
	valinstant(o, DontDelete|DontEnum|ReadOnly, "length", numval(real len sparams));
	po := nobj(ex, nil, nil);
	valinstant(o, DontEnum, "prototype", objval(po));
	valinstant(po, DontEnum, "constructor", objval(o));
	valinstant(o, DontDelete|DontEnum|ReadOnly, "arguments", null);
	jp.val.val = objval(o);

	if(debug['p']){
		s := array of byte (funcprint(ex, o) + "\n");
		sys->write(stdout, s, len s);
	}

	p.code = c;
}

#
# install a variable for the id just lexed
#
codevar(p: ref Parser, forcenew: int): ref Prop
{
	name := p.code.strs[p.id];
	vs := p.code.vars;
	i : int;
	if(!forcenew){
		for(i = 0; i < len vs; i++)
			if(vs[i].name == name)
				return vs[i];
	}else{
		i = len vs;
	}
	vs = array[i+1] of ref Prop;
	vs[:] = p.code.vars;
	p.code.vars = vs;
	vs[i] = ref Prop(0, name, ref RefVal(undefined));
	return vs[i];
}

#stmts	:
#	| stmts stmt
#	;
stmts(p: ref Parser)
{
	while((op := look(p)) != '}' && op != Leos)
		stmt(p);
}

#stmt	: '{' stmts '}'
#	| Lvar varlist ';'
#	| exp ';'
#	| ';'
#	| Lif '(' exp ')' stmt
#	| Lif '(' exp ')' stmt Lelse stmt
#	| Lwhile '(' exp ')' stmt
#	| Lfor '(' zexp ';' zexp ';' zexp ')' stmt
#	| Lfor '(' Lvar varlist ';' zexp ';' zexp ')' stmt
#	| Lfor '(' lhsexp Lin exp ')' stmt
#	| Lfor '(' Lvar Lid [init] Lin exp ')' stmt
#	| Lcontinue ';'
#	| Lbreak ';'
#	| Lreturn zexp ';'	# no line term after return
#	| Lwith '(' exp ')' stmt
#	;
stmt(p: ref Parser)
{
	pc, oinloop: int;

	case op := look(p){
	';' =>
		lexemit(p);
	'{' =>
		lex(p);
		stmts(p);
		mustbe(p, '}');
	Lvar =>
		lexemit(p);
		pc = epatch(p);
		varlist(p);
		semi(p);
		patch(p, pc);
	* =>
		exp(p);
		semi(p);
		emit(p, ';');
	Lif =>
		lexemit(p);
		pc = epatch(p);
		mustbe(p, '(');
		exp(p);
		mustbe(p, ')');
		patch(p, pc);
		pc = epatch(p);
		stmt(p);
		patch(p, pc);
		pc = epatch(p);
		if(look(p) == Lelse){
			lex(p);
			stmt(p);
		}
		patch(p, pc);
	Lwhile or
	Lwith =>
		lexemit(p);
		pc = epatch(p);
		mustbe(p, '(');
		exp(p);
		mustbe(p, ')');
		patch(p, pc);
		oinloop = p.inloop;
		if(op == Lwhile)
			p.inloop = 1;
		pc = epatch(p);
		stmt(p);
		patch(p, pc);
		p.inloop = oinloop;
	Lfor =>
		fpc := p.code.npc;
		lexemit(p);
		mustbe(p, '(');
		if(look(p) == Lvar){
			lex(p);
			pc = epatch(p);
			varlist(p);
			patch(p, pc);
			if(look(p) == Lin){
				check1var(p);
				p.code.ops[fpc] = byte Lforvarin;
				lex(p);
				pc = epatch(p);
				exp(p);
				patch(p, pc);
			}else{
				p.code.ops[fpc] = byte Lforvar;
				mustbe(p, ';');
				pc = epatch(p);
				zexp(p);
				patch(p, pc);
				mustbe(p, ';');
				pc = epatch(p);
				zexp(p);
				patch(p, pc);
			}
		}else{
			pc = epatch(p);
			lhspc := p.code.npc;
			zexp(p);
			patch(p, pc);
			if(look(p) == Lin){
				p.code.ops[fpc] = byte Lforin;
				checklhsexp(p, lhspc);
				lex(p);
				pc = epatch(p);
				exp(p);
				patch(p, pc);
			}else{
				mustbe(p, ';');
				pc = epatch(p);
				zexp(p);
				patch(p, pc);
				mustbe(p, ';');
				pc = epatch(p);
				zexp(p);
				patch(p, pc);
			}
		}
		mustbe(p, ')');
		oinloop = p.inloop;
		p.inloop = 1;
		pc = epatch(p);
		stmt(p);
		patch(p, pc);
		p.inloop = oinloop;
	Lcontinue or
	Lbreak =>
		lexemit(p);
		semi(p);
		if(!p.inloop)
			error(p, tokname(op)+" not in a for or while loop");
	Lreturn =>
		lexemit(p);
		if(look(p) != ';' && !p.lastnl)
			exp(p);
		semi(p);
		emit(p, ';');
		if(!p.infunc)
			error(p, tokname(op)+" not in a function");
	}
}

semi(p: ref Parser)
{
	op := look(p);
	if(op == ';'){
		lex(p);
		return;
	}
	if(op == '}' || op == Leos || p.lastnl)
		return;
	mustbe(p, ';');
}

#varlist	: vardecl
#	| varlist ',' vardecl
#	;
#
#vardecl	: Lid init
#	;
#
#init	:
#	| '=' asexp
#	;
varlist(p: ref Parser)
{
	#
	# these declaration aren't supposed
	# to override current definitions
	#
	mustbe(p, Lid);
	codevar(p, 0);
	emitconst(p, Lid, p.id);
	if(look(p) == '='){
		lex(p);
		asexp(p);
		emit(p, '=');
	}
	if(look(p) != ',')
		return;
	emit(p, Lgetval);
	lex(p);
	varlist(p);
	emit(p, ',');
}

#
# check that only 1 id is declared in the var list
#
check1var(p: ref Parser)
{
	if(p.code.ops[p.code.npc-1] == byte ',')
		error(p, "only one identifier allowed");
}

#zexp	:
#	| exp
#	;
zexp(p: ref Parser)
{
	op := look(p);
	if(op == ';' || op == ')')
		return;
	exp(p);
}

#exp	: asexp
#	| exp ',' asexp
#	;
exp(p: ref Parser)
{
	asexp(p);
	while(look(p) == ','){
		lex(p);
		emit(p, Lgetval);
		asexp(p);
		emit(p, ',');
	}
}

#asexp	: condexp
#	| lhsexp asop asexp
#	;
#
#asop	: '=' | Lmulas | Ldivas | Lmodas | Laddas | Lsubas
#		| Llshas | Lrshas | Lrshuas | Landas | Lxoras | Loras
#	;
asops := array[] of { '=', Lmulas, Ldivas, Lmodas, Laddas, Lsubas
		| Llshas, Lrshas, Lrshuas, Landas, Lxoras, Loras };
asbaseops := array[] of { '=', '*', '/', '%', '+', '-'
		| Llsh, Lrsh, Lrshu, '&', '^', '|' };
asexp(p: ref Parser)
{
	lhspc := p.code.npc;
	condexp(p);
	i := inops(look(p), asops);
	if(i >= 0){
		op := lex(p);
		checklhsexp(p, lhspc);
		if(op != '=')
			emit(p, Lasop);
		asexp(p);
		emit(p, asbaseops[i]);
		if(op != '=')
			emit(p, Las);
	}
}

#condexp	: ororexp
#	| ororexp '?' asexp ':' asexp
#	;
condexp(p: ref Parser)
{
	ororexp(p);
	if(look(p) == '?'){
		lexemit(p);
		pc := epatch(p);
		asexp(p);
		mustbe(p, ':');
		patch(p, pc);
		pc = epatch(p);
		asexp(p);
		patch(p, pc);
	}
}

#ororexp	: andandexp
#	| ororexp op andandexp
#	;
ororexp(p: ref Parser)
{
	andandexp(p);
	while(look(p) == Loror){
		lexemit(p);
		pc := epatch(p);
		andandexp(p);
		patch(p, pc);
	}
}

#andandexp	: laexp
#	| andandexp op laexp
#	;
andandexp(p: ref Parser)
{
	laexp(p, 0);
	while(look(p) == Landand){
		lexemit(p);
		pc := epatch(p);
		laexp(p, 0);
		patch(p, pc);
	}
}

#laexp	: unexp
#	| laexp op laexp
#	;
prectab := array[] of
{
	array[] of { '|' },
	array[] of { '^' },
	array[] of { '&' },
	array[] of { Leq, Lneq },
	array[] of { '<', '>', Lleq, Lgeq },
	array[] of { Llsh, Lrsh, Lrshu },
	array[] of { '+', '-' },
	array[] of { '*', '/', '%' },
};
laexp(p: ref Parser, prec: int)
{
	unexp(p);
	for(pr := len prectab - 1; pr >= prec; pr--){
		while(inops(look(p), prectab[pr]) >= 0){
			emit(p, Lgetval);
			op := lex(p);
			laexp(p, prec + 1);
			emit(p, op);
		}
	}
}

#unexp	: postexp
#	| Ldelete unexp
#	| Lvoid unexp
#	| Ltypeof unexp
#	| Linc unexp
#	| Ldec unexp
#	| '+' unexp
#	| '-' unexp
#	| '~' unexp
#	| '!' unexp
#	;
preops := array[] of { Ldelete, Lvoid, Ltypeof, Linc, Ldec, '+', '-', '~', '!' };
unexp(p: ref Parser)
{
	if(inops(look(p), preops) >= 0){
		op := lex(p);
		unexp(p);
		if(op == '-')
			op = Lpresub;
		else if(op == '+')
			op = Lpreadd;
		emit(p, op);
		return;
	}
	postexp(p);
}

#postexp	: lhsexp
#	| lhsexp Linc	# no line terminators before Linc or Ldec
#	| lhsexp Ldec
#	;
postexp(p: ref Parser)
{
	lhsexp(p, 0);
	if(p.lastnl)
		return;
	op := look(p);
	if(op == Linc || op == Ldec){
		if(op == Linc)
			op = Lpostinc;
		else
			op = Lpostdec;
		lex(p);
		emit(p, op);
	}
}

#
# verify that the last expression is actually a lhsexp
#
checklhsexp(p: ref Parser, pc: int)
{

	case int p.code.ops[p.code.npc-1]{
	Lthis or
	')' or
	'.' or
	'[' or
	Lcall or
	Lnew or
	Lnewcall =>
		return;
	}

	case int p.code.ops[pc]{
	Lid or
	Lnum or
	Lstr =>
		npc := pc + 1;
		(npc, nil) = getconst(p.code.ops, npc);
		if(npc == p.code.npc)
			return;
	}

	(nil, e) := pexp(mkpprint(p.ex, p.code), pc, p.code.npc);
	error(p, "only left-hand-side expressions allowed: "+e);
}

#lhsexp	: newexp
#	| callexp
#	;
#callexp: memexp args
#	| callexp args
#	| callexp '[' exp ']'
#	| callexp '.' Lid
#	;
#newexp	: memexp
#	| Lnew newexp
#	;
#memexp	: primexp
#	| memexp '[' exp ']'
#	| memexp '.' Lid
#	| Lnew memexp args
#	;
lhsexp(p: ref Parser, hasnew: int): int
{
	a: int;
	if(look(p) == Lnew){
		lex(p);
		hasnew = lhsexp(p, hasnew + 1);
		if(hasnew){
			emit(p, Lnew);
			hasnew--;
		}
		return hasnew;
	}
	primexp(p);
	for(;;){
		op := look(p);
		if(op == '('){
			op = Lcall;
			if(hasnew){
				hasnew--;
				#
				# stupid different order of evaluation
				#
				emit(p, Lgetval);
				op = Lnewcall;
			}
			a = args(p);
			emitconst(p, op, a);
		}else if(op == '['){
			emit(p, Lgetval);
			lex(p);
			exp(p);
			mustbe(p, ']');
			emit(p, '[');
		}else if(op == '.'){
			lex(p);
			mustbe(p, Lid);
			emitconst(p, Lid, p.id);
			emit(p, '.');
		}else
			return hasnew;
	}
}

#primexp	: Lthis
#	| Lid
#	| Lnum
#	| Lstr
#	| '(' exp ')'
#	;
primexp(p: ref Parser)
{
	case t := lex(p){
	Lthis =>
		emit(p, t);
	Lid or
	Lnum or
	Lstr =>
		emitconst(p, t, p.id);
	'(' =>
		emit(p, '(');
		exp(p);
		mustbe(p, ')');
		emit(p, ')');
	* =>
		error(p, "expected an expression");
	}
}

#args	: '(' ')'
#	| '(' arglist ')'
#	;
#
#arglist	: asexp
#	| arglist ',' asexp
#	;
args(p: ref Parser): int
{
	mustbe(p, '(');
	if(look(p) == ')'){
		lex(p);
		return 0;
	}
	a := 0;
	for(;;){
		asexp(p);
		emit(p, Lgetval);
		a++;
		if(look(p) == ')'){
			lex(p);
			return a;
		}
		mustbe(p, ',');
	}
}

inops(tok: int, ops: array of int): int
{
	for(i := 0; i < len ops; i++)
		if(tok == ops[i])
			return i;
	return -1;
}

mustbe(p: ref Parser, t: int)
{
	tt := lex(p);
	if(tt != t)
		error(p, "expected "+tokname(t)+" found "+tokname(tt));
}

toknames := array[] of
{
	Leos-Lbase =>		"end of input",
	Landas-Lbase =>		"&=",
	Loras-Lbase =>		"|=",
	Lxoras-Lbase =>		"^=",
	Llshas-Lbase =>		"<<=",
	Lrshas-Lbase =>		">>=",
	Lrshuas-Lbase =>	">>>=",
	Laddas-Lbase =>		"+=",
	Lsubas-Lbase =>		"-=",
	Lmulas-Lbase =>		"*=",
	Ldivas-Lbase =>		"/=",
	Lmodas-Lbase =>		"%=",
	Loror-Lbase =>		"||",
	Landand-Lbase =>	"&&",
	Leq-Lbase =>		"==",
	Lneq-Lbase =>		"!=",
	Lleq-Lbase =>		"<=",
	Lgeq-Lbase =>		">=",
	Llsh-Lbase =>		"<<",
	Lrsh-Lbase =>		">>",
	Lrshu-Lbase =>		">>>",
	Linc-Lbase =>		"++",
	Ldec-Lbase =>		"--",
	Lnum-Lbase =>		"a number",
	Lid-Lbase =>		"an identifier",
	Lstr-Lbase =>		"a string",
	Lthis-Lbase =>		"this",
	Ltypeof-Lbase =>	"typeof",
	Ldelete-Lbase =>	"delete",
	Lvoid-Lbase =>		"void",
	Lwhile-Lbase =>		"while",
	Lfor-Lbase =>		"for",
	Lbreak-Lbase =>		"break",
	Lcontinue-Lbase =>	"continue",
	Lwith-Lbase =>		"with",
	Lreturn-Lbase =>	"return",
	Lfunction-Lbase =>	"function",
	Lvar-Lbase =>		"var",
	Lif-Lbase =>		"if",
	Lelse-Lbase =>		"else",
	Lin-Lbase =>		"in",
	Lnew-Lbase =>		"new",

	Lpreadd-Lbase =>	"+",
	Lpresub-Lbase =>	"-",
	Lpostinc-Lbase =>	"++",
	Lpostdec-Lbase =>	"--",
	Lcall-Lbase =>		"call",
	Lnewcall-Lbase =>	"newcall",
	Lgetval-Lbase =>	"[[GetValue]]",
	Las-Lbase =>		"[[as]]",
	Lasop-Lbase =>		"[[asop]]",
	Lforin-Lbase =>		"forin",
	Lforvar-Lbase =>	"forvar",
	Lforvarin-Lbase =>	"forvarin",
	Lcase-Lbase =>		"case",
	Lcatch-Lbase =>		"catch",
	Lclass-Lbase =>		"class",
	Lconst-Lbase =>		"const",
	Ldebugger-Lbase =>	"debugger",
	Ldefault-Lbase =>	"default",
	Ldo-Lbase =>		"do",
	Lenum-Lbase =>		"enum",
	Lexport-Lbase =>	"export",
	Lextends-Lbase =>	"extends",
	Lfinally-Lbase =>	"finally",
	Limport-Lbase =>	"import",
	Lsuper-Lbase =>		"super",
	Lswitch-Lbase =>	"switch",
	Lthrow-Lbase =>		"throw",
	Ltry-Lbase=>		"try",
};

tokname(t: int): string
{
	if(t < Lbase){
		s := "";
		s[0] = t;
		return s;
	}
	if(t-Lbase >= len toknames || toknames[t-Lbase] == "")
		return sprint("<%d>", t);
	return toknames[t-Lbase];
}

lexemit(p: ref Parser)
{
	emit(p, lex(p));
}

emit(p: ref Parser, t: int)
{
	if(t > 255)
		fatal(p.ex, sprint("emit too big: %d\n", t));
	if(p.code.npc >= len p.code.ops){
		ops := array[2 * len p.code.ops] of byte;
		ops[:] = p.code.ops;
		p.code.ops = ops;
	}
	p.code.ops[p.code.npc++] = byte t;
}

emitconst(p: ref Parser, op, c: int)
{
	emit(p, op);
	if(c < 0)
		fatal(p.ex, "emit negative constant");
	if(c >= 255){
		if(c >= 65536)
			fatal(p.ex, "constant too large");
		emit(p, 255);
		emit(p, c & 16rff);
		c >>= 8;
	}
	emit(p, c);
}

epatch(p: ref Parser): int
{
	pc := p.code.npc;
	emit(p, 0);
	emit(p, 0);
	return pc;
}

patch(p: ref Parser, pc: int)
{
	val := p.code.npc - pc;
	if(val >= 65536)
		fatal(p.ex, "patch constant too large");
	p.code.ops[pc] = byte val;
	p.code.ops[pc+1] = byte(val >> 8);
}

getconst(ops: array of byte, pc: int): (int, int)
{
	c := int ops[pc++];
	if(c == 255){
		c = int ops[pc] + (int ops[pc+1] << 8);
		pc += 2;
	}
	return (pc, c);
}

getjmp(ops: array of byte, pc: int): (int, int)
{
	c := int ops[pc] + (int ops[pc+1] << 8) + pc;
	pc += 2;
	return (pc, c);
}

mkcode(): ref Code
{
	return ref Code(array[16] of byte, 0, nil, nil, nil, nil);
}

look(p: ref Parser): int
{
	if(p.token == -1)
		p.token = lex(p);
	return p.token;
}

lex(p: ref Parser): int
{
	t := p.token;
	if(t != -1){
		p.token = -1;
		return t;
	}

	p.lastnl = 0;
	while(p.srci < p.esrc){
		c := p.src[p.srci++];
		case c{
		'\r' =>
			p.lastnl = 1;
		'\n' =>
			p.lineno++;
			p.lastnl = 1;
		' ' or
		'\t' or
		'\v' or
		'\u000c' =>	# form feed
			;
		'"' or
		'\''=>
			return lexstring(p, c);
		'(' or
		')' or
		'[' or
		']' or
		'{' or
		'}' or
		',' or
		';' or
		'~' or
		'?' or
		':' =>
			return c;
		'.' =>
			if(p.srci < p.esrc && (map[p.src[p.srci]] & Mdigit) != byte 0){
				p.srci--;
				return lexnum(p);
			}
			return '.';
		'^' =>
			if(p.srci < p.esrc && p.src[p.srci] == '='){
				p.srci++;
				return Lxoras;
			}
			return '^';
		'*' =>
			if(p.srci < p.esrc && p.src[p.srci] == '='){
				p.srci++;
				return Lmulas;
			}
			return '*';
		'%' =>
			if(p.srci < p.esrc && p.src[p.srci] == '='){
				p.srci++;
				return Lmodas;
			}
			return '%';
		'=' =>
			if(p.srci < p.esrc && p.src[p.srci] == '='){
				p.srci++;
				return Leq;
			}
			return '=';
		'!' =>
			if(p.srci < p.esrc && p.src[p.srci] == '='){
				p.srci++;
				return Lneq;
			}
			return '!';
		'+' =>
			if(p.srci < p.esrc){
				c = p.src[p.srci];
				if(c == '='){
					p.srci++;
					return Laddas;
				}
				if(c == '+'){
					p.srci++;
					return Linc;
				}
			}
			return '+';
		'-' =>
			if(p.srci < p.esrc){
				c = p.src[p.srci];
				if(c == '='){
					p.srci++;
					return Lsubas;
				}
				if(c == '-'){
					p.srci++;
					return Ldec;
				}
			}
			return '-';
		'|' =>
			if(p.srci < p.esrc){
				c = p.src[p.srci];
				if(c == '='){
					p.srci++;
					return Loras;
				}
				if(c == '|'){
					p.srci++;
					return Loror;
				}
			}
			return '|';
		'&' =>
			if(p.srci < p.esrc){
				c = p.src[p.srci];
				if(c == '='){
					p.srci++;
					return Landas;
				}
				if(c == '&'){
					p.srci++;
					return Landand;
				}
			}
			return '&';
		'/' =>
			if(p.srci < p.esrc){
				c = p.src[p.srci];
				if(c == '='){
					p.srci++;
					return Ldivas;
				}
				if(c == '/'){
					p.srci++;
					if(lexcom(p) < 0)
						return Leos;
					break;
				}
				if(c == '*'){
					p.srci++;
					if(lexmcom(p) < 0)
						return Leos;
					break;
				}
			}
			return '/';
		'>' =>
			if(p.srci < p.esrc){
				c = p.src[p.srci];
				if(c == '='){
					p.srci++;
					return Lgeq;
				}
				if(c == '>'){
					p.srci++;
					c = p.src[p.srci];
					if(c == '='){
						p.srci++;
						return Lrshas;
					}
					if(c == '>'){
						p.srci++;
						c = p.src[p.srci];
						if(c == '='){
							p.srci++;
							return Lrshuas;
						}
						return Lrshu;
					}
					return Lrsh;
				}
			}
			return '>';
		'<' =>
			if(p.srci < p.esrc){
				c = p.src[p.srci];
				if(c == '='){
					p.srci++;
					return Lleq;
				}
				if(c == '<'){
					p.srci++;
					c = p.src[p.srci];
					if(c == '='){
						p.srci++;
						return Llshas;
					}
					return Llsh;
				}
			}
			return '<';
		'0' to '9' =>
			p.srci--;
			return lexnum(p);
		* =>
			if((map[c] & Malpha) != byte 0)
				return lexid(p);
			s := "";
			s[0] = c;
			error(p, "unknown character '"+s+"'");
		}
	}
	return Leos;
}

#
# single line comment
#
lexcom(p: ref Parser): int
{
	while(p.srci < p.esrc){
		c := p.src[p.srci];
		if(c == '\n' || c == '\r')
			return 0;
		p.srci++;
	}
	return -1;
}

#
# multi-line comment
#
lexmcom(p: ref Parser): int
{
	star := 0;
	while(p.srci < p.esrc){
		c := p.src[p.srci++];
		if(c == '/' && star)
			return 0;
		star = c == '*';
	}
	return -1;
}

lexid(p: ref Parser): int
{
	srci := p.srci - 1;
	while(p.srci < p.esrc){
		c := p.src[p.srci];
		if(c >= 256
		|| (map[c] & (Malpha|Mdigit)) == byte 0)
			break;
		p.srci++;
	}
	id := p.src[srci:p.srci];
	t := keywdlook(id);
	if(t != -1)
		return t;
	p.id = strlook(p, id);
	return Lid;
}

ParseReal, ParseHex, ParseOct: con 1 << iota;

#
# parse a numeric identifier
# format [0-9]+(r[0-9A-Za-z]+)?
# or ([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?
#
lexnum(p: ref Parser): int
{
	v: real;
	(p.srci, v) = parsenum(p.ex, p.src, p.srci, ParseReal|ParseHex|ParseOct);
	p.id = numlook(p, v);
	return Lnum;
}

parsenum(ex: ref Exec, s: string, si, how: int): (int, real)
{
	Inf: con "Infinity";

	osi := si;
	lens := len s;
	while(si < lens && iswhite(s[si]))
		si++;
	if(si >= lens)
		return (osi, Math->NaN);
	c := s[si];
	neg := 0;
	if(c == '+')
		si++;
	else if(c == '-'){
		si++;
		neg = 1;
	}
	v := 0.;
	if((how & ParseReal) && si + len Inf <= lens && s[si:si+len Inf] == Inf){
		si += len Inf;
		v = Math->Infinity;
	}else{
		nsi := si;
		(si, v) = parsenumval(ex, s, si, how);
		if(si == nsi)
			return (osi, Math->NaN);
	}
	if(neg)
		v = -v;
	while(si < lens && iswhite(s[si]))
		si++;
	return (si, v);
}

#
# parse a bunch of difference subsets of numbers
#
parsenumval(ex: ref Exec, s: string, si, how: int): (int, real)
{
	Int, Oct, Hex, FracSeen, Frac, ExpSeen, ExpSignSeen, Exp: con iota;

	lens := len s;
	if(si >= lens)
		return (si, Math->NaN);
	ssi := si;
	c := s[si];
	state := Int;
	if(c == '.' && (how & ParseReal)){
		state = FracSeen;
		si++;
	}else if(c == '0'){
		if(si+1 >= lens)
			return (si+1, 0.);
		c = s[si+1];
		if(c == '.' && (how & ParseReal)){
			state = Frac;
			si += 2;
		}else if((c == 'x' || c == 'X') && (how & ParseHex)){
			state = Hex;
			ssi += 2;
			si += 2;
		}else if(how & ParseOct)
			state = Oct;
	}

done:	while(si < lens){
		c = s[si];
		case state{
		Int =>
			if((map[c] & Mdigit) != byte 0)
				break;
			if((map[c] & Mexp) != byte 0 && (how & ParseReal))
				state = ExpSeen;
			else if(c == '.' && (how & ParseReal))
				state = Frac;
			else
				break done;
		Hex =>
			if((map[c] & Mhex) == byte 0)
				break done;
		Oct =>
			if((map[c] & Moct) == byte 0)
				break done;
		FracSeen or
		Frac =>
			if((map[c] & Mdigit) != byte 0)
				state = Frac;
			else if((map[c] & Mexp) != byte 0)
				state = ExpSeen;
			else
				break done;
		ExpSeen =>
			if((map[c] & Msign) != byte 0)
				state = ExpSignSeen;
			else if((map[c] & Mdigit) != byte 0)
				state = Exp;
			else
				break done;
		ExpSignSeen or
		Exp =>
			if((map[c] & Mdigit) != byte 0)
				state = Exp;
			else
				break done;
		}
		si++;
	}

	esi := si;
	if(state == FracSeen)
		return (si - 1, Math->NaN);
	if(state == ExpSeen){
		state = Frac;
		esi--;
	}else if(state == ExpSignSeen){
		state = Frac;
		esi -= 2;
	}
	buf := s[ssi:esi];
	v: real;
	case state{
	* =>
		# only if the above lexing code is wrong
		fatal(ex, "bad parse of numerical constant '"+buf+"'");
		v = 0.;
	Oct =>
		v = strtoi(ex, buf, 8);
	Hex =>
		v = strtoi(ex, buf, 16);
	Int or
	Frac or
	Exp =>
		v = real buf;
	}
	return (si, v);
}

#
# called only from parsenumval
# can never fatal error if that routine works correctly
#
strtoi(ex: ref Exec, t: string, base: int): real
{
	if(len t == 0)
		return Math->NaN;

	v := 0.;
	for(i := 0; i < len t; i++){
		c := t[i];
		if(c >= '0' && c <= '9')
			c -= '0';
		else if(c >= 'a' && c <= 'z')
			c -= 'a' - 10;
		else
			c -= 'A' - 10;
		if(c >= base){
			fatal(ex, "digit '"+t[i:i+1]+"' is not radix "+string base);
			return Math->NaN;
		}
		v = v * real base + real c;
	}
	return v;
}

lexstring(p: ref Parser, end: int): int
{
	s := "";
	i := 0;
	for(;;){
		if(p.srci >= p.esrc){
			error(p, "end of file in string constant");
			break;
		}
		c := p.src[p.srci];
		if(c == '\n' || c == '\r'){
			error(p, "newline in string constant");
			break;
		}
		p.srci++;
		if(c == end)
			break;
		if(c == '\\'){
			c = escchar(p);
			if(c == Leos)
				continue;
		}
		s[i++] = c;
	}
	p.id = strlook(p, s);
	return Lstr;
}

escchar(p: ref Parser): int
{
	v: int;
	if(p.srci >= p.esrc)
		return Leos;
	c := p.src[p.srci++];
	if(c == 'u' || c == 'x'){
		d := 2;
		if(c == 'u')
			d = 4;
		v = 0;
		for(i := 0; i < d; i++){
			if(p.srci >= p.esrc || (map[c = p.src[p.srci]] & (Mdigit|Mhex)) == byte 0){
				error(p, "malformed hex escape sequence");
				break;
			}
			p.srci++;
			if((map[c] & Mdigit) != byte 0)
				c -= '0';
			else if((map[c] & Mlower) != byte 0)
				c = c - 'a' + 10;
			else if((map[c] & Mupper) != byte 0)
				c = c - 'A' + 10;
			v = v * 16 + c;
		}
		return v;
	}
	if(c >= '0' && c <= '7'){
		v = c - '0';
		if(p.srci < p.esrc && (c = p.src[p.srci]) >= '0' && c <= '7'){
			p.srci++;
			v = v * 8 + c - '0';
			if(v <= 8r37 && p.srci < p.esrc && (c = p.src[p.srci]) >= '0' && c <= '7'){
				p.srci++;
				v = v * 8 + c - '0';
			}
		}
		return v;
	}

	if(c < len escmap && (v = int escmap[c]) < 255)
		return v;
	return c;
}

keywdlook(s: string): int
{
	m: int;
	l := 1;
	r := len keywords - 1;
	while(l <= r){
		m = (r + l) >> 1;
		if(keywords[m].name <= s)
			l = m + 1;
		else
			r = m - 1;
	}
	m = l - 1;
	if(keywords[m].name == s)
		return keywords[m].token;
	return -1;
}

strlook(p: ref Parser, s: string): int
{
	for(i := 0; i < len p.code.strs; i++)
		if(p.code.strs[i] == s)
			return i;
	strs := array[i + 1] of string;
	strs[:] = p.code.strs;
	strs[i] = s;
	p.code.strs = strs;
	return i;
}

numlook(p: ref Parser, r: real): int
{
	for(i := 0; i < len p.code.nums; i++)
		if(p.code.nums[i] == r)
			return i;
	nums := array[i + 1] of real;
	nums[:] = p.code.nums;
	nums[i] = r;
	p.code.nums = nums;
	return i;
}

iswhite(c: int): int
{
	case c {
	'\r' or
	'\n' or
	' ' or
	'\t' or
	'\v' or
	'\u000c' =>	# form feed
		return 1;
	}
	return 0;
}

error(p: ref Parser, s: string)
{
	p.errors++;
	p.ex.error += sys->sprint("%d: syntax error: %s\n", p.lineno, s);
	if(p.errors >= maxerr)
		runtime(p.ex, p.ex.error);
}

fatal(ex: ref Exec, msg: string)
{
	if(debug['f']){
		print("fatal ecmascript error: %s\n", msg);
		if(""[5] == -1);	# abort
	}
	runtime(ex, "unrecoverable internal ecmascript error: "+ msg);
}
