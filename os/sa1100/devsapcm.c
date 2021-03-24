#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/error.h"

int pcmdebug=0;
#define DPRINT if(pcmdebug)print
#define DPRINT1 if(pcmdebug > 1)print
#define DPRINT2 if(pcmdebug > 2)print
#define PCMERR(x)	pce(x);

#define CS3REG	(ulong*)(0x18000000)

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

/*
 *  Support for 2 card slots usng StrongArm pcmcia support.
 *
 */
enum
{
	bCLK=		4,
};

enum
{
	/*
	 *  configuration registers - they start at an offset in attribute
	 *  memory found in the CIS.
	 */
	Rconfig=	0,
	 Creset=	 (1<<7),	/*  reset device */
	 Clevel=	 (1<<6),	/*  level sensitive interrupt line */

};


enum	{
	Maxctab=	8,	/* maximum configuration table entries */
	Maxslot=	2
};

typedef struct Slot	Slot;
typedef struct Conftab	Conftab;


/* configuration table entry */
struct Conftab
{
	int	index;
	ushort	irqs;		/* legal irqs */
	ushort	port;		/* port address */
	uchar	irqtype;
	uchar	nioregs;	/* number of io registers */
	uchar	bit16;		/* true for 16 bit access */
	uchar	vcc;
	uchar	vpp1;
	uchar	vpp2;
	uchar	memwait;
	ulong	maxwait;
	ulong	readywait;
	ulong	otherwait;
};

/* a card slot */
struct Slot
{
	Lock;
	Label;
	int	ref;

	long	memlen;		/* memory length */
	uchar	base;		/* index register base */
	uchar	slotno;		/* slot number */

	/* status */
	uchar	special;	/* in use for a special device */
	uchar	occupied;
	uchar	battery;	/* XXX always zero? */
	uchar	wrprot;
	uchar	powered;
	uchar	configed;
	uchar	enabled;
	uchar	dsize;
	uchar	busy;

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

	struct	{
		void (*f)(Ureg*, void*);
		void* a;
	} devint;
	struct {
		void (*f)(Ureg*, void*, int);
		void *a;
	} notify;
	uchar	irqtype;
};

static Slot	*slot;
static Slot	*lastslot;

static void	slotdis(Slot *);
static void	cisinit(Slot*);
static void	setbuswait(Slot *, int, int);
static void	sapcmintr(Ureg*, void*);
static void	sapcmreset(void);
static int	pcmio(int, ISAConf*);
static long	pcmread(int, int, void*, long, ulong);
static long	pcmwrite(int, int, void*, long, ulong);

static void sapcmdump(Slot*);

static ulong GPIOrdy[2];
static ulong GPIOeject[2];
static ulong GPIOmask;
static ulong GPIOall[2];

/*
 *  get info about card
 */
static void
slotinfo(Slot *pp)
{
	ulong gplr;
	int was;

	gplr = *GPLR;
	was = pp->occupied;
	pp->occupied = (gplr & GPIOeject[pp->slotno]) ? 0 : 1;
	pp->busy = (gplr & GPIOrdy[pp->slotno]) ? 0 : 1;
	pp->powered = pcmpowered(pp->slotno);
	pp->battery = 0;
	pp->wrprot = 0;
	if (!was & pp->occupied)
		print("PCMCIA card %d inserted\n", pp->slotno);
	if (was & !pp->occupied)
		print("PCMCIA card %d removed!\n", pp->slotno);
}

/*
 *  enable the slot card
 */
static void
slotena(Slot *pp)
{
	if(pp->enabled)
		return;
	DPRINT("Enable slot# %d\n", pp->slotno);
	pcmpower(pp->slotno, 1);

	/* get configuration */
	slotinfo(pp);
	if(pp->occupied){
		if (pp->powered == 3)
			setbuswait(pp, Qattr, 600);
		else
			setbuswait(pp, Qattr, 250);
		cisinit(pp);
		pp->enabled = 1;
	} else {
		slotdis(pp);
	}
}

/*
 *  disable the slot card
 */
static void
slotdis(Slot *pp)
{
	pcmpower(pp->slotno, 0);
	if (pp->enabled)
		DPRINT("Disable slot# %d\n", pp->slotno);
	pp->enabled = 0;
}

/*
 *  status change interrupt
 */
static void
sapcmintr(Ureg *ur, void *a)
{
	uchar was;
	Slot *pp;
	ulong gedr;

	USED(ur,a);

	gedr = *GEDR;
	DPRINT("sapcm intr, GEDR: %lux GPLR: %lux GPDR %lux GAFR %lux\n",
		gedr, *GPLR, *GPDR, *GAFR);
	gedr &= GPIOmask;
	*GEDR = gedr;	/* Clear interrupts */
	if(slot == 0)
		return;

	for(pp = slot; pp < lastslot; pp++){
		was = pp->occupied;
		slotinfo(pp);
		if(!pp->occupied)
		{
			if(was != pp->occupied){
				slotdis(pp);
				if (pp->special && pp->notify.f)
					(*pp->notify.f)(ur, pp->notify.a, 1);
			}
		}
	}
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

	sapcmreset();
	for(pp = slot; pp < lastslot; pp++){
		if(pp->special)
			continue;	/* already taken */
		increfp(pp);

		if(pp->occupied)
		if(strstr(pp->verstr, idstr))
		{
			DPRINT("Slot #%d: Found %s - ",pp->slotno, idstr);
			if(isa == 0 || pcmio(pp->slotno, isa) == 0){
				DPRINT("ok.\n");
				pp->special = 1;
				isa->sairq = pcmpin(pp->slotno, PCMready);
				return pp->slotno;
			}
			print("error with isa io for %s\n", idstr);
		}
		decrefp(pp);
	}
	return -1;
}

void
pcmspecialclose(int slotno)
{
	Slot *pp;

	if((slotno < 0) || (slotno >= Maxslot))
		panic("pcmspecialclose");
	pp = slot + slotno;
	pp->special = 0;	/* Is this OK ? */
	*GFER &= ~GPIOrdy[pp->slotno];
	*GRER &= ~GPIOrdy[pp->slotno];
	decrefp(pp);
}

void
pcmnotify(int slotno, void (*f)(Ureg*, void*, int), void* a)
{
	Slot *pp;

	if((slotno < 0) || (slotno >= Maxslot))
		panic("pcmnotify");
	pp = slot + slotno;
	if (pp->special)
	{
		pp->notify.f = f;
		pp->notify.a = a;
	}
}

/*
 * Field interrupts to PCMCIA slot drivers.
 */
static void
OLDpcmdevintr(Ureg *ur, void *s)
{
	Slot *pp = s;

	*GEDR = GPIOrdy[pp->slotno];
	(pp->devint.f)(ur, pp->devint.a);
}

void
OLDpcmciaintrenable(int slotno, void (*f)(Ureg*, void*), void* a)
{
	ulong	b;
	Slot	*pp;

	if((slotno < 0) || (slotno >= Maxslot))
		panic("pcmciaintrenable");
	b = GPIOrdy[slotno];
	*GFER |= b;
	*GRER &= ~b;
	*GPDR &= ~b;
	pp = slot + slotno;
	pp->devint.f = f;
	pp->devint.a = a;
	DPRINT("pcmciaintrenable, slot=%d  f=%lux\n", slotno, (ulong)f);
//	intrenable(slotno ? GPIOrdy_1 : GPIOrdy_0, pcmdevintr, pp, BusGPIO);
}

static void
toggledbg()
{
	if (++pcmdebug > 3)
		pcmdebug = 0;
	print("pcmdebug=%d\n", pcmdebug);
}	

static int
pcmgen(Chan *c, Dirtab *tab, int ntab, int i, Dir *dp)
{
	int slotno;
	Qid qid;
	long len;
	Slot *pp;
	char name[NAMELEN];

	USED(tab, ntab);
	if(i>=3*Maxslot)
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

static void
sapcmdump(Slot *pp)
{
	USED(pp);
}

/*
 *  set up for slot cards
 */
static void
sapcmreset(void)
{
	static int already;
	int slotno, v;
	Slot *pp;

	if(already)
		return;
	already = 1;
	DPRINT("sapcm reset\n");

	slot = xalloc(Maxslot * sizeof(Slot));

	*MECR = (bCLK<<26)|(bCLK<<21)|(bCLK<<16)|
		(bCLK<<10)|(bCLK<<5)|(bCLK<<0);

	lastslot = slot;

	for(slotno = 0; slotno < Maxslot; slotno++){
		pp = lastslot++;
		pp->slotno = pp - slot;
		pp->memlen = 64*MB;
		GPIOeject[slotno] = (1<<pcmpin(slotno, PCMeject));
		GPIOrdy[slotno] = (1<<pcmpin(slotno, PCMready));
		GPIOall[slotno] = GPIOeject[slotno] | GPIOrdy[slotno];
		*GAFR &= ~GPIOall[slotno];
		*GPDR &= ~GPIOall[slotno];
		*GRER |= GPIOeject[slotno];
		*GFER |= GPIOeject[slotno];
		*GRER &= ~GPIOrdy[slotno];
		*GFER &= ~GPIOrdy[slotno];
		GPIOmask |= GPIOeject[slotno];
		slotdis(pp);
		intrenable(pcmpin(slotno, PCMeject), sapcmintr, 0, BusGPIO);
		if((v = pcmpin(slotno, PCMstschng)) >= 0) {	/* status change interrupt */
			intrenable(v, sapcmintr, 0, BusGPIO);
			GPIOmask |= (1<<v);
		}
	}
}

static Chan*
sapcmattach(char *spec)
{
	return devattach('y', spec);
}

static int
sapcmwalk(Chan *c, char *name)
{
	return devwalk(c, name, 0, 0, pcmgen);
}

static void
sapcmstat(Chan *c, char *db)
{
	devstat(c, db, 0, 0, pcmgen);
}

static Chan*
sapcmopen(Chan *c, int omode)
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
sapcmclose(Chan *c)
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
	Slot *pp;

	pp = slot + slotno;
	if(pp->memlen < offset)
		return 0;
	if(pp->memlen < offset + n)
		n = pp->memlen - offset;

	if(waserror()){
		nexterror();
	}

	if (attr)
	{
		int ii;
		uchar *b = (uchar *)a;
		ushort *p = (ushort *)PCMCIAAttr(slotno) + offset;

		for (ii = 0; ii < n; ii++)
			*b++ = *(uchar *)p++;
	}
	else
		memmoveb(a, (uchar *)PCMCIAMem(slotno) + offset, n);
	poperror();
	return n;
}

static long
sapcmread(Chan *c, void *a, long n, ulong offset)
{
	char *cp, buf[2048];
	ulong p;
	Slot *pp;
	int ii;

	p = TYPE(c);
	switch(p){
	case Qdir:
		return devdirread(c, a, n, 0, 0, pcmgen);
	case Qmem:
	case Qattr:
		return pcmread(SLOTNO(c), p==Qattr, a, n, offset);
	case Qctl:
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
		if(pp->special)
			cp += sprint(cp, "%d-bit data path\n", pp->dsize);
		if(pp->busy)
			cp += sprint(cp, "busy\n");
		if(pp->enabled && (ii = strlen(pp->verstr)))
			cp += sprint(cp, "verstr %d\n%s\n", ii, pp->verstr);
		cp += sprint(cp, "battery lvl %d\n", pp->battery);
		/* DUMP registers here */
		cp += sprint(cp, "mecr 0x%lux\n",
			(SLOTNO(c) ? *MECR >> 16 : *MECR) & 0x7fff);
/*
		if(brutus)
		cp += sprint(cp, "cs3bits 0x%x vccreq %d sts %d\n",
			(*CS3REG >> Cs3bits(SLOTNO(c))) & 0xff,
			(*CS3REG >> Vccreqbits(SLOTNO(c))) & Vccreqmsk,
			(*CS3REG & Stschg(SLOTNO(c))) ? 1 : 0);
*/
		*cp = 0;
		return readstr(offset, a, n, buf);
	default:
		n=0;
		break;
	}
	return n;
}

static long
pcmwrite(int slotno, int attr, void *a, long n, ulong offset)
{
	Slot *pp;

	pp = slot + slotno;
	if(pp->memlen < offset)
		return 0;
	if(pp->memlen < offset + n)
		n = pp->memlen - offset;

	if(waserror()){
		nexterror();
	}

	memmoveb((uchar *)(attr ? PCMCIAAttr(slotno) : PCMCIAMem(slotno)) + offset, a, n);

	poperror();
	return n;
}

static void
setbuswait(Slot *pp, int type, int wait)
{
	ulong mecr;
	int i;
	int sh;

	if (wait < 61)
		wait = 61;
	/* 6 times the cycle time (nanoseconds) */
	i = (10000 * 2 * 3) / (conf.cpuspeed / 100000);
	if (i > 0)
		i = (wait + i - 1) / i;
	else
		i = 0x1f;
	if (i > 0)
		i -= 1;
	else
		i = 1;

	sh = pp->slotno ? 16 : 0;

	switch (type) {
	case Qctl:
		sh += 0;
		break;
	case Qattr:
		sh += 5;
		break;
	case Qmem:
		sh += 10;
		break;
	}
	mecr = *MECR & ~(0x1f << sh);
	mecr |= i<<sh;
	*MECR = mecr;
	DPRINT("bus (%d/%d) wait set to %d (%d,%lux)\n", pp->slotno, type, wait, i, mecr);
}

static long
sapcmwrite(Chan *c, void *a, long n, ulong offset)
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

		if(strncmp(buf, "vpp", 3) == 0)
			pcmsetvpp(pp->slotno, atoi(buf+3));
		else if(strncmp(buf, "mecr", 4) == 0) {
			if(SLOTNO(c))
				*MECR = (*MECR & 0x7fff) | (strtol(buf+4, 0, 16) << 16);
			else
				*MECR = (*MECR & (0x7fff << 16)) | strtol(buf+4, 0, 16);
		}
		break;
	case Qmem:
	case Qattr:
		pp = slot + SLOTNO(c);
		if(pp->occupied == 0 || pp->enabled == 0)
			error(Eio);
		n = pcmwrite(SLOTNO(c), p == Qattr, a, n, offset);
		if(n < 0)
			error(Eio);
		break;
	default:
		error(Ebadusefd);
	}
	return n;
}

Dev sapcmdevtab = {
	'y',
	"sapcm",

	sapcmreset,
	devinit,
	sapcmattach,
	devdetach,
	devclone,
	sapcmwalk,
	sapcmstat,
	sapcmopen,
	devcreate,
	sapcmclose,
	sapcmread,
	devbread,
	sapcmwrite,
	devbwrite,
	devremove,
	devwstat,
};

/*
 *  configure the Slot for IO.  We assume very heavily that we can read
 *  configuration info from the CIS.  If not, we won't set up correctly.
 */

static int
pce(char *s)
{
	USED(s);
	DPRINT("pcmio failed: %s\n", s);
	return -1;
}

static int
pcmio(int slotno, ISAConf *isa)
{
	uchar x, *p;
	Slot *pp;
	Conftab *ct, *et, *t;
	int i, index, irq;
	char *cp;

	irq = isa->irq;
	if(irq == 2)
		irq = 9;

	if(slotno > Maxslot)
		return PCMERR("bad slot#");
	pp = slot + slotno;

	if(!pp->occupied)
		return PCMERR("empty slot");

	isa->port &= 0xffff;
	et = &pp->ctab[pp->nctab];

	ct = 0;
	for(i = 0; i < isa->nopt; i++){
		if(strncmp(isa->opt[i], "index=", 6))
			continue;
		index = strtol(&isa->opt[i][6], &cp, 0);
		if(cp == &isa->opt[i][6] || index >= pp->nctab)
			return PCMERR("bad index");
		ct = &pp->ctab[index];
	}
	if(ct == 0){
	
		/* assume default is right */
		if(pp->def)
			ct = pp->def;
		else
			ct = pp->ctab;
	
		/* try for best match */
		if(ct->nioregs == 0 || ct->port != isa->port || ((1<<irq) & ct->irqs) == 0){
			for(t = pp->ctab; t < et; t++)
				if(t->nioregs && t->port == isa->port && ((1<<irq) & t->irqs)){
					ct = t;
					break;
				}
		}
		if(ct->nioregs == 0 || ((1<<irq) & ct->irqs) == 0){
			for(t = pp->ctab; t < et; t++)
				if(t->nioregs && ((1<<irq) & t->irqs)){
					ct = t;
					break;
				}
		}
		if(ct->nioregs == 0){
			for(t = pp->ctab; t < et; t++)
				if(t->nioregs){
					ct = t;
					break;
				}
		}
	}

	if(ct == et || ct->nioregs == 0)
		return PCMERR("no valid ctab");
	if(isa->port == 0 && ct->port == 0)
		return PCMERR("no port");

	/* route interrupts */
	isa->irq = irq;

	DPRINT("pcmio: Slot #%d: vpp=0\n", slotno);
	pcmsetvpp(slotno, 0);
	DPRINT("pcmio: Slot #%d: vcc=%d\n", slotno, ct->vcc);
	pcmsetvcc(slotno, ct->vcc);
	DPRINT("pcmio: Slot #%d: vpp=%d\n", slotno, ct->vpp1);
	pcmsetvpp(slotno, ct->vpp1);
	setbuswait(pp, Qctl, 90);

	/* 16-bit data path */
	if(ct->bit16)
		pp->dsize = 16;
	else
		pp->dsize = 8;

	if(isa->port == 0)
		isa->port = ct->port;

	/* only touch Rconfig if it is present */
	if(pp->cpresent & (1<<Rconfig)){
		p = (uchar*)(PCMCIAAttr(slotno) + pp->caddr + Rconfig);

		/* set configuration and interrupt type */
		x = ct->index;
		if((ct->irqtype & 0x20) && ((ct->irqtype & 0x40)==0))
		{
			DPRINT("level\n");
			x |= Clevel;
		}
		else
			DPRINT("pulse\n");
		*p = x;
		delay(5);

	}
	isa->port += PCMCIAIO(slotno);
	return 0;
}

/*
 *  read and crack the card information structure enough to set
 *  important parameters like power
 */
static void	tcfig(int, uchar *);
static void	tentry(int, uchar *);
static void	tvers1(int, uchar *);
static void	tdefault(int, uchar *);

static void (*parse[256])(int, uchar *) =
{
[0x15]	tvers1,
[0x1A]	tcfig,
[0x1B]	tentry,
};

static uchar
readc(Slot *pp)
{
	uchar x;

	if(pp->cispos >= (1<<13))
	{
		DPRINT("readc: EOF!\n");
		gotolabel(pp);
	}
	x = pp->cisbase[2*pp->cispos];
	DPRINT2(" %.2ux", x);
	pp->cispos++;
	if (!(pp->cispos % 8))
		DPRINT2("\n");
	return(x);
}

static void
cisgen(int slotno, uchar *bp)
{
	uchar type = bp[0];

	if(parse[type])
		(*parse[type])(slotno, bp);
	else if (type != 0)
		tdefault(slotno, bp);
}

void
cisread(int slotno, void (*f)(int, uchar *))
{
	uchar buf[257];
	uchar link;
	uchar type;
	int this, i;
	Slot *pp = slot + slotno;

	pp->cispos = 0;
	if (!setlabel(pp))
	{
		/* loop through all the tuples */
		for(i = 0; i < 1000; i++) {
			int jj;

			this = pp->cispos;
			if (this >= (1<<13))
				break;
			DPRINT2("P: ");
			type = buf[0] = readc(pp);
			if (type == 0xFF)
				break;
			link = buf[1] = readc(pp);
			if ((this + 2 + link) > (1 << 13))
				link = (1 << 13) - this - 2;
			for (jj = 0; jj < link; jj++)
				buf[jj + 2] = readc(pp);
			DPRINT2("\n");

			(*f)(pp->slotno, buf);

			if(link == 0xff)
				break;
			pp->cispos = this + (2+link);
		}
	}
}

static void
cisinit(Slot *pp)
{
	DPRINT("cisinit slot# %d:", pp->slotno);
	memset(pp->ctab, 0, sizeof(pp->ctab));
	pp->verstr[0] = 0;
	pp->caddr = 0;
	pp->cpresent = 0;
	pp->configed = 0;
	pp->nctab = 0;
	pp->cisbase = (uchar *)PCMCIAAttr(pp->slotno);

	cisread(pp->slotno, cisgen);
	if(pp->cpresent & (1<<Rconfig))
	{
		/*  Reset card */
		*(uchar*)(PCMCIAAttr(pp->slotno) + pp->caddr + Rconfig) = Creset;
		delay(2);
		*(uchar*)(PCMCIAAttr(pp->slotno) + pp->caddr + Rconfig) = 0;
		delay(2);
	}
}

static ulong
getlong(uchar **bpp, int size)
{
	uchar c;
	int i;
	ulong x;

	x = 0;
	for(i = 0; i < size; i++){
		c = *(*bpp)++;
		x |= c<<(i*8);
	}
	return x;
}

static void
tdefault(int, uchar *bp)
{
	int link;

	if (pcmdebug)
	{
		print("type: %x\n", *bp++);
		link = *bp++;
		while (--link > 0)
		{
			print(" %.2ux", *bp++);
		}
		print("\n");
	}
}


static void
tcfig(int slotno, uchar *bp)
{
	uchar size, rasize, rmsize;
	Slot *pp = slot + slotno;

	bp += 2;	/* type & link */
	DPRINT("tcfig: ");
	size = *bp++;
	rasize = (size&0x3) + 1;
	rmsize = ((size>>2)&0xf) + 1;
	bp++;
	pp->caddr = getlong(&bp, rasize);
	pp->cpresent = getlong(&bp, rmsize);
	DPRINT("caddr=%lux, cpresent=%ux\n",
		pp->caddr, pp->cpresent);
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
microvolt(uchar **bpp)
{
	uchar c;
	ulong microvolts;
	ulong exp;

	c = *(*bpp)++;
	exp = vexp[c&0x7];
	microvolts = vmant[(c>>3)&0xf]*exp;
	while(c & 0x80){
		c = *(*bpp)++;
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
nanoamps(uchar **bpp)
{
	uchar c;
	ulong nanoamps;

	c = *(*bpp)++;
	nanoamps = vexp[c&0x7]*vmant[(c>>3)&0xf];
	while(c & 0x80){
		c = *(*bpp)++;
		if(c == 0x7d || c == 0x7e || c == 0x7f)
			nanoamps = 0;
	}
	return nanoamps;
}

/*
 *  only nominal voltage is important for config
 */
static ulong
power(uchar **bpp)
{
	uchar feature;
	ulong mv;

	mv = 0;
	feature = *(*bpp)++;
	if(feature & 1)
		mv = microvolt(bpp);
	if(feature & 2)
		microvolt(bpp);
	if(feature & 4)
		microvolt(bpp);
	if(feature & 8)
		nanoamps(bpp);
	if(feature & 0x10)
		nanoamps(bpp);
	if(feature & 0x20)
		nanoamps(bpp);
	if(feature & 0x40)
		nanoamps(bpp);
	return mv/1000000;
}

static ulong mantissa[16] =
{ 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, };

static ulong exponent[8] =
{ 1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, };

static ulong
ttiming(uchar **bpp, int scale)
{
	uchar unscaled;
	ulong nanosecs;

	DPRINT2(" t:");
	unscaled = *(*bpp)++;
	nanosecs = (mantissa[(unscaled>>3)&0xf]*exponent[unscaled&7])/10;
	nanosecs = nanosecs * vexp[scale];
	return nanosecs;
}

static void
timing(uchar **bpp, Conftab *ct)
{
	uchar c, i;

	DPRINT2(" T:");
	c = *(*bpp)++;
	i = c&0x3;
	if(i != 3)
		ct->maxwait = ttiming(bpp, i);		/* max wait */
	i = (c>>2)&0x7;
	if(i != 7)
		ct->readywait = ttiming(bpp, i);		/* max ready/busy wait */
	i = (c>>5)&0x7;
	if(i != 7)
		ct->otherwait = ttiming(bpp, i);		/* reserved wait */
	DPRINT("maxw %lud rdyw %lud othw %lud\n",
		ct->maxwait, ct->readywait, ct->otherwait);
}

static void
iospaces(uchar **bpp, Conftab *ct)
{
	uchar c;
	int i;
	ulong len;

	c = *(*bpp)++;
	ct->nioregs = 1<<(c&0x1f);
	ct->bit16 = ((c>>5)&3) >= 2;
	if((c & 0x80) == 0)
		return;

	c = *(*bpp)++;
	for(i = (c&0xf)+1; i; i--){
		ct->port = getlong(bpp, (c>>4)&0x3);
		len = getlong(bpp, (c>>6)&0x3);
		USED(len);
	}
}

static void
irq(uchar **bpp, Conftab *ct)
{
	uchar c;

	c = *(*bpp)++;
	ct->irqtype = c & 0xe0;
	if(c & 0x10)
		ct->irqs = getlong(bpp, 2);
	else
		ct->irqs = 1<<(c&0xf);
	// ct->irqs &= 0xDEB8;		/* levels available to card */
}

static void
memspace(uchar **bpp, int asize, int lsize, int host)
{
	ulong haddress, address, len;

	len = getlong(bpp, lsize)*256;
	address = getlong(bpp, asize)*256;
	USED(len, address);
	if(host){
		haddress = getlong(bpp, asize)*256;
		USED(haddress);
	}
}

static void
tentry(int slotno, uchar *bp)
{
	uchar c, i, feature;
	Conftab *ct;
	Slot *pp = slot + slotno;

	if(pp->nctab >= Maxctab)
		return;
	DPRINT2(" c:");
	bp += 2;	/* type & link */
	c = *bp++;
	ct = &pp->ctab[pp->nctab++];

	/* copy from last default config */
	if(pp->def)
		*ct = *pp->def;

	ct->index = c & 0x3f;

	/* is this the new default? */
	if(c & 0x40)
		pp->def = ct;

	/* memory wait specified? */
	if(c & 0x80){
		DPRINT2(" w:");
		i = *bp++;
		if(i&0x80)
			ct->memwait = 1;
	}

	DPRINT2(" f:");
	feature = *bp++;
	switch(feature&0x3){
	case 1:
		ct->vcc = power(&bp);
		break;
	case 2:
		ct->vcc = power(&bp);
		ct->vpp1 = ct->vpp2 = power(&bp);
		break;
	case 3:
		ct->vcc = power(&bp);
		ct->vpp1 = power(&bp);
		ct->vpp2 = power(&bp);
		break;
	default:
		break;
	}
	if(feature&0x4)
		timing(&bp, ct);
	if(feature&0x8)
		iospaces(&bp, ct);
	if(feature&0x10)
		irq(&bp, ct);
	switch((feature>>5)&0x3){
	case 1:
		memspace(&bp, 0, 2, 0);
		break;
	case 2:
		memspace(&bp, 2, 2, 0);
		break;
	case 3:
		c = *bp++;
		for(i = 0; i <= (c&0x7); i++)
			memspace(&bp, (c>>5)&0x3, (c>>3)&0x3, c&0x80);
		break;
	}
	pp->configed++;
}

static void
tvers1(int slotno, uchar *bp)
{
	int  i;
	uchar c;
	Slot *pp = slot + slotno;

	DPRINT("tvers1: ");
	bp += 2;	/* type & link */
	bp += 2;	/* major & minor */
	for(i = 0; i < sizeof(pp->verstr)-1; i++){
		c = *bp++;
		if(c == 0)
			c = '\n';
		if(c == 0xff)
			break;
		pp->verstr[i] = c;
	}
	pp->verstr[i] = 0;
	DPRINT("%s\n", pp->verstr);
}

#define USHORT_READ_WORKS_OK
#define USHORT_WRITE_WORKS_OK
void
inss(ulong p, void* buf, int ns)
{
#ifdef USHORT_READ_WORKS_OK
	ushort *addr;

	addr = (ushort*)buf;
	for(;ns > 0; ns--)
		*addr++ = _ins(p);
#else
	uchar *addr, *port;

	port = (uchar *)p;
	addr = (uchar *)buf;
	for (;ns > 0; ns--)
	{
		addr[0] = *port;
		addr[1] = *port;
		addr += 2;
	}
#endif
}

void
outss(ulong p, void* buf, int ns)
{
#ifdef USHORT_WRITE_WORKS_OK
	ushort *addr;

	addr = (ushort*)buf;
	for(;ns > 0; ns--)
		_outs(p, *addr++);
#else
	uchar *addr, *port;

	port = (uchar *)p;
	addr = (uchar *)buf;
	for (;ns > 0; ns--)
	{
		*port = addr[0];
		*port = addr[1];
		addr += 2;
	}
#endif
}
