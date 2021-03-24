#include	"u.h"
#include	"ureg.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"bootparam.h"

enum {
	Qctl,
	Qmap,
	Qdata,
	Qprofile,
};

static Dirtab nprofdir[]=
{
	[ Qctl ]	"npctl",	{Qctl},		0,	0666,
	[ Qmap ]	"npmap",	{Qmap},		0,	0666,
	[ Qdata ]	"npdata",	{Qdata},	0,	0444,
	[ Qprofile ]	"nprof",	{Qprofile},	0,	0444,
};

enum {
	WSZ = 4,
	BuckLine = 18,
	SymLine = 24,
};
/*
 * This profiler requires a platform-specific installprof() that hooks
 *  into a suitable clock interrupt.  The best kind of clock interrupt
 *  is one that is separate from the scheduling clock AND interrupts
 *  even when splhi().
 * Hooking into the scheduler interrupt is barely acceptable.
 *
 * Another required platform-specific function is wasbusy().  The ideal
 *  wasbusy() can distinguish between idle splhi (e.g in runproc()) and
 *  running a real interrupt handler, and knows the difference between
 *  running the GC at idle vs. between threads.
 */

extern void *realloc(void *, ulong);
extern struct Dev nprofdevtab;
extern char etext[];

static Lock nprof_lock;
static int nprof_opens;
static int nprof_run;
static int nprof_enb;
static int nprof_pri = Nrq;
static int nprof_reg;
static int nprof_cnt;
static int nprof_bsz;
static long *nprof_hits;
static ulong nprof_start;
static ulong nprof_end;
static long nprof_tick;

static int nprof_mapsz;
static ulong *nprof_addr;

typedef struct Profcmd Profcmd;

struct Profcmd {
	int n;
	char b[256];
};

static int nprof_lookup(ulong);
static void nprof_filesz(void);

static void
nprof_off(int nosyms)
{
	nprof_run = 0;
	nprof_enb = 0;
	nprof_cnt = 0;
	if (nosyms) {
		free(nprof_addr);
		nprof_addr = nil;
		nprof_mapsz = 0;
	}
	free(nprof_hits);
	nprof_hits = nil;
	nprof_tick = 0;
	nprof_filesz();
}

/*
 * Entry from profiling clock interrupt.  Count 't' ticks.
 */
static void
nprof(Ureg *ur, int t)
{
	ulong pc;
	int ix;

	if (!nprof_run)
		return;
	nprof_tick += t;
	if (!wasbusy(nprof_pri))
		return;

	if (nprof_reg)
		pc = ur->link;
	else
		pc = ur->pc;
	if (nprof_mapsz) {
		ix = nprof_lookup(pc) - 1;
		if (ix < 0)
			return;

		/* DEBUG: */
		if (
			(pc < nprof_addr[ix])
			||
			((ix < nprof_cnt-1) && (pc >= nprof_addr[ix+1]))
		)
		{
			static errs;

			if (errs++ % 100 == 0)
				print("prof: errs=%d, pc=%lux, bucket=%lux-%lux\n",
					errs, pc,
					nprof_addr[ix], nprof_addr[ix+1]);
			return;
		}
	} else {
		if ((pc < nprof_start) || (pc >= nprof_end))
			return;
		ix = (pc - nprof_start) / nprof_bsz;
	}
	nprof_hits[ix] += t;
}

/*
 * Print profiling results.
 */
char	*nprof_lbl_pc  = "   count  %%     PC   |";
char	*nprof_lbl_sym = "   count  %%     name   |";

#define NCOLS	3

static void
prt_prof(int allf)
{
	int	ix, rix;
	long	cnt, match_ticks, thresh, total;
	int	num_match, row, col, nrows;
	int	col_prt[NCOLS];

	if (nprof_cnt <= 0)
		return;
	/*
	 * Count and print total number of bucket hits
	 */
	total = 0;
	for (ix = 0; ix < nprof_cnt; ix++)
		total += nprof_hits[ix];
	if (total == 0)
		return;
	print("Profile is %ld %% of total time\n", total * 100 / nprof_tick);
	if (!nprof_mapsz)
		print("PC bucket size is #%x\n", nprof_bsz);
	/*
	 * Threshold is either 1 (if "allf") or 1% of total
	 */
	if (allf)
		thresh = 1;
	else
		thresh = (total + 99) / 100;	/* a good 1% */
	if (thresh == 0)
		thresh = 1;
	/*
	 * Count the number of samples meeting the threshold
	 */
	match_ticks = 0;
	num_match = 0;
	for (ix = 0; ix < nprof_cnt; ix++) {
		if ((cnt = nprof_hits[ix]) >= thresh) {
			match_ticks += cnt;
			num_match++;
		}
	}
	if (thresh > 1)
	      print("Portion displayed (>=%ld hits) is %ld %% of total time\n",
			thresh, match_ticks * 100 / nprof_tick);
	/*
	 * Divide into NCOLS columns.
	 * Figure the bucket indices for the first row for each column
	 */
	for (col = 0; col < NCOLS; col++) {
		col_prt[col] = nprof_cnt;
		print( nprof_mapsz ? nprof_lbl_sym : nprof_lbl_pc );
	}
	print("\n");

	nrows = (num_match + NCOLS - 1) / NCOLS;
	row = 0;
	col = 0;
	rix = -1;
	for (ix = 0; ix < nprof_cnt; ix++) {
		if (nprof_hits[ix] < thresh)
			continue;
		if (++rix != row)
			continue;
		col_prt[col] = ix;	/* first bucket for 'col' */
		row += nrows;
		col++;
	}

	for (row = 0; row < nrows; row++) {
		for (col = 0; col < NCOLS; col++) {
			for (ix = col_prt[col]; ix < nprof_cnt; ix++) {
				if ((cnt = nprof_hits[ix]) < thresh)
					continue;
				print("%8ld%3ld ", cnt,
					(cnt * 100 + total/2) / total);
				if (nprof_mapsz)
					print("%10.10N |", nprof_addr[ix]);
				else
					print("%-lu8.8x |", nprof_start + ix*nprof_bsz);
				col_prt[col] = ix+1;
				break;
			}
		}
		print("\n");
	}
	print("   -----       -----\n");
	print("%8ld    %8d\n", match_ticks, num_match);
}

/*
 * Set profiling parameters for constant-sized buckets and turn on profiling.
 * Turn it off if parameters are not acceptable.
 */
static int
set_prof(long addr, long size, int cnt, int enb)
{
	nprof_off(1);
	if ((enb == 0) || (cnt <= 0))
		return 0;
	nprof_start = addr;
	nprof_end = addr + size;
	nprof_bsz = (size + cnt - 1) / cnt;
	if (nprof_bsz <= 0)
		nprof_bsz = sizeof (long);
#define ROUNDUP(a,b) ((((a)+(b)-1)/(b))*(b))
	nprof_bsz = ROUNDUP(nprof_bsz,sizeof (long));
	cnt = (size + nprof_bsz - 1) / nprof_bsz;
	nprof_hits = malloc(cnt * WSZ);
	if (nprof_hits == nil)
		error(Enomem);
	nprof_cnt = cnt;
	nprof_filesz();
	installprof(nprof);

	print("profile: start %lux, end %lux, num %d\n", nprof_start, nprof_end, nprof_cnt);
	nprof_enb = enb;
	return 1;
}

static long
getval(char *f, int base)
{
	char *r;
	long val;

	val = strtol(f, &r, base);
	if (r && *r)
		error(Ebadarg);
	return val;
}

static void
startprof(int nf, char *f[])
{
	long cnt;
	long sa;
	long len;


	if (nf >= 1)
		cnt = getval(f[0], 0);
	else
		cnt = 1024;
	if (nf >= 2)
		sa = getval(f[1], 16);
	else
		sa = KTZERO;
	if (nf >= 3)
		len = getval(f[2], 16);
	else
		len = (ulong)etext - sa;
	set_prof(sa, len, cnt, 1);
}

static void
clrprf(void)
{
	if (nprof_enb) {
		nprof_tick = 0;
		memset(nprof_hits, 0, nprof_cnt * WSZ);
	} else
		set_prof(KTZERO, (ulong)etext - KTZERO, 4096, 1);
}

static void
prtprf(Rune r)
{
	nprof_run = 0;
	prt_prof(r == 'H');
	if (r == 'B')
		clrprf();
	nprof_run = nprof_enb && !nprof_opens;
}

/*
 * Returns index of first slot whose value is > 'addr'
 * Can return 'nprof_cnt', i.e. one beyond last entry.
 */
static int
nprof_lookup(ulong addr)
{
	int hi = nprof_cnt, lo = 0;

	while (hi > lo) {
		int ii;

		ii = (hi + lo)>>1;
		if (nprof_addr[ii] > addr)
			hi = ii;
		else
			lo = ii + 1;
	}
	return lo;
}

static int
nprof_allocsyms(int cnt)
{
	void *m;

	if (!cnt)
		return 0;

	if (!(m = realloc(nprof_hits, cnt * WSZ)))
		return 0;
	nprof_hits = m;

	if (!(m = realloc(nprof_addr, cnt * sizeof *nprof_addr)))
		return 0;
	nprof_addr = m;

	nprof_mapsz = cnt;
	return 1;
}

static int
nprof_addmap(ulong addr)
{
	int ix;

	lock(&nprof_lock);
	if (nprof_cnt >= nprof_mapsz) {
		if (!nprof_allocsyms(nprof_cnt + 300)) {
			unlock(&nprof_lock);
			return 0;
		}
	}

	ix = nprof_lookup(addr);
	if (ix < nprof_cnt) {
		if ((ix > 0) && (addr == nprof_addr[ix-1])) {
			unlock(&nprof_lock);
			return 1;
		}
		memmove(&nprof_addr[ix+1], &nprof_addr[ix], (nprof_cnt-ix)*sizeof *nprof_addr);
		memmove(&nprof_hits[ix+1], &nprof_hits[ix], (nprof_cnt-ix)*WSZ);
	}
	nprof_addr[ix] = addr;
	nprof_hits[ix] = 0;
	nprof_cnt++;
	nprofdir[Qdata].length += WSZ;
	nprofdir[Qmap].length += SymLine;
	nprofdir[Qprofile].length += SymLine;
	unlock(&nprof_lock);
	return 1;
}

static void
nprof_filesz()
{
	nprofdir[Qdata].length = WSZ*nprof_cnt;
	if (nprof_mapsz) {
		nprofdir[Qmap].length = SymLine*nprof_cnt;
		nprofdir[Qprofile].length = SymLine*nprof_cnt;
	} else {
		nprofdir[Qmap].length = 0;
		nprofdir[Qprofile].length = BuckLine*nprof_cnt;
	}
}

static void
nprofinit(void)
{
	debugkey('b', "profile", prtprf, 0);
	debugkey('B', "profile start/clr", prtprf, 0);
	debugkey('H', "profile high res", prtprf, 0);
}

static Chan*
nprofattach(char *spec)
{
	return devattach(nprofdevtab.dc, spec);
}

static int
nprofwalk(Chan *c, char *name)
{
	return devwalk(c, name, nprofdir, nelem(nprofdir), devgen);
}

static void
nprofstat(Chan *c, char *dp)
{
	devstat(c, dp, nprofdir, nelem(nprofdir), devgen);
}

static Chan*
nprofopen(Chan *c, int omode)
{
	int om;

	switch (c->qid.path) {
	case Qmap:
	case Qctl:
		om = omode & 7;
		if (om != ORDWR && om != OWRITE)
			break;
		if ((c->aux = malloc(sizeof (Profcmd))) == nil)
			error(Enomem);
		if (waserror()) {
			free(c->aux);
			c->aux = nil;
			nexterror();
		}
		c = devopen(c, omode, nprofdir, nelem(nprofdir), devgen);
		poperror();
		lock(&nprof_lock);
		nprof_opens++;
		nprof_run = 0;
		unlock(&nprof_lock);
		return c;
	}
	c = devopen(c, omode, nprofdir, nelem(nprofdir), devgen);
	return c;
}

static void
nprofclose(Chan *c)
{
	if (c->flag & COPEN) {
		if (c->aux) {
			free(c->aux);
			c->aux = nil;
			lock(&nprof_lock);
			if (--nprof_opens == 0) {
				if (c->qid.path == Qmap) {
					nprof_allocsyms(nprof_mapsz);
				}
				nprof_run = nprof_enb;
			}
			unlock(&nprof_lock);
		}
	}
}

static long
readwords(ulong offset, uchar *va, long n, long *mem, int max)
{
	int sz = max*WSZ;
	uchar *eva;
	ulong w;

	if (offset & (WSZ-1))
		error(Ebadarg);
	if (offset >= sz)
		return 0;
	if (offset + n > sz)
		n = sz - offset;
	n &= ~(WSZ-1);
	eva = va + n;
	mem += offset / WSZ;
	while (va < eva) {
		w = *mem++;
		*va++ = w>>24;
		*va++ = w>>16;
		*va++ = w>>8;
		*va++ = w;
	}
	return n;
}

static long
nprofread(Chan *c, void *va, long n, ulong offset)
{
	char buf[PRINTSIZE];
	int r;
	int ix;
	int bsz, lsz;

	if(n <= 0)
		return n;
	switch (c->qid.path) {
	case Qctl:
		r = 0;
		if (nprof_reg)
			r += snprint(buf + r, sizeof buf - r, "link\n");
		else
			r += snprint(buf + r, sizeof buf - r, "pc\n");
		if (nprof_pri > Nrq)
			r += snprint(buf + r, sizeof buf - r, "anypri\n");
		else
			r += snprint(buf + r, sizeof buf - r, "pri %d\n", nprof_pri);
		if (nprof_enb) {
			if (nprof_mapsz)
				r += snprint(buf + r, sizeof buf - r,
					"syms=%d\n", nprof_cnt);
			else {
				r += snprint(buf + r, sizeof buf -r,
					"start %lux, end %lux, bsz %ux\n",
						nprof_start, nprof_end, nprof_bsz);
			}
		}
		return readstr(offset, va, r, buf);
	case Qprofile:
	case Qmap:
		if (!nprof_enb)
			return 0;

		if (nprof_mapsz)
			lsz = SymLine;
		else
			lsz = BuckLine;
		ix = offset/lsz;
		offset -= lsz*ix;
		bsz = PRINTSIZE - UTFmax;
		bsz -= bsz % lsz;

		for (r = 0; r < bsz; ix++) {
			if (ix >= nprof_cnt)
				break;
			if (c->qid.path == Qprofile) {
				if (nprof_mapsz)
					r += snprint(buf + r, sizeof buf - r, "%14.14N %8ld\n",
						nprof_addr[ix], nprof_hits[ix]);
				else
					r += snprint(buf + r, sizeof buf - r, "%8.8lux %8ld\n",
						nprof_start + ix * nprof_bsz, nprof_hits[ix]);
			} else if (nprof_mapsz)
				r += snprint(buf + r, sizeof buf - r, "%14.14N %8lux\n",
					nprof_addr[ix], nprof_addr[ix]);
			else
				return 0;
		}
		buf[r] = 0;
		return readstr(offset, va, n, buf);
	case Qdata:
		return readwords(offset, va, n, nprof_hits, nprof_cnt);
	case CHDIR:
		return devdirread(c, va, n, nprofdir, nelem(nprofdir), devgen);
	default:
		error(Ebadusefd);
	}
	return -1;
}

static void
nprofcmd(int path, char *cmd)
{
	char *fields[8], **f = fields;
	int nf;

	nf = parsefields(cmd, fields, nelem(fields), " \t");
	if (nf >= nelem(fields))
		error(Etoobig);

	switch(path){
	case Qctl:
		if (nf < 1) {
			startprof(0, 0);
			return;
		}
		while (nf >= 1) {
			if (strcmp(f[0], "pc") == 0)
				nprof_reg = 0;
			else if (strcmp(f[0], "link") == 0)
				nprof_reg = 1;
			else if (strcmp(f[0], "print") == 0)
				prt_prof(0);
			else if (strcmp(f[0], "startclr") == 0)
				clrprf();
			else if (strcmp(f[0], "start") == 0) {
				startprof(nf-1, f+1);
				return;
			} else if (strcmp(f[0], "stop") == 0)
				nprof_off(0);
			else if (strcmp(f[0], "syms") == 0) {
				nprof_off(1);
				if (
					!nprof_addmap(KZERO)
					||
					!nprof_addmap((ulong)etext)
				)
					error(Enomem);
			} else if (strcmp(f[0], "pri") == 0) {
				if (nf < 2)
					error(Ebadarg);
				nprof_pri = getval(f[1], 0);
				nf--;
				f++;
			} else if (strcmp(f[0], "anypri") == 0)
				nprof_pri = Nrq;
			nf--;
			f++;
		}
		break;
	case Qmap:
		if ((nf < 1) || !nprof_mapsz)
			error(Ebadarg);

		if (!nprof_addmap(getval(fields[0], 16)))
			error(Enomem);
		if (!nprof_enb) {
			installprof(nprof);
			nprof_enb = 1;	/* Will start profiling when file is closed */
		}
		break;
	}
}

static long
nprofwrite(Chan *c, void *va, long n, ulong)
{
	char *ua = va;
	Profcmd *pf;
	int r, eb;

	switch(c->qid.path) {
	case Qctl:
	case Qmap:
		pf = c->aux;
		r = 0;
nxtline:	if (r < n) {
			eb = n - r;
			if (eb > sizeof pf->b)
				eb = sizeof pf->b;
			while (pf->n < eb) {
				if ((pf->b[pf->n++] = ua[r++]) == '\n') {
					pf->b[pf->n - 1] = 0;
					pf->n = 0;
					nprofcmd(c->qid.path, pf->b);
					goto nxtline;
				}
			}
			if (pf->n >= sizeof pf->b) {
				pf->n = 0;
				error(Etoobig);
			}
		}
		return r;
	default:
		error(Ebadusefd);
		break;
	}
	return -1;
}

Dev nprofdevtab = {
	'n',
	"nprof",

	devreset,
	nprofinit,
	nprofattach,
	devdetach,
	devclone,
	nprofwalk,
	nprofstat,
	nprofopen,
	devcreate,
	nprofclose,
	nprofread,
	devbread,
	nprofwrite,
	devbwrite,
	devremove,
	devwstat,
};
