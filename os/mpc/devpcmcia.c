#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/error.h"

/*
 *  MPC821/3 PCMCIA driver (prototype)
 *
 * unlike the i82365 adapter, there isn't an offset register:
 * card addresses are simply the lower order 26 bits of the host address.
 *
 * to do:
 *	split allocation of memory/attrib (all 26 bits valid) and io space (typically 10 or 12 bits)
 *	correct config
 *	interrupts and i/o space access
 *	DMA
 */

enum
{
	Maxctlr=	1,

	/* pipr */
	Cbvs1=	1<<15,
	Cbvs2=	1<<14,
	Cbwp=	1<<13,
	Cbcd2=	1<<12,
	Cbcd1=	1<<11,
	Cbbvd2=	1<<10,
	Cbbvd1=	1<<9,
	Cbrdy=	1<<8,

	/* pscr */
	Cbvs1_c=	1<<15,
	Cbvs2_c=	1<<14,
	Cbwp_c=	1<<13,
	Cbcd2_c=	1<<12,
	Cbcd1_c=	1<<11,
	Cbbvd2_c=	1<<10,
	Cbbvd1_c=	1<<9,
	Cbrdy_l=	1<<7,
	Cbrdy_h=	1<<6,
	Cbrdy_r=	1<<5,
	Cbrdy_f=	1<<4,

	/* per */
	Cb_evs1=	1<<15,
	Cb_evs2=	1<<14,
	Cb_ewp=	1<<13,
	Cb_ecd2=	1<<12,
	Cb_ecd1=	1<<11,
	Cb_ebvd2=	1<<10,
	Cb_ebvd1=	1<<9,
	Cb_erdy_l=	1<<7,
	Cb_erdy_h=	1<<6,
	Cb_erdy_r=	1<<5,
	Cb_erdy_f=	1<<4,

	/* pgcrb */
	Cbdreq_int=	0<<14,
	Cbdreq_iois16=	2<<14,
	Cbdreq_spkr=	3<<14,
	Cboe=	1<<7,
	Cbreset=	1<<6,

	/* porN */
	Rport8=	0<<6,
	Rport16=	1<<6,
	Rmtype=	7<<3,	/* memory type field */
	 Rmem=	0<<3,	/* common memory space */
	 Rattrib=	2<<3,	/* attribute space */
	 Rio=		3<<3,
	 Rdma=	4<<3,	/* normal DMA */
	 Rdmalx=	5<<3,	/* DMA, last transaction */
	 RA22_23= 6<<3,	/* ``drive A22 and A23 signals on CE2 and CE1'' */
	RslotB=	1<<2,	/* select slot B (always, on MPC823) */
	Rwp=	1<<1,	/* write protect */
	Rvalid=	1<<0,	/* region valid */

	Nmap=		8,		/* max number of maps to use */

	/*
	 *  configuration registers - they start at an offset in attribute
	 *  memory found in the CIS.
	 */
	Rconfig=	0,
	 Creset=	 (1<<7),	/*  reset device */
	 Clevel=	 (1<<6),	/*  level sensitive interrupt line */
	Rccsr=	2,
	 Ciack	= (1<<0),
	 Cipend	= (1<<1),
	 Cpwrdown=	(1<<2),
	 Caudioen=	(1<<3),
	 Ciois8=	(1<<5),
	 Cchgena=	(1<<6),
	 Cchange=	(1<<7),
	Rpin=	4,	/* pin replacement register */
	Rscpr=	6,	/* socket and copy register */
	Riob0=	10,
	Riob1=	12,
	Riob2=	14,
	Riob3=	16,
	Riolim=	18,

	Maxctab=	8,		/* maximum configuration table entries */
	MaxCIS = 8192,		/* maximum CIS size in bytes */
	Mgran = 8192,		/* maximum size of reads and writes */
};

typedef struct Ctlr Ctlr;
typedef struct Slot	Slot;
typedef struct Conftab	Conftab;
typedef struct IOport IOport;

/*
 * Map between physical memory space and PCMCIA card memory space.
 */
typedef struct PCMmap PCMmap;
struct PCMmap {
	ulong	ca;			/* card address */
	ulong	cea;			/* card end address */
	uchar*	isa;			/* kernel's emulated ISA address */
	int	len;			/* length of the ISA area */
	int	attr;			/* attribute memory */
	int	slotno;			/* owning slot */
	int	ref;
};

/* a controller */
struct Ctlr
{
	int	dev;
	int	nslot;

	/* memory maps */
	Lock	mlock;		/* lock down the maps */
	PCMmap	mmap[Nmap];	/* maps */

	/* IO port allocation */
	ulong	nextport;
};
static Ctlr *controller[Maxctlr];
static int ncontroller;

struct IOport
{
	ushort	port;	/* ISA port address */
	uchar	nlines;	/* number of io address lines */
	uchar	bit16;	/* true for 16 bit access */
	int	iolen;
	PCMmap*	map;
};

/* configuration table entry */
struct Conftab
{
	int	index;
	ushort	irqs;		/* legal irqs */
	uchar	irqtype;
	IOport	ports[2];
	int	nport;
	int	vcc;
	int	vpp1;
	int	vpp2;
	uchar	memwait;
	ulong	maxwait;
	ulong	readywait;
	ulong	otherwait;
};

/* a card slot */
struct Slot
{
	Lock;
	int	ref;

	Ctlr	*cp;		/* controller for this slot */
	long	memlen;		/* memory length */
	uchar	slotno;		/* slot number */

	/* status */
	uchar	special;	/* in use for a special device */
	uchar	already;	/* already inited */
	uchar	occupied;
	uchar	battery;
	uchar	wrprot;
	uchar	powered;
	uchar	configed;
	uchar	enabled;
	uchar	busy;
	uchar	v3_3;

	/* cis info */
	char	verstr[512];	/* version string */
	uchar	cpresent;	/* config registers present */
	ulong	caddr;		/* relative address of config registers */
	int	nctab;		/* number of config table entries */
	Conftab	ctab[Maxctab];
	Conftab	*def;		/* default conftab */

	/* for walking through cis */
	int	cispos;		/* current position scanning cis */
	uchar	*cisbase;

	struct {
		void	(*f)(Ureg*, void*, int);
		void	*a;
	} notify;
	struct {
		void	(*f)(Ureg*, void*);
		void	*a;
	} intr;
};
static Slot	*slot;
static Slot	*lastslot;
static int	nslot;

static	Map	pcmmapv[Nmap+1];
static	RMap	pcmmaps = {"PCMCIA mappings"};

static void	cisread(Slot*);
static void	pcmciaintr(Ureg*, void*);
static void	pcmciareset(void);
static int	pcmio(int, ISAConf*);
static long	pcmread(int, int, void*, long, ulong);
static long	pcmwrite(int, int, void*, long, ulong);
static void	pcmunmap(Slot*, PCMmap*);

static void*	pcmmalloc(ulong, long);
static void	pcmfree(void*, long);

static void pcmciadump(Slot*);

/*
 *  get info about card
 */
static void
slotinfo(Slot *pp)
{
	ulong pipr;

	pipr = m->iomem->pipr;
	pp->v3_3 = (pipr&Cbvs1)!=0;
	pp->occupied = (pipr&(Cbcd1|Cbcd2))==0;
	pp->powered = pcmpowered(pp->slotno);
	pp->battery = (pipr & (Cbbvd1|Cbbvd2))>>9;
	pp->wrprot = (pipr&Cbwp)!=0;
	pp->busy = (pipr&Cbrdy)==0;
}

/*
 *  enable the slot card
 */
static void
slotena(Slot *pp)
{
	IMM *io;

	if(pp->enabled)
		return;
	/* TO DO: power up and unreset, wait's are empirical (???) */
	pcmpower(pp->slotno, 1);
	eieio();
	delay(300);
	io = m->iomem;
	io->pgcrb |= Cbreset;	/* active high */
	eieio();
	delay(100);
	io->pgcrb &= ~Cbreset;
	eieio();
	delay(500);	/* ludicrous delay */

	/* get configuration */
	slotinfo(pp);
	if(pp->occupied){
		cisread(pp);
		pp->enabled = 1;
	} else{
		print("empty slot\n");
	}
}

/*
 *  disable the slot card
 */
static void
slotdis(Slot *pp)
{
	int i;
	PCMmap *pm;

	pcmpower(pp->slotno, 0);
	for(i = 0; i < nelem(pp->cp->mmap); i++){
		pm = &pp->cp->mmap[i];
		if(m->iomem->pcmr[i].option & Rvalid && pm->slotno == pp->slotno)
			pcmunmap(pp, pm);
	}
	pp->enabled = 0;
}

void
pcmintrenable(int slotno, void (*f)(Ureg*, void*), void *arg)
{
	Slot *pp;

	if(slotno < 0 || slotno >= nslot)
		panic("pcmintrenable");
	pp = slot+slotno;
	pp->intr.f = f;
	pp->intr.a = arg;
	m->iomem->per |= Cb_erdy_l | Cb_erdy_f;	/* assumes used for irq, not rdy */
}

/*
 *  status change interrupt
 *
 * BUG: this should probably wake a monitoring process
 * to read the CIS, rather than holding other interrupts out here.
 */
static void
pcmciaintr(Ureg *ur, void *a)
{
	ulong events;
	uchar csc, was;
	Slot *pp;

	USED(ur,a);
	if(slot == 0)
		return;

	events = m->iomem->pscr;
	eieio();
	m->iomem->pscr = events;
print("PCM: #%lux|", events);
	/*
	 * voltage change 1,2
	 * write protect change
	 * card detect 1,2
	 * battery voltage 1 change (or SPKR-bar)
	 * battery voltage 2 change (or STSCHG-bar)
	 * card B rdy / IRQ-bar low
	 * card B rdy / IRQ-bar high
	 * card B rdy / IRQ-bar rising edge
	 * card B rdy / IRQ-bar falling edge
	 */

	for(pp = slot; pp < lastslot; pp++){
		csc = m->iomem->pipr & (Cbcd1|Cbcd2);
		was = pp->occupied;
		slotinfo(pp);
		if(csc == 0 && was != pp->occupied){
			if(!pp->occupied){
				slotdis(pp);
				if(pp->special && pp->notify.f != nil)
					pp->notify.f(ur, pp->notify.a, 1);
			}
		}
		if(pp->occupied && (m->iomem->pipr & Cbrdy) == 0){ /* interrupt */
print("PCMI#%lux|", (m->iomem->pipr&0xFF00));
			if(pp->intr.f != nil)
				pp->intr.f(ur, pp->intr.a);
		}
	}
}

static uchar greycode[] = {
	0, 1, 3, 2, 6, 7, 5, 4, 014, 015, 017, 016, 012, 013, 011, 010,
	030, 031, 033, 032, 036, 037, 035, 034, 024, 025, 027
};

/*
 *  get a map for pc card region, return corrected len
 */
PCMmap*
pcmmap(int slotno, ulong offset, int len, int attr)
{
	Slot *pp;
	PCMmap *pm, *nm;
	IMM *io;
	int i;
	ulong e, bsize, code, opt;

	if(0)
		print("pcmmap: %d #%lux %d #%x\n", slotno, offset, len, attr);
	pp = slot + slotno;
	if(!pp->occupied)
		return nil;

	/* convert offset to granularity */
	if(len <= 0)
		len = 1;
	e = offset+len;
	for(i=0;; i++){
		if(i >= nelem(greycode))
			return nil;
		bsize = 1<<i;
		offset &= ~(bsize-1);
		if(e <= offset+bsize)
			break;
	}
	code = greycode[i];
	if(0)
		print("i=%d bsize=%lud code=0%luo\n", i, bsize, code);
	e = offset+bsize;
	len = bsize;

	lock(&pp->cp->mlock);

	/* look for an existing map that covers the right area */
	io = m->iomem;
	nm = nil;
	for(i=0; i<Nmap; i++){
		pm = &pp->cp->mmap[i];
		if(io->pcmr[i].option & Rvalid &&
		   pm->slotno == slotno &&
		   pm->attr == attr &&
		   offset >= pm->ca && e <= pm->cea){
			pm->ref++;
			unlock(&pp->cp->mlock);
			return pm;
		}
		if(nm == 0 && pm->ref == 0)
			nm = pm;
	}
	pm = nm;
	if(pm == nil){
		unlock(&pp->cp->mlock);
		return nil;
	}

	/* set up new map */
	pm->isa = pcmmalloc(offset, len);
	if(pm->isa == nil){
		/* address not available: in use, or too much to map */
		unlock(&pp->cp->mlock);
		return 0;
	}
	if(0)
		print("mx=%d isa=#%lux\n", (int)(pm - pp->cp->mmap), PADDR(pm->isa));

	pm->len = len;
	pm->ca = offset;
	pm->cea = pm->ca + pm->len;
	pm->attr = attr;
	i = pm - pp->cp->mmap;
	io->pcmr[i].option &= ~Rvalid;	/* disable map before changing it */
	io->pcmr[i].base = PADDR(pm->isa);
	opt = attr;
	opt |= code<<27;
	if((attr&Rmtype) == Rio){
		opt |= 4<<12;	/* PSST */
		opt |= 8<<7;	/* PSL */
		opt |= 2<<16;	/* PSHT */
	}else{
		opt |= 6<<12;	/* PSST */
		opt |= 24<<7;	/* PSL */
		opt |= 8<<16;	/* PSHT */
	}
	if((attr & Rport16) == 0)
		opt |= Rport8;
	if(pp->cp->nslot == 1 || slotno == 1)
		opt |= RslotB;
	io->pcmr[i].option = opt | Rvalid;
	pm->slotno = slotno;
	pm->ref = 1;

	unlock(&pp->cp->mlock);
	return pm;
}

static void
pcmiomap(Slot *pp, IOport *iop)
{
	int n, attr;

	if(0)
		print("pcm iomap #%x %d\n", iop->port, iop->iolen);
	if(iop->iolen <= 0)
		return;
	if(iop->port == 0){
		n = 1<<iop->nlines;
		lock(&pp->cp->mlock);
		if(pp->cp->nextport == 0)
			pp->cp->nextport = 0xF000;
		pp->cp->nextport = (pp->cp->nextport + n - 1) & ~(n-1);
		unlock(&pp->cp->mlock);
		iop->port = pp->cp->nextport;
		iop->iolen = n;
		pp->cp->nextport += n;
	}
	attr = Rio;
	if(iop->bit16)
		attr |= Rport16;
	iop->map = pcmmap(pp->slotno, iop->port, iop->iolen, attr);
}

static void
pcmunmap(Slot *pp, PCMmap* pm)
{
	int i;

	lock(&pp->cp->mlock);
	if(pp->slotno == pm->slotno && --pm->ref == 0){
		i = pm - pp->cp->mmap;
		m->iomem->pcmr[i].option = 0;
		m->iomem->pcmr[i].base = 0;
		pcmfree(pm->isa, pm->len);
	}
	unlock(&pp->cp->mlock);
}

static void
increfp(Slot *pp)
{
	lock(pp);
	if(pp->ref++ == 0)
		slotena(pp);
	unlock(pp);
}

static void
decrefp(Slot *pp)
{
	lock(pp);
	if(pp->ref-- == 1)
		slotdis(pp);
	unlock(pp);
}

/*
 *  look for a card whose version contains 'idstr'
 */
int
pcmspecial(char *idstr, ISAConf *isa)
{
	Slot *pp;
	extern char *strstr(char*, char*);

	pcmciareset();
	for(pp = slot; pp < lastslot; pp++){
		if(pp->special)
			continue;	/* already taken */
		increfp(pp);
		if(pp->occupied && strstr(pp->verstr, idstr)){
			print("Slot #%d: Found %s - ",pp->slotno, idstr);
			if(isa == 0 || pcmio(pp->slotno, isa) == 0){
				print("ok.\n");
				pp->special = 1;
				return pp->slotno;
			}
			print("error with isa io\n");
		}
		decrefp(pp);
	}
	return -1;
}

void
pcmspecialclose(int slotno)
{
	Slot *pp;

	if(slotno >= nslot)
		panic("pcmspecialclose");
	pp = slot + slotno;
	pp->special = 0;
	decrefp(pp);
}

void
pcmnotify(int slotno, void (*f)(Ureg*, void*, int), void* a)
{
	Slot *pp;

	if(slotno < 0 || slotno >= nslot)
		panic("pcmnotify");
	pp = slot + slotno;
	if(pp->occupied && pp->special){
		pp->notify.f = f;
		pp->notify.a = a;
	}
}

/*
 * reserve pcmcia slot address space [addr, addr+size[,
 * returning a pointer to it, or nil if the space was already reserved.
 */
static void *
pcmmalloc(ulong addr, long size)
{
	addr = rmapalloc(&pcmmaps, PCMCIAMEM+addr, size, size);
	if(addr == 0)
		return nil;
	return KADDR(addr);
}

static void
pcmfree(void *p, long size)
{
	if(p != nil && size > 0)
		mapfree(&pcmmaps, PADDR(p), size);
}

enum
{
	Qdir,
	Qmem,
	Qattr,
	Qctl,
};

#define SLOTNO(c)	((c->qid.path>>8)&0xff)
#define TYPE(c)		(c->qid.path&0xff)
#define QID(s,t)	(((s)<<8)|(t))

static int
pcmgen(Chan *c, Dirtab *tab, int ntab, int i, Dir *dp)
{
	int slotno;
	Qid qid;
	long len;
	Slot *pp;
	char name[NAMELEN];

	USED(tab, ntab);
	if(i>=3*nslot)
		return -1;
	slotno = i/3;
	pp = slot + slotno;
	len = 0;
	switch(i%3){
	case 0:
		qid.path = QID(slotno, Qmem);
		sprint(name, "pcm%dmem", slotno);
		len = pp->memlen;
		break;
	case 1:
		qid.path = QID(slotno, Qattr);
		sprint(name, "pcm%dattr", slotno);
		len = pp->memlen;
		break;
	case 2:
		qid.path = QID(slotno, Qctl);
		sprint(name, "pcm%dctl", slotno);
		break;
	}
	qid.vers = 0;
	devdir(c, qid, name, len, eve, 0660, dp);
	return 1;
}

static Ctlr*
pcmciaprobe(int dev)
{
	Ctlr *cp;

	cp = xalloc(sizeof(Ctlr));
	cp->dev = dev;
	cp->nslot = 1;

	/* TO DO: set low power mode? ... */
	/* TO DO: two slots except on 823 */
	controller[ncontroller++] = cp;
	return cp;
}

/*
 * used only when debugging
 */
static void
pcmciadump(Slot *)
{
	IMM *io;
	int i;

	io = m->iomem;
	print("pipr #%4.4lux pscr #%4.4lux per #%4.4lux pgcrb #%8.8lux\n",
		io->pipr & 0xFFFF, io->pscr & 0xFFFF, io->per & 0xFFFF, io->pgcrb);
	for(i=0; i<8; i++)
		print("pbr%d #%8.8lux por%d #%8.8lux\n", i, io->pcmr[i].base, i, io->pcmr[i].option);
}

/*
 *  set up for slot cards
 */
static void
pcmciareset(void)
{
	static int already;
	int i, j;
	Ctlr *cp;
	IMM *io;
	Slot *pp;

	if(already)
		return;
	already = 1;

	cp = controller[0];

	mapinit(&pcmmaps, pcmmapv, sizeof(pcmmapv));
	mapfree(&pcmmaps, PCMCIAMEM, PCMCIALEN);

	io = m->iomem;

	for(i=0; i<8; i++){
		io->pcmr[i].option = 0;
		io->pcmr[i].base = 0;
	}

	io->pgcrb = (1<<(31-PCMCIAlevel)) | (1<<(23-PCMCIAlevel));
	io->pscr = ~0;	/* reset status */
	io->per = 0xFE00;	/* enable interrupts */
	/* TO DO: Cboe, Cbreset */

	pcmenable();

	/* look for controllers */
	pcmciaprobe(1);
	for(i = 0; i < ncontroller; i++)
		nslot += controller[i]->nslot;
	slot = xalloc(nslot * sizeof(Slot));

	/* TO DO: if the card is there turn on 5V power to keep its battery alive */
	lastslot = slot;
	for(i = 0; i < ncontroller; i++){
		cp = controller[i];
		for(j = 0; j < cp->nslot; j++){
			pp = lastslot++;
			pp->slotno = pp - slot;
			pp->memlen = 64*MB;
			pp->cp = cp;
			//slotdis(pp);
		}
	}
	if(0)
		pcmciadump(slot);
	intrenable(PCMCIAlevel, pcmciaintr, cp, BUSUNKNOWN);
	print("pcmcia reset\n");
}

static Chan*
pcmciaattach(char *spec)
{
	return devattach('y', spec);
}

static int
pcmciawalk(Chan *c, char *name)
{
	return devwalk(c, name, 0, 0, pcmgen);
}

static void
pcmciastat(Chan *c, char *db)
{
	devstat(c, db, 0, 0, pcmgen);
}

static Chan*
pcmciaopen(Chan *c, int omode)
{
	if(c->qid.path == CHDIR){
		if(omode != OREAD)
			error(Eperm);
	} else
		increfp(slot + SLOTNO(c));
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

static void
pcmciaclose(Chan *c)
{
	if(c->flag & COPEN)
		if(c->qid.path != CHDIR)
			decrefp(slot+SLOTNO(c));
}

/* a memmove using only bytes */
static void
memmoveb(uchar *to, uchar *from, int n)
{
	while(n-- > 0)
		*to++ = *from++;
}

static long
pcmread(int slotno, int attr, void *a, long n, ulong offset)
{
	int i, len;
	PCMmap *m;
	void *ka;
	uchar *ac;
	Slot *pp;

	pp = slot + slotno;
	if(pp->memlen < offset)
		return 0;
	if(pp->memlen < offset + n)
		n = pp->memlen - offset;

	ac = a;
	for(len = n; len > 0; len -= i){
		if((i = len) > Mgran)
			i = Mgran;
		m = pcmmap(pp->slotno, offset, i, attr? Rattrib: Rmem);
		if(m == 0)
			error("can't map PCMCIA card");
		if(waserror()){
			if(m)
				pcmunmap(pp, m);
			nexterror();
		}
		if(offset + len > m->cea)
			i = m->cea - offset;
		else
			i = len;
		ka = m->isa + offset - m->ca;
		memmoveb(ac, ka, i);
		poperror();
		pcmunmap(pp, m);
		offset += i;
		ac += i;
	}

	return n;
}

static long
pcmciaread(Chan *c, void *a, long n, ulong offset)
{
	char *cp, *buf;
	ulong p;
	Slot *pp;

	p = TYPE(c);
	switch(p){
	case Qdir:
		return devdirread(c, a, n, 0, 0, pcmgen);
	case Qmem:
	case Qattr:
		return pcmread(SLOTNO(c), p==Qattr, a, n, offset);
	case Qctl:
		buf = malloc(READSTR);
		if(buf == nil)
			error(Enomem);
		if(waserror()){
			free(buf);
			nexterror();
		}
		cp = buf;
		pp = slot + SLOTNO(c);
		if(pp->occupied)
			cp += sprint(cp, "occupied\n");
		if(pp->enabled)
			cp += sprint(cp, "enabled\n");
		if(pp->powered)
			cp += sprint(cp, "powered\n");
		if(pp->configed)
			cp += sprint(cp, "configed\n");
		if(pp->wrprot)
			cp += sprint(cp, "write protected\n");
		if(pp->busy)
			cp += sprint(cp, "busy\n");
		if(pp->v3_3)
			cp += sprint(cp, "3.3v ok\n");
		cp += sprint(cp, "battery lvl %d\n", pp->battery);
		/* TO DO: could return pgcrb values for debugging */
		*cp = 0;
		n = readstr(offset, a, n, buf);
		poperror();
		free(buf);
		break;
	default:
		n=0;
		break;
	}
	return n;
}

static long
pcmwrite(int dev, int attr, void *a, long n, ulong offset)
{
	int i, len;
	PCMmap *m;
	void *ka;
	uchar *ac;
	Slot *pp;

	pp = slot + dev;
	if(pp->memlen < offset)
		return 0;
	if(pp->memlen < offset + n)
		n = pp->memlen - offset;

	ac = a;
	for(len = n; len > 0; len -= i){
		if((i = len) > Mgran)
			i = Mgran;
		m = pcmmap(pp->slotno, offset, i, attr? Rattrib: Rmem);
		if(m == 0)
			error("can't map PCMCIA card");
		if(waserror()){
			if(m)
				pcmunmap(pp, m);
			nexterror();
		}
		if(offset + len > m->cea)
			i = m->cea - offset;
		else
			i = len;
		ka = m->isa + offset - m->ca;
		memmoveb(ka, ac, i);
		poperror();
		pcmunmap(pp, m);
		offset += i;
		ac += i;
	}

	return n;
}

static long
pcmciawrite(Chan *c, void *a, long n, ulong offset)
{
	ulong p;
	Slot *pp;
	char buf[32];

	p = TYPE(c);
	switch(p){
	case Qctl:
		if(n >= sizeof(buf))
			n = sizeof(buf) - 1;
		strncpy(buf, a, n);
		buf[n] = 0;
		pp = slot + SLOTNO(c);
		if(!pp->occupied)
			error(Eio);

		/* set vpp on card */
		if(strncmp(buf, "vpp", 3) == 0){
			p = strtol(buf+3, nil, 0);
			pcmsetvpp(pp->slotno, p);
		}
		break;
	case Qmem:
	case Qattr:
		pp = slot + SLOTNO(c);
		if(pp->occupied == 0 || pp->enabled == 0)
			error(Eio);
		n = pcmwrite(pp->slotno, p == Qattr, a, n, offset);
		if(n < 0)
			error(Eio);
		break;
	default:
		error(Ebadusefd);
	}
	return n;
}

Dev pcmciadevtab = {
	'y',
	"pcmcia",

	pcmciareset,
	devinit,
	pcmciaattach,
	devdetach,
	devclone,
	pcmciawalk,
	pcmciastat,
	pcmciaopen,
	devcreate,
	pcmciaclose,
	pcmciaread,
	devbread,
	pcmciawrite,
	devbwrite,
	devremove,
	devwstat,
};

/*
 *  configure the Slot for IO.  We assume very heavily that we can read
 *  configuration info from the CIS.  If not, we won't set up correctly.
 */
static int
pcmio(int slotno, ISAConf *isa)
{
	Slot *pp;
	Conftab *ct, *et, *t;
	PCMmap *pm;
	uchar *p;
	int irq, i, x;

	irq = isa->irq;
	if(irq == 2)
		irq = 9;

	if(slotno > nslot)
		return -1;
	pp = slot + slotno;

	if(!pp->occupied)
		return -1;

	et = &pp->ctab[pp->nctab];

	/* assume default is right */
	if(pp->def)
		ct = pp->def;
	else
		ct = pp->ctab;
	/* try for best match */
	if(ct->ports[0].nlines == 0 || ct->ports[0].port != isa->port || ((1<<irq) & ct->irqs) == 0){
		for(t = pp->ctab; t < et; t++)
			if(t->ports[0].nlines && t->ports[0].port == isa->port && ((1<<irq) & t->irqs)){
				ct = t;
				break;
			}
	}
	if(ct->ports[0].nlines == 0 || ((1<<irq) & ct->irqs) == 0){
		for(t = pp->ctab; t < et; t++)
			if(t->ports[0].nlines && ((1<<irq) & t->irqs)){
				ct = t;
				break;
			}
	}
	if(ct->ports[0].nlines == 0){
		for(t = pp->ctab; t < et; t++)
			if(t->ports[0].nlines){
				ct = t;
				break;
			}
	}
print("slot %d: nlines=%d iolen=%d irq=%d ct->index=%d nport=%d ct->port=#%ux/%lux\n", slotno, ct->ports[0].nlines, ct->ports[0].iolen, irq, ct->index, ct->nport, ct->ports[0].port, isa->port);
	if(ct == et || ct->ports[0].nlines == 0)
		return -1;
	/* route interrupts */
	isa->irq = irq;
	//wrreg(pp, Rigc, irq | Fnotreset | Fiocard);
	delay(2);

	/* set power and enable device */
	pcmsetvcc(pp->slotno, ct->vcc);
	pcmsetvpp(pp->slotno, ct->vpp1);

	delay(2);	/* could poll BSY during power change */

	for(i=0; i<ct->nport; i++)
		pcmiomap(pp, &ct->ports[i]);

	if(ct->nport)
		isa->port = ct->ports[0].port;

	/* only touch Rconfig if it is present */
	if(pp->cpresent & (1<<Rconfig)){
print("Rconfig present: #%lux\n", pp->caddr+Rconfig);
		/*  Reset adapter */
		pm = pcmmap(slotno, pp->caddr + Rconfig, 1, Rattrib);
		if(pm == nil)
			return -1;

		p = pm->isa + pp->caddr + Rconfig - pm->ca;

		/* set configuration and interrupt type */
		x = ct->index;
		if((ct->irqtype & 0x20) && ((ct->irqtype & 0x40)==0 || isa->irq>7))
			x |= Clevel;
		*p = x;
		delay(5);

		pcmunmap(pp, pm);
print("Adapter reset\n");
	}

	return 0;
}

/*
 *  read and crack the card information structure enough to set
 *  important parameters like power
 */
static void	tcfig(Slot*, int);
static void	tentry(Slot*, int);
static void	tvers1(Slot*, int);
static void tvers2(Slot*, int);

static void (*parse[256])(Slot*, int) =
{
[0x15]	tvers1,
[0x1A]	tcfig,
[0x1B]	tentry,
[0x40]	tvers2,
};

static int
readc(Slot *pp, uchar *x)
{
	if(pp->cispos >= MaxCIS)
		return 0;
	*x = pp->cisbase[2*pp->cispos];
	pp->cispos++;
	return 1;
}

static void
cisread(Slot *pp)
{
	uchar link;
	uchar type;
	int this, i;
	PCMmap *m;

	memset(pp->ctab, 0, sizeof(pp->ctab));
	pp->caddr = 0;
	pp->cpresent = 0;
	pp->configed = 0;
	pp->nctab = 0;
	m = pcmmap(pp->slotno, 0, MaxCIS*2, Rattrib);
	if(m == 0)
		return;
	pp->cisbase = m->isa;
	pp->cispos = 0;

	/* loop through all the tuples */
	for(i = 0; i < 1000; i++){
		this = pp->cispos;
		if(readc(pp, &type) != 1)
			break;
		if(type == 0xFF)
			break;
		if(readc(pp, &link) != 1)
			break;
		if(parse[type])
			(*parse[type])(pp, type);
		if(link == 0xff)
			break;
		pp->cispos = this + (2+link);
	}
	pcmunmap(pp, m);
}

static ulong
getlong(Slot *pp, int size)
{
	uchar c;
	int i;
	ulong x;

	x = 0;
	for(i = 0; i < size; i++){
		if(readc(pp, &c) != 1)
			break;
		x |= c<<(i*8);
	}
	return x;
}

static void
tcfig(Slot *pp, int ttype)
{
	uchar size, rasize, rmsize;
	uchar last;

	USED(ttype);
	if(readc(pp, &size) != 1)
		return;
	rasize = (size&0x3) + 1;
	rmsize = ((size>>2)&0xf) + 1;
	if(readc(pp, &last) != 1)
		return;
	pp->caddr = getlong(pp, rasize);
	pp->cpresent = getlong(pp, rmsize);
}

static ulong vexp[8] =
{
	1, 10, 100, 1000, 10000, 100000, 1000000, 10000000
};
static ulong vmant[16] =
{
	10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, 90,
};

static ulong
microvolt(Slot *pp)
{
	uchar c;
	ulong microvolts;
	ulong exp;

	if(readc(pp, &c) != 1)
		return 0;
	exp = vexp[c&0x7];
	microvolts = vmant[(c>>3)&0xf]*exp;
	while(c & 0x80){
		if(readc(pp, &c) != 1)
			return 0;
		switch(c){
		case 0x7d:
			break;		/* high impedence when sleeping */
		case 0x7e:
		case 0x7f:
			microvolts = 0;	/* no connection */
			break;
		default:
			exp /= 10;
			microvolts += exp*(c&0x7f);
		}
	}
	return microvolts;
}

static ulong
nanoamps(Slot *pp)
{
	uchar c;
	ulong nanoamps;

	if(readc(pp, &c) != 1)
		return 0;
	nanoamps = vexp[c&0x7]*vmant[(c>>3)&0xf];
	while(c & 0x80){
		if(readc(pp, &c) != 1)
			return 0;
		if(c == 0x7d || c == 0x7e || c == 0x7f)
			nanoamps = 0;
	}
	return nanoamps;
}

/*
 *  only nominal voltage is important for config
 */
static ulong
power(Slot *pp)
{
	uchar feature;
	ulong mv;

	mv = 0;
	if(readc(pp, &feature) != 1)
		return 0;
	if(feature & 1)
		mv = microvolt(pp);
	if(feature & 2)
		microvolt(pp);
	if(feature & 4)
		microvolt(pp);
	if(feature & 8)
		nanoamps(pp);
	if(feature & 0x10)
		nanoamps(pp);
	if(feature & 0x20)
		nanoamps(pp);
	if(feature & 0x40)
		nanoamps(pp);
	return mv/1000000;
}

static ulong mantissa[16] =
{ 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, };

static ulong exponent[8] =
{ 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, };

static ulong
ttiming(Slot *pp, int scale)
{
	uchar unscaled;
	ulong nanosecs;

	if(readc(pp, &unscaled) != 1)
		return 0;
	nanosecs = (mantissa[(unscaled>>3)&0xf]*exponent[unscaled&7])/10;
	nanosecs = nanosecs * vexp[scale];
	return nanosecs;
}

static void
timing(Slot *pp, Conftab *ct)
{
	uchar c, i;

	if(readc(pp, &c) != 1)
		return;
	i = c&0x3;
	if(i != 3)
		ct->maxwait = ttiming(pp, i);		/* max wait */
	i = (c>>2)&0x7;
	if(i != 7)
		ct->readywait = ttiming(pp, i);		/* max ready/busy wait */
	i = (c>>5)&0x7;
	if(i != 7)
		ct->otherwait = ttiming(pp, i);		/* reserved wait */
}

static void
iospaces(Slot *pp, Conftab *ct)
{
	IOport *iop;
	uchar c;
	int i, bit16, nlines;

	if(readc(pp, &c) != 1)
		return;
	nlines = c&0x1f;
	bit16 = ((c>>5)&3) >= 2;
	if((c & 0x80) == 0) {
		if(ct->nport < nelem(ct->ports)){
			iop = &ct->ports[ct->nport++];
			iop->port = 0;
			iop->iolen = 0;
			iop->nlines = nlines;
			iop->bit16 = bit16;
		}
		return;
	}

	if(readc(pp, &c) != 1)
		return;
	for(i = (c&0xf)+1; i; i--){
		if(ct->nport < nelem(ct->ports)){
			iop = &ct->ports[ct->nport++];
			iop->bit16 = bit16;
			iop->nlines = nlines;
			iop->port = getlong(pp, (c>>4)&0x3);
			iop->iolen = getlong(pp, (c>>6)&0x3);
		}
	}
}

static void
irq(Slot *pp, Conftab *ct)
{
	uchar c;

	if(readc(pp, &c) != 1)
		return;
	ct->irqtype = c & 0xe0;
	if(c & 0x10)
		ct->irqs = getlong(pp, 2);
	else
		ct->irqs = 1<<(c&0xf);
	ct->irqs &= 0xDEB8;		/* levels available to card */
}

static void
memspace(Slot *pp, int asize, int lsize, int host)
{
	ulong haddress, address, len;

	len = getlong(pp, lsize)*256;
	address = getlong(pp, asize)*256;
	USED(len, address);
	if(host){
		haddress = getlong(pp, asize)*256;
		USED(haddress);
	}
}

static void
tentry(Slot *pp, int ttype)
{
	uchar c, i, feature;
	Conftab *ct;

	USED(ttype);

	if(pp->nctab >= Maxctab)
		return;
	if(readc(pp, &c) != 1)
		return;
	ct = &pp->ctab[pp->nctab++];

	/* copy from last default config */
	if(pp->def)
		*ct = *pp->def;

	ct->index = c & 0x3f;

	/* is this the new default? */
	if(c & 0x40){
		pp->def = ct;
		ct->nport = 0;
	}

	/* memory wait specified? */
	if(c & 0x80){
		if(readc(pp, &i) != 1)
			return;
		if(i&0x80)
			ct->memwait = 1;
	}

	if(readc(pp, &feature) != 1)
		return;
	switch(feature&0x3){
	case 1:
		ct->vcc = power(pp);
		ct->vpp1 = ct->vpp2 = -1;
		break;
	case 2:
		ct->vcc = power(pp);
		ct->vpp1 = power(pp);
		ct->vpp2 = -1;
		break;
	case 3:
		ct->vcc = power(pp);
		ct->vpp1 = power(pp);
		ct->vpp2 = power(pp);
		break;
	default:
		break;
	}
	if(feature&0x4)
		timing(pp, ct);
	if(feature&0x8)
		iospaces(pp, ct);
	if(feature&0x10)
		irq(pp, ct);
	switch((feature>>5)&0x3){
	case 1:
		memspace(pp, 0, 2, 0);
		break;
	case 2:
		memspace(pp, 2, 2, 0);
		break;
	case 3:
		if(readc(pp, &c) != 1)
			return;
		for(i = 0; i <= (c&0x7); i++)
			memspace(pp, (c>>5)&0x3, (c>>3)&0x3, c&0x80);
		break;
	}
	pp->configed++;
}

static void
tvers1(Slot *pp, int ttype)
{
	uchar c, major, minor;
	int  i;

	USED(ttype);
	if(readc(pp, &major) != 1)
		return;
	if(readc(pp, &minor) != 1)
		return;
	for(i = 0; i < sizeof(pp->verstr)-1; i++){
		if(readc(pp, &c) != 1)
			return;
		if(c == 0)
			c = '\n';
		if(c == 0xff)
			break;
		pp->verstr[i] = c;
	}
	pp->verstr[i] = 0;
}

static void
tvers2(Slot *pp, int ttype)
{
	uchar c;
	int  i, j;

	USED(ttype);
	if(pp->verstr[0])
		return;
	pp->cispos += 9;	/* vers, comply, dindex[2], x,x, spec8, spec9, ndhr*/
	for(j=0; j<2; j++) {
		for(i = 0; i < sizeof(pp->verstr)-1; i++){
			if(readc(pp, &c) != 1)
				return;
			if(c == 0)
				c = '\n';
			if(c == 0xff)
				break;
			pp->verstr[i] = c;
		}
		pp->verstr[i++] = '\n';
		pp->verstr[i] = 0;
	}
}
