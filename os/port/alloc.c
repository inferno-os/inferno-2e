#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "interp.h"
#include "audit.h"

#define left	u.s.bhl
#define right	u.s.bhr
#define fwd	u.s.bhf
#define prev	u.s.bhv
#define parent	u.s.bhp

struct Pool
{
	char*	name;
	int	pnum;
	ulong	maxsize;
	int	quanta;
	int	chunk;
	ulong	cursize;
	ulong	arenasize;
	ulong	hw;
	Lock	l;
	Bhdr*	root;
	Bhdr*	chain;
	int	nalloc;
	int	nfree;
	int	nbrk;
	int	lastfree;
	void	(*move)(void*, void*);
};

static
struct
{
	int	n;
	Pool	pool[MAXPOOL];
	// Lock	l;
} table = {
	3,
	{
		{ "main",  0,	 4*1024*1024, 31,  128*1024 },
		{ "heap",  1,	16*1024*1024, 31,  128*1024 },
		{ "image", 2,	 8*1024*1024, 31, 300*1024 },
	}
};

Pool*	mainmem = &table.pool[0];
Pool*	heapmem = &table.pool[1];
Pool*	imagmem = &table.pool[2];

static void _auditmemloc(char *, void *);
void (*auditmemloc)(char *, void *) = _auditmemloc;
static void _poolfault(void *, char *, ulong);
void (*poolfault)(void *, char *, ulong) = _poolfault;

int	poolcompact(Pool*);

void
poolimmutable(void *v)
{
	Bhdr *b;

	D2B(b, v);
	b->magic = MAGIC_I;
}

void
poolmutable(void *v)
{
	Bhdr *b;

	D2B(b, v);
	b->magic = MAGIC_A;
}

Bhdr*
poolchain(Pool *p)
{
	return p->chain;
}

void
pooldel(Pool *p, Bhdr *t)
{
	Bhdr *s, *f, *rp, *q;

	if(t->parent == nil && p->root != t) {
		t->prev->fwd = t->fwd;
		t->fwd->prev = t->prev;
		return;
	}

	if(t->fwd != t) {
		f = t->fwd;
		s = t->parent;
		f->parent = s;
		if(s == nil)
			p->root = f;
		else {
			if(s->left == t)
				s->left = f;
			else
				s->right = f;
		}

		rp = t->left;
		f->left = rp;
		if(rp != nil)
			rp->parent = f;
		rp = t->right;
		f->right = rp;
		if(rp != nil)
			rp->parent = f;

		t->prev->fwd = t->fwd;
		t->fwd->prev = t->prev;
		return;
	}

	if(t->left == nil)
		rp = t->right;
	else {
		if(t->right == nil)
			rp = t->left;
		else {
			f = t;
			rp = t->right;
			s = rp->left;
			while(s != nil) {
				f = rp;
				rp = s;
				s = rp->left;
			}
			if(f != t) {
				s = rp->right;
				f->left = s;
				if(s != nil)
					s->parent = f;
				s = t->right;
				rp->right = s;
				if(s != nil)
					s->parent = rp;
			}
			s = t->left;
			rp->left = s;
			s->parent = rp;
		}
	}
	q = t->parent;
	if(q == nil)
		p->root = rp;
	else {
		if(t == q->left)
			q->left = rp;
		else
			q->right = rp;
	}
	if(rp != nil)
		rp->parent = q;
}

void
pooladd(Pool *p, Bhdr *q)
{
	int size;
	Bhdr *tp, *t;

	q->magic = MAGIC_F;

	q->left = nil;
	q->right = nil;
	q->parent = nil;
	q->fwd = q;
	q->prev = q;

	t = p->root;
	if(t == nil) {
		p->root = q;
		return;
	}

	size = q->size;

	tp = nil;
	while(t != nil) {
		if(size == t->size) {
			q->prev = t->prev;
			q->prev->fwd = q;
			q->fwd = t;
			t->prev = q;
			return;
		}
		tp = t;
		if(size < t->size)
			t = t->left;
		else
			t = t->right;
	}

	q->parent = tp;
	if(size < tp->size)
		tp->left = q;
	else
		tp->right = q;
}

void
poolsummary(void)
{
	int x = 0;
	char buf[400];

	print("\n");
	print("    cursize     maxsize       hw         nalloc       nfree      nbrk       max      name\n");

	x=poolread( buf, sizeof buf - 1, x );
	buf[x] = 0;
	putstrn(buf, x);
	print("\n");
}

void*
poolalloc(Pool *p, int size)
{
	Bhdr *q, *t;
	int alloc, ldr, ns, frag;
	int osize;

	if(size < 0 || size >= 1024*1024*1024)	/* for sanity and to avoid overflow */
		return nil;
	osize = size;
	size = (size + BHDRSIZE + p->quanta) & ~(p->quanta);

	ilock(&p->l);
	p->nalloc++;

	t = p->root;
	q = nil;
	while(t) {
		if(t->size == size) {
			t = t->fwd;
			pooldel(p, t);
			t->magic = MAGIC_A;
			p->cursize += t->size;
			if(p->cursize > p->hw)
				p->hw = p->cursize;
			iunlock(&p->l);
			return B2D(t);
		}
		if(size < t->size) {
			q = t;
			t = t->left;
		}
		else
			t = t->right;
	}
	if(q != nil) {
		pooldel(p, q);
		q->magic = MAGIC_A;
		frag = q->size - size;
		if(frag < (size>>2)) {
			p->cursize += q->size;
			if(p->cursize > p->hw)
				p->hw = p->cursize;
			iunlock(&p->l);
			return B2D(q);
		}
		/* Split */
		ns = q->size - size;
		q->size = size;
		B2T(q)->hdr = q;
		t = B2NB(q);
		t->size = ns;
		B2T(t)->hdr = t;
		pooladd(p, t);
		p->cursize += q->size;
		if(p->cursize > p->hw)
			p->hw = p->cursize;
		iunlock(&p->l);
		return B2D(q);
	}

	ns = p->chunk;
	if(size > ns)
		ns = size;
	ldr = p->quanta+1;

	alloc = ns+ldr+sizeof(t->magic);
	p->arenasize += alloc;
	if(p->arenasize > p->maxsize) {
		p->arenasize -= alloc;
		ns = p->maxsize-p->arenasize-ldr-sizeof(t->magic);
		ns &= ~p->quanta;
		if (ns < size) {
			if(poolcompact(p)) {
				iunlock(&p->l);
				return poolalloc(p, osize);
			}

			iunlock(&p->l);
			if ((p != mainmem) || (ns > 512))
				print("arena too large: %s size %d cursize %lud arenasize %lud maxsize %lud, alloc = %d\n", p->name, osize, p->cursize, p->arenasize, p->maxsize, alloc);
			return nil;
		}
		alloc = ns+ldr+sizeof(t->magic);
		p->arenasize += alloc;
	}

	p->nbrk++;
	t = xalloc(alloc + p->quanta);
	if(t == nil) {
		p->nbrk--;
		iunlock(&p->l);
		return nil;
	}
	/* Alignment at 'quanta' boundary (to help audit) */
	t = (Bhdr *)(((ulong)t + p->quanta) & ~p->quanta);

	t->magic = MAGIC_E;		/* Make a leader */
	t->size = ldr;
	t->csize = ns+ldr;
	t->clink = p->chain;
	p->chain = t;
	B2T(t)->hdr = t;
	t = B2NB(t);

	t->magic = MAGIC_A;		/* Make the block we are going to return */
	t->size = size;
	B2T(t)->hdr = t;
	q = t;

	ns -= size;			/* Free the rest */
	if(ns > 0) {
		q = B2NB(t);
		q->size = ns;
		B2T(q)->hdr = q;
		pooladd(p, q);
	}
	B2NB(q)->magic = MAGIC_E;	/* Mark the end of the chunk */

	p->cursize += t->size;
	if(p->cursize > p->hw)
		p->hw = p->cursize;
	iunlock(&p->l);
	return B2D(t);
}

void
poolfree(Pool *p, void *v)
{
	Bhdr *b, *c;

	D2B(b, v);
	if (p->pnum) AUDITFREE(p->pnum, getcallerpc(&p), v);

	ilock(&p->l);
	p->nfree++;
	p->cursize -= b->size;

	c = B2NB(b);
	if(c->magic == MAGIC_F) {	/* Join forward */
		pooldel(p, c);
		c->magic = 0;
		b->size += c->size;
		B2T(b)->hdr = b;
	}

	c = B2PT(b)->hdr;
	if(c->magic == MAGIC_F) {	/* Join backward */
		pooldel(p, c);
		b->magic = 0;
		c->size += b->size;
		b = c;
		B2T(b)->hdr = b;
	}

	pooladd(p, b);
	iunlock(&p->l);
}

static ulong
poolmax(Pool *p)
{
	Bhdr *t;
	ulong size;

	ilock(&p->l);
	size = (p->maxsize - p->arenasize) & ~p->quanta
			- sizeof(t->magic) - (p->quanta+1);
	t = p->root;
	if(t != nil) {
		while(t->right != nil)
			t = t->right;
		if(size < t->size)
			size = t->size;
	}
	size = size - BHDRSIZE;
	iunlock(&p->l);
	if(size < 0)
		return 0;
	return size;
}

int
poolread(char *va, int count, ulong offset)
{
	Pool *p;
	int n, i, signed_off;

	n = 0;
	signed_off = offset;
	for(i = 0; i < table.n; i++) {
		p = &table.pool[i];
		n += snprint(va+n, count-n, "%11lud %11lud %11lud %11d %11d %11d %11lud %s\n",
			p->cursize,
			p->maxsize,
			p->hw,
			p->nalloc,
			p->nfree,
			p->nbrk,
			poolmax(p),
			p->name);

		if(signed_off > 0) {
			signed_off -= n;
			if(signed_off < 0) {
				memmove(va, va+n+signed_off, -signed_off);
				n = -signed_off;
			}
			else
				n = 0;
		}

	}
	return n;
}

void*
malloc(ulong size)
{
	void *v;

	v = poolalloc(mainmem, size);
	AUDITALLOC(0, getcallerpc(&size), v);
	if(v != nil)
		memset(v, 0, size);
	return v;
}

void*
smalloc(ulong size)
{
	void *v;

	for(;;) {
		v = poolalloc(mainmem, size);
		if(v != nil)
			break;
		tsleep(&up->sleep, return0, 0, 100);
	}
	AUDITALLOC(0, getcallerpc(&size), v);
	memset(v, 0, size);
	return v;
}

void*
mallocz(ulong size, int clr)
{
	void *v;

	v = poolalloc(mainmem, size);
	AUDITALLOC(0, getcallerpc(&size), v);
	if(clr && v != nil)
		memset(v, 0, size);
	return v;
}

void
free(void *v)
{
	Bhdr *b;

	if(v != nil) {
		D2B(b, v);
		AUDITFREE(0, getcallerpc(&v), v);
		poolfree(mainmem, v);
	}
}

void*
realloc(void *v, ulong size)
{
	Bhdr *b;
	void *nv;
	int osize;

	if (v != nil) {
		D2B(b, v);

		osize = b->size - BHDRSIZE;
		if(osize >= size)
			return v;
	}
	else
		SET(osize);

	nv = poolalloc(mainmem, size);
	AUDITALLOC(0, getcallerpc(&v), nv);
	if(nv != nil) {
		if (v != nil) {
			memmove(nv, v, osize);
			AUDITFREE(0, getcallerpc(&v), v);
			poolfree(mainmem, v);
		}
		else
			memset(nv, 0, size);
	}
	return nv;
}

int
msize(void *v)
{
	Bhdr *b;

	D2B(b, v);
	return b->size - BHDRSIZE;
}

void*
calloc(ulong n, ulong szelem)
{
	return malloc(n*szelem);
}

void
pooldump(Bhdr *b, int d, int c)
{
	Bhdr *t;

	if(b == nil)
		return;

	print("%.8lux %.8lux %.8lux %c %4d %lud (f %.8lux p %.8lux)\n",
		b, b->left, b->right, c, d, b->size, b->fwd, b->prev);
	d++;
	for(t = b->fwd; t != b; t = t->fwd)
		print("\t%.8lux %.8lux %.8lux\n", t, t->prev, t->fwd);
	pooldump(b->left, d, 'l');
	pooldump(b->right, d, 'r');
}

void
poolshow(void)
{
	int i;

	for(i = 0; i < table.n; i++) {
		print("Arena: %s root=%.8lux\n", table.pool[i].name, table.pool[i].root);
		pooldump(table.pool[i].root, 0, 'R');
	}
}

void
poolsetcompact(Pool *p, void (*move)(void*, void*))
{
	p->move = move;
}

int
poolcompact(Pool *pool)
{
	Bhdr *base, *limit, *ptr, *end, *next;
	int compacted, recov, nb;

	if(pool->move == nil || pool->lastfree == pool->nfree)
		return 0;

	pool->lastfree = pool->nfree;

	base = pool->chain;
	ptr = B2NB(base);	/* First Block in arena has clink */
	limit = B2LIMIT(base);
	compacted = 0;

	pool->root = nil;
	end = ptr;
	recov = 0;
	while(base != nil) {
		next = B2NB(ptr);
		if(ptr->magic == MAGIC_A || ptr->magic == MAGIC_I) {
			if(ptr != end) {
				memmove(end, ptr, ptr->size);
				pool->move(B2D(ptr), B2D(end));
				recov = (uchar*)ptr - (uchar*)end;
				compacted = 1;
			}
			end = B2NB(end);
		}
		if(next >= limit) {
			nb = (uchar*)limit - (uchar*)end;
print("recovered %d bytes\n", recov);
print("%d bytes at end\n", nb);
			if(nb > 0){
				if(nb < pool->quanta+1)
					panic("poolcompact: leftover too small\n");
				end->size = nb;
				pooladd(pool, end);
			}
			base = base->clink;
			if(base == nil)
				break;
			ptr = B2NB(base);
			end = ptr;	/* could do better by copying between chains */
			limit = B2LIMIT(base);
			recov = 0;
		} else
			ptr = next;
	}

	return compacted;
}

void
poolsize(Pool *p, int max, int contig)
{
	void *x;

	p->maxsize = max;
	if (contig) {
		p->chunk = max-1024;
		x = poolalloc(p, p->chunk);
		if(x == nil)
			panic("poolsize: don't have %d bytes\n", p->chunk);
		poolfree(p, x);
		p->hw = 0;
	}
}

static void
_poolfault(void *v, char *msg, ulong c)
{
	setpanic();
	auditmemloc(msg, v);
	panic("%s %lux (from %N/%N)", msg, v, getcallerpc(&v), c);
}

static void
dumpvl(char *msg, ulong *v, int n)
{
	int	ii;
	int	ll;

	ll = print("%s at %lux: ", msg, v);
	for (ii = 0; ii < n; ii++) {
		if (ll >= 60) {
			print("\n");
			ll = print("    %lux: ", v);
		}
		ll += print(" %lux", *v++);
	}
	print("\n");
	USED(ll);
}

static void
corrupted(char *str, char *msg, Pool *p, Bhdr *b, void *v)
{
	print("%s(%lux): pool %s CORRUPT: %s at %lux'%lud(magic=%lux)\n",
		str, v, p->name, msg, b, b->size, b->magic);
	dumpvl("bad Bhdr", (ulong *)((ulong)b & ~3)-4, 10);
}

static void
_auditmemloc(char *str, void *v)
{
	Pool *p;
	Bhdr *bc, *ec, *b, *nb, *fb = nil;
	char *fmsg, *msg;
	ulong fsz;

	SET(fsz, fmsg);
	for (p = &table.pool[0]; p < &table.pool[nelem(table.pool)]; p++) {
		ilock(&p->l);
		for (bc = p->chain; bc != nil; bc = bc->clink) {
			if (bc->magic != MAGIC_E) {
				iunlock(&p->l);
				corrupted(str, "chain hdr!=MAGIC_E", p, bc, v);
				goto nextpool;
			}
			ec = B2LIMIT(bc);
			if ((v >= bc) && (v < ec))
				goto found;
		}
		iunlock(&p->l);
nextpool:	;
	}
	print("%s: %lux not in pools\n", str, v);
	return;

found:
	for (b = bc; b < ec; b = nb) {
		switch(b->magic) {
		case MAGIC_F:
			msg = "free blk";
			break;
		case MAGIC_I:
			msg = "immutable block";
			break;
		case MAGIC_A:
			msg = "block";
			break;
		default:
			if (b == bc && b->magic == MAGIC_E) {
				msg = "pool hdr";
				break;
			}
			iunlock(&p->l);
			corrupted(str, "bad magic", p, b, v);
			goto badchunk;
		}
		if (b->size <= 0 || (b->size & p->quanta)) {
			iunlock(&p->l);
			corrupted(str, "bad size", p, b, v);
			goto badchunk;
		}
		if (fb != nil)
			break;
		nb = B2NB(b);
		if (v < nb) {
			fb = b;
			fsz = b->size;
			fmsg = msg;
		}
	}
	iunlock(&p->l);
	if (b >= ec) {
		if (b > ec)
			corrupted(str, "chain size mismatch", p, b, v);
		else if (b->magic != MAGIC_E)
			corrupted(str, "chain end!=MAGIC_E", p, b, v);
	}
badchunk:
	if (fb != nil) {
		print("%s: %lux in %s:", str, v, p->name);
		if (fb == v)
			print(" is %s '%lux\n", fmsg, fsz);
		else
			print(" in %s at %lux'%lux\n", fmsg, fb, fsz);
		dumpvl("area", (ulong *)((ulong)v & ~3)-4, 20);
	}
}

char *
poolaudit(char*(*audit)(int, Bhdr *))
{
	Pool *p;
	Bhdr *bc, *ec, *b;
	char *r = nil;

	for (p = &table.pool[0]; p < &table.pool[nelem(table.pool)]; p++) {
		ilock(&p->l);
		for (bc = p->chain; bc != nil; bc = bc->clink) {
			if (bc->magic != MAGIC_E) {
				iunlock(&p->l);
				return "bad chain hdr";
			}
			ec = B2LIMIT(bc);
			for (b = bc; b < ec; b = B2NB(b)) {
				if (b->size <= 0 || (b->size & p->quanta))
					r = "bad size in bhdr";
				else
					switch(b->magic) {
					case MAGIC_E:
						if (b != bc) {
							r = "unexpected MAGIC_E";
							break;
						}
					case MAGIC_F:
					case MAGIC_A:
					case MAGIC_I:
						r = audit(p->pnum, b);
						break;
					default:
						r = "bad magic";
					}
				if (r != nil) {
					iunlock(&p->l);
					return r;
				}
			}
			if (b != ec || b->magic != MAGIC_E) {
				iunlock(&p->l);
				return "bad chain ending";
			}
		}
		iunlock(&p->l);
	}
	return r;
}

void
poolinit(void)
{
	debugkey('m', "memory pools", poolsummary, 0);
}
