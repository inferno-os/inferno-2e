exec(ex: ref Exec, code: ref Code): Completion
{
	ssp := ex.sp;

	r := estmt(ex, code, 0, code.npc);

	if(ssp != ex.sp)
		runtime(ex, "internal error: exec stack not balanced");

	return r;
}

estmt(ex: ref Exec, code: ref Code, pc, epc: int): Completion
{
	e: ref Ref;
	ev: ref Val;
	k, apc, pc2, apc2, pc3, apc3: int;

	v : ref Val = nil;

	k1 := CNormal;
	while(pc < epc){
		v1 : ref Val = nil;

		op := int code.ops[pc++];
		if(debug['e'] > 1)
			print("estmt(pc %d, sp %d) %s\n", pc-1, ex.sp, tokname(op));
		case op {
		Lbreak =>
			return (CBreak, v);
		Lcontinue =>
			return (CContinue, v);
		Lreturn =>
			(pc, v) = eexpval(ex, code, pc, code.npc);
			return (CReturn, v);
		Lif =>
			(pc, apc) = getjmp(code.ops, pc);
			(pc, ev) = eexpval(ex, code, pc, apc);
			(pc, apc) = getjmp(code.ops, pc);
			(pc2, apc2) = getjmp(code.ops, apc);
			if(toBoolean(ex, ev) != false)
				(k1, v1) = estmt(ex, code, pc, apc);
			else if(pc2 != apc2)
				(k1, v1) = estmt(ex, code, pc2, apc2);
			pc = apc2;
		Lwhile =>
			(pc, apc) = getjmp(code.ops, pc);
			(pc2, apc2) = getjmp(code.ops, apc);
			for(;;){
				(nil, ev) = eexpval(ex, code, pc, apc);
				if(toBoolean(ex, ev) == false)
					break;
				(k, v1) = estmt(ex, code, pc2, apc2);
				if(v1 != nil)
					v = v1;
				if(k == CBreak)
					break;
				if(k == CReturn)
					return (k, v1);
			}
			pc = apc2;
		Lfor or
		Lforvar =>
			(pc, apc) = getjmp(code.ops, pc);
			(pc, nil) = eexpval(ex, code, pc, apc);
			(pc, apc) = getjmp(code.ops, pc);
			(pc2, apc2) = getjmp(code.ops, apc);
			(pc3, apc3) = getjmp(code.ops, apc2);
			for(;;){
				(nil, e) = eexp(ex, code, pc, apc);
				if(e != nil && toBoolean(ex, getValue(ex, e)) == false)
					break;
				(k, v1) = estmt(ex, code, pc3, apc3);
				if(v1 != nil)
					v = v1;
				if(k == CBreak)
					break;
				if(k == CReturn)
					return (k, v1);
				eexpval(ex, code, pc2, apc2);
			}
			pc = apc3;
		Lforin or
		Lforvarin =>
			(pc, apc) = getjmp(code.ops, pc);
			(pc2, apc2) = getjmp(code.ops, apc);
			(pc3, apc3) = getjmp(code.ops, apc2);
			if(op == Lforvarin){
				(nil, nil) = eexp(ex, code, pc, apc);
				# during for only evaluate the id, not the initializer
				apc = pc + 1;
			}
			(nil, ev) = eexpval(ex, code, pc2, apc2);
			bo := toObject(ex, ev);

			#
			# note this won't enumerate host properties
			#
			enum:
			for(o := bo; o != nil; o = o.prototype){
				if(o.host != nil && o.host != me)
					continue;
				for(i := 0; i < len o.props; i++){
					if(o.props[i] == nil
					|| (o.props[i].attr & DontEnum)
					|| propshadowed(bo, o, o.props[i].name))
						continue;
					(nil, e) = eexp(ex, code, pc, apc);
					putValue(ex, e, strval(o.props[i].name));
					(k, v1) = estmt(ex, code, pc3, apc3);
					if(v1 != nil)
						v = v1;
					if(k == CBreak)
						break enum;
					if(k == CReturn)
						return (k, v1);
				}
			}
			pc = apc3;
		Lwith =>
			(pc, apc) = getjmp(code.ops, pc);
			(pc, ev) = eexpval(ex, code, pc, apc);
			pushscope(ex, toObject(ex, ev));
			(pc, apc) = getjmp(code.ops, pc);
			(k1, v1) = estmt(ex, code, pc, apc);
			popscope(ex);
			pc = apc;
		';' =>
			;
		Lvar =>
			(pc, apc) = getjmp(code.ops, pc);
			(pc, nil) = eexp(ex, code, pc, apc);
		* =>
			(pc, e) = eexp(ex, code, pc-1, code.npc);
			if(e != nil)
				v1 = getValue(ex, e);
			if(debug['v'])
				print("%s\n", toString(ex, v1));
		}

		if(v1 != nil)
			v = v1;
		if(k1 != CNormal)
			return (k1, v);
	}
	return (CNormal, v);
}

eexpval(ex: ref Exec, code: ref Code, pc, epc: int): (int, ref Val)
{
	e: ref Ref;

	(pc, e) = eexp(ex, code, pc, epc);
	if(e == nil)
		v := undefined;
	else
		v = getValue(ex, e);
	return (pc, v);
}

eexp(ex: ref Exec, code: ref Code, pc, epc: int): (int, ref Ref)
{
	o, th: ref Obj;
	a1: ref Ref;
	v, v1, v2: ref Val;
	s: string;
	r1, r2: real;
	c, apc, i1, i2: int;

	savesp := ex.sp;
out:	while(pc < epc){
		op := int code.ops[pc++];
		if(debug['e'] > 1){
			case op{
			Lid or
			Lstr =>
				(nil, c) = getconst(code.ops, pc);
				print("eexp(pc %d, sp %d) %s '%s'\n", pc-1, ex.sp, tokname(op), code.strs[c]);
			Lnum =>
				(nil, c) = getconst(code.ops, pc);
				print("eexp(pc %d, sp %d) %s '%g'\n", pc-1, ex.sp, tokname(op), code.nums[c]);
			* =>
				print("eexp(pc %d, sp %d) %s\n", pc-1, ex.sp, tokname(op));
			}
		}
		case op{
		Lthis =>
			v1 = objval(ex.this);
		Lnum =>
			(pc, c) = getconst(code.ops, pc);
			v1 = numval(code.nums[c]);
		Lstr =>
			(pc, c) = getconst(code.ops, pc);
			v1 = strval(code.strs[c]);
		Lid =>
			(pc, c) = getconst(code.ops, pc);
			epush(ex, esprimid(ex, code.strs[c]));
			continue;
		'.' =>
			a1 = epop(ex);
			v1 = epopval(ex);
			epush(ex, ref Ref(1, nil, toObject(ex, v1), a1.name));
			continue;
		'[' =>
			v2 = epopval(ex);
			v1 = epopval(ex);
			epush(ex, ref Ref(1, nil, toObject(ex, v1), toString(ex, v2)));
			continue;
		Lpostinc or
		Lpostdec =>
			a1 = epop(ex);
			r1 = toNumber(ex, getValue(ex, a1));
			v1 = numval(r1);
			if(op == Lpostinc)
				r1++;
			else
				r1--;
			putValue(ex, a1, numval(r1));
		Linc or
		Ldec or
		Lpreadd or
		Lpresub =>
			a1 = epop(ex);
			r1 = toNumber(ex, getValue(ex, a1));
			case op{
			Linc =>
				r1++;
			Ldec =>
				r1--;
			Lpresub =>
				r1 = -r1;
			}
			v1 = numval(r1);
			if(op == Linc || op == Ldec)
				putValue(ex, a1, v1);
		'~' =>
			v = epopval(ex);
			i1 = toInt32(ex, v);
			i1 = ~i1;
			v1 = numval(real i1);
		'!' =>
			v = epopval(ex);
			v1 = toBoolean(ex, v);
			if(v1 == true)
				v1 = false;
			else
				v1 = true;
		Ltypeof =>
			a1 = epop(ex);
			if(a1.isref && getBase(ex, a1) == nil)
				s = "undefined";
			else case (v1 = getValue(ex, a1)).ty{
			TUndef =>
				s = "undefined";
			TNull =>
				s = "object";
			TBool =>
				s = "boolean";
			TNum =>
				s = "number";
			TStr =>
				s = "string";
			TObj =>
				if(v1.obj.call != nil)
					s = "function";
				else
					s = "object";
			}
			v1 = strval(s);
		Ldelete =>
			a1 = epop(ex);
			o = getBase(ex, a1);
			s = getPropertyName(ex, a1);
			if(o != nil)
				esdelete(ex, o, s, 0);
			v1 = undefined;
		Lvoid =>
			epopval(ex);
			v = undefined;
		'*' or
		'/' or
		'%' or
		'-' =>
			v2 = epopval(ex);
			a1 = epop(ex);
			r1 = toNumber(ex, getValue(ex, a1));
			r2 = toNumber(ex, v2);
			case op{
			'*' =>
				r1 = r1 * r2;
			'/' =>
				r1 = r1 / r2;
			'%' =>
				r1 = fmod(r1, r2);
			'-' =>
				r1 = r1 - r2;
			}
			v1 = numval(r1);
		'+' =>
			v2 = epopval(ex);
			a1 = epop(ex);
			v1 = toPrimitive(ex, getValue(ex, a1), NoHint);
			v2 = toPrimitive(ex, v2, NoHint);
			if(v1.ty == TStr || v2.ty == TStr)
				v1 = strval(toString(ex, v1)+toString(ex, v2));
			else
				v1 = numval(toNumber(ex, v1)+toNumber(ex, v2));
		Llsh or
		Lrsh or
		Lrshu or
		'&' or
		'^' or
		'|' =>
			v2 = epopval(ex);
			a1 = epop(ex);
			i1 = toInt32(ex, getValue(ex, a1));
			i2 = toInt32(ex, v2);
			case op{
			Llsh =>
				i1 <<= i2 & 16r1f;
			Lrsh =>
				i1 >>= i2 & 16r1f;
			Lrshu =>
				i1 = int (((big i1) & 16rffffffff) >> (i2 & 16r1f));
			'&' =>
				i1 &= i2;
			'|' =>
				i1 |= i2;
			'^' =>
				i1 ^= i2;
			}
			v1 = numval(real i1);
		'=' or
		Las =>
			v1 = epopval(ex);
			a1 = epop(ex);
			putValue(ex, a1, v1);
		'<' or
		'>' or
		Lleq or
		Lgeq =>
			v2 = epopval(ex);
			v1 = epopval(ex);
			if(op == '>' || op == Lleq){
				v = v1;
				v1 = v2;
				v2 = v;
			}
			v1 = toPrimitive(ex, v1, TNum);
			v2 = toPrimitive(ex, v2, TNum);
			if(v1.ty == TStr && v2.ty == TStr){
				if(v1.str < v2.str)
					v1 = true;
				else
					v1 = false;
			}else{
				r1 = toNumber(ex, v1);
				r2 = toNumber(ex, v2);
				if(isnan(r1) || isnan(r2))
					v1 = undefined;
				else if(r1 < r2)
					v1 = true;
				else
					v1 = false;
			}
			if(op == Lgeq || op == Lleq){
				if(v1 == false)
					v1 = true;
				else
					v1 = false;
			}
		Leq or
		Lneq =>
			v2 = epopval(ex);
			v1 = epopval(ex);
			v = false;
			while(v1.ty != v2.ty){
				if(v1 == null && v2 == undefined
				|| v1 == undefined && v2 == null)
					v1 = v2;
				else if(v1.ty == TNum && v2.ty == TStr)
					v2 = numval(toNumber(ex, v2));
				else if(v1.ty == TStr && v2.ty == TNum)
					v1 = numval(toNumber(ex, v1));
				else if(v1.ty == TBool)
					v1 = numval(toNumber(ex, v1));
				else if(v2.ty == TBool)
					v2 = numval(toNumber(ex, v2));
				else if(v2.ty == TObj && (v1.ty == TStr || v1.ty == TNum))
					v2 = toPrimitive(ex, v2, NoHint);
				else if(v1.ty == TObj && (v2.ty == TStr || v2.ty == TNum))
					v1 = toPrimitive(ex, v1, NoHint);
				else{
					v1 = true;
					v2 = false;
				}
			}
			case v1.ty{
			TUndef or
			TNull =>
				v = true;
			TNum =>
				if(v1.num == v2.num)
					v = true;
			TBool =>
				if(v1 == v2)
					v = true;
			TStr =>
				if(v1.str == v2.str)
					v = true;
			TObj =>
				if(v1.obj == v2.obj)
					v = true;
			}
			if(op == Lneq){
				if(v == false)
					v = true;
				else
					v = false;
			}
			v1 = v;
		Landand =>
			v1 = epopval(ex);
			(pc, apc) = getjmp(code.ops, pc);
			if(toBoolean(ex, v1) != false){
				(pc, a1) = eexp(ex, code, pc, apc);
				v1 = getValue(ex, a1);
			}
			pc = apc;
		Loror =>
			v1 = epopval(ex);
			(pc, apc) = getjmp(code.ops, pc);
			if(toBoolean(ex, v1) != true){
				(pc, a1) = eexp(ex, code, pc, apc);
				v1 = getValue(ex, a1);
			}
			pc = apc;
		'?' =>
			v1 = epopval(ex);
			(pc, apc) = getjmp(code.ops, pc);
			v1 = toBoolean(ex, v1);
			if(v1 == true)
				(pc, a1) = eexp(ex, code, pc, apc);
			pc = apc;
			(pc, apc) = getjmp(code.ops, pc);
			if(v1 != true)
				(pc, a1) = eexp(ex, code, pc, apc);
			pc = apc;
			v1 = getValue(ex, a1);
		Lasop =>
			a1 = epop(ex);
			epush(ex, a1);
			v1 = getValue(ex, a1);
		Lgetval =>
			v1 = epopval(ex);
		',' =>
			v1 = epopval(ex);
			epop(ex);
			# a1's value already gotten by Lgetval
		'(' or
		')' =>
			continue;
		Lcall or
		Lnewcall =>
			(pc, c) = getconst(code.ops, pc);
			args := array[c] of ref Val;
			c = ex.sp - c;
			for(sp := c; sp < ex.sp; sp++)
				args[sp-c] = getValue(ex, ex.stack[sp]);
			ex.sp = c;
			a1 = epop(ex);
			v = getValue(ex, a1);
			o = getobj(v);
			if(op == Lcall){
				if(o == nil || o.call == nil)
					runtime(ex, "can only call function objects");
				th = nil;
				if(a1.isref){
					th = getBase(ex, a1);
					if(th != nil && isactobj(th))
						th = nil;
				}

				# have to execute functions in the same context as they
				# were defined, but need to use current stack.
				if (o.call.ex == nil)
					a1 = escall(ex, v.obj, th, args);
				else {
					fnex := ref *o.call.ex;
					fnex.stack = ex.stack;
					fnex.sp = ex.sp;
					# drop ref to stack to avoid array duplication should stack grow
					ex.stack = nil;
					a1 = escall(fnex, v.obj, th, args);
					# restore stack, sp is OK as escall() ensures that stack is balanced
					ex.stack = fnex.stack;
				}
			}else{
				if(o == nil || o.construct == nil)
					runtime(ex, "new must be given a constructor object");
				a1 = valref(objval(esconstruct(ex, o, args)));
			}
			epush(ex, a1);
			args = nil;
			continue;
		Lnew =>
			v = epopval(ex);
			o = getobj(v);
			if(o == nil || o.construct == nil)
				runtime(ex, "new must be given a constructor object");
			v1 = objval(esconstruct(ex, o, nil));
		';' =>
			break out;
		* =>
			fatal(ex, sprint("eexp: unknown op %s\n", tokname(op)));
		}
		epushval(ex, v1);
	}

	if(savesp == ex.sp)
		return (pc, nil);

	if(savesp != ex.sp-1)
		print("unbalanced stack in eexp: %d %d\n", savesp, ex.sp);
	return (pc, epop(ex));
}

epushval(ex: ref Exec, v: ref Val)
{
	epush(ex, valref(v));
}

epush(ex: ref Exec, r: ref Ref)
{
	if(ex.sp >= len ex.stack){
		st := array[2 * len ex.stack] of ref Ref;
		st[:] = ex.stack;
		ex.stack = st;
	}
	ex.stack[ex.sp++] = r;
}

epop(ex: ref Exec): ref Ref
{
	if(ex.sp == 0)
		fatal(ex, "popping too far off the estack\n");
	return ex.stack[--ex.sp];
}

epopval(ex: ref Exec): ref Val
{
	if(ex.sp == 0)
		fatal(ex, "popping too far off the estack\n");
	return getValue(ex, ex.stack[--ex.sp]);
}
