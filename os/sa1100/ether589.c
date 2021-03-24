#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"
#include "../port/error.h"
#include "../port/netif.h"

#include "etherif.h"

#define	DPRINT	     if(0) print
#define XCVRDEBUG    if(0) print
#define IRQDEBUG     if(0) print
//#define DEBUG	     1

enum {						/* all windows */
	CommandR		= 0x000E,
	IntStatusR		= 0x000E,
};

enum {						/* Commands */
	GlobalReset		= 0x0000,
	SelectRegisterWindow	= 0x0001,
	EnableDcConverter	= 0x0002,
	RxDisable		= 0x0003,
	RxEnable		= 0x0004,
	RxReset			= 0x0005,
	Stall			= 0x0006,	/* 3C90x */
	TxDone			= 0x0007,
	RxDiscard		= 0x0008,
	TxEnable		= 0x0009,
	TxDisable		= 0x000A,
	TxReset			= 0x000B,
	RequestInterrupt	= 0x000C,
	AcknowledgeInterrupt	= 0x000D,
	SetInterruptEnable	= 0x000E,
	SetIndicationEnable	= 0x000F,	/* SetReadZeroMask */
	SetRxFilter		= 0x0010,
	SetRxEarlyThresh	= 0x0011,
	SetTxAvailableThresh	= 0x0012,
	SetTxStartThresh	= 0x0013,
	StatisticsEnable	= 0x0015,
	StatisticsDisable	= 0x0016,
	DisableDcConverter	= 0x0017,
	SetTxReclaimThresh	= 0x0018,	/* PIO-only adapters */
	PowerUp			= 0x001B,	/* not all adapters */
	PowerDownFull		= 0x001C,	/* not all adapters */
	PowerAuto		= 0x001D,	/* not all adapters */
};

enum {						/* (Global|Rx|Tx)Reset command bits */
	tpAuiReset		= 0x0001,	/* 10BaseT and AUI transceivers */
	endecReset		= 0x0002,	/* internal Ethernet encoder/decoder */
	networkReset		= 0x0004,	/* network interface logic */
	fifoReset		= 0x0008,	/* FIFO control logic */
	aismReset		= 0x0010,	/* autoinitialise state-machine logic */
	hostReset		= 0x0020,	/* bus interface logic */
	dmaReset		= 0x0040,	/* bus master logic */
	vcoReset		= 0x0080,	/* on-board 10Mbps VCO */
	updnReset		= 0x0100,	/* upload/download (Rx/TX) logic */

	resetMask		= 0x01FF,
};

enum {						/* SetRxFilter command bits */
	receiveIndividual	= 0x0001,	/* match station address */
	receiveMulticast	= 0x0002,
	receiveBroadcast	= 0x0004,
	receiveAllFrames	= 0x0008,	/* promiscuous */
};

enum {						/* IntStatus bits */
	interruptLatch		= 0x0001,
	hostError		= 0x0002,	/* Adapter Failure */
	txComplete		= 0x0004,
	txAvailable		= 0x0008,
	rxComplete		= 0x0010,
	rxEarly			= 0x0020,
	intRequested		= 0x0040,
	updateStats		= 0x0080,
	commandInProgress	= 0x1000,

	interruptMask		= 0x00FE,
};

#define COMMAND(port, cmd, a)	outs((port)+CommandR, ((cmd)<<11)|(a))
#define STATUS(port)		ins((port)+IntStatusR)

enum {						/* Window 0 - setup */
	Wsetup			= 0x0000,
						/* registers */
	ManufacturerID		= 0x0000,	/* 3C5[08]*, 3C59[27] */
	ProductID		= 0x0002,	/* 3C5[08]*, 3C59[27] */
	ConfigControl		= 0x0004,	/* 3C5[08]*, 3C59[27] */
	AddressConfig		= 0x0006,	/* 3C5[08]*, 3C59[27] */
	ResourceConfig		= 0x0008,	/* 3C5[08]*, 3C59[27] */
	EepromCommand		= 0x000A,
	EepromData		= 0x000C,
						/* AddressConfig Bits */
	autoSelect9		= 0x0080,
	xcvrMask9		= 0xC000,
						/* ConfigControl bits */
	Ena			= 0x0001,
	base10TAvailable9	= 0x0200,
	coaxAvailable9		= 0x1000,
	auiAvailable9		= 0x2000,
						/* EepromCommand bits */
	EepromReadRegister	= 0x0080,
	EepromBusy		= 0x8000,
};

#define EEPROMCMD(port, cmd, a)	outs((port)+EepromCommand, (cmd)|(a))
#define EEPROMBUSY(port)	(ins((port)+EepromCommand) & EepromBusy)
#define EEPROMDATA(port)	ins((port)+EepromData)

enum {						/* Window 1 - operating set */
	Wop			= 0x0001,
						/* registers */
	Fifo			= 0x0000,
	RxError			= 0x0004,	/* 3C59[0257] only */
	RxStatus		= 0x0008,
	Timer			= 0x000A,
	TxStatus		= 0x000B,
	TxFree			= 0x000C,
						/* RxError bits */
	rxOverrun		= 0x0001,
	runtFrame		= 0x0002,
	alignmentError		= 0x0004,	/* Framing */
	crcError		= 0x0008,
	oversizedFrame		= 0x0010,
	dribbleBits		= 0x0080,
						/* RxStatus bits */
	rxBytes			= 0x1FFF,	/* 3C59[0257] mask */
	rxBytes9		= 0x07FF,	/* 3C5[078]9 mask */
	rxError9		= 0x3800,	/* 3C5[078]9 error mask */
	rxOverrun9		= 0x0000,
	oversizedFrame9		= 0x0800,
	dribbleBits9		= 0x1000,
	runtFrame9		= 0x1800,
	alignmentError9		= 0x2000,	/* Framing */
	crcError9		= 0x2800,
	rxError			= 0x4000,
	rxIncomplete		= 0x8000,
						/* TxStatus Bits */
	txStatusOverflow	= 0x0004,
	maxCollisions		= 0x0008,
	txUnderrun		= 0x0010,
	txJabber		= 0x0020,
	interruptRequested	= 0x0040,
	txStatusComplete	= 0x0080,
};

enum {						/* Window 2 - station address */
	Wstation		= 0x0002,
};

enum {						/* Window 3 - FIFO management */
	Wfifo			= 0x0003,
						/* registers */
	InternalConfig		= 0x0000,	/* 3C509B, 3C589, 3C59[0257] */
	OtherInt		= 0x0004,	/* 3C59[0257] */
	RomControl		= 0x0006,	/* 3C509B, 3C59[27] */
	MacControl		= 0x0006,	/* 3C59[0257] */
	ResetOptions		= 0x0008,	/* 3C59[0257] */
	RxFree			= 0x000A,
						/* InternalConfig bits */
	disableBadSsdDetect	= 0x00000100,
	ramLocation		= 0x00000200,	/* 0 external, 1 internal */
	ramPartition5to3	= 0x00000000,
	ramPartition3to1	= 0x00010000,
	ramPartition1to1	= 0x00020000,
	ramPartition3to5	= 0x00030000,
	ramPartitionMask	= 0x00030000,
	xcvr10BaseT		= 0x00000000,
	xcvrAui			= 0x00100000,	/* 10BASE5 */
	xcvr10Base2		= 0x00300000,
	xcvr100BaseTX		= 0x00400000,
	xcvr100BaseFX		= 0x00500000,
	xcvrMii			= 0x00600000,
	xcvrMask		= 0x00700000,
	autoSelect		= 0x01000000,
						/* MacControl bits */
	deferExtendEnable	= 0x0001,
	deferTimerSelect	= 0x001E,	/* mask */
	fullDuplexEnable	= 0x0020,
	allowLargePackets	= 0x0040,
	extendAfterCollision	= 0x0080,	/* 3C90xB */
	flowControlEnable	= 0x0100,	/* 3C90xB */
	vltEnable		= 0x0200,	/* 3C90xB */
						/* ResetOptions bits */
	baseT4Available		= 0x0001,
	baseTXAvailable		= 0x0002,
	baseFXAvailable		= 0x0004,
	base10TAvailable	= 0x0008,
	coaxAvailable		= 0x0010,
	auiAvailable		= 0x0020,
	miiConnector		= 0x0040,
};

enum {						/* Window 4 - diagnostic */
	Wdiagnostic		= 0x0004,
						/* registers */
	VcoDiagnostic		= 0x0002,
	FifoDiagnostic		= 0x0004,
	NetworkDiagnostic	= 0x0006,
	PhysicalMgmt		= 0x0008,
	MediaStatus		= 0x000A,
	BadSSD			= 0x000C,
	UpperBytesOk		= 0x000D,
						/* FifoDiagnostic bits */
	txOverrun		= 0x0400,
	rxUnderrun		= 0x2000,
	receiving		= 0x8000,
						/* PhysicalMgmt bits */
	mgmtClk			= 0x0001,
	mgmtData		= 0x0002,
	mgmtDir			= 0x0004,
	cat5LinkTestDefeat	= 0x8000,
						/* MediaStatus bits */
	dataRate100		= 0x0002,
	crcStripDisable		= 0x0004,
	enableSqeStats		= 0x0008,
	collisionDetect		= 0x0010,
	carrierSense		= 0x0020,
	jabberGuardEnable	= 0x0040,
	linkBeatEnable		= 0x0080,
	jabberDetect		= 0x0200,
	polarityReversed	= 0x0400,
	linkBeatDetect		= 0x0800,
	txInProg		= 0x1000,
	dcConverterEnabled	= 0x4000,
	auiDisable		= 0x8000,	/* 10BaseT transceiver selected */
};

enum {						/* Window 5 - internal state */
	Wstate			= 0x0005,
						/* registers */
	TxStartThresh		= 0x0000,
	TxAvailableThresh	= 0x0002,
	RxEarlyThresh		= 0x0006,
	RxFilter		= 0x0008,
	InterruptEnable		= 0x000A,
	IndicationEnable	= 0x000C,
};

enum {						/* Window 6 - statistics */
	Wstatistics		= 0x0006,
						/* registers */
	CarrierLost		= 0x0000,
	SqeErrors		= 0x0001,
	MultipleColls		= 0x0002,
	SingleCollFrames	= 0x0003,
	LateCollisions		= 0x0004,
	RxOverruns		= 0x0005,
	FramesXmittedOk		= 0x0006,
	FramesRcvdOk		= 0x0007,
	FramesDeferred		= 0x0008,
	UpperFramesOk		= 0x0009,
	BytesRcvdOk		= 0x000A,
	BytesXmittedOk		= 0x000C,
};

enum {
	SelectWindow	= 0x01,		/* SelectWindow command */
	EEPROMcmd	= 0x0A,
	EEPROMdata	= 0x0C,
};


typedef struct {
	Lock	wlock;				/* window access */

	int	attached;
	Block*	rbp;				/* receive buffer */

	Block*	txbp;				/* FIFO -based transmission */
	int	txthreshold;
	int	txbusy;

	long	interrupts;			/* statistics */
	long	timer;
	long	stats[BytesRcvdOk+3];

	int	xcvr;				/* transceiver type */
	int	rxearly;			/* RxEarlyThreshold */
	int	ts;				/* threshold shift */
} Ctlr;


static Block*
rbpalloc(Block* (*f)(int))
{
	Block *bp;
	ulong addr;

	/* allocate on 32-byte boundary */
	if(bp = f(ROUNDUP(sizeof(Etherpkt), 4) + 31)){
		addr = (ulong)bp->base;
		addr = ROUNDUP(addr, 32);
		bp->rp = (uchar*)addr;
	}

	return bp;
}

static void
promiscuous(void* arg, int on)
{
	int filter, port;
	Ether *ether;

	ether = (Ether*)arg;
	port = ether->port;

	filter = receiveBroadcast|receiveIndividual;
	if(ether->nmaddr)
		filter |= receiveMulticast;
	if(on)
		filter |= receiveAllFrames;
	COMMAND(port, SetRxFilter, filter);
}

static void
multicast(void* arg, uchar *addr, int on)
{
	int filter, port;
	Ether *ether;

	USED(addr, on);

	ether = (Ether*)arg;
	port = ether->port;

	filter = receiveBroadcast|receiveIndividual;
	if(ether->nmaddr)
		filter |= receiveMulticast;
	if(ether->prom)
		filter |= receiveAllFrames;
	COMMAND(port, SetRxFilter, filter);
}

static void
attach(Ether* ether)
{
	int port, mask;
	Ctlr *ctlr;

	ctlr = ether->ctlr;
	ilock(&ctlr->wlock);
	if(ctlr->attached){
		iunlock(&ctlr->wlock);
		return;
	}

	/*
	 * Set the receiver packet filter for this and broadcast addresses,
	 * set the interrupt masks for all interrupts, enable the receiver
	 * and transmitter.
	 */
	promiscuous(ether, ether->prom);

	port = ether->port;
	COMMAND(port, RxEnable, 0);
	COMMAND(port, TxEnable, 0);

	COMMAND(port, SetIndicationEnable, interruptMask);
	COMMAND(port, SetInterruptEnable, interruptMask);
	COMMAND(port, AcknowledgeInterrupt, 0xff);
	COMMAND(port, AcknowledgeInterrupt, 0xff);

	/* configure PCMCIA interrupt pin */
        mask = (1<<ether->irq);
        *GFER |= mask;			/* falling edge */
        *GRER &= ~mask;			/* not rising edge */
        *GPDR &= ~mask;			/* input */

	ctlr->attached = 1;
	iunlock(&ctlr->wlock);
}

static void
statistics(Ether* ether)
{
	int port, i, u, w;
	Ctlr *ctlr;

	port = ether->port;
	ctlr = ether->ctlr;

	/*
	 * 3C59[27] require a read between a PIO write and
	 * reading a statistics register.
	 */
	w = (STATUS(port)>>13) & 0x07;
	COMMAND(port, SelectRegisterWindow, Wstatistics);
	STATUS(port);

	for(i = 0; i < UpperFramesOk; i++)
		ctlr->stats[i] += inb(port+i) & 0xFF;
	u = inb(port+UpperFramesOk) & 0xFF;
	ctlr->stats[FramesXmittedOk] += (u & 0x30)<<4;
	ctlr->stats[FramesRcvdOk] += (u & 0x03)<<8;
	ctlr->stats[BytesRcvdOk] += ins(port+BytesRcvdOk) & 0xFFFF;
	ctlr->stats[BytesRcvdOk+1] += ins(port+BytesXmittedOk) & 0xFFFF;

	switch(ctlr->xcvr){

	case xcvrMii:
	case xcvr100BaseTX:
	case xcvr100BaseFX:
		COMMAND(port, SelectRegisterWindow, Wdiagnostic);
		STATUS(port);
		ctlr->stats[BytesRcvdOk+2] += inb(port+BadSSD);
		break;
	}

	COMMAND(port, SelectRegisterWindow, w);
}

static void
txstart(Ether* ether)
{
	int port, len, fifo;
	Ctlr *ctlr;
	Block *bp;

	port = ether->port;
	fifo = port+Fifo;
	ctlr = ether->ctlr;

	/*
	 * Attempt to top-up the transmit FIFO. If there's room simply
	 * stuff in the packet length (unpadded to a dword boundary), the
	 * packet data (padded) and remove the packet from the queue.
	 * If there's no room post an interrupt for when there is.
	 * This routine is called both from the top level and from interrupt
	 * level and expects to be called with ctlr->wlock already locked
	 * and the correct register window (Wop) in place.
	 */
	for(;;) {
		if (ctlr->txbp){
			bp = ctlr->txbp;
			ctlr->txbp = 0;
		} else {
			bp = qget(ether->oq);
			if(bp == nil)
				break;
		}

		len = ROUNDUP(BLEN(bp), 4);
		if(len+4 <= ins(port+TxFree)){
			/* 32 bit header */
			outs(fifo, BLEN(bp));
			outs(fifo, 0x0000);
			/* data rounded to 32 bits */
			{
			uchar *p = (uchar *)bp->rp;
			uchar *e = p + len;

			while (p < e) {
				outs(fifo, p[1] << 8 | p[0]);
				outs(fifo, p[3] << 8 | p[2]);
				p += 4;
			}
			}

			freeb(bp);
			ether->outpackets++;
		} else {
			ctlr->txbp = bp;
			if(ctlr->txbusy == 0){
				ctlr->txbusy = 1;
				COMMAND(port, SetTxAvailableThresh, len>>ctlr->ts);
			}
			break;
		}
	}
}

static void
transmit(Ether* ether)
{
	Ctlr *ctlr;
	int port, w;

	port = ether->port;
	ctlr = ether->ctlr;

	ilock(&ctlr->wlock);
	w = (STATUS(port)>>13) & 0x07;
	COMMAND(port, SelectRegisterWindow, Wop);
	txstart(ether);
	COMMAND(port, SelectRegisterWindow, w);
	iunlock(&ctlr->wlock);
}

static void
receive(Ether* ether)
{
	int len, port, rxstatus;
	Ctlr *ctlr;
	Block *bp, *rbp;

	port = ether->port;
	ctlr = ether->ctlr;

	while(((rxstatus = ins(port+RxStatus)) & rxIncomplete) == 0){

		/* If there was an error, log it and continue.  */
		if (rxstatus & rxError) {
			switch (rxstatus & rxError9) {

			case rxOverrun9:
				ether->overflows++;
				break;

			case oversizedFrame9:
			case runtFrame9:
				ether->buffs++;
				break;

			case alignmentError9:
				ether->frames++;
				break;

			case crcError9:
				ether->crcs++;
				break;

			}
			goto next;
		}

		/*
		 * If there was an error or a new receive buffer can't be
		 * allocated, discard the packet and go on to the next.
		 */
		bp = rbpalloc(iallocb);
		if (!bp)
			goto next;

		rbp = ctlr->rbp;
		ctlr->rbp = bp;

		/*
		 * A valid receive packet awaits:
		 *	if using PIO, read it into the buffer;
		 *	discard the packet from the FIFO;
		 *	pass the packet on to whoever wants it.
		 */
		len = (rxstatus & rxBytes9);
		rbp->wp = rbp->rp + len;
		{
		int i, count = HOWMANY(len, 4);
		ulong *p = (ulong *)rbp->rp;
		ulong ioaddr = port+Fifo;

		for (i = 0; i < count; i++) {
			*p = (ins(ioaddr) << 16 | ins(ioaddr));
			p++;
		}
		}

		etheriq(ether, rbp, 1);

	next:	
		COMMAND(port, RxDiscard, 0);
		while(STATUS(port) & commandInProgress)
			;

	}
}

static void
interrupt(Ureg*, void* arg)
{
	Ether *ether;
	int port, status, s, w, x;
	Ctlr *ctlr;

	ether = arg;
	port = ether->port;
	ctlr = ether->ctlr;

	/* ack interrupt (clear edge detect) */
        *GEDR = (1 << ether->irq); 

	ilock(&ctlr->wlock);
	w = (STATUS(port)>>13) & 0x07;
	COMMAND(port, SelectRegisterWindow, Wop);

	IRQDEBUG("IRQ");
	ctlr->interrupts++;
	ctlr->timer += inb(port+Timer) & 0xFF;
	while((status = STATUS(port)) & (interruptMask|interruptLatch)){
		IRQDEBUG(" ");
		if(status & hostError){
			IRQDEBUG("hostError");
			/*
			 * Adapter failure, try to find out why, reset if
			 * necessary. What happens if Tx is active and a reset
			 * occurs, need to retransmit? This probably isn't right.
			 */
			COMMAND(port, SelectRegisterWindow, Wdiagnostic);
			x = ins(port+FifoDiagnostic);
			COMMAND(port, SelectRegisterWindow, Wop);
			print("#l%d: status 0x%uX, diag 0x%uX\n", ether->ctlrno, status, x);

			if(x & txOverrun){
				COMMAND(port, TxReset, 0);
				COMMAND(port, TxEnable, 0);
			}

			if(x & rxUnderrun){
				/*
				 * This shouldn't happen...
				 * Reset the receiver and restore the filter and RxEarly
				 * threshold before re-enabling.
				 */
				COMMAND(port, SelectRegisterWindow, Wstate);
				s = (port+RxFilter) & 0x000F;
				COMMAND(port, SelectRegisterWindow, Wop);
				COMMAND(port, RxReset, 0);
				while(STATUS(port) & commandInProgress)
					;
				COMMAND(port, SetRxFilter, s);
				COMMAND(port, SetRxEarlyThresh, ctlr->rxearly>>ctlr->ts);
				COMMAND(port, RxEnable, 0);
			}

			status &= ~hostError;
		}

		if(status & (rxComplete)){
			IRQDEBUG("rxComplete");
			receive(ether);
			status &= ~(rxComplete);
		}

		if(status & txComplete){
			IRQDEBUG("txComplete");
			/*
			 * Pop the TxStatus stack, accumulating errors.
			 * Adjust the TX start threshold if there was an underrun.
			 * If there was a Jabber or Underrun error, reset
			 * the transmitter.
			 * For all conditions enable the transmitter.
			 */
			s = 0;
			do {
				if(x = inb(port+TxStatus))
					outb(port+TxStatus, 0);
				s |= x;
			} while(STATUS(port) & txComplete);

			if(s & txUnderrun){
				COMMAND(port, SelectRegisterWindow, Wdiagnostic);
				while(ins(port+MediaStatus) & txInProg)
					;
				COMMAND(port, SelectRegisterWindow, Wop);
				if(ctlr->txthreshold < ETHERMAXTU)
					ctlr->txthreshold += ETHERMINTU;
			}

			/*
			 * According to the manual, maxCollisions does not require
			 * a TxReset, merely a TxEnable. However, evidence points to
			 * it being necessary on the 3C905. The jury is still out.
			 */
			if (s & (txJabber|txUnderrun|maxCollisions)){
				COMMAND(port, TxReset, 0);
				while(STATUS(port) & commandInProgress)
					;
				COMMAND(port, SetTxStartThresh, ctlr->txthreshold>>ctlr->ts);
			}

			print("#l%d: txstatus 0x%uX, threshold %d\n", ether->ctlrno, s, ctlr->txthreshold);
			COMMAND(port, TxEnable, 0);
			ether->oerrs++;
			status &= ~txComplete;
			status |= txAvailable;
		}

		if(status & txAvailable){
			IRQDEBUG("txAvailable");
			COMMAND(port, AcknowledgeInterrupt, txAvailable);
			ctlr->txbusy = 0;
			txstart(ether);
			status &= ~txAvailable;
		}

		if(status & updateStats){
			IRQDEBUG("updateStats");
			statistics(ether);
			status &= ~updateStats;
		}

		/* Currently, this shouldn't happen.  */
		if(status & rxEarly){
			IRQDEBUG("rxEarly");
			COMMAND(port, AcknowledgeInterrupt, rxEarly);
			status &= ~rxEarly;
		}

		/* Panic if there are any interrupts not dealt with.  */
		if(status & interruptMask)
			panic("#l%d: interrupt mask 0x%uX\n", ether->ctlrno, status);

		COMMAND(port, AcknowledgeInterrupt, interruptLatch);
	}

	COMMAND(port, SelectRegisterWindow, w);
	iunlock(&ctlr->wlock);
}

static long
ifstat(Ether* ether, void* a, long n, ulong offset)
{
	char *p;
	int len;
	Ctlr *ctlr;

	if(n == 0)
		return 0;

	ctlr = ether->ctlr;

	ilock(&ctlr->wlock);
	statistics(ether);
	iunlock(&ctlr->wlock);

	p = malloc(READSTR);
	len = snprint(p, READSTR, "interrupts: %lud\n", ctlr->interrupts);
	len += snprint(p+len, READSTR-len, "timer: %lud\n", ctlr->timer);
	len += snprint(p+len, READSTR-len, "carrierlost: %lud\n", ctlr->stats[CarrierLost]);
	len += snprint(p+len, READSTR-len, "sqeerrors: %lud\n", ctlr->stats[SqeErrors]);
	len += snprint(p+len, READSTR-len, "multiplecolls: %lud\n", ctlr->stats[MultipleColls]);
	len += snprint(p+len, READSTR-len, "singlecollframes: %lud\n", ctlr->stats[SingleCollFrames]);
	len += snprint(p+len, READSTR-len, "latecollisions: %lud\n", ctlr->stats[LateCollisions]);
	len += snprint(p+len, READSTR-len, "rxoverruns: %lud\n", ctlr->stats[RxOverruns]);
	len += snprint(p+len, READSTR-len, "framesxmittedok: %lud\n", ctlr->stats[FramesXmittedOk]);
	len += snprint(p+len, READSTR-len, "framesrcvdok: %lud\n", ctlr->stats[FramesRcvdOk]);
	len += snprint(p+len, READSTR-len, "framesdeferred: %lud\n", ctlr->stats[FramesDeferred]);
	len += snprint(p+len, READSTR-len, "bytesrcvdok: %lud\n", ctlr->stats[BytesRcvdOk]);
	len += snprint(p+len, READSTR-len, "bytesxmittedok: %lud\n", ctlr->stats[BytesRcvdOk+1]);

	snprint(p+len, READSTR-len, "badssd: %lud\n", ctlr->stats[BytesRcvdOk+2]);

	n = readstr(offset, a, n, p);
	free(p);

	return n;
}

static void
setxcvr(int port, int xcvr, int)
{
	int x;

	COMMAND(port, SelectRegisterWindow, Wsetup);
	x = ins(port+AddressConfig) & ~xcvrMask9;
	x |= (xcvr>>20)<<14;
	outs(port+AddressConfig, x);

	COMMAND(port, TxReset, 0);
	while(STATUS(port) & commandInProgress)
		;
	COMMAND(port, RxReset, 0);
	while(STATUS(port) & commandInProgress)
		;
}

static int
autoselect(int port)
{
	int media, x;

	/*
	 * Pathetic attempt at automatic media selection.
	 * Really just to get the Fast Etherlink 10BASE-T/100BASE-TX
	 * cards operational.
	 * It's a bonus if it works for anything else.
	 */
	COMMAND(port, SelectRegisterWindow, Wsetup);
	x = ins(port+ConfigControl);
	media = 0;
	if(x & base10TAvailable9)
		media |= base10TAvailable;
	if(x & coaxAvailable9)
		media |= coaxAvailable;
	if(x & auiAvailable9)
		media |= auiAvailable;

	XCVRDEBUG("autoselect: media %uX\n", media);

	COMMAND(port, SelectRegisterWindow, Wdiagnostic);
	XCVRDEBUG("autoselect: media status %uX\n", ins(port+MediaStatus));

	if(media & baseTXAvailable){
		/*
		 * Must have InternalConfig register.
		 */
		setxcvr(port, xcvr100BaseTX, 1);

		COMMAND(port, SelectRegisterWindow, Wdiagnostic);
		x = ins(port+MediaStatus) & ~(dcConverterEnabled|jabberGuardEnable);
		outs(port+MediaStatus, linkBeatEnable|x);
		delay(10);

		if(ins(port+MediaStatus) & linkBeatDetect)
			return xcvr100BaseTX;
		outs(port+MediaStatus, x);
	}

	if(media & base10TAvailable){
		setxcvr(port, xcvr10BaseT, 1);

		COMMAND(port, SelectRegisterWindow, Wdiagnostic);
		x = ins(port+MediaStatus) & ~dcConverterEnabled;
		outs(port+MediaStatus, linkBeatEnable|jabberGuardEnable|x);
		delay(100);

		XCVRDEBUG("autoselect: 10BaseT media status %uX\n", ins(port+MediaStatus));
		if(ins(port+MediaStatus) & linkBeatDetect)
			return xcvr10BaseT;
		outs(port+MediaStatus, x);
	}

	/*
	 * Botch.
	 */
	return autoSelect;
}

static int
eepromdata(int port, int offset)
{
	COMMAND(port, SelectRegisterWindow, Wsetup);
	while(EEPROMBUSY(port))
		;
	EEPROMCMD(port, EepromReadRegister, offset);
	while(EEPROMBUSY(port))
		;
	return EEPROMDATA(port);
}


extern int etherelnk3reset(Ether*);
static Ether *dbg_ether;

static int
reset(Ether *ether)
{
	int did, i, slot, port, rxearly, x, xcvr;
	uchar ea[Eaddrlen];
	Ctlr *ctlr;

	if(ether->port == 0)
		ether->port = 0x240;		/* default position in PCMCIA space */

	slot = pcmspecial("3C589", ether);
	if(slot < 0) 
		slot = pcmspecial("589E", ether);
	if(slot < 0)
		return -1;
	DPRINT("Ethernet found in slot #%d\n",slot);

	/* save pointer for debugging */
	dbg_ether = ether;

	port = ether->port;
	ether->pcmslot = slot;

	/* configure to use PCMCIA interrupt */
	ether->irq = ether->sairq;
	ether->tbdf = BusGPIO;

	/* set Window 0 configuration registers */
	COMMAND(port, SelectWindow, 0);

	/* ROM size & base - must be set before we can access ROM */
	/* transceiver type (for now always 10baseT) */
	x = ins(port + AddressConfig);
	outs(port + AddressConfig, x & 0x20);

	/* IRQ must be 3 on 3C589 */
	x = ins(port + ResourceConfig);
	outs(port + ResourceConfig, 0x3f00 | (x&0xfff));

	/* move product ID to register */
	while(ins(port+EEPROMcmd) & 0x8000)
		;
	outs(port+EEPROMcmd, (2<<6)|3);
	while(ins(port+EEPROMcmd) & 0x8000)
		;
	x = ins(port+EEPROMdata);
	outs(port + ProductID, x);

	COMMAND(port, SelectRegisterWindow, Wsetup);
	x = ins(port+AddressConfig);
	xcvr = ((x & xcvrMask9)>>14)<<20;
	if(x & autoSelect9)
		xcvr |= autoSelect;
	rxearly = 2044;

	/*
	 * Check if the adapter's station address is to be overridden.
	 * If not, read it from the EEPROM and set in ether->ea prior to loading the
	 * station address in Wstation. The EEPROM returns 16-bits at a time.
	 */
	memset(ea, 0, Eaddrlen);
	if(memcmp(ea, ether->ea, Eaddrlen) == 0){
		for(i = 0; i < Eaddrlen/2; i++){
			x = eepromdata(port, i);
			ether->ea[2*i] = x>>8;
			ether->ea[2*i+1] = x;
		}
	}

	COMMAND(port, SelectRegisterWindow, Wstation);
	for(i = 0; i < Eaddrlen; i++)
		outb(port+i, ether->ea[i]);

	XCVRDEBUG("reset: xcvr %uX\n", xcvr);

	if(xcvr & autoSelect) {
		xcvr = autoselect(port);
		XCVRDEBUG("autoselect returns: xcvr %uX, did 0x%uX\n", xcvr, did);
	}
	
	switch(xcvr){
	case xcvr10BaseT:
		/*
		 * Enable Link Beat and Jabber to start the
		 * transceiver.
		 */
		XCVRDEBUG("Enabling 10Base-T ...\n");
		COMMAND(port, SelectRegisterWindow, Wdiagnostic);
		x = ins(port+MediaStatus) & ~dcConverterEnabled;
		x |= linkBeatEnable | jabberGuardEnable;
		outs(port+MediaStatus, x);

		/* check the line status */
		delay(1);
		x = ins(port+MediaStatus);
		if (!(x & auiDisable))
			print("ERROR: 10Base-T not enabled!\n");
		if (!(x & linkBeatDetect))
			print("WARNING: No 10Base-T Link Detected\n");
		if (x & polarityReversed)
			print("WARNING: 10Base-T Parity Reversal Detected\n");
		break;
		
	case xcvr10Base2:
		XCVRDEBUG("Enabling 10Base-2 ...\n");
		COMMAND(port, SelectRegisterWindow, Wdiagnostic);
		x = ins(port+MediaStatus) & ~(linkBeatEnable|jabberGuardEnable);
		outs(port+MediaStatus, x);

		/*
		 * Start the DC-DC converter.
		 * Wait > 800 microseconds.
		 */
		COMMAND(port, EnableDcConverter, 0);
		delay(1);
		break;
	}

	/*
	 * Wop is the normal operating register set.
	 * The 3C59[0257] adapters allow access to more than one register window
	 * at a time, but there are situations where switching still needs to be
	 * done, so just do it.
	 * Clear out any lingering Tx status.
	 */
	COMMAND(port, SelectRegisterWindow, Wop);
	while(inb(port+TxStatus))
		outb(port+TxStatus, 0);

	/*
	 * Allocate a controller structure, clear out the
	 * adapter statistics, clear the statistics logged into ctlr
	 * and enable statistics collection. Xcvr is needed in order
	 * to collect the BadSSD statistics.
	 */
	ether->ctlr = malloc(sizeof(Ctlr));
	ctlr = ether->ctlr;

	ilock(&ctlr->wlock);
	ctlr->xcvr = xcvr;
	statistics(ether);
	memset(ctlr->stats, 0, sizeof(ctlr->stats));

	ctlr->xcvr = xcvr;
	ctlr->rxearly = rxearly;
	if(rxearly >= 2048)
		ctlr->ts = 2;

	COMMAND(port, StatisticsEnable, 0);

	/*
	 * Allocate any receive buffers.
	 */
	ctlr->rbp = rbpalloc(allocb);

	/*
	 * Set a base TxStartThresh which will be incremented
	 * if any txUnderrun errors occur and ensure no RxEarly
	 * interrupts happen.
	 */
	ctlr->txthreshold = ETHERMAXTU/2;
	COMMAND(port, SetTxStartThresh, ctlr->txthreshold>>ctlr->ts);
	COMMAND(port, SetRxEarlyThresh, rxearly>>ctlr->ts);

	iunlock(&ctlr->wlock);

	/* Linkage to the generic ethernet driver.  */
	ether->port = port;
	ether->attach = attach;
	ether->transmit = transmit;
	ether->interrupt = interrupt;
	ether->ifstat = ifstat;
	ether->promiscuous = promiscuous;
	ether->multicast = multicast;
	ether->arg = ether;

	return 0;
}

#ifdef DEBUG
static void
dbg_status(Rune)
{
	Ctlr *ctlr;
        int port, w, i, s;

	if (!dbg_ether) {
		print("3c589 Card not found\n");
		return;
	}

        port = dbg_ether->port;
        ctlr = dbg_ether->ctlr;
	
	print("Interrupt count: %ld\n", ctlr->interrupts);

	/* dump status registers */
	ilock(&ctlr->wlock);
	s = STATUS(port);
        w = (s>>13) & 0x07;

	print("Status Register:\n");
	print("\tCurrent Window: 0x%x\n", w);
	print("\tCommand in Progress: %d\n", (s & 0x1000) ? 1 : 0);
	print("\tUpdate Statistics: %d\n", (s & 0x0080) ? 1 : 0);
	print("\tInterrupt Requested: %d\n", (s & 0x0040) ? 1 : 0);
	print("\tRX Early: %d\n", (s & 0x0020) ? 1 : 0);
	print("\tRX Complete: %d\n", (s & 0x0010) ? 1 : 0);
	print("\tTX Available: %d\n", (s & 0x0008) ? 1 : 0);
	print("\tTX Complete: %d\n", (s & 0x0004) ? 1 : 0);
	print("\tAdapter Failure: %d\n", (s & 0x0002) ? 1 : 0);
	print("\tInterrupt Latch: %d\n", (s & 0x0001) ? 1 : 0);

	COMMAND(port, SelectRegisterWindow, Wstate);
	STATUS(port);
	print("Command Results and Internal State:\n");
	print("\tTX Start Threshold: 0x%x\n", ins(port+TxStartThresh));
	print("\tTX Available Threshold: 0x%x\n", ins(port+TxAvailableThresh));
	print("\tRX Early Threshold: 0x%x\n", ins(port+RxEarlyThresh));
	print("\tRX Filter: 0x%x\n", ins(port+RxFilter));
	print("\tInterrupt Enable: 0x%x\n", ins(port+InterruptEnable));
	print("\tIndication Enable: 0x%x\n", ins(port+IndicationEnable));

        COMMAND(port, SelectRegisterWindow, Wdiagnostic);
	STATUS(port);

	i = ins(port+MediaStatus);
	print("Media Type and Status Port:\n");
	print("\tTP (10Base-T) Enabled: %d\n", (i & 0x8000) ? 1 : 0);
	print("\tCoax (10Base-2) Enabled: %d\n", (i & 0x4000) ? 1 : 0);
	print("\tSQE Present: %d\n", (i & 0x1000) ? 1 : 0);
	print("\tValid Link Beat Detected (TP): %d\n", (i & 0x0800) ? 1 : 0);
	print("\tPolarity Reversal Delected (TP): %d\n", (i & 0x0400) ? 1 : 0);
	print("\tJabber Detected (TP): %d\n", (i & 0x0200) ? 1 : 0);
	print("\tUnsquelch (TP): %d\n", (i & 0x0100) ? 1 : 0);
	print("\tLink Beat Enabled (TP): %d\n", (i & 0x0080) ? 1 : 0);
	print("\tJabber Enabled (TP): %d\n", (i & 0x0040) ? 1 : 0);
	print("\tCarrier sense (CRS): %d\n", (i & 0x0020) ? 1 : 0);
	print("\tCollision: %d\n", (i & 0x0010) ? 1 : 0);
	print("\tSQE Statistics Enabled: %d\n", (i & 0x0008) ? 1 : 0);
	print("\tCRC Strip Disable: %d\n", (i & 0x0004) ? 1 : 0);

	i = ins(port+NetworkDiagnostic);
	print("Net Diagnostic Port:\n");
	print("\tExternal Loopback: %d\n", (i & 0x8000) ? 1 : 0);
	print("\tENDAC Loopback: %d\n", (i & 0x4000) ? 1 : 0);
	print("\tController Loopback: %d\n", (i & 0x2000) ? 1 : 0);
	print("\tFIFO Loopback: %d\n", (i & 0x1000) ? 1 : 0);
	print("\tTX Enabled: %d\n", (i & 0x0800) ? 1 : 0);
	print("\tRX Enabled: %d\n", (i & 0x0400) ? 1 : 0);
	print("\tTX Transmitting: %d\n", (i & 0x0200) ? 1 : 0);
	print("\tTX Reset Req.: %d\n", (i & 0x0100) ? 1 : 0);
	print("\tStatistics Enabled: %d\n", (i & 0x0080) ? 1 : 0);
	{
	char rev[16] = {"? BCDEFGHIJKLMNO"};
	print("\tASIC Revision: 3C589%c\n", rev[(i >> 1) & 0xf]);
	}

        i = ins(port+FifoDiagnostic);
        print("FIFO Diagnostic Port:\n");
        print("\tRX Receiving: %d\n", (i & 0x8000) ? 1 : 0);
        print("\tRX Underrun: %d\n", (i & 0x2000) ? 1 : 0);
        print("\tRX Status Underrun: %d\n", (i & 0x1000) ? 1 : 0);
        print("\tRX Overrun: %d\n", (i & 0x0800) ? 1 : 0);
        print("\tTX Overrun: %d\n", (i & 0x0400) ? 1 : 0);

        COMMAND(port, SelectRegisterWindow, w);
        iunlock(&ctlr->wlock);

	interrupt(0, dbg_ether);
}
#endif

void
ether589link(void)
{
#ifdef DEBUG
	debugkey('3', "3c589 Status", dbg_status, 0);
#endif
	addethercard("3C589", reset);
}


