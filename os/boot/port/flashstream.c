#include <lib9.h>
#include <styx.h>
#include "dat.h"
#include "fns.h"
#include "flash.h"
#include "flashptab.h"
#include "flashstream.h"
#include "bpi.h"
#include "screen.h"
#include "error.h"

extern FlashMap flashmap[];
extern int nflash;


static FlashInfo *flashinfo;


static void
flash_flush(FlashInfo *f)
{
	if(f->last_secsize) {
		ulong base = f->last_secbase;
		print("flash: %ux-%ux: erasing/writing... ",
			base, base+f->last_secsize-1);
		soflush(stdout);
		flash_write_sector(f->m, base, f->secbuf);
		print("done\n");
		f->last_secsize = 0;
	}
}

static int
flash_close(Ostream *_s)
{
	FlashOstream *s = (FlashOstream*)_s;
	flash_flush(s->flashinfo);
	return 0;
}

static int
flash_read(Istream *_s, void *buf, int n)
{
	FlashIstream *s = (FlashIstream*)_s;
	FlashInfo *f = s->flashinfo;
	ulong ofs = s->pbase+s->pos;
	ulong e;
	ulong t = 0;

	if(n > s->size-s->pos)
		n = s->size-s->pos;
	if(n <= 0)
		return 0;
	e = f->last_secbase+f->last_secsize;

	/* if there's a cached write, grab the data from there... */
	if(ofs >= f->last_secbase && ofs < e) {
		int cs = n;	/* cached size */
		if(ofs+cs > e)
			cs = e;
		memmove(buf, f->secbuf+ofs-f->last_secbase, cs);
		ofs += cs;
		n -= cs;
		t += cs;
	}

	/* otherwise just get it from the flash */
	if(n > 0) {
		memmove(buf, (void*)(f->m->base+ofs), n);
		t += n;
	}

	s->pos += t;
	return t;
}


static int
flash_write(Ostream *_s, const void *va, int n)
{
	int t;
	FlashOstream *s = (FlashOstream*)_s;
	uchar *buf = (uchar*)va;
	FlashInfo *f = s->flashinfo;
	ulong ofs = s->pbase+s->pos;

	print("Flash write: %ux,%ux -> %ux-%ux\n", buf, n, ofs, ofs+n-1);
	t = 0;
	if(n == 0)
		flash_flush(f);
	if(n > s->size-s->pos)
		n = s->size-s->pos;
	while(n > 0) {
		ulong secstart = flash_sectorbase(f->m, ofs);
		ulong secsize = flash_sectorsize(f->m, ofs);
		ulong s_offset;
		ulong bufsize;

		if(f->last_secbase != secstart)
			flash_flush(f);

		if(f->last_secsize == 0) {
			if(!f->secbuf) {
				error(Enomem);
				return -1;
			}
			f->last_secsize = secsize;
			f->last_secbase = secstart;
			
			if(ofs != secstart || n < secsize)
				memmove(f->secbuf, (void*)(f->m->base+secstart), secsize);
		}

		s_offset = ofs-secstart;
		bufsize = n;
		if(s_offset+bufsize > secsize)
			bufsize = secsize - s_offset;
		memmove((void*)((ulong)f->secbuf + s_offset), buf, bufsize);

		n -= bufsize;
		buf += bufsize;
		ofs += bufsize;
		t += bufsize;
	}
	s->pos += t;
	return t;
}



uchar*
flash_mmap(Ostream *_s)
{
	FlashOstream *s = (FlashOstream*)_s;
	return (uchar*)(s->flashinfo->m->base + s->pbase);
}


static FlashInfo *
flashopen(FlashPTab *pt, const char *a)
{
	const char *pn;
	FlashInfo *f = &flashinfo[0];
	if((pn = strchr(a, '!'))) {
		f = &flashinfo[atoi(a)];
		++pn;
	} else
		pn = a;
	if(flash_init(f->m) < 0) {
		error("flash init failed");
		return 0;
	}
	if(flashptab_get(f->m, pn, pt) < 0) {
		char ebuf[ERRLEN];
		sprint(ebuf, "%s: no ptab", pn);
		error(ebuf);
		return 0;
	}
	return f;
}

int
flash_openi(FlashIstream *s, const char *a)
{
	FlashPTab pt;
	FlashInfo *f = flashopen(&pt, a);
	if(!f)
		return -1;
	s->flashinfo = f;
	s->pbase = pt.start;
	s->size = pt.length;
	s->read = flash_read;
	s->close = 0;
	s->mmap = flash_mmap;
	s->pos = 0;
	return 0;
}

int
flash_openo(FlashOstream *s, const char *a)
{
	FlashPTab pt;
	FlashInfo *f = flashopen(&pt, a);
	if(!f)
		return -1;
	s->flashinfo = f;
	s->pbase = pt.start;
	s->size = pt.length;
	s->write = flash_write;
	s->close = flash_close;
	s->mmap = nil;
	s->pos = 0;
	return 0;
}


static Istream*
flash_sd_openi(const char *args)
{
	FlashIstream *s = (FlashIstream*)malloc(sizeof(FlashIstream));
	
	if(s == nil) {
		error(Enomem);
		return nil;
	}
	if(!*args || flash_openi(s, args) < 0) {
		free(s);
		return nil;
	}
	return s;
}

static Ostream*
flash_sd_openo(const char *args)
{
	FlashOstream *s = (FlashOstream*)malloc(sizeof(FlashIstream));
	
	if(s == nil) {
		error(Enomem);
		return nil;
	}
	if(!*args || flash_openo(s, args) < 0) {
		free(s);
		return nil;
	}
	return s;
}

static StreamDev flash_sd = {
	"F",
	flash_sd_openi,
	flash_sd_openo,
};

#ifdef NOSTYX

#	define newflashchan(fi, fn, pt)	0
#	define freeflashchan(name)

#else

typedef struct FlashList FlashList;
typedef struct FCAux FCAux;

struct FCAux
{
	FlashInfo *fi;
	FlashPTab pt;
	Ostream *os;
	Istream *is;
};

struct FlashList
{
	BPChan b;
	FlashList *link;
};

static FlashList *flashchans;

static char None[] = "none";

static int
fcopen(BPChan *c, int)
{
	FCAux *f = c->aux;
	FlashIstream *is;
	FlashOstream *os;

	is = malloc(sizeof(FlashIstream));
	if(is == nil) {
		c->err = lasterr;
		return -1;
	}
	os = malloc(sizeof(FlashOstream));
	if(os == nil) {
		free(is);
		c->err = lasterr;
		return -1;
	}
	is->flashinfo = os->flashinfo = f->fi;
	is->size = os->size = f->pt.length;
	is->read = flash_read;
	is->close = 0;
	is->mmap = 0;
	is->pos = 0;
	is->pbase = f->pt.start;
	os->write = flash_write;
	os->close = flash_close;
	os->mmap = 0;
	os->pos = 0;
	os->pbase = f->pt.start;
	f->is = is;
	f->os = os;
	status(c->d.name, 0, c->d.length);
	return 0;
}

static void
fcclunk(BPChan *c)
{
	FCAux *f = c->aux;

	sd_closeo(f->os);
	statusclear();
}

static int
fcread(BPChan *c, uchar *buf, long n, long ofs)
{
	FCAux *f = c->aux;
	siseek(f->is, ofs, 0);
	n = sread(f->is, buf, n);
	status(c->d.name, ofs+n, c->d.length);
	return n;
}

static int
fcwrite(BPChan *c, uchar *b, long n, long ofs)
{
	FCAux *f = c->aux;
	uchar buf[MAXFDATA];

	memcpy(buf, b, n);
	soseek(f->os, ofs, 0);
	n = swrite(f->os, buf, n);
	status(c->d.name, ofs+n, c->d.length);
	return n;
}

static int
newflashchan(FlashInfo *fi, int fn, FlashPTab *pt)
{
	FCAux *fcaux;
	FlashList *fl = malloc(sizeof(*fl)+sizeof(*fcaux));
	BPChan *b;
	Dir *d;

	if(!bpi->file2chan)
		return 0;
	if(fl == nil) {
		error(Enomem);
		return -1;
	}
	memset(fl, 0, sizeof(*fl)+sizeof(*fcaux));
	fcaux = (FCAux*)(((char*)fl)+sizeof(*fl));
	fcaux->fi = fi;
	memmove(&fcaux->pt, pt, sizeof(*pt));
	b = &fl->b;
	d = &b->d;
	sprint(d->name, "flash%d%s", fn, pt->name);
	d->length = pt->length;
	strcpy(d->uid, None);
	strcpy(d->gid, None);
	d->mode = pt->perm;
	b->open = fcopen;
	b->clunk = fcclunk;
	b->read = fcread;
	b->write = fcwrite;
	b->aux = fcaux;
	bpi->file2chan(b);
	fl->link = flashchans;
	flashchans = fl;
	return 0;
}

static void
freeflashchan(char *name)
{
	FlashList *fl;
	FlashList *p;
	BPChan *b;
	FCAux *f;

	p = nil;
	fl = flashchans;
	while(fl != nil) {
		b = &fl->b;
		f = b->aux;
		if(strcmp(name, f->pt.name) == 0) {
			b->read = nil;
			b->write = nil;
			bpi->file2chan(b);
			if(p == nil)
				flashchans = fl->link;
			else
				p->link = fl->link;
			free(fl);
			return;
		}
		p = fl;
		fl = fl->link;
	}
}

#endif
static int
delpart(FlashMap *f, char *name)
{
	FlashPTab pt;

	if(flashptab_get(f, name, &pt) == -1) {
		error("nonexistant partition");
		return -1;
	}
	freeflashchan(pt.name);
	memset(&pt, 0, sizeof(pt));
	return flashptab_set(f, name, &pt);
}

static int
cmd_ptab(int argc, char **argv, int *nargv)
{
	char *cp;
	static FlashMap *f = &flashmap[0];
	int i, n;
	FlashPTab pt;
	int prot;

	// find /# option to select new flash number, where # = 0 to 9
	for(cp=argv[0]; *cp; cp++)
		if(*cp == '/') {
			int c = cp[1];
			if(c >= '0' && c <= '9') {
				f = &flashmap[c-'0'];
				strcpy(cp, cp+2);
				cp--;
			}
		}

	if(flash_init(f) < 0)
		return -1;
	if(strstr(argv[0], "/m")) {
		ulong a;
		for(a=0; a<f->totsize; a += flash_sectorsize(f, a))
			print("%8.8ux-%8.8ux (%8.8ux) : %ux\n", a,
				a+flash_sectorsize(f, a)-1,
				flash_sectorsize(f,a),
				flash_isprotected(f,a));
		return 0;
	}
	if(((prot = strstr(argv[0], "/p") != 0) || strstr(argv[0], "/u")) && argc == 2) {
		if(flashptab_get(f, argv[1], &pt) < 0)
			return -1;
		return flash_protect(f, pt.start, pt.length, prot) ? 0 : -1;
	}
	if(strstr(argv[0], "/a") && argc == 2)			// set autoboot
		return flashptab_setboot(f, argv[1]);
	if(strstr(argv[0], "/d") && argc == 2)			// delete
		return delpart(f, argv[1]);
	if(argc == 1 && argv[0][1] == 0) {
		n = flashptab_get(f, (char*)FLASHPTAB_PARTITION_PNUM, nil);
		if(n < 0) {
			error("no ptable");
			return -1; 
		}
		n /= sizeof(FlashPTab);
		print("dev__ ____base ____size perm flgs name________________ (max=%d boot=%ux)\n", n, flashptab_getboot(f));
		for(i=FLASHPTAB_MIN_PNUM; i<n; i++)
			if(flashptab_get(f, (char*)i, &pt) >= 0)
				print("F!%-2d: %8ux %8ux %4o %4ux %s\n",
					i, pt.start, pt.length, pt.perm,
					pt.flags, pt.name);
		return 0;
	} 
	if(argc == 7) {
		delpart(f, argv[1]);
		delpart(f, argv[6]);
		pt.start = nargv[2];
		pt.length = nargv[3];
		pt.perm = strtoul(argv[4], 0, 8);
		pt.flags = nargv[5];
		strncpy(pt.name, argv[6], sizeof(pt.name));
		n = flashptab_set(f, argv[1], &pt);
		if(n != -1)
			n = newflashchan(&flashinfo[f-flashmap], f-flashmap, &pt);
		return n;
	}
	// print("usage: P[/#][/m][/p][/u][/a][/d] [pnum start size perm flags name]\n");
	return -1;
}


int
getflashbase(void) { return flashmap[0].base; }

void
setflashbase(int b) { flashmap[0].base = b; }


void
flashstreamlink(void)
{
	int i;
	FlashPTab pt;
	int n;
	int fn;

	addcmd('P', cmd_ptab, 0, 6, "ptab mgmt");
	nbindenv("flashbase", getflashbase, setflashbase);
	addstreamdevlink(&flash_sd);

	flashinfo = malloc(nflash*sizeof(FlashInfo));
	memset(flashinfo, 0, nflash*sizeof(FlashInfo));

	for(fn=0; fn<nflash; fn++) {
		FlashInfo *f = &flashinfo[fn];
		f->m = &flashmap[fn];
		if(flash_init(f->m) < 0)
			continue;
		f->secbuf = malloc(f->m->secsize);
		n = flashptab_get(f->m, (char*)FLASHPTAB_PARTITION_PNUM, nil);
		if(n < 0)
			continue;
		n /= sizeof(FlashPTab);
		for(i=FLASHPTAB_MIN_PNUM; i<n; i++) {
			if(flashptab_get(f->m, (char*)i, &pt) >= 0) 
				newflashchan(f, fn, &pt);
		}
	}
}

