#include "limbo.h"

char *storename[Dend]=
{
	/* Dtype */	"type",
	/* Dfn */	"function",
	/* Dglobal */	"global",
	/* Darg */	"argument",
	/* Dlocal */	"local",
	/* Dconst */	"con",
	/* Dfield */	"field",
	/* Dtag */	"pick tag",
	/* Dimport */	"import",
	/* Dunbound */	"unbound",
	/* Dundef */	"undefined",
	/* Dwundef */	"undefined",
};

char *storeart[Dend] =
{
	/* Dtype */	"a ",
	/* Dfn */	"a ",
	/* Dglobal */	"a ",
	/* Darg */	"an ",
	/* Dlocal */	"a ",
	/* Dconst */	"a ",
	/* Dfield */	"a ",
	/* Dtag */	"a",
	/* Dimport */	"an ",
	/* Dunbound */	"",
	/* Dundef */	"",
	/* Dwundef */	"",
};

int storespace[Dend] =
{
	/* Dtype */	0,
	/* Dfn */	0,
	/* Dglobal */	1,
	/* Darg */	1,
	/* Dlocal */	1,
	/* Dconst */	0,
	/* Dfield */	1,
	/* Dtag */	0,
	/* Dimport */	0,
	/* Dunbound */	0,
	/* Dundef */	0,
	/* Dwundef */	0,
};

static	Decl	*scopes[MaxScope];
static	Decl	*tails[MaxScope];
static	Decl	zdecl;

void
popscopes(void)
{
	Decl *d;

	/*
	 * clear out any decls left in syms
	 */
	while(scope >= ScopeBuiltin){
		for(d = scopes[scope--]; d != nil; d = d->next){
			if(d->sym != nil){
				d->sym->decl = d->old;
				d->old = nil;
			}
		}
	}

	if(impdecl != nil){
		for(d = impdecl->ty->ids; d != nil; d = d->next){
			d->sym->decl = nil;
			d->old = nil;
		}
	}
	impdecl = nil;

	scope = ScopeBuiltin;
	scopes[ScopeBuiltin] = nil;
	tails[ScopeBuiltin] = nil;
}

void
declstart(void)
{
	Decl *d;

	iota = mkids(&nosrc, enter("iota", 0), tint, nil);
	iota->init = mkconst(&nosrc, 0);

	scope = ScopeNils;
	scopes[ScopeNils] = nil;
	tails[ScopeNils] = nil;

	nildecl = mkdecl(&nosrc, Dglobal, tany);
	nildecl->sym = enter("nil", 0);
	installids(Dglobal, nildecl);
	d = mkdecl(&nosrc, Dglobal, tstring);
	d->sym = enter("", 0);
	installids(Dglobal, d);

	scope = ScopeGlobal;
	scopes[ScopeGlobal] = nil;
	tails[ScopeGlobal] = nil;
}

void
redecl(Decl *d)
{
	Decl *old;

	old = d->sym->decl;
	if(old->store == Dwundef)
		return;
	error(d->src.start, "redeclaration of %K, previously declared as %k on line %L",
		d, old, old->src.start);
}

void
checkrefs(Decl *d)
{
	Decl *id, *m;
	long refs;

	for(; d != nil; d = d->next){
		switch(d->store){
		case Dtype:
			refs = d->refs;
			if(d->ty->kind == Tadt){
				for(id = d->ty->ids; id != nil; id = id->next){
					d->refs += id->refs;
					if(id->store != Dfn)
						continue;
					if(id->init == nil && d->importid == nil)
						error(d->src.start, "function %s.%s not defined", d->sym->name, id->sym->name);
					if(!id->refs && d->importid == nil)
						warn(d->src.start, "function %s.%s not referenced", d->sym->name, id->sym->name);
				}
			}
			if(d->ty->kind == Tmodule){
				for(id = d->ty->ids; id != nil; id = id->next){
					refs += id->refs;
					if(id->iface != nil)
						id->iface->refs += id->refs;
					if(id->store == Dtype){
						for(m = id->ty->ids; m != nil; m = m->next){
							refs += m->refs;
							if(m->iface != nil)
								m->iface->refs += m->refs;
						}
					}
				}
				d->refs = refs;
			}
			if(superwarn && !refs)
				warn(d->src.start, "%K not referenced", d);
			break;
		case Dglobal:
			if(!superwarn)
				break;
		case Dlocal:
		case Darg:
			if(!d->refs && d->sym != nil
			&& d->sym->name != nil && d->sym->name[0] != '.')
				warn(d->src.start, "%K not referenced", d);
			break;
		case Dconst:
			if(superwarn && !d->refs && d->sym != nil)
				warn(d->src.start, "%K not referenced", d);
			if(d->ty == tstring && d->init != nil)
				d->init->decl->refs += d->refs;
			break;
		case Dfn:
			if(d->init == nil && d->importid == nil)
				error(d->src.start, "%K not defined", d);
			if(superwarn && !d->refs)
				warn(d->src.start, "%K not referenced", d);
			break;
		case Dimport:
			if(superwarn && !d->refs)
				warn(d->src.start, "%K not referenced", d);
			break;
		}
	}
}

Node*
vardecl(Decl *ids, Type *t)
{
	Node *n;

	n = mkn(Ovardecl, mkn(Oseq, nil, nil), nil);
	n->decl = ids;
	n->ty = t;
	return n;
}

void
vardecled(Node *n)
{
	Decl *ids, *last;
	Type *t;
	int store;

	store = Dlocal;
	if(scope == ScopeGlobal)
		store = Dglobal;
	ids = n->decl;
	installids(store, ids);
	t = n->ty;
	for(last = ids; ids != nil; ids = ids->next){
		ids->ty = t;
		last = ids;
	}
	n->left->decl = last;
}

Node*
condecl(Decl *ids, Node *init)
{
	Node *n;

	n = mkn(Ocondecl, mkn(Oseq, nil, nil), init);
	n->decl = ids;
	return n;
}

void
condecled(Node *n)
{
	Decl *ids, *last;

	ids = n->decl;
	installids(Dconst, ids);
	for(last = ids; ids != nil; ids = ids->next){
		ids->ty = tunknown;
		last = ids;
	}
	n->left->decl = last;
}

Node*
importdecl(Node *m, Decl *ids)
{
	Node *n;

	n = mkn(Oimport, mkn(Oseq, nil, nil), m);
	n->decl = ids;
	return n;
}

void
importdecled(Node *n)
{
	Decl *ids, *last;

	ids = n->decl;
	installids(Dimport, ids);
	for(last = ids; ids != nil; ids = ids->next){
		ids->ty = tunknown;
		last = ids;
	}
	n->left->decl = last;
}

Node*
mkscope(Node *body)
{
	Node *n;

	n = mkn(Oscope, nil, body);
	if(body != nil)
		n->src = body->src;
	return n;
}

Node*
fndecl(Node *n, Type *t, Node *body)
{
	n = mkbin(Ofunc, n, body);
	n->ty = t;
	return n;
}

void
fndecled(Node *n)
{
	Decl *d;
	Node *left;

	left = n->left;
	if(left->op == Oname){
		d = left->decl->sym->decl;
		if(d == nil){
			d = mkids(&left->src, left->decl->sym, n->ty, nil);
			installids(Dfn, d);
		}
		left->decl = d;
		d->refs++;
	}
	pushscope();
	installids(Darg, n->ty->ids);
	n->ty->ids = popscope();
}

/*
 * check the function declaration only
 * the body will be type checked later by fncheck
 */
Decl *
fnchk(Node *n)
{
	Decl *d, *inadt;
	Type *t;

	d = n->left->decl;
	if(n->left->op == Odot)
		d = n->left->right->decl;
	if(d == nil)
		fatal("decl() fnchk nil");
	n->left->decl = d;
	if(d->store == Dglobal || d->store == Dfield)
		d->store = Dfn;
	if(d->store != Dfn || d->init != nil)
		nerror(n, "redeclaration of function %D, previously declared as %k on line %L",
			d, d, d->src.start);
	d->init = n;

	t = n->ty;
	inadt = d->dot;
	if(inadt != nil && (inadt->store != Dtype || inadt->ty->kind != Tadt))
		inadt = nil;
	t = validtype(t, inadt);
	if(debug['d'])
		print("declare function %D ty %T newty %T\n", d, d->ty, t);
	t = usetype(t);

	if(!tcompat(d->ty, t, 0))
		nerror(n, "type mismatch: %D defined as %T declared as %T on line %L",
			d, t, d->ty, d->src.start);
	if(t->varargs != 0)
		nerror(n, "cannot define functions with a '*' argument, such as %D", d);

	d->ty = t;
	d->offset = idoffsets(t->ids, MaxTemp, IBY2WD);
	d->src = n->src;

	d->locals = nil;

	n->ty = t;

	return d;
}

Node*
globalas(Node *dst, Node *v, int valok)
{
	Node *tv;

	if(v == nil)
		return nil;
	if(v->op == Oas || v->op == Odas){
		v = globalas(v->left, v->right, valok);
		if(v == nil)
			return nil;
	}else if(valok && !initable(dst, v, 0))
		return nil;
	switch(dst->op){
	case Oname:
		if(dst->decl->init != nil)
			nerror(dst, "duplicate assignment to %V, previously assigned on line %L",
				dst, dst->decl->init->src.start);
		if(valok)
			dst->decl->init = v;
		return v;
	case Otuple:
		if(valok && v->op != Otuple)
			fatal("can't deal with %n in tuple case of globalas", v);
		tv = v->left;
		for(dst = dst->left; dst != nil; dst = dst->right){
			globalas(dst->left, tv->left, valok);
			if(valok)
				tv = tv->right;
		}
		return v;
	}
	fatal("can't deal with %n in globalas", dst);
	return nil;
}

int
needsstore(Decl *d)
{
	if(!d->refs)
		return 0;
	if(d->importid != nil)
		return 0;
	if(storespace[d->store])
		return 1;
	return 0;
}

/*
 * return the list of all referenced storage variables
 */
Decl*
vars(Decl *d)
{
	Decl *v, *n;

	while(d != nil && !needsstore(d))
		d = d->next;
	for(v = d; v != nil; v = v->next){
		while(v->next != nil){
			n = v->next;
			if(needsstore(n))
				break;
			v->next = n->next;
		}
	}
	return d;
}

/*
 * declare variables from the left side of a := statement
 */
static int
recdasdecl(Node *n, int store)
{
	Decl *d, *old;
	int ok;

	switch(n->op){
	case Otuple:
		ok = 1;
		for(n = n->left; n != nil; n = n->right)
			ok &= recdasdecl(n->left, store);
		return ok;
	case Oname:
		if(n->decl == nildecl)
			return 1;
		d = mkids(&n->src, n->decl->sym, nil, nil);
		installids(store, d);
		old = d->old;
		if(old != nil
		&& old->store != Dfn
		&& old->store != Dwundef
		&& old->store != Dundef)
			warn(d->src.start,  "redeclaration of %K, previously declared as %k on line %L",
				d, old, old->src.start);
		n->decl = d;
		d->refs++;
		return 1;
	}
	return 0;
}

int
dasdecl(Node *n)
{
	int store, ok;

	if(scope == ScopeGlobal)
		store = Dglobal;
	else
		store = Dlocal;

	ok = recdasdecl(n, store);
	if(!ok)
		nerror(n, "illegal declaration expression %V", n);
	return ok;
}

/*
 * declare global variables in nested := expressions
 */
void
gdasdecl(Node *n)
{
	if(n == nil)
		return;

	if(n->op == Odas){
		gdasdecl(n->right);
		dasdecl(n->left);
	}else{
		gdasdecl(n->left);
		gdasdecl(n->right);
	}
}

Decl*
undefed(Src *src, Sym *s)
{
	Decl *d;

	d = mkids(src, s, tnone, nil);
	error(src->start, "%s is not declared", s->name);
	installids(Dwundef, d);
	return d;
}

void
pushscope(void)
{
	if(scope >= MaxScope)
		fatal("scope too deep");
	scope++;
	scopes[scope] = nil;
	tails[scope] = nil;
}

Decl*
curscope(void)
{
	return scopes[scope];
}

/*
 * revert to old declarations for each symbol in the currect scope.
 * remove the effects of any imported adt types
 * whenever the adt is imported from a module,
 * we record in the type's decl the module to use
 * when calling members.  the process is reversed here.
 */
Decl*
popscope(void)
{
	Decl *id;
	Type *t;

	for(id = scopes[scope]; id != nil; id = id->next){
		if(id->sym != nil){
			id->sym->decl = id->old;
			id->old = nil;
		}
		if(id->importid != nil)
			id->importid->refs += id->refs;
		t = id->ty;
		if(id->store == Dtype
		&& t->decl != nil
		&& t->decl->timport == id)
			t->decl->timport = id->timport;
	}
	return scopes[scope--];
}

/*
 * make a new scope,
 * preinstalled with some previously installed identifiers
 * don't add the identifiers to the scope chain,
 * so they remain separate from any newly installed ids
 *
 * these routines assume no ids are imports
 */
void
repushids(Decl *ids)
{
	Sym *s;

	if(scope >= MaxScope)
		fatal("scope too deep");
	scope++;
	scopes[scope] = nil;
	tails[scope] = nil;

	for(; ids != nil; ids = ids->next){
		if(ids->scope != scope
		&& (ids->dot == nil || ids->dot->sym != impmod
			|| ids->scope != ScopeGlobal || scope != ScopeGlobal + 1))
			fatal("repushids scope mismatch");
		s = ids->sym;
		if(s != nil){
			if(s->decl != nil && s->decl->scope >= scope)
				ids->old = s->decl->old;
			else
				ids->old = s->decl;
			s->decl = ids;
		}
	}
}

/*
 * pop a scope which was started with repushids
 * return any newly installed ids
 */
Decl*
popids(Decl *ids)
{
	for(; ids != nil; ids = ids->next){
		if(ids->sym != nil){
			ids->sym->decl = ids->old;
			ids->old = nil;
		}
	}
	return popscope();
}

void
installids(int store, Decl *ids)
{
	Decl *d, *last;
	Sym *s;

	last = nil;
	for(d = ids; d != nil; d = d->next){
		d->scope = scope;
		if(d->store == Dundef)
			d->store = store;
		s = d->sym;
		if(s != nil){
			if(s->decl != nil && s->decl->scope >= scope){
				redecl(d);
				d->old = s->decl->old;
			}else
				d->old = s->decl;
			s->decl = d;
		}
		last = d;
	}
	if(ids != nil){
		d = tails[scope];
		if(d == nil)
			scopes[scope] = ids;
		else
			d->next = ids;
		tails[scope] = last;
	}
}

Decl*
mkids(Src *src, Sym *s, Type *t, Decl *next)
{
	Decl *d;
	static Decl z;

	d = mkdecl(src, Dundef, t);
	d->next = next;
	d->sym = s;
	return d;
}

Decl*
mkdecl(Src *src, int store, Type *t)
{
	Decl *d;
	static Decl z;

	d = allocmem(sizeof *d);
	*d = z;
	d->src = *src;;
	d->store = store;
	d->ty = t;
	return d;
}

Decl*
dupdecl(Decl *old)
{
	Decl *d;

	d = allocmem(sizeof *d);
	*d = *old;
	d->next = nil;
	return d;
}

Decl*
appdecls(Decl *d, Decl *dd)
{
	Decl *t;

	if(d == nil)
		return dd;
	for(t = d; t->next != nil; t = t->next)
		;
	t->next = dd;
	return d;
}

Decl*
revids(Decl *id)
{
	Decl *d, *next;

	d = nil;
	for(; id != nil; id = next){
		next = id->next;
		id->next = d;
		d = id;
	}
	return d;
}

long
idoffsets(Decl *id, long offset, int al)
{
	for(; id != nil; id = id->next){
		if(storespace[id->store]){
usedty(id->ty);
			offset = align(offset, id->ty->align);
			id->offset = offset;
			offset += id->ty->size;
		}
	}
	return align(offset, al);
}

int
declconv(va_list *arg, Fconv *f)
{
	Decl *d;
	char buf[4096], *s;

	d = va_arg(*arg, Decl*);
	if(d->sym == nil)
		s = "<???>";
	else
		s = d->sym->name;
	seprint(buf, buf+sizeof(buf), "%s %s", storename[d->store], s);
	strconv(buf, f);
	return 0;
}

int
storeconv(va_list *arg, Fconv *f)
{
	Decl *d;
	char buf[4096];

	d = va_arg(*arg, Decl*);
	seprint(buf, buf+sizeof(buf), "%s%s", storeart[d->store], storename[d->store]);
	strconv(buf, f);
	return 0;
}

int
dotconv(va_list *arg, Fconv *f)
{
	Decl *d;
	char buf[4096], *p, *s;

	d = va_arg(*arg, Decl*);
	buf[0] = 0;
	p = buf;
	if(d->dot != nil && d->dot->sym != impmod){
		s = ".";
		if(d->dot->ty != nil && d->dot->ty->kind == Tmodule)
			s = "->";
		p = seprint(buf, buf+sizeof(buf), "%D%s", d->dot, s);
	}
	seprint(p, buf+sizeof(buf), "%s", d->sym->name);
	strconv(buf, f);
	return 0;
}

/*
 * merge together two sorted lists, yielding a sorted list
 */
static Decl*
namemerge(Decl *e, Decl *f)
{
	Decl rock, *d;

	d = &rock;
	while(e != nil && f != nil){
		if(strcmp(e->sym->name, f->sym->name) <= 0){
			d->next = e;
			e = e->next;
		}else{
			d->next = f;
			f = f->next;
		}
		d = d->next;
	}
	if(e != nil)
		d->next = e;
	else
		d->next = f;
	return rock.next;
}

/*
 * recursively split lists and remerge them after they are sorted
 */
static Decl*
recnamesort(Decl *d, int n)
{
	Decl *r, *dd;
	int i, m;

	if(n <= 1)
		return d;
	m = n / 2 - 1;
	dd = d;
	for(i = 0; i < m; i++)
		dd = dd->next;
	r = dd->next;
	dd->next = nil;
	return namemerge(recnamesort(d, n / 2),
			recnamesort(r, (n + 1) / 2));
}

/*
 * sort the ids by name
 */
Decl*
namesort(Decl *d)
{
	Decl *dd;
	int n;

	n = 0;
	for(dd = d; dd != nil; dd = dd->next)
		n++;
	return recnamesort(d, n);
}

void
printdecls(Decl *d)
{
	for(; d != nil; d = d->next)
		print("%d: %K %T ref %d\n", d->offset, d, d->ty, d->refs);
}
