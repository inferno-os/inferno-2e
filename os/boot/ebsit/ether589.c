/**************************************************************************
3C589B -  3Com 3c5x9 support for BOOTP/TFTP Bootstrap Program

Author: Martin Renters.
  Date: Mar 22 1995

  Last modified on Tue Jan 28 09:27:55 PST 1997 by ehs

 This code is based heavily on David Greenman's if_ed.c driver and
  Andres Vega Garcia's if_ep.c driver.

 Copyright (C) 1993-1994, David Greenman, Martin Renters.
 Copyright (C) 1993-1995, Andres Vega Garcia.
 Copyright (C) 1995, Serge Babkin.
  This software may be used, modified, copied, distributed, and sold, in
  both source and binary form provided that the above copyright and these
  terms are retained. Under no circumstances are the authors responsible for
  the proper functioning of this software, nor do the authors assume any
  responsibility for damages incurred with its use.

3c509 support added by Serge Babkin (babkin@hq.icb.chel.su)

***************************************************************************/

#include <lib9.h>
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "pcmlib.h"
#include "io.h"
#include "etherif.h"
#include "../net/netboot.h"

/*
 * file  3c589b.h
 */

/* Last modified on Tue Jan 21 09:37:56 PST 1997 by ehs     */

/*
 * Registers and commands of 3Com EtherLink III controller.
 * Ref: EtherLink III Parallel Tasking ISA, EISA, Micro Channel
 *         and PCMCIA Adapter Drivers Technical Reference (1994)
 * Where relevant, assumes 3C589B or equivalent.
 */

/*
 * Identifiers for the current revision of the 3Com 3C589B
 */
#define MFG_ID    0x6d50
#define PROD_ID   0x9150

/*
 * Transmit packet structure
 *   Preamble 0: Flag bits (15:10), length (9:0)
 *   Preamble 1: zeros
 *   Packet data
 *   Pad to dword
 */

#define INT_ON_TX_DONE   0x8000
#define DISABLE_CRC_GEN  0x2000

/*
 * Register port definitions (Chapter 5, Window Set)
 * Per window (subset only for now)
 *  Size in bits, R = Read, W = Write
 */

/* global (all windows) */

#define R_COMMAND   0x0e       /* 16, W */
#define R_STATUS    0x0e       /* 16, R (8 ok) */

#define S_WINDOW    0xe000     /* mask for window # */
#define S_BUSY      0x1000     /* mask for busy bit (IP) */
#define S_STATUS    0x00ff     /* mask for status bits (see below) */

/* window 0 (setup) */

#define R_EEP_DATA  0x0c       /* 16, RW */
#define R_EEP_CMD   0x0a       /* 16, RW (see EEP_* below) */
#define R_RSRC_CFG  0x08       /* 16, RW */
#define R_ADDR_CFG  0x06       /* 16, RW */
#define R_CFG_CTRL  0x04       /* 16, RW (see CFG_* below) */
#define R_MFG_ID    0x00       /* 16, R */

/* window 1 (operating set) */

#define R_FREE_TX   0x0c       /* 16, R */
#define R_TX_STATUS 0x0b       /*  8, RW (see TX_S_* below) */
#define R_RX_STATUS 0x08       /* 16, R  (see RX_S_* below) */
#define R_TX_DATA   0x00       /* 16, W (8, 32 ok) */
#define R_RX_DATA   0x00       /* 16, R (8, 32 ok) */

/* window 2 (station address) */

#define R_STN_ADDR  0x00       /* 48, RW (6 bytes 0x00 MSB - 0x05 LSB */

/* window 3 (FIFO management) */

#define R_FREE_RX   0x0a       /* 16, R */

/* window 4 (diagnostic) */

#define R_MEDIA     0x0a       /* 16, RW */
#define R_FIFO_DIAG 0x04       /* 16, RW */

/* window 5 (internal state) */

#define R_RD_0_MASK 0x0c       /* 16, R */
#define R_INTR_MASK 0x0a       /* 16, R */
#define R_RX_FILTER 0x08       /* 16, R */
#define R_RX_EARLY  0x06       /* 16, R */
#define R_TX_AVAIL  0x02       /* 16, R */
#define R_TX_START  0x00       /* 16, R */

/* window 6 (statistics) */

/*
 * Command definitions (Chapter 6, Command Register).
 * Each command consists of (5 bit opcode, 11 bit argument)
 * Masks defined below, thresholds must be multiples of 4.
 */

#define EL3_CMD(op,args)     ((ushort)((op)<<11)|(args))

#define GLOBAL_RESET         0x00   /* reset mask (16 cycle) */
#define WINDOW_SELECT        0x01   /* window */
#define START_COAX           0x02
#define RX_DISABLE           0x03
#define RX_ENABLE            0x04
#define RX_RESET             0x05   /* reset mask (multi cycle) */
#define RX_DISCARD           0x08   /* (multi cycle) */
#define TX_ENABLE            0x09
#define TX_DISABLE           0x0a
#define TX_RESET             0x0b   /* reset mask (multi cycle) */
#define REQ_INTR             0x0c
#define ACK_INTR             0x0d   /* status mask (below) */
#define SET_INTR_MASK        0x0e   /* status mask (below) */
#define SET_RD_0_MASK        0x0f   /* status mask (below) */
#define SET_RX_FILTER        0x10   /* filter mask (below) */
#define SET_RX_EARLY_THRESH  0x11   /* threshold */
#define SET_TX_AVAIL_THRESH  0x12   /* threshold */
#define SET_TX_START_THRESH  0x13   /* threshold */
#define STATS_ENABLE         0x15
#define STATS_DISABLE        0x16
#define STOP_COAX            0x17
#define POWER_UP             0x1b
#define POWER_DOWN           0x1c
#define POWER_AUTO           0x1d

/* Filter bits used in Rx Filter Register, Set Rx Filter command */

#define F_STATION            0x01
#define F_MCAST              0x02
#define F_BCAST              0x04
#define F_PROMISC            0x08

/*
 * Mask bits used in Status Register, Read Zero Mask, Interrupt Mask
 * Also, command Acknowledge Interrupt (see description for actual effect),
 *       Set Interrupt Mask, Set Read Zero Mask (except Interrupt Latch)
 */

#define S_INTR_LATCH    0x01
#define S_CARD_FAILURE  0x02
#define S_TX_COMPLETE   0x04
#define S_TX_AVAIL      0x08
#define S_RX_COMPLETE   0x10
#define S_RX_EARLY      0x20
#define S_INT_REQ       0x40
#define S_UPD_STATS     0x80

#define S_5_INTS                (S_CARD_FAILURE|S_TX_COMPLETE|\
				 S_TX_AVAIL|S_RX_COMPLETE|S_RX_EARLY)

/* Mask bits used in RX Status Register. */

#define RX_S_IC       0x8000     /* incomplete */
#define RX_S_ER       0x4000     /* error */
#define RX_S_ERRCODE  0x3800     /* error type */
#define RX_S_BYTES    0x07ff     /* byte count */

/* Mask bits used in TX Status Register. */

#define TX_S_CM       0x80       /* complete */
#define TX_S_IS       0x40       /* interrupt on success */
#define TX_S_JB       0x20       /* jabber error */
#define TX_S_UN       0x10       /* underrun */
#define TX_S_MC       0x08       /* max collisions */
#define TX_S_OF       0x04       /* overflow */

/* Mask bits used in Media Type Register (partial) */

#define MEDIA_TP      0x00c0     /* link beat + jabber (10BASE-T) enable */

/* Mask bits used in FIFO Diagnostic Register (partial) */

#define FD_RXU        0x2000     /* RX underrun */
#define FD_TXO        0x0400     /* TX overrun */

/* Mask bits used in EEPROM Command Register */

#define EEP_EBY       0x8000     /* busy */
#define EEP_TST       0x4000     /* test mode */
#define EEP_TAG       0x0700     /* tag register */
#define EEP_CMD       0x00ff     /* EEPROM command */
#define EEP_OP        0x00c0     /* EEPROM opcode */
#define EEP_ADDR      0x003f     /* EEPROM addr (or command extension) */
#define EEP_OP_READ   0x0080     /* EEPROM read command */

#define CFG_AUI       0x2000     /* AUI connector */
#define CFG_10B2      0x1000     /* thin-wire transceiver */
#define CFG_10BT      0x0200     /* twisted-pair transceiver */

#define MAX_EEPROMBUSY  1000     /* maximum tries on busy */

/* EEPROM addresses (16 bit locations) */

#define EEPROMSIZE             0x40

#define EEPROM_NODE_ADDR_0	0x0	/* Word */
#define EEPROM_NODE_ADDR_1	0x1	/* Word */
#define EEPROM_NODE_ADDR_2	0x2	/* Word */
#define EEPROM_PROD_ID		0x3	/* 0x9[0-f]50 */
#define EEPROM_MFG_ID		0x7	/* 0x6d50 */
#define EEPROM_ADDR_CFG		0x8	/* Base addr */
#define EEPROM_RESOURCE_CFG	0x9	/* IRQ. Bits 12-15 */


#define EL3_IO_BASE 0x300
#define EL3_IO_SIZE 0x10
#define EL3_IO_OFFSET 0x0

#define EL3_AUI 0x1
#define EL3_TP  0x2

typedef struct {
	ulong  base_addr;           /* I/O base addr */
	ushort if_port;             /* AUI/TP */
	uchar  dev_addr[6];         /* ethernet address */
} EL3;

static EL3 el3_dev;

extern int netdebug;

#define SET_WINDOW(x) outs(ioaddr + R_COMMAND, EL3_CMD(WINDOW_SELECT, (x)))

static ushort
read_eeprom(int ioaddr, int index)
{
	outs(ioaddr + R_EEP_CMD, EEP_OP_READ + index);
	/* Pause for at least 162 us. for the read to take place. */
	delay(30);
	return ins(ioaddr + R_EEP_DATA);
}

static int
el3_config(EL3 *dev)
{
	short manid;
	short confctl;
	ulong ioaddr = dev->base_addr;
	ushort *w_ptr;
	int i;

	SET_WINDOW(0);

	/* 3c589B Specs say you must do this for PCMCIA */
	/* IRQ 3 */
	outs(ioaddr + R_RSRC_CFG, 0x3f00);

	/* Make sure connection type is set in address config */
	outs(ioaddr + R_ADDR_CFG, (short)(dev->if_port << 14));

	manid = ins(ioaddr + R_MFG_ID);
	if (manid != MFG_ID) {
		print("FATAL: id mismatch: expected 0x6d50, got 0x%4.4ux\n", manid);
		while(1);
		return 0;
	}

	/* Configuraton control reg. Contains interface types available */
	confctl = ins(ioaddr + R_CFG_CTRL);

	/* read ethernet address from EEPROM  - N.B. host order */
	w_ptr = (ushort *)dev->dev_addr;
	for (i = 0; i < 3; i++)
		hnputs((uchar*)&w_ptr[i], read_eeprom(ioaddr, i));

	return 1;
}

static void
el3_reset(EL3 *dev)
{
	ulong ioaddr = dev->base_addr;
	char tmpb;
	short tmpw;
	int i;

	SET_WINDOW(0);

#ifdef ndef /* LWS specs say not needed on PCMCIA */
	outs(ioaddr + R_CFG_CTRL, 0x0001);    /* Activate board. */
#endif

	outs(ioaddr + R_ADDR_CFG, dev->if_port << 14);  /* Set the xcvr. */
	outs(ioaddr + R_RSRC_CFG, 0x3f00);         /* Set the IRQ line. */

	/* Reset the receiver */
	outs(ioaddr + R_COMMAND, EL3_CMD(RX_RESET, 0));

	/* Reset the transmitter */
	outs(ioaddr + R_COMMAND, EL3_CMD(TX_RESET, 0));

	/* Switch to the stats window, and clear all stats by reading. */
	outs(ioaddr + R_COMMAND, EL3_CMD(STATS_DISABLE, 0));
	SET_WINDOW(6);
	for (i = 0; i < 9; i++)
		tmpb = inb(ioaddr+i);
	tmpw = ins(ioaddr + 10);
	tmpw = ins(ioaddr + 12);

	/* Set the station address in window 2. N.B. Host order */
	SET_WINDOW(2);
	for (i = 0; i < 3; i++)  /* Do word writes */
		outs(ioaddr + 2*i, *((short *)(&dev->dev_addr[2*i])));

	if (dev->if_port == EL3_AUI)
		/* Start the coax transceiver. We should really wait 50ms...*/
		/* LWS Specs clain on PCMCIA this just turns on the LED */
		outs(ioaddr + R_COMMAND, EL3_CMD(START_COAX, 0));
	else if (dev->if_port == EL3_TP) {
		/* 10baseT interface, enabled link beat and jabber check. */
		SET_WINDOW(4);
		outs(ioaddr + R_MEDIA, ins(ioaddr + R_MEDIA) | MEDIA_TP);
	}

	/* Switch to register set 1 for normal use. */
	SET_WINDOW(1);

	/* Enable station address only for booting */
	outs(ioaddr + R_COMMAND, EL3_CMD(SET_RX_FILTER, F_STATION | F_BCAST));

	/* Allow status bits to be seen. */
	outs(ioaddr + R_COMMAND, EL3_CMD(SET_RD_0_MASK, 0xff));

	/* Ack all pending events, and set active indicator mask. */
	outs(ioaddr + R_COMMAND,  EL3_CMD(ACK_INTR, S_5_INTS|S_INTR_LATCH|S_INT_REQ));

	/* Disable all interrupts, but they still appear in status reg */
	outs(ioaddr + R_COMMAND, EL3_CMD(SET_INTR_MASK, 0));

	outs(ioaddr + R_COMMAND, EL3_CMD(STATS_ENABLE, 0)); /* Turn on statistics. */
	outs(ioaddr + R_COMMAND, EL3_CMD(RX_ENABLE, 0)); /* Enable the receiver. */
	outs(ioaddr + R_COMMAND, EL3_CMD(TX_ENABLE, 0)) ; /* Enable transmitter. */

	i= ins(ioaddr + R_STATUS);
}

static int
el3_transmit(uchar *d, uint len)
{
	int pad;
	int status;
	ulong ioaddr = el3_dev.base_addr;

	pad = (4 - (len&3)) & 3;

	/*
	     * The 3c589 automatically pads short packets to minimum ethernet length,
	     * and we drop packets that are too large.
	     */
	if (len + pad > ETHERMAXTU)
		return 0;

	/* drop acknowledgements  - pop TX status stack */
	while ((status = inb(ioaddr + R_TX_STATUS)) & TX_S_CM) {
		if (status & (TX_S_UN | TX_S_MC | TX_S_OF)) {
			outs(ioaddr + R_COMMAND, EL3_CMD(TX_RESET, 0));
			outs(ioaddr + R_COMMAND, EL3_CMD(TX_ENABLE, 0));
		}
		outb(ioaddr + R_TX_STATUS, 0x0);
	}

	/* Wait for free bytes in output FIFO */
	while (ins(ioaddr + R_FREE_TX) < len + pad + 4) {
		/* no room in FIFO */
	}

	/* Output preamble - signal interrupt when xmit complete */
#ifdef ndef
	outs(ioaddr + R_TX_DATA, INT_ON_TX_DONE | len);
#else
	outs(ioaddr + R_TX_DATA, len);
#endif
	outs(ioaddr + R_TX_DATA, 0x0);	/* Second word meaningless */

	/* write packet */
	outss(ioaddr + R_TX_DATA, d, len/2);
	if (len & 1)
		outb(ioaddr + R_TX_DATA, *((const uchar *)d + len - 1));

	while (pad--)
		outb(ioaddr + R_TX_DATA, 0);	/* Padding */

	return 1;
}

static int
el3_poll(uchar *packet)
{
	ulong ioaddr = el3_dev.base_addr;
	short status, cst;
	register short rx_fifo;
	int plen;

	cst = ins(ioaddr + R_STATUS);
	if ((cst & (S_RX_COMPLETE | S_RX_EARLY)) == 0) {
		/* acknowledge  everything */
		outs(ioaddr + R_COMMAND, EL3_CMD(ACK_INTR, (cst & S_5_INTS)));
		outs(ioaddr + R_COMMAND, EL3_CMD(ACK_INTR, S_INTR_LATCH));
		return 0;
	}

	status = ins(ioaddr + R_RX_STATUS);
	if (status & RX_S_ER) {
		outs(ioaddr + R_COMMAND, EL3_CMD(RX_DISCARD, 0));
		return 0;
	}

	rx_fifo = status & RX_S_BYTES;
	if (rx_fifo == 0)
		return 0;

	/* read packet */
	inss(ioaddr + R_RX_DATA, packet, rx_fifo/2);
	if (rx_fifo & 1) packet[rx_fifo-1] = inb(ioaddr + R_RX_DATA);
	plen = rx_fifo;

	while (1) {
		status = ins(ioaddr + R_RX_STATUS);
		rx_fifo = status & RX_S_BYTES;

		if (rx_fifo > 0) {
			inss(ioaddr + R_RX_DATA, packet+plen, rx_fifo/2);
			if (rx_fifo & 1)
				packet[plen + rx_fifo - 1] = inb(ioaddr + R_RX_DATA);
			plen+=rx_fifo;
		}

		if ((status & RX_S_IC) == 0) {  /* !incomplete */
			break;
		}

		delay(10);
	}

	/* acknowledge reception of packet */
	outs(ioaddr + R_COMMAND, EL3_CMD(RX_DISCARD, 0));
	while (ins(ioaddr + R_STATUS) & S_BUSY)
		continue;

	return plen;
}

static int
reset589(Ether *ether)
{
	int slot;
	int port;
	char *names[] = {
		"3C589", "3C589D", "Megahertz 589E"	};
	int i;

	if(ether->port == 0)
		ether->port = EL3_IO_BASE;

	for(i=0; i<nelem(names); i++) {
		slot = pcmspecial(names[i], ether);
		if(slot >= 0)
			break;
	}
	if(slot < 0)
		return -1;

	port = ether->port;
	ether->pcmslot = slot;

	if(netdebug & NETDBG_ETH_INIT)
		print("Configuring %s on socket %d\n", names[i], slot);

	pcm_set_iomode(slot);
	pcm_set_conf_regs(slot, 0x43, 0x0);
	pcm_port_map(slot, port, port + EL3_IO_SIZE, EL3_IO_OFFSET, 0x2);

	el3_dev.base_addr = port;    /* Set I/O base address */
	el3_dev.if_port = EL3_TP;     /* Set ethernet type - 10bT */

	if (!el3_config(&el3_dev))
		return 0;  /* error */

	el3_reset(&el3_dev);

	ether->transmit = el3_transmit;
	ether->poll = el3_poll;
	memcpy(ether->ea, el3_dev.dev_addr, Eaddrlen);
	return 0;

}

void
ether589link(void)
{
	addethercard("3C589", reset589);
}

