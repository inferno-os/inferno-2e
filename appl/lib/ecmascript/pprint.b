PPrint: adt
{
	ex:	ref Exec;
	code:	ref Code;
	stack:	array of string;
	sp:	int;
};

mkpprint(ex: ref Exec, code: ref Code): ref PPrint
{
	return ref PPrint(ex, code, array[4] of string, 0);
}

funcprint(ex: ref Exec, func: ref Ecmascript->Obj): string
{
	params := func.call.params;
	(nil, name) := str->splitr(func.val.str, ".");
	s := "function " + name + "(";
	sep := "";
	for(i := 0; i < len params; i++){
		s += sep + params[i];
		sep = ", ";
	}
	s += "){";
	if(func.host != nil)
		s += "[host code]";
	else
		s += "\n" + pprint(ex, func.call.code, "	");
	s += "}";
	return s;
}

pprint(ex: ref Exec, code: ref Code, indent: string): string
{
	pp := ref PPrint(ex, code, array[4] of string, 0);

	s := pstmt(pp, 0, code.npc, indent);

	if(pp.sp != 0)
		fatal(ex, "pprint stack not balanced");

	return s;
}

pstmt(pp: ref PPrint, pc, epc: int, indent: string): string
{
	e, e1, e2: string;
	apc: int;

	code := pp.code;
	s := "";
	while(pc < epc){
		s += indent;
		case op := int code.ops[pc++]{
		Lbreak or
		Lcontinue or
		Lreturn =>
			s += tokname(op);
			if(op == Lreturn){
				(pc, e) = pexp(pp, pc, code.npc);
				s += " " + e;
			}
			s += ";\n";
		Lif or
		Lwith or
		Lwhile =>
			(pc, apc) = getjmp(code.ops, pc);
			(pc, e) = pexp(pp, pc, apc);
			(pc, apc) = getjmp(code.ops, pc);
			s += tokname(op) + "(" + e + "){\n";
			s += pstmt(pp, pc, apc, indent+"	");
			if(op == Lif){
				(pc, apc) = getjmp(code.ops, apc);
				if(pc != apc)
					s += indent + "}else{\n";
				s += pstmt(pp, pc, apc, indent+"	");
			}
			s += indent + "}\n";
			pc = apc;
		Lfor or
		Lforvar or
		Lforin or
		Lforvarin =>
			(pc, apc) = getjmp(code.ops, pc);
			(pc, e) = pexp(pp, pc, apc);
			(pc, apc) = getjmp(code.ops, pc);
			(pc, e1) = pexp(pp, pc, apc);
			s += "for(";
			if(op == Lforvar || op == Lforvarin)
				s += "var ";
			s += e;
			if(op == Lfor || op == Lforvar){
				(pc, apc) = getjmp(code.ops, pc);
				(pc, e2) = pexp(pp, pc, apc);
				s += "; " + e1 + "; " + e2;
			}else
				s += " in " + e1;
			s += "){\n";
			(pc, apc) = getjmp(code.ops, pc);
			s += pstmt(pp, pc, apc, indent+"	");
			s += indent + "}\n";
			pc = apc;
		';' =>
			s += ";\n";
		Lvar =>
			(pc, apc) = getjmp(code.ops, pc);
			(pc, e) = pexp(pp, pc, apc);
			s += "var " + e + ";\n";
		* =>
			(pc, e) = pexp(pp, pc-1, code.npc);
			s += e + ";\n";
		}
	}
	return s;
}

pexp(pp: ref PPrint, pc, epc: int): (int, string)
{
	c, apc: int;
	s, f, a, a1, a2: string;

	code := pp.code;
	savesp := pp.sp;
out:	while(pc < epc){
		case op := int code.ops[pc++]{
		Lthis =>
			s = "this";
		Lid or
		Lnum or
		Lstr =>
			(pc, c) = getconst(code.ops, pc);
			if(op == Lnum)
				s = string code.nums[c];
			else{
				s = code.strs[c];
				if(op == Lstr)
					s = "\""+escstr(code.strs[c])+"\"";
			}
		'*' or
		'/' or
		'%' or
		'+' or
		'-' or
		Llsh or
		Lrsh or
		Lrshu or
		'<' or
		'>' or
		Lleq or
		Lgeq or
		Leq or
		Lneq or
		'&' or
		'^' or
		'|' or
		'=' or
		'.' or
		',' or
		'[' =>
			a2 = ppop(pp);
			a1 = ppop(pp);
			s = tokname(op);
			if(a1[0] == '='){
				s += "=";
				a1 = a1[1:];
			}
			if(op == '[')
				s = a1 + "[" + a2 + "]";
			else{
				if(op != '.'){
					if(op != ',')
						s = " " + s;
					s = s + " ";
				}
				s = a1 + s + a2;
			}
		Ltypeof or
		Ldelete or
		Lvoid or
		Lnew or
		Linc or
		Ldec or
		Lpreadd or
		Lpresub or
		'~' or
		'!' or
		Lpostinc or
		Lpostdec =>
			a = ppop(pp);
			s = tokname(op);
			if(op == Lpostinc || op == Lpostdec)
				s = a + s;
			else{
				if(op == Ltypeof || op == Ldelete || op == Lvoid || op == Lnew)
					s += " ";
				s += a;
			}
		'(' =>
			s = "(";
		')' =>
			s = ppop(pp);
			if(ppop(pp) != "(")
				fatal(pp.ex, "unbalanced () in pexp");
			s = "(" + s + ")";
		Lgetval or
		Las =>
			continue;
		Lasop =>
			s = "=" + ppop(pp);
		Lcall or
		Lnewcall =>
			(pc, c) = getconst(code.ops, pc);
			a = "";
			sep := "";
			for(sp := pp.sp-c; sp < pp.sp; sp++){
				a += sep + pp.stack[sp];
				sep = ", ";
			}
			pp.sp -= c;
			f = ppop(pp);
			if(op == Lnewcall)
				f = "new " + f;
			s = f + "(" + a + ")";
		';' =>
			break out;
		Landand or
		Loror or
		'?' =>
			s = ppop(pp);
			(pc, apc) = getjmp(code.ops, pc);
			(pc, a1) = pexp(pp, pc, apc);
			s += " " + tokname(op) + " " + a1;
			if(op == '?'){
				(pc, apc) = getjmp(code.ops, pc);
				(pc, a2) = pexp(pp, pc, apc);
				s += " : "+ a2;
			}
		* =>
			fatal(pp.ex, "pexp: unknown op " + tokname(op));
		}
		ppush(pp, s);
	}

	if(savesp == pp.sp)
		return (pc, "");

	if(savesp != pp.sp-1)
		fatal(pp.ex, "unbalanced stack in pexp");
	return (pc, ppop(pp));
}

ppush(pp: ref PPrint, s: string)
{
	if(pp.sp >= len pp.stack){
		st := array[2 * len pp.stack] of string;
		st[:] = pp.stack;
		pp.stack = st;
	}
	pp.stack[pp.sp++] = s;
}

ppop(pp: ref PPrint): string
{
	if(pp.sp == 0)
		fatal(pp.ex, "popping too far off the pstack");
	return pp.stack[--pp.sp];
}

unescmap :=	array[128] of 
{
	'\'' =>		byte '\'',
	'"' =>		byte '"',
	'\\' =>		byte '\\',
	'\b' =>		byte 'b',
	'\u000c' =>	byte 'f',
	'\n' =>		byte 'n',
	'\r' =>		byte 'r',
	'\t' =>		byte 't',

	* =>		byte 0
};

escstr(s: string): string
{
	n := len s;
	sb := "";
	for(i := 0; i < n; i++){
		c := s[i];
		if(c < 128 && (e := int unescmap[c])){
			sb[len sb] = '\\';
			sb[len sb] = e;
		}else if(c > 128 || c < 32){
			sb += "\\u0000";
			for(j := 1; j <= 4; j++){
				sb[len sb - j] = "0123456789abcdef"[c & 16rf];
				c >>= 4;
			}
		}else
			sb[len sb] = c;
	}
	return sb;
}
