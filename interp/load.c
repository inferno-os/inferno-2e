#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "raise.h"
#include <kernel.h>

#define	A(r)	*((Array**)(r))

Module*	modules;

static int
operand(uchar **p)
{
	int c;
	uchar *cp;

	cp = *p;
	c = cp[0];
	switch(c & 0xC0) {
	case 0x00:
		*p = cp+1;
		return c;
	case 0x40:
		*p = cp+1;
		return c|~0x7F;
	case 0x80:
		*p = cp+2;
		if(c & 0x20)
			c |= ~0x3F;
		else
			c &= 0x3F;
		return (c<<8)|cp[1];		
	case 0xC0:
		*p = cp+4;
		if(c & 0x20)
			c |= ~0x3F;
		else
			c &= 0x3F;
		return (c<<24)|(cp[1]<<16)|(cp[2]<<8)|cp[3];		
	}
	return 0;	
}

static ulong
disw(uchar **p)
{
	ulong v;
	uchar *c;

	c = *p;
	v  = c[0] << 24;
	v |= c[1] << 16;
	v |= c[2] << 8;
	v |= c[3];
	*p = c + 4;
	return v;
}

double
canontod(ulong v[2])
{
	union { double d; unsigned long ul[2]; } a;
	a.d = 1.;
	if(a.ul[0]) {
		a.ul[0] = v[0];
		a.ul[1] = v[1];
	}
	else {
		a.ul[1] = v[0];
		a.ul[0] = v[1];
	}
	return a.d;
}

Type*
dtype(void (*destroy)(Heap*, int), int size, uchar *map, int mapsize)
{
	Type *t;

	t = malloc(sizeof(Type)+mapsize);
	if(t != nil) {
		t->ref = 1;
		t->free = destroy;
		t->mark = markheap;
		t->size = size;
		t->np = mapsize;
		memmove(t->map, map, mapsize);
	}
	return t;
}

int
brpatch(Inst *ip, Module *m)
{
	switch(ip->op) {
	case ICALL:
	case IJMP:
	case IBEQW:
	case IBNEW:
	case IBLTW:
	case IBLEW:
	case IBGTW:
	case IBGEW:
	case IBEQB:
	case IBNEB:
	case IBLTB:
	case IBLEB:
	case IBGTB:
	case IBGEB:
	case IBEQF:
	case IBNEF:
	case IBLTF:
	case IBLEF:
	case IBGTF:
	case IBGEF:
	case IBEQC:
	case IBNEC:
	case IBLTC:
	case IBLEC:
	case IBGTC:
	case IBGEC:
	case IBEQL:
	case IBNEL:
	case IBLTL:
	case IBLEL:
	case IBGTL:
	case IBGEL:
	case ISPAWN:
		if(ip->d.imm < 0 || ip->d.imm >= m->nprog)
			return 0;
		ip->d.imm = (WORD)&m->prog[ip->d.imm];
		break;
	}
	return 1;
}

Module*
parsemod(char *path, uchar *code, ulong mtime, Qid qid)
{
	Heap *h;
	Inst *ip;
	Type *pt;
	String *s;
	Module *m;
	Array *ary;
	ulong ul[2];
	WORD lo, hi;
	int lsize, id, v, entry, entryt, tnp, tsz, siglen;
	int de, pc, i, n, isize, dsize, hsize, dasp;
	uchar *mod, sm, *istream, **isp, *si, *addr, *dastack[DADEPTH];

	istream = code;
	isp = &istream;

	m = malloc(sizeof(Module));
	if(m == nil)
		return nil;

	m->mtime = mtime;
	m->qid = qid;
	m->origmp = H;

	switch(operand(isp)) {
	default:
		kwerrstr("bad magic");
		goto bad;
	case SMAGIC:
		siglen = operand(isp);
		if(verifysigner(*isp, siglen, *isp+siglen) == 0) {
			kwerrstr("security violation");
			goto bad;
		}
		*isp += siglen;
		break;		
	case XMAGIC:
		break;
	}

	m->rt = operand(isp);
	m->ss = operand(isp);
	isize = operand(isp);
	dsize = operand(isp);
	hsize = operand(isp);
	lsize = operand(isp);
	entry = operand(isp);
	entryt = operand(isp);

	m->nprog = isize;
	m->prog = mallocz(isize*sizeof(Inst), 0);
	if(m->prog == nil) {
		kwerrstr("no memory");
		goto bad;
	}

	m->ref = 1;

	ip = m->prog;
	for(i = 0; i < isize; i++) {
		ip->op = *istream++;
		ip->add = *istream++;
		ip->reg = 0;
		ip->s.imm = 0;
		ip->d.imm = 0;
		switch(ip->add & ARM) {
		case AXIMM:
		case AXINF:
		case AXINM:
			ip->reg = operand(isp);
		 	break;
		}
		switch(UXSRC(ip->add)) {
		case SRC(AFP):
		case SRC(AMP):	
		case SRC(AIMM):
			ip->s.ind = operand(isp);
			break;
		case SRC(AIND|AFP):
		case SRC(AIND|AMP):
			ip->s.i.f = operand(isp);
			ip->s.i.s = operand(isp);
			break;
		}
		switch(UXDST(ip->add)) {
		case DST(AFP):
		case DST(AMP):	
			ip->d.ind = operand(isp);
			break;
		case DST(AIMM):
			ip->d.ind = operand(isp);
			if(brpatch(ip, m) == 0) {
				kwerrstr("bad branch addr");
				goto bad;
			}
			break;
		case DST(AIND|AFP):
		case DST(AIND|AMP):
			ip->d.i.f = operand(isp);
			ip->d.i.s = operand(isp);
			break;
		}
		ip++;		
	}

	m->ntype = hsize;
	m->type = malloc(hsize*sizeof(Type*));
	if(m->type == nil) {
		kwerrstr("no memory");
		goto bad;
	}
	for(i = 0; i < hsize; i++) {
		id = operand(isp);
		if(id > hsize) {
			kwerrstr("heap id range");
			goto bad;
		}
		tsz = operand(isp);
		tnp = operand(isp);
		pt = dtype(freeheap, tsz, istream, tnp);
		if(pt == nil) {
			kwerrstr("no memory");
			goto bad;
		}
		istream += tnp;
		m->type[id] = pt;
	}

	if(dsize != 0) {
		pt = m->type[0];
		if(pt == 0 || pt->size != dsize) {
			kwerrstr("bad desc for mp");
			goto bad;
		}
		h = heapz(pt);
		m->origmp = H2D(uchar*, h);
	}
	addr = m->origmp;
	dasp = 0;
	for(;;) {
		sm = *istream++;
		if(sm == 0)
			break;
		n = DLEN(sm);
		if(n == 0)
			n = operand(isp);
		v = operand(isp);
		si = addr + v;
		switch(DTYPE(sm)) {
		default:
			kwerrstr("bad data item");
			goto bad;
		case DEFS:
			s = c2string((char*)istream, n);
			istream += n;
			*(String**)si = s;
			break;
		case DEFB:
			for(i = 0; i < n; i++)
				*si++ = *istream++;
			break;
		case DEFW:
			for(i = 0; i < n; i++) {
				*(WORD*)si = disw(isp);
				si += sizeof(WORD);
			}
			break;
		case DEFL:
			for(i = 0; i < n; i++) {
				hi = disw(isp);
				lo = disw(isp);
				*(LONG*)si = (LONG)hi << 32 | (LONG)(ulong)lo;
				si += sizeof(LONG);
			}
			break;
		case DEFF:
			for(i = 0; i < n; i++) {
				ul[0] = disw(isp);
				ul[1] = disw(isp);
				*(REAL*)si = canontod(ul);
				si += sizeof(REAL);
			}
			break;
		case DEFA:			/* Array */
			v = disw(isp);
			if(v < 0 || v > m->ntype) {
				kwerrstr("bad array type");
				goto bad;
			}
			pt = m->type[v];
			v = disw(isp);
			h = nheap(sizeof(Array)+(pt->size*v));
			h->t = &Tarray;
			h->t->ref++;
			ary = H2D(Array*, h);
			ary->t = pt;
			ary->len = v;
			ary->root = H;
			ary->data = (uchar*)ary+sizeof(Array);
			memset((void*)ary->data, 0, pt->size*v);
			initarray(pt, ary);
			A(si) = ary;
			break;			
		case DIND:			/* Set index */
			ary = A(si);
			if(ary == H || D2H(ary)->t != &Tarray) {
				kwerrstr("ind not array");
				goto bad;
			}
			v = disw(isp);
			if(v > ary->len || v < 0 || dasp >= DADEPTH) {
				kwerrstr("array init range");
				goto bad;
			}
			dastack[dasp++] = addr;
			addr = ary->data+v*ary->t->size;
			break;
		case DAPOP:
			if(dasp == 0) {
				kwerrstr("pop range");
				goto bad;
			}
			addr = dastack[--dasp];
			break;
		}
	}
	mod = istream;
	strncpy(m->name, (char*)mod, NAMELEN);
	while(*istream++)
		;

	for(i = 0; i < lsize; i++) {
		pc = operand(isp);
		de = operand(isp);
		v  = disw(isp);
		pt = nil;
		if(de != -1)
			pt = m->type[de];
		mlink(m, istream, v, pc, pt);
		while(*istream++)
			;
	}

	m->eclr = &ieclr;
	m->entryt = nil;
	m->entry = m->prog;
	if((ulong)entry < isize && (ulong)entryt < hsize) {
		m->entry = &m->prog[entry];
		m->entryt = m->type[entryt];
	}

	if(cflag) {
		if((m->rt&DONTCOMPILE) == 0)
			compile(m, isize, nil);
	}
	else
	if(m->rt & MUSTCOMPILE) {
		if(compile(m, isize, nil) == 0) {
			kwerrstr("compiler required");
			goto bad;
		}
	}

	m->path = strdup(path);
	if(m->path == nil) {
		kwerrstr("no memory");
		goto bad;
	}
	m->link = modules;
	modules = m;

	return m;
bad:
	destroy(m->origmp);
	free(m->prog);
	if(m->type != nil) {
		for(i = 0; i < m->ntype; i++)
			freetype(m->type[i]);
		free(m->type);
	}
	free(m);
	return nil;
}

Module*
newmod(char *s)
{
	Module *m;

	m = malloc(sizeof(Module));
	if(m == nil)
		error(exNomem);
	m->ref = 1;
	m->path = s;
	m->origmp = H;
	strncpy(m->name, s, NAMELEN-1);
	m->link = modules;
	modules = m;
	return m;
}

Module*
lookmod(char *s)
{
	Module *m;

	for(m = modules; m != nil; m = m->link)
		if(strcmp(s, m->path) == 0)
			break;

	if(m != nil)
		m->ref++;

	return m;
}

void
unload(Module *m)
{
	int i;
	Module **last, *mm;

	m->ref--;
	if(m->ref > 0)
		return;
	if(m->ref == -1)
		abort();

	last = &modules;
	for(mm = modules; mm != nil; mm = mm->link) {
		if(mm == m) {
			*last = m->link;
			break;
		}
		last = &mm->link;
	}

	destroy(m->origmp);

	destroylinks(m);

	for(i = 0; i < m->ntype; i++)
		freetype(m->type[i]);

	free(m->type);
	free(m->prog);
	free(m->path);
	free(m);
}
