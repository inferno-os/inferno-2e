#include	<lib9.h>
#include	<bio.h>
#include	<bootexec.h>
#include	<mach.h>

/*
 *	All a.out header types.  The dummy entry allows canonical
 *	processing of the union as a sequence of longs
 */

typedef struct {
	union{
		Exec hdr;		/* in a.out.h */
		struct mipsexec v;	/* Hobbit uses this header too */
		struct mips4kexec v4;
		struct sparcexec k;
		struct nextexec n;
		struct i960exec i;
	} e;
	long dummy;		/* padding to ensure extra long */
} ExecHdr;

static	void	i960boot(Fhdr *, ExecHdr *);
static	void	nextboot(Fhdr *, ExecHdr *);
static	void	sparcboot(Fhdr *, ExecHdr *);
static	void	mipsboot(Fhdr *, ExecHdr *);
static	void	mips4kboot(Fhdr *, ExecHdr *);
static	void	common(Fhdr *, ExecHdr *);
static	void	adotout(Fhdr *, ExecHdr *);
static	void	setsym(Fhdr *, long, long, long, long);
static	void	setdata(Fhdr *, long, long, long, long);
static	void	settext(Fhdr *, long, long, long, long);
static	void	hswal(long *, int, long (*) (long));
static	long	_round(long, long);

/*
 *	definition of per-executable file type structures
 */

typedef struct Exectable{
	long	magic;			/* big-endian magic number of file */
	char	*name;			/* executable identifier */
	int	type;			/* Internal code */
	Mach	*mach;			/* Per-machine data */
	ulong	hsize;			/* header size */
	long	(*swal)(long);		/* beswal or leswal */
	void	(*hparse)(Fhdr *, ExecHdr *);
} ExecTable;

extern	Mach	msparc;

ExecTable exectab[] =
{
	{ V_MAGIC,			/* Mips v.out */
		"mips plan 9 executable",
		FMIPS,
		0,
		sizeof(Exec),
		beswal,
		adotout },
	{ 0x160<<16,			/* Mips boot image */
		"mips plan 9 boot image",
		FMIPSB,
		0,
		sizeof(struct mipsexec),
		beswal,
		mipsboot },
	{ (0x160<<16)|3,		/* Mips boot image */
		"mips 4k plan 9 boot image",
		FMIPSB,
		0,
		sizeof(struct mips4kexec),
		beswal,
		mips4kboot },
	{ K_MAGIC,			/* Sparc k.out */
		"sparc plan 9 executable",
		FSPARC,
		&msparc,
		sizeof(Exec),
		beswal,
		adotout },
	{ 0x01030107, 			/* Sparc boot image */
		"sparc plan 9 boot image",
		FSPARCB,
		&msparc,
		sizeof(struct sparcexec),
		beswal,
		sparcboot },
	{ A_MAGIC,			/* 68020 2.out & boot image */
		"68020 plan 9 executable",
		F68020,
		0,
		sizeof(Exec),
		beswal,
		common },
	{ 0xFEEDFACE,			/* Next boot image */
		"next plan 9 boot image",
		FNEXTB,
		0,
		sizeof(struct nextexec),
		beswal,
		nextboot },
	{ I_MAGIC,			/* I386 8.out & boot image */
		"386 plan 9 executable",
		FI386,
		0,
		sizeof(Exec),
		beswal,
		common },
	{ J_MAGIC,			/* I960 6.out (big-endian) */
		"960 plan 9 executable",
		FI960,
		0,
		sizeof(Exec),
		beswal,
		adotout },
	{ 0x61010200, 			/* I960 boot image (little endian) */
		"960 plan 9 boot image",
		FI960B,
		0,
		sizeof(struct i960exec),
		leswal,
		i960boot },
	{ X_MAGIC,			/* 3210 x.out */
		"3210 plan 9 executable",
		F3210,
		0,
		sizeof(Exec),
		beswal,
		adotout },
	{ 0 },
};

Mach	*mach = &msparc;		/* Global current machine table */

int
crackhdr(int fd, Fhdr *fp)
{
	ExecTable *mp;
	ExecHdr d;
	int nb, magic;

	fp->type = FNONE;
	if ((nb = read(fd, (char *)&d.e, sizeof(d.e))) <= 0)
		return 0;
	fp->magic = magic = beswal(d.e.hdr.magic);	/* big-endian */
	for (mp = exectab; mp->magic; mp++) {
		if (mp->magic == magic && nb >= mp->hsize) {
			hswal((long *) &d, sizeof(d.e)/sizeof(long), mp->swal);
			fp->type = mp->type;
			fp->name = mp->name;
			fp->hdrsz = mp->hsize;		/* zero on bootables */
			mach = mp->mach;
			mp->hparse(fp, &d);
			seek(fd, mp->hsize, 0);		/* seek to end of header */
			return 1;
		}
	}
	return 0;
}
/*
 * Convert header to canonical form
 */
static void
hswal(long *lp, int n, long (*swap) (long))
{
	while (n--) {
		*lp = (*swap) (*lp);
		lp++;
	}
}
/*
 *	Crack a normal a.out-type header
 */
static void
adotout(Fhdr *fp, ExecHdr *hp)
{
	long pgsize = mach->pgsize;

	settext(fp, hp->e.hdr.entry, pgsize+sizeof(Exec),
			hp->e.hdr.text, sizeof(Exec));
	setdata(fp, _round(pgsize+fp->txtsz+sizeof(Exec), pgsize),
		hp->e.hdr.data, fp->txtsz+sizeof(Exec), hp->e.hdr.bss);
	setsym(fp, hp->e.hdr.syms, hp->e.hdr.spsz, hp->e.hdr.pcsz, fp->datoff+fp->datsz);
}

/*
 *	68020 2.out and 68020 bootable images
 *	386I 8.out and 386I bootable images
 *
 */
static void
common(Fhdr *fp, ExecHdr *hp)
{
	long kbase = mach->kbase;

	adotout(fp, hp);
	if (fp->entry & kbase) {		/* Boot image */
		switch(fp->type) {
		case F68020:
			fp->type = F68020B;
			fp->name = "68020 plan 9 boot image";
			fp->hdrsz = 0;		/* header stripped */
			break;
		case FI386:
			fp->type = FI386B;
			fp->txtaddr = sizeof(Exec);
			fp->name = "386 plan 9 boot image";
			fp->hdrsz = 0;		/* header stripped */
			fp->dataddr = fp->txtaddr+fp->txtsz;
			break;
		default:
			break;
		}
		fp->txtaddr |= kbase;
		fp->entry |= kbase;
		fp->dataddr |= kbase;
	}
}

/*
 *	mips bootable image.
 */
static void
mipsboot(Fhdr *fp, ExecHdr *hp)
{
	switch(hp->e.hdr.magic) {
	default:
	case 0407:	/* some kind of mips */
		fp->type = FMIPSB;
		settext(fp, hp->e.v.mentry, hp->e.v.text_start, hp->e.v.tsize,
					sizeof(struct mipsexec)+4);
		setdata(fp, hp->e.v.data_start, hp->e.v.dsize,
				fp->txtoff+hp->e.v.tsize, hp->e.v.bsize);
		break;
	case 0413:	/* some kind of mips */
		fp->type = FMIPSB;
		settext(fp, hp->e.v.mentry, hp->e.v.text_start, hp->e.v.tsize, 0);
		setdata(fp, hp->e.v.data_start, hp->e.v.dsize, hp->e.v.tsize,
					hp->e.v.bsize);
		break;
	}
	setsym(fp, hp->e.v.nsyms, 0, hp->e.v.u0.pcsize, hp->e.v.symptr);
	fp->hdrsz = 0;		/* header stripped */
}

/*
 *	mips4k bootable image.
 */
static void
mips4kboot(Fhdr *fp, ExecHdr *hp)
{
	print("mips4kboot not implemented\n");
	exit(1);
}

/*
 *	sparc bootable image
 */
static void
sparcboot(Fhdr *fp, ExecHdr *hp)
{
	fp->type = FSPARCB;
	settext(fp, hp->e.k.sentry, hp->e.k.sentry, hp->e.k.stext,
					sizeof(struct sparcexec));
	setdata(fp, hp->e.k.sentry+hp->e.k.stext, hp->e.k.sdata,
					fp->txtoff+hp->e.k.stext, hp->e.k.sbss);
	setsym(fp, hp->e.k.ssyms, 0, hp->e.k.sdrsize, fp->datoff+hp->e.k.sdata);
	fp->hdrsz = 0;		/* header stripped */
}

/*
 *	next bootable image
 */
static void
nextboot(Fhdr *fp, ExecHdr *hp)
{
	print("nextboot not implemented\n");
	exit(1);
}

/*
 *	I960 bootable image
 */
static void
i960boot(Fhdr *fp, ExecHdr *hp)
{
	print("i960boot not implemented\n");
	exit(1);
}


static void
settext(Fhdr *fp, long e, long a, long s, long off)
{
	fp->txtaddr = a;
	fp->entry = e;
	fp->txtsz = s;
	fp->txtoff = off;
}
static void
setdata(Fhdr *fp, long a, long s, long off, long bss)
{
	fp->dataddr = a;
	fp->datsz = s;
	fp->datoff = off;
	fp->bsssz = bss;
}
static void
setsym(Fhdr *fp, long sy, long sppc, long lnpc, long symoff)
{
	fp->symsz = sy;
	fp->symoff = symoff;
	fp->sppcsz = sppc;
	fp->sppcoff = fp->symoff+fp->symsz;
	fp->lnpcsz = lnpc;
	fp->lnpcoff = fp->sppcoff+fp->sppcsz;
}


static long
_round(long a, long b)
{
	long w;

	w = (a/b)*b;
	if (a!=w)
		w += b;
	return(w);
}
