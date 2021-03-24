maxstack:	int;				# max size of a stack frame called

precasttab := array[Tend] of array of ref Type;

optabinit()
{
	ct := array[Tend] of ref Type;
	for(i := 0; i < Tend; i++)
		precasttab[i] = ct;
	precasttab[Tstring] = array[Tend] of { Tbyte => tint, };
	precasttab[Tbig] = array[Tend] of { Tbyte => tint, };
	precasttab[Treal] = array[Tend] of { Tbyte => tint, };
	precasttab[Tbyte] = array[Tend] of { Tstring => tint, Tbig => tint, Treal => tint, };

	casttab = array[Tend] of { * => array[Tend] of {* => 0}};

	casttab[Tint][Tint] = IMOVW;
	casttab[Tbig][Tbig] = IMOVL;
	casttab[Treal][Treal] = IMOVF;
	casttab[Tbyte][Tbyte] = IMOVB;
	casttab[Tstring][Tstring] = IMOVP;

	casttab[Tint][Tbyte] = ICVTWB;
	casttab[Tint][Treal] = ICVTWF;
	casttab[Tint][Tstring] = ICVTWC;
	casttab[Tbyte][Tint] = ICVTBW;
	casttab[Treal][Tint] = ICVTFW;
	casttab[Tstring][Tint] = ICVTCW;

	casttab[Tint][Tbig] = ICVTWL;
	casttab[Treal][Tbig] = ICVTFL;
	casttab[Tstring][Tbig] = ICVTCL;
	casttab[Tbig][Tint] = ICVTLW;
	casttab[Tbig][Treal] = ICVTLF;
	casttab[Tbig][Tstring] = ICVTLC;

	casttab[Treal][Tstring] = ICVTFC;
	casttab[Tstring][Treal] = ICVTCF;

	casttab[Tstring][Tarray] = ICVTCA;
	casttab[Tarray][Tstring] = ICVTAC;

	#
	# placeholders; fixed in precasttab
	#
	casttab[Tbyte][Tstring] = 16rff;
	casttab[Tstring][Tbyte] = 16rff;
	casttab[Tbyte][Treal] = 16rff;
	casttab[Treal][Tbyte] = 16rff;
	casttab[Tbyte][Tbig] = 16rff;
	casttab[Tbig][Tbyte] = 16rff;
}

#
# global variable and constant initialization checking
#
vcom(ids: ref Decl): int
{
	ok := 1;
	for(v := ids; v != nil; v = v.next)
		ok &= varcom(v);
	for(v = ids; v != nil; v = v.next)
		v.init = simplify(v.init);
	return ok;
}

simplify(n: ref Node): ref Node
{
	if(n == nil)
		return nil;
	if(debug['F'])
		print("simplify %s\n", nodeconv(n));
	n = efold(rewrite(n));
	if(debug['F'])
		print("simplified %s\n", nodeconv(n));
	return n;
}

#
# rewrite an expression to make it easiser to compile,
# or give the correct results
#
rewrite(n: ref Node): ref Node
{
	v: big;
	t: ref Type;
	d: ref Decl;
	nn: ref Node;

	if(n == nil)
		return nil;

	left := n.left;
	right := n.right;

	#
	# rewrites
	#
	case n.op{
	Oname =>
		d = n.decl;
		if(d.importid != nil){
			left = mkbin(Omdot, dupn(1, n.src, d.eimport), mkdeclname(n.src, d.importid));
			left.ty = n.ty;
			return rewrite(left);
		}
	Odas =>
		n.op = Oas;
		return rewrite(n);
	Oneg =>
		n.left = rewrite(left);
		if(n.ty == treal)
			break;
		left = n.left;
		n.right = left;
		n.left = mkconst(n.src, big 0);
		n.left.ty = n.ty;
		n.op = Osub;
	Ocomp =>
		v = big 0;
		v = ~v;
		n.right = mkconst(n.src, v);
		n.right.ty = n.ty;
		n.left = rewrite(left);
		n.op = Oxor;
	Oinc or
	Odec or
	Opreinc or
	Opredec =>
		n.left = rewrite(left);
		case n.ty.kind{
		Treal =>
			n.right = mkrconst(n.src, 1.0);
		Tint or
		Tbig or
		Tbyte =>
			n.right = mkconst(n.src, big 1);
			n.right.ty = n.ty;
		* =>
			fatal("can't rewrite inc/dec "+nodeconv(n));
		}
		if(n.op == Opreinc)
			n.op = Oaddas;
		else if(n.op == Opredec)
			n.op = Osubas;
	Oslice =>
		if(right.left.op == Onothing)
			right.left = mkconst(right.left.src, big 0);
		n.left = rewrite(left);
		n.right = rewrite(right);
	Oindex =>
		n.op = Oindx;
		n.left = rewrite(left);
		n.right = rewrite(right);
		n = mkunary(Oind, n);
		n.ty = n.left.ty;
		n.left.ty = tint;
	Oload =>
		n.right = mkn(Oname, nil, nil);
		n.right.src = n.left.src;
		n.right.decl = n.ty.tof.decl;
		n.right.ty = n.ty;
		n.left = rewrite(left);
	Ocast =>
		n.op = Ocast;
		t = precasttab[left.ty.kind][n.ty.kind];
		if(t != nil){
			n.left = mkunary(Ocast, left);
			n.left.ty = t;
			return rewrite(n);
		}
		n.left = rewrite(left);
	Ocall =>
		t = left.ty;
		if(t.kind == Tref)
			t = t.tof;
		if(t.kind == Tfn){
			n.left = rewrite(left);
			if(right != nil)
				n.right = rewrite(right);
			break;
		}
		case n.ty.kind{
		Tref =>
			n = mkunary(Oref, n);
			n.ty = n.left.ty;
			n.left.ty = n.left.ty.tof;
			n.left.left.ty = n.left.ty;
			return rewrite(n);
		Tadt =>
			n.op = Otuple;
			n.right = nil;
			if(n.ty.tags != nil){
				n.left = nn = mkunary(Oseq, mkconst(n.src, big left.right.decl.tag));
				if(right != nil){
					nn.right = right;
					nn.src.stop = right.src.stop;
				}
				n.ty = left.right.decl.ty.tof;
			}else
				n.left = right;
			return rewrite(n);
		Tadtpick =>
			n.op = Otuple;
			n.right = nil;
			n.left = nn = mkunary(Oseq, mkconst(n.src, big left.right.decl.tag));
			if(right != nil){
				nn.right = right;
				nn.src.stop = right.src.stop;
			}
			n.ty = left.right.decl.ty.tof;
			return rewrite(n);
		* =>
			fatal("can't deal with "+nodeconv(n)+" in rewrite/Ocall");
		}
	Omdot =>
		#
		# what about side effects from left?
		#
		d = right.decl;
		case d.store{
		Dfn =>
			n.left = rewrite(left);
			if(right.op == Odot){
				n.right = dupn(1, left.src, right.right);
				n.right.ty = d.ty;
			}
		Dconst or
		Dtag or
		Dtype =>
			# handled by fold
			return n;
		Dglobal =>
			right.op = Oconst;
			right.c = ref Const(big d.offset, 0.);
			right.ty = tint;

			n.left = left = mkunary(Oind, left);
			left.ty = tint;
			n.op = Oadd;
			n = mkunary(Oind, n);
			n.ty = n.left.ty;
			n.left.ty = tint;
			n.left = rewrite(n.left);
			return n;
		* =>
			fatal("can't deal with "+nodeconv(n)+" in rewrite/Omdot");
		}
	Odot =>
		#
		# what about side effects from left?
		#
		d = right.decl;
		case d.store{
		Dfn =>
			if(right.left != nil){
				n = mkbin(Omdot, dupn(1, left.src, right.left), right);
				right.left = nil;
				n.ty = d.ty;
				return rewrite(n);
			}
			n.op = Oname;
			n.decl = d;
			n.right = nil;
			n.left = nil;
			return n;
		Dconst or
		Dtag or
		Dtype =>
			# handled by fold
			return n;
		}
		right.op = Oconst;
		right.c = ref Const(big d.offset, 0.);
		right.ty = tint;

		if(left.ty.kind != Tref){
			n.left = mkunary(Oadr, left);
			n.left.ty = tint;
		}
		n.op = Oadd;
		n = mkunary(Oind, n);
		n.ty = n.left.ty;
		n.left.ty = tint;
		n.left = rewrite(n.left);
		return n;
	Oadr =>
		left = rewrite(left);
		n.left = left;
		if(left.op == Oind)
			return left.left;
	Otagof =>
		if(n.decl == nil){
			n.op = Oind;
			return rewrite(n);
		}
		return n;
	* =>
		n.left = rewrite(left);
		n.right = rewrite(right);
	}

	return n;
}

#
# label a node with sethi-ullman numbers and addressablity
# genaddr interprets addable to generate operands,
# so a change here mandates a change there.
#
# addressable:
#	const			Rconst	$value		 may also be Roff or Rdesc
#	Asmall(local)		Rreg	value(FP)
#	Asmall(global)		Rmreg	value(MP)
#	ind(Rareg)		Rreg	value(FP)
#	ind(Ramreg)		Rmreg	value(MP)
#	ind(Rreg)		Radr	*value(FP)
#	ind(Rmreg)		Rmadr	*value(MP)
#	ind(Raadr)		Radr	value(value(FP))
#	ind(Ramadr)		Rmadr	value(value(MP))
#
# almost addressable:
#	adr(Rreg)		Rareg
#	adr(Rmreg)		Ramreg
#	add(const, Rareg)	Rareg
#	add(const, Ramreg)	Ramreg
#	add(const, Rreg)	Raadr
#	add(const, Rmreg)	Ramadr
#	add(const, Raadr)	Raadr
#	add(const, Ramadr)	Ramadr
#	adr(Radr)		Raadr
#	adr(Rmadr)		Ramadr
#
# strangely addressable:
#	fn			Rpc
#	mdot(module,exp)	Rmpc
#
sumark(n: ref Node): ref Node
{
	if(n == nil)
		return nil;

	n.temps = byte 0;
	n.addable = Rcant;

	left := n.left;
	right := n.right;
	if(left != nil){
		sumark(left);
		n.temps = left.temps;
	}
	if(right != nil){
		sumark(right);
		if(right.temps == n.temps)
			n.temps++;
		else if(right.temps > n.temps)
			n.temps = right.temps;
	}

	case n.op{
	Oadr =>
		case int left.addable{
		int Rreg =>
			n.addable = Rareg;
		int Rmreg =>
			n.addable = Ramreg;
		int Radr =>
			n.addable = Raadr;
		int Rmadr =>
			n.addable = Ramadr;
		}
	Oind =>
		case int left.addable{
		int Rreg =>
			n.addable = Radr;
		int Rmreg =>
			n.addable = Rmadr;
		int Rareg =>
			n.addable = Rreg;
		int Ramreg =>
			n.addable = Rmreg;
		int Raadr =>
			n.addable = Radr;
		int Ramadr =>
			n.addable = Rmadr;
		}
	Oname =>
		case n.decl.store{
		Darg or
		Dlocal =>
			n.addable = Rreg;
		Dglobal =>
			n.addable = Rmreg;
		Dtype =>
			#
			# check for inferface to load
			#
			if(n.decl.ty.kind == Tmodule)
				n.addable = Rmreg;
		Dfn =>
			n.addable = Rpc;
		* =>
			fatal("cannot deal with "+declconv(n.decl)+" in Oname in "+nodeconv(n));
		}
	Omdot =>
		n.addable = Rmpc;
	Oconst =>
		case n.ty.kind{
		Tint =>
			v := int n.c.val;
			if(v < 0 && ((v >> 29) & 7) != 7
			|| v > 0 && (v >> 29) != 0){
				n.decl = globalconst(n);
				n.addable = Rmreg;
			}else
				n.addable = Rconst;
		Tbig =>
			n.decl = globalBconst(n);
			n.addable = Rmreg;
		Tbyte =>
			n.decl = globalbconst(n);
			n.addable = Rmreg;
		Treal =>
			n.decl = globalfconst(n);
			n.addable = Rmreg;
		Tstring =>
			n.decl = globalsconst(n);
			n.addable = Rmreg;
		* =>
			fatal("cannot const in sumark "+typeconv(n.ty));
		}
	Oadd =>
		if(right.addable == Rconst){
			case int left.addable{
			int Rareg =>
				n.addable = Rareg;
			int Ramreg =>
				n.addable = Ramreg;
			int Rreg or
			int Raadr =>
				n.addable = Raadr;
			int Rmreg or
			int Ramadr =>
				n.addable = Ramadr;
			}
		}
	}
	if(n.addable < Rcant)
		n.temps = byte 0;
	else if(n.temps == byte 0)
		n.temps = byte 1;
	return n;
}

mktn(t: ref Type): ref Node
{
	n := mkn(Oname, nil, nil);
	usedesc(mktdesc(t));
	n.ty = t;
	if(t.decl == nil)
		fatal("mktn nil decl t "+typeconv(t));
	n.decl = t.decl;
	n.addable = Rdesc;
	return n;
}

#
# compile an expression with an implicit assignment
# note: you are not allowed to use nto.src
#
# need to think carefully about the types used in moves
#
ecom(src: Src, nto, n: ref Node): ref Node
{
	tleft, tright, tto, ttn: ref Node;
	t: ref Type;

	if(debug['e']){
		print("ecom: %s\n", nodeconv(n));
		if(nto != nil)
			print("ecom nto: %s\n", nodeconv(nto));
	}

	if(n.addable < Rcant){
		#
		# think carefully about the type used here
		#
		if(nto != nil)
			genmove(src, Mas, n.ty, n, nto);
		return nto;
	}

	left := n.left;
	right := n.right;
	op := n.op;
	case op{
	* =>
		fatal("can't ecom "+nodeconv(n));
		return nto;
	Onothing =>
		break;
	Oused =>
		if(nto != nil)
			fatal("superflous used "+nodeconv(left)+" nto "+nodeconv(nto));
		tto = talloc(left.ty, nil);
		ecom(left.src, tto, left);
		tfree(tto);
	Oas =>
		if(right.ty == tany)
			right.ty = n.ty;
		if(left.op == Oname && left.decl.ty == tany){
			if(nto == nil)
				nto = tto = talloc(right.ty, nil);
			left = nto;
			nto = nil;
		}
		if(left.op == Oinds){
			indsascom(src, nto, n);
			tfree(tto);
			break;
		}
		if(left.op == Oslice){
			slicelcom(src, nto, n);
			tfree(tto);
			break;
		}

		if(left.op == Otuple){
			if(right.addable >= Ralways
			|| right.op != Oname
			|| tupaliased(right, left)){
				tright = talloc(n.ty, nil);
				ecom(n.right.src, tright, right);
				right = tright;
			}
			tuplcom(right, n.left);
			if(nto != nil)
				genmove(src, Mas, n.ty, right, nto);
			tfree(tright);
			tfree(tto);
			break;
		}

		#
		# check for left/right aliasing and build right into temporary
		#
		if(right.op == Otuple
		&& (left.op != Oname || tupaliased(left, right)))
			right = ecom(right.src, tright = talloc(right.ty, nil), right);

		#
		# think carefully about types here
		#
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
		ecom(n.src, left, right);
		if(nto != nil)
			genmove(src, Mas, nto.ty, left, nto);
		tfree(tleft);
		tfree(tright);
		tfree(tto);
	Ochan =>
		genchan(src, n.ty.tof, nto);
	Oinds =>
		if(right.addable < Ralways){
			if(left.addable >= Rcant)
				(left, tleft) = eacom(left, nil);
		}else if(left.temps <= right.temps){
			right = ecom(right.src, tright = talloc(right.ty, nil), right);
			if(left.addable >= Rcant)
				(left, tleft) = eacom(left, nil);
		}else{
			(left, tleft) = eacom(left, nil);
			right = ecom(right.src, tright = talloc(right.ty, nil), right);
		}
		genop(n.src, op, left, right, nto);
		tfree(tleft);
		tfree(tright);
	Osnd =>
		if(right.addable < Rcant){
			if(left.addable >= Rcant)
				(left, tleft) = eacom(left, nto);
		}else if(left.temps < right.temps){
			(right, tright) = eacom(right, nto);
			if(left.addable >= Rcant)
				(left, tleft) = eacom(left, nil);
		}else{
			(left, tleft) = eacom(left, nto);
			(right, tright) = eacom(right, nil);
		}
		genrawop(n.src, ISEND, right, nil, left);
		if(nto != nil)
			genmove(src, Mas, right.ty, right, nto);
		tfree(tleft);
		tfree(tright);
	Orcv =>
		if(nto == nil){
			ecom(n.src, tto = talloc(n.ty, nil), n);
			tfree(tto);
			return nil;
		}
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
		if(left.ty.kind == Tchan){
			genrawop(src, IRECV, left, nil, nto);
		}else{
			recvacom(src, nto, n);
		}
		tfree(tleft);
	Ocons =>
		#
		# another temp which can go with analysis
		#
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nil);
		if(!sameaddr(right, nto)){
			ecom(right.src, tto = talloc(n.ty, nto), right);
			genmove(src, Mcons, left.ty, left, tto);
			if(!sameaddr(tto, nto))
				genmove(src, Mas, nto.ty, tto, nto);
		}else
			genmove(src, Mcons, left.ty, left, nto);
		tfree(tleft);
		tfree(tto);
	Ohd =>
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
		genmove(src, Mhd, nto.ty, left, nto);
		tfree(tleft);
	Otl =>
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
		genmove(src, Mtl, left.ty, left, nto);
		tfree(tleft);
	Otuple =>
		tupcom(nto, n);
	Oadd or
	Osub or
	Omul or
	Odiv or
	Omod or
	Oand or
	Oor or
	Oxor or
	Olsh or
	Orsh =>
		#
		# check for 2 operand forms
		#
		if(sameaddr(nto, left)){
			if(right.addable >= Rcant)
				(right, tright) = eacom(right, nto);
			genop(src, op, right, nil, nto);
			tfree(tright);
			break;
		}

		if(opcommute[op] && sameaddr(nto, right) && n.ty != tstring){
			if(left.addable >= Rcant)
				(left, tleft) = eacom(left, nto);
			genop(src, opcommute[op], left, nil, nto);
			tfree(tleft);
			break;
		}

		if(right.addable < left.addable
		&& opcommute[op]
		&& n.ty != tstring){
			op = opcommute[op];
			left = right;
			right = n.left;
		}
		if(left.addable < Ralways){
			if(right.addable >= Rcant)
				(right, tright) = eacom(right, nto);
		}else if(right.temps <= left.temps){
			left = ecom(left.src, tleft = talloc(left.ty, nto), left);
			if(right.addable >= Rcant)
				(right, tright) = eacom(right, nil);
		}else{
			(right, tright) = eacom(right, nto);
			left = ecom(left.src, tleft = talloc(left.ty, nil), left);
		}

		#
		# check for 2 operand forms
		#
		if(sameaddr(nto, left))
			genop(src, op, right, nil, nto);
		else if(opcommute[op] && sameaddr(nto, right) && n.ty != tstring)
			genop(src, opcommute[op], left, nil, nto);
		else
			genop(src, op, right, left, nto);
		tfree(tleft);
		tfree(tright);
	Oaddas or
	Osubas or
	Omulas or
	Odivas or
	Omodas or
	Oandas or
	Ooras or
	Oxoras or
	Olshas or
	Orshas =>
		if(left.op == Oinds){
			indsascom(src, nto, n);
			break;
		}
		if(right.addable < Rcant){
			if(left.addable >= Rcant)
				(left, tleft) = eacom(left, nto);
		}else if(left.temps < right.temps){
			(right, tright) = eacom(right, nto);
			if(left.addable >= Rcant)
				(left, tleft) = eacom(left, nil);
		}else{
			(left, tleft) = eacom(left, nto);
			(right, tright) = eacom(right, nil);
		}
		genop(n.src, op, right, nil, left);
		if(nto != nil)
			genmove(src, Mas, left.ty, left, nto);
		tfree(tleft);
		tfree(tright);
	Olen =>
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
		op = -1;
		t = left.ty;
		if(t == tstring)
			op = ILENC;
		else if(t.kind == Tarray)
			op = ILENA;
		else if(t.kind == Tlist)
			op = ILENL;
		else
			fatal("can't len "+nodeconv(n));
		genrawop(src, op, left, nil, nto);
		tfree(tleft);
	Oneg =>
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
		genop(n.src, op, left, nil, nto);
		tfree(tleft);
	Oinc or
	Odec =>
		if(left.op == Oinds){
			indsascom(src, nto, n);
			break;
		}
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nil);
		if(nto != nil)
			genmove(src, Mas, left.ty, left, nto);
		if(right.addable >= Rcant)
			fatal("inc/dec amount not addressable: "+nodeconv(n));
		genop(n.src, op, right, nil, left);
		tfree(tleft);
	Ospawn =>
		callcom(n.src, op, left, nto);
	Ocall =>
		callcom(n.src, op, n, nto);
	Oref =>
		t = left.ty;
		if(left.op == Oname && left.decl.store == Dtype){
			genrawop(src, INEW, mktn(t), nil, nto);
			break;
		}
		if(t.kind == Tadt && t.tags != nil){
			pickdupcom(src, nto, left);
			break;
		}

		tt := t;
		if(left.op == Oconst && left.decl.store == Dtag)
			t = left.decl.ty.tof;

		#
		# could eliminate temp if nto does not occur
		# in tuple initializer
		#
		tto = talloc(n.ty, nto);
		genrawop(src, INEW, mktn(t), nil, tto);
		tright = ref znode;
		tright.op = Oind;
		tright.left = tto;
		tright.right = nil;
		tright.ty = tt;
		sumark(tright);
		ecom(src, tright, left);
		genmove(src, Mas, n.ty, tto, nto);
		tfree(tto);
	Oload =>
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
		tright = talloc(tint, nil);
		genrawop(src, ILEA, right, nil, tright);
		genrawop(src, ILOAD, left, tright, nto);
		tfree(tleft);
		tfree(tright);
	Ocast =>
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
		t = left.ty;
		genrawop(src, casttab[t.kind][n.ty.kind], left, nil, nto);
		tfree(tleft);
	Oarray =>
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
		if(arrayz)
			genrawop(left.src, INEWAZ, left, mktn(n.ty.tof), nto);
		else
			genrawop(left.src, INEWA, left, mktn(n.ty.tof), nto);
		if(right != nil)
			arraycom(nto, right);
		tfree(tleft);
	Oslice =>
		tn := right.right;
		right = right.left;

		#
		# make the left node of the slice directly addressable
		# therefore, if it's len is taken (via tn),
		# left's tree won't be rewritten
		#
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nil);

		if(tn.op == Onothing){
			tn = mkn(Olen, left, nil);
			tn.src = src;
			tn.ty = tint;
			sumark(tn);
		}
		if(tn.addable < Ralways){
			if(right.addable >= Rcant)
				(right, tright) = eacom(right, nil);
		}else if(right.temps <= tn.temps){
			tn = ecom(tn.src, ttn = talloc(tn.ty, nil), tn);
			if(right.addable >= Rcant)
				(right, tright) = eacom(right, nil);
		}else{
			(right, tright) = eacom(right, nil);
			tn = ecom(tn.src, ttn = talloc(tn.ty, nil), tn);
		}
		op = ISLICEA;
		if(nto.ty == tstring)
			op = ISLICEC;

		#
		# overwrite the destination last,
		# since it might be used in computing the slice bounds
		#
		if(!sameaddr(left, nto))
			ecom(left.src, nto, left);

		genrawop(src, op, right, tn, nto);
		tfree(tleft);
		tfree(tright);
		tfree(ttn);
	Oindx =>
		if(right.addable < Rcant){
			if(left.addable >= Rcant)
				(left, tleft) = eacom(left, nto);
		}else if(left.temps < right.temps){
			(right, tright) = eacom(right, nto);
			if(left.addable >= Rcant)
				(left, tleft) = eacom(left, nil);
		}else{
			(left, tleft) = eacom(left, nto);
			(right, tright) = eacom(right, nil);
		}
		if(nto.addable >= Ralways)
			nto = ecom(src, tto = talloc(nto.ty, nil), nto);
		op = IINDX;
		case left.ty.tof.size{
		IBY2LG =>
			op = IINDL;
			if(left.ty.tof == treal)
				op = IINDF;
		IBY2WD =>
			op = IINDW;
		1 =>
			op = IINDB;
		}
		genrawop(src, op, left, nto, right);
		tfree(tleft);
		tfree(tright);
		tfree(tto);
	Oind =>
		(n, tleft) = eacom(n, nto);
		genmove(src, Mas, n.ty, n, nto);
		tfree(tleft);
	Onot or
	Oandand or
	Ooror or
	Oeq or
	Oneq or
	Olt or
	Oleq or
	Ogt or
	Ogeq =>
		p := bcom(n, 1, nil);
		genmove(src, Mas, tint, sumark(mkconst(src, big 1)), nto);
		pp := genrawop(src, IJMP, nil, nil, nil);
		patch(p, nextinst());
		genmove(src, Mas, tint, sumark(mkconst(src, big 0)), nto);
		patch(pp, nextinst());
	}
	return nto;
}

#
# compile exp n to yield an addressable expression
# use reg to build a temporary; if t is a temp, it is usable
#
# note that 0adr's are strange as they are only used
# for calculating the addresses of fields within adt's.
# therefore an Oind is the parent or grandparent of the Oadr,
# and we pick off all of the cases where Oadr's argument is not
# addressable by looking from the Oind.
#
eacom(n, t: ref Node): (ref Node, ref Node)
{
	reg: ref Node;

	if(debug['e'] || debug['E'])
		print("eacom: %s\n", nodeconv(n));

	left := n.left;
	if(n.op != Oind){
		ecom(n.src, reg = talloc(n.ty, t), n);
		reg.src = n.src;
		return (reg, reg);
	}

	if(left.op == Oadd && left.right.op == Oconst){
		if(left.left.op == Oadr){
			(left.left.left, reg) = eacom(left.left.left, t);
			sumark(n);
			if(n.addable >= Rcant)
				fatal("eacom can't make node addressable: "+nodeconv(n));
			return (n, reg);
		}
		reg = talloc(left.left.ty, t);
		ecom(left.left.src, reg, left.left);
		left.left.decl = reg.decl;
		left.left.addable = Rreg;
		left.left = reg;
		left.addable = Raadr;
		n.addable = Radr;
	}else if(left.op == Oadr){
		reg = talloc(left.left.ty, t);
		ecom(left.left.src, reg, left.left);

		#
		# sleaze: treat the temp as the type of the field, not the enclosing structure
		#
		reg.ty = n.ty;
		reg.src = n.src;
		return (reg, reg);
	}else{
		reg = talloc(left.ty, t);
		ecom(left.src, reg, left);
		n.left = reg;
		n.addable = Radr;
	}
	return (n, reg);
}

#
# compile an assignment to an array slice
#
slicelcom(src: Src, nto, n: ref Node): ref Node
{
	tleft, tright, tv: ref Node;

	left := n.left.left;
	right := n.left.right.left;
	v := n.right;
	if(right.addable < Ralways){
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
	}else if(left.temps <= right.temps){
		right = ecom(right.src, tright = talloc(right.ty, nto), right);
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nil);
	}else{
		(left, tleft) = eacom(left, nil);		# dangle on right and v
		right = ecom(right.src, tright = talloc(right.ty, nil), right);
	}

	case n.op{
	Oas =>
		if(v.addable >= Rcant)
			(v, tv) = eacom(v, nil);
	}

	genrawop(n.src, ISLICELA, v, right, left);
	if(nto != nil)
		genmove(src, Mas, n.ty, left, nto);
	tfree(tleft);
	tfree(tv);
	tfree(tright);
	return nto;
}

#
# compile an assignment to a string location
#
indsascom(src: Src, nto, n: ref Node): ref Node
{
	tleft, tright, tv, tu, u: ref Node;

	left := n.left.left;
	right := n.left.right;
	v := n.right;
	if(right.addable < Ralways){
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nto);
	}else if(left.temps <= right.temps){
		right = ecom(right.src, tright = talloc(right.ty, nto), right);
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nil);
	}else{
		(left, tleft) = eacom(left, nil);		# dangle on right and v
		right = ecom(right.src, tright = talloc(right.ty, nil), right);
	}

	case n.op{
	Oas =>
		if(v.addable >= Rcant)
			(v, tv) = eacom(v, nil);
	Oinc or
	Odec =>
		if(v.addable >= Rcant)
			fatal("inc/dec amount not addable");
		u = tu = talloc(tint, nil);
		genop(n.left.src, Oinds, left, right, u);
		if(nto != nil)
			genmove(src, Mas, n.ty, u, nto);
		nto = nil;
		genop(n.src, n.op, v, nil, u);
		v = u;
	Oaddas or
	Osubas or
	Omulas or
	Odivas or
	Omodas or
	Oandas or
	Ooras or
	Oxoras or
	Olshas or
	Orshas =>
		if(v.addable >= Rcant)
			(v, tv) = eacom(v, nil);
		u = tu = talloc(tint, nil);
		genop(n.left.src, Oinds, left, right, u);
		genop(n.src, n.op, v, nil, u);
		v = u;
	}

	genrawop(n.src, IINSC, v, right, left);
	tfree(tleft);
	tfree(tv);
	tfree(tright);
	tfree(tu);
	if(nto != nil)
		genmove(src, Mas, n.ty, v, nto);
	return nto;
}

callcom(src: Src, op: int, n, ret: ref Node)
{
	tmod: ref Node;

	args := n.right;
	nfn := n.left;
	if(nfn.addable != Rpc && nfn.addable != Rmpc)
		fatal("can't gen call addresses");
	if(nfn.ty.tof != tnone && ret == nil){
		ecom(src, tmod = talloc(nfn.ty.tof, nil), n);
		tfree(tmod);
		return;
	}
	if(nfn.ty.varargs != byte 0){
		d := dupdecl(nfn.right.decl);
		nfn.decl = d;
		d.desc = gendesc(d, idoffsets(nfn.ty.ids, MaxTemp, MaxAlign), nfn.ty.ids);
	}

	frame := talloc(tint, nil);

	mod := nfn.left;
	if(nfn.addable == Rmpc){
		if(mod.addable >= Rcant)
			(mod, tmod) = eacom(mod, nil);		# dangle always
		nfn.right.addable = Roff;
	}

	#
	# allocate the frame
	#
	if(nfn.addable == Rmpc && nfn.ty.varargs == byte 0){
		genrawop(src, IMFRAME, mod, nfn.right, frame);
	}else{
		in := genrawop(src, IFRAME, nil, nil, frame);
		in.sm = Adesc;
		in.s.decl = nfn.decl;
	}

	#
	# build a fake node for the argument area
	#
	toff := ref znode;
	tadd := ref znode;
	pass := ref znode;
	toff.op = Oconst;
	toff.c = ref Const;
	toff.addable = Rconst;
	toff.ty = tint;
	tadd.op = Oadd;
	tadd.addable = Raadr;
	tadd.left = frame;
	tadd.right = toff;
	tadd.ty = tint;
	pass.op = Oind;
	pass.addable = Radr;
	pass.left = tadd;

	#
	# compile all the args
	#
	d := nfn.ty.ids;
	off := 0;
	for(a := args; a != nil; a = a.right){
		off = d.offset;
		toff.c.val = big off;
		pass.ty = d.ty;
		ecom(a.left.src, pass, a.left);
		d = d.next;
	}
	if(off > maxstack)
		maxstack = off;

	#
	# pass return value
	#
	if(ret != nil){
		toff.c.val = big(REGRET*IBY2WD);
		pass.ty = nfn.ty.tof;
		genrawop(src, ILEA, ret, nil, pass);
	}

	#
	# call it
	#
	iop: int;
	if(nfn.addable == Rmpc){
		iop = IMCALL;
		if(op == Ospawn)
			iop = IMSPAWN;
		genrawop(src, iop, frame, nfn.right, mod);
		tfree(tmod);
	}else{
		iop = ICALL;
		if(op == Ospawn)
			iop = ISPAWN;
		in := genrawop(src, iop, frame, nil, nil);
		in.d.decl = nfn.decl;
		in.dm = Apc;
	}
	tfree(frame);
}

#
# initialization code for arrays
# a must be addressable (< Rcant)
#
arraycom(a, elems: ref Node)
{
	top, out: ref Inst;
	ri, n: ref Node;

	if(debug['A'])
		print("arraycom: %s %s\n", nodeconv(a), nodeconv(elems));

	c := elems.ty.cse;
	if(c.wild != nil)
		arraydefault(a, c.wild.right);

	tindex := ref znode;
	fake := ref znode;
	tmp := talloc(tint, nil);
	tindex.op = Oindx;
	tindex.addable = Rcant;
	tindex.left = a;
	tindex.right = nil;
	tindex.ty = tint;
	fake.op = Oind;
	fake.addable = Radr;
	fake.left = tmp;
	fake.ty = a.ty.tof;

	for(e := elems; e != nil; e = e.right){
		#
		# just duplicate the initializer for Oor
		#
		for(q := e.left.left; q != nil; q = q.right){
			if(q.left.op == Owild)
				continue;
	
			body := e.left.right;
			if(q.right != nil)
				body = dupn(0, nosrc, body);
			top = nil;
			out = nil;
			ri = nil;
			if(q.left.op == Orange){
				#
				# for(i := q.left.left; i <= q.left.right; i++)
				#
				ri = talloc(tint, nil);
				ri.src = q.left.src;
				ecom(q.left.src, ri, q.left.left);
	
				# i <= q.left.right;
				n = mkn(Oleq, ri, q.left.right);
				n.src = q.left.src;
				n.ty = tint;
				top = nextinst();
				out = bcom(n, 1, nil);
	
				tindex.right = ri;
			}else{
				tindex.right = q.left;
			}
	
			tindex.addable = Rcant;
			tindex.src = q.left.src;
			ecom(tindex.src, tmp, tindex);
	
			ecom(body.src, fake, body);
	
			if(q.left.op == Orange){
				# i++
				n = mkbin(Oinc, ri, sumark(mkconst(ri.src, big 1)));
				n.ty = tint;
				n.addable = Rcant;
				ecom(n.src, nil, n);
	
				# jump to test
				patch(genrawop(q.left.src, IJMP, nil, nil, nil), top);
				patch(out, nextinst());
				tfree(ri);
			}
		}
	}
	tfree(tmp);
}

#
# default initialization code for arrays.
# compiles to
#	n = len a;
#	while(n){
#		n--;
#		a[n] = elem;
#	}
#
arraydefault(a, elem: ref Node)
{
	e: ref Node;

	if(debug['A'])
		print("arraydefault: %s %s\n", nodeconv(a), nodeconv(elem));

	t := mkn(Olen, a, nil);
	t.src = elem.src;
	t.ty = tint;
	t.addable = Rcant;
	n := talloc(tint, nil);
	n.src = elem.src;
	ecom(t.src, n, t);

	top := nextinst();
	out := bcom(n, 1, nil);

	t = mkbin(Odec, n, sumark(mkconst(elem.src, big 1)));
	t.ty = tint;
	t.addable = Rcant;
	ecom(t.src, nil, t);

	if(elem.addable >= Rcant)
		(elem, e) = eacom(elem, nil);

	t = mkn(Oindx, a, n);
	t.src = elem.src;
	t = mkbin(Oas, mkunary(Oind, t), elem);
	t.ty = elem.ty;
	t.left.ty = elem.ty;
	t.left.left.ty = tint;
	sumark(t);
	ecom(t.src, nil, t);

	patch(genrawop(t.src, IJMP, nil, nil, nil), top);

	tfree(n);
	tfree(e);
	patch(out, nextinst());
}

tupcom(nto, n: ref Node)
{
	if(debug['Y'])
		print("tupcom %s\nto %s\n", nodeconv(n), nodeconv(nto));

	#
	# build a fake node for the tuple
	#
	toff := ref znode;
	tadd := ref znode;
	fake := ref znode;
	tadr := ref znode;
	toff.op = Oconst;
	toff.c = ref Const;
	toff.ty = tint;
	tadr.op = Oadr;
	tadr.left = nto;
	tadr.ty = tint;
	tadd.op = Oadd;
	tadd.left = tadr;
	tadd.right = toff;
	tadd.ty = tint;
	fake.op = Oind;
	fake.left = tadd;
	sumark(fake);
	if(fake.addable >= Rcant)
		fatal("tupcom: bad value exp "+nodeconv(fake));

	#
	# compile all the exps
	#
	d := n.ty.ids;
	for(e := n.left; e != nil; e = e.right){
		toff.c.val = big d.offset;
		fake.ty = d.ty;
		ecom(e.left.src, fake, e.left);
		d = d.next;
	}
}

tuplcom(n, nto: ref Node)
{
	if(debug['Y'])
		print("tuplcom %s\nto %s\n", nodeconv(n), nodeconv(nto));

	#
	# build a fake node for the tuple
	#
	toff := ref znode;
	tadd := ref znode;
	fake := ref znode;
	tadr := ref znode;
	toff.op = Oconst;
	toff.c = ref Const;
	toff.ty = tint;
	tadr.op = Oadr;
	tadr.left = n;
	tadr.ty = tint;
	tadd.op = Oadd;
	tadd.left = tadr;
	tadd.right = toff;
	tadd.ty = tint;
	fake.op = Oind;
	fake.left = tadd;
	sumark(fake);
	if(fake.addable >= Rcant)
		fatal("tuplcom: bad value exp for "+nodeconv(fake));

	#
	# compile all the exps
	#
	tas := ref znode;
	d := nto.ty.ids;
	if(nto.ty.kind == Tadtpick)
		d = nto.ty.tof.ids.next;
	for(e := nto.left; e != nil; e = e.right){
		as := e.left;
		if(as.op != Oname || as.decl != nildecl){
			toff.c.val = big d.offset;
			fake.ty = d.ty;
			fake.src = as.src;
			if(as.addable < Rcant)
				genmove(as.src, Mas, d.ty, fake, as);
			else{
				tas.op = Oas;
				tas.ty = d.ty;
				tas.src = as.src;
				tas.left = as;
				tas.right = fake;
				tas.addable = Rcant;
				ecom(as.src, nil, tas);
			}
		}
		d = d.next;
	}
}

#
# boolean compiler
# fall through when condition == true
#
bcom(n: ref Node, true: int, b: ref Inst): ref Inst
{
	tleft, tright: ref Node;

	if(debug['b'])
		print("bcom %s %d\n", nodeconv(n), true);

	left := n.left;
	right := n.right;
	op := n.op;
	case op{
	Onothing =>
		return b;
	Onot =>
		return bcom(n.left, !true, b);
	Oandand =>
		if(!true)
			return oror(n, true, b);
		return andand(n, true, b);
	Ooror =>
		if(!true)
			return andand(n, true, b);
		return oror(n, true, b);
	Ogt or
	Ogeq or
	Oneq or
	Oeq or
	Olt or
	Oleq =>
		break;
	* =>
		if(n.ty.kind == Tint){
			right = mkconst(n.src, big 0);
			right.addable = Rconst;
			left = n;
			op = Oneq;
			break;
		}
		fatal("can't bcom "+nodeconv(n));
		return b;
	}

	if(true)
		op = oprelinvert[op];

	if(left.addable < right.addable){
		t := left;
		left = right;
		right = t;
		op = opcommute[op];
	}

	if(right.addable < Ralways){
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nil);
	}else if(left.temps <= right.temps){
		right = ecom(right.src, tright = talloc(right.ty, nil), right);
		if(left.addable >= Rcant)
			(left, tleft) = eacom(left, nil);
	}else{
		(left, tleft) = eacom(left, nil);
		right = ecom(right.src, tright = talloc(right.ty, nil), right);
	}
	bb := genbra(n.src, op, left, right);
	bb.branch = b;
	tfree(tleft);
	tfree(tright);
	return bb;
}

andand(n: ref Node, true: int, b: ref Inst): ref Inst
{
	if(debug['b'])
		print("andand %s\n", nodeconv(n));
	b = bcom(n.left, true, b);
	b = bcom(n.right, true, b);
	return b;
}

oror(n: ref Node, true: int, b: ref Inst): ref Inst
{
	if(debug['b'])
		print("oror %s\n", nodeconv(n));
	bb := bcom(n.left, !true, nil);
	b = bcom(n.right, true, b);
	patch(bb, nextinst());
	return b;
}

#
# generate code for a recva expression
# this is just a hacked up small alt
#
recvacom(src: Src, nto, n: ref Node)
{
	left := n.left;

	labs := array[1] of Label;
	labs[0].isptr = left.addable >= Rcant;
	c := ref Case;
	c.nlab = 1;
	c.nsnd = 0;
	c.offset = 0;
	c.labs = labs;
	talt := mktalt(c);

	which := talloc(tint, nil);
	tab := talloc(talt, nil);

	#
	# build the node for the address of each channel,
	# the values to send, and the storage for values received
	#
	off := ref znode;
	adr := ref znode;
	add := ref znode;
	slot := ref znode;
	off.op = Oconst;
	off.c = ref Const;
	off.ty = tint;
	off.addable = Rconst;
	adr.op = Oadr;
	adr.left = tab;
	adr.ty = tint;
	add.op = Oadd;
	add.left = adr;
	add.right = off;
	add.ty = tint;
	slot.op = Oind;
	slot.left = add;
	sumark(slot);

	#
	# gen the channel
	# this sleaze is lying to the garbage collector
	#
	off.c.val = big(2*IBY2WD);
	if(left.addable < Rcant)
		genmove(src, Mas, tint, left, slot);
	else
		ecom(src, slot, left);

	#
	# gen the value
	#
	off.c.val += big IBY2WD;
	genrawop(left.src, ILEA, nto, nil, slot);

	#
	# number of senders and receivers
	#
	off.c.val = big 0;
	genmove(src, Mas, tint, sumark(mkconst(src, big 0)), slot);
	off.c.val += big IBY2WD;
	genmove(src, Mas, tint, sumark(mkconst(src, big 1)), slot);
	off.c.val += big IBY2WD;

	genrawop(src, IALT, tab, nil, which);
	tfree(which);
	tfree(tab);
}

#
# generate code to duplicate an adt with pick fields
# this is just a hacked up small pick
# n is Oind(exp)
#
pickdupcom(src: Src, nto, n: ref Node)
{
	jmps: ref Inst;

	if(n.op != Oind)
		fatal("pickdupcom not Oind: " + nodeconv(n));

	t := n.ty;
	nlab := t.decl.tag;

	#
	# generate global which has case labels
	#
	d := mkids(src, enter(".c"+string nlabel++, 0), mktype(src.start, src.stop, Tcase, nil, nil), nil);
	d.init = mkdeclname(src, d);

	clab := ref znode;
	clab.addable = Rmreg;
	clab.left = nil;
	clab.right = nil;
	clab.op = Oname;
	clab.ty = d.ty;
	clab.decl = d;

	#
	# generate a temp to hold the real value
	# then generate a case on the tag
	#
	orig := n.left;
	tmp := talloc(orig.ty, nil);
	ecom(src, tmp, orig);
	orig = mkunary(Oind, tmp);
	orig.ty = tint;
	sumark(orig);

	dest := mkunary(Oind, nto);
	dest.ty = nto.ty.tof;
	sumark(dest);

	genrawop(src, ICASE, orig, nil, clab);

	labs := array[nlab] of Label;

	i := 0;
	jmps = nil;
	for(tg := t.tags; tg != nil; tg = tg.next){
		stg := tg;
		for(; tg.next != nil; tg = tg.next)
			if(stg.ty != tg.next.ty)
				break;
		start := sumark(simplify(mkdeclname(src, stg)));
		stop := start;
		node := start;
		if(stg != tg){
			stop = sumark(simplify(mkdeclname(src, tg)));
			node = mkbin(Orange, start, stop);
		}

		labs[i].start = start;
		labs[i].stop = stop;
		labs[i].node = node;
		labs[i++].inst = nextinst();

		genrawop(src, INEW, mktn(tg.ty.tof), nil, nto);
		genmove(src, Mas, tg.ty.tof, orig, dest);

		j := genrawop(src, IJMP, nil, nil, nil);
		j.branch = jmps;
		jmps = j;
	}

	#
	# this should really be a runtime error
	#
	wild := genrawop(src, IJMP, nil, nil, nil);
	patch(wild, wild);

	patch(jmps, nextinst());
	tfree(tmp);

	if(i > nlab)
		fatal("overflowed label tab for pickdupcom");

	c := ref Case;
	c.nlab = i;
	c.nsnd = 0;
	c.labs = labs;
	c.iwild = wild;

	d.ty.cse = c;
	usetype(d.ty);
	installids(Dglobal, d);
}

#
# see if name n occurs anywhere in e
#
tupaliased(n, e: ref Node): int
{
	for(;;){
		if(e == nil)
			return 0;
		if(e.op == Oname && e.decl == n.decl)
			return 1;
		if(tupaliased(n, e.left))
			return 1;
		e = e.right;
	}
	return 0;
}

#
# put unaddressable constants in the global data area
#
globalconst(n: ref Node): ref Decl
{
	s := enter(".i." + hex(int n.c.val, 8), 0);
	d := s.decl;
	if(d == nil){
		d = mkids(n.src, s, tint, nil);
		installids(Dglobal, d);
		d.init = n;
		d.refs++;
	}
	return d;
}

globalBconst(n: ref Node): ref Decl
{
	s := enter(".B." + bhex(n.c.val, 16), 0);
	d := s.decl;
	if(d == nil){
		d = mkids(n.src, s, tbig, nil);
		installids(Dglobal, d);
		d.init = n;
		d.refs++;
	}
	return d;
}

globalbconst(n: ref Node): ref Decl
{
	s := enter(".b." + hex(int n.c.val & 16rff, 2), 0);
	d := s.decl;
	if(d == nil){
		d = mkids(n.src, s, tbyte, nil);
		installids(Dglobal, d);
		d.init = n;
		d.refs++;
	}
	return d;
}

globalfconst(n: ref Node): ref Decl
{
	ba := array[8] of byte;
	math->export_real(ba, array[] of {n.c.rval});
	fs := ".f.";
	for(i := 0; i < 8; i++)
		fs += hex(int ba[i], 2);
	if(fs != ".f." + bhex(math->realbits64(n.c.rval), 16))
		fatal("bad globalfconst number");
	s := enter(fs, 0);
	d := s.decl;
	if(d == nil){
		d = mkids(n.src, s, treal, nil);
		installids(Dglobal, d);
		d.init = n;
		d.refs++;
	}
	return d;
}

globalsconst(n: ref Node): ref Decl
{
	s := n.decl.sym;
	n.decl = nil;
	d := s.decl;
	if(d == nil){
		d = mkids(n.src, s, tstring, nil);
		installids(Dglobal, d);
		d.init = n;
	}
	d.refs++;
	n.decl = d;
	return d;
}

#
# make a global of type t
# used to make initialized data
#
globalztup(t: ref Type): ref Decl
{
	z := ".z." + string t.size + ".";
	desc := t.decl.desc;
	for(i := 0; i < desc.nmap; i++)
		z += hex(int desc.map[i], 2);
	s := enter(z, 0);
	d := s.decl;
	if(d == nil){
		d = mkids(t.src, s, t, nil);
		installids(Dglobal, d);
		d.init = nil;
	}
	d.refs++;
	return d;
}
