/*
 * USB client-mode driver
 *	endpoint 0	control (read and write)
 *	endpoint 1	OUT bulk endpoint (host to us)
 *	endpoint 2	IN bulk or interrupt endpoint (us to host)
 *
 * this is similar to the SA1100 assignment, and
 * sufficient to run Styx over USB, or to emulate simple devices.
 * the 823 USB implementation would allow a further endpoint,
 * and endpoints could have both IN and OUT modes.
 *
 * to do:
 *	data toggle control
 *	tx error recovery
 */

#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "ureg.h"

#include "../port/error.h"

enum {
	LOWSPEED = 0,	/* zero for 12m/bit, one for 1.5m/bit */
	Chatty = 0,	/* debugging */
};

//#include "usb.h"

/*
 * USB packet definitions
 */

#define	GET2(p)	((((p)[1]&0xFF)<<8)|((p)[0]&0xFF))
#define	PUT2(p,v)	{((p)[0] = (v)); ((p)[1] = (v)>>8);}

enum {
	/* request type */
	RH2D = 0<<7,
	RD2H = 1<<7,
	Rstandard = 0<<5,
	Rclass = 1<<5,
	Rvendor = 2<<5,
	Rdevice = 0,
	Rinterface = 1,
	Rendpt = 2,
	Rother = 3,

	/* standard requests */
	GET_STATUS = 0,
	CLEAR_FEATURE = 1,
	SET_FEATURE = 3,
	SET_ADDRESS = 5,
	GET_DESCRIPTOR = 6,
	SET_DESCRIPTOR = 7,
	GET_CONFIGURATION = 8,
	SET_CONFIGURATION = 9,
	GET_INTERFACE = 10,
	SET_INTERFACE = 11,
	SYNCH_FRAME = 12,

	/* hub class feature selectors */
	C_HUB_LOCAL_POWER = 0,
	C_HUB_OVER_CURRENT,
	PORT_CONNECTION = 0,
	PORT_ENABLE = 1,
	PORT_SUSPEND = 2,
	PORT_OVER_CURRENT = 3,
	PORT_RESET = 4,
	PORT_POWER = 8,
	PORT_LOW_SPEED = 9,
	C_PORT_CONNECTION = 16,
	C_PORT_ENABLE,
	C_PORT_SUSPEND,
	C_PORT_OVER_CURRENT,
	C_PORT_RESET,

	/* descriptor types */
	DEVICE = 1,
	CONFIGURATION = 2,
	STRING = 3,
	INTERFACE = 4,
	ENDPOINT = 5,
	HID = 0x21,
	REPORT = 0x22,
	PHYSICAL = 0x23,

	/* feature selectors */
	DEVICE_REMOTE_WAKEUP = 1,
	ENDPOINT_STALL = 0,
};

/*
 * CPM firmware structures
 */

typedef struct USBparam USBparam;
struct USBparam {
	ushort	epptr[4];
	ulong	rstate;
	ulong	rptr;
	ushort	frame_n;
	ushort	rbcnt;
	ulong	rtemp;
};

typedef struct EPparam EPparam;
struct EPparam {
	ushort	rbase;
	ushort	tbase;
	uchar	rfcr;
	uchar	tfcr;
	ushort	mrblr;
	ushort	rbptr;
	ushort	tbptr;

	ulong	tstate;
	ulong	tptr;
	ushort	tcrc;
	ushort	tbcnt;
	ulong	res[2];
};

enum {
	Nrdre		= 8,	/* receive descriptor ring entries */
	Ntdre		= 4,	/* transmit descriptor ring entries */

	Maxpkt		= 1023,		/* maximum USB packet size (data part) */
	Rbsize		= Maxpkt+2,		/* ring buffer size (including 2 byte crc) */
	Bufsize		= (Rbsize+7)&~7,	/* aligned */

	Nendpt		= 4,	/* endpoints supported by firmware */

	CPusb		= CPscc1,	/* replaces SCC1 on 823 */
};

#define	MkPID(x) (((~x&0xF)<<4)|(x&0xF))
enum {
	TokIN = MkPID(9),
	TokOUT = MkPID(1),
	TokSOF = MkPID(5),
	TokSETUP = MkPID(0xD),
	TokDATA0 = MkPID(3),
	TokDATA1 = MkPID(0xB),
	TokACK = MkPID(2),
	TokNAK = MkPID(0xA),
	TokPRE = MkPID(0xC),
};

enum {
	/* BDEmpty, BDWrap, BDInt, BDLast, BDFirst */
	RxDtype=	3<<6,
	  RxData0=	0<<6,	/* DATA0 (OUT) */
	  RxData1=	1<<6,	/* DATA1 (OUT) */
	  RxData0S=	2<<6,	/* DATA0 (SETUP) */
	RxeNO=		1<<4,	/* non octet-aligned */
	RxeAB=		1<<3,	/* frame aborted: bit stuff error */
	RxeCR=		1<<2,	/* CRC error */
	RxeOV=		1<<1,	/* overrun */
	RxErrs=		(RxeNO|RxeAB|RxeCR|RxeOV),

	TxTC=		1<<10,	/* transmit CRC */
	TxCNF=		1<<9,	/* transmit confirmation */
	TxLSP=		1<<8,	/* low speed (host) */
	TxNoPID=		0<<6,	/* do not send PID */
	TxData0=		2<<6,	/* add DATA0 PID */
	TxData1=		3<<6,	/* add DATA1 PID */
	TxeNAK=		1<<4,	/* nak received */
	TxeSTAL=		1<<3,	/* stall received */
	TxeTO=		1<<2,	/* timeout */
	TxeUN=		1<<1,	/* underrun */
	TxErrs=		(TxeNAK|TxeSTAL|TxeTO|TxeUN),

	/* usmod */
	EN=		1<<0,	/* enable USB */
	HOST=	1<<1,	/* host mode */
	TEST=	1<<2,	/* test mode */
	RESUME=	1<<6,	/* generate resume condition */
	LSS=		1<<7,	/* low-speed signalling */

	/* usber */
	Freset=	1<<9,
	Fidle=	1<<8,
	Ftxe3=	1<<7,
	Ftxe2=	1<<6,
	Ftxe1=	1<<5,
	Ftxe0=	1<<4,
	Ftxe=	Ftxe0|Ftxe1|Ftxe2|Ftxe3,
	Fsof=	1<<3,
	Fbsy=	1<<2,
	Ftxb=	1<<1,
	Frxb=	1<<0,

	/* uscom */
	FifoFill=		1<<7,
	FifoFlush=	1<<6,

	/* usep0-3 */
	EPNSHIFT=	12,
	EPctl=	0<<8,
	EPintr=	1<<8,
	EPbulk=	2<<8,
	EPiso=	3<<8,
	EPmulti=	1<<5,
	EPrte=	1<<4,
	THSmask=	3<<2,
	  THSok=	0<<2,
	  THSignore= 1<<2,
	  THSnak=	2<<2,
	  THSstall=	3<<2,
	RHSmask= 3<<0,
	  RHSok=	0<<0,
	  RHSignore= 1<<0,
	  RHSnak=	2<<0,
	  RHSstall=	3<<0,

	/* CPM/USB commands (or'd with USBCmd) */
	StopTxEndPt	= 1<<4,
	RestartTxEndPt	= 2<<4,

	SOFmask=	(1<<11)-1,

	IsData1=	1<<6,	/* bit in rtog/xtog to distinguish DATA1 */
};

#define	USBABITS	(SIBIT(14)|SIBIT(15))
#define	USBRXCBIT	(SIBIT(10)|SIBIT(11))
#define	USBTXCBIT	(SIBIT(6)|SIBIT(7))

/*
 * software structures
 */

typedef struct Ctlr Ctlr;
typedef struct Endpt Endpt;

struct Endpt {
	Ref;
	int	x;	/* index in Ctlr.pts */
	int	rbsize;
	int	txe;	/* Ftxe0<<Endpt.x */
	int	mode;	/* OREAD, OWRITE, ORDWR */
	int	maxpkt;
	int	type;		/* EPbulk, etc */
	int	rtog;		/* RxData0 or RxData1, next expected */
	int	xtog;	/* TxData0 or TxData1, next sent */
	int	txrestart;
	int	rxblocked;
	ushort*	usep;	/* &e->ctlr->usb->usep[e->x] */
	Rendez	ir;
	EPparam*	ep;
	Ring;

	Queue*	oq;
	Queue*	iq;
	Ctlr*	ctlr;

	ulong	outpackets;
	ulong	outbytes;
	ulong	inpackets;
	ulong	inbytes;
	ulong	alignerrs;
	ulong	badframes;
	ulong	badcrcs;
	ulong	overflows;
	ulong	soverflows;
	ulong	missed;
	ulong	txnacks;
	ulong	txtimeouts;
	ulong	txstalls;
	ulong	txunderruns;
	ulong	seqerrs;
};

struct Ctlr {
	Lock;
	Ref;
	int	init;
	USB*	usb;
	USBparam *usbp;
	CPMdev*	cpm;
	Endpt	pts[Nendpt];
};

enum {
	/* endpoint assignment */
	EPsetup = 0,
	EPread = 1,	/* we read, they write */
	EPwrite = 2,	/* we write, they read */
	/* endpoint 3 currently unused */
};

enum {
	Qdir,
	Qctl,
	Qaddr,
	Qdata,
	Qstat,
	Qframe,
	Qsetup,
};
#define	QID(path)	((path)&0xF)

static	Dirtab	usbtab[] = {
	"usbctl",		{Qctl, 0},			0,	0200,
	"usbdata",		{Qdata, 0},		0,	0600,
	"usbstat",		{Qstat, 0},			0,	0400,
	"usbaddr",	{Qaddr, 0},		0,	0400,
	"usbframe",	{Qframe, 0},		0,	0400,
	"usbsetup",	{Qsetup, 0},		0,	0600,
};

static	Ctlr	usbctlr[1];
static	char	Eusbreset[] = "USB bus reset";

static	void	dumpusb(Block*, char*);
static	void	interrupt(Ureg*, void*);
static	void	setupep(int, EPparam*, int, int, int);
static	void	eptflow(void*);
static	void	eptrestart(Endpt*);
static	void	eptstop(Endpt*);
static	void	eptflush(Endpt*);

static void
resetusb(void)
{
	IMM *io;
	USB *usb;
	USBparam *usbp;
	EPparam *ep;
	CPMdev *cpm;
	int brg, i;

	brg = brgalloc();
	if(brg < 0){
		print("usb: no baud rate generator is free\n");
		return;
	}

	/* select USB port pins */
	io = ioplock();
	io->padir &= ~USBABITS;
	io->papar |= USBABITS;
	io->paodr &= ~USBABITS;
	io->pcpar = (io->pcpar & ~USBRXCBIT) | USBTXCBIT;
	io->pcdir = (io->pcdir & ~USBRXCBIT) | USBTXCBIT;
	io->pcso |= USBRXCBIT;
	iopunlock();

	archdisableusb();

	ep = cpmalloc(Nendpt*sizeof(*ep), 32);
	if(ep == nil){
		print("can't allocate USB\n");
		return;
	}

	cpm = cpmdev(CPusb);
	usb = cpm->regs;
	usb->usmod = 0;
	usbp = cpm->param;
	usbp->frame_n = 0;
	usbp->rstate = 0;
	usbp->rptr = 0;
	usbp->rtemp = 0;
	usbp->rbcnt =0;
	for(i=0; i<Nendpt; i++){
		usb->usep[i] = (i<<EPNSHIFT) | EPbulk | THSignore | RHSignore;
		usbp->epptr[i] = PADDR(ep+i) & 0xffff;
		ep[i].rbase = 0;
		ep[i].tbase = 0;
		ep[i].rfcr = 0x10;
		ep[i].tfcr = 0x10;
		ep[i].mrblr = Bufsize;
		ep[i].rbptr = 0;
		ep[i].tbptr = 0;
		ep[i].tstate = 0;
		ep[i].tptr = 0;
	}

	usbctlr->usb = usb;
	usbctlr->usbp = usbp;
	usbctlr->cpm = cpm;

	usb->usmod = 0;
	if(LOWSPEED)
		usb->usmod |= LSS;

	/* set up baud rate generator for appropriate speed */
	if(usb->usmod & LSS){
		print("USB: low speed\n");
		io->brgc[brg] = baudgen(4*1500000, 1) | BaudEnable;
	}else{
		print("USB: high speed\n");
		io->brgc[brg] = baudgen(4*12*MHz, 1) | BaudEnable;
	}
	eieio();
	if(1)
		print("usbbrg=%8.8lux\n", io->brgc[brg]);

	delay(1);	/* see MPC823 errata error CPM2 */
	sccnmsi(1, brg, 0);	/* set R1CS */

	setupep(EPsetup, ep, EPctl, ORDWR, 8);
	setupep(EPread, ep+EPread, EPbulk, OREAD, Maxpkt);
	setupep(EPwrite, ep+EPwrite, EPbulk, OWRITE, Maxpkt);

	archenableusb((usb->usmod&LSS)==0, 0);

	usb->usadr = 0;
	intrenable(VectorCPIC+cpm->irq, interrupt, usbctlr, BUSUNKNOWN);
	usb->usbmr = Freset|Ftxe|Ftxb|Frxb;	/* enable events (exclude idle and SOF, inter alia) */
	usb->usbmr &= ~Ftxe;	/* don't enable Ftxe */
	eieio();
	usb->usber = ~0;	/* clear events */
	eieio();
	/* the device will be enabled when open, to save power */
}

static int
eptmode(Endpt *e)
{
	int usep;

	usep = (e->x<<EPNSHIFT) | e->type;
	if(e->mode == OWRITE)
		usep |= THSok | RHSstall;
	else if(e->mode == OREAD)
		usep |= THSstall | RHSok;
	else
		usep |= RHSok | THSok;
	if(e->type != EPiso)
		usep |= EPrte;
	return usep;
}

static void
setupep(int n, EPparam *ep, int type, int mode, int maxpkt)
{
	Endpt *e;
	int rbsize;

	e = &usbctlr->pts[n];
	e->x = n;
	e->ctlr = usbctlr;
	e->txe = Ftxe0<<n;
	e->type = type;
	e->xtog = TxData0;
	e->rtog = RxData0;
	e->mode = mode;
	e->maxpkt = maxpkt;
	e->usep = &e->ctlr->usb->usep[n];
	*e->usep = eptmode(e);
	rbsize = ((n==0?64:Bufsize)+2+3)&~3;	/* 0 mod 4 */
	if(e->oq == nil)
		e->oq = qopen(8*Bufsize, n==0, nil, nil);
	if(e->iq == nil)
		e->iq = qopen(Nrdre*Bufsize, n==0, eptflow, e);
	e->ep = ep;
	if(e->rdr == nil)
		if(ioringinit(e, Nrdre, Ntdre, rbsize) < 0)
			panic("usbreset");
	ep->mrblr = rbsize;
	ep->rbase = PADDR(e->rdr);
	ep->rbptr = ep->rbase;
	ep->tbase = PADDR(e->tdr);
	ep->tbptr = ep->tbase;
	eieio();
}

static void
eptreset(Endpt *e)
{
	if(e->ntq && e->txrestart == 0)
		eptflush(e);
	if(e->iq)
		qhangup(e->iq, Eusbreset);
	if(e->oq)
		qhangup(e->oq, Eusbreset);
	e->rtog = RxData0;
	e->xtog = TxData0;
}

static void
eptclose(Endpt *e)
{
	if(decref(e) == 0){
		if(e->iq)
			qclose(e->iq);
		if(e->oq)
			qclose(e->oq);
		/* TO DO: eptstop(e) when output queue drained */
	}
}

static void
txstart(Endpt *e)
{
	int len, flags, dofill;
	Block *b;
	BD *dre;

	if(e->ctlr->init)
		return;
	dofill = 0;
	while(e->ntq < Ntdre-1){
		b = qget(e->oq);
		if(b == 0)
			break;
		dre = &e->tdr[e->tdrh];
		if(dre->status & BDReady)
			panic("txstart");
		flags = e->xtog | TxTC | BDReady;
		e->xtog ^= TxData1^TxData0;
		len = BLEN(b);	/* Motorola manuals incorrectly say len can't be zero */
		dcflush(b->rp, len);
		if(e->txb[e->tdrh] != nil)
			panic("usb: txstart");
		e->txb[e->tdrh] = b;
		dre->addr = PADDR(b->rp);
		dre->length = len;
		eieio();
		dre->status = (dre->status & BDWrap) | BDInt|BDLast | flags;
		eieio();
		e->outpackets++;
		e->ntq++;
		e->tdrh = NEXT(e->tdrh, Ntdre);
		if(e->ntq == 1)
			dofill = 1;
	}
	eieio();
	if(e->ntq){
		/* work to do: tell the chip to collect it */
		if(e->txrestart){
			eptrestart(e);
			dofill = 1;
		}
		if(dofill){
			e->ctlr->usb->uscom = FifoFill | e->x;
			eieio();
		}
	}
}

static void
transmit(Endpt *r)
{
	ilock(r->ctlr);
	txstart(r);
	iunlock(r->ctlr);
}

static void
eptstop(Endpt *e)
{
	cpmop(e->ctlr->cpm, USBCmd|StopTxEndPt, e->x<<1);
}

static void
eptflush(Endpt *e)
{
	if(!e->txrestart){
		eptstop(e);
		e->txrestart = 1;
		e->ctlr->usb->uscom = FifoFlush | e->x;
	}
}

static void
eptrestart(Endpt *e)
{
	cpmop(e->ctlr->cpm, USBCmd|RestartTxEndPt, e->x<<1);
	e->txrestart = 0;
}

static void
eptsetuse(Endpt *e, int mask, int oldv, int newv)
{
	int usep;

	usep = *e->usep;
	if((usep & mask) == oldv)
		*e->usep = (usep & ~mask) | newv;
}

static int
usbctlpacket(Ctlr *ctlr, uchar *p, int n)
{
	if(n < 8)
		return 0;
	if((p[0] & RD2H) == 0 && p[1] == SET_ADDRESS){
		ctlr->usb->usadr = GET2(p+2)&0x7F;
		return 1;
	}
	return 0;
}

static void
eptintr(Endpt *e, int events)
{
	int len, status, txerr, tag;
	BD *dre;
	Block *b;
	static char *pktype[] = {"OUT0", "OUT1", "SETUP0", "??"};
	char buf[40];

	if(events & Frxb && !e->rxblocked){
		dre = &e->rdr[e->rdrx];
		while(((status = dre->status) & BDEmpty) == 0){
			tag = (dre->status>>6)&3;
			if(tag ==3 && e->x == EPsetup && (*e->usep&RHSmask)==RHSstall)	/* reset STALL status on next SETUP */
				*e->usep = (*e->usep & ~(RHSstall|THSstall)) | RHSok|THSok;
			if(Chatty){
				snprint(buf, sizeof(buf), "usb rx%d t=%s", e->x, pktype[tag]);
				dumpbd(buf, dre, 14);
			}
			if(status & RxErrs || (status & (BDFirst|BDLast)) != (BDFirst|BDLast)){
				if((status & (BDFirst|BDLast)) != (BDFirst|BDLast))
					print("usb: part pkt\n");
				if(status & RxeNO)
					e->alignerrs++;
				if(status & RxeAB)
					e->badframes++;
				if(status & RxeCR)
					e->badcrcs++;
				if(status & RxeOV)
					e->overflows++;
				/* there won't have been a handshake; no need to stall */
			}else if(e->iq != nil){
				/*
				 * We have a packet. Read it into the next
				 * free ring buffer, if any.
				 */
				len = dre->length-2;	/* discard CRC */
				if(len < 0)
					panic("usb: eptintr len=%d\n", dre->length);
				if(e->x == EPsetup && usbctlpacket(e->ctlr, KADDR(dre->addr), len)){
					e->inpackets++;
					e->inbytes += len;
					/* no further processing required */
				}else{
					if(e->type != EPctl &&
					    e->rtog != (status&RxDtype)){
						e->seqerrs++;
						/* let the host time out and reset */
						goto Discard;
					}
					if(qproduce(e->iq, KADDR(dre->addr), len)>=0){
						e->inpackets++;
						e->inbytes += len;
						if(e->type != EPiso)
							e->rtog ^= RxData0^RxData1;
						dcflush(KADDR(dre->addr), len);
					}else{
						e->soverflows++;
						e->rxblocked = 1;
						/* collect it next time */
						break;
					}
				}
			}

		Discard:
			/*
			 * Finished with this descriptor, reinitialise it,
			 * give it back to the chip, then on to the next...
			 */
			dre->length = 0;
			dre->status = (dre->status & BDWrap) | BDEmpty | BDInt;
			eieio();

			e->rdrx = NEXT(e->rdrx, Nrdre);
			dre = &e->rdr[e->rdrx];
		}
		if(qfull(e->iq)){
			e->rxblocked = 1;
			/* inefficient because MPC delivers data anyhow */
			eptsetuse(e, RHSmask, RHSok, RHSnak);
		}
	}

	/*
	 * Transmitter interrupt: handle anything queued for a free descriptor.
	 */
	if(events & (Ftxb|e->txe)){
		txerr = 0;
		ilock(e->ctlr);
		while(e->ntq){
			dre = &e->tdr[e->tdri];
			if(dre->status & BDReady)
				break;
			if(Chatty){
				snprint(buf, sizeof(buf), "usbtx%d", e->x);
				dumpbd(buf, dre, 8);
			}
			if(dre->status & TxErrs){
				if(dre->status & TxeNAK)
					e->txnacks++;
				if(dre->status & TxeTO)
					e->txtimeouts++;
				if(dre->status & TxeSTAL)
					e->txstalls++;
				if(dre->status & TxeUN)
					e->txunderruns++;
				print("USB tx err: %4.4ux", dre->status&TxErrs);
				if(e->x == EPwrite)
					eptsetuse(e, THSmask, THSok, THSstall);
				txerr |= dre->status & (TxeTO|TxeUN);
			}
			b = e->txb[e->tdri];
			if(b == nil || dre->addr != PADDR(b->rp))
				panic("usb/interrupt: bufp");
			e->txb[e->tdri] = nil;
			e->outbytes += BLEN(b);
			e->outpackets++;
			freeb(b);
			e->ntq--;
			e->tdri = NEXT(e->tdri, Ntdre);
		}
		if(0 && txerr)	/* disabled because it doesn't recover properly */
			eptflush(e);
		txstart(e);
		iunlock(e->ctlr);
	}
}

/*
 * called by ../port/qio.c when input queue drains after blocking
 */
static void
eptflow(void *a)
{
	Endpt *e;

	e = a;
	ilock(e->ctlr);
	if(e->rxblocked){
		e->rxblocked = 0;
		eptsetuse(e, RHSmask, RHSnak, RHSok);
		eptintr(e, Frxb);
	}
	iunlock(e->ctlr);
}

static void
interrupt(Ureg*, void *arg)
{
	int events, i;
	Endpt *e;
	Ctlr *ctlr;
	USB *usb;

	ctlr = arg;
	usb = ctlr->usb;
	events = usb->usber;
	eieio();
	usb->usber = events;
	eieio();

	events &= ~Fidle;
	if(events & Fsof){
		if(0)
			print("SOF #%ux\n", ctlr->usbp->frame_n&SOFmask);
	}
	if(events & Freset){
		/* signal reset to application, if active */
		/* flush fifos and restart */
		usb->usadr = 0;	/* on reset, device required to be device 0 */
		e = &ctlr->pts[EPsetup];
		if(e->ref && e->iq != nil)
			qproduce(e->iq, "\xFF\xFFreset\n", 8);
		for(i=1; i<Nendpt; i++)
			eptreset(&ctlr->pts[i]);
		events &= ~Freset;
	}
//events &= ~Ftxe;
	if(events == 0)
		return;
	if(Chatty)
		print("USB#%x\n", events);
	if(events & Fbsy)
		ctlr->pts[EPread].missed++;
	for(i=0; i<Nendpt; i++){
		e = &ctlr->pts[i];
		if(e->rdr != nil /* && e->ref */)
			eptintr(e, events/*&~Ftxe*/);
	}
}

static void
dumpusb(Block *b, char *msg)
{
	int i;

	print("%s: %8.8lux [%ld]: ", msg, (ulong)b->rp, BLEN(b));
	for(i=0; i<BLEN(b) && i < 16; i++)
		print(" %.2x", b->rp[i]);
	print("\n");
}

static Endpt *
endpoint(int i)
{
	if(i < 0 || i >= 3)
		error(Ebadarg);
	return &usbctlr->pts[i];
}

static void
usbreset(void)
{
	resetusb();
}

static void
usbinit(void)
{
	/* could have a kproc to handle control requests? */
}

Chan*
usbattach(char* spec)
{
	return devattach('U', spec);
}

static int
usbwalk(Chan *c, char *name)
{
	return devwalk(c, name, usbtab, nelem(usbtab), devgen);
}

static void
usbstat(Chan *c, char *db)
{
	devstat(c, db, usbtab, nelem(usbtab), devgen);
}

static Chan*
usbopen(Chan *c, int omode)
{
	int i;
	Endpt *e;

	c = devopen(c, omode, usbtab, nelem(usbtab), devgen);
	switch(QID(c->qid.path)){
	default:
		return c;
	case Qsetup:
		e = &usbctlr->pts[EPsetup];
		if(incref(e) > 1){
			incref(usbctlr);
			error(Einuse);	/* usbclose will decref */
		}
		if(e->iq)
			qreopen(e->iq);
		if(e->oq)
			qreopen(e->oq);
		break;
	case Qdata:
		for(i=1; i<Nendpt; i++){
			e = &usbctlr->pts[i];
			if(incref(e) == 1){
				if(e->iq)
					qreopen(e->iq);
				if(e->oq)
					qreopen(e->oq);
			}
		}
		break;
	}
	if(incref(usbctlr) == 1){
		usbctlr->usb->usber = ~0;	/* clear events */
		usbctlr->usb->usmod |= EN;
		tsleep(&up->sleep, return0, 0, 1);	/* must wait a tick before any output (rmagee@audesi.com) */
	}
	return c;
}

static void
usbclose(Chan *c)
{
	int i;

	if((c->flag & COPEN) == 0)
		return;
	switch(QID(c->qid.path)){
	default:
		return;
	case Qsetup:
		eptclose(&usbctlr->pts[EPsetup]);
		break;
	case Qdata:
		for(i=1; i<Nendpt; i++)
			eptclose(&usbctlr->pts[i]);
		break;
	}
	if(decref(usbctlr) == 0)
		usbctlr->usb->usmod &= ~EN;
}

static long
usbread(Chan *c, void *buf, long n, ulong offset)
{
	Ctlr *usb;
	Endpt *e;
	char *s;
	int i, l;

	usb = usbctlr;
	switch(QID(c->qid.path)){
	case Qdir:
		return devdirread(c, buf, n, usbtab, nelem(usbtab), devgen);
	case Qsetup:
		return qread(usb->pts[EPsetup].iq, buf, n);
	case Qdata:
		return qread(usb->pts[EPread].iq, buf, n);
	case Qctl:
		n=0;
		break;
	case Qstat:
		s = malloc(READSTR);
		if(waserror()){
			free(s);
			nexterror();
		}
		l = 0;
		for(i=0; i<Nendpt; i++){
			e = &usbctlr->pts[i];
			l += snprint(s+l, READSTR-l, "%d rtog %d xtog %d max %d in %lud %lud out %lud %lud frame %lud align %lud crc %lud over %lud qfull %lud seq %lud miss %lud usep %4.4ux\n",
				i, (e->rtog&IsData1)!=0, (e->xtog&IsData1)!=0, e->maxpkt, e->inbytes, e->inpackets, e->outbytes, e->outpackets,
				e->badframes, e->alignerrs, e->badcrcs, e->overflows, e->soverflows, e->seqerrs, e->missed, e->usep?*e->usep: 0);
		}
		n = readstr(offset, buf, n, s);
		poperror();
		free(s);
		break;
	case Qaddr:
		return readnum(offset, buf, n, usb->usb->usadr, NUMSIZE);
	case Qframe:
		return readnum(offset, buf, n, usb->usbp->frame_n&SOFmask, NUMSIZE);
	default:
		n=0;
		break;
	}
	return n;
}

/*
 * transmit data from an endpoint, limiting each queued block
 * as required by the endpoint's max packet size.
 */
static long
sendept(Endpt *e, void *buf, long n)
{
	Block *b;
	long nw;
	uchar *a;

	if(e == nil)
		error(Ebadusefd);
	if(e->oq == nil)
		error(Eio);
	if((*e->usep & THSmask) == THSstall)
		error("endpoint stalled");
	if(e->x == EPsetup)
		e->xtog = TxData1;
	a = buf;
	do{
		nw = n;
		if(nw > e->maxpkt)
			nw = e->maxpkt;
		b = allocb(nw);
		if(waserror()){
			freeb(b);
			nexterror();
		}
		memmove(b->wp, a, nw);
		b->wp += nw;
		a += nw;
		poperror();
		qbwrite(e->oq, b);
		transmit(e);
	}while((n -= nw) > 0);
	return a-(uchar*)buf;
}

static long
usbwrite(Chan *c, void *buf, long n, ulong)
{
	Ctlr *usb;
	char cmd[64], *fields[4];
	Endpt *e;
	int nf, i;

	usb = usbctlr;
	switch(QID(c->qid.path)){
	default:
		error(Ebadusefd);
		return 0;
	case Qctl:
		if(n > sizeof(cmd)-1)
			n = sizeof(cmd)-1;
		memmove(cmd, buf, n);
		cmd[n] = 0;
		nf = parsefields(cmd, fields, nelem(fields), " \t\n");
		if(nf < 2)
			error(Ebadarg);
		if(strcmp(fields[0], "addr") == 0){	/* for testing, not part of the spec */
			i = strtol(fields[1], nil, 0);
			if(i < 0 || i >= 128)
				error(Ebadarg);
			/* should wait until idle? */
			usb->usb->usadr = i;
		}else if(strcmp(fields[0], "nak") == 0){	/* for testing, not part of the spec */
			i = strtol(fields[1], nil, 0);
			if(i == EPwrite)
				eptsetuse(&usb->pts[i], THSmask, THSok, THSnak);
			else if(i == EPread)
				eptsetuse(&usb->pts[i], RHSmask, RHSok, RHSnak);
			else
				error(Ebadarg);
		}else if(strcmp(fields[0], "stall") == 0){
			i = strtol(fields[1], nil, 0);
			if(i == EPwrite || i == EPsetup)
				eptsetuse(&usb->pts[i], THSmask, THSok, THSstall);
			else if(i == EPread || i == EPsetup)
				eptsetuse(&usb->pts[i], RHSmask, RHSok, RHSstall);
			else
				error(Ebadarg);
		}else if(strcmp(fields[0], "unstall") == 0){
			i = strtol(fields[1], nil, 0);
			if(i == EPwrite || i == EPsetup)
				eptsetuse(&usb->pts[i], THSmask, THSstall, THSok);
			else if(i == EPread || i == EPsetup)
				eptsetuse(&usb->pts[i], RHSmask, RHSstall, RHSok);
			else
				error(Ebadarg);
		}else if(nf > 2 && strcmp(fields[0], "maxpkt") == 0){
			e = endpoint(strtol(fields[1], nil, 0));
			i = strtol(fields[2], nil, 0);
			if(i < 1 || i > Bufsize || e->x==0 && (i < 8 || i > 64 || (i&(i-1))!=0))
				error(Ebadarg);
			e->maxpkt = i;
		}else if(strcmp(fields[0], "rdtog") == 0){
			i = strtol(fields[1], nil, 0);
			if(nf > 2){
				e = endpoint(i);
				i = strtol(fields[2], nil, 0);
			}else
				e = &usb->pts[EPread];
			e->rtog = i? RxData1: RxData0;
		}else if(strcmp(fields[0], "wrtog") == 0){
			i = strtol(fields[1], nil, 0);
			if(nf > 2){
				e = endpoint(i);
				i = strtol(fields[2], nil, 0);
			}else
				e = &usb->pts[EPwrite];
			e->xtog = i? TxData1: TxData0;
		}else
			error(Ebadarg);
		break;
	case Qsetup:
		n = sendept(&usb->pts[EPsetup], buf, n);
		break;
	case Qdata:
		n = sendept(&usb->pts[EPwrite], buf, n);
		break;
	}
	return n;
}

Dev usbdevtab = {
	'U',
	"usb",

	usbreset,
	usbinit,
	usbattach,
	devdetach,
	devclone,
	usbwalk,
	usbstat,
	usbopen,
	devcreate,
	usbclose,
	usbread,
	devbread,
	usbwrite,
	devbwrite,
	devremove,
	devwstat,
};
