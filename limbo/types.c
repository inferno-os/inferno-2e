#include "limbo.h"
#include "libcrypt.h"

char *kindname[Tend] =
{
	/* Tnone */	"no type",
	/* Tadt */	"adt",
	/* Tadt */	"adt",
	/* Tarray */	"array",
	/* Tbig */	"big",
	/* Tbyte */	"byte",
	/* Tchan */	"chan",
	/* Treal */	"real",
	/* Tfn */	"fn",
	/* Tint */	"int",
	/* Tlist */	"list",
	/* Tmodule */	"module",
	/* Tref */	"ref",
	/* Tstring */	"string",
	/* Ttuple */	"tuple",

	/* Tainit */	"array initializers",
	/* Talt */	"alt channels",
	/* Tany */	"polymorphic type",
	/* Tarrow */	"->",
	/* Tcase */	"case int labels",
	/* Tcasec */	"case string labels",
	/* Tdot */	".",
	/* Terror */	"type error",
	/* Tgoto */	"goto labels",
	/* Tid */	"id",
	/* Tiface */	"module interface",
};

Tattr tattr[Tend] =
{
	/*		 isptr	refable	conable	big	vis */
	/* Tnone */	{ 0,	0,	0,	0,	0, },
	/* Tadt */	{ 0,	1,	0,	1,	1, },
	/* Tadtpick */	{ 0,	1,	0,	1,	1, },
	/* Tarray */	{ 1,	0,	0,	0,	1, },
	/* Tbig */	{ 0,	0,	1,	1,	1, },
	/* Tbyte */	{ 0,	0,	1,	0,	1, },
	/* Tchan */	{ 1,	0,	0,	0,	1, },
	/* Treal */	{ 0,	0,	1,	1,	1, },
	/* Tfn */	{ 0,	0,	0,	0,	1, },
	/* Tint */	{ 0,	0,	1,	0,	1, },
	/* Tlist */	{ 1,	0,	0,	0,	1, },
	/* Tmodule */	{ 1,	0,	0,	0,	1, },
	/* Tref */	{ 1,	0,	0,	0,	1, },
	/* Tstring */	{ 1,	0,	1,	0,	1, },
	/* Ttuple */	{ 0,	1,	0,	1,	1, },

	/* Tainit */	{ 0,	0,	0,	1,	0, },
	/* Talt */	{ 0,	0,	0,	1,	0, },
	/* Tany */	{ 1,	0,	0,	0,	0, },
	/* Tarrow */	{ 0,	0,	0,	0,	1, },
	/* Tcase */	{ 0,	0,	0,	1,	0, },
	/* Tcasec */	{ 0,	0,	0,	1,	0, },
	/* Tdot */	{ 0,	0,	0,	0,	1, },
	/* Terror */	{ 0,	1,	1,	0,	0, },
	/* Tgoto */	{ 0,	0,	0,	1,	0, },
	/* Tid */	{ 0,	0,	0,	0,	1, },
	/* Tiface */	{ 0,	0,	0,	1,	0, },
};

static	Teq	*eqclass[Tend];

static	Type	ztype;
static	int	eqrec;
static	int	eqset;
static	int	tcomset;

static	int	idcompat(Decl*, Decl*, int);
static	int	rtcompat(Type *t1, Type *t2, int any);
static	int	assumeteq(Type *t1, Type *t2);
static	int	assumetcom(Type *t1, Type *t2);
static	int	cleartcomrec(Type *t);
static	int	rtequal(Type*, Type*);
static	int	cleareqrec(Type*);
static	int	idequal(Decl*, Decl*, int, int*);
static	int	rtsign(Type*, uchar*, int, int);
static	int	clearrec(Type*);
static	int	idsign(Decl*, int, uchar*, int, int);
static	int	idsign1(Decl*, int, uchar*, int, int);

void
typeinit(void)
{
	anontupsym = enter(".tuple", 0);

	ztype.sbl = -1;
	ztype.ok = 0;
	ztype.rec = 0;

	tbig = mktype(&noline, &noline, Tbig, nil, nil);
	tbig->size = IBY2LG;
	tbig->align = IBY2LG;
	tbig->ok = OKmask;

	tbyte = mktype(&noline, &noline, Tbyte, nil, nil);
	tbyte->size = 1;
	tbyte->align = 1;
	tbyte->ok = OKmask;

	tint = mktype(&noline, &noline, Tint, nil, nil);
	tint->size = IBY2WD;
	tint->align = IBY2WD;
	tint->ok = OKmask;

	treal = mktype(&noline, &noline, Treal, nil, nil);
	treal->size = IBY2FT;
	treal->align = IBY2FT;
	treal->ok = OKmask;

	tstring = mktype(&noline, &noline, Tstring, nil, nil);
	tstring->size = IBY2WD;
	tstring->align = IBY2WD;
	tstring->ok = OKmask;

	tany = mktype(&noline, &noline, Tany, nil, nil);
	tany->size = IBY2WD;
	tany->align = IBY2WD;
	tany->ok = OKmask;

	tnone = mktype(&noline, &noline, Tnone, nil, nil);
	tnone->size = 0;
	tnone->align = 1;
	tnone->ok = OKmask;

	terror = mktype(&noline, &noline, Terror, nil, nil);
	terror->size = 0;
	terror->align = 1;
	terror->ok = OKmask;

	tunknown = mktype(&noline, &noline, Terror, nil, nil);
	tunknown->size = 0;
	tunknown->align = 1;
	tunknown->ok = OKmask;
}

void
typestart(void)
{
	descriptors = nil;
	nfns = 0;
	nadts = 0;

	memset(eqclass, 0, sizeof eqclass);

	typebuiltin(mkids(&nosrc, enter("int", 0), nil, nil), tint);
	typebuiltin(mkids(&nosrc, enter("big", 0), nil, nil), tbig);
	typebuiltin(mkids(&nosrc, enter("byte", 0), nil, nil), tbyte);
	typebuiltin(mkids(&nosrc, enter("string", 0), nil, nil), tstring);
	typebuiltin(mkids(&nosrc, enter("real", 0), nil, nil), treal);
}

Teq*
modclass(void)
{
	return eqclass[Tmodule];
}

Type*
mktype(Line *start, Line *stop, int kind, Type *tof, Decl *args)
{
	Type *t;

	t = allocmem(sizeof *t);
	*t = ztype;
	t->src.start = *start;
	t->src.stop = *stop;
	t->kind = kind;
	t->tof = tof;
	t->ids = args;
	return t;
}

Type*
mktalt(Case *c)
{
	Type *t;
	char buf[32];
	static int nalt;

	t = mktype(&noline, &noline, Talt, nil, nil);
	t->decl = mkdecl(&nosrc, Dtype, t);
	seprint(buf, buf+sizeof(buf), ".a%d", nalt++);
	t->decl->sym = enter(buf, 0);
	t->cse = c;
	return usetype(t);
}

/*
 * copy t and the top level of ids
 */
Type*
copytypeids(Type *t)
{
	Type *nt;
	Decl *id, *new, *last;

	nt = allocmem(sizeof *nt);
	*nt = *t;
	last = nil;
	for(id = t->ids; id != nil; id = id->next){
		new = allocmem(sizeof *id);
		*new = *id;
		if(last == nil)
			nt->ids = new;
		else
			last->next = new;
		last = new;
	}
	return nt;
}

/*
 * make each of the ids have type t
 */
Decl*
typeids(Decl *ids, Type *t)
{
	Decl *id;

	if(ids == nil)
		return nil;

	ids->ty = t;
	for(id = ids->next; id != nil; id = id->next){
		id->ty = t;
	}
	return ids;
}

void
typebuiltin(Decl *d, Type *t)
{
	d->ty = t;
	t->decl = d;
	installids(Dtype, d);
}

Node *
fielddecl(int store, Decl *ids)
{
	Node *n;

	n = mkn(Ofielddecl, nil, nil);
	n->decl = ids;
	for(ids; ids != nil; ids = ids->next)
		ids->store = store;
	return n;
}

Node *
typedecl(Decl *ids, Type *t)
{
	Node *n;

	if(t->decl == nil)
		t->decl = ids;
	n = mkn(Otypedecl, nil, nil);
	n->decl = ids;
	n->ty = t;
	for(; ids != nil; ids = ids->next)
		ids->ty = t;
	return n;
}

void
typedecled(Node *n)
{
	installids(Dtype, n->decl);
}

Node *
adtdecl(Decl *ids, Node *fields)
{
	Node *n;
	Type *t;

	n = mkn(Oadtdecl, nil, nil);
	t = mktype(&ids->src.start, &ids->src.stop, Tadt, nil, nil);
	n->decl = ids;
	n->left = fields;
	n->ty = t;
	t->decl = ids;
	for(; ids != nil; ids = ids->next)
		ids->ty = t;
	return n;
}

void
adtdecled(Node *n)
{
	Decl *d, *ids;

	d = n->ty->decl;
	installids(Dtype, d);
	pushscope();
	fielddecled(n->left);
	n->ty->ids = popscope();
	for(ids = n->ty->ids; ids != nil; ids = ids->next)
		ids->dot = d;
}

void
fielddecled(Node *n)
{
	for(; n != nil; n = n->right){
		switch(n->op){
		case Oseq:
			fielddecled(n->left);
			break;
		case Oadtdecl:
			adtdecled(n);
			return;
		case Otypedecl:
			typedecled(n);
			return;
		case Ofielddecl:
			installids(Dfield, n->decl);
			return;
		case Ocondecl:
			condecled(n);
			gdasdecl(n->right);
			return;
		case Opickdecl:
			pickdecled(n);
			return;
		default:
			fatal("can't deal with %O in fielddecled", n->op);
		}
	}
}

int
pickdecled(Node *n)
{
	Decl *d;
	int tag;

	if(n == nil)
		return 0;
	tag = pickdecled(n->left);
	pushscope();
	fielddecled(n->right->right);
	d = n->right->left->decl;
	d->ty->ids = popscope();
	installids(Dtag, d);
	for(; d != nil; d = d->next)
		d->tag = tag++;
	return tag;
}

/*
 * make the tuple type used to initialize adt t
 */
Type*
mkadtcon(Type *t)
{
	Decl *id, *new, *last;
	Type *nt;

	nt = allocmem(sizeof *nt);
	*nt = *t;
	last = nil;
	nt->ids = nil;
	nt->kind = Ttuple;
	for(id = t->ids; id != nil; id = id->next){
		if(id->store != Dfield)
			continue;
		new = allocmem(sizeof *id);
		*new = *id;
		new->cyc = 0;
		if(last == nil)
			nt->ids = new;
		else
			last->next = new;
		last = new;
	}
	last->next = nil;
	return nt;
}

/*
 * make the tuple type used to initialize t,
 * an adt with pick fields tagged by tg
 */
Type*
mkadtpickcon(Type *t, Type *tgt)
{
	Decl *id, *new, *last;
	Type *nt;

	last = mkids(&tgt->decl->src, nil, tint, nil);
	last->store = Dfield;
	nt = mktype(&t->src.start, &t->src.stop, Ttuple, nil, last);
	for(id = t->ids; id != nil; id = id->next){
		if(id->store != Dfield)
			continue;
		new = allocmem(sizeof *id);
		*new = *id;
		new->cyc = 0;
		last->next = new;
		last = new;
	}
	for(id = tgt->ids; id != nil; id = id->next){
		if(id->store != Dfield)
			continue;
		new = allocmem(sizeof *id);
		*new = *id;
		new->cyc = 0;
		last->next = new;
		last = new;
	}
	last->next = nil;
	return nt;
}

/*
 * make an identifier type
 */
Type*
mkidtype(Src *src, Sym *s)
{
	Type *t;

	t = mktype(&src->start, &src->stop, Tid, nil, nil);
	if(s->unbound == nil){
		s->unbound = mkdecl(src, Dunbound, nil);
		s->unbound->sym = s;
	}
	t->decl = s->unbound;
	return t;
}

/*
 * make a qualified type for t->s
 */
Type*
mkarrowtype(Line *start, Line *stop, Type *t, Sym *s)
{
	Src src;

	src.start = *start;
	src.stop = *stop;
	t = mktype(start, stop, Tarrow, t, nil);
	if(s->unbound == nil){
		s->unbound = mkdecl(&src, Dunbound, nil);
		s->unbound->sym = s;
	}
	t->decl = s->unbound;
	return t;
}

/*
 * make a qualified type for t.s
 */
Type*
mkdottype(Line *start, Line *stop, Type *t, Sym *s)
{
	Src src;

	src.start = *start;
	src.stop = *stop;
	t = mktype(start, stop, Tdot, t, nil);
	if(s->unbound == nil){
		s->unbound = mkdecl(&src, Dunbound, nil);
		s->unbound->sym = s;
	}
	t->decl = s->unbound;
	return t;
}

/*
 * look up the name f in the fields of a module, adt, or tuple
 */
Decl*
namedot(Decl *ids, Sym *s)
{
	for(; ids != nil; ids = ids->next)
		if(ids->sym == s)
			return ids;
	return nil;
}

/*
 * complete the declaration of an adt
 * methods frames get sized in module definition or during function definition
 * place the methods at the end of the field list
 */
void
adtdefd(Type *t)
{
	Decl *d, *id, *next, *aux, *store, *auxhd, *tagnext;
	int seentags;

	if(debug['x'])
		print("adt %T defd\n", t);
	d = t->decl;
	tagnext = nil;
	store = nil;
	for(id = t->ids; id != nil; id = next){
		if(id->store == Dtag){
			if(t->tags != nil)
				error(id->src.start, "only one set of pick fields allowed");
			tagnext = pickdefd(t, id);
			next = tagnext;
			if(store != nil)
				store->next = next;
			else
				t->ids = next;
			continue;
		}else{
			id->dot = d;
			next = id->next;
			store = id;
		}
	}
	aux = nil;
	store = nil;
	auxhd = nil;
	seentags = 0;
	for(id = t->ids; id != nil; id = next){
		if(id == tagnext)
			seentags = 1;

		next = id->next;
		id->dot = d;
		id->ty = topvartype(verifytypes(id->ty, d), id, 1);
		if(id->store == Dfield && id->ty->kind == Tfn)
			id->store = Dfn;
		if(id->store == Dfn || id->store == Dconst){
			if(store != nil)
				store->next = next;
			else
				t->ids = next;
			if(aux != nil)
				aux->next = id;
			else
				auxhd = id;
			aux = id;
		}else{
			if(seentags)
				error(id->src.start, "pick fields must be the last data fields in an adt");
			store = id;
		}
	}
	if(aux != nil)
		aux->next = nil;
	if(store != nil)
		store->next = auxhd;
	else
		t->ids = auxhd;

	for(id = t->tags; id != nil; id = id->next){
		id->ty = verifytypes(id->ty, d);
		if(id->ty->tof == nil)
			id->ty->tof = mkadtpickcon(t, id->ty);
	}
}

/*
 * assemble the data structure for an adt with a pick clause.
 * since the scoping rules for adt pick fields are strange,
 * we have a cutomized check for overlapping defitions.
 */
Decl*
pickdefd(Type *t, Decl *tg)
{
	Decl *id, *xid, *lasttg, *d;
	Type *tt;
	int tag;

	lasttg = nil;
	d = t->decl;
	t->tags = tg;
	tag = 0;
	while(tg != nil){
		tt = tg->ty;
		if(tt->kind != Tadtpick || tg->tag != tag)
			break;
		tt->decl = tg;
		lasttg = tg;
		for(; tg != nil; tg = tg->next){
			if(tg->ty != tt)
				break;
			tag++;
			lasttg = tg;
			tg->dot = d;
		}
		for(id = tt->ids; id != nil; id = id->next){
			xid = namedot(t->ids, id->sym);
			if(xid != nil)
				error(id->src.start, "redeclaration of %K, previously declared as %k on line %L",
					id, xid, xid->src.start);
			id->dot = d;
		}
	}
	if(lasttg == nil){
		error(t->src.start, "empty pick field declaration in %T", t);
		t->tags = nil;
	}else
		lasttg->next = nil;
	d->tag = tag;
	return tg;
}

Node*
moddecl(Decl *ids, Node *fields)
{
	Node *n;
	Type *t;

	n = mkn(Omoddecl, mkn(Oseq, nil, nil), nil);
	t = mktype(&ids->src.start, &ids->src.stop, Tmodule, nil, nil);
	n->decl = ids;
	n->left = fields;
	n->ty = t;
	return n;
}

void
moddecled(Node *n)
{
	Decl *d, *ids;
	Type *t;
	Sym *s;
	char buf[StrSize];
	int isimp;

	d = n->decl;
	installids(Dtype, d);
	isimp = 0;
	for(ids = d; ids != nil; ids = ids->next){
		if(ids->sym == impmod){
			isimp = 1;
			d = ids;
			impdecl = ids;
		}
		ids->ty = n->ty;
	}
	pushscope();
	fielddecled(n->left);

	d->ty->ids = popscope();

	/*
	 * make the current module the -> parent of all contained decls->
	 */
	for(ids = d->ty->ids; ids != nil; ids = ids->next)
		ids->dot = d;

	t = d->ty;
	t->decl = d;
	if(debug['m'])
		print("declare module %s\n", d->sym->name);

	/*
	 * add the iface declaration in case it's needed later
	 */
	seprint(buf, buf+sizeof(buf), ".m.%s", d->sym->name);
	installids(Dglobal, mkids(&d->src, enter(buf, 0), tnone, nil));

	if(isimp){
		for(ids = d->ty->ids; ids != nil; ids = ids->next){
			s = ids->sym;
			if(s->decl != nil && s->decl->scope >= scope){
				redecl(ids);
				ids->old = s->decl->old;
			}else
				ids->old = s->decl;
			s->decl = ids;
			ids->scope = scope;
		}
	}
}

/*
 * for each module in id,
 * link by field ext all of the decls for
 * functions needed in external linkage table
 * collect globals and make a tuple for all of them
 */
Type*
mkiface(Decl *m)
{
	Decl *iface, *last, *globals, *glast, *id, *d;
	Type *t;
	char buf[StrSize];

	iface = last = allocmem(sizeof(Decl));
	globals = glast = mkdecl(&m->src, Dglobal, mktype(&m->src.start, &m->src.stop, Tadt, nil, nil));
	for(id = m->ty->ids; id != nil; id = id->next){
		switch(id->store){
		case Dglobal:
			glast = glast->next = dupdecl(id);
			id->iface = globals;
			glast->iface = id;
			break;
		case Dfn:
			id->iface = last = last->next = dupdecl(id);
			last->iface = id;
			break;
		case Dtype:
			if(id->ty->kind != Tadt)
				break;
			for(d = id->ty->ids; d != nil; d = d->next){
				if(d->store == Dfn){
					d->iface = last = last->next = dupdecl(d);
					last->iface = d;
				}
			}
			break;
		}
	}
	last->next = nil;
	iface = namesort(iface->next);

	if(globals->next != nil){
		glast->next = nil;
		globals->ty->ids = namesort(globals->next);
		globals->ty->decl = globals;
		globals->sym = enter(".mp", 0);
		globals->dot = m;
		globals->next = iface;
		iface = globals;
	}

	/*
	 * make the interface type and install an identifier for it
	 * the iface has a ref count if it is loaded
	 */
	t = mktype(&m->src.start, &m->src.stop, Tiface, nil, iface);
	seprint(buf, buf+sizeof(buf), ".m.%s", m->sym->name);
	id = enter(buf, 0)->decl;
	t->decl = id;
	id->ty = t;

	/*
	 * dummy node so the interface is initialized
	 */
	id->init = mkn(Onothing, nil, nil);
	id->init->ty = t;
	id->init->decl = id;
	return t;
}

void
joiniface(Type *mt, Type *t)
{
	Decl *id, *d, *iface, *globals;

	iface = t->ids;
	globals = iface;
	if(iface != nil && iface->store == Dglobal)
		iface = iface->next;
	for(id = mt->tof->ids; id != nil; id = id->next){
		switch(id->store){
		case Dglobal:
			for(d = id->ty->ids; d != nil; d = d->next)
				d->iface->iface = globals;
			break;
		case Dfn:
			id->iface->iface = iface;
			iface = iface->next;
			break;
		default:
			fatal("unknown store %k in joiniface", id);
			break;
		}
	}
	if(iface != nil)
		fatal("join iface not matched");
	mt->tof = t;
}

/*
 * eliminate unused declarations from interfaces
 * label offset within interface
 */
void
narrowmods(void)
{
	Teq *eq;
	Decl *id, *last;
	Type *t;
	long offset;

	for(eq = modclass(); eq != nil; eq = eq->eq){
		t = eq->ty->tof;

		if(t->linkall == 0){
			last = nil;
			for(id = t->ids; id != nil; id = id->next){
				if(id->refs == 0){
					if(last == nil)
						t->ids = id->next;
					else
						last->next = id->next;
				}else
					last = id;
			}

			/*
			 * need to resize smaller interfaces
			 */
			resizetype(t);
		}

		offset = 0;
		for(id = t->ids; id != nil; id = id->next)
			id->offset = offset++;

		/*
		 * rathole to stuff number of entries in interface
		 */
		t->decl->init->val = offset;
	}
}

/*
 * check to see if any data field of module m if referenced.
 * if so, mark all data in m
 */
void
moddataref(void)
{
	Teq *eq;
	Decl *id;

	for(eq = modclass(); eq != nil; eq = eq->eq){
		id = eq->ty->tof->ids;
		if(id != nil && id->store == Dglobal && id->refs)
			for(id = eq->ty->ids; id != nil; id = id->next)
				if(id->store == Dglobal)
					modrefable(id->ty);
	}
}

/*
 * move the global declarations in interface to the front
 */
Decl*
modglobals(Decl *mod, Decl *globals)
{
	Decl *id, *head, *last;

	/*
	 * make a copy of all the global declarations
	 * 	used for making a type descriptor for globals ONLY
	 * note we now have two declarations for the same variables,
	 * which is apt to cause problems if code changes
	 *
	 * here we fix up the offsets for the real declarations
	 */
	idoffsets(mod->ty->ids, 0, 1);

	last = head = allocmem(sizeof(Decl));
	for(id = mod->ty->ids; id != nil; id = id->next)
		if(id->store == Dglobal)
			last = last->next = dupdecl(id);

	last->next = globals;
	return head->next;
}

/*
 * snap all id type names to the actual type
 * check that all types are completely defined
 * verify that the types look ok
 */
Type*
validtype(Type *t, Decl *inadt)
{
	if(t == nil)
		return t;
	bindtypes(t);
	t = verifytypes(t, inadt);
	cycsizetype(t);
	teqclass(t);
	return t;
}

Type*
usetype(Type *t)
{
	if(t == nil)
		return t;
	t = validtype(t, nil);
	reftype(t);
	return t;
}

/*
 * checks that t is a valid top-level type
 */
Type*
topvartype(Type *t, Decl *id, int tyok)
{
	if(t->kind == Tadt && t->tags != nil || t->kind == Tadtpick)
		error(id->src.start, "cannot declare %s with type %T", id->sym->name, t);
	if(!tyok && t->kind == Tfn)
		error(id->src.start, "cannot declare %s to be a function", id->sym->name);
	return t;
}

Type*
toptype(Src *src, Type *t)
{
	if(t->kind == Tadt && t->tags != nil || t->kind == Tadtpick)
		error(src->start, "%T, an adt with pick fields, must be used with ref", t);
	if(t->kind == Tfn)
		error(src->start, "data cannot have a fn type like %T", t);
	return t;
}

void
usedty(Type *t)
{
	if(t != nil && (t->ok | OKmodref) != OKmask)
		fatal("used ty %t %2.2ux", t, t->ok);
}

void
bindtypes(Type *t)
{
	Decl *id;

	if(t == nil)
		return;
	if((t->ok & OKbind) == OKbind)
		return;
	t->ok |= OKbind;
	switch(t->kind){
	case Tadt:
	case Tadtpick:
	case Tmodule:
	case Terror:
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tnone:
	case Tany:
	case Tiface:
	case Tainit:
	case Talt:
	case Tcase:
	case Tcasec:
	case Tgoto:
		break;
	case Tarray:
	case Tarrow:
	case Tchan:
	case Tdot:
	case Tlist:
	case Tref:
		bindtypes(t->tof);
		break;
	case Tid:
		id = t->decl->sym->decl;
		if(id == nil)
			id = undefed(&t->src, t->decl->sym);
		/* save a little space */
		id->sym->unbound = nil;
		t->decl = id;
		break;
	case Ttuple:
		for(id = t->ids; id != nil; id = id->next)
			bindtypes(id->ty);
		break;
	case Tfn:
		for(id = t->ids; id != nil; id = id->next)
			bindtypes(id->ty);
		bindtypes(t->tof);
		break;
	default:
		fatal("bindtypes: unknown type kind %d", t->kind);
	}
}

/*
 * walk the type checking for validity
 */
Type*
verifytypes(Type *t, Decl *adtt)
{
	Decl *id, *last;
	char buf[32];
	int i;

	if(t == nil)
		return nil;
	if((t->ok & OKverify) == OKverify)
		return t;
	t->ok |= OKverify;
if((t->ok & (OKverify|OKbind)) != (OKverify|OKbind))
fatal("verifytypes bogus ok for %t", t);
	switch(t->kind){
	case Terror:
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tnone:
	case Tany:
	case Tiface:
	case Tainit:
	case Talt:
	case Tcase:
	case Tcasec:
	case Tgoto:
		break;
	case Tref:
		t->tof = verifytypes(t->tof, adtt);
		if(t->tof != nil && !tattr[t->tof->kind].refable){
			error(t->src.start, "cannot have a ref %T", t->tof);
			return terror;
		}
		break;
	case Tchan:
	case Tarray:
	case Tlist:
		t->tof = toptype(&t->src, verifytypes(t->tof, adtt));
		break;
	case Tid:
		t->ok &= ~OKverify;
		return verifytypes(idtype(t), adtt);
	case Tarrow:
		t->ok &= ~OKverify;
		return verifytypes(arrowtype(t, adtt), adtt);
	case Tdot:
		/*
		 * verify the parent adt & lookup the tag fields
		 */
		t->ok &= ~OKverify;
		return verifytypes(dottype(t, adtt), adtt);
	case Tadt:
		/*
		 * this is where Tadt may get tag fields added
		 */
		adtdefd(t);
		break;
	case Tadtpick:
		for(id = t->ids; id != nil; id = id->next){
			id->ty = topvartype(verifytypes(id->ty, id->dot), id, 0);
			if(id->store == Dconst)
				error(t->src.start, "pick fields cannot be a con like %s", id->sym->name);
		}
		break;
	case Tmodule:
		for(id = t->ids; id != nil; id = id->next){
			id->ty = verifytypes(id->ty, nil);
			if(id->store == Dglobal && id->ty->kind == Tfn)
				id->store = Dfn;
			if(id->store != Dtype && id->store != Dfn)
				topvartype(id->ty, id, 0);
		}
 		break;
	case Ttuple:
		if(t->decl == nil){
			t->decl = mkdecl(&t->src, Dtype, t);
			t->decl->sym = enter(".tuple", 0);
		}
		i = 0;
		for(id = t->ids; id != nil; id = id->next){
			id->store = Dfield;
			if(id->sym == nil){
				seprint(buf, buf+sizeof(buf), "t%d", i);
				id->sym = enter(buf, 0);
			}
			i++;
			id->ty = toptype(&id->src, verifytypes(id->ty, adtt));
		}
		break;
	case Tfn:
		last = nil;
		for(id = t->ids; id != nil; id = id->next){
			id->store = Darg;
			id->ty = topvartype(verifytypes(id->ty, adtt), id, 0);
			if(id->implicit){
				if(adtt == nil)
					error(t->src.start, "function is not a member of an adt, so can't use self");
				else if(id != t->ids)
					error(id->src.start, "only the first argument can use self");
				else if(id->ty != adtt->ty && (id->ty->kind != Tref || id->ty->tof != adtt->ty))
					error(id->src.start, "self argument's type must be %s or ref %s",
						adtt->sym->name, adtt->sym->name);
			}
			last = id;
		}
		t->tof = toptype(&t->src, verifytypes(t->tof, adtt));
		if(t->varargs && (last == nil || last->ty != tstring))
			error(t->src.start, "variable arguments must be preceeded by a string");
		break;
	default:
		fatal("verifytypes: unknown type kind %d", t->kind);
	}
	return t;
}

/*
 * resolve an id type
 */
Type*
idtype(Type *t)
{
	Decl *id;
	Type *tt;

	id = t->decl;
	if(id->store == Dunbound)
		fatal("idtype: unbound decl");
	tt = id->ty;
	if(id->store != Dtype && id->store != Dtag){
		if(id->store == Dundef){
			id->store = Dwundef;
			error(t->src.start, "%s is not declared", id->sym->name);
		}else if(id->store == Dimport){
			id->store = Dwundef;
			error(t->src.start, "%s's type cannot be determined", id->sym->name);
		}else if(id->store != Dwundef)
			error(t->src.start, "%s is not a type", id->sym->name);
		return terror;
	}
	if(tt == nil){
		error(t->src.start, "%t not fully defined", t);
		return terror;
	}
	return tt;
}

/*
 * resolve a -> qualified type
 */
Type*
arrowtype(Type *t, Decl *adtt)
{
	Type *tt;
	Decl *id;

	id = t->decl;
	if(id->ty != nil){
		if(id->store == Dunbound)
			fatal("arrowtype: unbound decl has a type");
		return id->ty;
	}

	/*
	 * special hack to allow module variables to derive other types
	 */ 
	tt = t->tof;
	if(tt->kind == Tid){
		id = tt->decl;
		if(id->store == Dunbound)
			fatal("arrowtype: Tid's decl unbound");
		if(id->store == Dimport){
			id->store = Dwundef;
			error(t->src.start, "%s's type cannot be determined", id->sym->name);
			return terror;
		}

		/*
		 * forward references to module variables can't be resolved
		 */
		if(id->store != Dtype && !(id->ty->ok & OKbind)){
			error(t->src.start, "%s's type cannot be determined", id->sym->name);
			return terror;
		}

		if(id->store == Dwundef)
			return terror;
		tt = id->ty = verifytypes(id->ty, adtt);
		if(tt == nil){
			error(t->tof->src.start, "%T is not a module", t->tof);
			return terror;
		}
	}else
		tt = verifytypes(t->tof, adtt);
	t->tof = tt;
	if(tt == terror)
		return terror;
	if(tt->kind != Tmodule){
		error(t->src.start, "%T is not a module", tt);
		return terror;
	}
	id = namedot(tt->ids, t->decl->sym);
	if(id == nil){
		error(t->src.start, "%s is not a member of %T", t->decl->sym->name, tt);
		return terror;
	}
	if(id->store == Dtype && id->ty != nil){
		t->decl = id;
		return id->ty;
	}
	error(t->src.start, "%T is not a type", t);
	return terror;
}

/*
 * resolve a . qualified type
 */
Type*
dottype(Type *t, Decl *adtt)
{
	Type *tt;
	Decl *id;

	if(t->decl->ty != nil){
		if(t->decl->store == Dunbound)
			fatal("dottype: unbound decl has a type");
		return t->decl->ty;
	}
	t->tof = tt = verifytypes(t->tof, adtt);
	if(tt == terror)
		return terror;
	if(tt->kind != Tadt){
		error(t->src.start, "%T is not an adt", tt);
		return terror;
	}
	id = namedot(tt->tags, t->decl->sym);
	if(id != nil && id->ty != nil){
		t->decl = id;
		return id->ty;
	}
	error(t->src.start, "%s is not a pick tag of %T", t->decl->sym->name, tt);
	return terror;
}

/*
 * walk a type, putting all adts, modules, and tuples into equivalence classes
 */
void
teqclass(Type *t)
{
	Decl *id, *tg;
	Teq *teq;

	if(t == nil || (t->ok & OKclass) == OKclass)
		return;
	t->ok |= OKclass;
	switch(t->kind){
	case Terror:
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tnone:
	case Tany:
	case Tiface:
	case Tainit:
	case Talt:
	case Tcase:
	case Tcasec:
	case Tgoto:
		return;
	case Tref:
		teqclass(t->tof);
		return;
	case Tchan:
	case Tarray:
	case Tlist:
		teqclass(t->tof);
		if(!debug['Z'])
			return;
		break;
	case Tadt:
	case Tadtpick:
	case Ttuple:
		for(id = t->ids; id != nil; id = id->next)
			teqclass(id->ty);
		for(tg = t->tags; tg != nil; tg = tg->next)
			teqclass(tg->ty);
		break;
	case Tmodule:
		t->tof = mkiface(t->decl);
		for(id = t->ids; id != nil; id = id->next)
			teqclass(id->ty);
		break;
	case Tfn:
		for(id = t->ids; id != nil; id = id->next)
			teqclass(id->ty);
		teqclass(t->tof);
		return;
	default:
		fatal("teqclass: unknown type kind %d", t->kind);
		return;
	}

	/*
	 * find an equivalent type
	 * stupid linear lookup could be made faster
	 */
	if((t->ok & OKsized) != OKsized)
		fatal("eqclass type not sized: %t", t);

	for(teq = eqclass[t->kind]; teq != nil; teq = teq->eq){
		if(t->size == teq->ty->size && tequal(t, teq->ty)){
			t->eq = teq;
			if(t->kind == Tmodule)
				joiniface(t, t->eq->ty->tof);
			return;
		}
	}

	/*
	 * if no equiv type, make one
	 */
	t->eq = allocmem(sizeof(Teq));
	t->eq->id = 0;
	t->eq->ty = t;
	t->eq->eq = eqclass[t->kind];
	eqclass[t->kind] = t->eq;
}

/*
 * record that we've used the type
 * using a type uses all types reachable from that type
 */
void
reftype(Type *t)
{
	Decl *id, *tg;

	if(t == nil || (t->ok & OKref) == OKref)
		return;
	t->ok |= OKref;
	if(t->decl != nil && t->decl->refs == 0)
		t->decl->refs++;
	switch(t->kind){
	case Terror:
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tnone:
	case Tany:
	case Tiface:
	case Tainit:
	case Talt:
	case Tcase:
	case Tcasec:
	case Tgoto:
		break;
	case Tref:
	case Tchan:
	case Tarray:
	case Tlist:
		if(t->decl != nil){
			if(nadts >= lenadts){
				lenadts = nadts + 32;
				adts = reallocmem(adts, lenadts * sizeof *adts);
			}
			adts[nadts++] = t->decl;
		}
		reftype(t->tof);
		break;
	case Tadt:
	case Tadtpick:
	case Ttuple:
		if(t->kind == Tadt || t->kind == Ttuple && t->decl->sym != anontupsym){
			if(nadts >= lenadts){
				lenadts = nadts + 32;
				adts = reallocmem(adts, lenadts * sizeof *adts);
			}
			adts[nadts++] = t->decl;
		}
		for(id = t->ids; id != nil; id = id->next)
			if(id->store != Dfn)
				reftype(id->ty);
		for(tg = t->tags; tg != nil; tg = tg->next)
			reftype(tg->ty);
		break;
	case Tmodule:
		/*
		 * a module's elements should get used individually
		 */
		break;
	case Tfn:
		for(id = t->ids; id != nil; id = id->next)
			reftype(id->ty);
		reftype(t->tof);
		break;
	default:
		fatal("reftype: unknown type kind %d", t->kind);
		break;
	}
}

/*
 * check all reachable types for cycles and illegal forward references
 * find the size of all the types
 */
void
cycsizetype(Type *t)
{
	Decl *id, *tg;

	if(t == nil || (t->ok & (OKcycsize|OKcyc|OKsized)) == (OKcycsize|OKcyc|OKsized))
		return;
	t->ok |= OKcycsize;
	switch(t->kind){
	case Terror:
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tnone:
	case Tany:
	case Tiface:
	case Tainit:
	case Talt:
	case Tcase:
	case Tcasec:
	case Tgoto:
		t->ok |= OKcyc;
		sizetype(t);
		break;
	case Tref:
	case Tchan:
	case Tarray:
	case Tlist:
		cyctype(t);
		sizetype(t);
		cycsizetype(t->tof);
		break;
	case Tadt:
	case Ttuple:
		cyctype(t);
		sizetype(t);
		for(id = t->ids; id != nil; id = id->next)
			cycsizetype(id->ty);
		for(tg = t->tags; tg != nil; tg = tg->next){
			if((tg->ty->ok & (OKcycsize|OKcyc|OKsized)) == (OKcycsize|OKcyc|OKsized))
				continue;
			tg->ty->ok |= (OKcycsize|OKcyc|OKsized);
			for(id = tg->ty->ids; id != nil; id = id->next)
				cycsizetype(id->ty);
		}
		break;
	case Tadtpick:
		t->ok &= ~OKcycsize;
		cycsizetype(t->decl->dot->ty);
		break;
	case Tmodule:
		cyctype(t);
		sizetype(t);
		for(id = t->ids; id != nil; id = id->next)
			cycsizetype(id->ty);
		sizeids(t->ids, 0);
		break;
	case Tfn:
		cyctype(t);
		sizetype(t);
		for(id = t->ids; id != nil; id = id->next)
			cycsizetype(id->ty);
		cycsizetype(t->tof);
		sizeids(t->ids, MaxTemp);
		break;
	default:
		fatal("cycsizetype: unknown type kind %d", t->kind);
		break;
	}
}

/*
 * marks for checking for arcs
 */
enum
{
	ArcValue	= 1 << 0,
	ArcList		= 1 << 1,
	ArcArray	= 1 << 2,
	ArcRef		= 1 << 3,
	ArcCyc		= 1 << 4		/* cycle found */
};

void
cyctype(Type *t)
{
	Decl *id, *tg;

	if((t->ok & OKcyc) == OKcyc)
		return;
	t->ok |= OKcyc;
	t->rec |= TRcyc;
	switch(t->kind){
	case Terror:
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tnone:
	case Tany:
	case Tfn:
	case Tchan:
	case Tarray:
	case Tref:
	case Tlist:
		break;
	case Tadt:
	case Tmodule:
	case Ttuple:
		for(id = t->ids; id != nil; id = id->next)
			cycfield(t, id);
		for(tg = t->tags; tg != nil; tg = tg->next){
			if((tg->ty->ok & OKcyc) == OKcyc)
				continue;
			tg->ty->ok |= OKcyc;
			for(id = tg->ty->ids; id != nil; id = id->next)
				cycfield(t, id);
		}
		break;
	default:
		fatal("checktype: unknown type kind %d", t->kind);
		break;
	}
	t->rec &= ~TRcyc;
}

void
cycfield(Type *base, Decl *id)
{
	int arc;

	if(!storespace[id->store])
		return;
	arc = cycarc(base, id->ty);

	if((arc & (ArcCyc|ArcValue)) == (ArcCyc|ArcValue)){
		if(id->cycerr == 0)
			error(base->src.start, "illegal type cycle without a reference in field %s of %t",
				id->sym->name, base);
		id->cycerr = 1;
	}else if(arc & ArcCyc){
		if((arc & ArcArray) && id->cyc == 0){
			if(id->cycerr == 0)
				error(base->src.start, "illegal circular reference to type %T in field %s of %t",
					id->ty, id->sym->name, base);
			id->cycerr = 1;
		}
		id->cycle = 1;
	}else if(id->cyc != 0){
		if(id->cycerr == 0)
			error(id->src.start, "spurious cyclic qualifier for field %s of %t", id->sym->name, base);
		id->cycerr = 1;
	}
}

int
cycarc(Type *base, Type *t)
{
	Decl *id, *tg;
	int me, arc;

	if(t == nil)
		return 0;
	if(t->rec & TRcyc){
		if(tequal(t, base)){
			if(t->kind == Tmodule)
				return ArcCyc | ArcRef;
			else
				return ArcCyc | ArcValue;
		}
		return 0;
	}
	t->rec |= TRcyc;
	me = 0;
	switch(t->kind){
	case Terror:
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tnone:
	case Tany:
	case Tchan:
	case Tfn:
		break;
	case Tarray:
		me = cycarc(base, t->tof) & ~ArcValue | ArcArray;
		break;
	case Tref:
		me = cycarc(base, t->tof) & ~ArcValue | ArcRef;
		break;
	case Tlist:
		me = cycarc(base, t->tof) & ~ArcValue | ArcList;
		break;
	case Tadt:
	case Tadtpick:
	case Tmodule:
	case Ttuple:
		me = 0;
		for(id = t->ids; id != nil; id = id->next){
			if(!storespace[id->store])
				continue;
			arc = cycarc(base, id->ty);
			if((arc & ArcCyc) && id->cycerr == 0)
				me |= arc;
		}
		for(tg = t->tags; tg != nil; tg = tg->next){
			arc = cycarc(base, tg->ty);
			if((arc & ArcCyc) && tg->cycerr == 0)
				me |= arc;
		}

		if(t->kind == Tmodule)
			me = me & ArcCyc | ArcRef;
		else
			me &= ArcCyc | ArcValue;
		break;
	default:
		fatal("cycarc: unknown type kind %d", t->kind);
		break;
	}
	t->rec &= ~TRcyc;
	return me;
}

/*
 * set the sizes and field offsets for t
 * look only as deeply as needed to size this type.
 * cycsize type will clean up the rest.
 */
void
sizetype(Type *t)
{
	Decl *id, *tg;
	Szal szal;
	long sz, al, a;

	if(t == nil)
		return;
	if((t->ok & OKsized) == OKsized)
		return;
	t->ok |= OKsized;
if((t->ok & (OKverify|OKsized)) != (OKverify|OKsized))
fatal("sizetype bogus ok for %t", t);
	switch(t->kind){
	default:
		fatal("sizetype: unknown type kind %d", t->kind);
		break;
	case Terror:
	case Tnone:
	case Tbyte:
	case Tint:
	case Tbig:
	case Tstring:
	case Tany:
	case Treal:
		fatal("%T should have a size", t);
		break;
	case Tref:
	case Tchan:
	case Tarray:
	case Tlist:
	case Tmodule:
		t->size = t->align = IBY2WD;
		break;
	case Ttuple:
	case Tadt:
		if(t->tags == nil){
			if(!debug['z']){
				szal = sizeids(t->ids, 0);
				t->size = align(szal.size, szal.align);
				t->align = szal.align;
			}else{
				szal = sizeids(t->ids, 0);
				t->align = IBY2LG;
				t->size = align(szal.size, IBY2LG);
			}
			return;
		}
		if(!debug['z']){
			szal = sizeids(t->ids, IBY2WD);
			sz = szal.size;
			al = szal.align;
			if(al < IBY2WD)
				al = IBY2WD;
		}else{
			szal = sizeids(t->ids, IBY2WD);
			sz = szal.size;
		}
		for(tg = t->tags; tg != nil; tg = tg->next){
			if((tg->ty->ok & OKsized) == OKsized)
				continue;
			tg->ty->ok |= OKsized;
			if(!debug['z']){
				szal = sizeids(tg->ty->ids, sz);
				a = szal.align;
				if(a < al)
					a = al;
				tg->ty->size = align(szal.size, a);
				tg->ty->align = a;
			}else{
				szal = sizeids(tg->ty->ids, sz);
				tg->ty->size = align(szal.size, IBY2LG);
				tg->ty->align = IBY2LG;
			}			
		}
		break;
	case Tfn:
		t->size = 0;
		t->align = 1;
		break;
	case Tainit:
		t->size = 0;
		t->align = 1;
		break;
	case Talt:
		t->size = t->cse->nlab * 2*IBY2WD + 2*IBY2WD;
		t->align = IBY2WD;
		break;
	case Tcase:
	case Tcasec:
		t->size = t->cse->nlab * 3*IBY2WD + 2*IBY2WD;
		t->align = IBY2WD;
		break;
	case Tgoto:
		t->size = t->cse->nlab * IBY2WD + IBY2WD;
		if(t->cse->iwild != nil)
			t->size += IBY2WD;
		t->align = IBY2WD;
		break;
	case Tiface:
		sz = IBY2WD;
		for(id = t->ids; id != nil; id = id->next){
			sz = align(sz, IBY2WD) + IBY2WD;
			sz += id->sym->len + 1;
			if(id->dot->ty->kind == Tadt)
				sz += id->dot->sym->len + 1;
		}
		t->size = sz;
		t->align = IBY2WD;
		break;
	}
}

Szal
sizeids(Decl *id, long off)
{
	Szal szal;
	int a, al;

	al = 1;
	for(; id != nil; id = id->next){
		if(storespace[id->store]){
			sizetype(id->ty);
			/*
			 * alignment can be 0 if we have
			 * illegal forward declarations.
			 * just patch a; other code will flag an error
			 */
			a = id->ty->align;
			if(a == 0)
				a = 1;

			if(a > al)
				al = a;

			off = align(off, a);
			id->offset = off;
			off += id->ty->size;
		}
	}
	szal.size = off;
	szal.align = al;
	return szal;
}

long
align(long off, int align)
{
	if(align == 0)
		fatal("align 0");
	while(off % align)
		off++;
	return off;
}

/*
 * recalculate a type's size
 */
void
resizetype(Type *t)
{
	if((t->ok & OKsized) == OKsized){
		t->ok &= ~OKsized;
		cycsizetype(t);
	}
}

/*
 * check if a module is accessable from t
 * if so, mark that module interface
 */
void
modrefable(Type *t)
{
	Decl *id, *m, *tg;

	if(t == nil || (t->ok & OKmodref) == OKmodref)
		return;
	if((t->ok & OKverify) != OKverify)
		fatal("modrefable unused type %t", t);
	t->ok |= OKmodref;
	switch(t->kind){
	case Terror:
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tnone:
	case Tany:
		break;
	case Tchan:
	case Tref:
	case Tarray:
	case Tlist:
		modrefable(t->tof);
		break;
	case Tmodule:
		t->tof->linkall = 1;
		t->decl->refs++;
		for(id = t->ids; id != nil; id = id->next){
			switch(id->store){
			case Dglobal:
			case Dfn:
				modrefable(id->ty);
				break;
			case Dtype:
				if(id->ty->kind != Tadt)
					break;
				for(m = id->ty->ids; m != nil; m = m->next)
					if(m->store == Dfn)
						modrefable(m->ty);
				break;
			}
		}
		break;
	case Tfn:
	case Tadt:
	case Ttuple:
		for(id = t->ids; id != nil; id = id->next)
			if(id->store != Dfn)
				modrefable(id->ty);
		for(tg = t->tags; tg != nil; tg = tg->next){
			if((tg->ty->ok & OKmodref) == OKmodref)
				continue;
			tg->ty->ok |= OKmodref;
			for(id = tg->ty->ids; id != nil; id = id->next)
				modrefable(id->ty);
		}
		modrefable(t->tof);
		break;
	case Tadtpick:
		modrefable(t->decl->dot->ty);
		break;
	default:
		fatal("unknown type kind %d", t->kind);
		break;
	}
}

Desc*
gendesc(Decl *d, long size, Decl *decls)
{
	if(debug['D'])
		print("generate desc for %D\n", d);
	return usedesc(mkdesc(size, decls));
}

Desc*
mkdesc(long size, Decl *d)
{
	uchar *pmap;
	long len, n;

	len = (size+8*IBY2WD-1) / (8*IBY2WD);
	pmap = allocmem(len);
	memset(pmap, 0, len);
	n = descmap(d, pmap, 0);
	if(n >= 0)
		n = n / (8*IBY2WD) + 1;
	else
		n = 0;
	if(n > len)
		fatal("wrote off end of decl map: %ld %ld", n, len);
	return enterdesc(pmap, size, n);
}

Desc*
mktdesc(Type *t)
{
	Desc *d;
	uchar *pmap;
	long len, n;

usedty(t);
	if(debug['D'])
		print("generate desc for %T\n", t);
	if(t->decl == nil){
		t->decl = mkdecl(&t->src, Dtype, t);
		t->decl->sym = enter("_mktdesc_", 0);
	}
	if(t->decl->desc != nil)
		return t->decl->desc;
	len = (t->size+8*IBY2WD-1) / (8*IBY2WD);
	pmap = allocmem(len);
	memset(pmap, 0, len);
	n = tdescmap(t, pmap, 0);
	if(n >= 0)
		n = n / (8*IBY2WD) + 1;
	else
		n = 0;
	if(n > len)
		fatal("wrote off end of type map for %T: %ld %ld 0x%2.2ux", t, n, len, t->ok);
	d = enterdesc(pmap, t->size, n);
	t->decl->desc = d;
	return d;
}

Desc*
enterdesc(uchar *map, long size, long nmap)
{
	Desc *d, *last;
	int c;

	last = nil;
	for(d = descriptors; d != nil; d = d->next){
		if(d->size > size || d->size == size && d->nmap > nmap)
			break;
		if(d->size == size && d->nmap == nmap){
			c = memcmp(d->map, map, nmap);
			if(c == 0){
				free(map);
				return d;
			}
			if(c > 0)
				break;
		}
		last = d;
	}
	d = allocmem(sizeof *d);
	d->id = -1;
	d->used = 0;
	d->map = map;
	d->size = size;
	d->nmap = nmap;
	if(last == nil){
		d->next = descriptors;
		descriptors = d;
	}else{
		d->next = last->next;
		last->next = d;
	}
	return d;
}

Desc*
usedesc(Desc *d)
{
	d->used = 1;
	return d;
}

/*
 * create the pointer description byte map for every type in decls
 * each bit corresponds to a word, and is 1 if occupied by a pointer
 * the high bit in the byte maps the first word
 */
long
descmap(Decl *decls, uchar *map, long start)
{
	Decl *d;
	long last, m;

	if(debug['D'])
		print("descmap offset %ld\n", start);
	last = -1;
	for(d = decls; d != nil; d = d->next){
		if(d->store == Dtype && d->ty->kind == Tmodule
		|| d->store == Dfn
		|| d->store == Dconst)
			continue;
		m = tdescmap(d->ty, map, d->offset + start);
		if(debug['D']){
			if(d->sym != nil)
				print("descmap %s type %T offset %ld returns %ld\n",
					d->sym->name, d->ty, d->offset+start, m);
			else
				print("descmap type %T offset %ld returns %ld\n", d->ty, d->offset+start, m);
		}
		if(m >= 0)
			last = m;
	}
	return last;
}

long
tdescmap(Type *t, uchar *map, long offset)
{
	Label *lab;
	long i, e, m;
	int bit;

	if(t == nil)
		return -1;

	m = -1;
	if(t->kind == Talt){
		lab = t->cse->labs;
		e = t->cse->nlab;
		offset += IBY2WD * 2;
		for(i = 0; i < e; i++){
			if(lab[i].isptr){
				bit = offset / IBY2WD % 8;
				map[offset / (8*IBY2WD)] |= 1 << (7 - bit);
				m = offset;
			}
			offset += 2*IBY2WD;
		}
		return m;
	}
	if(t->kind == Tcasec){
		e = t->cse->nlab;
		offset += IBY2WD;
		for(i = 0; i < e; i++){
			bit = offset / IBY2WD % 8;
			map[offset / (8*IBY2WD)] |= 1 << (7 - bit);
			offset += IBY2WD;
			bit = offset / IBY2WD % 8;
			map[offset / (8*IBY2WD)] |= 1 << (7 - bit);
			m = offset;
			offset += 2*IBY2WD;
		}
		return m;
	}

	if(tattr[t->kind].isptr){
		bit = offset / IBY2WD % 8;
		map[offset / (8*IBY2WD)] |= 1 << (7 - bit);
		return offset;
	}
	if(t->kind == Tadtpick)
		t = t->tof;
	if(t->kind == Ttuple || t->kind == Tadt){
		if(debug['D'])
			print("descmap adt offset %d\n", offset);
		if(t->rec != 0)
			fatal("illegal cyclic type %t in tdescmap", t);
		t->rec = 1;
		offset = descmap(t->ids, map, offset);
		t->rec = 0;
		return offset;
	}

	return -1;
}

/*
 * can a t2 be assigned to a t1?
 * any means Tany matches all types,
 * not just references
 */
int
tcompat(Type *t1, Type *t2, int any)
{
	int ok, v;

	if(t1 == t2)
		return 1;
	tcomset = 0;
	ok = rtcompat(t1, t2, any);
	v = cleartcomrec(t1) + cleartcomrec(t2);
	if(v != tcomset)
		fatal("recid t1 %t and t2 %t not balanced in tcompat: %d v %d", t1, t2, v, tcomset);
	return ok;
}

static int
rtcompat(Type *t1, Type *t2, int any)
{
	if(t1 == t2)
		return 1;
	if(t1 == nil || t2 == nil)
		return 0;
	if(t1->kind == Terror || t2->kind == Terror)
		return 1;

	t1->rec |= TRcom;
	t2->rec |= TRcom;
	switch(t1->kind){
	default:
		fatal("unknown type %t v %t in rtcompat", t1, t2);
	case Tstring:
		return t2->kind == Tstring || t2->kind == Tany;
	case Tnone:
	case Tint:
	case Tbig:
	case Tbyte:
	case Treal:
		return t1->kind == t2->kind;
	case Tany:
		if(tattr[t2->kind].isptr)
			return 1;
		return any;
	case Tref:
	case Tlist:
	case Tarray:
	case Tchan:
		if(t1->kind != t2->kind){
			if(t2->kind == Tany)
				return 1;
			return 0;
		}
		if(t1->kind != Tref && assumetcom(t1, t2))
			return 1;
		return rtcompat(t1->tof, t2->tof, 0);
	case Tfn:
		break;
	case Ttuple:
		if(t2->kind == Tadt && t2->tags == nil
		|| t2->kind == Ttuple){
			if(assumetcom(t1, t2))
				return 1;
			return idcompat(t1->ids, t2->ids, any);
		}
		if(t2->kind == Tadtpick){
			t2->tof->rec |= TRcom;
			if(assumetcom(t1, t2->tof))
				return 1;
			return idcompat(t1->ids, t2->tof->ids->next, any);
		}
		return 0;
	case Tadt:
		if(t2->kind == Ttuple && t1->tags == nil){
			if(assumetcom(t1, t2))
				return 1;
			return idcompat(t1->ids, t2->ids, any);
		}
		if(t1->tags != nil && t2->kind == Tadtpick)
			t2 = t2->decl->dot->ty;
		break;
	case Tadtpick:
/*
		if(t2->kind == Ttuple)
			return idcompat(t1->tof->ids->next, t2->ids, any);
*/
		break;
	case Tmodule:
		if(t2->kind == Tany)
			return 1;
		break;
	}
	return tequal(t1, t2);
}

/*
 * add the assumption that t1 and t2 are compatable
 */
static int
assumetcom(Type *t1, Type *t2)
{
	Type *r1, *r2;

	if(t1->tcom == nil && t2->tcom == nil){
		tcomset += 2;
		t1->tcom = t2->tcom = t1;
	}else{
		if(t1->tcom == nil){
			r1 = t1;
			t1 = t2;
			t2 = r1;
		}
		for(r1 = t1->tcom; r1 != r1->tcom; r1 = r1->tcom)
			;
		for(r2 = t2->tcom; r2 != nil && r2 != r2->tcom; r2 = r2->tcom)
			;
		if(r1 == r2)
			return 1;
		if(r2 == nil)
			tcomset++;
		t2->tcom = t1;
		for(; t2 != r1; t2 = r2){
			r2 = t2->tcom;
			t2->tcom = r1;
		}
	}
	return 0;
}

static int
cleartcomrec(Type *t)
{
	Decl *id;
	int n;

	n = 0;
	for(; t != nil && (t->rec & TRcom) == TRcom; t = t->tof){
		t->rec &= ~TRcom;
		if(t->tcom != nil){
			t->tcom = nil;
			n++;
		}
		if(t->kind == Tadtpick)
			n += cleartcomrec(t->tof);
		if(t->kind == Tmodule)
			t = t->tof;
		for(id = t->ids; id != nil; id = id->next)
			n += cleartcomrec(id->ty);
		for(id = t->tags; id != nil; id = id->next)
			n += cleartcomrec(id->ty);
	}
	return n;
}

/*
 * id1 and id2 are the fields in an adt or tuple
 * simple structural check; ignore names
 */
static int
idcompat(Decl *id1, Decl *id2, int any)
{
	for(; id1 != nil; id1 = id1->next){
		if(id1->store != Dfield)
			continue;
		while(id2 != nil && id2->store != Dfield)
			id2 = id2->next;
		if(id2 == nil
		|| id1->store != id2->store
		|| !rtcompat(id1->ty, id2->ty, any))
			return 0;
		id2 = id2->next;
	}
	while(id2 != nil && id2->store != Dfield)
		id2 = id2->next;
	return id2 == nil;
}

int
tequal(Type *t1, Type *t2)
{
	int ok, v;

	eqrec = 0;
	eqset = 0;
	ok = rtequal(t1, t2);
	v = cleareqrec(t1) + cleareqrec(t2);
	if(v != eqset)
		fatal("recid t1 %t and t2 %t not balanced in tequal: %d %d", t1, t2, v, eqset);
	return ok;
}

/*
 * structural equality on types
 */
static int
rtequal(Type *t1, Type *t2)
{
	/*
	 * this is just a shortcut
	 */
	if(t1 == t2)
		return 1;

	if(t1 == nil || t2 == nil)
		return 0;
	if(t1->kind == Terror || t2->kind == Terror)
		return 1;

	if(t1->kind != t2->kind)
		return 0;

	if(t1->eq != nil && t2->eq != nil)
		return t1->eq == t2->eq;

	t1->rec |= TReq;
	t2->rec |= TReq;
	switch(t1->kind){
	default:
		fatal("unknown type %t v %t in rtequal", t1, t2);
	case Tnone:
	case Tbig:
	case Tbyte:
	case Treal:
	case Tint:
	case Tstring:
		/*
		 * this should always be caught by t1 == t2 check
		 */
		fatal("bogus value type %t vs %t in rtequal", t1, t2);
		return 1;
	case Tref:
	case Tlist:
	case Tarray:
	case Tchan:
		if(t1->kind != Tref && assumeteq(t1, t2))
			return 1;
		return rtequal(t1->tof, t2->tof);
	case Tfn:
		if(t1->varargs != t2->varargs)
			return 0;
		if(!idequal(t1->ids, t2->ids, 0, storespace))
			return 0;
		return rtequal(t1->tof, t2->tof);
	case Ttuple:
		if(assumeteq(t1, t2))
			return 1;
		return idequal(t1->ids, t2->ids, 0, storespace);
	case Tadt:
	case Tadtpick:
	case Tmodule:
		if(assumeteq(t1, t2))
			return 1;

		/*
		 * compare interfaces when comparing modules
		 */
		if(t1->kind == Tmodule)
			return idequal(t1->tof->ids, t2->tof->ids, 1, nil);

		/*
		 * picked adts; check parent,
		 * assuming equiv picked fields,
		 * then check picked fields are equiv
		 */
		if(t1->kind == Tadtpick && !rtequal(t1->decl->dot->ty, t2->decl->dot->ty))
			return 0;

		/*
		 * adts with pick tags: check picked fields for equality
		 */
		if(!idequal(t1->tags, t2->tags, 1, nil))
			return 0;

		return idequal(t1->ids, t2->ids, 1, storespace);
	}
}

static int
assumeteq(Type *t1, Type *t2)
{
	Type *r1, *r2;

	if(t1->teq == nil && t2->teq == nil){
		eqrec++;
		eqset += 2;
		t1->teq = t2->teq = t1;
	}else{
		if(t1->teq == nil){
			r1 = t1;
			t1 = t2;
			t2 = r1;
		}
		for(r1 = t1->teq; r1 != r1->teq; r1 = r1->teq)
			;
		for(r2 = t2->teq; r2 != nil && r2 != r2->teq; r2 = r2->teq)
			;
		if(r1 == r2)
			return 1;
		if(r2 == nil)
			eqset++;
		t2->teq = t1;
		for(; t2 != r1; t2 = r2){
			r2 = t2->teq;
			t2->teq = r1;
		}
	}
	return 0;
}

/*
 * checking structural equality for adts, tuples, and fns
 */
static int
idequal(Decl *id1, Decl *id2, int usenames, int *storeok)
{
	/*
	 * this is just a shortcut
	 */
	if(id1 == id2)
		return 1;

	for(; id1 != nil; id1 = id1->next){
		if(storeok != nil && !storeok[id1->store])
			continue;
		while(id2 != nil && storeok != nil && !storeok[id2->store])
			id2 = id2->next;
		if(id2 == nil
		|| usenames && id1->sym != id2->sym
		|| id1->store != id2->store
		|| id1->implicit != id2->implicit
		|| id1->cyc != id2->cyc
		|| !rtequal(id1->ty, id2->ty))
			return 0;
		id2 = id2->next;
	}
	while(id2 != nil && storeok != nil && !storeok[id2->store])
		id2 = id2->next;
	return id1 == nil && id2 == nil;
}

static int
cleareqrec(Type *t)
{
	Decl *id;
	int n;

	n = 0;
	for(; t != nil && (t->rec & TReq) == TReq; t = t->tof){
		t->rec &= ~TReq;
		if(t->teq != nil){
			t->teq = nil;
			n++;
		}
		if(t->kind == Tadtpick)
			n += cleareqrec(t->decl->dot->ty);
		if(t->kind == Tmodule)
			t = t->tof;
		for(id = t->ids; id != nil; id = id->next)
			n += cleareqrec(id->ty);
		for(id = t->tags; id != nil; id = id->next)
			n += cleareqrec(id->ty);
	}
	return n;
}

/*
 * create type signatures
 * sign the same information used
 * for testing type equality
 */
ulong
sign(Decl *d)
{
	Type *t;
	uchar *sig, md5sig[MD5dlen];
	char buf[StrSize];
	int i, sigend, sigalloc, v;

	t = d->ty;
	if(t->sig != 0)
		return t->sig;

	sig = 0;
	sigend = -1;
	sigalloc = 1024;
	while(sigend < 0 || sigend >= sigalloc){
		sigalloc *= 2;
		sig = reallocmem(sig, sigalloc);
		eqrec = 0;
		sigend = rtsign(t, sig, sigalloc, 0);
		v = clearrec(t);
		if(v != eqrec)
			fatal("recid not balanced in sign: %d %d", v, eqrec);
		eqrec = 0;
	}
	sig[sigend] = '\0';

	if(signdump != nil){
		seprint(buf, buf+sizeof(buf), "%D", d);
		if(strcmp(buf, signdump) == 0){
			print("sign %D len %d\n", d, sigend);
			print("%s\n", (char*)sig);
		}
	}

	md5(sig, sigend, md5sig, nil);
	for(i = 0; i < MD5dlen; i += 4)
		t->sig ^= md5sig[i+0] | (md5sig[i+1]<<8) | (md5sig[i+2]<<16) | (md5sig[i+3]<<24);
	if(debug['S'])
		print("signed %D type %T len %d sig %#lux\n", d, t, sigend, t->sig);
	free(sig);
	return t->sig;
}

enum
{
	SIGSELF =	'S',
	SIGVARARGS =	'*',
	SIGCYC =	'y',
	SIGREC =	'@'
};

static int sigkind[Tend] =
{
	/* Tnone */	'n',
	/* Tadt */	'a',
	/* Tadtpick */	'p',
	/* Tarray */	'A',
	/* Tbig */	'B',
	/* Tbyte */	'b',
	/* Tchan */	'C',
	/* Treal */	'r',
	/* Tfn */	'f',
	/* Tint */	'i',
	/* Tlist */	'L',
	/* Tmodule */	'm',
	/* Tref */	'R',
	/* Tstring */	's',
	/* Ttuple */	't',
};

static int
rtsign(Type *t, uchar *sig, int lensig, int spos)
{
	Decl *id, *tg;
	char name[32];
	int kind, lenname;

	if(t == nil)
		return spos;

	if(spos < 0 || spos + 8 >= lensig)
		return -1;

	if(t->eq != nil && t->eq->id){
		if(t->eq->id < 0 || t->eq->id > eqrec)
			fatal("sign rec %T %d %d", t, t->eq->id, eqrec);

		sig[spos++] = SIGREC;
		seprint(name, name+sizeof(name), "%d", t->eq->id);
		lenname = strlen(name);
		if(spos + lenname > lensig)
			return -1;
		strcpy((char*)&sig[spos], name);
		spos += lenname;
		return spos;
	}
	if(t->eq != nil){
		eqrec++;
		t->eq->id = eqrec;
	}

	kind = sigkind[t->kind];
	sig[spos++] = kind;
	if(kind == 0)
		fatal("no sigkind for %t", t);

	t->rec = 1;
	switch(t->kind){
	default:
		fatal("bogus type %t in rtsign", t);
		return -1;
	case Tnone:
	case Tbig:
	case Tbyte:
	case Treal:
	case Tint:
	case Tstring:
		return spos;
	case Tref:
	case Tlist:
	case Tarray:
	case Tchan:
		return rtsign(t->tof, sig, lensig, spos);
	case Tfn:
		if(t->varargs != 0)
			sig[spos++] = SIGVARARGS;
		spos = idsign(t->ids, 0, sig, lensig, spos);
		return rtsign(t->tof, sig, lensig, spos);
	case Ttuple:
		return idsign(t->ids, 0, sig, lensig, spos);
	case Tadt:
		/*
		 * this is a little different than in rtequal,
		 * since we flatten the adt we used to represent the globals
		 */
		if(t->eq == nil){
			if(strcmp(t->decl->sym->name, ".mp") != 0)
				fatal("no t->eq field for %t", t);
			spos--;
			for(id = t->ids; id != nil; id = id->next){
				spos = idsign1(id, 1, sig, lensig, spos);
				if(spos < 0 || spos >= lensig)
					return -1;
				sig[spos++] = ';';
			}
			return spos;
		}
		spos = idsign(t->ids, 1, sig, lensig, spos);
		if(spos < 0 || t->tags == nil)
			return spos;

		/*
		 * convert closing ')' to a ',', then sign any tags
		 */
		sig[spos-1] = ',';
		for(tg = t->tags; tg != nil; tg = tg->next){
			lenname = tg->sym->len;
			if(spos + lenname + 2 >= lensig)
				return -1;
			strcpy((char*)&sig[spos], tg->sym->name);
			spos += lenname;
			sig[spos++] = '=';
			sig[spos++] = '>';

			spos = rtsign(tg->ty, sig, lensig, spos);
			if(spos < 0 || spos >= lensig)
				return -1;

			if(tg->next != nil)
				sig[spos++] = ',';
		}
		if(spos >= lensig)
			return -1;
		sig[spos++] = ')';
		return spos;
	case Tadtpick:
		spos = idsign(t->ids, 1, sig, lensig, spos);
		if(spos < 0)
			return spos;
		return rtsign(t->decl->dot->ty, sig, lensig, spos);
	case Tmodule:
		if(t->tof->linkall == 0)
			fatal("signing a narrowed module");

		if(spos >= lensig)
			return -1;
		sig[spos++] = '{';
		for(id = t->tof->ids; id != nil; id = id->next){
			if(strcmp(id->sym->name, ".mp") == 0){
				spos = rtsign(id->ty, sig, lensig, spos);
				if(spos < 0)
					return -1;
				continue;
			}
			spos = idsign1(id, 1, sig, lensig, spos);
			if(spos < 0 || spos >= lensig)
				return -1;
			sig[spos++] = ';';
		}
		if(spos >= lensig)
			return -1;
		sig[spos++] = '}';
		return spos;
	}
}

static int
idsign(Decl *id, int usenames, uchar *sig, int lensig, int spos)
{
	int first;

	if(spos >= lensig)
		return -1;
	sig[spos++] = '(';
	first = 1;
	for(; id != nil; id = id->next){
		if(id->store == Dlocal)
			fatal("local %s in idsign", id->sym->name);

		if(!storespace[id->store])
			continue;

		if(!first){
			if(spos >= lensig)
				return -1;
			sig[spos++] = ',';
		}

		spos = idsign1(id, usenames, sig, lensig, spos);
		if(spos < 0)
			return -1;
		first = 0;
	}
	if(spos >= lensig)
		return -1;
	sig[spos++] = ')';
	return spos;
}

static int
idsign1(Decl *id, int usenames, uchar *sig, int lensig, int spos)
{
	char *name;
	int lenname;

	if(usenames){
		name = id->sym->name;
		lenname = id->sym->len;
		if(spos + lenname + 1 >= lensig)
			return -1;
		strcpy((char*)&sig[spos], name);
		spos += lenname;
		sig[spos++] = ':';
	}

	if(spos + 2 >= lensig)
		return -1;

	if(id->implicit != 0)
		sig[spos++] = SIGSELF;

	if(id->cyc != 0)
		sig[spos++] = SIGCYC;

	return rtsign(id->ty, sig, lensig, spos);
}

static int
clearrec(Type *t)
{
	Decl *id;
	int n;

	n = 0;
	for(; t != nil && t->rec; t = t->tof){
		t->rec = 0;
		if(t->eq != nil && t->eq->id != 0){
			t->eq->id = 0;
			n++;
		}
		if(t->kind == Tmodule){
			for(id = t->tof->ids; id != nil; id = id->next)
				n += clearrec(id->ty);
			return n;
		}
		if(t->kind == Tadtpick)
			n += clearrec(t->decl->dot->ty);
		for(id = t->ids; id != nil; id = id->next)
			n += clearrec(id->ty);
		for(id = t->tags; id != nil; id = id->next)
			n += clearrec(id->ty);
	}
	return n;
}

int
typeconv(va_list *arg, Fconv *f)
{
	Type *t;
	char *p, buf[1024];

	t = va_arg(*arg, Type*);
	if(t == nil){
		p = "nothing";
	}else{
		p = buf;
		buf[0] = 0;
		tprint(buf, buf+sizeof(buf), t);
	}
	strconv(p, f);
	return 0;
}

int
stypeconv(va_list *arg, Fconv *f)
{
	Type *t;
	char *p, buf[1024];

	t = va_arg(*arg, Type*);
	if(t == nil){
		p = "nothing";
	}else{
		p = buf;
		buf[0] = 0;
		stprint(buf, buf+sizeof(buf), t);
	}
	strconv(p, f);
	return 0;
}

int
ctypeconv(va_list *arg, Fconv *f)
{
	Type *t;
	char buf[1024];

	t = va_arg(*arg, Type*);
	buf[0] = 0;
	ctprint(buf, buf+sizeof(buf), t);
	strconv(buf, f);
	return 0;
}

char*
tprint(char *buf, char *end, Type *t)
{
	Decl *id;

	if(t == nil)
		return buf;
	if(t->kind >= Tend)
		return seprint(buf, end, "kind %d", t->kind);
	t->pr = 1;
	switch(t->kind){
	case Tarrow:
		buf = seprint(buf, end, "%T->%s", t->tof, t->decl->sym->name);
		break;
	case Tdot:
		buf = seprint(buf, end, "%T.%s", t->tof, t->decl->sym->name);
		break;
	case Tid:
		buf = seprint(buf, end, "%s", t->decl->sym->name);
		break;
	case Tint:
	case Tbig:
	case Tstring:
	case Treal:
	case Tbyte:
	case Tany:
	case Tnone:
	case Terror:
	case Tainit:
	case Talt:
	case Tcase:
	case Tcasec:
	case Tgoto:
	case Tiface:
		buf = secpy(buf, end, kindname[t->kind]);
		break;
	case Tref:
		buf = secpy(buf, end, "ref ");
		buf = tprint(buf, end, t->tof);
		break;
	case Tchan:
	case Tarray:
	case Tlist:
		buf = seprint(buf, end, "%s of ", kindname[t->kind]);
		buf = tprint(buf, end, t->tof);
		break;
	case Tadtpick:
		buf = seprint(buf, end, "%s.%s", t->decl->dot->sym->name, t->decl->sym->name);
		break;
	case Tadt:
		if(t->decl->dot != nil && t->decl->dot->sym != impmod)
			buf = seprint(buf, end, "%s->%s", t->decl->dot->sym->name, t->decl->sym->name);
		else
			buf = seprint(buf, end, "%s", t->decl->sym->name);
		break;
	case Tmodule:
		buf = seprint(buf, end, "%s", t->decl->sym->name);
		break;
	case Ttuple:
		buf = secpy(buf, end, "(");
		for(id = t->ids; id != nil; id = id->next){
			buf = tprint(buf, end, id->ty);
			if(id->next != nil)
				buf = secpy(buf, end, ", ");
		}
		buf = secpy(buf, end, ")");
		break;
	case Tfn:
		buf = secpy(buf, end, "fn(");
		for(id = t->ids; id != nil; id = id->next){
			if(id->sym == nil)
				buf = secpy(buf, end, "nil: ");
			else
				buf = seprint(buf, end, "%s: ", id->sym->name);
			if(id->implicit)
				buf = secpy(buf, end, "self ");
			buf = tprint(buf, end, id->ty);
			if(id->next != nil)
				buf = secpy(buf, end, ", ");
		}
		if(t->varargs && t->ids != nil)
			buf = secpy(buf, end, ", *");
		else if(t->varargs)
			buf = secpy(buf, end, "*");
		if(t->tof != nil && t->tof->kind != Tnone){
			buf = secpy(buf, end, "): ");
			buf = tprint(buf, end, t->tof);
			break;
		}
		buf = secpy(buf, end, ")");
		break;
	default:
		yyerror("tprint: unknown type kind %d", t->kind);
		break;
	}
	t->pr = 0;
	return buf;
}

char*
stprint(char *buf, char *end, Type *t)
{
	if(t == nil)
		return buf;
	switch(t->kind){
	case Tid:
		return seprint(buf, end, "id %s", t->decl->sym->name);
	case Tadt:
	case Tadtpick:
	case Tmodule:
		buf = secpy(buf, end, kindname[t->kind]);
		buf = secpy(buf, end, " ");
		return tprint(buf, end, t);
	}
	return tprint(buf, end, t);
}
