#include	"u.h"
#include	"ureg.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

enum{
	Qdir,
	Qctl,
	Qtrans,
};

enum { MaxSymsz=32, };
char ksymlinefmt[] = "%8lux %*.*s\n";
static int max_symsz = 1;

static Dirtab ksymdir[]=
{
	{ "ctl",	{Qctl},		0,	0666 },
	{ "trans",	{Qtrans},	0,	0666 },
};

extern void *realloc(void *, ulong);
extern struct Dev ksymdevtab;
extern char etext[];

typedef struct Ksym Ksym;

struct Ksym {
	ulong	val;
	int	map;
};

static int ksym_opens;

static Ksym *ksym_map;		/* space for value -> name map */
static int ksym_mapsz;		/* max number of mappings */
static int ksym_cnt;		/* number of valid mappings */

static char *ksym_name;		/* space for names */
static int ksym_namesz;		/* size of space for names */
static int ksym_nextname;	/* used portion of space for names */

static struct ksymcmd {
	int n;
	char b[256];
};

static int ksym_lookup(ulong);

static void
ksym_free(void)
{
	free(ksym_map);
	max_symsz = 1;
	ksym_map = nil;
	ksym_mapsz = 0;
	ksym_cnt = 0;

	free(ksym_name);
	ksym_name = nil;
	ksym_namesz = 0;
	ksym_nextname = 0;
}

static ulong
getval(char *f, int base)
{
	char *r;
	ulong val;

	val = strtoul(f, &r, base);
	if (r && *r)
		error(Ebadarg);
	return val;
}

/*
 * Returns index of first slot whose value is > 'val'
 * Can return 'ksym_cnt', i.e. one beyond last entry.
 */
static int
ksym_lookup(ulong val)
{
	int ii;
	int hi = ksym_cnt, lo = 0;

	while (hi > lo) {
		ii = (hi + lo)>>1;
		if (ksym_map[ii].val > val)
			hi = ii;
		else
			lo = ii + 1;
	}
	return lo;
}

static int
ksym_allocmap(int cnt)
{
	void *m;

	if (!cnt)
		return 0;

	if (!(m = realloc(ksym_map, cnt * sizeof *ksym_map)))
		return 0;
	ksym_map = m;
	ksym_mapsz = cnt;
	return 1;
}

static int
ksym_allocnames(int sz)
{
	void *m;

	if (!sz)
		return 0;

	if (!(m = realloc(ksym_name, sz)))
		return 0;
	ksym_name = m;
	ksym_namesz = sz;
	return 1;
}

static int
ksym_add(char *name, ulong val)
{
	int ii;
	int ix;

	if (ksym_cnt >= ksym_mapsz) {
		if (!ksym_allocmap(ksym_cnt + 300))
			return 0;
	}

	ii = strlen(name);
	if (ii > MaxSymsz)
		ii = MaxSymsz;
	if (ksym_nextname+ii+1 >= ksym_namesz)
		if (!ksym_allocnames(ksym_namesz+ii+(ksym_mapsz-ksym_cnt)*8))
			return 0;
	ix = ksym_nextname;
	memmove(&ksym_name[ix], name, ii);
	ksym_name[ix + ii] = 0;
	ksym_nextname += ii+1;
	if (ii > max_symsz)
		max_symsz = ii;
	ii = ksym_lookup(val);
	if (ii < ksym_cnt) {
		if ((ii > 0) && (val == ksym_map[ii-1].val)) {
			ksym_nextname = ix;	/* keep only one name */
			return 1;
		}
		memmove(&ksym_map[ii+1], &ksym_map[ii], (ksym_cnt-ii)*sizeof *ksym_map);
	}
	ksym_map[ii].val = val;
	ksym_map[ii].map = ix;
	ksym_cnt++;
	return 1;
}

static int
Nconv(va_list *arg, Fconv *fp)
{
	ulong val;
	static char sbuf[MaxSymsz+13];
	int ii;
	int found = 0;
	ulong d;

	SET(ii);
	val = va_arg(*arg, ulong);
	if (
		(ksym_map != nil)
		&&
		((ii = ksym_lookup((ulong)val) - 1) >= 0)
	) {
		d = val - ksym_map[ii].val;
		if (d == 0) {
			strconv(&ksym_name[ksym_map[ii].map], fp);
			return 0;
		} else if (d < 0x200000) {
			ii = snprint(sbuf, sizeof sbuf - 1, "%s+%lux",
				&ksym_name[ksym_map[ii].map], d);
			found = 1;
		}
	}
	if (!found)
		ii = snprint(sbuf, sizeof sbuf - 1, "%lux", val);
	sbuf[ii] = 0;
	strconv(sbuf, fp);
	return 0;
}

static void
ksyminit(void)
{
	fmtinstall('N', Nconv);
}

static Chan*
ksymattach(char *spec)
{
	return devattach(ksymdevtab.dc, spec);
}

static int
ksymgen(Chan *c, Dirtab *tab, int ntab, int i, Dir *dp)
{
	switch (tab[i].qid.path) {
	case Qctl:
		tab[i].length = ksym_cnt * (max_symsz+10);
		break;
	}
	return devgen(c, tab, ntab, i, dp);
}

static int
ksymwalk(Chan *c, char *name)
{
	return devwalk(c, name, ksymdir, nelem(ksymdir), ksymgen);
}

static void
ksymstat(Chan *c, char *dp)
{
	devstat(c, dp, ksymdir, nelem(ksymdir), ksymgen);
}

static Chan*
ksymopen(Chan *c, int omode)
{
	int path = c->qid.path & ~CHDIR;

	if (waserror()) {
		free(c->aux);
		c->aux = nil;
		nexterror();
	}
	switch (path) {
	case Qctl:
		if ((c->aux = malloc(sizeof (struct ksymcmd))) == nil)
			error(Enomem);
		break;
	}
	c = devopen(c, omode, ksymdir, nelem(ksymdir), devgen);
	ksym_opens++;
	poperror();
	return c;
}

static void
ksymclose(Chan *c)
{
	if (c->flag & COPEN) {
		free(c->aux);
		c->aux = nil;
		if (--ksym_opens == 0) {
			ksym_allocmap(ksym_cnt);
			ksym_allocnames(ksym_nextname);
		}
	}
}

static long
ksymread(Chan *c, void *va, long n, ulong offset)
{
	char buf[PRINTSIZE], *bp;
	int r, ret, ix;
	int symline, maxsyms, buflim;

	if(n <= 0)
		return n;
	switch(c->qid.path & ~CHDIR){
	case Qdir:
		return devdirread(c, va, n, ksymdir, nelem(ksymdir), ksymgen);
	case Qctl:
		if (!ksym_mapsz)
			return 0;
		symline = max_symsz + 10;
		maxsyms=(sizeof buf-3)/symline;
		buflim = maxsyms*symline;
		ix = offset/symline;
		offset -= symline*ix;
		ret = 0;
		bp = va;
		while (n > 0) {
			for (r = 0; r < buflim; ix++) {
				if (ix >= ksym_cnt)
					break;
				r += snprint(buf + r, sizeof buf - r, ksymlinefmt,
					ksym_map[ix].val, max_symsz, max_symsz,
					&ksym_name[ksym_map[ix].map]);
			}
			buf[r] = 0;
			r = readstr(offset, bp, n, buf);
			if (r <= 0)
				break;
			bp += r;
			n -= r;
			ret += r;
			offset = 0;
		}
		return ret;
	default:
		error(Ebadusefd);
	}
	return -1;		/* never reached */
}

static void
ksymcmd(char *cmd, int seek0)
{
	char *fields[8], **f = fields;
	int nf;

	nf = parsefields(cmd, fields, nelem(fields), " \t");
	if (nf >= nelem(fields))
		error(Etoobig);

	if (nf < 1)
		return;
	while (nf >= 1) {
		if (strcmp(f[0], "clear") == 0)
			ksym_free();
		else if (strcmp(f[0], "kernel") == 0) {
			if (!ksym_add("_kzero", KZERO) || !ksym_add("etext", (ulong)etext))
				error(Enomem);
		} else
			break;
		nf--;
	}
	if (nf == 2) {
		if (seek0)
			ksym_free();
		if (!ksym_add(fields[1], getval(fields[0], 16)))
			error(Enomem);
	} else if (nf == 0)
		return;
	else
		error(Ebadarg);
}

static long
ksymwrite(Chan *c, void *va, long n, ulong offset)
{
	char *ua = va;
	struct ksymcmd *pf;
	int wc;
	int clr;
	int e;

	if (c->qid.path != Qctl)
		error(Ebadusefd);

	pf = c->aux;
	wc = 0;
	clr = (offset == 0);
	while ((e = n - wc) > 0) {
		int jj;

		if (e > sizeof pf->b)
			e = sizeof pf->b;
		for (jj = pf->n; jj < e; jj++) {
			if ((pf->b[jj] = ua[wc++]) == '\n')
				break;
		}
		if (jj < e) {
			pf->b[jj] = 0;	/* end string whack the newline */
			pf->n = 0;
			ksymcmd(pf->b, clr);
			clr = 0;
		} else if (e >= sizeof pf->b) {
			error(Etoobig);
		} else {
			pf->n = e;
			break;
		}
	}
	return wc;
}

Dev ksymdevtab = {
	'N',
	"ksym",

	devreset,
	ksyminit,
	ksymattach,
	devdetach,
	devclone,
	ksymwalk,
	ksymstat,
	ksymopen,
	devcreate,
	ksymclose,
	ksymread,
	devbread,
	ksymwrite,
	devbwrite,
	devremove,
	devwstat,
};
