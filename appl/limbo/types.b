
kindname := array [Tend] of
{
	Tnone =>	"no type",
	Tadt =>		"adt",
	Tadtpick =>	"adt",
	Tarray =>	"array",
	Tbig =>		"big",
	Tbyte =>	"byte",
	Tchan =>	"chan",
	Treal =>	"real",
	Tfn =>		"fn",
	Tint =>		"int",
	Tlist =>	"list",
	Tmodule =>	"module",
	Tref =>		"ref",
	Tstring =>	"string",
	Ttuple =>	"tuple",

	Tainit =>	"array initializers",
	Talt =>		"alt channels",
	Tany =>		"polymorphic type",
	Tarrow =>	"->",
	Tcase =>	"case int labels",
	Tcasec =>	"case string labels",
	Tdot =>		".",
	Terror =>	"type error",
	Tgoto =>	"goto labels",
	Tid =>		"id",
	Tiface =>	"module interface",
};

tattr = array[Tend] of
{
	#		     isptr	refable	conable	big	vis
	Tnone =>	Tattr(0,	0,	0,	0,	0),
	Tadt =>		Tattr(0,	1,	0,	1,	1),
	Tadtpick =>	Tattr(0,	1,	0,	1,	1),
	Tarray =>	Tattr(1,	0,	0,	0,	1),
	Tbig =>		Tattr(0,	0,	1,	1,	1),
	Tbyte =>	Tattr(0,	0,	1,	0,	1),
	Tchan =>	Tattr(1,	0,	0,	0,	1),
	Treal =>	Tattr(0,	0,	1,	1,	1),
	Tfn =>		Tattr(0,	1,	0,	0,	1),
	Tint =>		Tattr(0,	0,	1,	0,	1),
	Tlist =>	Tattr(1,	0,	0,	0,	1),
	Tmodule =>	Tattr(1,	0,	0,	0,	1),
	Tref =>		Tattr(1,	0,	0,	0,	1),
	Tstring =>	Tattr(1,	0,	1,	0,	1),
	Ttuple =>	Tattr(0,	1,	0,	1,	1),

	Tainit =>	Tattr(0,	0,	0,	1,	0),
	Talt =>		Tattr(0,	0,	0,	1,	0),
	Tany =>		Tattr(1,	0,	0,	0,	0),
	Tarrow =>	Tattr(0,	0,	0,	0,	1),
	Tcase =>	Tattr(0,	0,	0,	1,	0),
	Tcasec =>	Tattr(0,	0,	0,	1,	0),
	Tdot =>		Tattr(0,	0,	0,	0,	1),
	Terror =>	Tattr(0,	1,	1,	0,	0),
	Tgoto =>	Tattr(0,	0,	0,	1,	0),
	Tid =>		Tattr(0,	0,	0,	0,	1),
	Tiface =>	Tattr(0,	0,	0,	1,	0),
};

eqclass:	array of ref Teq;

ztype:		Type;
eqrec:		int;
eqset:		int;
adts:		array of ref Decl;
nadts:		int;
anontupsym:	ref Sym;

typeinit()
{
	anontupsym = enter(".tuple", 0);

	ztype.sbl = -1;
	ztype.ok = byte 0;
	ztype.rec = byte 0;

	tbig = mktype(noline, noline, Tbig, nil, nil);
	tbig.size = IBY2LG;
	tbig.align = IBY2LG;
	tbig.ok = OKmask;

	tbyte = mktype(noline, noline, Tbyte, nil, nil);
	tbyte.size = 1;
	tbyte.align = 1;
	tbyte.ok = OKmask;

	tint = mktype(noline, noline, Tint, nil, nil);
	tint.size = IBY2WD;
	tint.align = IBY2WD;
	tint.ok = OKmask;

	treal = mktype(noline, noline, Treal, nil, nil);
	treal.size = IBY2FT;
	treal.align = IBY2FT;
	treal.ok = OKmask;

	tstring = mktype(noline, noline, Tstring, nil, nil);
	tstring.size = IBY2WD;
	tstring.align = IBY2WD;
	tstring.ok = OKmask;

	tany = mktype(noline, noline, Tany, nil, nil);
	tany.size = IBY2WD;
	tany.align = IBY2WD;
	tany.ok = OKmask;

	tnone = mktype(noline, noline, Tnone, nil, nil);
	tnone.size = 0;
	tnone.align = 1;
	tnone.ok = OKmask;

	terror = mktype(noline, noline, Terror, nil, nil);
	terror.size = 0;
	terror.align = 1;
	terror.ok = OKmask;

	tunknown = mktype(noline, noline, Terror, nil, nil);
	tunknown.size = 0;
	tunknown.align = 1;
	tunknown.ok = OKmask;
}

typestart()
{
	descriptors = nil;
	nfns = 0;
	adts = nil;
	nadts = 0;

	eqclass = array[Tend] of ref Teq;

	typebuiltin(mkids(nosrc, enter("int", 0), nil, nil), tint);
	typebuiltin(mkids(nosrc, enter("big", 0), nil, nil), tbig);
	typebuiltin(mkids(nosrc, enter("byte", 0), nil, nil), tbyte);
	typebuiltin(mkids(nosrc, enter("string", 0), nil, nil), tstring);
	typebuiltin(mkids(nosrc, enter("real", 0), nil, nil), treal);
}

modclass(): ref Teq
{
	return eqclass[Tmodule];
}

mktype(start: Line, stop: Line, kind: int, tof: ref Type, args: ref Decl): ref Type
{
	t := ref ztype;
	t.src.start = start;
	t.src.stop = stop;
	t.kind = kind;
	t.tof = tof;
	t.ids = args;
	return t;
}

nalt: int;
mktalt(c: ref Case): ref Type
{
	t := mktype(noline, noline, Talt, nil, nil);
	t.decl = mkdecl(nosrc, Dtype, t);
	t.decl.sym = enter(".a"+string nalt++, 0);
	t.cse = c;
	return usetype(t);
}

#
# copy t and the top level of ids
#
copytypeids(t: ref Type): ref Type
{
	last: ref Decl;

	nt := ref *t;
	for(id := t.ids; id != nil; id = id.next){
		new := ref *id;
		if(last == nil)
			nt.ids = new;
		else
			last.next = new;
		last = new;
	}
	return nt;
}

#
# make each of the ids have type t
#
typeids(ids: ref Decl, t: ref Type): ref Decl
{
	if(ids == nil)
		return nil;

	ids.ty = t;
	for(id := ids.next; id != nil; id = id.next)
		id.ty = t;
	return ids;
}

typebuiltin(d: ref Decl, t: ref Type)
{
	d.ty = t;
	t.decl = d;
	installids(Dtype, d);
}

fielddecl(store: int, ids: ref Decl): ref Node
{
	n := mkn(Ofielddecl, nil, nil);
	n.decl = ids;
	for(; ids != nil; ids = ids.next)
		ids.store = store;
	return n;
}

typedecl(ids: ref Decl, t: ref Type): ref Node
{
	if(t.decl == nil)
		t.decl = ids;
	n := mkn(Otypedecl, nil, nil);
	n.decl = ids;
	n.ty = t;
	for(; ids != nil; ids = ids.next)
		ids.ty = t;
	return n;
}

typedecled(n: ref Node)
{
	installids(Dtype, n.decl);
}

adtdecl(ids: ref Decl, fields: ref Node): ref Node
{
	n := mkn(Oadtdecl, nil, nil);
	t := mktype(ids.src.start, ids.src.stop, Tadt, nil, nil);
	n.decl = ids;
	n.left = fields;
	n.ty = t;
	t.decl = ids;
	for(; ids != nil; ids = ids.next)
		ids.ty = t;
	return n;
}

adtdecled(n: ref Node)
{
	d := n.ty.decl;
	installids(Dtype, d);
	pushscope();
	fielddecled(n.left);
	n.ty.ids = popscope();
	for(ids := n.ty.ids; ids != nil; ids = ids.next)
		ids.dot = d;
}

fielddecled(n: ref Node)
{
	for(; n != nil; n = n.right){
		case n.op{
		Oseq =>
			fielddecled(n.left);
		Oadtdecl =>
			adtdecled(n);
			return;
		Otypedecl =>
			typedecled(n);
			return;
		Ofielddecl =>
			installids(Dfield, n.decl);
			return;
		Ocondecl =>
			condecled(n);
			gdasdecl(n.right);
			return;
		Opickdecl =>
			pickdecled(n);
			return;
		* =>
			fatal("can't deal with "+opname[n.op]+" in fielddecled");
		}
	}
}

pickdecled(n: ref Node): int
{
	if(n == nil)
		return 0;
	tag := pickdecled(n.left);
	pushscope();
	fielddecled(n.right.right);
	d := n.right.left.decl;
	d.ty.ids = popscope();
	installids(Dtag, d);
	for(; d != nil; d = d.next)
		d.tag = tag++;
	return tag;
}

#
# make the tuple type used to initialize adt t
#
mkadtcon(t: ref Type): ref Type
{
	last: ref Decl;

	nt := ref *t;
	nt.ids = nil;
	nt.kind = Ttuple;
	for(id := t.ids; id != nil; id = id.next){
		if(id.store != Dfield)
			continue;
		new := ref *id;
		new.cyc = byte 0;
		if(last == nil)
			nt.ids = new;
		else
			last.next = new;
		last = new;
	}
	last.next = nil;
	return nt;
}

#
# make the tuple type used to initialize t,
# an adt with pick fields tagged by tg
#
mkadtpickcon(t, tgt: ref Type): ref Type
{
	last := mkids(tgt.decl.src, nil, tint, nil);
	last.store = Dfield;
	nt := mktype(t.src.start, t.src.stop, Ttuple, nil, last);
	for(id := t.ids; id != nil; id = id.next){
		if(id.store != Dfield)
			continue;
		new := ref *id;
		new.cyc = byte 0;
		last.next = new;
		last = new;
	}
	for(id = tgt.ids; id != nil; id = id.next){
		if(id.store != Dfield)
			continue;
		new := ref *id;
		new.cyc = byte 0;
		last.next = new;
		last = new;
	}
	last.next = nil;
	return nt;
}

#
# make an identifier type
#
mkidtype(src: Src, s: ref Sym): ref Type
{
	t := mktype(src.start, src.stop, Tid, nil, nil);
	if(s.unbound == nil){
		s.unbound = mkdecl(src, Dunbound, nil);
		s.unbound.sym = s;
	}
	t.decl = s.unbound;
	return t;
}

#
# make a qualified type for t->s
#
mkarrowtype(start: Line, stop: Line, t: ref Type, s: ref Sym): ref Type
{
	t = mktype(start, stop, Tarrow, t, nil);
	if(s.unbound == nil){
		s.unbound = mkdecl(Src(start, stop), Dunbound, nil);
		s.unbound.sym = s;
	}
	t.decl = s.unbound;
	return t;
}

#
# make a qualified type for t.s
#
mkdottype(start: Line, stop: Line, t: ref Type, s: ref Sym): ref Type
{
	t = mktype(start, stop, Tdot, t, nil);
	if(s.unbound == nil){
		s.unbound = mkdecl(Src(start, stop), Dunbound, nil);
		s.unbound.sym = s;
	}
	t.decl = s.unbound;
	return t;
}

#
# look up the name f in the fields of a module, adt, or tuple
#
namedot(ids: ref Decl, s: ref Sym): ref Decl
{
	for(; ids != nil; ids = ids.next)
		if(ids.sym == s)
			return ids;
	return nil;
}

#
# complete the declaration of an adt
# methods frames get sized in module definition or during function definition
# place the methods at the end of the field list
#
adtdefd(t: ref Type)
{
	next, aux, store, auxhd, tagnext: ref Decl;

	if(debug['x'])
		print("adt %s defd\n", typeconv(t));
	d := t.decl;
	tagnext = nil;
	store = nil;
	for(id := t.ids; id != nil; id = next){
		if(id.store == Dtag){
			if(t.tags != nil)
				error(id.src.start, "only one set of pick fields allowed");
			tagnext = pickdefd(t, id);
			next = tagnext;
			if(store != nil)
				store.next = next;
			else
				t.ids = next;
			continue;
		}else{
			id.dot = d;
			next = id.next;
			store = id;
		}
	}
	aux = nil;
	store = nil;
	auxhd = nil;
	seentags := 0;
	for(id = t.ids; id != nil; id = next){
		if(id == tagnext)
			seentags = 1;

		next = id.next;
		id.dot = d;
		id.ty = topvartype(verifytypes(id.ty, d), id, 1);
		if(id.store == Dfield && id.ty.kind == Tfn)
			id.store = Dfn;
		if(id.store == Dfn || id.store == Dconst){
			if(store != nil)
				store.next = next;
			else
				t.ids = next;
			if(aux != nil)
				aux.next = id;
			else
				auxhd = id;
			aux = id;
		}else{
			if(seentags)
				error(id.src.start, "pick fields must be the last data fields in an adt");
			store = id;
		}
	}
	if(aux != nil)
		aux.next = nil;
	if(store != nil)
		store.next = auxhd;
	else
		t.ids = auxhd;

	for(id = t.tags; id != nil; id = id.next){
		id.ty = verifytypes(id.ty, d);
		if(id.ty.tof == nil)
			id.ty.tof = mkadtpickcon(t, id.ty);
	}

}

#
# assemble the data structure for an adt with a pick clause.
# since the scoping rules for adt pick fields are strange,
# we have a cutomized check for overlapping defitions.
#
pickdefd(t: ref Type, tg: ref Decl): ref Decl
{
	lasttg : ref Decl = nil;
	d := t.decl;
	t.tags = tg;
	tag := 0;
	while(tg != nil){
		tt := tg.ty;
		if(tt.kind != Tadtpick || tg.tag != tag)
			break;
		tt.decl = tg;
		lasttg = tg;
		for(; tg != nil; tg = tg.next){
			if(tg.ty != tt)
				break;
			tag++;
			lasttg = tg;
			tg.dot = d;
		}
		for(id := tt.ids; id != nil; id = id.next){
			xid := namedot(t.ids, id.sym);
			if(xid != nil)
				error(id.src.start, "redeclaration of "+declconv(id)+
					" previously declared as "+storeconv(xid)+" on line "+lineconv(xid.src.start));
			id.dot = d;
		}
	}
	if(lasttg == nil){
		error(t.src.start, "empty pick field declaration in "+typeconv(t));
		t.tags = nil;
	}else
		lasttg.next = nil;
	d.tag = tag;
	return tg;
}

moddecl(ids: ref Decl, fields: ref Node): ref Node
{
	n := mkn(Omoddecl, mkn(Oseq, nil, nil), nil);
	t := mktype(ids.src.start, ids.src.stop, Tmodule, nil, nil);
	n.decl = ids;
	n.left = fields;
	n.ty = t;
	return n;
}

moddecled(n: ref Node)
{
	d := n.decl;
	installids(Dtype, d);
	isimp := 0;
	for(ids := d; ids != nil; ids = ids.next){
		if(ids.sym == impmod){
			isimp = 1;
			d = ids;
			impdecl = ids;
		}
		ids.ty = n.ty;
	}
	pushscope();
	fielddecled(n.left);

	d.ty.ids = popscope();

	#
	# make the current module the . parent of all contained decls.
	#
	for(ids = d.ty.ids; ids != nil; ids = ids.next)
		ids.dot = d;

	t := d.ty;
	t.decl = d;
	if(debug['m'])
		print("declare module %s\n", d.sym.name);

	#
	# add the iface declaration in case it's needed later
	#
	installids(Dglobal, mkids(d.src, enter(".m."+d.sym.name, 0), tnone, nil));

	if(isimp){
		for(ids = d.ty.ids; ids != nil; ids = ids.next){
			s := ids.sym;
			if(s.decl != nil && s.decl.scope >= scope){
				redecl(ids);
				ids.old = s.decl.old;
			}else
				ids.old = s.decl;
			s.decl = ids;
			ids.scope = scope;
		}
	}
}

#
# for each module in id,
# link by field ext all of the decls for
# functions needed in external linkage table
# collect globals and make a tuple for all of them
#
mkiface(m: ref Decl): ref Type
{
	iface := last := ref Decl;
	globals := glast := mkdecl(m.src, Dglobal, mktype(m.src.start, m.src.stop, Tadt, nil, nil));
	for(id := m.ty.ids; id != nil; id = id.next){
		case id.store{
		Dglobal =>
			glast = glast.next = dupdecl(id);
			id.iface = globals;
			glast.iface = id;
		Dfn =>
			id.iface = last = last.next = dupdecl(id);
			last.iface = id;
		Dtype =>
			if(id.ty.kind != Tadt)
				break;
			for(d := id.ty.ids; d != nil; d = d.next){
				if(d.store == Dfn){
					d.iface = last = last.next = dupdecl(d);
					last.iface = d;
				}
			}
		}
	}
	last.next = nil;
	iface = namesort(iface.next);

	if(globals.next != nil){
		glast.next = nil;
		globals.ty.ids = namesort(globals.next);
		globals.ty.decl = globals;
		globals.sym = enter(".mp", 0);
		globals.dot = m;
		globals.next = iface;
		iface = globals;
	}

	#
	# make the interface type and install an identifier for it
	# the iface has a ref count if it is loaded
	#
	t := mktype(m.src.start, m.src.stop, Tiface, nil, iface);
	id = enter(".m."+m.sym.name, 0).decl;
	t.decl = id;
	id.ty = t;

	#
	# dummy node so the interface is initialized
	#
	id.init = mkn(Onothing, nil, nil);
	id.init.ty = t;
	id.init.decl = id;
	return t;
}

joiniface(mt, t: ref Type)
{
	iface := t.ids;
	globals := iface;
	if(iface != nil && iface.store == Dglobal)
		iface = iface.next;
	for(id := mt.tof.ids; id != nil; id = id.next){
		case id.store{
		Dglobal =>
			for(d := id.ty.ids; d != nil; d = d.next)
				d.iface.iface = globals;
		Dfn =>
			id.iface.iface = iface;
			iface = iface.next;
		* =>
			fatal("unknown store "+storeconv(id)+" in joiniface");
		}
	}
	if(iface != nil)
		fatal("join iface not matched");
	mt.tof = t;
}

#
# eliminate unused declarations from interfaces
# label offset within interface
#
narrowmods()
{
	id: ref Decl;
	for(eq := modclass(); eq != nil; eq = eq.eq){
		t := eq.ty.tof;

		if(t.linkall == byte 0){
			last : ref Decl = nil;
			for(id = t.ids; id != nil; id = id.next){
				if(id.refs == 0){
					if(last == nil)
						t.ids = id.next;
					else
						last.next = id.next;
				}else
					last = id;
			}

			#
			# need to resize smaller interfaces
			#
			resizetype(t);
		}

		offset := 0;
		for(id = t.ids; id != nil; id = id.next)
			id.offset = offset++;

		#
		# rathole to stuff number of entries in interface
		#
		t.decl.init.c = ref Const;
		t.decl.init.c.val = big offset;
	}
}

#
# check to see if any data field of module m if referenced.
# if so, mark all data in m
#
moddataref()
{
	for(eq := modclass(); eq != nil; eq = eq.eq){
		id := eq.ty.tof.ids;
		if(id != nil && id.store == Dglobal && id.refs)
			for(id = eq.ty.ids; id != nil; id = id.next)
				if(id.store == Dglobal)
					modrefable(id.ty);
	}
}

#
# move the global declarations in interface to the front
#
modglobals(mod, globals: ref Decl): ref Decl
{
	#
	# make a copy of all the global declarations
	# 	used for making a type descriptor for globals ONLY
	# note we now have two declarations for the same variables,
	# which is apt to cause problems if code changes
	#
	# here we fix up the offsets for the real declarations
	#
	idoffsets(mod.ty.ids, 0, 1);

	last := head := ref Decl;
	for(id := mod.ty.ids; id != nil; id = id.next)
		if(id.store == Dglobal)
			last = last.next = dupdecl(id);

	last.next = globals;
	return head.next;
}

#
# snap all id type names to the actual type
# check that all types are completely defined
# verify that the types look ok
#
validtype(t: ref Type, inadt: ref Decl): ref Type
{
	if(t == nil)
		return t;
	bindtypes(t);
	t = verifytypes(t, inadt);
	cycsizetype(t);
	teqclass(t);
	return t;
}

usetype(t: ref Type): ref Type
{
	if(t == nil)
		return t;
	t = validtype(t, nil);
	reftype(t);
	return t;
}

#
# checks that t is a valid top-level type
#
topvartype(t: ref Type, id: ref Decl, tyok: int): ref Type
{
	if(t.kind == Tadt && t.tags != nil || t.kind == Tadtpick)
		error(id.src.start, "cannot declare "+id.sym.name+" with type "+typeconv(t));
	if(!tyok && t.kind == Tfn)
		error(id.src.start, "cannot declare "+id.sym.name+" to be a function");
	return t;
}

toptype(src: Src, t: ref Type): ref Type
{
	if(t.kind == Tadt && t.tags != nil || t.kind == Tadtpick)
		error(src.start, typeconv(t)+", an adt with pick fields, must be used with ref");
	if(t.kind == Tfn)
		error(src.start, "data cannot have a fn type like "+typeconv(t));
	return t;
}

usedty(t: ref Type)
{
	if(t != nil && (t.ok | OKmodref) != OKmask)
		fatal("used ty " + stypeconv(t) + " " + hex(int t.ok, 2));
}

bindtypes(t: ref Type)
{
	id: ref Decl;

	if(t == nil)
		return;
	if((t.ok & OKbind) == OKbind)
		return;
	t.ok |= OKbind;
	case t.kind{
	Tadt or
	Tadtpick or
	Tmodule or
	Terror or
	Tint or
	Tbig or
	Tstring or
	Treal or
	Tbyte or
	Tnone or
	Tany or
	Tiface or
	Tainit or
	Talt or
	Tcase or
	Tcasec or
	Tgoto =>
		break;
	Tarray or
	Tarrow or
	Tchan or
	Tdot or
	Tlist or
	Tref =>
		bindtypes(t.tof);
	Tid =>
		id = t.decl.sym.decl;
		if(id == nil)
			id = undefed(t.src, t.decl.sym);
		# save a little space
		id.sym.unbound = nil;
		t.decl = id;
	Ttuple =>
		for(id = t.ids; id != nil; id = id.next)
			bindtypes(id.ty);
	Tfn =>
		for(id = t.ids; id != nil; id = id.next)
			bindtypes(id.ty);
		bindtypes(t.tof);
	* =>
		fatal("bindtypes: unknown type kind "+string t.kind);
	}
}

#
# walk the type checking for validity
#
verifytypes(t: ref Type, adtt: ref Decl): ref Type
{
	id: ref Decl;

	if(t == nil)
		return nil;
	if((t.ok & OKverify) == OKverify)
		return t;
	t.ok |= OKverify;
if((t.ok & (OKverify|OKbind)) != (OKverify|OKbind))
fatal("verifytypes bogus ok for " + stypeconv(t));
	case t.kind{
	Terror or
	Tint or
	Tbig or
	Tstring or
	Treal or
	Tbyte or
	Tnone or
	Tany or
	Tiface or
	Tainit or
	Talt or
	Tcase or
	Tcasec or
	Tgoto =>
		break;
	Tref =>
		t.tof = verifytypes(t.tof, adtt);
		if(t.tof != nil && !tattr[t.tof.kind].refable){
			error(t.src.start, "cannot have a ref " + typeconv(t.tof));
			return terror;
		}
	Tchan or
	Tarray or
	Tlist =>
		t.tof = toptype(t.src, verifytypes(t.tof, adtt));
	Tid =>
		t.ok &= ~OKverify;
		return verifytypes(idtype(t), adtt);
	Tarrow =>
		t.ok &= ~OKverify;
		return verifytypes(arrowtype(t, adtt), adtt);
	Tdot =>
		#
		# verify the parent adt & lookup the tag fields
		#
		t.ok &= ~OKverify;
		return verifytypes(dottype(t, adtt), adtt);
	Tadt =>
		#
		# this is where Tadt may get tag fields added
		#
		adtdefd(t);
	Tadtpick =>
		for(id = t.ids; id != nil; id = id.next){
			id.ty = topvartype(verifytypes(id.ty, id.dot), id, 0);
			if(id.store == Dconst)
				error(t.src.start, "cannot declare a con like "+id.sym.name+" within a pick");
		}
	Tmodule =>
		for(id = t.ids; id != nil; id = id.next){
			id.ty = verifytypes(id.ty, nil);
			if(id.store == Dglobal && id.ty.kind == Tfn)
				id.store = Dfn;
			if(id.store != Dtype && id.store != Dfn)
				topvartype(id.ty, id, 0);
		}
	Ttuple =>
		if(t.decl == nil){
			t.decl = mkdecl(t.src, Dtype, t);
			t.decl.sym = anontupsym;
		}
		i := 0;
		for(id = t.ids; id != nil; id = id.next){
			id.store = Dfield;
			if(id.sym == nil)
				id.sym = enter("t"+string i, 0);
			i++;
			id.ty = toptype(id.src, verifytypes(id.ty, adtt));
		}
	Tfn =>
		last : ref Decl = nil;
		for(id = t.ids; id != nil; id = id.next){
			id.store = Darg;
			id.ty = topvartype(verifytypes(id.ty, adtt), id, 0);
			if(id.implicit != byte 0){
				if(adtt == nil)
					error(t.src.start, "function is not a member of an adt, so can't use self");
				else if(id != t.ids)
					error(id.src.start, "only the first argument can use self");
				else if(id.ty != adtt.ty && (id.ty.kind != Tref || id.ty.tof != adtt.ty))
					error(id.src.start, "self argument's type must be "+adtt.sym.name+" or ref "+adtt.sym.name);
			}
			last = id;
		}
		t.tof = toptype(t.src, verifytypes(t.tof, adtt));
		if(t.varargs != byte 0 && (last == nil || last.ty != tstring))
			error(t.src.start, "variable arguments must be preceeded by a string");
	* =>
		fatal("verifytypes: unknown type kind "+string t.kind);
	}
	return t;
}

#
# resolve an id type
#
idtype(t: ref Type): ref Type
{
	id := t.decl;
	if(id.store == Dunbound)
		fatal("idtype: unbound decl");
	tt := id.ty;
	if(id.store != Dtype && id.store != Dtag){
		if(id.store == Dundef){
			id.store = Dwundef;
			error(t.src.start, id.sym.name+" is not declared");
		}else if(id.store == Dimport){
			id.store = Dwundef;
			error(t.src.start, id.sym.name+"'s type cannot be determined");
		}else if(id.store != Dwundef)
			error(t.src.start, id.sym.name+" is not a type");
		return terror;
	}
	if(tt == nil){
		error(t.src.start, stypeconv(t)+" not fully defined");
		return terror;
	}
	return tt;
}

#
# resolve a -> qualified type
#
arrowtype(t: ref Type, adtt: ref Decl): ref Type
{
	id := t.decl;
	if(id.ty != nil){
		if(id.store == Dunbound)
			fatal("arrowtype: unbound decl has a type");
		return id.ty;
	}

	#
	# special hack to allow module variables to derive other types
	# 
	tt := t.tof;
	if(tt.kind == Tid){
		id = tt.decl;
		if(id.store == Dunbound)
			fatal("arrowtype: Tid's decl unbound");
		if(id.store == Dimport){
			id.store = Dwundef;
			error(t.src.start, id.sym.name+"'s type cannot be determined");
			return terror;
		}

		#
		# forward references to module variables can't be resolved
		#
		if(id.store != Dtype && (id.ty.ok & OKbind) != OKbind){
			error(t.src.start, id.sym.name+"'s type cannot be determined");
			return terror;
		}

		if(id.store == Dwundef)
			return terror;
		tt = id.ty = verifytypes(id.ty, adtt);
		if(tt == nil){
			error(t.tof.src.start, typeconv(t.tof)+" is not a module");
			return terror;
		}
	}else
		tt = verifytypes(t.tof, adtt);
	t.tof = tt;
	if(tt == terror)
		return terror;
	if(tt.kind != Tmodule){
		error(t.src.start, typeconv(tt)+" is not a module");
		return terror;
	}
	id = namedot(tt.ids, t.decl.sym);
	if(id == nil){
		error(t.src.start, t.decl.sym.name+" is not a member of "+typeconv(tt));
		return terror;
	}
	if(id.store == Dtype && id.ty != nil){
		t.decl = id;
		return id.ty;
	}
	error(t.src.start, typeconv(t)+" is not a type");
	return terror;
}

#
# resolve a . qualified type
#
dottype(t: ref Type, adtt: ref Decl): ref Type
{
	if(t.decl.ty != nil){
		if(t.decl.store == Dunbound)
			fatal("dottype: unbound decl has a type");
		return t.decl.ty;
	}
	t.tof = tt := verifytypes(t.tof, adtt);
	if(tt == terror)
		return terror;
	if(tt.kind != Tadt){
		error(t.src.start, typeconv(tt)+" is not an adt");
		return terror;
	}
	id := namedot(tt.tags, t.decl.sym);
	if(id != nil && id.ty != nil){
		t.decl = id;
		return id.ty;
	}
	error(t.src.start, t.decl.sym.name+" is not a pick tag of "+typeconv(tt));
	return terror;
}

#
# walk a type, putting all adts, modules, and tuples into equivalence classes
#
teqclass(t: ref Type)
{
	id: ref Decl;

	if(t == nil || (t.ok & OKclass) == OKclass)
		return;
	t.ok |= OKclass;
	case t.kind{
	Terror or
	Tint or
	Tbig or
	Tstring or
	Treal or
	Tbyte or
	Tnone or
	Tany or
	Tiface or
	Tainit or
	Talt or
	Tcase or
	Tcasec or
	Tgoto =>
		return;
	Tref =>
		teqclass(t.tof);
		return;
	Tchan or
	Tarray or
	Tlist =>
		teqclass(t.tof);
#ZZZ elim return to fix recursive chans, etc
		if(!debug['Z'])
			return;
	Tadt or
	Tadtpick or
	Ttuple =>
		for(id = t.ids; id != nil; id = id.next)
			teqclass(id.ty);
		for(tg := t.tags; tg != nil; tg = tg.next)
			teqclass(tg.ty);
	Tmodule =>
		t.tof = mkiface(t.decl);
		for(id = t.ids; id != nil; id = id.next)
			teqclass(id.ty);
	Tfn =>
		for(id = t.ids; id != nil; id = id.next)
			teqclass(id.ty);
		teqclass(t.tof);
		return;
	* =>
		fatal("teqclass: unknown type kind "+string t.kind);
	}

	#
	# find an equivalent type
	# stupid linear lookup could be made faster
	#
	if((t.ok & OKsized) != OKsized)
		fatal("eqclass type not sized: " + stypeconv(t));

	for(teq := eqclass[t.kind]; teq != nil; teq = teq.eq){
		if(t.size == teq.ty.size && tequal(t, teq.ty)){
			t.eq = teq;
			if(t.kind == Tmodule)
				joiniface(t, t.eq.ty.tof);
			return;
		}
	}

	#
	# if no equiv type, make one
	#
	eqclass[t.kind] = t.eq = ref Teq(0, t, eqclass[t.kind]);
}

#
# record that we've used the type
# using a type uses all types reachable from that type
#
reftype(t: ref Type)
{
	id: ref Decl;

	if(t == nil || (t.ok & OKref) == OKref)
		return;
	t.ok |= OKref;
	if(t.decl != nil && t.decl.refs == 0)
		t.decl.refs++;
	case t.kind{
	Terror or
	Tint or
	Tbig or
	Tstring or
	Treal or
	Tbyte or
	Tnone or
	Tany or
	Tiface or
	Tainit or
	Talt or
	Tcase or
	Tcasec or
	Tgoto =>
		break;
	Tref or
	Tchan or
	Tarray or
	Tlist =>
		if(t.decl != nil){
			if(nadts >= len adts){
				a := array[nadts + 32] of ref Decl;
				a[0:] = adts;
				adts = a;
			}
			adts[nadts++] = t.decl;
		}
		reftype(t.tof);
	Tadt or
	Tadtpick or
	Ttuple =>
		if(t.kind == Tadt || t.kind == Ttuple && t.decl.sym != anontupsym){
			if(nadts >= len adts){
				a := array[nadts + 32] of ref Decl;
				a[0:] = adts;
				adts = a;
			}
			adts[nadts++] = t.decl;
		}
		for(id = t.ids; id != nil; id = id.next)
			if(id.store != Dfn)
				reftype(id.ty);
		for(tg := t.tags; tg != nil; tg = tg.next)
			reftype(tg.ty);
	Tmodule =>
		#
		# a module's elements should get used individually
		#
		break;
	Tfn =>
		for(id = t.ids; id != nil; id = id.next)
			reftype(id.ty);
		reftype(t.tof);
	* =>
		fatal("reftype: unknown type kind "+string t.kind);
	}
}

#
# check all reachable types for cycles and illegal forward references
# find the size of all the types
#
cycsizetype(t: ref Type)
{
	id: ref Decl;

	if(t == nil || (t.ok & (OKcycsize|OKcyc|OKsized)) == (OKcycsize|OKcyc|OKsized))
		return;
	t.ok |= OKcycsize;
	case t.kind{
	Terror or
	Tint or
	Tbig or
	Tstring or
	Treal or
	Tbyte or
	Tnone or
	Tany or
	Tiface or
	Tainit or
	Talt or
	Tcase or
	Tcasec or
	Tgoto =>
		t.ok |= OKcyc;
		sizetype(t);
	Tref or
	Tchan or
	Tarray or
	Tlist =>
		cyctype(t);
		sizetype(t);
		cycsizetype(t.tof);
	Tadt or
	Ttuple =>
		cyctype(t);
		sizetype(t);
		for(id = t.ids; id != nil; id = id.next)
			cycsizetype(id.ty);
		for(tg := t.tags; tg != nil; tg = tg.next){
			if((tg.ty.ok & (OKcycsize|OKcyc|OKsized)) == (OKcycsize|OKcyc|OKsized))
				continue;
			tg.ty.ok |= (OKcycsize|OKcyc|OKsized);
			for(id = tg.ty.ids; id != nil; id = id.next)
				cycsizetype(id.ty);
		}
	Tadtpick =>
		t.ok &= ~OKcycsize;
		cycsizetype(t.decl.dot.ty);
	Tmodule =>
		cyctype(t);
		sizetype(t);
		for(id = t.ids; id != nil; id = id.next)
			cycsizetype(id.ty);
		sizeids(t.ids, 0);
	Tfn =>
		cyctype(t);
		sizetype(t);
		for(id = t.ids; id != nil; id = id.next)
			cycsizetype(id.ty);
		cycsizetype(t.tof);
		sizeids(t.ids, MaxTemp);
#ZZZ need to align?
	* =>
		fatal("cycsizetype: unknown type kind "+string t.kind);
	}
}

#
# marks for checking for arcs
#
	ArcValue,
	ArcList,
	ArcArray,
	ArcRef,
	ArcCyc,			# cycle found
	ArcXXX:
		con 1 << iota;

cyctype(t: ref Type)
{
	if((t.ok & OKcyc) == OKcyc)
		return;
	t.ok |= OKcyc;
	t.rec |= TRcyc;
	case t.kind{
	Terror or
	Tint or
	Tbig or
	Tstring or
	Treal or
	Tbyte or
	Tnone or
	Tany or
	Tfn or
	Tchan or
	Tarray or
	Tref or
	Tlist =>
		break;
	Tadt or
	Tmodule or
	Ttuple =>
		for(id := t.ids; id != nil; id = id.next)
			cycfield(t, id);
		for(tg := t.tags; tg != nil; tg = tg.next){
			if((tg.ty.ok & OKcyc) == OKcyc)
				continue;
			tg.ty.ok |= OKcyc;
			for(id = tg.ty.ids; id != nil; id = id.next)
				cycfield(t, id);
		}
	* =>
		fatal("cyctype: unknown type kind "+string t.kind);
	}
	t.rec &= ~TRcyc;
}

cycfield(base: ref Type, id: ref Decl)
{
	if(!storespace[id.store])
		return;
	arc := cycarc(base, id.ty);

	if((arc & (ArcCyc|ArcValue)) == (ArcCyc|ArcValue)){
		if(id.cycerr == byte 0)
			error(base.src.start, "illegal type cycle without a reference in field "
				+id.sym.name+" of "+stypeconv(base));
		id.cycerr = byte 1;
	}else if(arc & ArcCyc){
		if((arc & ArcArray) && id.cyc == byte 0){
			if(id.cycerr == byte 0)
				error(base.src.start, "illegal circular reference to type "+typeconv(id.ty)
					+" in field "+id.sym.name+" of "+stypeconv(base));
			id.cycerr = byte 1;
		}
		id.cycle = byte 1;
	}else if(id.cyc != byte 0){
		if(id.cycerr == byte 0)
			error(id.src.start, "spurious cyclic qualifier for field "+id.sym.name+" of "+stypeconv(base));
		id.cycerr = byte 1;
	}
}

cycarc(base, t: ref Type): int
{
	if(t == nil)
		return 0;
	if((t.rec & TRcyc) == TRcyc){
		if(tequal(t, base)){
			if(t.kind == Tmodule)
				return ArcCyc | ArcRef;
			else
				return ArcCyc | ArcValue;
		}
		return 0;
	}
	t.rec |= TRcyc;
	me := 0;
	case t.kind{
	Terror or
	Tint or
	Tbig or
	Tstring or
	Treal or
	Tbyte or
	Tnone or
	Tany or
	Tchan or
	Tfn =>
		break;
	Tarray =>
		me = cycarc(base, t.tof) & ~ArcValue | ArcArray;
	Tref =>
		me = cycarc(base, t.tof) & ~ArcValue | ArcRef;
	Tlist =>
		me = cycarc(base, t.tof) & ~ArcValue | ArcList;
	Tadt or
	Tadtpick or
	Tmodule or
	Ttuple =>
		me = 0;
		arc: int;
		for(id := t.ids; id != nil; id = id.next){
			if(!storespace[id.store])
				continue;
			arc = cycarc(base, id.ty);
			if((arc & ArcCyc) && id.cycerr == byte 0)
				me |= arc;
		}
		for(tg := t.tags; tg != nil; tg = tg.next){
			arc = cycarc(base, tg.ty);
			if((arc & ArcCyc) && tg.cycerr == byte 0)
				me |= arc;
		}

		if(t.kind == Tmodule)
			me = me & ArcCyc | ArcRef;
		else
			me &= ArcCyc | ArcValue;
	* =>
		fatal("cycarc: unknown type kind "+string t.kind);
	}
	t.rec &= ~TRcyc;
	return me;
}

#
# set the sizes and field offsets for t
# look only as deeply as needed to size this type.
# cycsize type will clean up the rest.
#
sizetype(t: ref Type)
{
	id: ref Decl;
	sz, al, s, a: int;

	if(t == nil)
		return;
	if((t.ok & OKsized) == OKsized)
		return;
	t.ok |= OKsized;
if((t.ok & (OKverify|OKsized)) != (OKverify|OKsized))
fatal("sizetype bogus ok for " + stypeconv(t));
	case t.kind{
	* =>
		fatal("sizetype: unknown type kind "+string t.kind);
	Terror or
	Tnone or
	Tbyte or
	Tint or
	Tbig or
	Tstring or
	Tany or
	Treal =>
		fatal(typeconv(t)+" should have a size");
	Tref or
	Tchan or
	Tarray or
	Tlist or
	Tmodule =>
		t.size = t.align = IBY2WD;
	Tadt or
	Ttuple =>
		if(t.tags == nil){
#ZZZ
			if(!debug['z']){
				(sz, t.align) = sizeids(t.ids, 0);
				t.size = align(sz, t.align);
			}else{
				(sz, nil) = sizeids(t.ids, 0);
				t.align = IBY2LG;
				t.size = align(sz, IBY2LG);
			}
			return;
		}
#ZZZ
		if(!debug['z']){
			(sz, al) = sizeids(t.ids, IBY2WD);
			if(al < IBY2WD)
				al = IBY2WD;
		}else{
			(sz, nil) = sizeids(t.ids, IBY2WD);
		}
		for(tg := t.tags; tg != nil; tg = tg.next){
			if((tg.ty.ok & OKsized) == OKsized)
				continue;
			tg.ty.ok |= OKsized;
#ZZZ
			if(!debug['z']){
				(s, a) = sizeids(tg.ty.ids, sz);
				if(a < al)
					a = al;
				tg.ty.size = align(s, a);
				tg.ty.align = a;
			}else{
				(s, nil) = sizeids(tg.ty.ids, sz);
				tg.ty.size = align(s, IBY2LG);
				tg.ty.align = IBY2LG;
			}			
		}
	Tfn =>
		t.size = 0;
		t.align = 1;
	Tainit =>
		t.size = 0;
		t.align = 1;
	Talt =>
		t.size = t.cse.nlab * 2*IBY2WD + 2*IBY2WD;
		t.align = IBY2WD;
	Tcase or
	Tcasec =>
		t.size = t.cse.nlab * 3*IBY2WD + 2*IBY2WD;
		t.align = IBY2WD;
	Tgoto =>
		t.size = t.cse.nlab * IBY2WD + IBY2WD;
		if(t.cse.iwild != nil)
			t.size += IBY2WD;
		t.align = IBY2WD;
	Tiface =>
		sz = IBY2WD;
		for(id = t.ids; id != nil; id = id.next){
			sz = align(sz, IBY2WD) + IBY2WD;
			sz += len array of byte id.sym.name + 1;
			if(id.dot.ty.kind == Tadt)
				sz += len array of byte id.dot.sym.name + 1;
		}
		t.size = sz;
		t.align = IBY2WD;
	}
}

sizeids(id: ref Decl, off: int): (int, int)
{
	al := 1;
	for(; id != nil; id = id.next){
		if(storespace[id.store]){
			sizetype(id.ty);
			#
			# alignment can be 0 if we have
			# illegal forward declarations.
			# just patch a; other code will flag an error
			#
			a := id.ty.align;
			if(a == 0)
				a = 1;

			if(a > al)
				al = a;

			off = align(off, a);
			id.offset = off;
			off += id.ty.size;
		}
	}
	return (off, al);
}

align(off, align: int): int
{
	if(align == 0)
		fatal("align 0");
	while(off % align)
		off++;
	return off;
}

#
# recalculate a type's size
#
resizetype(t: ref Type)
{
	if((t.ok & OKsized) == OKsized){
		t.ok &= ~OKsized;
		cycsizetype(t);
	}
}

#
# check if a module is accessable from t
# if so, mark that module interface
#
modrefable(t: ref Type)
{
	id: ref Decl;

	if(t == nil || (t.ok & OKmodref) == OKmodref)
		return;
	if((t.ok & OKverify) != OKverify)
		fatal("modrefable unused type "+stypeconv(t));
	t.ok |= OKmodref;
	case t.kind{
	Terror or
	Tint or
	Tbig or
	Tstring or
	Treal or
	Tbyte or
	Tnone or
	Tany =>
		break;
	Tchan or
	Tref or
	Tarray or
	Tlist =>
		modrefable(t.tof);
	Tmodule =>
		t.tof.linkall = byte 1;
		t.decl.refs++;
		for(id = t.ids; id != nil; id = id.next){
			case id.store{
			Dglobal or
			Dfn =>
				modrefable(id.ty);
			Dtype =>
				if(id.ty.kind != Tadt)
					break;
				for(m := id.ty.ids; m != nil; m = m.next)
					if(m.store == Dfn)
						modrefable(m.ty);
			}
		}
	Tfn or
	Tadt or
	Ttuple =>
		for(id = t.ids; id != nil; id = id.next)
			if(id.store != Dfn)
				modrefable(id.ty);
		for(tg := t.tags; tg != nil; tg = tg.next){
			if((tg.ty.ok & OKmodref) == OKmodref)
				continue;
			tg.ty.ok |= OKmodref;
			for(id = tg.ty.ids; id != nil; id = id.next)
				modrefable(id.ty);
		}
		modrefable(t.tof);
	Tadtpick =>
		modrefable(t.decl.dot.ty);
	* =>
		fatal("modrefable: unknown type kind "+string t.kind);
	}
}

gendesc(d: ref Decl, size: int, decls: ref Decl): ref Desc
{
	if(debug['D'])
		print("generate desc for %s\n", dotconv(d));
	return usedesc(mkdesc(size, decls));
}

mkdesc(size: int, d: ref Decl): ref Desc
{
	pmap := array[(size+8*IBY2WD-1) / (8*IBY2WD)] of { * => byte 0 };
	n := descmap(d, pmap, 0);
	if(n >= 0)
		n = n / (8*IBY2WD) + 1;
	else
		n = 0;
	return enterdesc(pmap, size, n);
}

mktdesc(t: ref Type): ref Desc
{
usedty(t);
	if(debug['D'])
		print("generate desc for %s\n", typeconv(t));
	if(t.decl == nil){
		t.decl = mkdecl(t.src, Dtype, t);
		t.decl.sym = enter("_mktdesc_", 0);
	}
	if(t.decl.desc != nil)
		return t.decl.desc;
	pmap := array[(t.size+8*IBY2WD-1) / (8*IBY2WD)] of {* => byte 0};
	n := tdescmap(t, pmap, 0);
	if(n >= 0)
		n = n / (8*IBY2WD) + 1;
	else
		n = 0;
	d := enterdesc(pmap, t.size, n);
	t.decl.desc = d;
	return d;
}

enterdesc(map: array of byte, size, nmap: int): ref Desc
{
	last : ref Desc = nil;
	for(d := descriptors; d != nil; d = d.next){
		if(d.size > size || d.size == size && d.nmap > nmap)
			break;
		if(d.size == size && d.nmap == nmap){
			c := mapcmp(d.map, map, nmap);
			if(c == 0)
				return d;
			if(c > 0)
				break;
		}
		last = d;
	}

	d = ref Desc(-1, 0, map, size, nmap, nil);
	if(last == nil){
		d.next = descriptors;
		descriptors = d;
	}else{
		d.next = last.next;
		last.next = d;
	}
	return d;
}

mapcmp(a, b: array of byte, n: int): int
{
	for(i := 0; i < n; i++)
		if(a[i] != b[i])
			return int a[i] - int b[i];
	return 0;
}

usedesc(d: ref Desc): ref Desc
{
	d.used = 1;
	return d;
}

#
# create the pointer description byte map for every type in decls
# each bit corresponds to a word, and is 1 if occupied by a pointer
# the high bit in the byte maps the first word
#
descmap(decls: ref Decl, map: array of byte, start: int): int
{
	if(debug['D'])
		print("descmap offset %d\n", start);
	last := -1;
	for(d := decls; d != nil; d = d.next){
		if(d.store == Dtype && d.ty.kind == Tmodule
		|| d.store == Dfn
		|| d.store == Dconst)
			continue;
		m := tdescmap(d.ty, map, d.offset + start);
		if(debug['D']){
			if(d.sym != nil)
				print("descmap %s type %s offset %d returns %d\n", d.sym.name, typeconv(d.ty), d.offset+start, m);
			else
				print("descmap type %s offset %d returns %d\n", typeconv(d.ty), d.offset+start, m);
		}
		if(m >= 0)
			last = m;
	}
	return last;
}

tdescmap(t: ref Type, map: array of byte, offset: int): int
{
	i, e, bit: int;

	if(t == nil)
		return -1;

	m := -1;
	if(t.kind == Talt){
		lab := t.cse.labs;
		e = t.cse.nlab;
		offset += IBY2WD * 2;
		for(i = 0; i < e; i++){
			if(lab[i].isptr){
				bit = offset / IBY2WD % 8;
				map[offset / (8*IBY2WD)] |= byte 1 << (7 - bit);
				m = offset;
			}
			offset += 2*IBY2WD;
		}
		return m;
	}
	if(t.kind == Tcasec){
		e = t.cse.nlab;
		offset += IBY2WD;
		for(i = 0; i < e; i++){
			bit = offset / IBY2WD % 8;
			map[offset / (8*IBY2WD)] |= byte 1 << (7 - bit);
			offset += IBY2WD;
			bit = offset / IBY2WD % 8;
			map[offset / (8*IBY2WD)] |= byte 1 << (7 - bit);
			m = offset;
			offset += 2*IBY2WD;
		}
		return m;
	}

	if(tattr[t.kind].isptr){
		bit = offset / IBY2WD % 8;
		map[offset / (8*IBY2WD)] |= byte 1 << (7 - bit);
		return offset;
	}
	if(t.kind == Tadtpick)
		t = t.tof;
	if(t.kind == Ttuple || t.kind == Tadt){
		if(debug['D'])
			print("descmap adt offset %d\n", offset);
		if(t.rec != byte 0)
			fatal("illegal cyclic type "+stypeconv(t)+" in tdescmap");
		t.rec = byte 1;
		offset = descmap(t.ids, map, offset);
		t.rec = byte 0;
		return offset;
	}

	return -1;
}

tcomset: int;

#
# can a t2 be assigned to a t1?
# any means Tany matches all types,
# not just references
#
tcompat(t1, t2: ref Type, any: int): int
{
	if(t1 == t2)
		return 1;
	tcomset = 0;
	ok := rtcompat(t1, t2, any);
	v := cleartcomrec(t1) + cleartcomrec(t2);
	if(v != tcomset)
		fatal("recid t1 "+stypeconv(t1)+" and t2 "+stypeconv(t2)+" not balanced in tcompat: "+string v+" "+string tcomset);
	return ok;
}

rtcompat(t1, t2: ref Type, any: int): int
{
	if(t1 == t2)
		return 1;
	if(t1 == nil || t2 == nil)
		return 0;
	if(t1.kind == Terror || t2.kind == Terror)
		return 1;

	t1.rec |= TRcom;
	t2.rec |= TRcom;
	case t1.kind{
	* =>
		fatal("unknown type "+stypeconv(t1)+" v "+stypeconv(t2)+" in rtcompat");
		return 0;
	Tstring =>
		return t2.kind == Tstring || t2.kind == Tany;
	Tnone or
	Tint or
	Tbig or
	Tbyte or
	Treal =>
		return t1.kind == t2.kind;
	Tany =>
		if(tattr[t2.kind].isptr)
			return 1;
		return any;
	Tref or
	Tlist or
	Tarray or
	Tchan =>
		if(t1.kind != t2.kind){
			if(t2.kind == Tany)
				return 1;
			return 0;
		}
		if(t1.kind != Tref && assumetcom(t1, t2))
			return 1;
		return rtcompat(t1.tof, t2.tof, 0);
	Tfn =>
		break;
	Ttuple =>
		if(t2.kind == Tadt && t2.tags == nil
		|| t2.kind == Ttuple){
			if(assumetcom(t1, t2))
				return 1;
			return idcompat(t1.ids, t2.ids, any);
		}
		if(t2.kind == Tadtpick){
			t2.tof.rec |= TRcom;
			if(assumetcom(t1, t2.tof))
				return 1;
			return idcompat(t1.ids, t2.tof.ids.next, any);
		}
		return 0;
	Tadt =>
		if(t2.kind == Ttuple && t1.tags == nil){
			if(assumetcom(t1, t2))
				return 1;
			return idcompat(t1.ids, t2.ids, any);
		}
		if(t1.tags != nil && t2.kind == Tadtpick)
			t2 = t2.decl.dot.ty;
	Tadtpick =>
		#if(t2.kind == Ttuple)
		#	return idcompat(t1.tof.ids.next, t2.ids, any);
		break;
	Tmodule =>
		if(t2.kind == Tany)
			return 1;
	}
	return tequal(t1, t2);
}

#
# add the assumption that t1 and t2 are compatable
#
assumetcom(t1, t2: ref Type): int
{
	r1, r2: ref Type;

	if(t1.tcom == nil && t2.tcom == nil){
		tcomset += 2;
		t1.tcom = t2.tcom = t1;
	}else{
		if(t1.tcom == nil){
			r1 = t1;
			t1 = t2;
			t2 = r1;
		}
		for(r1 = t1.tcom; r1 != r1.tcom; r1 = r1.tcom)
			;
		for(r2 = t2.tcom; r2 != nil && r2 != r2.tcom; r2 = r2.tcom)
			;
		if(r1 == r2)
			return 1;
		if(r2 == nil)
			tcomset++;
		t2.tcom = t1;
		for(; t2 != r1; t2 = r2){
			r2 = t2.tcom;
			t2.tcom = r1;
		}
	}
	return 0;
}

cleartcomrec(t: ref Type): int
{
	n := 0;
	for(; t != nil && (t.rec & TRcom) == TRcom; t = t.tof){
		t.rec &= ~TRcom;
		if(t.tcom != nil){
			t.tcom = nil;
			n++;
		}
		if(t.kind == Tadtpick)
			n += cleartcomrec(t.tof);
		if(t.kind == Tmodule)
			t = t.tof;
		for(id := t.ids; id != nil; id = id.next)
			n += cleartcomrec(id.ty);
		for(id = t.tags; id != nil; id = id.next)
			n += cleartcomrec(id.ty);
	}
	return n;
}

#
# id1 and id2 are the fields in an adt or tuple
# simple structural check; ignore names
#
idcompat(id1, id2: ref Decl, any: int): int
{
	for(; id1 != nil; id1 = id1.next){
		if(id1.store != Dfield)
			continue;
		while(id2 != nil && id2.store != Dfield)
			id2 = id2.next;
		if(id2 == nil
		|| id1.store != id2.store
		|| !rtcompat(id1.ty, id2.ty, any))
			return 0;
		id2 = id2.next;
	}
	while(id2 != nil && id2.store != Dfield)
		id2 = id2.next;
	return id2 == nil;
}

#
# structural equality on types
# t->recid is used to detect cycles
# t->rec is used to clear t->recid
#
tequal(t1, t2: ref Type): int
{
	eqrec = 0;
	eqset = 0;
	ok := rtequal(t1, t2);
	v := cleareqrec(t1) + cleareqrec(t2);
	if(v != eqset)
		fatal("recid t1 "+stypeconv(t1)+" and t2 "+stypeconv(t2)+" not balanced in tequal: "+string v+" "+string eqset);
	return ok;
}

rtequal(t1, t2: ref Type): int
{
	#
	# this is just a shortcut
	#
	if(t1 == t2)
		return 1;

	if(t1 == nil || t2 == nil)
		return 0;
	if(t1.kind == Terror || t2.kind == Terror)
		return 1;

	if(t1.kind != t2.kind)
		return 0;

	if(t1.eq != nil && t2.eq != nil)
		return t1.eq == t2.eq;

	t1.rec |= TReq;
	t2.rec |= TReq;
	case t1.kind{
	* =>
		fatal("bogus type "+stypeconv(t1)+" vs "+stypeconv(t2)+" in rtequal");
		return 0;
	Tnone or
	Tbig or
	Tbyte or
	Treal or
	Tint or
	Tstring =>
		#
		# this should always be caught by t1 == t2 check
		#
		fatal("bogus value type "+stypeconv(t1)+" vs "+stypeconv(t2)+" in rtequal");
		return 1;
	Tref or
	Tlist or
	Tarray or
	Tchan =>
		if(t1.kind != Tref && assumeteq(t1, t2))
			return 1;
		return rtequal(t1.tof, t2.tof);
	Tfn =>
		if(t1.varargs != t2.varargs)
			return 0;
		if(!idequal(t1.ids, t2.ids, 0, storespace))
			return 0;
		return rtequal(t1.tof, t2.tof);
	Ttuple =>
		if(assumeteq(t1, t2))
			return 1;
		return idequal(t1.ids, t2.ids, 0, storespace);
	Tadt or
	Tadtpick or
	Tmodule =>
		if(assumeteq(t1, t2))
			return 1;

		#
		# compare interfaces when comparing modules
		#
		if(t1.kind == Tmodule)
			return idequal(t1.tof.ids, t2.tof.ids, 1, nil);

		#
		# picked adts; check parent,
		# assuming equiv picked fields,
		# then check picked fields are equiv
		#
		if(t1.kind == Tadtpick && !rtequal(t1.decl.dot.ty, t2.decl.dot.ty))
			return 0;

		#
		# adts with pick tags: check picked fields for equality
		#
		if(!idequal(t1.tags, t2.tags, 1, nil))
			return 0;

		return idequal(t1.ids, t2.ids, 1, storespace);
	}
}

assumeteq(t1, t2: ref Type): int
{
	r1, r2: ref Type;

	if(t1.teq == nil && t2.teq == nil){
		eqrec++;
		eqset += 2;
		t1.teq = t2.teq = t1;
	}else{
		if(t1.teq == nil){
			r1 = t1;
			t1 = t2;
			t2 = r1;
		}
		for(r1 = t1.teq; r1 != r1.teq; r1 = r1.teq)
			;
		for(r2 = t2.teq; r2 != nil && r2 != r2.teq; r2 = r2.teq)
			;
		if(r1 == r2)
			return 1;
		if(r2 == nil)
			eqset++;
		t2.teq = t1;
		for(; t2 != r1; t2 = r2){
			r2 = t2.teq;
			t2.teq = r1;
		}
	}
	return 0;
}

#
# checking structural equality for modules, adts, tuples, and fns
#
idequal(id1, id2: ref Decl, usenames: int, storeok: array of int): int
{
	#
	# this is just a shortcut
	#
	if(id1 == id2)
		return 1;

	for(; id1 != nil; id1 = id1.next){
		if(storeok != nil && !storeok[id1.store])
			continue;
		while(id2 != nil && storeok != nil && !storeok[id2.store])
			id2 = id2.next;
		if(id2 == nil
		|| usenames && id1.sym != id2.sym
		|| id1.store != id2.store
		|| id1.implicit != id2.implicit
		|| id1.cyc != id2.cyc
		|| !rtequal(id1.ty, id2.ty))
			return 0;
		id2 = id2.next;
	}
	while(id2 != nil && storeok != nil && !storeok[id2.store])
		id2 = id2.next;
	return id1 == nil && id2 == nil;
}

cleareqrec(t: ref Type): int
{
	n := 0;
	for(; t != nil && (t.rec & TReq) == TReq; t = t.tof){
		t.rec &= ~TReq;
		if(t.teq != nil){
			t.teq = nil;
			n++;
		}
		if(t.kind == Tadtpick)
			n += cleareqrec(t.decl.dot.ty);
		if(t.kind == Tmodule)
			t = t.tof;
		for(id := t.ids; id != nil; id = id.next)
			n += cleareqrec(id.ty);
		for(id = t.tags; id != nil; id = id.next)
			n += cleareqrec(id.ty);
	}
	return n;
}

#
# create type signatures
# sign the same information used
# for testing type equality
#
sign(d: ref Decl): int
{
	t := d.ty;
	if(t.sig != 0)
		return t.sig;

	sigend := -1;
	sigalloc := 1024;
	sig: array of byte;
	while(sigend < 0 || sigend >= sigalloc){
		sigalloc *= 2;
		sig = array[sigalloc] of byte;
		eqrec = 0;
		sigend = rtsign(t, sig, 0);
		v := clearrec(t);
		if(v != eqrec)
			fatal("recid not balanced in sign: "+string v+" "+string eqrec);
		eqrec = 0;
	}

	if(signdump != "" && dotconv(d) == signdump){
		print("sign %s len %d\n", dotconv(d), sigend);
		print("%s\n", string sig[:sigend]);
	}

	md5sig := array[Keyring->MD5dlen] of {* => byte 0};
	md5(sig, sigend, md5sig, nil);

	for(i := 0; i < Keyring->MD5dlen; i += 4)
		t.sig ^= int md5sig[i+0] | (int md5sig[i+1]<<8) | (int md5sig[i+2]<<16) | (int md5sig[i+3]<<24);

	if(debug['S'])
		print("signed %s type %s len %d sig %#ux\n", dotconv(d), typeconv(t), sigend, t.sig);
	return t.sig;
}

SIGSELF:	con byte 'S';
SIGVARARGS:	con byte '*';
SIGCYC:		con byte 'y';
SIGREC:		con byte '@';

sigkind := array[Tend] of
{
	Tnone =>	byte 'n',
	Tadt =>		byte 'a',
	Tadtpick =>	byte 'p',
	Tarray =>	byte 'A',
	Tbig =>		byte 'B',
	Tbyte =>	byte 'b',
	Tchan =>	byte 'C',
	Treal =>	byte 'r',
	Tfn =>		byte 'f',
	Tint =>		byte 'i',
	Tlist =>	byte 'L',
	Tmodule =>	byte 'm',
	Tref =>		byte 'R',
	Tstring =>	byte 's',
	Ttuple =>	byte 't',

	* => 		byte 0,
};

rtsign(t: ref Type, sig: array of byte, spos: int): int
{
	id: ref Decl;

	if(t == nil)
		return spos;

	if(spos < 0 || spos + 8 >= len sig)
		return -1;

	if(t.eq != nil && t.eq.id){
		if(t.eq.id < 0 || t.eq.id > eqrec)
			fatal("sign rec "+typeconv(t)+" "+string t.eq.id+" "+string eqrec);

		sig[spos++] = SIGREC;
		name := array of byte string t.eq.id;
		if(spos + len name > len sig)
			return -1;
		sig[spos:] = name;
		spos += len name;
		return spos;
	}
	if(t.eq != nil){
		eqrec++;
		t.eq.id = eqrec;
	}

	kind := sigkind[t.kind];
	sig[spos++] = kind;
	if(kind == byte 0)
		fatal("no sigkind for "+typeconv(t));

	t.rec = byte 1;
	case t.kind{
	* =>
		fatal("bogus type "+stypeconv(t)+" in rtsign");
		return -1;
	Tnone or
	Tbig or
	Tbyte or
	Treal or
	Tint or
	Tstring =>
		return spos;
	Tref or
	Tlist or
	Tarray or
	Tchan =>
		return rtsign(t.tof, sig, spos);
	Tfn =>
		if(t.varargs != byte 0)
			sig[spos++] = SIGVARARGS;
		spos = idsign(t.ids, 0, sig, spos);
		return rtsign(t.tof, sig, spos);
	Ttuple =>
		return idsign(t.ids, 0, sig, spos);
	Tadt =>
		#
		# this is a little different than in rtequal,
		# since we flatten the adt we used to represent the globals
		#
		if(t.eq == nil){
			if(t.decl.sym.name != ".mp")
				fatal("no t.eq field for "+typeconv(t));
			spos--;
			for(id = t.ids; id != nil; id = id.next){
				spos = idsign1(id, 1, sig, spos);
				if(spos < 0 || spos >= len sig)
					return -1;
				sig[spos++] = byte ';';
			}
			return spos;
		}
		spos = idsign(t.ids, 1, sig, spos);
		if(spos < 0 || t.tags == nil)
			return spos;

		#
		# convert closing ')' to a ',', then sign any tags
		#
		sig[spos-1] = byte ',';
		for(tg := t.tags; tg != nil; tg = tg.next){
			name := array of byte (tg.sym.name + "=>");
			if(spos + len name > len sig)
				return -1;
			sig[spos:] = name;
			spos += len name;

			spos = rtsign(tg.ty, sig, spos);
			if(spos < 0 || spos >= len sig)
				return -1;

			if(tg.next != nil)
				sig[spos++] = byte ',';
		}
		if(spos >= len sig)
			return -1;
		sig[spos++] = byte ')';
		return spos;
	Tadtpick =>
		spos = idsign(t.ids, 1, sig, spos);
		if(spos < 0)
			return spos;
		return rtsign(t.decl.dot.ty, sig, spos);
	Tmodule =>
		if(t.tof.linkall == byte 0)
			fatal("signing a narrowed module");

		if(spos >= len sig)
			return -1;
		sig[spos++] = byte '{';
		for(id = t.tof.ids; id != nil; id = id.next){
			if(id.sym.name == ".mp"){
				spos = rtsign(id.ty, sig, spos);
				if(spos < 0)
					return -1;
				continue;
			}
			spos = idsign1(id, 1, sig, spos);
			if(spos < 0 || spos >= len sig)
				return -1;
			sig[spos++] = byte ';';
		}
		if(spos >= len sig)
			return -1;
		sig[spos++] = byte '}';
		return spos;
	}
}

idsign(id: ref Decl, usenames: int, sig: array of byte, spos: int): int
{
	if(spos >= len sig)
		return -1;
	sig[spos++] = byte '(';
	first := 1;
	for(; id != nil; id = id.next){
		if(id.store == Dlocal)
			fatal("local "+id.sym.name+" in idsign");

		if(!storespace[id.store])
			continue;

		if(!first){
			if(spos >= len sig)
				return -1;
			sig[spos++] = byte ',';
		}

		spos = idsign1(id, usenames, sig, spos);
		if(spos < 0)
			return -1;
		first = 0;
	}
	if(spos >= len sig)
		return -1;
	sig[spos++] = byte ')';
	return spos;
}

idsign1(id: ref Decl, usenames: int, sig: array of byte, spos: int): int
{
	if(usenames){
		name := array of byte (id.sym.name+":");
		if(spos + len name >= len sig)
			return -1;
		sig[spos:] = name;
		spos += len name;
	}

	if(spos + 2 >= len sig)
		return -1;

	if(id.implicit != byte 0)
		sig[spos++] = SIGSELF;

	if(id.cyc != byte 0)
		sig[spos++] = SIGCYC;

	return rtsign(id.ty, sig, spos);
}

clearrec(t: ref Type): int
{
	id: ref Decl;

	n := 0;
	for(; t != nil && t.rec != byte 0; t = t.tof){
		t.rec = byte 0;
		if(t.eq != nil && t.eq.id != 0){
			t.eq.id = 0;
			n++;
		}
		if(t.kind == Tmodule){
			for(id = t.tof.ids; id != nil; id = id.next)
				n += clearrec(id.ty);
			return n;
		}
		if(t.kind == Tadtpick)
			n += clearrec(t.decl.dot.ty);
		for(id = t.ids; id != nil; id = id.next)
			n += clearrec(id.ty);
		for(id = t.tags; id != nil; id = id.next)
			n += clearrec(id.ty);
	}
	return n;
}

typeconv(t: ref Type): string
{
	if(t == nil)
		return "nothing";
	return tprint(t);
}

stypeconv(t: ref Type): string
{
	if(t == nil)
		return "nothing";
	return stprint(t);
}

tprint(t: ref Type): string
{
	id: ref Decl;

	if(t == nil)
		return "";
	s := "";
	if(t.kind < 0 || t.kind >= Tend){
		s += "kind ";
		s += string t.kind;
		return s;
	}
	if(t.pr != byte 0 && t.decl != nil){
		if(t.decl.dot != nil && t.decl.dot.sym != impmod){
			s += t.decl.dot.sym.name;
			s += "->";
		}
		s += t.decl.sym.name;
		return s;
	}
	t.pr = byte 1;
	case t.kind{
	Tarrow =>
		s += tprint(t.tof);
		s += "->";
		s += t.decl.sym.name;
	Tdot =>
		s += tprint(t.tof);
		s += ".";
		s += t.decl.sym.name;
	Tid =>
		s += t.decl.sym.name;
	Tint or
	Tbig or
	Tstring or
	Treal or
	Tbyte or
	Tany or
	Tnone or
	Terror or
	Tainit or
	Talt or
	Tcase or
	Tcasec or
	Tgoto or
	Tiface =>
		s += kindname[t.kind];
	Tref =>
		s += "ref ";
		s += tprint(t.tof);
	Tchan or
	Tarray or
	Tlist =>
		s += kindname[t.kind];
		s += " of ";
		s += tprint(t.tof);
	Tadtpick =>
		s += t.decl.dot.sym.name + "." + t.decl.sym.name;
	Tadt =>
		if(t.decl.dot != nil && t.decl.dot.sym != impmod)
			s += t.decl.dot.sym.name + "->";
		s += t.decl.sym.name;
	Tmodule =>
		s += t.decl.sym.name;
	Ttuple =>
		s += "(";
		for(id = t.ids; id != nil; id = id.next){
			s += tprint(id.ty);
			if(id.next != nil)
				s += ", ";
		}
		s += ")";
	Tfn =>
		s += "fn(";
		for(id = t.ids; id != nil; id = id.next){
			if(id.sym == nil)
				s += "nil: ";
			else{
				s += id.sym.name;
				s += ": ";
			}
			if(id.implicit != byte 0)
				s += "self ";
			s += tprint(id.ty);
			if(id.next != nil)
				s += ", ";
		}
		if(t.varargs != byte 0 && t.ids != nil)
			s += ", *";
		else if(t.varargs != byte 0)
			s += "*";
		if(t.tof != nil && t.tof.kind != Tnone){
			s += "): ";
			s += tprint(t.tof);
		}else
			s += ")";
	* =>
		yyerror("tprint: unknown type kind "+string t.kind);
	}
	t.pr = byte 0;
	return s;
}

stprint(t: ref Type): string
{
	if(t == nil)
		return "";
	s := "";
	case t.kind{
	Tid =>
		s += "id ";
		s += t.decl.sym.name;
	Tadt or
	Tadtpick or
	Tmodule =>
		return kindname[t.kind] + " " + tprint(t);
	}
	return tprint(t);
}
