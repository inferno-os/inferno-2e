/* 
Flash chips supported:

	AMD:
		Am29F040B
		Am29LV800BT
		Am29LV800BB
		Am29LV160BT
		Am29LV160BB
	Atmel:
		AT29LV1024
	Sharp:
		LH28F016SU

Flash chips that haven't been tested, but can probably be made to work easily:

	Sharp:
		LH28F016SA
		LH28F008SA
	Intel:  TE28F160S3 -support UMEC and Duoyuan phone -Peter
*/

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"../port/error.h"

typedef struct FlashMap FlashMap;

struct FlashMap {
	ulong base;
	uchar width;		/* number of chips across bus */
	uchar l2width;		/* log2(width) */
	uchar man_id;		/* manufacturer's ID */
	uchar pmode;		/* programming mode */
	ushort dev_id;		/* device ID */
	ushort flags;		/* various config flags */
	ulong totsize; 		/* total flash size*width, in bytes */
	ulong secsize; 		/* sector size*width, in bytes */
	uint l2secsize;		/* log2(secsize) */
	ulong bootlo;		/* lowest address of boot area */
	ulong boothi;		/* highest address of boot area+1 */
	ulong bootmap;		/* bitmap of boot area layout 
				 * broken into pieces of MBSECSIZE*width */
	char *man_name;	/* manufacturer name */
	char *dev_name;	/* device name */
};

enum {
	FLASH_FLAG_NEEDUNLOCK = 0x01,	/* need unlock before access */
	FLASH_FLAG_NEEDERASE = 0x04,	/* need to erase before writing */
	FLASH_FLAG_WRITEMANY = 0x08,	/* can write many words at once */
};

enum {
	FLASH_PMODE_AMD_29,		/* use AMD29xxx-style */
	FLASH_PMODE_SHARP_SA,		/* use Sharp SA-style */
	FLASH_PMODE_SHARP_SU,		/* use Sharp SU-style */
};

ulong	flash_sectorsize(FlashMap*, ulong ofs);
ulong	flash_sectorbase(FlashMap*, ulong ofs);
uchar	flash_isprotected(FlashMap*, ulong ofs);
int	flash_protect(FlashMap*, ulong ofs, ulong size, int yes);
int	flash_write_sector(FlashMap*, ulong ofs, ulong *data);
int	flash_init(FlashMap*);


static int	STUPID_HACK;		/* Foolish inconsistency is the bane of large projects. */

#define FLASHPTAB_MAGIC_OFS	(STUPID_HACK+0x0010)	/* ulong */
#define FLASHPTAB_OFS_OFS	(STUPID_HACK+0x0014)	/* ulong */
#define FLASHPTAB_SIZE_OFS	(STUPID_HACK+0x0018)	/* ulong */

#define FLASH_MON_OFS_OFS	(STUPID_HACK+0x0020)	/* ulong */
#define FLASH_MON_SIZE_OFS	(STUPID_HACK+0x0024)	/* ulong */
#define FLASH_AUTOBOOT_OFS_OFS	(STUPID_HACK+0x0028)	/* ulong */
#define FLASH_BOOT_OFS_OFS	(STUPID_HACK+0x002c)	/* ulong */

#define	FLASHPTAB_MAGIC		0xc001babe
// #define FLASHPTAB_OFS_DFLT	0x0600
// #define FLASHPTAB_SIZE_DFLT	16

#define FLASHPTAB_MAXNAME	20

#define FLASHPTAB_MIN_PNUM		-2
#define FLASHPTAB_PARTITION_PNUM	-2
#define FLASHPTAB_ALL_PNUM		-1

typedef struct FlashPTab {
	char 	name[FLASHPTAB_MAXNAME];
	ulong	start;
	ulong	length;
	ushort	perm;
	ushort	flags;
} FlashPTab;


enum {
	FLASHPTAB_FLAG_BOOT = 0x0010,
	FLASHPTAB_FLAG_KERN = 0x0020,
	FLASHPTAB_FLAG_TEST = 0x0040,
	FLASHPTAB_FLAG_FAIL = 0x0080,
};

extern int flashptab_get(FlashMap *f, char *, FlashPTab *);
extern int flashptab_set(FlashMap *f, char *, FlashPTab *);


enum { Qstat = 0,
 };

static int Qflashctl = 666;
static int debug = 0;


// most of the support for simultaneous multiple flashes is here,
// but it probably isn't quite complete (and certainly some means
// of initially detecting the other flashes is needed)

typedef struct FlashInfo FlashInfo;
struct FlashInfo {
	FlashMap;
	FlashPTab *ptab;
	int ptab_size;
	Dirtab *dir;
	int dir_size;
	QLock lock;
	Rendez vous;
	int num;

	/* to avoid re-erasing during sequential writes: */
	ulong last_secbase;
	ulong last_secsize;
	ulong *secbuf;
	Rendez vous_to;
	int timeout;
	int timeout_thread;
};

static FlashInfo *flash = 0;
static FlashInfo mainflash;

#define FLASH_FLUSH_TIMEOUT 5000	/* msec */


static int	 
flashwalk(Chan* c, char* name)
{
	return devwalk(c, name, flash->dir, flash->dir_size, devgen);
}

static void	 
flashstat(Chan* c, char* dp)
{
	devstat(c, dp, flash->dir, flash->dir_size, devgen);
}

static Chan*
flashopen(Chan* c, int omode)
{
	omode = openmode(omode);

	return devopen(c, omode, flash->dir, flash->dir_size, devgen);
}

static void	 
flashclose(Chan*)
{
}



static ulong
chksum(ulong addr, ulong len)
{
	uchar *c = (uchar*)addr;
	ulong sum = 0;
	while(len--) 
		sum += *c++;
	return sum;
}


static long	 
flashread(Chan* c, void* buf, long n, ulong offset)
{
	int total = 0;
	FlashInfo *f = &flash[0];
	int len = f->dir[c->qid.path].length;

	if(c->qid.path == Qflashctl)
		return 0;

	if(c->qid.path & CHDIR) {
		if(debug)
			print("dir=%lux dirsize=%d\n", f->dir, f->dir_size);
		return devdirread(c, buf, n, f->dir, f->dir_size, devgen);
	}

	if(c->qid.path < 0 || c->qid.path >= f->dir_size)
		return 0;

	if(n <= 0)
		return 0;

	if(c->qid.path == Qstat){
		char *c = malloc(16384);
		char *s = c;
		int i, x, p;
		s += sprint(s, "%2.2ux %2.2ux %1.1d %8.8lux %8.8lux %s %s\n",
			f->man_id, f->dev_id, f->width, f->secsize, f->totsize,
			f->man_name, f->dev_name);
		qlock(&f->lock);
		x = -1;
		for(i=0; i<=f->totsize; i += flash_sectorsize(f, i)) {
			p = i >= f->totsize ? -1 : flash_isprotected(f, i);
			if(x >= 0 && x != p) {
				s += sprint(s, "%8.8ux: %1.1ux\n", i-1, x);
				x = -1;
			}
			if(p == -1)
				break;
			if(x < 0)
				s += sprint(s, "%8.8x-", i);
			x = p;
		}
		qunlock(&f->lock);
		for(x=1; x<f->dir_size; x++) {
			p = f->dir[x].qid.vers;
			s += sprint(s, "%8.8lux %8.8lux %8.8lux %s\n",
					f->ptab[p].start,
					f->ptab[p].length,
					chksum(f->base + f->ptab[p].start,
						f->ptab[p].length),
					f->ptab[p].name);
		}
		len = s-c;
		if(offset + n > len)
			n = len - offset;
		if(n <= 0) {
			free(c);
			return 0;
		}	
		memmove(buf, c+offset, n);
		free(c);
		return n;
	}

	if(offset + n > len) 
		n = len - offset;

	qlock(&f->lock);	
	offset += f->ptab[c->qid.vers].start;

	/* if there's a cached write, grab the data from there... */
	if(f->secbuf && offset >= f->last_secbase
				&& offset < f->last_secbase+f->last_secsize) {
		int cs = n;	/* cached size */
		if(offset+cs > f->last_secbase+f->last_secsize)
			cs = f->last_secbase+f->last_secsize-offset;
    		memmove(buf,
			(void*)((ulong)f->secbuf+offset-f->last_secbase), cs);
		offset += cs;
		n -= cs;
		total += cs;
	} 

	/* otherwise just get it from the flash */
	if(n > 0) {
    		memmove(buf, (void*)(f->base+offset), n);
		total += n;
	}

	qunlock(&f->lock);
	return total;
}


static int
flash_check_timeout(void *)
{
	FlashInfo *f = &flash[0];
	if(f->timeout > 0) {
		f->timeout = 0;
		return 0;
	} else
		return 1;
}


static void
flash_flush(FlashInfo *f)
{
	if(f->secbuf) {
		if(debug)
			print("flash%d: flush %8.8lux-%8.8lux from %8.8lux: erase/write... ",
				f->num,
				f->last_secbase,
				f->last_secbase+f->last_secsize-1,
				f->secbuf);
		flash_write_sector(f, f->last_secbase, f->secbuf);
		if(debug)
			print("done\n");
		free(f->secbuf);
		f->secbuf = 0;
	}
}


static void
flash_timeout(void *vf)
{
	FlashInfo *f = (FlashInfo*)vf;
	do {
		tsleep(&f->vous_to, flash_check_timeout, f, f->timeout);
	} while(f->timeout != 0);
	if(!canqlock(&f->lock)) 
		f->timeout_thread = 0;
	else {
		if(debug)
			print("flash%d: timeout buffer flush\n", f->num);
		f->timeout_thread = 0;
		flash_flush(f);
		qunlock(&f->lock);
	}
	pexit("", 0);
}


static long	 
flashwrite(Chan* c, void* buf, long n, ulong offset)
{
	ulong partoff; 
	ulong total = 0;
	ulong s_offset;
	ulong bufsize;
	FlashInfo *f = &flash[0];

	/* print("flashwrite(%8.8ux, %8.8ux, ofs=%8.8ux)\n", buf, n, offset); /* */

	if(c->qid.path == Qflashctl) {
		if(strncmp(buf, "sync",4) == 0) {
			qlock(&f->lock);
			flash_flush(f);			/* sync our flash */
			qunlock(&f->lock);
		}
		else 
			error(Ebadarg);

		return n;
	}


	if(c->qid.path <= Qstat || c->qid.path >= f->dir_size)
		return 0;

	if(offset + n > f->dir[c->qid.path].length) 
		n = f->dir[c->qid.path].length - offset;
	if(n <= 0)
		return 0;

	partoff = f->ptab[c->qid.vers].start;

	qlock(&f->lock);

	while( n ) {

		ulong secstart = flash_sectorbase(f, partoff+offset);
		ulong secsize = flash_sectorsize(f, partoff+offset);
 
		if(f->last_secbase != secstart)
			flash_flush(f);

		if(!f->secbuf) {
			f->last_secsize = secsize;
			f->secbuf = malloc(secsize);
			f->last_secbase = secstart;

			if(partoff+offset != secstart || n < secsize) 
				memmove(f->secbuf, (void*)(f->base+secstart),
						secsize);
		}

		s_offset = partoff+offset-secstart;
		bufsize = n;
		if(s_offset+bufsize > secsize)
			bufsize = secsize - s_offset;
		memmove((void*)((ulong)f->secbuf + s_offset), buf, bufsize);

		n -= bufsize;
		buf = (void*)((ulong)buf + bufsize);
		offset += bufsize;
		total += bufsize;
	}

	f->timeout = FLASH_FLUSH_TIMEOUT;
	if(f->timeout_thread)
		wakeup(&f->vous_to);
	else {
		f->timeout_thread = 1;
		kproc("flash", flash_timeout, f);
	}
	qunlock(&f->lock);

	return total;
}


static void
flashinit(void)
{
	FlashInfo *f = &mainflash;

	memset(f, 0, sizeof(FlashInfo));
	f->base = conf.flashbase;
	if(flash_init(f) >= 0) {
		int i, d;
		int fn = 0;

		f->num = fn;
		i = flashptab_get(f, (char*)FLASHPTAB_PARTITION_PNUM, nil);
		if(i < 0) {
			print("flash @%lux: no ptab\n", f->base);
			i = 0;
		}
		flash = f;
		f->ptab_size = i/sizeof(FlashPTab);
/*		print("flash @%ux: ptab_size=%ux/%d\n", 
				f->base, i, f->ptab_size); */
		f->ptab = (FlashPTab*)malloc(sizeof(FlashPTab)*
				(f->ptab_size-FLASHPTAB_MIN_PNUM));
		f->dir = malloc((f->ptab_size-FLASHPTAB_MIN_PNUM+2)
					*sizeof(Dirtab));
		d = 1;
		for(i=FLASHPTAB_MIN_PNUM; i<f->ptab_size; i++) {
			int n=i-FLASHPTAB_MIN_PNUM;
			if(flashptab_get(f, (char*)i, &f->ptab[n]) >= 0) {
				sprint(f->dir[d].name, "flash%d%s",
					fn, f->ptab[n].name);
				f->dir[d].qid = (Qid){d, n};
				f->dir[d].length = f->ptab[n].length;
				f->dir[d].perm = f->ptab[n].perm;
/*				print("  F!%d: %ux %ux %o %ux %s\n",
					i,
					f->ptab[n].start,
					f->dir[d].length,
					f->dir[d].perm,
					f->ptab[n].flags,
					f->dir[d].name); */
				d++;
			}
		}
		/* Set up ctl file */
		Qflashctl = d;
		sprint(f->dir[d].name, "flash%dctl",fn);
		f->dir[d].qid = (Qid){d,0};
		f->dir[d].length = 0;
		f->dir[d].perm = 0666;
		d++;

		f->dir_size = d;
		sprint(f->dir[Qstat].name, "flash%dstat", fn);	
		f->dir[Qstat].qid = (Qid){Qstat, 0};
		f->dir[Qstat].length = 0;
		f->dir[Qstat].perm = 0444;
	} else
		print("flash @%lux: not found\n", f->base);
}

static Chan*
flashattach(char* spec)
{
	if(debug)
		print("attach: f=%lux base=%lux ptab=%lux size=%d\n",
			flash, flash->base, flash->ptab, flash->ptab_size); 
	if(flash)
		return devattach('W', spec);
	else
		return nil;
}

static void
flashwstat(Chan *c, char *dp)
{
	FlashInfo *f = &flash[0];
	ulong partoff;
	ulong size;
	Dir d;
	int r;

	convM2D(dp, &d);
	partoff = f->ptab[c->qid.vers].start;
	size = f->dir[c->qid.path].length;
	if(debug)
		print("wstat: ofs=%lo size=%lo mode=%luo\n",
			partoff, size, d.mode);
	qlock(&f->lock);
	r = flash_protect(f, partoff, size, !(d.mode&0222));
	qunlock(&f->lock);
	if (!r)
		error(Ebadarg);
}

Dev flashdevtab = {
	'W',
	"flash",
	
	devreset,
	flashinit,
	flashattach,
	devdetach,
	devclone,
	flashwalk,
	flashstat,
	flashopen,
	devcreate,
	flashclose,
	flashread,
	devbread,
	flashwrite,
	devbwrite,
	devremove,
	flashwstat,
};

#define BUSWIDTH	4	/* number of bytes across bus */
#define L2BUSWIDTH	2	/* log2(BUSWIDTH) */
#define MBSECSIZE	4096	/* minimum boot sector size */
#define L2MBSECSIZE	12	/* log2(MBSECSIZE) */

#define BS_16K_8K_8K_32K	((1<<3)|(1<<5)|(1<<7)|(1<<15))
#define BS_32K_8K_8K_16K	((1<<7)|(1<<9)|(1<<11)|(1<<15))

ulong
flash_sectorsize(FlashMap *f, ulong offset)
{
	if(f->secsize >= MBSECSIZE
			&& offset >= f->bootlo && offset < f->boothi) {
		int n = (offset-f->bootlo)>>(L2MBSECSIZE+f->l2width);
		ulong b = f->bootmap;
		ulong s;
		SET(s);
		while(n >= 0) {
			s = 1;
			while(!(b&1)) {
				b >>= 1;
				++s;
				n--;
			}
			while(b&1) {
				b >>= 1;
				n--;
			}
		}
		return s<<(L2MBSECSIZE+f->l2width);
	}
	if(offset >= f->totsize)
		return 0;
	return f->secsize;
}

ulong
flash_sectorbase(FlashMap *f, ulong offset)
{
	/* assumes sectors are aligned to their size */
	ulong s = flash_sectorsize(f, offset);
	if(s)
		return offset&~(s-1);
	else
		return (ulong)-1;
}

static void
unlock_write(FlashMap *f)
{
	if(f->flags & FLASH_FLAG_NEEDUNLOCK) {
		*(ulong *)(f->base + 0x5555*BUSWIDTH) = 0xaaaaaaaa;
		*(ulong *)(f->base + 0x2aaa*BUSWIDTH) = 0x55555555;
	}
}

static void
reset_read_mode(FlashMap *f)
{
	unlock_write(f);
	switch(f->pmode) {
	case FLASH_PMODE_AMD_29:
		*(ulong*)(f->base + 0x5555*BUSWIDTH) = 0xf0f0f0f0;
		delay(20);
		break;
	case FLASH_PMODE_SHARP_SA:
	case FLASH_PMODE_SHARP_SU:
	default:
		*(ulong*)(f->base) = 0xffffffff;
		break;
	}
}

static void
abort_and_reset(FlashMap *f)
{
	switch(f->pmode) {
	case FLASH_PMODE_SHARP_SA:
	case FLASH_PMODE_SHARP_SU:
		// *(ulong*)(f->base) = 0x80808080;
		break;
	}
	reset_read_mode(f);
}

static void 
autoselect_mode(FlashMap *f)
{
	unlock_write(f);
  	*(ulong *)(f->base + 0x5555*BUSWIDTH) = 0x90909090;
	delay(20);
}


static void
flash_readid(FlashMap *f)
{
	ulong id;
	char *chipman = "?";
	char *chipname = "?";

	f->flags = FLASH_FLAG_NEEDUNLOCK;
	autoselect_mode(f);
	f->man_id = *(ulong *)(f->base + 0x00*BUSWIDTH) & 0xff;
	id = *(ulong*)(f->base + 0x01*BUSWIDTH);
	f->dev_id = id & 0xff;
	f->secsize = 0;
	f->totsize = 0;
	f->l2width = L2BUSWIDTH;
	f->bootmap = 0;
	f->bootlo = f->boothi = 0;
	switch(f->man_id) {
	case 0x01:
		chipman = "AMD";
		f->flags |= FLASH_FLAG_NEEDERASE;
		f->pmode = FLASH_PMODE_AMD_29;
		switch(f->dev_id) {
		case 0xa4:
			chipname = "Am29F040B";
			f->totsize = 512*_K_;
			f->secsize = 64*_K_;
			f->l2secsize = 16;
			break;
		case 0xda:
			chipname = "Am29LV800BT";
			f->totsize = 1*_M_;
			f->bootlo = (1*_K_-64)*_K_;
			f->bootmap = BS_32K_8K_8K_16K;
			goto Am29LV;
		case 0x5b:
			chipname = "Am29LV800BB";
			f->totsize = 1*_M_;
			f->bootmap = BS_16K_8K_8K_32K;
			goto Am29LV;
		case 0xc4:
			chipname = "Am29LV160BT";
			f->totsize = 2*_M_;
			f->bootlo = (2*_K_-64)*_K_;
			f->bootmap = BS_32K_8K_8K_16K;
			goto Am29LV;
		case 0x49:	
			chipname = "Am29LV160BB";
			f->totsize = 2*_M_;
			f->bootmap = BS_16K_8K_8K_32K;
		    Am29LV:
			f->l2secsize = 16;		/* 64K */
			if((id&0xff00) == 0x2200)
				f->l2width = L2BUSWIDTH-1;
			break;
		}
		break;
	case 0x1f:
		chipman = "Atmel";
		f->pmode = FLASH_PMODE_AMD_29;
		switch(f->dev_id) {
		case 0x26:
			chipname = "AT29LV1024";
			f->totsize = 128*_K_;
			f->l2secsize = 8;		/* 256 bytes */
			f->l2width = L2BUSWIDTH-1;
			f->flags |= FLASH_FLAG_WRITEMANY;
			break;
		}
		break;
	case 0x89:	// from LH28F008SA docs
	case 0xb0:	// from LH28F016SU docs
		chipman = "Sharp";
		f->flags &= ~FLASH_FLAG_NEEDUNLOCK;
		f->pmode = FLASH_PMODE_SHARP_SA;
		autoselect_mode(f);
		id = *(ulong*)(f->base + 0x01*BUSWIDTH);
		switch((f->dev_id = id&0xffff)) {
		case 0x6688:
			chipname = "LH28F016SU";
			goto SharpLH28F016;
		case 0x66a0:
			chipname = "LH28F016SA";
		SharpLH28F016:
			f->totsize = 2*_M_;
			goto SharpLH28F;
		case 0xa2a2:
			chipname = "LH28F008SA";
			f->totsize = 1*_M_;
		SharpLH28F:
			f->l2secsize = 16;		/* 64K */
			f->l2width = L2BUSWIDTH-1;
			goto end;
		case 0xd0:
                        chipman = "Intel";
                        chipname = "TE28F160S3";
                        f->l2secsize =16;
                        f->totsize = 2*_M_;
                        f->l2width = L2BUSWIDTH-1;
                end:    break;

		}
		break;
	default:
		break;
	}
	reset_read_mode(f);
	f->secsize = 1<<f->l2secsize;
	if(!f->boothi)
		f->boothi = f->bootlo + f->secsize;
	if(!f->bootmap)
		f->bootmap = 1<<(f->secsize/MBSECSIZE-1);
	f->width = 1<<f->l2width;

	f->secsize *= f->width;
	f->totsize *= f->width;
	f->bootlo *= f->width;
	f->boothi *= f->width;

	f->man_name = chipman;
	f->dev_name = chipname;

/*	print("flash: id=%ux/%ux (%s %s), ss=%ux fs=%ux w=%d b=%ux-%ux f=%ux\n",
		f->man_id, f->dev_id,
		chipman, chipname,
		f->secsize, f->totsize, f->width,
		f->bootlo, f->boothi, f->flags); */
}


static ulong
protmask(FlashMap *f, ulong x)
{
	int i;
	ulong r, b, m;
	r = 0;
	b = 1<<(L2BUSWIDTH-f->l2width);
	m = b-1;
	b <<= 3;
	for(i=0; i<BUSWIDTH; i++) {
		r |= ((x&1)<<i);
		if((i&m) == m)
			x >>= b;
	}
	return r;
}

uchar
flash_isprotected(FlashMap *f, ulong offset)
{
	ulong x;
	switch(f->pmode) {
	case FLASH_PMODE_AMD_29:
		autoselect_mode(f);
		x = *(ulong *)(f->base + offset + 0x02*BUSWIDTH);
		reset_read_mode(f);
		return protmask(f, x);
	default:
		return 0;
	}
}


static int
amd_erase_sector(FlashMap *f, ulong offset)
{
	unlock_write(f);
	*(ulong *)(f->base + 0x5555*BUSWIDTH) = 0x80808080;
	unlock_write(f);
	*(ulong *)(f->base + offset) = 0x30303030;
	return timer_devwait((ulong*)(f->base + offset),
				0xa0a0a0a0, 0xa0a0a0a0, MS2TMR(2000)) >= 0;
}

static int
flash_protect_sector(FlashMap *f, ulong ofs, int yes)
{
	ulong *p;
	int i;
	ofs += f->base;
	switch(f->pmode) {
	case FLASH_PMODE_AMD_29:
		p = (ulong*)(ofs + (yes ? 0x02*BUSWIDTH : 0x42*BUSWIDTH));
		if(!archflash12v(1))
			return 0;
		*p = 0x60606060;
		i = 0;
		do {
			if(++i > 25) {
				reset_read_mode(f);
				return 0;
			}
			*p = 0x60606060;
			microdelay(yes ? 150 : 15000);
			*p = 0x40404040;
		} while(protmask(f, *p) != protmask(f, 0xffffffff));
		reset_read_mode(f);
		return 1;
	default:
		return 0;
	}
}


int
flash_protect(FlashMap *f, ulong ofs, ulong size, int yes)
{
	ulong x = flash_sectorbase(f, ofs);
	while(x < ofs+size) {
		if(!flash_protect_sector(f, x, yes))
			return 0;
		x += flash_sectorsize(f, x);
	}
	return 1;
}


static int
sharp_write_sector(FlashMap *f, ulong offset, ulong *buf, ulong secsize)
{
	int i;
	ulong *p = (ulong*)(f->base + offset);
	*p = 0x50505050;	// clear CSR
	*p = 0x20202020;	// erase
	*p = 0xd0d0d0d0;	// verify
	if(timer_devwait(p, 0x00800080, 0x00800080, MS2TMR(2000)) < 0
			|| (*p & 0x00280028)) {
		abort_and_reset(f);
		return 0;
	}
	for(i=secsize >> 2; i; i--, p++, buf++) {
		*p = 0x40404040;
		*p = *buf;
		if(timer_devwait(p, 0x00800080, 0x00800080, US2TMR(100)) < 0) {
			abort_and_reset(f);
			return 0;
		}
	}
	reset_read_mode(f);
	return 1;
}


static int
fast_write_sector(FlashMap *f, ulong offset, ulong *buf, ulong secsize)
{
	int i;
	unlock_write(f);
	*(ulong *)(f->base + 0x5555*BUSWIDTH) = 0xa0a0a0a0;
	for(i=secsize/sizeof(ulong); i; i--, offset += sizeof(ulong), buf++)
		*(ulong*)(f->base + offset) = *buf;
	offset -= sizeof(ulong);
	--buf;
	if(timer_devwait((ulong*)(f->base + offset),
			0x80808080, *buf&0x80808080, MS2TMR(2000)) < 0) {
		abort_and_reset(f);
		return 0;
	}
	return 1;
}

static int
slow_write_sector(FlashMap *f, ulong offset, ulong *buf, ulong secsize)
{
	int i;
	for(i=secsize/sizeof(ulong); i; i--, offset += sizeof(ulong), buf++) {
		ulong val = *buf;
		unlock_write(f);
		*(ulong *)(f->base + 0x5555*BUSWIDTH) = 0xa0a0a0a0;
		*(ulong *)(f->base + offset) = val;
		if(timer_devwait((ulong*)(f->base + offset),
				0x80808080, val&0x80808080, US2TMR(100)) < 0) {
			abort_and_reset(f);
			return 0;
		}
	}
	return 1;
}


int
flash_write_sector(FlashMap *f, ulong offset, ulong *buf)
{
	int tries;
	ulong secsize = flash_sectorsize(f, offset);

	if(offset+secsize > f->totsize)
		panic("flash: attempt to erase %8.8lux\n", offset);
	if(flash_isprotected(f, offset)) {
		print("flash: %lux-%lux is protected\n",
			offset, offset+secsize-1);
		return 0;
	}
	if(offset >= f->bootlo && offset < f->boothi) 
		print("flash: erasing boot area! (%lux-%lux)\n",
			offset, offset+secsize-1);
	for(tries = 3; tries; tries--)
	{
		switch(f->pmode) {
		case FLASH_PMODE_SHARP_SA:
			if(!sharp_write_sector(f, offset, buf, secsize))
				continue;
			break;
		case FLASH_PMODE_AMD_29:
			if((f->flags & FLASH_FLAG_NEEDERASE)
					&& !amd_erase_sector(f, offset))
				continue;
			if(!((f->flags & FLASH_FLAG_WRITEMANY)
				? fast_write_sector(f, offset, buf, secsize)
				: slow_write_sector(f, offset, buf, secsize)))
				continue;
			break;
		default:
			continue;
		}
		if(!memcmp((void*)(f->base+offset), buf, secsize))
			return 1;
		print("flash: verify error (%lux-%lux)\n",
			offset, offset+secsize-1);
	}
	print("flash: write failure (%lux-%lux)\n",
		offset, offset+secsize-1);
	return 0;
}


int
flash_init(FlashMap *f)
{
	flash_readid(f);
	archflashwp(0);
	return (f->secsize > 0) ? 0 : -1;
}

// Note: all of this code assumes that the monitor code resides at offset 0
// in the flash.  Newer demons can specify the start and size of their
// boot monitor (demon) code.  This should be updated to obtain such 
// information and make use of it.


static int
flashptab_getnum(FlashPTab *pt, char *name, int np)
{
	if((int)name >= -127 && (int)name <= 127)
		return (int)name;
	else if(name[0] == '-' || (name[0] >= '0' && name[0] <= '9'))
		return strtol(name, 0, 0);
	else if(strcmp(name, "partition") == 0)
		return FLASHPTAB_PARTITION_PNUM;
	else if(strcmp(name, "all") == 0)
		return FLASHPTAB_ALL_PNUM;
	else {
		int i;
		for(i=0; i<np; i++)
			if(strcmp(name, pt[i].name) == 0)
				return i;
		return -128;
	} 
}

// call with string name (actual name or partition number as string)
// or with partition number cast to a char*
// returns partition size for valid partition
// npt can be nil and will not be copied in that case

int
flashptab_get(FlashMap *f, char *name, FlashPTab *npt)
{
	ulong ptsize;
	ulong ptofs;
	int pn, np;
	FlashPTab *pt;

	SET(ptsize);
	SET(ptofs);
	if(*(ulong*)(f->base + FLASHPTAB_MAGIC_OFS) != FLASHPTAB_MAGIC)
		STUPID_HACK = 0x400;
	if(*(ulong*)(f->base + FLASHPTAB_MAGIC_OFS) == FLASHPTAB_MAGIC) {
		ptsize = *(ulong*)(f->base + FLASHPTAB_SIZE_OFS);
		ptofs = *(ulong*)(f->base + FLASHPTAB_OFS_OFS);
		np = ptsize / sizeof(FlashPTab);
		pt = (FlashPTab*)(f->base + ptofs);
	} else {
		pt = 0;
		np = 0;
	}
	pn = flashptab_getnum(pt, name, np);

	if(pn == -1) {
		if(npt) {
			strcpy(npt->name, "all");
			npt->start = 0;
			npt->length = f->totsize;
			npt->perm = 0644; 
			npt->flags = 0;
		}
		return f->totsize;
	}
	if(pn == -2 && pt) {
		if(npt) {
			strcpy(npt->name, "partition");
			npt->start = ptofs;
			npt->length = ptsize;
			npt->perm = 0644; 
			npt->flags = 0;
		}
		return ptsize;
	} else if(pn >= 0 && pn < np && pt[pn].name[0]) {
		if(npt)
			*npt = pt[pn];
		return pt[pn].length;
	} else
		return -1;
}
	

int
flashptab_set(FlashMap *f, char *name, FlashPTab *npt)
{
	ulong ptsize, ptofs;
	int np, pn;
	FlashPTab *pt;

	if(*(ulong*)(f->base + FLASHPTAB_MAGIC_OFS) != FLASHPTAB_MAGIC)
		return -1;
	ptsize = *(ulong*)(f->base + FLASHPTAB_SIZE_OFS);
	ptofs = *(ulong*)(f->base + FLASHPTAB_OFS_OFS);
	np = ptsize / sizeof(FlashPTab);
	pt = (FlashPTab*)(f->base + ptofs);
	pn = flashptab_getnum(pt, name, np);
	if(pn == -128)
		for(pn=0; pn<np; pn++)
			if(!pt[pn].name[0])
				break;
	if(pn >= 0 && pn < np) {
		ulong data[2];
		ulong dofs, ofs;
		uchar *dptr, *buf;
		int r;
		int dsize, secsize;
		if(npt) {
			dptr = (uchar*)npt;
			dsize = sizeof *npt;
			dofs = ptofs + pn*sizeof(FlashPTab);
		} else {
			data[0] = pt[pn].start;
			data[1] = pt[pn].start;
			dptr = (uchar*)data;
			dofs = FLASH_AUTOBOOT_OFS_OFS;
			dsize = sizeof(ulong) + (data[0] ? sizeof(ulong) : 0);
		}
		secsize = flash_sectorsize(f, dofs);
		ofs = flash_sectorbase(f, dofs);
		buf = malloc(secsize);
		memmove(buf, (uchar*)f->base+ofs, secsize);
		memmove(buf+dofs-ofs, dptr, dsize);
		r = flash_write_sector(f, ofs, (ulong*)buf);
		free(buf);
		return r ? 0 : -1;
	}
	return -1;
}
