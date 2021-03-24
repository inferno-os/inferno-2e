/* ******************************************************************* *\
		Our Copyright and Notes  		
\* ******************************************************************* */
/*

          Coda: an Experimental Distributed File System

       Copyright (c) 1987-1995 Carnegie Mellon University
                      All Rights Reserved.

Permission  to use, copy, modify and distribute this software and
its documentation is hereby granted (including for commercial  or
for-profit use), provided that both the copyright notice and this
permission  notice  appear  in  all  copies  of   the   software,
derivative  works or modified versions, and any portions thereof,
and that both notices appear  in  supporting  documentation,  and
that  credit  is  given  to  Carnegie  Mellon  University  in all
publications reporting on direct or indirect use of this code  or
its derivatives.

CODA  IS  AN  EXPERIMENTAL  SOFTWARE PACKAGE AND IS KNOWN TO HAVE
BUGS, SOME OF WHICH MAY  HAVE  SERIOUS  CONSEQUENCES.    CARNEGIE
MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.
CARNEGIE MELLON DISCLAIMS ANY  LIABILITY  OF  ANY  KIND  FOR  ANY
DAMAGES  WHATSOEVER RESULTING DIRECTLY OR INDIRECTLY FROM THE USE
OF THIS SOFTWARE OR OF ANY DERIVATIVE WORK.

Carnegie Mellon encourages (but does not require) users  of  this
software to return any improvements or extensions that they make,
and to grant Carnegie Mellon the  rights  to  redistribute  these
changes  without  encumbrance.   

*/

/*
 * These DATA are taken from:
 *	Intel: 82365SL PC Card Interface Controller (PCIC)
 *	Order Number 290423-001
 * and from
 *      Cirrus: CL-PD6710/PD672X PCMCIA Host Adapters, Version 2.5
 */

/* Author:
 *	Robert V. Baron (rvb@cmu.edu)
 */

/*
 *	NOTES:	make intr on removal turn all off !!!
 *		add dissociate win from pcm_info[].port/mem.
 *		should you be able to open w/o card inserted?
 *		should card removal close?
 */

/* Taken from VB ELX code. Hacked for testing IT */
/* Last modified on Tue Jan 28 09:25:35 PST 1997 by ehs     */
/*      modified on Fri Aug 30 11:59:04 PDT 1996 by chaiken */
/*      modified on Mon Jun  3 16:19:14 PDT 1996 by hayter */


/* ******************************************************************* *\
	 	Basic functions for device switch
\* ******************************************************************* */
#include <lib9.h>
#include "mem.h"
#include "dat.h"
#include "fns.h"

#include "io.h"
#define CL67XXX
#include "pcmcia.h"
#include "pcmlib.h"

#include "etherif.h"
#include "../net/netboot.h"


#define TRC(x) if (0) (x)

#define GET_REG(socket, offset) \
(outb(INDEX(socket), BASE_OFFSET(socket) + (offset)), \
(uchar)inb(DATA(socket)))

#define PUT_REG(socket, offset, val) \
outb(INDEX(socket), BASE_OFFSET(socket) + (offset)); \
outb(DATA(socket), (val))

extern int netdebug;


#define FLD 20

struct pcm_generic {
    char vers1_0[FLD];
    int unit;
};

struct pcm_win {
    uchar	socket;
    uchar	win;
    uchar	type;
    struct range *range;
    void 	(*set)(struct pcm_win *);
    void	(*dealloc)(struct pcm_win *);
    int		start;
    int		stop;
    int		offset;
    uchar	flag_sz;
    uchar	flag_wt;
    uchar	flag_pr;
    uchar 	flags;
};


struct port {
    uchar active;
    struct pcm_win win;
};

struct mem {
    uchar active;
    struct pcm_win win;
};

struct socket_info {
    char exists;	/* The socket exists       */
#define	  VERS1   0x02
#define	  ATTACH  0x04
    char present;	/* A card is in the socket */
    char changed;	/* Card status changed   */
    int	 (*fn)();	/* call back function on removal */
    int  arg;
    short manfid[2];    /* Manufacturer id on card */
    char vers1_1[FLD];
    char vers1_2[FLD];
    char vers1_3[FLD];
    char vers1_4[FLD];
    int  config_offset; /* offset of configuration registers */
    int  config_mask;   /* mask of implemented registers */

    struct pcm_generic gen;
    struct mem   mem[5];		/* mem  windows alloc'ed */
    struct port  port[2];		/* port windows alloc'ed */
};


struct range {
    int start;
    int size;
    struct range *next;
};


struct socket_info pcm_info[NSOCKETS];

static struct pcm_win *pcm_alloc_mwin(int socket);
static struct pcm_win *pcm_alloc_pwin(int socket);

static void pcm_reset(int socket);
static int pcm_loadvers1(int socket);

static void pcm_set_pwin(int socket, int win, int start, int stop, int offset,
		  int flags);
static void pcm_set_mwin(int socket, int win,
		  int start, int stop, int offset,
		  int flag_sz, int flag_wt, int flag_pr);

static int  win_map(struct pcm_win *wp);
static void win_unmap(struct pcm_win *wp);

static int pcmcia_delay = 2000;	/* microseconds */


int pcm_set_iomode(int socket)
{
    uchar data = GET_REG(socket, PCInt);

    PUT_REG(socket, PCInt, data | PCInt_IOMode);
    return GET_REG(socket, PCInt);
}

/* Probe routine: Do we have a PCMCIA controller */
int pcmprobe(void)
{
#ifdef CL67XXX
    uchar r1, r2;
    
    /* the top two bits of ChipInfo should toggle on successive reads */
    PUT_REG(0, PDInfo, 0);
    r1 = GET_REG(0, PDInfo);
    r2 = GET_REG(0, PDInfo);
	
    if (((r1 & 0xc0) == 0xc0) && ((r2 & 0xc0) == 0)) {
	if(netdebug & NETDBG_PCMCIA_INIT)
		print("PCMPROBE: found CL-PD67%c0 rev level %d\n",
	       		(r1 & 0x20) ? '2':'1', (r1 & 0x1e)>>1);
	return 1;
    }
	
    if(netdebug & NETDBG_PCMCIA_INIT)
    	print("PCMPROBE: Did not find PCMCIA chip. Info gave 0x%ux then 0x%ux\n",
	   r1, r2);
    return 0;
#else
    return 1;    /* hope for the best? */
#endif
}

/* This is the initialise routine -- basically expect it will be called */
/* with some sort of device structure to fill in                        */

/* Don't call unless probe returned that a controller exists!           */
int pcmattach(void)
{
    struct socket_info *sip;
    int     socket,
            found = 0;
    uchar  card;

#ifdef CL67XXX
#ifdef	FAST_CLOCK
    /* Disable 7/4 clock munging if using 25MHz clock (Lectrice, not IT) */
    PUT_REG(0, PCCLMisc2, 0xb);
#endif
    /* try turning off "dynamic" mode */
    PUT_REG(0, PCCLMisc2, 0);
#endif

    for (socket = 0, sip = pcm_info; socket < NSOCKETS; socket++, sip++) {
    	card = GET_REG(socket, PCRev);
	/* Top two bits = 10 --> memory & I/O device. That is all we like */
	/* Bottom four bits are revision, check for 82365SL compatible    */
	if (card == 0x82 || card == 0x83) {
	    found++;
	    sip->exists = 1;
	    card = GET_REG(socket, PCSR);
	    /* Check for card fully inserted */
	    sip->present = (card & 0x0c) == 0x0c;
	    sip->changed = 0;
	
	} else {
	    sip->exists = 0;
	}
    }
    if(netdebug & NETDBG_PCMCIA_INIT)
    	print("pcm:  port = %ux, (%d sockets)\n", INDEX(0), found);

    for (socket = 0, sip = pcm_info; socket < NSOCKETS; socket++, sip++) {
	if (sip->present) {
    	    if(netdebug & NETDBG_PCMCIA_INIT)
	    	print("socket%d:  occupied\n", socket);
	    pcm_reset(socket);
	    pcm_loadvers1(socket);
	}
    }
    return 1;
}


/* ******************************************************************* *\
    		Card Status Maintainance
\* ******************************************************************* */

static void pcm_off(int socket)
{
    struct socket_info *sip = &pcm_info[socket];

    PUT_REG(socket, PCPwrCr, 0);	/* turn off power */
    PUT_REG(socket, PCInt, 0);		/* reset back on */
    sip->exists &= ~ATTACH;
}

static void pcm_reset(int socket)
{
    uchar en;
    int    i;

    /*maybe we should kill the power */
    TRC(print("pcm_reset %d: Power off\n", socket));
    pcm_off(socket);

    /* ... and the mappings */
    en = GET_REG(socket, PCMapEnable);
    if (en != 0 && (netdebug & NETDBG_PCMCIA_INIT))
	print(", had mapped %ux\n", en);
    PUT_REG(socket, PCMapEnable, 0);
    for (i=0; i<2; ++i)
	pcm_set_pwin(socket, i, 0, 1, 0, 0);
    for (i= 0; i<5; ++i)
	pcm_set_mwin(socket, i, 0, 1, 0, 0, 0, 0);
    TRC(print("pcm_reset %d: Done\n", socket));
}

void pcm_ready(int socket)
{
    int     i;
    uchar  sr;
    /* Turn on Vcc power iff card detected (bit 5 = auto_power) */

    /* Turn on interface signals */
    TRC(print("pcm_ready %d: Interface on\n", socket));
    PUT_REG(socket, PCPwrCr, 0xf0);

    delay(25);   /* delay 25 ms (PCMCIA requires > 20 ms) */

    i = 0; sr = 0;
    while ((i++ < 10000) && ((sr & 0x60) != 0x60)) {
	sr = GET_REG(socket, PCSR);
    }
    if(netdebug & NETDBG_PCMCIA_INIT)
    	print("power came on after %d iterations %ux\n", i-1, sr);

    /* clear the card reset */
    PUT_REG(socket, PCInt, 0x40);    	/* !RESET */

    /* setup timing (CL67xxx defaults) */
    PUT_REG(socket, PCTiming0+TimingSetup, 0x1);
    microdelay(pcmcia_delay);
    PUT_REG(socket, PCTiming0+TimingCmd, 0x6);
    microdelay(pcmcia_delay);
    PUT_REG(socket, PCTiming0+TimingRecov, 0x0);
    microdelay(pcmcia_delay);
    PUT_REG(socket, PCTiming1+TimingSetup, 1);
    microdelay(pcmcia_delay);
    PUT_REG(socket, PCTiming1+TimingCmd, 0xf);
    microdelay(pcmcia_delay);
    PUT_REG(socket, PCTiming1+TimingRecov, 0);
    microdelay(pcmcia_delay);

    delay(100);		/* to make sure the card is really ready */
}

static char *devTypes[16] = {  /* CIS DEVTYPE_* */
    "NULL", "ROM", "OTPROM", "EPROM", "EEPROM", "FLASH",
    "SRAM", "DRAM", "(res 8)","(res 9)","(res A)","(res B)","(res C)", 
    "FUNCSPEC", "EXTEND", "(res F)" };

static char *speedTbl[8] = {
    "NULL", "250 ns", "200 ns", "150 ns", "100 ns",
    "(res 5)", "(res 6)", "EXTEND"};

static const int sizeTbl[8] = {
    512, 2048, 8192, 32768, 128*1024, 512*1024, 2048*1024, -1 };

static char2 *
c2copy(char *cp, char2 *c2p, int len)
{
    int i;
    int c;

    for (i = len-1; i > 0 && ((c = BLO(c2p[0])) != 0); i--, c2p++) {
	if (c == 0xff) break;
	*cp++ = c;
    }
    *cp = 0;

    for (; (c = BLO(c2p[0])) != 0; c2p++) {
	if (c == 0xff) break;
    }
    return ++c2p;
}

#ifdef DEBUG
static void pcm_dump(char2 *dstart, char2 *dend)
{
    char2  *dp;

    dp = dstart;
    while (dp < dend) {
        if ((dp - dstart)%16 == 0)
	    TRC(print("\n  "));
	TRC(print(" %02ux", BLO(dp[0])));
	dp++;
    }
}
#else
#define pcm_dump(dstart,dend)     /* no code */
#endif

static int pcm_loadvers1(int socket)
{
    struct socket_info *sip = &pcm_info[socket];
    struct pcm_win *wp;
    char2 *c2p, *c2pbeg, *c2pend;
    int i;
    int hack_dev_type = 0, hack_dev_size = 0;
    int hack_jedec_manuf = 0, hack_jedec_part = 0;
    int found = 0;
    int linked = 1;

    /* set null values */
    sip->config_offset = sip->config_mask = 0;
    sip->manfid[0] = sip->manfid[1] = 0x0000;

    /* enable socket */
    pcm_ready(socket);

    /* Map the attribute memory */
    TRC(print("loadvers1: Map attr\n"));
    wp =  pcm_attr_map(socket, 0, 1024);
    if (!wp)
	goto poweroff;

    c2pbeg = (char2 *)(MIObase + wp->start);

    TRC(print("pcm%d: Attr base is 0x%ux\n", socket, c2pbeg));

    /* Humm, this looks like experience -- maybe the card has to warm up */
    for (i = 10; (BLO(c2pbeg[0]) != CISTPL_DEVICE) && --i > 0; ) {
	microdelay(pcmcia_delay);
    }
    if (i == 0) {
	TRC(print("Oops\n"));
	print(" bad CIS %ux,%ux,%ux,%ux @0x00\n",
		BLO(c2pbeg[0]), BLO(c2pbeg[1]),
		BLO(c2pbeg[2]), BLO(c2pbeg[3]));
	goto mapoff;
    }

    /* Run through the tuples digging out stuff */
    for (c2p = c2pbeg, c2pend = c2pbeg + 512/sizeof(char2); c2p < c2pend;) {
	char2 *dp = &c2p[2]; 	// had &
	char2 *dend = dp + BLO(c2p[1]);
        uchar code = BLO(c2p[0]);

	switch(code) {

	case CISTPL_NULL:
	    c2p++;
	    continue; /* NOT break, don't do normal link step */
		
	case CISTPL_DEVICE:
	case CISTPL_DEVICE_A:
	    if (code == CISTPL_DEVICE) {
		TRC(print("pcm%d: DEVICE:", socket));
		hack_dev_type = BLO(dp[0]) & 0xF0;
		hack_dev_size = BLO(dp[1]);
	    } else
		TRC(print("pcm%d: DEVICE_A:", socket));

	    while ((dp < dend) && (BLO(dp[0]) != 0xff)) {
		TRC(print(" %s", devTypes[(BLO(dp[0]) >> 4) & 0xf]));
		if (BLO(dp[0]) != 0) {
		    if ((BLO(dp[0]) & 0x8) != 0)
			TRC(print(" (WP)"));
		    TRC(print(" %s", speedTbl[BLO(dp[0]) & 0x7]));
		    if ((BLO(dp[0]) & 0x7) == 7) {
			dp++;
			while (BLO(dp[0]) & 0x80) dp++;
		    }
		    dp++;
		    /* Note size field is one less than actual */
		    TRC(print(" %d bytes.", 
				(((BLO(dp[0])>>3) & 0x1f)+1) * 
				sizeTbl[BLO(dp[0]) & 7]));
		}
		dp++;
	    }
	    TRC(print("\n"));
	    break;
	    
	case CISTPL_NO_LINK:
	    TRC(print("pcm%d: NO_LINK\n", socket));
	    linked = 0;
	    break;

	case CISTPL_VERS_1: {
	    int maj, min;
	    maj = BLO(c2p[2]);
	    min = BLO(c2p[3]);
	    sip->exists |= VERS1;

	    if (BLO(c2p[4]) != 0xFF) {
		char2 *cc2p = &c2p[4];	// had &
		cc2p = c2copy(sip->vers1_1, cc2p, FLD);
		cc2p = c2copy(sip->vers1_2, cc2p, FLD);
		cc2p = c2copy(sip->vers1_3, cc2p, FLD);
		cc2p = c2copy(sip->vers1_4, cc2p, FLD);
		found = 1;
	    } else {
		static const char *nn = "none";
		strncpy(sip->vers1_1, nn, FLD);
		strncpy(sip->vers1_2, nn, FLD);
		strncpy(sip->vers1_3, nn, FLD);
		strncpy(sip->vers1_4, nn, FLD);
	    }
		
	    TRC(print("pcm%d: VERS_1 (%d,%d): %s,%s,%s,%s\n",
			socket, maj, min, sip->vers1_1,
			sip->vers1_2, sip->vers1_3, sip->vers1_4));
		
	    break;
	}
	
	case CISTPL_JEDEC_C:
	case CISTPL_JEDEC_A:
	    if (code == CISTPL_JEDEC_C) {
		TRC(print("pcm%d: JEDEC_C:", socket));
		hack_jedec_manuf = BLO(dp[0]);
		hack_jedec_part  = BLO(dp[1]);
	    } else
		TRC(print("pcm%d: JEDEC_A:", socket));
	    while (dp+1 < dend) {
		TRC(print(" (%d, %d) ", BLO(dp[0]), BLO(dp[1])));
		dp += 2;
	    }
	    TRC(print("\n"));
	    break;

        case CISTPL_CONFIG: {
            uchar  rasz = (BLO(dp[0]) & 0x07) + 1;
            uchar  rmsz = ((BLO(dp[0]) >> 3) & 0x07) + 1;
            
            dp += 2;
            sip->config_mask = 0;
            while (rmsz > 0) {
                rmsz--;
                sip->config_mask <<= 8;
                sip->config_mask |= BLO(dp[rasz+rmsz]);
            }
            sip->config_offset = 0;
            while (rasz > 0) {
                rasz--;
		sip->config_offset <<= 8;
                sip->config_offset |= BLO(dp[rasz]);
            }

	    TRC(print("pcm%d: CONFIG Offset: 0x%ux, Mask: 0x%ux\n",
			socket, sip->config_offset, sip->config_mask));

            break;
        }

	case CISTPL_CFTABLE_ENTRY:
	    TRC(print("pcm%d: CFTABLE_ENTRY %d", socket, BLO(dp[0]) & 0x3f));
	    pcm_dump(dp, dend);
	    TRC(print("\n"));
	    break;

	case CISTPL_DEVICEGEO:
	    TRC(print("pcm%d: DEVICEGEO: \n", socket));
	    while (dp < dend) {
		int bw =   1 << (BLO(dp[0]) - 1);
		int ebs = (1 << (BLO(dp[1]) - 1)) * bw;
		int rbs = (1 << (BLO(dp[2]) - 1)) * bw;
		int wbs = (1 << (BLO(dp[3]) - 1)) * bw;
		int part = 1 << (BLO(dp[4]) - 1);
		int hwil = 1 << (BLO(dp[5]) - 1);
		TRC(print("   Bus %d bytes wide, blocksize: erase %d read %d write %d\n       %d partitions, %d interleaved\n",
			   bw, ebs, rbs, wbs, part, hwil));
		dp += 6;
	    }
	    break;
		
	case CISTPL_MANFID:
	    sip->manfid[0] = (short)(BLO(dp[0]) | BLO(dp[1]) << 8);  /* man id */
	    sip->manfid[1] = (short)(BLO(dp[2]) | BLO(dp[3]) << 8);  /* card id */

	    TRC(print("pcm%d: Manufacturer ID: 0x%4.4ux Card ID: 0x%4.4ux\n",
			   socket, sip->manfid[0], sip->manfid[1]));
	    break;

	case CISTPL_FUNCID:
	    TRC(print("pcm%d: Function ID: 0x%ux\n", socket, BLO(dp[0])));
	    break;

	case CISTPL_FUNCE:
	    TRC(print("pcm%d: Function Extension", socket));
	    pcm_dump(dp, dend);
	    TRC(print("\n"));
            break;

	default:
	    TRC(print("pcm%d: TUPLE(%2ux)...", socket, code));
	    if (code != CISTPL_END && dp < dend)
		pcm_dump(dp, dend);
	    TRC(print("\n"));
	    break;
	}

	if (code == CISTPL_END) break; /* Done */
	/* In the normal case follow the link */
	c2p = dend;
    }
	
    if (!found) {
	TRC(print("Check hacks: dev 0x%ux, jm 0x%ux, siz 0x%ux, jp 0x%ux\n",
		       hack_dev_type, hack_jedec_manuf, hack_dev_size,
		       hack_jedec_part));
	/* Hack to decode AMD flash cards */
	if ((hack_dev_type == 0x50) && (hack_jedec_manuf == 0x01)) {
	    if ((hack_dev_size == 0x9D) && (hack_jedec_part == 0xA4)) {
		strncpy(sip->vers1_1, "AMD&BERG", FLD);
		strncpy(sip->vers1_2, "Flash Memory Card", FLD);
		strncpy(sip->vers1_3, "AmC010CFLKA", FLD);
		sip->exists |= VERS1;
		found = 1;
	    } else if ((hack_dev_size == 0x4E) && (hack_jedec_part == 0x3D)) {
		strncpy(sip->vers1_1, "AMD&BERG", FLD);
		strncpy(sip->vers1_2, "Flash Memory Card", FLD);
		strncpy(sip->vers1_3, "AmC020DFLKA", FLD);
		sip->exists |= VERS1;
		found = 1;
	    }
	}
    }    
    if(netdebug & NETDBG_PCMCIA_INIT)
    	if (!found)
		print("pcm%d: couldn't determine device info from CIS\n",
				socket);
   	else
		print("pcm%d: VERS1 = %s,%s,%s,%s\n", socket,
			sip->vers1_1, sip->vers1_2, sip->vers1_3, sip->vers1_4);
mapoff:
    pcm_attr_unmap(wp);
poweroff:
    pcm_reset(socket);
    return found;
}

void pcm_set_conf_regs(int socket, uchar cor, uchar csr) {
    struct socket_info *sip = &pcm_info[socket];
    struct pcm_win *wp = pcm_attr_map(socket, sip->config_offset, 0x4);
    uchar *cbase = (uchar *)(MIObase + wp->start);

    *(cbase + COR_OFFSET) = cor;
    microdelay(pcmcia_delay);

    *(cbase + CSR_OFFSET) = csr;
    microdelay(pcmcia_delay);
    win_unmap(wp);
}


void pcmcia_ready(int socket, char *str)
{
    uchar sr = GET_REG(socket, PCSR);
    USED(sr);
    if(netdebug & NETDBG_PCMCIA_INIT)
    	print("skt%d: %s: %s\n",
	   socket, str, sr & PCSR_Ready ? "ready" : "not ready");
}

int
pcmspecial(char *idstr, ISAConf *isa)
{
	int i;
	for(i=0; i<NSOCKETS; i++) {
		if(strcmp(idstr, pcm_info[i].vers1_2) == 0) {
			pcm_ready(i);
			return i;
		}
	}
	return -1;
}

void
pcmspecialclose(int slot)
{
}

/* Internal Selectors */
#define IOWindow        0
#define MemWindow       1

/*
 * PCMCIA spec Attr Memory Mapping
 */
struct pcm_win *pcm_attr_map(int socket, int where, int size)
{
    struct pcm_win *wp = pcm_alloc_mwin(socket);

    if (wp) {
	wp->start = 0;
	wp->stop = size;
	wp->offset = where;
	wp->flag_sz = 0;
	wp->flag_wt = 0;
	wp->flag_pr = Reg;
	win_map(wp);
    }
    return wp;
}

void pcm_attr_unmap(struct pcm_win *wp)
{
    win_unmap(wp);
}

/*
 * PCMCIA spec Common Memory Mapping
 */
struct pcm_win *pcm_mem_map(int socket, int where, int size, int readonly)
{
    struct pcm_win *wp = pcm_alloc_mwin(socket);

    if (wp) {
	wp->start = 0;
	wp->stop = size;
	wp->offset = where;
	wp->flag_sz = 0x80;
	wp->flag_wt = 0;
	wp->flag_pr = (readonly ? 0x80 : 0);
	win_map(wp);
    }
    return wp;
}

void pcm_mem_unmap(struct pcm_win *wp)
{
    win_unmap(wp);
}

/*
 * PCMCIA spec io port mapping
 */
struct pcm_win *pcm_port_map(int socket, int start, int end, int offset,
			     int flags)
{
    struct pcm_win *wp = pcm_alloc_pwin(socket);

    if (wp) {
	wp->start = start;
	wp->stop = end;
	wp->offset = offset;
	wp->flags = flags;
	win_map(wp);
    }
    return wp;
}

void pcm_port_unmap(struct pcm_win *wp)
{
    win_unmap(wp);
}

int  pcm_irq_map(int socket, int irq)
{
    uchar data;

    if(netdebug & NETDBG_PCMCIA_STATUS)
    	print("pcm%d: Irq %d\n", socket, irq);
    data = GET_REG(socket, PCInt);
    PUT_REG(socket, PCInt, (data & 0xf0)|(irq & 0x0f));
    return 1;
}

void pcm_irq_unmap(int socket)
{
    uchar data;

    data = GET_REG(socket, PCInt);
    PUT_REG(socket, PCInt, data & 0xf0);
}


/* ******************************************************************* *\
 * 		Resource range stuff (general)
\* ******************************************************************* */


/* must be initialized statically because we cannot 
 * guarantee that some driver might range_alloc in
 * its probe code.
 */

/* This code should really be part of the Operating System! */

struct range MemR = { 0xd0000, 0x10000, (struct range *) 0 };
struct range *MemRange = &MemR;
struct range PortR = {0x300, 0x010, (struct range *) 0 };
struct range *IORange = &PortR;


int range_alloc(struct range *range, int size)
{
    for (;range; range = range->next) {
	if (range->size >= size) {
	    int  start = range->start;

	    range->start += size;
	    range->size -= size;
	    if (range->size == 0) {
		if (range->next) {
		    *range = *range->next;
		}
	    }
	    return start;
	}
    }
    return -1;
}

static void range_free(struct range *range, int start, int size)
{
    struct range *prev = (struct range *) 0;
    struct range *newr;

    for (; (int)range && start > range->start + range->size; prev = range, range = range->next)
		;

    if (!range) {
    } else if (start + size == range->start) {
	range->start = start;
	range->size += size;
	return;
    } else if (range->start+range->size == start) {
	range->size += size;
	if (range->start+range->size == range->next->start) {
	    newr = range->next;	/* actually to be purged */
	    range->size += range->next->size;
	    range->next = range->next->next;
	}
	return;
    }
}


/* ******************************************************************* *\
 * 	Internal Intel 82365 manipulation code
\* ******************************************************************* */

/*
 * For port windows
 */
static struct pcm_win *
pcm_alloc_pwin(int socket)
{
    uchar      en;
    int		win;
    struct port	*pp;

    en = GET_REG(socket, PCMapEnable);
    if ((en & 0x40) == 0)
	win = 0;
    else if ((en & 0x80) == 0)
	win = 1;
    else
	return (struct pcm_win *) 0;
    
    pp = &pcm_info[socket].port[win];
    pp->win.type = IOWindow;
    pp->win.socket = socket;
    pp->win.win = win;
    pp->active = 1;
    return &pp->win;
}	

/* Set I/O Window mappings */
static void pcm_set_pwin(int socket, int win, int start, int stop, int offset,
		  int flags)
{
    /* Base register offsets for I/O window mapping */
    int prt_2_pcic_reg[2]  = {PCIOWinMap0, PCIOWinMap1};
    int prt_2_io_offset[2] = {PCIOWinOff0, PCIOWinOff1};

    int	reg_offset = prt_2_pcic_reg[win];
    int io_offset = prt_2_io_offset[win];
    uchar iocr;

    stop -= 1; 	/* start is included stop is not */

    PUT_REG(socket, reg_offset + IOStartLow,  start & 0xff);
    PUT_REG(socket, reg_offset + IOStartHigh, (start >> 8) & 0xff);

    PUT_REG(socket, reg_offset + IOEndLow,  stop & 0xff);
    PUT_REG(socket, reg_offset + IOEndHigh, (stop >> 8) & 0xff);

    /* I/O window offset regs are not contiguous with start and end regs */
    PUT_REG(socket, io_offset + IOOffsetLow,  offset & 0xff);
    PUT_REG(socket, io_offset + IOOffsetHigh, (offset >> 8) & 0xff);

    flags &= 0xf;
    iocr = GET_REG(socket, PCIOWinCtl);
    if (!win) {
	iocr = (iocr & 0xf0) + flags;
    } else {
	iocr = (iocr & 0x0f) + (flags << 4);
    }
    PUT_REG(socket, PCIOWinCtl, iocr);
}

static void pcm_call_set_pwin(struct pcm_win *wp)
{
    uchar  en;
    int     socket = wp->socket;

    if(netdebug & NETDBG_PCMCIA_STATUS)
    	print("pcm%d: I/O Window %d, base: 0x%ux, size: 0x%ux, offset: 0x%ux\n",
           socket, wp->win, wp->start, wp->stop-wp->start, wp->offset);
    pcm_set_pwin(socket, wp->win, wp->start, wp->stop, wp->offset, wp->flags);

    en = GET_REG(socket, PCMapEnable);
    en |= 1 << (wp->win + 6);
    PUT_REG(socket, PCMapEnable, en);
}

static void pcm_dealloc_pwin(struct pcm_win *wp)
{
    uchar  en;
    int     socket = wp->socket;

    en = GET_REG(socket, PCMapEnable);
    en &= ~(1 << (wp->win + 6));
    PUT_REG(socket, PCMapEnable, en);

    pcm_set_pwin(socket, wp->win, 0, 1, 0, 0);
    pcm_info[socket].port[wp->win].active = 0;
}

/*
 * For memory windows
 */
static struct pcm_win *
pcm_alloc_mwin(int socket)
{
    uchar  en;
    int	    win;
    struct mem *mp;

    en = GET_REG(socket, PCMapEnable);
    if ((en & 0x01) == 0)
	win = 0;
    else if ((en & 0x02) == 0)
	win = 1;
    else if ((en & 0x04) == 0)
	win = 2;
    else if ((en & 0x08) == 0)
	win = 3;
    else if ((en & 0x10) == 0)
	win = 4;
    else
	return (struct pcm_win *) 0;
    
    mp = &pcm_info[socket].mem[win];
    mp->win.type = MemWindow;
    mp->win.socket = socket;
    mp->win.win = win;
    mp->active = 1;
    return &mp->win;
}

static void pcm_set_mwin(int socket, int win, int start, int stop, int offset,
		  int flag_sz, int flag_wt, int flag_pr)
{
    /* Register offsets for Memory window mapping */
    int mem_2_pcic_reg[5] = {
	PCMemWinMap0, PCMemWinMap1, PCMemWinMap2, PCMemWinMap3, PCMemWinMap4};

    int	reg_offset = mem_2_pcic_reg[win];

    stop -= 1; 		/* start is included stop is not */
    offset -= start;	/* pretty stupid but start is added to offset */
                        /* I want offset to be relative to the PC card */
    start >>= 12;
    PUT_REG(socket, reg_offset + MemStartLow,  start & 0xff);
    PUT_REG(socket, reg_offset + MemStartHigh,
	    (((start >> 8) & 0x0f) | (flag_sz & 0xc0)));

    stop >>= 12;
    PUT_REG(socket, reg_offset + MemEndLow,  stop & 0xff);
    PUT_REG(socket, reg_offset + MemEndHigh,
	    (((stop >> 8) & 0x0f) | (flag_wt & 0xc0)));

    offset >>= 12;
    PUT_REG(socket, reg_offset + MemOffsetLow,  offset & 0xff);
    PUT_REG(socket, reg_offset + MemOffsetHigh,
	    (((offset >> 8) & 0x3f) | (flag_pr & 0xc0)));
}

static void pcm_call_set_mwin(struct pcm_win *wp)
{
    uchar en;
    int    socket = wp->socket;

    if(netdebug & NETDBG_PCMCIA_STATUS)
    	print("pcm%d: Mem Window %d, base: 0x%ux, size: 0x%ux, offset: 0x%ux (%c)\n",
           socket, wp->win, wp->start, wp->stop-wp->start, wp->offset,
           (wp->flag_pr == Reg ? 'a' : 'c'));
    pcm_set_mwin(socket, wp->win, wp->start, wp->stop, wp->offset,
		 wp->flag_sz, wp->flag_wt, wp->flag_pr);
    en = GET_REG(socket, PCMapEnable);
    en |= 1 << wp->win;
    PUT_REG(socket, PCMapEnable, en);
}	

static void pcm_dealloc_mwin(struct pcm_win *wp)
{
    uchar  en;
    int     socket = wp->socket;

    en = GET_REG(socket, PCMapEnable);
    en &= ~(1 << (wp->win));
    PUT_REG(socket, PCMapEnable, en);

    pcm_set_mwin(socket, wp->win, 0, 1, 0, 0, 0, 0);
    pcm_info[socket].mem[wp->win].active = 0;
}

/*
 *	Generic Window Ops
 */

static int win_map_range(struct pcm_win *wp, struct range *range)
{
    wp->range = range;
    switch (wp->type) {
        case MemWindow:
	    wp->set = pcm_call_set_mwin;
	    wp->dealloc = pcm_dealloc_mwin;
	    break;
        case IOWindow:
	    wp->set = pcm_call_set_pwin;
	    wp->dealloc = pcm_dealloc_pwin;
	    break;
        default:
    	    if(netdebug & NETDBG_PCMCIA_STATUS)
	    	print("win_map_range: Invalid window type 0x%ux\n", wp->type);
    }

    if (wp->start == 0) {
	if (wp->type == MemWindow) { 
                                    /* must factor in offset on page */
	    wp->stop = ((wp->stop + (wp->offset & (PCM_PAGE - 1))) +
			(PCM_PAGE - 1)) & ~(PCM_PAGE - 1);
	    wp->start = range_alloc(wp->range, wp->stop);
	
	    if (wp->start == 0)
		return -1;
	    wp->stop += wp->start;
	}
    } else
	wp->range = (struct range *)0;
    (wp->set)(wp);
    return 0;
}

static int win_map(struct pcm_win *wp)
{
    switch (wp->type) {
        case MemWindow:
	    return win_map_range(wp, MemRange);
	    break;
        case IOWindow:
	    return win_map_range(wp, (struct range *)0);
	    break;
        default:
    	    if(netdebug & NETDBG_PCMCIA_STATUS)
	    	print("win_map: Invalid window type 0x%ux\n", wp->type);
	    return -1;
    }
}


static void win_unmap(struct pcm_win *wp)
{
    (wp->dealloc)(wp);
    if (wp->range)
	range_free(wp->range, wp->start, wp->stop - wp->start);
    memset(wp, 0, sizeof (struct pcm_win));
}
