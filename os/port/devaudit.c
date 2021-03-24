#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"audit.h"


enum{
	Qdir,
	Qctl,
	Qmalloc,
	Qaudit,
};

static Dirtab auditdir[]=
{
	"ctl",		{Qctl},		0,		0666,
	"malloc",	{Qmalloc},	0,		0666,
	"audit",	{Qaudit},	0,		0666,
};

extern Dev auditdevtab;
static void auditinit(void);

enum {
	REPORT_SIZE = 8000,
	MAX_CALLERS = 500,

	NUM_PCHASH = (1<<13),
	PCHASH_MASK = NUM_PCHASH-1,

	MAPQ_BITS = 5,	/* smallest pool quantization */
	MAPQUANTA = (1<<MAPQ_BITS)-1,
		/* Address mapping for a 64 Mbyte address space
		   somewhere in a 128-Mbyte window */
	M_BITS = 10,	/* first-level index */
	S_BITS = 12,	/* second-level index */

	NUM_M = 1<<M_BITS,
	NUM_S = 1<<S_BITS,

	I_MAX = 1<<(M_BITS+S_BITS),
	O_MASK = (I_MAX-1) << MAPQ_BITS,
	ADDRSPACE = I_MAX<<MAPQ_BITS,

	TOOBIG = 2*1024*1024,
};

static QLock memlock;

static char *mallocp;
static int mallocpsz;
static short **segmap;
static ulong pooloffset;
static ushort *caller_ix;
static ulong *caller_pc;
static char *caller_pnum;
static int num_callers;
static int max_callers;
int audit_callers;

typedef struct Memaudit Memaudit;
struct Memaudit {
	long cnt;
	long mem;
};
static Memaudit *caller_cur, *caller_ref;

static char *pname[] = { "Main", "Heap", "Image" };
static void _auditmemloc(char *, void *);

char Enotenabled[] = "Audit not enabled";

#define I2B(i)		((Bhdr *)(((i)<<MAPQ_BITS)+pooloffset))
#define I2V(i)		B2D(I2B(i))
#define V2I(v)		(((ulong)(v)-pooloffset) >> MAPQ_BITS)

#define M2I(m,s)	(((m)<<S_BITS)+(s))
#define M2B(m,s)	I2B(M2I(m,s))
#define I2M(i)		((i) >> S_BITS)
#define I2S(i)		((i)&(NUM_S-1))

#define IS_ALLOC_MAGIC(b)	((b)->magic == MAGIC_A || (b)->magic == MAGIC_I)
#define IS_FREE_MAGIC(b)	((b)->magic == MAGIC_F)
#define IS_EPOOL_MAGIC(b)	((b)->magic == MAGIC_E)

static void
malloccounts(void)
{
	int mm, ss;
	int ix;
	short *m;
	Bhdr *b;


	if (caller_ix == nil)
		return;
	/*
	 * calculate changes based on saved copy
	 */
	memmove(caller_cur, caller_ref, (num_callers+1)*sizeof *caller_cur);
	for (mm = 0; mm < NUM_M; mm++) {
		if ((m = segmap[mm]) == nil)
			continue;
		for (ss = 0; ss < NUM_S; ss++) {
			if ((ix = m[ss]) > 0) {
				b = M2B(mm,ss);
				caller_cur[ix].cnt += 1;
				caller_cur[ix].mem += IS_ALLOC_MAGIC(b) ? b->size : 0;
			}
		}
	}
}

static short *mapmem;
static int mapavail;

static void
initmapmem(void)
{
	int av;

	av = (conf.npage*BY2PG - (ulong)end - KZERO) >> MAPQ_BITS;
	/* memory hog */
	mapmem = xalloc(av * sizeof (short));
	if (mapmem)
		mapavail = av;
}

static short *
allocsegmap(void)
{
	if (mapavail >= NUM_S) {
		short *a;

		mapavail -= NUM_S;
		a = mapmem;
		mapmem += NUM_S;
		return a;
	}
	return xalloc(sizeof (short)*NUM_S);
}

static int
pchash(ulong pc)
{
	int pch = pc & PCHASH_MASK;
	int ix;

	while ((ix = caller_ix[pch]) && (pc != caller_pc[ix]))
		pch = (pch + 1) & PCHASH_MASK;
	return(ix);
}

static int
newcaller(ulong pc)
{
	int ix;
	int pch;

	ix = num_callers + 1;
	if (ix >= max_callers)
		return num_callers;
	num_callers = ix;
	if (ix == max_callers-1)
		print("*** num_callers(%d) overflow, increase max_callers\n", num_callers);

	pch = pc & PCHASH_MASK;
	while (caller_ix[pch])
		pch = (pch + 1) & PCHASH_MASK;
	caller_ix[pch] = ix;
	caller_pc[ix] = pc;
	return ix;
}

static short *
mapv(void *v, int fill)
{
	long ii;
	int mm, em;
	short *m;
	Bhdr *b;

	ii = V2I(v);
	if (ii < 0 || ii >= I_MAX)
		return nil;
	mm = I2M(ii);
	if ((m = segmap[mm]) == nil) {
		m = allocsegmap();
		if (m == nil)
			return nil;
		segmap[mm] = m;
	}
	m += I2S(ii);

	if (fill) {
		b = I2B(ii);
		ii = V2I(B2NB(b))-1;
		if (ii >= I_MAX)
			ii = I_MAX-1;
		for (em = I2M(ii); em > mm; em -= 1) {
			if (segmap[em] == nil)
				segmap[em] = allocsegmap();
		}
	}
	return m;
}

static short *
mapi(long ii)
{
	int mm;
	short *m;

	mm = I2M(ii);
	if ((m = segmap[mm]) == nil)
		return nil;
	return m + I2S(ii);
}

static void
_auditalloc(int pnum, ulong pc, void *v)
{
	int ix;
	short *mptr;

	if (v) {
		if ((mptr = mapv(v, 1)) == nil)
			return;
		ix = pchash(pc);
		if (ix == 0) {
			ix = newcaller(pc);
			caller_pnum[ix] = pnum;
		}
		*mptr = ix;
	}
}

static void
_auditfree(int pnum, ulong pc, void *v)
{
	int ix;
	short *mptr;

	if ((mptr = mapv(v, 0)) == nil)
		return;
	ix = *mptr;
	if (ix > 0) {
		if (caller_pnum[ix] != pnum) {
			_auditmemloc("Free to wrong pool", v);
			print("... by %N to %s\n", pc, pname[pnum]);
		}
	} else if (ix == 0) {
		ix = pchash(pc);
		if (ix == 0) {
			ix = newcaller(pc);
			caller_pnum[ix] = pnum;
		}
	} else
		return;
	*mptr = -ix;
}

static void
unidentified()
{
	print(Ebadarg);
}

static char *
auditscan(int pnum, Bhdr *b)
{
	ulong pc = (ulong)unidentified;

	switch (b->magic) {
	case MAGIC_F:
		_auditfree(pnum, pc+4*pnum, b);
		break;
	case MAGIC_I:
	case MAGIC_A:
		_auditalloc(pnum, pc+4*pnum, b);
		break;
	}
	return nil;
}

static int
bhdrok(Bhdr *b)
{
	long ii;

	if (!IS_ALLOC_MAGIC(b) && !IS_FREE_MAGIC(b))
		return 0;
	if (b->size <= MAPQUANTA || b->size > TOOBIG)
		return 0;
	ii = V2I(B2NB(b))-1;
	if (ii < 0 || ii >= I_MAX)
		return 0;
	return 1;
}

static void
prt_owner(char *fmt, long ii)
{
	Bhdr *b;
	char *pn;
	ulong own;
	int ix;
	short *mptr;

	b = I2B(ii);
	print(fmt, B2D(b));

	mptr = mapi(ii);
	if (mptr == nil) {
		print(" not in pools\n");
		return;
	}
	ix = *mptr;
	if (ix > 0) {
		pn = pname[caller_pnum[ix]];
		own = caller_pc[ix];
		if (IS_ALLOC_MAGIC(b))
			print("(sz=%lux) (%s)owned by %N\n", b->size, pn, own);
		else
			print(" NOT MAGIC_A (%lux,%s) owned by: %N\n", b->magic, pn, own);
	} else if (ix < 0) {
		ix = -ix;
		pn = pname[caller_pnum[ix]];
		own = caller_pc[ix];
		if (IS_FREE_MAGIC(b))
			print("(sz=%lux) FREE", b->size);
		else
			print(" NOT MAGIC_F (%lux)", b->magic);
		print("(%s), previous owner %N\n", pn, own);
	} else {
		if (IS_ALLOC_MAGIC(b))
			print("(sz=%lux) allocated", b->size);
		else if (IS_FREE_MAGIC(b))
			print("(sz=%lux) FREE", b->size);
		else
			print(" not magic (%lux)", b->magic);
		print(", no previous owner\n");
	}
}

/*
 * Find currently allocated (or 'any' remembered) particle at or
 *  above this location.
 */
static long
find_above(long ii, int any)
{
	int mm, ss;
	short *m;

	mm = I2M(ii);
	ss = I2S(ii);
	for (; mm < NUM_M; mm++) {
		if ((m = segmap[mm]) != nil)
			for (; ss < NUM_S; ss++)
				if (m[ss] != 0 && (any || m[ss] > 0))
					return M2I(mm,ss);
		ss = 0;
	}
	return -1;
}

/*
 * Find currently allocated (or 'any' remembered) particle at or
 *  below this location,
 * or else return the lowest addressed currently free particle.
 */
static long
find_below(long ii, int any)
{
	int mm, ss;
	short *m;
	long r = -1;

	mm = I2M(ii);
	ss = I2S(ii);
	for (; mm >= 0; mm--) {
		if ((m = segmap[mm]) != nil)
			for (; ss >= 0; ss--)
				if (m[ss] != 0) {
					r = M2I(mm,ss);
					if (any || m[ss] > 0)
						return r;
				}
		ss = NUM_S-1;
	}
	return r;
}

/*
 * Given a likely block header, return next likely block header, with audits
 */
static long
find_nextb(long ii)
{
	short *mptr;
	Bhdr *b;
	char *when;
	int ix;

	b = I2B(ii);
	if (IS_EPOOL_MAGIC(b))
		return find_above(ii+1, 1);
	if (!bhdrok(b)) {
		print("Pool space corrupted at %lux", b);
		mptr = mapi(ii);
		if (mptr != nil) {
			ix = *mptr;
			if (ix == 0)
				print(", no previous owner\n");
			else {
				if (ix > 0)
					when = "currently";
				else {
					ix = -ix;
					when = "previously";
				}
				print("(%s) %s owned by %N\n",
				   pname[caller_pnum[ix]], when, caller_pc[ix]);
			}
			dumplongs("area", (ulong *)((ulong)b & ~31)-4, 20);
		}
		else
			print("not previously recorded\n");
		return -1;
	}
	return V2I(B2NB(b));
}

/*
 * Find a likely Bhdr containing the given particle
 */
static long
find_owner(long ii)
{
	long jj, kk;

	if ((kk = find_below(ii, 0)) < 0)
		return -1;
	while (1) {
		jj = find_nextb(kk);
		if (jj < 0)
			return -1;
		if (jj >= ii) {
			if (jj == ii)
				return jj;
			return kk;
		}
		kk = jj;
	}
	return -1;
}

/*
 * Heuristics to report relevant structures that may identify an address
 *  and its immediate upper and lower neighbors.  Be careful to access
 *  only addresses that have previously been recorded as part of pools.
 */
static void
_auditmemloc(char *str, void *v)
{
	long ll, ff, ii, nl, nu;
	int ix;

	print("%s %lux: ", str, v);

	ii = V2I(v);
	if (ii < 0 || ii >= I_MAX) {
		print("out of range\n");
		return;
	}
	ff = find_owner(ii);
	if (ff < 0) {
		/*
		 * Pool is corrupted or given address is not in pools.
		 * Find any remembered (i.e. current or former) Bhdr
		 *  here or below.  Pick some 'safe' address to dump.
		 */
		ff = find_below(ii, 1);
		nu = find_above(ii+1, 1);
		if (ff >= 0) {
			prt_owner("nearest %lux", ff);
			nl = find_owner(ff);
			v = I2V(ff);
		} else {
			print("%lux not found in pools\n", v);
			nl = -1;
			if (nu < 0)
				return;
			v = I2V(nu);
		}
	} else {
		if (I2V(ff) == v)
			prt_owner("is block", ff);
		else
			prt_owner("in block at %lux", ff);
		nl = find_owner(ff-1);
		nu = find_nextb(ff);
		if (nu < 0)
			nu = find_above(ii+1, 1);
	}
	/*
	 * Report some other previously owned blocks within the block
	 *  containing 'v'.
	 */
	for (ll = 0; ll <= 60; ii--) {
		ii = find_below(ii, 1);
		if (ii <= ff)
			break;
		if (ll == 0)
			ll = print("  old:");
		ix = *mapi(ii);
		ll += print(" %N", caller_pc[ix < 0 ? -ix : ix]);
	}
	if (ll)
		print("\n");

	if (nl < 0 && ff >= 0)
		nl = find_below(ff-1, 1);
	if (nl >= 0)
		prt_owner("  mem %lux below", nl);
	if (nu >= 0)
		prt_owner("  mem %lux above", nu);
	dumplongs("area", (ulong *)((ulong)v & ~31)-4, 20);
}

static void
_poolfault(void *v, char *str, ulong c)
{
	int interp = (up && up->type == Interp);

	if (!interp)
		setpanic();
	print("%s fault from %N called by %N\n", str, getcallerpc(&v), c);
	_auditmemloc(str, v);
	if (interp)
		disfault(0, str);
	panic(str);
}

static int
xallocok(void **p, ulong sz)
{
	return (*p = xalloc(sz)) != nil;
}

static void
xfreeok(void **p)
{
	if (*p != nil) {
		xfree(*p);
		*p = nil;
	}
}

static void
mallocdbginit(int max)
{
	void *v;
	Bhdr *b;
	char *f;

	if (caller_ix != nil || max == 0)
		return;

	v = malloc(4);
	D2B(b,v);
	free(v);
	/*
	 * Map pools into an address range from 0 to ADDRSPACE
	 * This will adequately handle any sbrk/xalloc space that
	 *  is confined to a contiguous address space ADDRSPACE/2 big.
	 */
	pooloffset = ((ulong)b & ~O_MASK) - ADDRSPACE/2;

	if (
		xallocok(&caller_cur, max * sizeof *caller_cur) &&
		xallocok(&caller_ref, max * sizeof *caller_ref) &&
		xallocok(&caller_pc, max * sizeof *caller_pc) &&
		xallocok(&caller_pnum, max * sizeof *caller_pnum) &&
		xallocok(&segmap, (NUM_M+1) * sizeof *segmap) &&
		xallocok(&mallocp, REPORT_SIZE) &&
		xallocok(&caller_ix, NUM_PCHASH * sizeof *caller_ix)
	) {
		mallocpsz = REPORT_SIZE;
		initmapmem();
		max_callers = max;
		f = poolaudit(auditscan);
		if (f != nil)
			print("Pool audit failed: %s\n", f);
		auditalloc = _auditalloc;
		auditfree = _auditfree;
		poolfault = _poolfault;
		auditmemloc = _auditmemloc;
	} else {
		xfreeok(&caller_cur);
		xfreeok(&caller_ref);
		xfreeok(&caller_pc);
		xfreeok(&caller_pnum);
		xfreeok(&segmap);
		xfreeok(&mallocp);
		xfreeok(&caller_ix);
		print("No extra memory for audit\n");
	}
}

static void
mallocdbgclr(void)
{
	int ii;

	if (caller_ix == nil)
		mallocdbginit(MAX_CALLERS);
	else {
		for (ii = 1; ii <= num_callers; ii++) {
			caller_ref[ii].mem -= caller_cur[ii].mem;
			caller_ref[ii].cnt -= caller_cur[ii].cnt;
		}
		memset(caller_cur, 0, (num_callers+1)*sizeof *caller_cur);
	}
}

static int
mallocsummary(char *buf, int len)
{
	long all_mem, total_sz, sz;
	int num_c, num_alloc, cnt;
	int ix;
	int ll;
	int pnum;
	int blen = len;

	*buf = 0;
	if (caller_ix == nil)
		return 0;
	caller_pc[max_callers-1] = (ulong)unidentified+16;
	/*
	 * calculate changes based on saved copy
	 */
	malloccounts();
	all_mem = 0;
	for (pnum = 0; pnum < nelem(pname); pnum++) {
		/*
		 * Summarize the changes
		 */
		num_c = 0;
		total_sz = 0;
		num_alloc = 0;
		for (ix = 1; ix <= num_callers; ix++) {
			if (caller_pnum[ix] != pnum)
				continue;
			sz = caller_cur[ix].mem;
			cnt = caller_cur[ix].cnt;
			if ((cnt == 0) && (sz == 0))
				continue;
			num_c += 1;
			total_sz += sz;
			num_alloc += cnt;
		}
		all_mem += total_sz;
		ll = snprint(buf, len,
			"***** %s changes/#allocs/mem: %d/%d/%ld\n",
			pname[pnum], num_c, num_alloc, total_sz);
		buf += ll;
		len -= ll;
		ll = 0;
		/*
		 * Print the changes
		 */
		for (ix = 1; ix <= num_callers; ix++) {
			int el;

			if (caller_pnum[ix] != pnum)
				continue;
			sz = caller_cur[ix].mem;
			cnt = caller_cur[ix].cnt;
			if ((cnt == 0) && (sz == 0))
				continue;

			el = ll;
			ll += snprint(buf+ll, len-ll, "%N/%d/%ld  ",
				caller_pc[ix], cnt, sz);
			if (len-ll <= 0)
				break;
			if (ll > 72) {
				buf += el;
				len -= el;
				ll -= el;
				buf[-1] = '\n';
			}
		}
		if (ll)
			buf[ll-1] = '\n';
		buf += ll;
		len -= ll;
	}
	len -= snprint(buf, len, "Total: %ld\n", all_mem);
	return blen - len;
}

static void
mallocdbg(Rune r)
{
	int n;

	if (caller_ix == nil)
		mallocdbginit(MAX_CALLERS);
	else {
		n = mallocsummary(mallocp, mallocpsz);
		if (r == 'U')
			mallocdbgclr();
		putstrn(mallocp, n);
	}
}

static void
auditreset(void)
{
	mallocdbginit(audit_callers);
}

static void
auditinit(void)
{
	debugkey('u', "malloc usage", mallocdbg, 0);
	debugkey('U', "malloc usage&clear", mallocdbg, 0);
}

static Chan*
auditattach(char *spec)
{
	return devattach(auditdevtab.dc, spec);
}

static int
auditwalk(Chan *c, char *name)
{
	return devwalk(c, name, auditdir, nelem(auditdir), devgen);
}

static void
auditstat(Chan *c, char *dp)
{
	devstat(c, dp, auditdir, nelem(auditdir), devgen);
}

static Chan*
auditopen(Chan *c, int omode)
{
	return devopen(c, omode, auditdir, nelem(auditdir), devgen);
}

static void
auditclose(Chan *c)
{
	if ((c->flag & COPEN) && !(c->qid.path & CHDIR))
		free(c->aux);
}

static long
auditread(Chan *c, void *buf, long n, ulong offset)
{
	if(n <= 0)
		return n;
	switch (c->qid.path & ~CHDIR) {
	case Qdir:
		return devdirread(c, buf, n, auditdir, nelem(auditdir), devgen);
	case Qmalloc:
		if (c->aux == nil) {
			c->aux = malloc(mallocpsz);
			if (c->aux == nil)
				error(Enomem);
			qlock(&memlock);
			mallocsummary(c->aux, mallocpsz);
			qunlock(&memlock);
		}
		return readstr(offset, buf, n, c->aux);
	case Qctl:
		if (caller_ix == nil)
			error(Enotenabled);
		qlock(&memlock);
		if (c->qid.path == Qctl && offset == 0) {
			mallocsummary(mallocp, mallocpsz);
			mallocdbgclr();
		}
		n = readstr(offset, buf, n, mallocp);
		qunlock(&memlock);
		return n;
	default:
		error(Ebadusefd);
	}
	return -1;		/* never reached */
}

static void
auditcmd(Chan *c, int nf, char **field)
{
	int ii;
	char *buf;
	int path = c->qid.path & ~CHDIR;

	switch (path) {
	default:
		error(Ebadusefd);
		break;
	case Qmalloc:
	case Qctl:
		if (caller_ix == nil) {
			if (nf <= 0 || strcmp(field[0], "init") != 0)
				error(Enotenabled);
			if (nf == 1) {
				mallocdbginit(MAX_CALLERS);
			} else if (nf == 2) {
				ii = strtol(field[1], nil, 0);
				if (ii <= 0)
					error(Ebadarg);
				mallocdbginit(ii);
			}
			else
				error(Ebadarg);
			qlock(&memlock);
			mallocsummary(mallocp, mallocpsz);
			qunlock(&memlock);
			return;
		}
		if (path == Qmalloc)
			buf = c->aux;
		else
			buf = mallocp;
		qlock(&memlock);
		for (ii = 0; ii < nf; ii++) {
			if (strcmp(field[ii], "update") == 0) {
				if (buf == nil) {
					buf = malloc(mallocpsz);
					if (buf == nil)
						error(Enomem);
					c->aux = buf;
				}
				mallocsummary(buf, mallocpsz);
			}
			else if (strcmp(field[ii], "clear") == 0)
				mallocdbgclr();
			else if (strcmp(field[ii], "reset") == 0)
				memset(caller_ref, 0, (num_callers+1)*sizeof *caller_ref);
			else {
				qunlock(&memlock);
				error(Ebadarg);
			}
		}
		qunlock(&memlock);
		return;
	case Qaudit:
		for (; nf > 0; nf--, field++)
			auditmemloc("Qaudit", (void *)strtoul(field[0], 0, 16));
		return;
	}
}

static long
auditwrite(Chan *c, void *va, long n, ulong offset)
{
	int nl, nf;
	char *line[6];
	char *field[4];
	char buf[100];
	int ii;

	USED(offset);
	switch (c->qid.path & ~CHDIR) {
	case Qctl:
	case Qmalloc:
	case Qaudit:
		if (n > sizeof buf-1)
			n = sizeof buf-1;
		memmove(buf, va, n);
		buf[n] = 0;
		nl = parsefields(buf, line, nelem(line), "\n");
		for (ii = 0; ii < nl; ii++) {
			nf = parsefields(line[ii], field, nelem(field), " \t");
			auditcmd(c, nf, field);
		}
		return n;
	default:
		error(Ebadusefd);
	}
	return n;
}

Dev auditdevtab = {
	'L',
	"audit",

	auditreset,
	auditinit,
	auditattach,
	devdetach,
	devclone,
	auditwalk,
	auditstat,
	auditopen,
	devcreate,
	auditclose,
	auditread,
	devbread,
	auditwrite,
	devbwrite,
	devremove,
	devwstat,
};
