#
# utility functions
#
biinst(o: ref Obj, bi: Builtin, p: ref Obj, h: ESHostobj): ref Obj
{
	bo := mkobj(p, "Function");
	bo.call = mkcall(nil, bi.params);
	bo.val = strval(bi.val);
	bo.host = h;
	varinstant(bo, DontEnum|DontDelete|ReadOnly, "length", ref RefVal(numval(real bi.length)));
	varinstant(o, DontEnum, bi.name, ref RefVal(objval(bo)));
	return bo;
}

biminst(o: ref Obj, bis: array of Builtin, p: ref Obj, h: ESHostobj)
{
	for(i := 0; i < len bis; i++)
		biinst(o, bis[i], p, h);
}

biarg(args: array of ref Val, i: int): ref Val
{
	if(i < len args)
		return args[i];
	return undefined;
}

#
# interface to builtin objects
#
get(ex: ref Ecmascript->Exec, o: ref Ecmascript->Obj, property: string): ref Ecmascript->Val
{
	return esget(ex, o, property, 1);
}

put(ex: ref Ecmascript->Exec, o: ref Ecmascript->Obj, property: string, val: ref Ecmascript->Val)
{
	return esput(ex, o, property, val, 1);
}

canput(ex: ref Ecmascript->Exec, o: ref Ecmascript->Obj, property: string): ref Ecmascript->Val
{
	return escanput(ex, o, property, 1);
}

hasproperty(ex: ref Ecmascript->Exec, o: ref Ecmascript->Obj, property: string): ref Ecmascript->Val
{
	return eshasproperty(ex, o, property, 1);
}

delete(ex: ref Ecmascript->Exec, o: ref Ecmascript->Obj, property: string)
{
	return esdelete(ex, o, property, 1);
}

defaultval(ex: ref Ecmascript->Exec, o: ref Ecmascript->Obj, tyhint: int): ref Ecmascript->Val
{
	return esdefaultval(ex, o, tyhint, 1);
}

call(ex: ref Ecmascript->Exec, f, this: ref Ecmascript->Obj, args: array of ref Ecmascript->Val): ref Ecmascript->Ref
{
	x, y: real;
	v: ref Val;

	if(this == nil)
		this = ex.global;
	if(f.host != me)
		return escall(ex, f, this, args);
	case f.val.str{
	"eval" =>
		v = ceval(ex, f, this, args);
	"parseInt" =>
		v = cparseInt(ex, f, this, args);
	"parseFloat" =>
		v = cparseFloat(ex, f, this, args);
	"escape" =>
		v = cescape(ex, f, this, args);
	"unescape" =>
		v = cunescape(ex, f, this, args);
	"isNaN" =>
		v = cisNaN(ex, f, this, args);
	"isFinite" =>
		v = cisFinite(ex, f, this, args);
	"Object" =>
		v = cobj(ex, f, this, args);
	"Object.prototype.toString" =>
		v = cobjprototoString(ex, f, this, args);
	"Object.prototype.valueOf" =>
		v = cobjprotovalueOf(ex, f, this, args);
	"Function" =>
		v = objval(nfunc(ex, f, args));
	"Function.Prototype" =>
		v = undefined;
	"Function.prototype.toString" =>
		v = cfuncprototoString(ex, f, this, args);
	"Array" =>
		v = objval(narray(ex, f, args));
	"Array.prototype.toString" =>
		v = carrayprototoString(ex, f, this, args);
	"Array.prototype.join" =>
		v = carrayprotojoin(ex, f, this, args);
	"Array.prototype.reverse" =>
		v = carrayprotoreverse(ex, f, this, args);
	"Array.prototype.sort" =>
		v = carrayprotosort(ex, f, this, args);
	"String" =>
		v = cstr(ex, f, this, args);
	"String.fromCharCode" =>
		v = cstrfromCharCode(ex, f, this, args);
	"String.prototype.toString" =>
		v = cstrprototoString(ex, f, this, args);
	"String.prototype.valueOf" =>
		v = cstrprototoString(ex, f, this, args);
	"String.prototype.charAt" =>
		v = cstrprotocharAt(ex, f, this, args);
	"String.prototype.charCodeAt" =>
		v = cstrprotocharCodeAt(ex, f, this, args);
	"String.prototype.indexOf" =>
		v = cstrprotoindexOf(ex, f, this, args);
	"String.prototype.lastIndexOf" =>
		v = cstrprotolastindexOf(ex, f, this, args);
	"String.prototype.split" =>
		v = cstrprotosplit(ex, f, this, args);
	"String.prototype.substring" =>
		v = cstrprotosubstring(ex, f, this, args);
	"String.prototype.toLowerCase" =>
		v = cstrprototoLowerCase(ex, f, this, args);
	"String.prototype.toUpperCase" =>
		v = cstrprototoUpperCase(ex, f, this, args);
# JavaScript 1.0
	"String.prototype.anchor" or
	"String.prototype.big" or
	"String.prototype.blink" or
	"String.prototype.bold" or
	"String.prototype.fixed" or
	"String.prototype.fontcolor" or
	"String.prototype.fontsize" or
	"String.prototype.italics" or
	"String.prototype.link" or
	"String.prototype.small" or
	"String.prototype.strike" or
	"String.prototype.sub" or
	"String.prototype.sup" =>
		s := toString(ex, objval(this));
		arg := toString(ex, biarg(args, 0));
		tag, endtag: string;
		case f.val.str{
		"String.prototype.anchor" =>
			tag = "<A NAME=\"" + arg + "\">";
			endtag = "</A>";
		"String.prototype.big" =>
			tag = "<BIG>";
			endtag = "</BIG>";
		"String.prototype.blink" =>
			tag = "<BLINK>";
			endtag = "</BLINK>";
		"String.prototype.bold" =>
			tag = "<B>";
			endtag = "</B>";
		"String.prototype.fixed" =>
			tag = "<TT>";
			endtag = "</TT>";
		"String.prototype.fontcolor" =>
			tag = "<FONT COLOR=\"" + arg + "\">";
			endtag = "</FONT>";
		"String.prototype.fontsize" =>
			tag = "<FONT SIZE=\"" + arg + "\">";
			endtag = "</FONT>";
		"String.prototype.italics" =>
			tag = "<I>";
			endtag = "</I>";
		"String.prototype.link" =>
			tag = "<A HREF=\"" + arg + "\">";
			endtag = "</A>";
		"String.prototype.small" =>
			tag = "<SMALL>";
			endtag = "</SMALL>";
		"String.prototype.strike" =>
			tag = "<STRIKE>";
			endtag = "</STRIKE>";
		"String.prototype.sub" =>
			tag = "<SUB>";
			endtag = "</SUB>";
		"String.prototype.sup" =>
			tag = "<SUP>";
			endtag = "</SUP>";
		}
		v = strval(tag + s + endtag);
	"Boolean" =>
		v = cbool(ex, f, this, args);
	"Boolean.prototype.toString" =>
		v = cboolprototoString(ex, f, this, args);
	"Boolean.prototype.valueOf" =>
		v = cboolprotovalueOf(ex, f, this, args);
	"Number" =>
		v = cnum(ex, f, this, args);
	"Number.prototype.toString" =>
		v = cnumprototoString(ex, f, this, args);
	"Number.prototype.valueOf" =>
		v = cnumprotovalueOf(ex, f, this, args);
	"Math.abs" or
	"Math.acos" or
	"Math.asin" or
	"Math.atan" or
	"Math.ceil" or
	"Math.cos" or
	"Math.exp" or
	"Math.floor" or
	"Math.log" or
	"Math.round" or
	"Math.sin" or
	"Math.sqrt" or
	"Math.tan" =>
		x = toNumber(ex, biarg(args, 0));
		case f.val.str{
		"Math.abs" =>
			if(x < 0.)
				x = -x;
			else if(x == 0.)
				x = 0.;
		"Math.acos" =>		x = math->acos(x);
		"Math.asin" =>		x = math->asin(x);
		"Math.atan" =>		x = math->atan(x);
		"Math.ceil" =>		x = math->ceil(x);
		"Math.cos" =>		x = math->cos(x);
		"Math.exp" =>		x = math->exp(x);
		"Math.floor" =>		x = math->floor(x);
		"Math.log" =>		x = math->log(x);
		"Math.round" =>		if((x == .0 && copysign(1., x) == -1.)
					|| (x < .0 && x >= -0.5))
						x = -0.;
					else
						x = math->floor(x+.5);
		"Math.sin" =>		x = math->sin(x);
		"Math.sqrt" =>		x = math->sqrt(x);
		"Math.tan" =>		x = math->tan(x);
		}
		v = numval(x);
	"Math.random" =>
		range := big 16r7fffffffffffffff;
		v = numval(real bigrand(range)/ real range);
	"Math.atan2" or
	"Math.max" or
	"Math.min" or
	"Math.pow" =>
		x = toNumber(ex, biarg(args, 0));
		y = toNumber(ex, biarg(args, 1));
		case f.val.str{
		"Math.atan2" =>
			x = math->atan2(x, y);
		"Math.max" =>
			if(x > y)
				;
			else if(x < y)
				x = y;
			else if(x == y){
				if(x == 0. && copysign(1., x) == -1. && copysign(1., y) == 1.)
					x = y;
			}else
				x = Math->NaN;
		"Math.min" =>
			if(x < y)
				;
			else if(x > y)
				x = y;
			else if(x == y){
				if(x == 0. && copysign(1., x) == 1. && copysign(1., y) == -1.)
					x = y;
			}else
				x = Math->NaN;
		"Math.pow" =>
			x = math->pow(x, y);
		}
		v = numval(x);
	"Date" =>
		v = cdate(ex, f, this, args);
	"Date.parse" =>
		v = cdateparse(ex, f, this, args);
	"Date.UTC" =>
		v = cdateUTC(ex, f, this, args);
	"Date.prototype.toString" or
	"Date.prototype.toLocaleString" =>
		v = cdateprototoString(ex, f, this, args);
	"Date.prototype.valueOf" or
	"Date.prototype.getTime" =>
		v = cdateprotovalueOf(ex, f, this, args);
	"Date.prototype.getYear" or
	"Date.prototype.getFullYear" or
	"Date.prototype.getMonth" or
	"Date.prototype.getDate" or
	"Date.prototype.getDay" or
	"Date.prototype.getHours" or
	"Date.prototype.getMinutes" or
	"Date.prototype.getSeconds" =>
		v = cdateprotoget(ex, f, this, args, !UTC);
	"Date.prototype.getUTCFullYear" or
	"Date.prototype.getUTCMonth" or
	"Date.prototype.getUTCDate" or
	"Date.prototype.getUTCDay" or
	"Date.prototype.getUTCHours" or
	"Date.prototype.getUTCMinutes" or
	"Date.prototype.getUTCSeconds" =>
		v = cdateprotoget(ex, f, this, args, UTC);
	"Date.prototype.getMilliseconds" or
	"Date.prototype.getUTCMilliseconds" =>
		v = cdateprotogetMilliseconds(ex, f, this, args);
	"Date.prototype.getTimezoneOffset" =>
		v = cdateprotogetTimezoneOffset(ex, f, this, args);
	"Date.prototype.setTime" =>
		v = cdateprotosetTime(ex, f, this, args);
	"Date.prototype.setMilliseconds" =>
		v = cdateprotosetMilliseconds(ex, f, this, args, !UTC);
	"Date.prototype.setUTCMilliseconds" =>
		v = cdateprotosetMilliseconds(ex, f, this, args, UTC);
	"Date.prototype.setSeconds" =>
		v = cdateprotosetSeconds(ex, f, this, args, !UTC);
	"Date.prototype.setUTCSeconds" =>
		v = cdateprotosetSeconds(ex, f, this, args, UTC);
	"Date.prototype.setMinutes" =>
		v = cdateprotosetMinutes(ex, f, this, args, !UTC);
	"Date.prototype.setUTCMinutes" =>
		v = cdateprotosetMinutes(ex, f, this, args, UTC);
	"Date.prototype.setHours" =>
		v = cdateprotosetHours(ex, f, this, args, !UTC);
	"Date.prototype.setUTCHours" =>
		v = cdateprotosetHours(ex, f, this, args, UTC);
	"Date.prototype.setDate" =>
		v = cdateprotosetDate(ex, f, this, args, !UTC);
	"Date.prototype.setUTCDate" =>
		v = cdateprotosetDate(ex, f, this, args, UTC);
	"Date.prototype.setMonth" =>
		v = cdateprotosetMonth(ex, f, this, args, !UTC);
	"Date.prototype.setUTCMonth" =>
		v = cdateprotosetMonth(ex, f, this, args, UTC);
	"Date.prototype.setFullYear" =>
		v = cdateprotosetFullYear(ex, f, this, args, !UTC);
	"Date.prototype.setUTCFullYear" =>
		v = cdateprotosetFullYear(ex, f, this, args, UTC);
	"Date.prototype.setYear" =>
		v = cdateprotosetYear(ex, f, this, args);
	"Date.prototype.toUTCString" or
	"Date.prototype.toGMTString" =>
		v = cdateprototoUTCString(ex, f, this, args);
	* =>
		v = nil;
	}
	if(v == nil)
		runtime(ex, "unknown function "+f.val.str+" in builtin call");
	return valref(v);
}

rsalt := big 12345678;

randinit(seed: big)
{
	rsalt = big seed;
	bigrand(big 1);
	bigrand(big 1);
}

RANDMASK: con (big 1<<63)-(big 1);

bigrand(modulus: big): big
{
	rsalt = rsalt * big 1103515245 + big 12345;
	if(modulus <= big 0)
		return big 0;
	return ((rsalt&RANDMASK)>>10) % modulus;
}

construct(ex: ref Ecmascript->Exec, f: ref Ecmascript->Obj, args: array of ref Ecmascript->Val): ref Ecmascript->Obj
{
	if(f.host != me)
		runtime(ex, "ecmascript builtin called incorrectly");
	case f.val.str{
	"Object" =>
		return nobj(ex, f, args);
	"Function" =>
		return nfunc(ex, f, args);
	"Array" =>
		return narray(ex, f, args);
	"String" or
	"Boolean" or
	"Number" =>
		return coerceToObj(ex, call(ex, f, nil, args).val).obj;
	"Date" =>
		return ndate(ex, f, args);
	}
	runtime(ex, "unknown constructor "+f.val.str+" in builtin construct");
	return nil;
}

ceval(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	if(len args < 1)
		return undefined;
	vs := coerceToVal(args[0]);
	if(!isstr(vs))
		return args[0];
	(k, v) := eval(ex, vs.str);
	if(k != CNormal || v == nil)
		v = undefined;
	return v;
}

cparseInt(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	sv := biarg(args, 0);
	s := toString(ex, sv);
	neg := 0;
	i := 0;
	if(len s >= i){
		if(s[i] == '-'){
			neg = 1;
			i++;
		}else if(s[i] == '+')
			i++;
	}
	rv := biarg(args, 1);
	if(rv == undefined)
		r := big 0;
	else
		r = big toInt32(ex, rv);
	if(r == big 0){
		if(len s > i && s[i] == '0'){
			r = big 8;
			if(len s >= i+2 && (s[i+1] == 'x' || s[i+1] == 'X'))
				r = big 16;
		}else
			r = big 10;
	}else if(r < big 0 || r > big 36)
		return numval(Math->NaN);
	if(r == big 16 && len s >= i+2 && s[i] == '0' && (s[i+1] == 'x' || s[i+1] == 'X'))
		i += 2;
	ok := 0;
	n := big 0;
	for(; i < len s; i++) {
		c := s[i];
		v := r;
		case c {
		'a' to 'z' =>
			v = big(c - 'a' + 10);
		'A' to 'Z' =>
			v = big(c - 'A' + 10);
		'0' to '9' =>
			v = big(c - '0');
		}
		if(v >= r)
			break;
		ok = 1;
		n = n * r + v;
	}
	if(!ok)
		return numval(Math->NaN);
	if(neg)
		n = -n;
	return numval(real n);
}

cparseFloat(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := toString(ex, biarg(args, 0));
	(nil, r) := parsenum(ex, s, 0, ParseReal);
	return numval(r);
}

cescape(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := toString(ex, biarg(args, 0));
	t := "";
	for(i := 0; i < len s; i++){
		c := s[i];
		case c{
		'A' to 'Z' or
		'a' to 'z' or
		'0' to '9' or
		'@' or '*' or '_' or '+' or '-' or '.' or '/' =>
			t[len t] = s[i];
		* =>
			e := "";
			do{
				d := c & 16rf;
				e = "0123456789abcdef"[d:d+1] + e;
				c >>= 4;
			}while(c);
			if(len e & 1)
				e = "0" + e;
			if(len e == 4)
				e = "u" + e;
			t += "%" + e;
		}
	}
	return strval(t);
}

cunescape(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := toString(ex, biarg(args, 0));
	t := "";
	for(i := 0; i < len s; i++){
		c := s[i];
		if(c == '%'){
			if(i + 5 < len s && s[i+1] == 'u'){
				(v, e) := str->toint(s[i+2:i+6], 16);
				if(e == ""){
					c = v;
					i += 5;
				}
			}else if(i + 2 < len s){
				(v, e) := str->toint(s[i+1:i+3], 16);
				if(e == ""){
					c = v;
					i += 2;
				}
			}
		}
		t[len t] = c;
	}
	return strval(t);
}

cisNaN(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	if(math->isnan(toNumber(ex, biarg(args, 0))))
		return true;
	return false;
}

cisFinite(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	r := toNumber(ex, biarg(args, 0));
	if(math->isnan(r) || r == +Infinity || r == -Infinity)
		return false;
	return true;
}

cobj(ex: ref Exec, f, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	o: ref Obj;

	v := biarg(args, 0);

	if(isnull(v) || isundefined(v))
		o = nobj(ex, f, args);
	else
		o = toObject(ex, v);
	return objval(o);
}

nobj(ex: ref Exec, nil: ref Ecmascript->Obj, args: array of ref Val): ref Ecmascript->Obj
{
	o: ref Obj;

	TUndef, TNull, TBool, TNum, TStr, TObj, NoHint: con iota;
	v := biarg(args, 0);

	case v.ty{
	TNull or TUndef =>
		o = mkobj(ex.objproto, "Object");
	TBool =>
		o = mkobj(ex.boolproto, "Boolean");
		o.val = v;
	TStr =>
		o = mkobj(ex.strproto, "String");
		o.val = v;
		varinstant(o, DontEnum|DontDelete|ReadOnly, "length", ref RefVal(numval(real len v.str)));
	TNum =>
		o = mkobj(ex.numproto, "Number");
		o.val = v;
	TObj =>
		o = v.obj;
	* =>
		runtime(ex, "unknown type in Object constructor");
	}
	return o;
}

cobjprototoString(nil: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	return strval("[object " + this.class + "]");
}

cobjprotovalueOf(nil: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	return objval(this);
}

nfunc(ex: ref Exec, nil: ref Ecmascript->Obj, args: array of ref Val): ref Ecmascript->Obj
{
	params := "";
	body := "";
	sep := "";
	for(i := 0; i < len args - 1; i++){
		params += sep + toString(ex, args[i]);
		sep = ",";
	}
	if(i < len args)
		body = toString(ex, args[i]);

	p := mkparser(ex, "function anonymous("+params+"){"+body+"}");
	fundecl(ex, p);
	if(p.errors)
		runtime(ex, ex.error);
	if(p.code.vars[0].name != "anonymous")
		runtime(ex, "parse failure");
	return p.code.vars[0].val.val.obj;
}

cfuncprototoString(ex: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	if(this.call == nil)
		runtime(ex, "Function.prototype.toString called for a non-Function object");
	return strval(funcprint(ex, this));
}

narray(ex: ref Exec, nil: ref Ecmascript->Obj, args: array of ref Val): ref Ecmascript->Obj
{
	o := mkobj(ex.arrayproto, "Array");
	length := big len args;
	if(length == big 1 && isnum(coerceToVal(args[0]))){
		length = toUint32(ex, args[0]);
		varinstant(o, DontEnum|DontDelete, "length", ref RefVal(numval(real length)));
	}else{
		varinstant(o, DontEnum|DontDelete, "length", ref RefVal(numval(real length)));
		for(i := 0; i < len args; i++)
			esput(ex, o, string i, args[i], 0);
	}

	return o;
}

carrayprototoString(ex: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	return carrayprotojoin(ex, nil, this, nil);
}

carrayprotojoin(ex: ref Exec, nil, this: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	length := toUint32(ex, esget(ex, this, "length", 0));
	sepv := biarg(args, 0);
	sep := ",";
	if(sepv != undefined)
		sep = toString(ex, sepv);
	s := "";
	ss := "";
	for(i := big 0; i < length; i++){
		tv := esget(ex, this, string i, 0);
		t := "";
		if(tv != undefined && tv != null)
			t = toString(ex, tv);
		s += ss + t;
		ss = sep;
	}
	return strval(s);
}

carrayprotoreverse(ex: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	length := toUint32(ex, esget(ex, this, "length", 0));
	mid := length / big 2;
	for(i := big 0; i < mid; i++){
		i1 := string i;
		v1 := esget(ex, this, i1, 0);
		i2 := string(length - i - big 1);
		v2 := esget(ex, this, i2, 0);
		if(v2 == undefined)
			esdelete(ex, this, i1, 0);
		else
			esput(ex, this, i1, v2, 0);
		if(v1 == undefined)
			esdelete(ex, this, i2, 0);
		else
			esput(ex, this, i2, v1, 0);
	}
	return objval(this);
}

carrayprotosort(ex: ref Exec, nil, this: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	length := toUint32(ex, esget(ex, this, "length", 0));
	cmp := biarg(args, 0);
	if(cmp == undefined)
		cmp = nil;
	else if(!isobj(cmp) || cmp.obj.call == nil)
		runtime(ex, "Array.prototype.sort argument is not a function");

	#
	# shell sort
	#
	for(m := (length+big 3)/big 5; m > big 0; m = (m+big 1)/big 3){
		for(i := length-m; i-- != big 0;){
			v1, v2 : ref Val = nil;
			ji := big -1;
			for(j := i+m; j < length; j += m){
				if(v1 == nil)
					v1 = esget(ex, this, string(j-m), 0);
				v2 = esget(ex, this, string(j), 0);
				cr : real;
				if(v1 == undefined && v2 == undefined)
					cr = 0.;
				else if(v1 == undefined)
					cr = 1.;
				else if(v2 == undefined)
					cr = -1.;
				else if(cmp == nil){
					s1 := toString(ex, v1);
					s2 := toString(ex, v2);
					if(s1 < s2)
						cr = -1.;
					else if(s1 > s2)
						cr = 1.;
					else
						cr = 0.;
				}else{
					#
					# this value not specified by docs
					#
					cr = toNumber(ex, getValue(ex, escall(ex, cmp.obj, this, array[] of {v1, v2})));
				}
				if(cr <= 0.)
					break;
				if(v2 == undefined)
					esdelete(ex, this, string(j-m), 0);
				else
					esput(ex, this, string(j-m), v2, 0);
				ji = j;
			}
			if(ji != big -1){
				if(v1 == undefined)
					esdelete(ex, this, string(ji), 0);
				else
					esput(ex, this, string(ji), v1, 0);
			}
		}
	}
	return objval(this);
}

cstr(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := "";
	if(len args > 0)
		s = toString(ex, biarg(args, 0));
	return strval(s);
}

cstrfromCharCode(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := "";
	for(i := 0; i < len args; i++)
		s[i] = toUint16(ex, args[i]);
	return strval(s);
}

cstrprototoString(ex: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	if(!isstrobj(this))
		runtime(ex, "String.prototype.toString called on non-String object");
	return this.val;
}

cstrprotocharAt(ex: ref Exec, nil, this: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := toString(ex, objval(this));
	rpos := toInteger(ex, biarg(args, 0));
	if(rpos < 0. || rpos >= real len s)
		s = "";
	else{
		pos := int rpos;
		s = s[pos: pos+1];
	}
	return strval(s);
}

cstrprotocharCodeAt(ex: ref Exec, nil, this: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := toString(ex, objval(this));
	rpos := toInteger(ex, biarg(args, 0));
	if(rpos < 0. || rpos >= real len s)
		c := Math->NaN;
	else
		c = real s[int rpos];
	return numval(c);
}

cstrprotoindexOf(ex: ref Exec, nil, this: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := toString(ex, objval(this));
	t := toString(ex, biarg(args, 0));
	rpos := toInteger(ex, biarg(args, 1));
	if(rpos < 0.)
		rpos = 0.;
	else if(rpos > real len s)
		rpos = real len s;
	lent := len t;
	stop := len s - lent;
	for(i := int rpos; i <= stop; i++)
		if(s[i:i+lent] == t)
			break;
	if(i > stop)
		i = -1;
	return numval(real i);
}

cstrprotolastindexOf(ex: ref Exec, nil, this: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := toString(ex, objval(this));
	t := toString(ex, biarg(args, 0));
	v := biarg(args, 1);
	rpos := toNumber(ex, v);
	if(math->isnan(rpos))
		rpos = Math->Infinity;
	else
		rpos = toInteger(ex, v);
	if(rpos < 0.)
		rpos = 0.;
	else if(rpos > real len s)
		rpos = real len s;
	lent := len t;
	i := len s - lent;
	if(i > int rpos)
		i = int rpos;
	for(; i >= 0; i--)
		if(s[i:i+lent] == t)
			break;
	return numval(real i);
}

cstrprotosplit(ex: ref Exec, nil, this: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := toString(ex, objval(this));
	a := narray(ex, nil, nil);
	tv := biarg(args, 0);
	ai := 0;
	if(tv == undefined)
		esput(ex, a, string ai, strval(s), 0);
	else{
		t := toString(ex, tv);
		lent := len t;
		stop := len s - lent;
		pos := 0;
		if(lent == 0){
			for(; pos < stop; pos++)
				esput(ex, a, string ai++, strval(s[pos:pos+1]), 0);
		}else{
			for(k := pos; k <= stop; k++){
				if(s[k:k+lent] == t){
					esput(ex, a, string ai++, strval(s[pos:k]), 0);
					pos = k + lent;
					k = pos - 1;
				}
			}
			esput(ex, a, string ai, strval(s[pos:k]), 0);
		}
	}
	return objval(a);
}

cstrprotosubstring(ex: ref Exec, nil, this: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	s := toString(ex, objval(this));
	rstart := toInteger(ex, biarg(args, 0));
	lens := real len s;
	rend := lens;
	if(len args >= 2)
		rend = toInteger(ex, biarg(args, 1));
	if(rstart < 0.)
		rstart = 0.;
	else if(rstart > lens)
		rstart = lens;
	if(rend < 0.)
		rend = 0.;
	else if(rend > lens)
		rend = lens;
	if(rstart > rend){
		lens = rstart;
		rstart = rend;
		rend = lens;
	}
	return strval(s[int rstart: int rend]);
}

# this doesn't use unicode tolower
cstrprototoLowerCase(ex: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	s := toString(ex, objval(this));
	for(i := 0; i < len s; i++)
		s[i] = tolower(s[i]);
	return strval(s);
}

#this doesn't use unicode toupper
cstrprototoUpperCase(ex: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	s := toString(ex, objval(this));
	for(i := 0; i < len s; i++)
		s[i] = toupper(s[i]);
	return strval(s);
}

cbool(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	return toBoolean(ex, biarg(args, 0));
}

tolower(c: int): int
{
	if(c >= 'A' && c <= 'Z')
		return c - 'A' + 'a';
	return c;
}

toupper(c: int): int
{
	if(c >= 'a' && c <= 'a')
		return c - 'a' + 'A';
	return c;
}

cboolprototoString(ex: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	if(!isboolobj(this))
		runtime(ex, "Boolean.prototype.toString called on non-Boolean object");
	return strval(toString(ex, this.val));
}

cboolprotovalueOf(ex: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	if(!isboolobj(this))
		runtime(ex, "Boolean.prototype.valueOf called on non-Boolean object");
	return this.val;
}

cnum(ex: ref Exec, nil, nil: ref Ecmascript->Obj, args: array of ref Val): ref Val
{
	r := 0.;
	if(len args > 0)
		r = toNumber(ex, biarg(args, 0));
	return numval(r);
}

cnumprototoString(ex: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	if(!isnumobj(this))
		runtime(ex, "Number.prototype.toString called on non-Number object");
	return this.val;
}

cnumprotovalueOf(ex: ref Exec, nil, this: ref Ecmascript->Obj, nil: array of ref Val): ref Val
{
	if(!isnumobj(this))
		runtime(ex, "Number.prototype.valueOf called on non-Number object");
	return strval(toString(ex, this.val));
}
