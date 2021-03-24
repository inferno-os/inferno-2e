#include	"u.h"
#include	"lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"

#define DEBUG	0
#define DPRINT	if(DEBUG)print
#define IDEBUG	0
#define IDPRINT if(IDEBUG)print

typedef	struct Drive		Drive;
typedef	struct Ident		Ident;
typedef	struct Controller	Controller;

enum
{
	Pdata=		0,	/* data port (16 bits) */
	Perror=		1,	/* error port (read) */
	 Eabort=	(1<<2),
	Pfeature=	1,	/* buffer mode port (write) */
	Pcount=		2,	/* sector count port */
	Psector=	3,	/* sector number port */
	Pcyllsb=	4,	/* least significant byte cylinder # */
	Pcylmsb=	5,	/* most significant byte cylinder # */
	Pdh=		6,	/* drive/head port */
	 DHmagic=	0xA0,
	 DHslave=	0x10,
	Pstatus=	7,	/* status port (read) */
	 Sbusy=		 (1<<7),
	 Sready=	 (1<<6),
	 Sdrq=		 (1<<3),
	 Serr=		 (1<<0),
	Pcmd=		7,	/* cmd port (write) */

	Pctl=		2,	/* device control, alternate status */
	 nIEN=		(1<<1),
	 Srst=		(1<<2),

	/* commands */
	Crecal=		0x10,
	Cread=		0x20,
	Cwrite=		0x30,
	Cedd=		0x90,	/* execute device diagnostics */
	Cident=		0xEC,
	Cident2=	0xFF,	/* pseudo command for post Cident interrupt */
	Csetbuf=	0xEF,

	Cpktcmd=	0xA0,
	Cidentd=	0xA1,
	Ctur=		0x00,
	Creqsense=	0x03,
	Ccapacity=	0x25,
	Cread2=		0x28,

	ATAtimo=	6000,	/* ms to wait for things to complete */
	ATAPItimo=	10000,

	NCtlr=		8,
	NDrive=		NCtlr*2,

	Maxloop=	1000000,
};

/*
 *  ident sector from drive.  this is from ANSI X3.221-1994
 */
struct Ident
{
	ushort	config;		/* general configuration info */
	ushort	cyls;		/* # of cylinders (default) */
	ushort	reserved0;
	ushort	heads;		/* # of heads (default) */
	ushort	b2t;		/* unformatted bytes/track */
	ushort	b2s;		/* unformated bytes/sector */
	ushort	s2t;		/* sectors/track (default) */
	ushort	reserved1[3];
/* 10 */
	ushort	serial[10];	/* serial number */
	ushort	type;		/* buffer type */
	ushort	bsize;		/* buffer size/512 */
	ushort	ecc;		/* ecc bytes returned by read long */
	ushort	firm[4];	/* firmware revision */
	ushort	model[20];	/* model number */
/* 47 */
	ushort	s2i;		/* number of sectors/interrupt */
	ushort	dwtf;		/* double word transfer flag */
	ushort	capabilities;
	ushort	reserved2;
	ushort	piomode;
	ushort	dmamode;
	ushort	cvalid;		/* (cvald&1) if next 4 words are valid */
	ushort	ccyls;		/* current # cylinders */
	ushort	cheads;		/* current # heads */
	ushort	cs2t;		/* current sectors/track */
	ushort	ccap[2];	/* current capacity in sectors */
	ushort	cs2i;		/* current number of sectors/interrupt */
/* 60 */
	ushort	lbasecs[2];	/* # LBA user addressable sectors */
	ushort	dmasingle;
	ushort	dmadouble;
/* 64 */
	ushort	reserved3[64];
	ushort	vendor[32];	/* vendor specific */
	ushort	reserved4[96];
};

/*
 *  a hard drive
 */
struct Drive
{
	Controller *cp;
	uchar	driveno;
	uchar	dh;
	uchar	atapi;		/* ATAPI */
	uchar	drqintr;	/* ATAPI */
	ulong	vers;		/* ATAPI */

	int	partok;

	Disc;
};

/*
 *  a controller for 2 drives
 */
struct Controller
{
	int	cmdport;	/* base port */
	int	ctlport;
	uchar	ctlrno;

	/*
	 *  current operation
	 */
	int	cmd;		/* current command */
	uchar	cmdblk[12];	/* ATAPI */
	int	len;		/* ATAPI */
	int	count;		/* ATAPI */
	uchar	lastcmd;	/* debugging info */
	uchar	*buf;		/* xfer buffer */
	int	tcyl;		/* target cylinder */
	int	thead;		/* target head */
	int	tsec;		/* target sector */
	int	tbyte;		/* target byte */
	int	nsecs;		/* length of transfer (sectors) */
	int	sofar;		/* sectors transferred so far */
	int	status;
	int	error;
	Drive	*dp;		/* drive being accessed */
};

static int atactlrmask;
static Controller *atactlr[NCtlr];
static int atadrivemask;
static Drive *atadrive[NDrive];

typedef struct Atadev Atadev;
typedef struct Atadev {
	int	cmdport;
	int	ctlport;
	int	irq;

	Pcidev*	p;
	void	(*ienable)(Atadev*);
} Atadev;

static void pc87415ienable(Atadev*);

static Atadev atadev[NCtlr] = {
	{ 0x1F0, 0x3F4, 14, },	/* primary */
	{ 0x170, 0x374, 15, },	/* secondary */
	{ 0x1E8, 0x3EC,  0, },	/* tertiary */
	{ 0x168, 0x36C,  0, },	/* quaternary */
};
static int natadev = 4;

static void	hardintr(Ureg*, void*);
static long	hardxfer(Drive*, Partition*, int, ulong, long);
static int	hardident(Drive*);
static Drive*	hardpart(Drive*);
static int	hardparams(Drive*);
static void	hardrecal(Drive*);
static int	hardprobe(Drive*, int, int, int);
static Drive*	atapipart(Drive*);
static void	atapiintr(Controller*);
static long	atapiio(Drive*, long);

static void
pc87415ienable(Atadev* devp)
{
	Pcidev *p;
	int x;

	p = devp->p;
	if(p == nil)
		return;

	x = pcicfgr32(p, 0x40);
	if(devp->cmdport == (p->mem[0].bar & ~0x01))
		x &= ~0x00000100;
	else
		x &= ~0x00000200;
	pcicfgw32(p, 0x40, x);
}

static void
atapci(void)
{
	Pcidev *p;
	int ccrp;

	p = nil;
	while(p = pcimatch(p, 0, 0)){
		if(p->vid == 0x100B && p->did == 0x0002){
			/*
			 * National Semiconductor PC87415.
			 * Disable interrupts on both channels until
			 * after they are probed for drives.
			 */
			pcicfgw32(p, 0x40, 0x00000300);

			/*
			 * Add any native-mode channels to the list to
			 * be probed.
			 */
			ccrp = pcicfgr8(p, PciCCRp);
			if((ccrp & 0x01) && natadev < nelem(atadev)){
				atadev[natadev].cmdport = p->mem[0].bar & ~0x01;
				atadev[natadev].ctlport = p->mem[1].bar & ~0x01;
				atadev[natadev].irq = p->intl;
				atadev[natadev].p = p;
				atadev[natadev].ienable = pc87415ienable;
				natadev++;
			}
			if((ccrp & 0x04) && natadev < nelem(atadev)){
				atadev[natadev].cmdport = p->mem[2].bar & ~0x01;
				atadev[natadev].ctlport = p->mem[3].bar & ~0x01;
				atadev[natadev].irq = p->intl;
				atadev[natadev].p = p;
				atadev[natadev].ienable = pc87415ienable;
				natadev++;
			}
		}
	}
}

static int
atactlrwait(Controller* ctlr, uchar pdh, uchar ready, ulong ticks)
{
	int port;
	uchar dh, status;

	port = ctlr->cmdport;
	dh = (inb(port+Pdh) & DHslave)^(pdh & DHslave);
	ticks += m->ticks+1;

	do{
		status = inb(port+Pstatus);
		if(status & Sbusy)
			continue;
		if(dh){
			outb(port+Pdh, pdh);
			dh = 0;
			continue;
		}
		if((status & ready) == ready)
			return 0;
	}while(m->ticks < ticks);

	DPRINT("ata%d: ctlrwait failed 0x%uX\n", ctlr->ctlrno, status);
	outb(port+Pdh, DHmagic);
	return -1;
}

static void
atadrivealloc(Controller* ctlr, int driveno, int atapi)
{
	Drive *drive;

	if((drive = xalloc(sizeof(Drive))) == 0){
		DPRINT("ata%d: can't xalloc drive0\n", ctlr->ctlrno);
		return;
	}
	drive->cp = ctlr;
	drive->driveno = driveno;
	drive->dh = DHmagic;
	if(driveno & 0x01)
		drive->dh |= DHslave;
	drive->vers = 1;
	if(atapi)
		drive->atapi = 1;

	atadrive[driveno] = drive;
}

static int
atactlrprobe(int ctlrno, Atadev* devp, int irq, int resetok)
{
	Controller *ctlr;
	int atapi, cmdport, ctlport, mask, once, timo;
	uchar error, status, msb, lsb;

	cmdport = devp->cmdport;
	ctlport = devp->ctlport;

	/*
	 * Check the existence of a controller by verifying a sensible
	 * value can be written to and read from the drive/head register.
	 * If it's OK, allocate and initialise a Controller structure.
	 */
	DPRINT("ata%d: port 0x%uX\n", ctlrno, cmdport);
	outb(cmdport+Pdh, DHmagic);
	for(timo = 30000; timo; timo--){
		microdelay(1);
		status = inb(cmdport+Pdh);
		if(status == DHmagic)
			break;
	}
	status = inb(cmdport+Pdh);
	if(status != DHmagic){
		DPRINT("ata%d: DHmagic not ok == 0x%uX, 0x%uX\n",
			ctlrno, status, inb(cmdport+Pstatus));
		return 0;
	}
	DPRINT("ata%d: DHmagic ok\n", ctlrno);
	if((ctlr = xalloc(sizeof(Controller))) == 0)
		return 0;
	ctlr->cmdport = cmdport;
	ctlr->ctlport = ctlport;
	ctlr->ctlrno = ctlrno;
	ctlr->lastcmd = 0xFF;

	/*
	 * Attempt to check the existence of drives on the controller
	 * by issuing a 'check device diagnostics' command.
	 * Issuing a device reset here would possibly destroy any BIOS
	 * drive remapping and, anyway, some controllers (Vibra16) don't
	 * seem to implement the control-block registers; do it if requested.
	 * At least one controller/ATAPI-drive combination doesn't respond
	 * to the Cedd (Micronics M54Li + Sanyo CRD-254P) so let's check for the
	 * ATAPI signature straight off. If we find it there will be no probe
	 * done for a slave. Tough.
	 */
	if(resetok && ctlport){
		outb(ctlport+Pctl, Srst|nIEN);
		delay(10);
		outb(ctlport+Pctl, 0);
		if(atactlrwait(ctlr, DHmagic, 0, MS2TK(20))){
			DPRINT("ata%d: Srst status 0x%uX/0x%uX/0x%uX\n", ctlrno,
				inb(cmdport+Pstatus), inb(cmdport+Pcylmsb), inb(cmdport+Pcyllsb));
			xfree(ctlr);
			return 0;
		}
	}

	/*
	 * Disable interrupts.
	 */
	outb(ctlport+Pctl, nIEN);

	once = 1;
retry:
	atapi = 0;
	mask = 0;
	status = inb(cmdport+Pstatus);
	DPRINT("ata%d: ATAPI 0x%uX 0x%uX 0x%uX\n", ctlrno, status,
		inb(cmdport+Pcylmsb), inb(cmdport+Pcyllsb));
	USED(status);
	if(/*status == 0 &&*/ inb(cmdport+Pcylmsb) == 0xEB && inb(cmdport+Pcyllsb) == 0x14){
		DPRINT("ata%d: ATAPI ok\n", ctlrno);
		atapi |= 0x01;
		mask |= 0x01;
		goto atapislave;
	}
	if(atactlrwait(ctlr, DHmagic, 0, MS2TK(1))){
		DPRINT("ata%d: Cedd status 0x%uX/0x%uX/0x%uX\n", ctlrno,
			inb(cmdport+Pstatus), inb(cmdport+Pcylmsb), inb(cmdport+Pcyllsb));
		if(once){
			once = 0;
			ctlr->cmd = 0;
			goto retry;
		}
		xfree(ctlr);
		return 0;
	}

	/*
	 * Can only get here if controller is not busy.
	 * If there are drives Sbusy will be set within 400nS.
	 * Wait for the command to complete (6 seconds max).
	 */
	ctlr->cmd = Cedd;
	outb(cmdport+Pcmd, Cedd);
	microdelay(1);
	status = inb(cmdport+Pstatus);
	if(!(status & Sbusy)){
		DPRINT("ata%d: !busy 1 0x%uX\n", ctlrno, status);
		xfree(ctlr);
		return 0;
	}
	for(timo = 6000; timo; timo--){
		status = inb(cmdport+Pstatus);
		if(!(status & Sbusy))
			break;
		delay(1);
	}
	DPRINT("ata%d: timo %d\n", ctlrno, 6000-timo);
	status = inb(cmdport+Pstatus);
	if(status & Sbusy){
		DPRINT("ata%d: busy 2 0x%uX\n", ctlrno, status);
		xfree(ctlr);
		return 0;
	}

	/*
	 * The diagnostic returns a code in the error register, good
	 * status is bits 6-0 == 0x01.
	 * The existence of the slave is more difficult to determine,
	 * different generations of controllers may respond in different
	 * ways. The standards here offer little light but only more and
	 * more heat:
	 *   1) the slave must be done and have dropped Sbusy by now (six
	 *	seconds for the master, 5 seconds for the slave). If it
	 *	hasn't, then it has either failed or the controller is
	 *	broken in some way (e.g. Vibra16 returns status of 0xFF);
	 *   2) theory says the status of a non-existent slave should be 0.
	 *	Of course, it's valid for all the bits to be 0 for a slave
	 *	that exists too...
	 *   3) a valid ATAPI drive can have status 0 and the ATAPI signature
	 *	in the cylinder registers after reset. Of course, if the drive
	 *	has been messed about by the BIOS or some other O/S then the
	 *	signature may be gone.
	 * When checking status, mask off the IDX bit.
	 */
	error = inb(cmdport+Perror);
	DPRINT("ata%d: master diag status 0x%uX, error 0x%uX\n",
		ctlr->ctlrno, inb(cmdport+Pstatus), error);
	if((error & ~0x80) == 0x01)
		mask |= 0x01;

atapislave:
	outb(cmdport+Pdh, DHmagic|DHslave);
	microdelay(1);
	status = inb(cmdport+Pstatus);
	error = inb(cmdport+Perror);
	DPRINT("ata%d: slave diag status 0x%uX, error 0x%uX\n",
		ctlr->ctlrno, status, error);
	if((status & ~0x02) && (status & (Sbusy|Serr)) == 0 && (error & ~0x80) == 0x01)
		mask |= 0x02;
	else if(status == 0){
		msb = inb(cmdport+Pcylmsb);
		lsb = inb(cmdport+Pcyllsb);
		DPRINT("ata%d: ATAPI slave 0x%uX 0x%uX 0x%uX\n", ctlrno, status,
			inb(cmdport+Pcylmsb), inb(cmdport+Pcyllsb));
		if(msb == 0xEB && lsb == 0x14){
			atapi |= 0x02;
			mask |= 0x02;
		}
	}
	outb(cmdport+Pdh, DHmagic);

//skipslave:
	if(mask == 0){
		xfree(ctlr);
		return 0;
	}
	ctlr->buf = ialloc(Maxxfer, 0);
	atactlr[ctlrno] = ctlr;

	if(mask & 0x01)
		atadrivealloc(ctlr, ctlrno*2, atapi & 0x01);
	if(mask & 0x02)
		atadrivealloc(ctlr, ctlrno*2+1, atapi & 0x02);

	setvec(Int0vec+irq, hardintr, ctlr);
	inb(cmdport+Pstatus);
	outb(ctlport+Pctl, 0);
	if(devp->ienable)
		devp->ienable(devp);

	return mask;
}

static Drive*
atadriveprobe(int driveno)
{
	Drive *drive;
	int ctlrno;

	ctlrno = driveno/2;
	if(atactlr[ctlrno] == 0)
		return nil;

	drive = atadrive[driveno];
	if(drive == nil)
		return nil;
	if(drive->online == 0){
		if(atadrivemask & (1<<driveno))
			return nil;
		atadrivemask |= 1<<driveno;
		if(hardparams(drive))
			return nil;
		if(drive->lba)
			print("hd%d: LBA %lud sectors\n",
				drive->driveno, drive->cap);
		else
			print("hd%d: CHS %d/%d/%d %lud sectors\n",
				drive->driveno, drive->cyl, drive->heads,
				drive->sectors, drive->cap);
		drive->online = 1;
	}

	return hardpart(drive);
}

int
hardinit(void)
{
	int ctlrno, devno, i, mask, resetok;
	ISAConf isa;
	Atadev *devp;

	atapci();

	ctlrno = 0;
	mask = 0;
	for(devno = 0; devno < natadev; devno++){
		devp = &atadev[devno];
		memset(&isa, 0, sizeof(ISAConf));
		isaconfig("ata", ctlrno, &isa);
		if(isa.port && isa.port != devp->cmdport)
			continue;
		if(isa.irq == 0 && (isa.irq = devp->irq) == 0)
			continue;

		resetok = 0;
		for(i = 0; i < isa.nopt; i++){
			DPRINT("ata%d: opt %s\n", ctlrno, isa.opt[i]);
			if(cistrcmp(isa.opt[i], "reset") == 0)
				resetok = 1;
		}

		if((i = atactlrprobe(ctlrno, devp, isa.irq, resetok)) == 0)
			continue;
		mask |= i<<(ctlrno*2);
		ctlrno++;
	}

	return mask;
}

long
hardseek(int driveno, long offset)
{
	Drive *drive;

	if((drive = atadriveprobe(driveno)) == nil)
		return -1;
	drive->offset = offset;
	return offset;
}

/*
 *  did an interrupt happen?
 */
static void
atawait(Controller *cp, int timo)
{
	ulong start;
	int x;

	x = spllo();
	start = m->ticks;
	while(TK2MS(m->ticks - start) < timo){
		if(cp->cmd == 0)
			break;
		if(cp->cmd == Cident2 && TK2MS(m->ticks - start) >= 1000)
			break;
	}
	if(TK2MS(m->ticks - start) >= timo){
		DPRINT("atawait timed out %ux\n", inb(cp->cmdport+Pstatus));
		hardintr(0, cp);
	}
	splx(x);
}

Partition*
sethardpart(int driveno, char *p)
{
	Partition *pp;
	Drive *dp;

	if((dp = atadriveprobe(driveno)) == nil)
		return nil;

	for(pp = dp->p; pp < &dp->p[dp->npart]; pp++)
		if(strcmp(pp->name, p) == 0){
			dp->current = pp;
			return pp;
		}
	return nil;
}

long
hardread(int driveno, void *a, long n)
{
	Drive *dp;
	long rv, i;
	int skip;
	uchar *aa = a;
	Partition *pp;
	Controller *cp;

	if((dp = atadriveprobe(driveno)) == nil)
		return 0;

	pp = dp->current;
	if(pp == 0)
		return -1;
	cp = dp->cp;

	skip = dp->offset % dp->bytes;
	for(rv = 0; rv < n; rv += i){
		i = hardxfer(dp, pp, Cread, dp->offset+rv-skip, n-rv+skip);
		if(i == 0)
			break;
		if(i < 0)
			return -1;
		i -= skip;
		if(i > n - rv)
			i = n - rv;
		memmove(aa+rv, cp->buf + skip, i);
		skip = 0;
	}
	dp->offset += rv;

	return rv;
}

/*
 *  wait for the controller to be ready to accept a command
 */
static int
cmdreadywait(Drive *drive)
{
	uchar ready;

	if(drive->atapi)
		ready = 0;
	else
		ready = Sready;

	return atactlrwait(drive->cp, drive->dh, ready, MS2TK(10));
}

/*
 *  transfer a number of sectors.  hardintr will perform all the iterative
 *  parts.
 */
static long
hardxfer(Drive *dp, Partition *pp, int cmd, ulong start, long len)
{
	Controller *cp;
	long lsec;

	if(dp->online == 0){
		DPRINT("hd%d: disk not on line\n", dp->driveno);
		return -1;
	}

	if(cmd == Cwrite)
		return -1;

	/*
	 *  cut transfer size down to disk buffer size
	 */
	start = start / dp->bytes;
	if(len > Maxxfer)
		len = Maxxfer;
	len = (len + dp->bytes - 1) / dp->bytes;

	/*
	 *  calculate physical address
	 */
	cp = dp->cp;
	lsec = start + pp->start;
	if(lsec >= pp->end){
		DPRINT("hd%d: read past end of partition\n", dp->driveno);
		return 0;
	}
	if(dp->lba){
		cp->tsec = lsec & 0xff;
		cp->tcyl = (lsec>>8) & 0xffff;
		cp->thead = (lsec>>24) & 0xf;
	} else {
		cp->tcyl = lsec/(dp->sectors*dp->heads);
		cp->tsec = (lsec % dp->sectors) + 1;
		cp->thead = (lsec/dp->sectors) % dp->heads;
	}

	/*
	 *  can't xfer past end of disk
	 */
	if(lsec+len > pp->end)
		len = pp->end - lsec;
	cp->nsecs = len;

	if(dp->atapi){
		cp->cmd = Cread2;
		cp->dp = dp;
		cp->sofar = 0;
		cp->status = 0;
		return atapiio(dp, lsec);
	}

	if(cmdreadywait(dp) < 0)
		return -1;

	/*
	 *  start the transfer
	 */
	cp->cmd = cmd;
	cp->dp = dp;
	cp->sofar = 0;
	cp->status = 0;
	DPRINT("hd%d: xfer:\ttcyl %d, tsec %d, thead %d\n",
		dp->driveno, cp->tcyl, cp->tsec, cp->thead);
	DPRINT("\tnsecs %d, sofar %d\n", cp->nsecs, cp->sofar);
	outb(cp->cmdport+Pcount, cp->nsecs);
	outb(cp->cmdport+Psector, cp->tsec);
	outb(cp->cmdport+Pdh, dp->dh | (dp->lba<<6) | cp->thead);
	outb(cp->cmdport+Pcyllsb, cp->tcyl);
	outb(cp->cmdport+Pcylmsb, cp->tcyl>>8);
	outb(cp->cmdport+Pcmd, cmd);

	atawait(cp, ATAtimo);

	if(cp->status & Serr){
		DPRINT("hd%d: err: status %lux, err %lux\n",
			dp->driveno, cp->status, cp->error);
		DPRINT("\ttcyl %d, tsec %d, thead %d\n",
			cp->tcyl, cp->tsec, cp->thead);
		DPRINT("\tnsecs %d, sofar %d\n", cp->nsecs, cp->sofar);
		return -1;
	}

	return cp->nsecs*dp->bytes;
}

static int
isatapi(Drive *drive)
{
	Controller *cp;

	cp = drive->cp;
	outb(cp->cmdport+Pdh, drive->dh);
	IDPRINT("hd%d: isatapi %d\n", drive->driveno, drive->atapi);
	outb(cp->cmdport+Pcmd, 0x08);
	drive->atapi = 1;
	if(cmdreadywait(drive) < 0){
		drive->atapi = 0;
		return 0;
	}
	drive->atapi = 0;
	drive->bytes = 512;
	microdelay(1);
	if(inb(cp->cmdport+Pstatus)){
		IDPRINT("hd%d: isatapi status %ux\n",
			drive->driveno, inb(cp->cmdport+Pstatus));
		return 0;
	}
	if(inb(cp->cmdport+Pcylmsb) != 0xEB || inb(cp->cmdport+Pcyllsb) != 0x14){
		IDPRINT("hd%d: isatapi cyl %ux %ux\n",
			drive->driveno, inb(cp->cmdport+Pcylmsb), inb(cp->cmdport+Pcyllsb));
		return 0;
	}
	drive->atapi = 1;

	return 1;
}

/*
 *  get parameters from the drive
 */
static int
hardident(Drive *dp)
{
	Controller *cp;
	Ident *ip;
	ulong lbasecs;
	char id[21];

	dp->bytes = 512;
	cp = dp->cp;

retryatapi:
	cp->nsecs = 1;
	cp->sofar = 0;
	cp->dp = dp;
	outb(cp->cmdport+Pdh, dp->dh);
	microdelay(1);
	if(inb(cp->cmdport+Pcylmsb) == 0xEB && inb(cp->cmdport+Pcyllsb) == 0x14){
		dp->atapi = 1;
		cp->cmd = Cidentd;
	}
	else
		cp->cmd = Cident;
	outb(cp->cmdport+Pcmd, cp->cmd);

	IDPRINT("hd%d: ident command %ux sent\n", dp->driveno, cp->cmd);
	atawait(cp, ATAPItimo);

	if(cp->status & Serr){
		IDPRINT("hd%d: bad disk ident status\n", dp->driveno);
		if(cp->error & Eabort){
			if(isatapi(dp) && cp->cmd != Cidentd)
				goto retryatapi;
		}
		return -1;
	}
	
	atawait(cp, ATAtimo);

	ip = (Ident*)cp->buf;
	memmove(id, ip->model, sizeof(id)-1);
	id[sizeof(id)-1] = 0;

	IDPRINT("hd%d: config 0x%uX capabilities 0x%uX\n",
		dp->driveno, ip->config, ip->capabilities);
	if(dp->atapi){
		if((ip->config & 0x0060) == 0x0020)
			dp->drqintr = 1;
		if((ip->config & 0x1F00) == 0x0000)
			dp->atapi = 2;
	}

	lbasecs = (ip->lbasecs[0]) | (ip->lbasecs[1]<<16);
	if((ip->capabilities & (1<<9)) && (lbasecs & 0xf0000000) == 0){
		dp->lba = 1;
		dp->cap = lbasecs;
	} else {
		dp->lba = 0;
	
		if(ip->cvalid&(1<<0)){
			/* use current settings */
			dp->cyl = ip->ccyls;
			dp->heads = ip->cheads;
			dp->sectors = ip->cs2t;
		}
		else{
			/* use default (unformatted) settings */
			dp->cyl = ip->cyls;
			dp->heads = ip->heads;
			dp->sectors = ip->s2t;
		}

		if(dp->heads >= 64 || dp->sectors >= 32)
			return -1;
		dp->cap = dp->cyl * dp->heads * dp->sectors;
	}
	IDPRINT("hd%d: %s  lba/atapi/drqintr: %d/%d/%d  C/H/S: %d/%d/%d  CAP: %ld\n",
		dp->driveno, id,
		dp->lba, dp->atapi, dp->drqintr,
		dp->cyl, dp->heads, dp->sectors,
		dp->cap);

	return 0;
}

/*
 *  probe the given sector to see if it exists
 */
static int
hardprobe(Drive *dp, int cyl, int sec, int head)
{
	Controller *cp;

	cp = dp->cp;
	if(cmdreadywait(dp) < 0)
		return -1;

	/*
	 *  start the transfer
	 */
	cp->cmd = Cread;
	cp->dp = dp;
	cp->sofar = 0;
	cp->nsecs = 1;
	cp->status = 0;
	outb(cp->cmdport+Pcount, 1);
	outb(cp->cmdport+Psector, sec+1);
	outb(cp->cmdport+Pdh, dp->dh | (dp->lba<<6) | head);
	outb(cp->cmdport+Pcyllsb, cyl);
	outb(cp->cmdport+Pcylmsb, cyl>>8);
	outb(cp->cmdport+Pcmd, Cread);

	atawait(cp, ATAtimo);

	if(cp->status & Serr)
		return -1;

	return 0;
}

/*
 *  figure out the drive parameters
 */
static int
hardparams(Drive *dp)
{
	int i, hi, lo;

	/*
	 *  first try the easy way, ask the drive and make sure it
	 *  isn't lying.
	 */
	dp->bytes = 512;
	if(hardident(dp) < 0)
		return -1;
	if(dp->atapi)
		return 0;
	if(dp->lba){
		i = dp->cap - 1;
		if(hardprobe(dp, (i>>8)&0xffff, (i&0xff)-1, (i>>24)&0xf) == 0)
			return 0;
	} else {
		if(hardprobe(dp, dp->cyl-1, dp->sectors-1, dp->heads-1) == 0)
			return 0;
	}

	IDPRINT("hd%d: hardparam: cyl %d sectors %d heads %d\n",
		dp->driveno, dp->cyl, dp->sectors, dp->heads);
	/*
	 *  the drive lied, determine parameters by seeing which ones
	 *  work to read sectors.
	 */
	dp->lba = 0;
	for(i = 0; i < 16; i++)
		if(hardprobe(dp, 0, 0, i) < 0)
			break;
	dp->heads = i;
	for(i = 0; i < 64; i++)
		if(hardprobe(dp, 0, i, 0) < 0)
			break;
	dp->sectors = i;
	for(i = 512; ; i += 512)
		if(hardprobe(dp, i, dp->sectors-1, dp->heads-1) < 0)
			break;
	lo = i - 512;
	hi = i;
	for(; hi-lo > 1;){
		i = lo + (hi - lo)/2;
		if(hardprobe(dp, i, dp->sectors-1, dp->heads-1) < 0)
			hi = i;
		else
			lo = i;
	}
	dp->cyl = lo + 1;
	dp->cap = dp->cyl * dp->heads * dp->sectors;

	if(dp->cyl == 0 || dp->heads == 0 || dp->sectors == 0)
		return -1;

	return 0;
}

static Drive*
hardpart(Drive *dp)
{
	Partition *pp;

	if(dp->partok)
		return dp;

	/*
	 *  we always have a partition for the whole disk
	 */
	pp = &dp->p[0];
	strcpy(pp->name, "disk");
	dp->npart = 1;
	pp->start = 0;
	if(dp->atapi)
		return atapipart(dp);
	pp->end = dp->cap;
	dp->partok = 1;

	return dp;
}

/*
 *  we get an interrupt for every sector transferred
 */
static void
hardintr(Ureg*, void *arg)
{
	Controller *cp;
	Drive *dp;
	long loop;

	cp = arg;
	if((dp = cp->dp) == 0)
		return;

	loop = 0;
	while((cp->status = inb(cp->cmdport+Pstatus)) & Sbusy)
		if(++loop > Maxloop){
			print("hd%d: intr busy: status 0x%lux\n", dp->driveno, cp->status);
			break;
		}
	switch(cp->cmd){
	case Cread:
	case Cident:
	case Cidentd:
		if(cp->status & Serr){
			cp->cmd = 0;
			cp->error = inb(cp->cmdport+Perror);
			return;
		}
		loop = 0;
		while((inb(cp->cmdport+Pstatus) & Sdrq) == 0)
			if(++loop > Maxloop){
				print("hd%d: intr drq: cmd %ux status %ux",
					dp->driveno, cp->cmd, inb(cp->cmdport+Pstatus));
				cp->cmd = 0;
				return;
			}
		inss(cp->cmdport+Pdata, &cp->buf[cp->sofar*dp->bytes],
			dp->bytes/2);
		cp->sofar++;
		if(cp->sofar >= cp->nsecs){
			if(cp->cmd == Cident && (cp->status & Sready) == 0)
				cp->cmd = Cident2; /* sometimes we get a second intr */
			else
				cp->cmd = 0;
			inb(cp->cmdport+Pstatus);
		}
		break;
	case Csetbuf:
	case Cident2:
		cp->cmd = 0;
		break;
	case Cpktcmd:
		atapiintr(cp);
		break;
	default:
		cp->cmd = 0;
		break;
	}
}

static int
atapiexec(Drive *dp)
{
	Controller *cp;
	int loop, s;

	cp = dp->cp;

	if(cmdreadywait(dp) < 0)
		return -1;

	s = splhi();	
	cp->sofar = 0;
	cp->error = 0;
	cp->cmd = Cpktcmd;
	outb(cp->cmdport+Pcount, 0);
	outb(cp->cmdport+Psector, 0);
	outb(cp->cmdport+Pfeature, 0);
	outb(cp->cmdport+Pcyllsb, cp->len);
	outb(cp->cmdport+Pcylmsb, cp->len>>8);
	outb(cp->cmdport+Pdh, dp->dh);
	outb(cp->cmdport+Pcmd, cp->cmd);

	if(dp->drqintr == 0){
		microdelay(1);
		for(loop = 0; (inb(cp->cmdport+Pstatus) & (Serr|Sdrq)) == 0; loop++){
			if(loop < 10000)
				continue;
			panic("hd%d: cmddrqwait: cmd=%lux status=%lux\n",
				dp->driveno, cp->cmd, inb(cp->cmdport+Pstatus));
		}
		outss(cp->cmdport+Pdata, cp->cmdblk, sizeof(cp->cmdblk)/2);
	}
	splx(s);

	atawait(cp, ATAPItimo);

	if(cp->status & Serr){
		DPRINT("hd%d: Bad packet command 0x%ux, error 0x%ux\n",
			dp->driveno, cp->cmdblk[0], cp->error);
		return -1;
	}

	return 0;
}

static long
atapiio(Drive *dp, long lba)
{
	int n;
	Controller *cp;

	cp = dp->cp;

	n = cp->nsecs*dp->bytes;
	cp->len = n;
	cp->count = 0;
	memset(cp->cmdblk, 0, 12);
	cp->cmdblk[0] = cp->cmd;
	cp->cmdblk[2] = lba >> 24;
	cp->cmdblk[3] = lba >> 16;
	cp->cmdblk[4] = lba >> 8;
	cp->cmdblk[5] = lba;
	cp->cmdblk[7] = cp->nsecs>>8;
	cp->cmdblk[8] = cp->nsecs;
	if(atapiexec(dp))
		return -1;
	if(cp->count != n)
		print("hd%d: short read %d != %d\n", dp->driveno, cp->count, n);

	return n;
}

static Drive*
atapipart(Drive *dp)
{
	Controller *cp;
	Partition *pp;
	int retrycount;

	cp = dp->cp;

	pp = &dp->p[0];
	pp->end = 0;

	retrycount = 0;
retry:
	if(retrycount++){
		IDPRINT("hd%d: atapipart: cmd 0x%uX error 0x%uX, retry %d\n",
			dp->driveno, cp->cmdblk[0], cp->error, retrycount);
		if((cp->status & Serr) && (cp->error & 0xF0) == 0x60){
			dp->vers++;
			if(retrycount < 3)
				goto again;
		}
		cp->dp = 0;
		IDPRINT("hd%d: atapipart: cmd %uX return error %uX, retry %d\n",
			dp->driveno, cp->cmd, cp->error, retrycount);
		return 0;
	}
again:
	cp->dp = dp;

	cp->len = 18;
	cp->count = 0;
	memset(cp->cmdblk, 0, sizeof(cp->cmdblk));
	cp->cmdblk[0] = Creqsense;
	cp->cmdblk[4] = 18;
	DPRINT("reqsense %d\n", retrycount);
	atapiexec(dp);
	//if(atapiexec(dp))
	//	goto retry;
	if(cp->count != 18){
		print("cmd=0x%2.2uX, lastcmd=0x%2.2uX ", cp->cmd, cp->lastcmd);
		print("cdsize count %d, status 0x%2.2uX, error 0x%2.2uX\n",
			cp->count, cp->status, cp->error);
		return 0;
	}

	cp->len = 8;
	cp->count = 0;
	memset(cp->cmdblk, 0, sizeof(cp->cmdblk));
	cp->cmdblk[0] = Ccapacity;
	DPRINT("capacity %d\n", retrycount);
	if(atapiexec(dp))
		goto retry;
	if(cp->count != 8){
		print("cmd=0x%2.2uX, lastcmd=0x%2.2uX ", cp->cmd, cp->lastcmd);
		print("cdsize count %d, status 0x%2.2uX, error 0x%2.2uX\n",
			cp->count, cp->status, cp->error);
		return 0;
	}
	dp->sectors = (cp->buf[0]<<24)|(cp->buf[1]<<16)|(cp->buf[2]<<8)|cp->buf[3];
	dp->bytes = (cp->buf[4]<<24)|(cp->buf[5]<<16)|(cp->buf[6]<<8)|cp->buf[7];
	if(dp->bytes > 2048 && dp->bytes <= 2352)
		dp->bytes = 2048;
	dp->cap = dp->sectors;
	IDPRINT("hd%d: atapipart secs %ud, bytes %ud, cap %ud\n",
		dp->driveno, dp->sectors, dp->bytes, dp->cap);
	cp->dp = 0;

	pp->end = dp->sectors;
	dp->partok = 1;

	return dp;
}

static void
atapiintr(Controller *cp)
{
	uchar cause;
	int count, loop, pbase;

	pbase = cp->cmdport;
	cause = inb(pbase+Pcount) & 0x03;
	DPRINT("hd%d: atapiintr %uX\n", cp->dp->driveno, cause);
	switch(cause){

	case 1:						/* command */
		if(cp->status & Serr){
			cp->lastcmd = cp->cmd;
			cp->cmd = 0;
			cp->error = inb(pbase+Perror);
			break;
		}
		outss(pbase+Pdata, cp->cmdblk, sizeof(cp->cmdblk)/2);
		break;

	case 2:						/* data in */
		if(cp->buf == 0){
			cp->lastcmd = cp->cmd;
			cp->cmd = 0;
			if(cp->status & Serr)
				cp->error = inb(pbase+Perror);
			cp->cmd = 0;
			break;	
		}
		loop = 0;
		while((cp->status & (Serr|Sdrq)) == 0){
			if(++loop > Maxloop){
				cp->status |= Serr;
				break;
			}
			cp->status = inb(pbase+Pstatus);
		}
		if(cp->status & Serr){
			cp->lastcmd = cp->cmd;
			cp->cmd = 0;
			cp->error = inb(pbase+Perror);
			print("hd%d: Cpktcmd status=0x%uX, error=0x%uX\n",
				cp->dp->driveno, cp->status, cp->error);
			break;
		}
		count = inb(pbase+Pcyllsb)|(inb(pbase+Pcylmsb)<<8);
		if(cp->count+count > Maxxfer)
			panic("hd%d: count %d, already %d\n", count, cp->count);
		inss(pbase+Pdata, cp->buf+cp->count, count/2);
		cp->count += count;
		break;

	case 3:						/* status */
		cp->lastcmd = cp->cmd;
		cp->cmd = 0;
		if(cp->status & Serr)
			cp->error = inb(cp->cmdport+Perror);
		break;
	}
}

Type Atatype = 	
	{	Thard,
		Fini|Fdos,
		hardinit, hardread, hardseek, dosboot, sethardpart,
		{ "h", },
	};

void
devatalink(void)
{
	addboottype(&Atatype);
}
