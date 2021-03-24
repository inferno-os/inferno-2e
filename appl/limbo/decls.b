
storename := array[Dend] of
{
	Dtype =>	"type",
	Dfn =>		"function",
	Dglobal =>	"global",
	Darg =>		"argument",
	Dlocal =>	"local",
	Dconst =>	"con",
	Dfield =>	"field",
	Dtag =>		"pick tag",
	Dimport =>	"import",
	Dunbound =>	"unbound",
	Dundef =>	"undefined",
	Dwundef =>	"undefined",
};

storeart := array[Dend] of
{
	Dtype =>	"a ",
	Dfn =>		"a ",
	Dglobal =>	"a ",
	Darg =>		"an ",
	Dlocal =>	"a ",
	Dconst =>	"a ",
	Dfield =>	"a ",
	Dtag =>		"a ",
	Dimport =>	"an ",
	Dunbound =>	"",
	Dundef =>	"",
	Dwundef =>	"",
};

storespace := array[Dend] of
{
	Dtype =>	0,
	Dfn =>		0,
	Dglobal =>	1,
	Darg =>		1,
	Dlocal =>	1,
	Dconst =>	0,
	Dfield =>	1,
	Dtag =>		0,
	Dimport =>	0,
	Dunbound =>	0,
	Dundef =>	0,
	Dwundef =>	0,
};

impdecl:	ref Decl;
scopes :=	array[MaxScope] of ref Decl;
tails :=	array[MaxScope] of ref Decl;
iota:		ref Decl;
zdecl:		Decl;

popscopes()
{
	d: ref Decl;

	#
	# clear out any decls left in syms
	#
	while(scope >= ScopeBuiltin){
		for(d = scopes[scope--]; d != nil; d = d.next){
			if(d.sym != nil){
				d.sym.decl = d.old;
				d.old = nil;
			}
		}
	}

	if(impdecl != nil){
		for(d = impdecl.ty.ids; d != nil; d = d.next){
			d.sym.decl = nil;
			d.old = nil;
		}
	}
	impdecl = nil;

	scope = ScopeBuiltin;
	scopes[ScopeBuiltin] = nil;
	tails[ScopeBuiltin] = nil;
}

declstart()
{
	iota = mkids(nosrc, enter("iota", 0), tint, nil);
	iota.init = mkconst(nosrc, big 0);

	scope = ScopeNils;
	scopes[ScopeNils] = nil;
	tails[ScopeNils] = nil;

	nildecl = mkdecl(nosrc, Dglobal, tany);
	nildecl.sym = enter("nil", 0);
	installids(Dglobal, nildecl);
	d := mkdecl(nosrc, Dglobal, tstring);
	d.sym = enterstring("");
	installids(Dglobal, d);

	scope = ScopeGlobal;
	scopes[ScopeGlobal] = nil;
	tails[ScopeGlobal] = nil;
}

redecl(d: ref Decl)
{
	old := d.sym.decl;
	if(old.store == Dwundef)
		return;
	error(d.src.start, "redeclaration of "+declconv(d)+", previously declared as "+storeconv(old)+" on line "+
		lineconv(old.src.start));
}

checkrefs(d: ref Decl)
{
	id, m: ref Decl;
	refs: int;

	for(; d != nil; d = d.next){
		case d.store{
		Dtype =>
			refs = d.refs;
			if(d.ty.kind == Tadt){
				for(id = d.ty.ids; id != nil; id = id.next){
					d.refs += id.refs;
					if(id.store != Dfn)
						continue;
					if(id.init == nil && d.importid == nil)
						error(d.src.start, "function "+d.sym.name+"."+id.sym.name+" not defined");
					if(superwarn && !id.refs && d.importid == nil)
						warn(d.src.start, "function "+d.sym.name+"."+id.sym.name+" not referenced");
				}
			}
			if(d.ty.kind == Tmodule){
				for(id = d.ty.ids; id != nil; id = id.next){
					refs += id.refs;
					if(id.iface != nil)
						id.iface.refs += id.refs;
					if(id.store == Dtype){
						for(m = id.ty.ids; m != nil; m = m.next){
							refs += m.refs;
							if(m.iface != nil)
								m.iface.refs += m.refs;
						}
					}
				}
				d.refs = refs;
			}
			if(superwarn && !refs)
				warn(d.src.start, declconv(d)+" not referenced");
		Dglobal =>
			if(superwarn && !d.refs && d.sym != nil && d.sym.name[0] != '.')
				warn(d.src.start, declconv(d)+" not referenced");
		Dlocal or
		Darg =>
			if(!d.refs && d.sym != nil)
				warn(d.src.start, declconv(d)+" not referenced");
		Dconst =>
			if(superwarn && !d.refs && d.sym != nil)
				warn(d.src.start, declconv(d)+" not referenced");
		Dfn =>
			if(d.init == nil && d.importid == nil)
				error(d.src.start, declconv(d)+" not defined");
			if(superwarn && !d.refs)
				warn(d.src.start, declconv(d)+" not referenced");
		Dimport =>
			if(superwarn && !d.refs)
				warn(d.src.start, declconv(d)+" not referenced");
		}
	}
}

vardecl(ids: ref Decl, t: ref Type): ref Node
{
	n := mkn(Ovardecl, mkn(Oseq, nil, nil), nil);
	n.decl = ids;
	n.ty = t;
	return n;
}

vardecled(n: ref Node)
{
	store := Dlocal;
	if(scope == ScopeGlobal)
		store = Dglobal;
	ids := n.decl;
	installids(store, ids);
	t := n.ty;
	for(last := ids; ids != nil; ids = ids.next){
		ids.ty = t;
		last = ids;
	}
	n.left.decl = last;
}

condecl(ids: ref Decl, init: ref Node): ref Node
{
	n := mkn(Ocondecl, mkn(Oseq, nil, nil), init);
	n.decl = ids;
	return n;
}

condecled(n: ref Node)
{
	ids := n.decl;
	installids(Dconst, ids);
	for(last := ids; ids != nil; ids = ids.next){
		ids.ty = tunknown;
		last = ids;
	}
	n.left.decl = last;
}

importdecl(m: ref Node, ids: ref Decl): ref Node
{
	n := mkn(Oimport, mkn(Oseq, nil, nil), m);
	n.decl = ids;
	return n;
}

importdecled(n: ref Node)
{
	ids := n.decl;
	installids(Dimport, ids);
	for(last := ids; ids != nil; ids = ids.next){
		ids.ty = tunknown;
		last = ids;
	}
	n.left.decl = last;
}

mkscope(body: ref Node): ref Node
{
	n := mkn(Oscope, nil, body);
	if(body != nil)
		n.src = body.src;
	return n;
}

fndecl(n: ref Node, t: ref Type, body: ref Node): ref Node
{
	n = mkbin(Ofunc, n, body);
	n.ty = t;
	return n;
}

fndecled(n: ref Node)
{
	left := n.left;
	if(left.op == Oname){
		d := left.decl.sym.decl;
		if(d == nil){
			d = mkids(left.src, left.decl.sym, n.ty, nil);
			installids(Dfn, d);
		}
		left.decl = d;
		d.refs++;
	}
	pushscope();
	installids(Darg, n.ty.ids);
	n.ty.ids = popscope();
}

#
# check the function declaration only
# the body will be type checked later by fncheck
#
fnchk(n: ref Node): ref Decl
{
	d := n.left.decl;
	if(n.left.op == Odot)
		d = n.left.right.decl;
	if(d == nil)
		fatal("decl() fnchk nil");
	n.left.decl = d;
	if(d.store == Dglobal || d.store == Dfield)
		d.store = Dfn;
	if(d.store != Dfn || d.init != nil)
		nerror(n, "redeclaration of function "+dotconv(d)+", previously declared as "
			+storeconv(d)+" on line "+lineconv(d.src.start));
	d.init = n;

	t := n.ty;
	inadt := d.dot;
	if(inadt != nil && (inadt.store != Dtype || inadt.ty.kind != Tadt))
		inadt = nil;
	t = validtype(t, inadt);
	if(debug['d'])
		print("declare function %s ty %s newty %s\n", dotconv(d), typeconv(d.ty), typeconv(t));
	t = usetype(t);

	if(!tcompat(d.ty, t, 0))
		nerror(n, "type mismatch: "+dotconv(d)+" defined as "
			+typeconv(t)+" declared as "+typeconv(d.ty)+" on line "+lineconv(d.src.start));
	if(t.varargs != byte 0)
		nerror(n, "cannot define functions with a '*' argument, such as "+dotconv(d));

	d.ty = t;
	d.offset = idoffsets(t.ids, MaxTemp, IBY2WD);
	d.src = n.src;

	d.locals = nil;

	n.ty = t;

	return d;
}

globalas(dst: ref Node, v: ref Node, valok: int): ref Node
{
	if(v == nil)
		return nil;
	if(v.op == Oas || v.op == Odas){
		v = globalas(v.left, v.right, valok);
		if(v == nil)
			return nil;
	}else if(valok && !initable(dst, v, 0))
		return nil;
	case dst.op{
	Oname =>
		if(dst.decl.init != nil)
			nerror(dst, "duplicate assignment to "+expconv(dst)+", previously assigned on line "
				+lineconv(dst.decl.init.src.start));
		if(valok)
			dst.decl.init = v;
		return v;
	Otuple =>
		if(valok && v.op != Otuple)
			fatal("can't deal with "+nodeconv(v)+" in tuple case of globalas");
		tv := v.left;
		for(dst = dst.left; dst != nil; dst = dst.right){
			globalas(dst.left, tv.left, valok);
			if(valok)
				tv = tv.right;
		}
		return v;
	}
	fatal("can't deal with "+nodeconv(dst)+" in globalas");
	return nil;
}

needsstore(d: ref Decl): int
{
	if(!d.refs)
		return 0;
	if(d.importid != nil)
		return 0;
	if(storespace[d.store])
		return 1;
	return 0;
}

#
# return the list of all referenced storage variables
#
vars(d: ref Decl): ref Decl
{
	while(d != nil && !needsstore(d))
		d = d.next;
	for(v := d; v != nil; v = v.next){
		while(v.next != nil){
			n := v.next;
			if(needsstore(n))
				break;
			v.next = n.next;
		}
	}
	return d;
}

#
# declare variables from the left side of a := statement
#
recdasdecl(n: ref Node, store: int): int
{
	case n.op{
	Otuple =>
		ok := 1;
		for(n = n.left; n != nil; n = n.right)
			ok &= recdasdecl(n.left, store);
		return ok;
	Oname =>
		if(n.decl == nildecl)
			return 1;
		d := mkids(n.src, n.decl.sym, nil, nil);
		installids(store, d);
		n.decl = d;
		old := d.old;
		if(old != nil
		&& old.store != Dfn
		&& old.store != Dwundef
		&& old.store != Dundef)
			warn(d.src.start, "redeclaration of "+declconv(d)+", previously declared as "
				+storeconv(old)+" on line "+lineconv(old.src.start));
		d.refs++;
		return 1;
	}
	return 0;
}

dasdecl(n: ref Node): int
{
	store := Dlocal;
	if(scope == ScopeGlobal)
		store = Dglobal;

	ok := recdasdecl(n, store);
	if(!ok)
		nerror(n, "illegal declaration expression "+expconv(n));
	return ok;
}

#
# declare global variables in nested := expressions
#
gdasdecl(n: ref Node)
{
	if(n == nil)
		return;

	if(n.op == Odas){
		gdasdecl(n.right);
		dasdecl(n.left);
	}else{
		gdasdecl(n.left);
		gdasdecl(n.right);
	}
}

undefed(src: Src, s: ref Sym): ref Decl
{
	d := mkids(src, s, tnone, nil);
	error(src.start, s.name+" is not declared");
	installids(Dwundef, d);
	return d;
}

pushscope()
{
	if(scope >= MaxScope)
		fatal("scope too deep");
	scope++;
	scopes[scope] = nil;
	tails[scope] = nil;
}

curscope(): ref Decl
{
	return scopes[scope];
}

#
# revert to old declarations for each symbol in the currect scope.
# remove the effects of any imported adt types
# whenever the adt is imported from a module,
# we record in the type's decl the module to use
# when calling members.  the process is reversed here.
#
popscope(): ref Decl
{
	for(id := scopes[scope]; id != nil; id = id.next){
		if(id.sym != nil){
			id.sym.decl = id.old;
			id.old = nil;
		}
		if(id.importid != nil)
			id.importid.refs += id.refs;
		t := id.ty;
		if(id.store == Dtype
		&& t.decl != nil
		&& t.decl.timport == id)
			t.decl.timport = id.timport;
	}
	return scopes[scope--];
}

#
# make a new scope,
# preinstalled with some previously installed identifiers
# don't add the identifiers to the scope chain,
# so they remain separate from any newly installed ids
#
# these routines assume no ids are imports
#
repushids(ids: ref Decl)
{
	if(scope >= MaxScope)
		fatal("scope too deep");
	scope++;
	scopes[scope] = nil;
	tails[scope] = nil;

	for(; ids != nil; ids = ids.next){
		if(ids.scope != scope
		&& (ids.dot == nil || ids.dot.sym != impmod
			|| ids.scope != ScopeGlobal || scope != ScopeGlobal + 1))
			fatal("repushids scope mismatch");
		s := ids.sym;
		if(s != nil){
			if(s.decl != nil && s.decl.scope >= scope)
				ids.old = s.decl.old;
			else
				ids.old = s.decl;
			s.decl = ids;
		}
	}
}

#
# pop a scope which was started with repushids
# return any newly installed ids
#
popids(ids: ref Decl): ref Decl
{
	for(; ids != nil; ids = ids.next){
		if(ids.sym != nil){
			ids.sym.decl = ids.old;
			ids.old = nil;
		}
	}
	return popscope();
}

installids(store: int, ids: ref Decl)
{
	last : ref Decl = nil;
	for(d := ids; d != nil; d = d.next){
		d.scope = scope;
		if(d.store == Dundef)
			d.store = store;
		s := d.sym;
		if(s != nil){
			if(s.decl != nil && s.decl.scope >= scope){
				redecl(d);
				d.old = s.decl.old;
			}else
				d.old = s.decl;
			s.decl = d;
		}
		last = d;
	}
	if(ids != nil){
		d = tails[scope];
		if(d == nil)
			scopes[scope] = ids;
		else
			d.next = ids;
		tails[scope] = last;
	}
}

mkids(src: Src, s: ref Sym, t: ref Type, next: ref Decl): ref Decl
{
	d := ref zdecl;
	d.src = src;
	d.store = Dundef;
	d.ty = t;
	d.next = next;
	d.sym = s;
	return d;
}

mkdecl(src: Src, store: int, t: ref Type): ref Decl
{
	d := ref zdecl;
	d.src = src;
	d.store = store;
	d.ty = t;
	return d;
}

dupdecl(old: ref Decl): ref Decl
{
	d := ref *old;
	d.next = nil;
	return d;
}

appdecls(d: ref Decl, dd: ref Decl): ref Decl
{
	if(d == nil)
		return dd;
	for(t := d; t.next != nil; t = t.next)
		;
	t.next = dd;
	return d;
}

revids(id: ref Decl): ref Decl
{
	next : ref Decl;
	d : ref Decl = nil;
	for(; id != nil; id = next){
		next = id.next;
		id.next = d;
		d = id;
	}
	return d;
}

idoffsets(id: ref Decl, offset: int, al: int): int
{
	for(; id != nil; id = id.next){
		if(storespace[id.store]){
usedty(id.ty);
			offset = align(offset, id.ty.align);
			id.offset = offset;
			offset += id.ty.size;
		}
	}
	return align(offset, al);
}

declconv(d: ref Decl): string
{
	if(d.sym == nil)
		return storename[d.store] + " " + "<???>";
	return storename[d.store] + " " + d.sym.name;
}

storeconv(d: ref Decl): string
{
	return storeart[d.store] + storename[d.store];
}

dotconv(d: ref Decl): string
{
	s: string;

	if(d.dot != nil && d.dot.sym != impmod){
		s = dotconv(d.dot);
		if(d.dot.ty != nil && d.dot.ty.kind == Tmodule)
			s += "->";
		else
			s += ".";
	}
	s += d.sym.name;
	return s;
}

#
# merge together two sorted lists, yielding a sorted list
#
namemerge(e, f: ref Decl): ref Decl
{
	d := rock := ref Decl;
	while(e != nil && f != nil){
		if(e.sym.name <= f.sym.name){
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
recnamesort(d: ref Decl, n: int): ref Decl
{
	if(n <= 1)
		return d;
	m := n / 2 - 1;
	dd := d;
	for(i := 0; i < m; i++)
		dd = dd.next;
	r := dd.next;
	dd.next = nil;
	return namemerge(recnamesort(d, n / 2),
			recnamesort(r, (n + 1) / 2));
}

#
# sort the ids by name
#
namesort(d: ref Decl): ref Decl
{
	n := 0;
	for(dd := d; dd != nil; dd = dd.next)
		n++;
	return recnamesort(d, n);
}

printdecls(d: ref Decl)
{
	for(; d != nil; d = d.next)
		print("%d: %s %s ref %d\n", d.offset, declconv(d), typeconv(d.ty), d.refs);
}
