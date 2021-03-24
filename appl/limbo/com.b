# back end

breaks:		array of ref Inst;
conts:		array of ref Inst;
labels:		array of ref Decl;
labdep:		int;
nocont:		ref Inst;
nlabel:		int;

modcom(entry: ref Decl)
{
	d, m: ref Decl;

	if(errors)
		return;

	if(emitcode != "" || emitstub || emittab != "" || emitsbl != ""){
		emit(curscope());
		popscope();
		return;
	}

	#
	# scom introduces global variables for case statements
	# and unaddressable constants, so it must be done before
	# popping the global scope
	#
	gent = sys->millisec();
	nlabel = 0;
	maxstack = MaxTemp;
	nocont = ref Inst;
	genstart();

	ok := 0;
	for(i := 0; i < nfns; i++){
		if(fns[i].refs > 1){
			fns[ok++] = fns[i];
			fncom(fns[i]);
		}
	}
	fns = fns[:ok];
	nfns = ok;
	if(blocks != -1)
		fatal("blocks not nested correctly");
	firstinst = firstinst.next;
	if(errors)
		return;

	globals := popscope();
	checkrefs(globals);
	if(errors)
		return;
	globals = vars(globals);
	moddataref();

	nils := popscope();
	m = nil;
	for(d = nils; d != nil; d = d.next){
		if(debug['n'])
			print("nil '%s' ref %d\n", d.sym.name, d.refs);
		if(d.refs && m == nil)
			m = dupdecl(d);
		d.offset = 0;
	}
	globals = appdecls(m, globals);
	globals = namesort(globals);
	globals = modglobals(impdecl, globals);
	vcom(globals);
	narrowmods();
	offset := idoffsets(globals, 0, IBY2WD);
	for(d = nils; d != nil; d = d.next){
		if(debug['n'])
			print("nil '%s' ref %d\n", d.sym.name, d.refs);
		if(d.refs)
			d.offset = m.offset;
	}

	if(debug['g']){
		print("globals:\n");
		printdecls(globals);
	}

	ndata := 0;
	for(d = globals; d != nil; d = d.next)
		ndata++;
	ndesc := resolvedesc(impdecl, offset, globals);
	ninst := resolvepcs(firstinst);
	nlink := resolvemod(impdecl);
	gent = sys->millisec() - gent;

	maxstack *= 10;
	if(fixss != 0)
		maxstack = fixss;

	if(debug['s'])
		print("%d instructions\n%d data elements\n%d type descriptors\n%d functions exported\n%d stack size\n",
			ninst, ndata, ndesc, nlink, maxstack);

	writet = sys->millisec();
	if(gendis){
		discon(XMAGIC);
		hints := 0;
		if(mustcompile)
			hints |= MUSTCOMPILE;
		if(dontcompile)
			hints |= DONTCOMPILE;
		discon(hints);		# runtime hints
		discon(maxstack);	# minimum stack extent size
		discon(ninst);
		discon(offset);
		discon(ndesc);
		discon(nlink);
		disentry(entry);
		disinst(firstinst);
		disdesc(descriptors);
		disvar(offset, globals);
		dismod(impdecl);
	}else{
		asminst(firstinst);
		asmentry(entry);
		asmdesc(descriptors);
		asmvar(offset, globals);
		asmmod(impdecl);
	}
	writet = sys->millisec() - writet;

	symt = sys->millisec();
	if(bsym != nil){
		sblmod(impdecl);

		sblfiles();
		sblinst(firstinst, ninst);
		sblty(adts, nadts);
		sblfn(fns, nfns);
		sblvar(globals);
	}
	symt = sys->millisec() - symt;

	firstinst = nil;
	lastinst = nil;
}

fncom(decl: ref Decl)
{
	#
	# pick up the function body and compile it
	# this code tries to clean up the parse nodes as fast as possible
	# function is Ofunc(name, body)
	#
	decl.pc = nextinst();
	tinit();
	labdep = 0;
	breaks = array[maxlabdep] of ref Inst;
	conts = array[maxlabdep] of ref Inst;
	labels = array[maxlabdep] of ref Decl;
	n := decl.init;
	decl.init = n.left;
	src := n.right.src;
	src.start = src.stop - 1;
	for(n = n.right; n != nil; n = n.right){
		if(n.op != Oseq){
			scom(n);
			break;
		}
		scom(n.left);
	}
	pushblock();
	in := genrawop(src, IRET, nil, nil, nil);
	popblock();
	reach(decl.pc);
	if(in.reach != byte 0 && decl.ty.tof != tnone)
		error(src.start, "no return at end of function " + dotconv(decl));
	decl.endpc = lastinst;
	if(labdep != 0)
		fatal("unbalanced label stack");
	breaks = nil;
	conts = nil;
	labels = nil;

	loc := declsort(appdecls(vars(decl.locals), tdecls()));

	decl.offset = idoffsets(loc, decl.offset, MaxAlign);
	for(last := decl.ty.ids; last != nil && last.next != nil; last = last.next)
		;
	if(last != nil)
		last.next = loc;
	else
		decl.ty.ids = loc;

	if(debug['f']){
		print("fn: %s\n", decl.sym.name);
		printdecls(decl.ty.ids);
	}

	decl.desc = gendesc(decl, decl.offset, decl.ty.ids);
	decl.locals = loc;
	if(last != nil)
		last.next = nil;
	else
		decl.ty.ids = nil;

	if(decl.offset > maxstack)
		maxstack = decl.offset;
}

#
# statement compiler
#
scom(n: ref Node)
{
	b: int;
	p, pp: ref Inst;
	left: ref Node;

	for(; n != nil; n = n.right){
		case n.op{
		Ocondecl or
		Otypedecl or
		Ovardecl or
		Oimport =>
			return;
		Ovardecli or
		Oscope =>
			break;
		Oif =>
			pushblock();
			left = simplify(n.left);
			if(left.op == Oconst && left.ty == tint){
				if(left.c.val != big 0)
					scom(n.right.left);
				else
					scom(n.right.right);
				popblock();
				return;
			}
			sumark(left);
			pushblock();
			p = bcom(left, 1, nil);
			popblock();
			scom(n.right.left);
			if(n.right.right != nil){
				pp = p;
				p = genrawop(lastinst.src, IJMP, nil, nil, nil);
				patch(pp, nextinst());
				scom(n.right.right);
			}
			patch(p, nextinst());
			popblock();
			return;
		Ofor =>
			n.left = left = simplify(n.left);
			if(left.op == Oconst && left.ty == tint){
				if(left.c.val == big 0)
					return;
				left.op = Onothing;
				left.ty = tnone;
				left.decl = nil;
			}
			pp = nextinst();
			b = pushblock();
			sumark(left);
			p = bcom(left, 1, nil);
			popblock();

			if(labdep >= maxlabdep)
				fatal("label stack overflow");
			breaks[labdep] = nil;
			conts[labdep] = nil;
			labels[labdep] = n.decl;
			labdep++;
			scom(n.right.left);
			labdep--;

			patch(conts[labdep], nextinst());
			if(n.right.right != nil){
				pushblock();
				scom(n.right.right);
				popblock();
			}
			repushblock(b);
			patch(genrawop(left.src, IJMP, nil, nil, nil), pp);
			popblock();
			patch(p, nextinst());
			patch(breaks[labdep], nextinst());
			return;
		Odo =>
			pp = nextinst();

			if(labdep >= maxlabdep)
				fatal("label stack overflow");
			breaks[labdep] = nil;
			conts[labdep] = nil;
			labels[labdep] = n.decl;
			labdep++;
			scom(n.right);
			labdep--;

			patch(conts[labdep], nextinst());

			left = simplify(n.left);
			if(left.op == Onothing
			|| left.op == Oconst && left.ty == tint){
				if(left.op == Onothing || left.c.val != big 0){
					pushblock();
					p = genrawop(left.src, IJMP, nil, nil, nil);
					popblock();
				}else
					p = nil;
			}else{
				pushblock();
				p = bcom(sumark(left), 0, nil);
				popblock();
			}
			patch(p, pp);
			patch(breaks[labdep], nextinst());
			return;
		Ocase or
		Opick or
		Oalt =>
			pushblock();
			if(labdep >= maxlabdep)
				fatal("label stack overflow");
			breaks[labdep] = nil;
			conts[labdep] = nocont;
			labels[labdep] = n.decl;
			labdep++;
			case n.op{
			Oalt =>
				altcom(n);
			Ocase or
			Opick =>
				casecom(n);
			}
			labdep--;
			patch(breaks[labdep], nextinst());
			popblock();
			return;
		Obreak =>
			pushblock();
			bccom(n, breaks);
			popblock();
		Ocont =>
			pushblock();
			bccom(n, conts);
			popblock();
		Oseq =>
			scom(n.left);
		Oret =>
			pushblock();
			if(n.left != nil){
				n.left = simplify(n.left);
				sumark(n.left);
				ecom(n.left.src, retalloc(ref Node, n.left), n.left);
			}
			genrawop(n.src, IRET, nil, nil, nil);
			popblock();
			return;
		Oexit =>
			pushblock();
			genrawop(n.src, IEXIT, nil, nil, nil);
			popblock();
			return;
		Onothing =>
			return;
		Ofunc =>
			fatal("Ofunc");
			return;
		* =>
			pushblock();
			n = simplify(n);
			sumark(n);
			ecom(n.src, nil, n);
			popblock();
			return;
		}
	}
}

#
# compile a break, continue
#
bccom(n: ref Node, bs: array of ref Inst)
{
	s: ref Sym;

	s = nil;
	if(n.decl != nil)
		s = n.decl.sym;
	ok := -1;
	for(i := 0; i < labdep; i++){
		if(bs[i] == nocont)
			continue;
		if(s == nil || labels[i] != nil && labels[i].sym == s)
			ok = i;
	}
	if(ok < 0)
		fatal("didn't find break or continue");
	p := genrawop(n.src, IJMP, nil, nil, nil);
	p.branch = bs[ok];
	bs[ok] = p;
}

casecom(cn: ref Node)
{
	left, p, tmp: ref Node;
	jmps, wild: ref Inst;

	c := cn.ty.cse;

	#
	# generate global which has case labels
	#
	d := mkids(cn.src, enter(".c"+string nlabel++, 0), cn.ty, nil);
	d.init = mkdeclname(cn.src, d);
	nto := ref znode;
	nto.addable = Rmreg;
	nto.left = nil;
	nto.right = nil;
	nto.op = Oname;
	nto.ty = d.ty;
	nto.decl = d;

	tmp = nil;
	left = cn.left;
	left = simplify(left);
	cn.left = left;
	sumark(left);
	if(debug['c'])
		print("case %s\n", nodeconv(left));
	ctype := cn.left.ty;
	if(left.addable >= Rcant){
		if(cn.op == Opick){
			ecom(left.src, nil, left);
			left = mkunary(Oind, dupn(1, left.src, left.left));
			left.ty = tint;
			sumark(left);
			ctype = tint;
		}else{
			(left, tmp) = eacom(left, nil);
		}
	}
	op := ICASE;
	if(ctype == tstring)
		op = ICASEC;
	genrawop(left.src, op, left, nil, nto);
	tfree(tmp);

	labs := c.labs;
	nlab := c.nlab;

	jmps = nil;
	wild = nil;
	for(n := cn.right; n != nil; n = n.right){
		j := nextinst();
		for(p = n.left.left; p != nil; p = p.right){
			if(debug['c'])
				print("case qualifier %s\n", nodeconv(p.left));
			case p.left.op{
			Oconst =>
				labs[findlab(ctype, p.left, labs, nlab)].inst = j;
			Orange =>
				labs[findlab(ctype, p.left.left, labs, nlab)].inst = j;
			Owild =>
				wild = j;
			}
		}

		if(debug['c'])
			print("case body for %s: %s\n", expconv(n.left.left), nodeconv(n.left.right));

		scom(n.left.right);

		src := lastinst.src;
		if(n.left.right == nil || n.left.right.op == Onothing)
			src = n.left.left.src;
		j = genrawop(src, IJMP, nil, nil, nil);
		j.branch = jmps;
		jmps = j;
	}
	patch(jmps, nextinst());
	if(wild == nil)
		wild = nextinst();

	c.iwild = wild;

	d.ty.cse = c;
	usetype(d.ty);
	installids(Dglobal, d);
}

altcom(nalt: ref Node)
{
	p, op, left: ref Node;
	jmps, wild, j: ref Inst = nil;

	talt := nalt.ty;
	c := talt.cse;
	nlab := c.nlab;
	nsnd := c.nsnd;
	comm := array[nlab] of ref Node;
	labs := array[nlab] of Label;
	tmps := array[nlab] of ref Node;
	c.labs = labs;

	#
	# built the type of the alt channel table
	# note that we lie to the garbage collector
	# if we know that another reference exists for the channel
	#
	is := 0;
	ir := nsnd;
	i := 0;
	for(n := nalt.left; n != nil; n = n.right){
		for(p = n.left.left; p != nil; p = p.right){
			left = simplify(p.left);
			p.left = left;
			if(left.op == Owild)
				continue;
			comm[i] = hascomm(left);
			left = comm[i].left;
			sumark(left);
			isptr := left.addable >= Rcant;
			if(comm[i].op == Osnd)
				labs[is++].isptr = isptr;
			else
				labs[ir++].isptr = isptr;
			i++;
		}
	}

	which := talloc(tint, nil);
	tab := talloc(talt, nil);

	#
	# build the node for the address of each channel,
	# the values to send, and the storage fro values received
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
	# compile the sending and receiving channels and values
	#
	is = 2*IBY2WD;
	ir = is + nsnd*2*IBY2WD;
	i = 0;
	for(n = nalt.left; n != nil; n = n.right){
		for(p = n.left.left; p != nil; p = p.right){
			if(p.left.op == Owild)
				continue;

			#
			# gen channel
			#
			op = comm[i];
			if(op.op == Osnd){
				off.c.val = big is;
				is += 2*IBY2WD;
			}else{
				off.c.val = big ir;
				ir += 2*IBY2WD;
			}
			left = op.left;

			#
			# this sleaze is lying to the garbage collector
			#
			if(left.addable < Rcant)
				genmove(left.src, Mas, tint, left, slot);
			else
				ecom(left.src, slot, left);

			#
			# gen value
			#
			off.c.val += big IBY2WD;
			(p.left, tmps[i]) = rewritecomm(p.left, comm[i], slot);

			i++;
		}
	}

	#
	# stuff the number of send & receive channels into the table
	#
	altsrc := nalt.src;
	altsrc.stop = (altsrc.stop & ~PosMask) | ((altsrc.stop + 3) & PosMask);
	off.c.val = big 0;
	genmove(altsrc, Mas, tint, sumark(mkconst(altsrc, big nsnd)), slot);
	off.c.val += big IBY2WD;
	genmove(altsrc, Mas, tint, sumark(mkconst(altsrc, big(nlab-nsnd))), slot);
	off.c.val += big IBY2WD;

	altop := IALT;
	if(c.wild != nil)
		altop = INBALT;
	genrawop(altsrc, altop, tab, nil, which);

	d := mkids(nalt.src, enter(".g"+string nlabel++, 0), mktype(nalt.src.start, nalt.src.stop, Tgoto, nil, nil), nil);
	d.ty.cse = c;
	d.init = mkdeclname(nalt.src, d);

	nto := ref znode;
	nto.addable = Rmreg;
	nto.left = nil;
	nto.right = nil;
	nto.op = Oname;
	nto.decl = d;
	nto.ty = d.ty;

	me := genrawop(altsrc, IGOTO, which, nil, nto);
	me.d.reg = IBY2WD;		# skip the number of cases field
	tfree(tab);
	tfree(which);

	#
	# compile the guard expressions and bodies
	#
	i = 0;
	is = 0;
	ir = nsnd;
	jmps = nil;
	wild = nil;
	for(n = nalt.left; n != nil; n = n.right){
		j = nil;
		for(p = n.left.left; p != nil; p = p.right){
			tj := nextinst();
			if(p.left.op == Owild){
				wild = nextinst();
			}else{
				if(comm[i].op == Osnd)
					labs[is++].inst = tj;
				else{
					labs[ir++].inst = tj;
					tacquire(tmps[i]);
				}
				sumark(p.left);
				if(debug['a'])
					print("alt guard %s\n", nodeconv(p.left));
				ecom(p.left.src, nil, p.left);
				tfree(tmps[i]);
				i++;
			}
			if(p.right != nil){
				tj = genrawop(lastinst.src, IJMP, nil, nil, nil);
				tj.branch = j;
				j = tj;
			}
		}

		patch(j, nextinst());
		if(debug['a'])
			print("alt body %s\n", nodeconv(n.left.right));
		scom(n.left.right);

		j = genrawop(lastinst.src, IJMP, nil, nil, nil);
		j.branch = jmps;
		jmps = j;
	}
	patch(jmps, nextinst());
	comm = nil;

	c.iwild = wild;

	usetype(d.ty);
	installids(Dglobal, d);
}

#
# rewrite the communication operand
# allocate any temps needed for holding value to send or receive
#
rewritecomm(n, comm, slot: ref Node): (ref Node, ref Node)
{
	adr, tmp: ref Node;

	if(n == nil)
		return (nil, nil);
	adr = nil;
	if(n == comm){
		if(comm.op == Osnd && sumark(n.right).addable < Rcant)
			adr = n.right;
		else{
			adr = tmp = talloc(n.ty, nil);
			tmp.src = n.src;
			if(comm.op == Osnd)
				ecom(n.right.src, tmp, n.right);
			else
				trelease(tmp);
		}
	}
	if(n.right == comm && n.op == Oas && comm.op == Orcv
	&& sumark(n.left).addable < Rcant)
		adr = n.left;
	if(adr != nil){
		genrawop(comm.left.src, ILEA, adr, nil, slot);
		return (adr, tmp);
	}
	(n.left, tmp) = rewritecomm(n.left, comm, slot);
	if(tmp == nil)
		(n.right, tmp) = rewritecomm(n.right, comm, slot);
	return (n, tmp);
}

#
# merge together two sorted lists, yielding a sorted list
#
declmerge(e, f: ref Decl): ref Decl
{
	d := rock := ref Decl;
	while(e != nil && f != nil){
		fs := f.ty.size;
		es := e.ty.size;
		v := 0;
		if(es <= IBY2WD || fs <= IBY2WD)
			v = fs - es;
		if(v == 0)
			v = e.refs - f.refs;
		if(v == 0)
			v = fs - es;
		if(v == 0)
			v = e.sym.name < f.sym.name;
		if(v >= 0){
			d.next = e;
			e = e.next;
		}else{
			d.next = f;
			f = f.next;
		}
		d = d.next;
	}
	if(e != nil)
		d.next = e;
	else
		d.next = f;
	return rock.next;
}

#
# recursively split lists and remerge them after they are sorted
#
recdeclsort(d: ref Decl, n: int): ref Decl
{
	if(n <= 1)
		return d;
	m := n / 2 - 1;
	dd := d;
	for(i := 0; i < m; i++)
		dd = dd.next;
	r := dd.next;
	dd.next = nil;
	return declmerge(recdeclsort(d, n / 2),
			recdeclsort(r, (n + 1) / 2));
}

#
# sort the ids by size and number of references
#
declsort(d: ref Decl): ref Decl
{
	n := 0;
	for(dd := d; dd != nil; dd = dd.next)
		n++;
	return recdeclsort(d, n);
}
