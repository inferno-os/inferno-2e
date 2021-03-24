/*
 * TDK LAC-CD021L ethernet PCMCIA card driver.
 */
#include "u.h"
#include "../port/lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"

#include "../port/error.h"
#include "../port/netif.h"

#include "etherif.h"

static int tdk_debug = 0;
#define DPRINT if(tdk_debug)print
#define DPRINT1 if(tdk_debug > 1)print
static void prtdbg(void);

/*
    card type
 */
typedef enum {
	MBH10302,
	MBH10304,
	TDK,
	CONTEC,
	LA501,
	cardtype = TDK,
	sram_config = 0,
};


/*====================================================================*/
/* 
 *	io port offsets from the base address 
 */
enum {
	TX_STATUS =	0,	/* transmit status register */
	RX_STATUS =	1,	/* receive status register */
	TX_INTR =	2,	/* transmit interrupt mask register */
	RX_INTR =	3,	/* receive interrupt mask register */
	TX_MODE =	4,	/* transmit mode register */
	RX_MODE =	5,	/* receive mode register */
	CONFIG_0 =	6,	/* configuration register 0 */
	CONFIG_1 =	7,	/* configuration register 1 */

	NODE_ID =	8,	/* node ID register            (bank 0) */
	MAR_ADR =	8,	/* multicast address registers (bank 1) */

	DATAPORT =	8,	/* buffer mem port registers   (bank 2) */
	TX_START =	10,	/* transmit start register */
	COL_CTRL =	11,	/* 16 collision control register */
	BMPR12 =	12,	/* reserved */
	BMPR13 =	13,	/* reserved */
	RX_SKIP =	14,	/* skip received packet register */

	LAN_CTRL =	16,	/* LAN card control register */

	MAC_ID =	0x1a,	/* hardware address */
};

/* 
    TX status & interrupt enable bits 
 */
enum {
	TX_TMT_OK =	0x80,
	TX_NET_BSY =	0x40,	/* carrier is detected */
	TX_ERR =	0x10,
	TX_COL =	0x04,
	TX_16_COL =	0x02,
	TX_TBUS_ERR =	0x01,
};

/* 
    RX status & interrupt enable bits 
 */

enum {
	RX_PKT_RDY =	0x80,	/* packet(s) in buffer */
	RX_BUS_ERR =	0x40,	/* bus read error */
	RX_DMA_EOP =	0x20,
	RX_LEN_ERR =	0x08,	/* short packet */
	RX_ALG_ERR =	0x04,	/* frame error */
	RX_CRC_ERR =	0x02,	/* CRC error */
	RX_OVR_FLO =	0x01,	/* overflow error */

};

/*
 * Receiver mode 
 */
enum {
	RM_BUF_EMP =	0x40 /* receive buffer is empty */
};

/*
 * Receiver pointer control
 */
enum {
	RP_SKP_PKT =	0x05 /* drop packet in buffer */
};

/* default bitmaps */
#define D_TX_INTR  ( TX_TMT_OK )
#define D_RX_INTR  ( RX_PKT_RDY | RX_LEN_ERR \
		   | RX_ALG_ERR | RX_CRC_ERR | RX_OVR_FLO )
#define TX_STAT_M  ( TX_TMT_OK )
#define RX_STAT_M  ( RX_PKT_RDY | RX_LEN_ERR \
                   | RX_ALG_ERR | RX_CRC_ERR | RX_OVR_FLO )

/* RX & TX mode settings */
enum {
	D_TX_MODE =	0x06,	/* no tests, detect carrier */
	ID_MATCHED =	0x02,	/* (RX_MODE) */
	RECV_ALL =	0x03,	/* (RX_MODE) */
};

/*
 * config_0
 */
enum {
	CONFIG0_DFL =	0x5a,	/* 16bit bus, 4K x 2 Tx queues */
	CONFIG0_DFL_1 =	0x5e,	/* 16bit bus, 8K x 2 Tx queues */
	CONFIG0_RST =	0xda,	/* Data Link Controler off (CONFIG_0) */
	CONFIG0_RST_1 =	0xde,	/* Data Link Controler off (CONFIG_0) */
};
/*
 * config_1
 */
enum {
	BANK_0 = 	0xa0, /* bank 0 (CONFIG_1) */
	BANK_1 = 	0xa4, /* bank 1 (CONFIG_1) */
	BANK_2 = 	0xa8, /* bank 2 (CONFIG_1) */
	CHIP_OFF = 	0x80, /* contrl chip power off (CONFIG_1) */
};

enum {
/* TX_START */
	DO_TX =		0x80,	/* do transmit packet */
	SEND_PKT =	0x81,	/* send a packet */
/* COL_CTRL */
	AUTO_MODE =	0x07,	/* Auto skip packet on 16 col detected */
	MANU_MODE =	0x03,	/* Stop and skip packet on 16 col */
	TDK_AUTO_MODE =	0x47,	/* Auto skip packet on 16 col detected */
	TDK_MANU_MODE =	0x43,	/* Stop and skip packet on 16 col */

/* LAN_CTRL */
	INTR_OFF =	0x0d,	/* LAN controler ignores interrupts */
	INTR_ON =	0x1d,	/* LAN controler will catch interrupts */
};

typedef struct {
	Lock	wlock;				/* window access */

	int	attached;
	int	config0_dfl;
	int	config0_rst;
	int	sram;

	Block*	txbp;				/* */
	int	txbusy;

	long	interrupts;			/* statistics */

} Ctlr;

static void
tdkdbg(Rune r)
{
	if (r == 'T')
	{
		if (++tdk_debug > 2)
			tdk_debug = 0;
		print("tdk debug=%d\n", tdk_debug);
	}
	else
		prtdbg();
}

static Block*
allocrbp(Block* (*f)(int))
{
	Block *bp;

	if (bp = f(ROUNDUP(sizeof(Etherpkt), 4))) {
		bp->rp = bp->base;
		if ((ulong) bp->rp < (ulong)end){
			print("allocrbp alloc error\n");
			return 0;
		}
	}

	return bp;
}

static void
promiscuous(void* arg, int on)
{
	int port;
	Ether *ether;

	ether = (Ether*)arg;
	port = ether->port;

	if (ether->nmaddr || on) {
		outb(port + RX_MODE, RECV_ALL);
	} else {
		outb(port + RX_MODE, ID_MATCHED);
	}
}

static void
multicast(void* arg, uchar *addr, int on)
{
	Ether *ether;

	USED(addr, on);

	ether = (Ether*)arg;

	DPRINT("multicast nmaddr == %d\n", ether->nmaddr);
	promiscuous(ether, ether->prom);
}

static void
attach(Ether* ether)
{
	int port, crap;
	Ctlr *ctlr;
	ulong mask;

	ctlr = ether->ctlr;
	ilock(&ctlr->wlock);
	if(ctlr->attached){
		iunlock(&ctlr->wlock);
		return;
	}

	port = ether->port;

	/*
	 * Set the receiver packet filter for this and broadcast addresses,
	 * set the interrupt masks for all interrupts, enable the receiver
	 * and transmitter.
	 */
	promiscuous(ether, ether->prom);

	/* reset Skip packet reg. */
	outb(port + RX_SKIP, 0x01);

	/* Enable Tx and Rx */
	outb(port + CONFIG_0, ctlr->config0_dfl);

	/* Init receive pointer ? */
	crap = ins(port + DATAPORT); USED(crap);
	crap = ins(port + DATAPORT); USED(crap);

	/* Clear all status */
	outb(port + TX_STATUS, 0xff);
	outb(port + RX_STATUS, 0xff);

	if( cardtype != TDK )
		outb(port + LAN_CTRL, INTR_OFF);

	/* Turn on Rx interrupts */
	outb(port + TX_INTR, D_TX_INTR);
	outb(port + RX_INTR, D_RX_INTR);

	/* Turn on interrupts from LAN card controller */
	if( cardtype != TDK )
		outb(port + LAN_CTRL, INTR_ON);

	mask = (1<<ether->irq);
	*GFER |= mask;
	*GRER &= ~mask;
	*GPDR &= ~mask;

	ctlr->attached = 1;
	iunlock(&ctlr->wlock);
}

static void
statistics(Ether* )
{
#ifdef OLD_CODE
	Ctlr *ctlr;
	int port;

	port = ether->port;
	ctlr = ether->ctlr;

#endif
}

static void
txstart(Ether* ether)
{
	int port, len;
	Ctlr *ctlr;
	Block *bp;
	int rem;
	int pkt;

	port = ether->port;
	ctlr = ether->ctlr;

	/*
	 * Attempt to top-up the transmit FIFO. If there's room simply
	 * stuff in the packet length (unpadded to a word boundary), the
	 * packet data (padded) and remove the packet from the queue.
	 * If there's no room post an interrupt for when there is.
	 * This routine is called both from the top level and from interrupt
	 * level and expects to be called with ctlr->wlock already locked
	 * and the correct register window (Wop) in place.
	 */
	rem = ctlr->sram;
	for(pkt = 0; ; )
	{
		if(bp = ctlr->txbp){
			ctlr->txbp = 0;
		}
		else{
			bp = qget(ether->oq);
			if(bp == nil)
				break;
		}

		len = BLEN(bp);
		rem -= 2;	/* Size of length */

		if ((len <= rem) && (ETHERMINTU <= rem))
		{
			if (len < ETHERMINTU)
				outs(port + DATAPORT, ETHERMINTU);
			else
				outs(port + DATAPORT, len);

			len = ROUNDUP(len, 2);
			outss(port + DATAPORT, bp->rp, len/2);
			freeb(bp);
			while (len < ETHERMINTU)
			{
				outs(port + DATAPORT, 0);
				len += 2;
			}
			pkt++;
			rem -= len;

			// ether->bytes += len;
			DPRINT1("TDK sent %d\n", len);
		}
		else
		{
			ctlr->txbp = bp;
			break;
		}
	}
	if (pkt)
	{
		ether->outpackets += pkt;
		ctlr->txbusy = 1;
		outb(port + TX_START, DO_TX | pkt);
	}
}

static void
transmit(Ether* ether)
{
	Ctlr *ctlr;

	ctlr = ether->ctlr;

	ilock(&ctlr->wlock);
	if (!ctlr->txbusy)
		txstart(ether);
	iunlock(&ctlr->wlock);
}

static void
receive(Ether* ether)
{
	int len, port, rxstatus;
	Block *bp;

	port = ether->port;

	while ((inb(port + RX_MODE) & RM_BUF_EMP) == 0) {
		rxstatus = ins(port + DATAPORT);

		DPRINT1("tdk rxing packet mode %2.2x rxstatus %4.4x.\n",
			inb(port + RX_MODE), rxstatus);
#ifndef final_version
		if (rxstatus == 0) {
			outb(port + RX_SKIP, RP_SKP_PKT);
			continue;
		}
#endif

		if ((rxstatus & 0xF0) != RX_DMA_EOP) {    /* There was an error. */
			if (rxstatus & RX_LEN_ERR) ether->buffs++;
			if (rxstatus & RX_ALG_ERR) ether->frames++;
			if (rxstatus & RX_CRC_ERR) ether->crcs++;
			if (rxstatus & RX_OVR_FLO) ether->overflows++;
			continue;
		}

		len = ins(port + DATAPORT);

		if (len > sizeof(Etherpkt)) {
			print("LAC-CD021L claimed a very large packet, size %d.\n",
				len);
			outb(port + RX_SKIP, RP_SKP_PKT);
			ether->buffs++;
			continue;
		}
 
		if ((bp = allocrbp(iallocb)) == 0)
		{
			DPRINT("Memory squeeze, dropping packet (len %d).\n", len);
			outb(port + RX_SKIP, RP_SKP_PKT);
			ether->soverflows++;
			continue;
		}
		inss(port + DATAPORT, bp->wp, HOWMANY(len, 2));
		bp->wp += len;
		etheriq(ether, bp, 1);
	}
}


static void
interrupt(Ureg*, void* arg)
{
	Ether *ether;
	int port;
	int rx_stat, tx_stat;
	Ctlr *ctlr;
	int work;

	ether = arg;
	ctlr = ether->ctlr;

	if(islo() == 0) {
		/* don't deadlock within interrupt */
		/* XXX can't we just call ilock() ? */
		if(!canlock(&ctlr->wlock))
			return;
	} else
		lock(&ctlr->wlock);

	*GEDR = (1<<ether->irq);

	port = ether->port;
	/* avoid multiple interrupts */
	outs(port + TX_INTR, 0x0000);
 
	/* wait for a while */
	microdelay(1);
 
	ctlr->interrupts++;

	for (work = 1; work; )
	{
		work = 0;

		/* get status */
		tx_stat = inb(port + TX_STATUS);
		rx_stat = inb(port + RX_STATUS);
	 
		/* clear status */
		outb(port + TX_STATUS, tx_stat);
		outb(port + RX_STATUS, rx_stat);
	   
		if (tdk_debug > 1) {
			print("TDK int, rx %2.2x.\n", rx_stat);
			print("         tx %2.2x.\n", tx_stat);
		}
	 
		if (rx_stat || (inb(port + RX_MODE) & RM_BUF_EMP) == 0) {
			receive(ether);
			work = 1;
		}

		if (tx_stat & TX_TMT_OK) {
			ctlr->txbusy = 0;
			txstart(ether);
			work = 1;
		}
	}

	if (tdk_debug > 1) {
		SET(tx_stat, rx_stat);
		print("TDK exiting interrupt,");
		print("	tx %2.2x, rx %2.2x.\n", tx_stat, rx_stat);
	}
 
	outb(port + TX_INTR, D_TX_INTR);
	outb(port + RX_INTR, D_RX_INTR);
 
	unlock(&ctlr->wlock);

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
	if (!p)
		error(Enomem);
	len = snprint(p, READSTR, "interrupts: %ld\n", ctlr->interrupts);
	USED(len);
	n = readstr(offset, a, n, p);
	free(p);

	return n;
}

typedef struct Adapter {
	int	port;
	int	irq;
	int	tbdf;
} Adapter;
static Block* adapter;

static void
tcmadapter(int port, int irq, int tbdf)
{
	Block *bp;
	Adapter *ap;

	bp = allocb(sizeof(Adapter));
	ap = (Adapter*)bp->rp;
	ap->port = port;
	ap->irq = irq;
	ap->tbdf = tbdf;

	bp->next = adapter;
	adapter = bp;
}

static Ether *pcmEther[2];

static void
prtdbg(void)
{
	int ii;

	for (ii = 0; ii < nelem(pcmEther); ii++)
	{
		Ether *ether = pcmEther[ii];
		Ctlr *ctlr;
		int jj,kk;
		uchar regs[4][8];

		if (!ether)
			continue;

		ctlr = ether->ctlr;
		if (!canlock(&ctlr->wlock))
			continue;

		for (jj = 0; jj < MAR_ADR; jj++)
			regs[0][jj] = inb(ether->port + jj);

		outb(ether->port + CONFIG_0, ctlr->config0_rst);
		outb(ether->port + CONFIG_1, BANK_0);
		for (jj = MAR_ADR; jj < LAN_CTRL; jj++)
			regs[1][jj] = inb(ether->port + jj);

		outb(ether->port + CONFIG_1, BANK_1);
		for (jj = MAR_ADR; jj < LAN_CTRL; jj++)
			regs[2][jj] = inb(ether->port + jj);

		outb(ether->port + CONFIG_0, ctlr->config0_dfl);
		outb(ether->port + CONFIG_1, BANK_2);
		for (jj = MAR_ADR + 2; jj < LAN_CTRL; jj++)
			regs[3][jj] = inb(ether->port + jj);
		regs[3][0] = regs[3][1] = 0;
		unlock(&ctlr->wlock);

		for (kk = 0; kk < 4; kk++)
		{
			if (kk == 0)
				print("\nSlot %d: ", ii);
			else
				print("\nBank %d: ", kk-1);
			for (jj = 0; jj < MAR_ADR; jj++)
				print(" %2.2x", regs[kk][jj]);
		}
		print("\n");
	}
}

static void
tdkcis(int slot, uchar *bp)
{
	Ether *ether = pcmEther[slot];
	int nb;

	DPRINT1("ET: %2.2x d[0-2]: %2.2x %2.2x %2.2x\n", bp[0], bp[2], bp[3], bp[4]);
	switch(bp[0])
	{
	case 0x22:		/* LAN info, Node address */
		if (bp[2] != 4)
			break;
		nb = bp[3];
		if (nb > nelem(ether->ea))
			nb = nelem(ether->ea);
		memmove(ether->ea, &bp[4], nb);
		DPRINT("ID from card: %2.2x %2.2x %2.2x %2.2x %2.2x %2.2x\n",
			ether->ea[0], ether->ea[1], ether->ea[2],
			ether->ea[3], ether->ea[4], ether->ea[5]);
		break;
	}
}

int
ethertdkreset(Ether* ether)
{
	int i, port;
	Block *bp, **bpp;
	Adapter *ap;
	uchar ea[Eaddrlen];
	Ctlr *ctlr;

	/*
	 * Any adapter matches if no ether->port is supplied, otherwise the
	 * ports must match. First see if an adapter that fits the bill has
	 * already been found. If not, scan for adapter on PCI, EISA and finally
	 * using the little ISA configuration dance. The EISA and ISA scan
	 * routines leave Wsetup mapped.
	 * If an adapter is found save the IRQ and transceiver type.
	 */
	port = 0;
	bpp = &adapter;
	for(bp = *bpp; bp; bp = bp->next){
		ap = (Adapter*)bp->rp;
		if(ether->port == 0 || ether->port == ap->port){
			port = ap->port;
			ether->irq = ap->irq;
			ether->tbdf = ap->tbdf;
			*bpp = bp->next;
			freeb(bp);
			break;
		}
		bpp = &bp->next;
	}
	if(port == 0)
		return -1;

	/*
	 * Allocate a controller structure, clear out the
	 * adapter statistics, clear the statistics logged into ctlr
	 * and enable statistics collection. Xcvr is needed in order
	 * to collect the BadSSD statistics.
	 */
	ether->ctlr = malloc(sizeof(Ctlr));

	ctlr = ether->ctlr;

	if( sram_config == 0 )
	{
		ctlr->sram = 4096;
		ctlr->config0_dfl =  CONFIG0_DFL;	/* 4K sram */
		ctlr->config0_rst =  CONFIG0_RST;
	}
	else
	{
		ctlr->sram = 8192;
		ctlr->config0_dfl =  CONFIG0_DFL_1;	/* 8K sram */
		ctlr->config0_rst =  CONFIG0_RST_1;
	}

	ilock(&ctlr->wlock);

	outb(port + CONFIG_0, ctlr->config0_rst);
	/*
	 * Check if the adapter's station address is to be overridden.
	 * If not, read it from the card and set in ether->ea prior to loading the
	 * station address.
	 */
	memset(ea, 0, Eaddrlen);

	if(memcmp(ea, ether->ea, Eaddrlen) == 0){
		DPRINT("Read ethernet card cis\n");
		cisread(ether->pcmslot, tdkcis);
	}

	/* Set hardware address */
	for (i = 0; i < 6; i++)
		outb(port + NODE_ID + i, ether->ea[i]);

	if (tdk_debug > 1) {
		print("node id: ");
		for (i = 0; i < 6; i++)
			print("%2.2X ", inb(port + NODE_ID + i));
		print("\n");
	}

	/* Switch to bank 1 */
	outb(port + CONFIG_1, BANK_1);

	/* set the multicast table to accept none. */
	for (i = 0; i < 6; i++)
		outb(port + MAR_ADR + i, 0x00);

	/* Switch to bank 2 (runtime mode) */
	outb(port + CONFIG_1, BANK_2);

	/* set 16col ctrl bits */
	if (cardtype == TDK)
{		outb(port + COL_CTRL, TDK_AUTO_MODE); }
	else
{		outb(port + COL_CTRL, AUTO_MODE); }

	/* clear Reserved Regs */
	outb(port + BMPR12, 0x00);
	outb(port + BMPR13, 0x00);
 
	statistics(ether);

	iunlock(&ctlr->wlock);
	/*
	 * Linkage to the generic ethernet driver.
	 */
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

static int
tdkreset(Ether *ether)
{
	int slot;
	int port;

	if(ether->irq == 0)
		ether->irq = 0x3;
	if(ether->port == 0)
		ether->port = 0x240;

	slot = pcmspecial("LAC-CD02x", ether);
	DPRINT("Ethernet found in slot #%d\n",slot);
	if(slot < 0)
		return -1;

	ether->pcmslot = slot;
	pcmEther[slot] = ether;

	port = ether->port;
	tcmadapter(port, ether->sairq, BusGPIO);

	if(ethertdkreset(ether) < 0){
		print("ethertdk driver did not reset\n");
		pcmspecialclose(slot);
		return -1;
	}

	return 0;
}


void
ethertdklink(void)
{
	debugkey('t', "tdk debug prt", tdkdbg, 0);
	debugkey('T', "tdk debug", tdkdbg, 0);
	addethercard("tdk",  tdkreset);
}
