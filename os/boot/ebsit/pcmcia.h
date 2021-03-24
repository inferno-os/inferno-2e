/*
 *        Hardware-defined structures and constants
 * Definitions are taken from
 *	Intel: 82365SL PC Card Interface Controller (PCIC)
 *	Order Number 290423-001
 * and from
 *      Cirrus: CL-PD6710/PD672X PCMCIA Host Adapters
 */

/* Author: Robert V. Baron (rvb@cmu.edu) */

/* Last modified on Tue Jan 21 09:36:07 PST 1997 by ehs     */

/* Taken from:
 * static char *rcsid = "Header: 
 * /udir/jjk/pkg/cra/elx/config_v1/vb1/RCS/pcmcia.h,v 1.1 1995/05/04 16:42:32 
 * jjk Exp $";
 */


#define PCM_PAGE  0x1000
#define NSOCKETS  4        /* Maximum number of pcmcia sockets      */

/*
 *                 Adapter registers
 * This is an incomplete list and only a few fields/masks are given
 * symbolic names.  See Intel or Cirrus data sheets for more info.
 */

#define PCMSIZE 	0x40   /* Number of registers per pcmcia socket */

#define PCRev		0x00
#define PCSR		0x01
#define	      PCSR_Ready         0x20
#define PCPwrCr		0x02
#define	PCInt		0x03
#define       PCInt_EnableMgrInt 0x10
#define       PCInt_IOMode       0x20
#define       PCInt_CardReset    0x40
#define       PCInt_RingEnable   0x80
#define PCChngSR 	0x04
#define PCChngInt 	0x05
#define PCMapEnable     0x06
#define PCIOWinCtl      0x07

#define PCIOWinMap0     0x08
#define PCIOWinMap1     0x0c

/* IO Window reg offsets from PCIOWinMap* */
#define       IOStartLow      0
#define       IOStartHigh     1
#define       IOEndLow        2
#define       IOEndHigh       3

#define PCIOWinOff0     0x36
#define PCIOWinOff1     0x38

#define       IOOffsetLow     0
#define       IOOffsetHigh    1

#define PCMemWinMap0    0x10
#define PCMemWinMap1    0x18
#define PCMemWinMap2    0x20
#define PCMemWinMap3    0x28
#define PCMemWinMap4    0x30

/* Memory Window reg offsets from PCMemWinMap* */
#define       MemStartLow     0
#define       MemStartHigh    1
#define       MemEndLow       2
#define       MemEndHigh      3
#define       MemOffsetLow    4
#define       MemOffsetHigh   5
#define           Reg            0x40

#ifdef	CL67XXX
#define	PCCLMisc2  	0x1e	/* CL-PD67XXX misc register 2 */
#define PDInfo          0x1f	/* CL-PD6672X info reg */
#endif

#define PCTiming0       0x3a
#define PCTiming1       0x3d

/* Timing reg offsets from PCTiming* */

#define       TimingSetup     0
#define       TimingCmd       1
#define       TimingRecov     2


/* Controller port memory mapping */

/* This is the controller location in ISA I/O space */
/* Note that these are arranged (and the chip responds) for 16 bit writes */
/* the controller ensures that the index gets written before the data     */
/* (this is an optimization not currently used)                           */
#define PCIC_INDEX 	0x3e0
#define PCIC_DATA  	0x3e1

/* These define where the ISA I/O controller access registers are  */
/* given PCMCIA socket. Note the CL67XXX allows only only 4 (0..3) */
#define INDEX(socket) (PCIC_INDEX)
#define DATA(socket)  (PCIC_DATA)

/* This gives the offset to add to get the register index for a given socket */
#define BASE_OFFSET(socket)  (((socket) & 0x3) * PCMSIZE)


/* Card Information Structure (CIS) tuples                       */
/* Partial listing, including all in ExCA minimum recommendation */

/* Tuple space is aligned to even isa byte addresses */
/* The char2 reflects this, with a correction for Lectrice */
typedef ushort  char2;
#define BLO(x)	((x)&0xff)
#define BHI(x)	((x)>>8)

/* Tuple codes */
#define CISTPL_NULL          0x00
#define	CISTPL_DEVICE        0x01
#define CISTPL_NO_LINK       0x14
#define	CISTPL_VERS_1        0x15
#define	CISTPL_DEVICE_A      0x17
#define	CISTPL_JEDEC_C	     0x18
#define CISTPL_JEDEC_A       0x19
#define CISTPL_CONFIG        0x1a
#define CISTPL_CFTABLE_ENTRY 0x1b
#define	CISTPL_DEVICEGEO     0x1e
#define	CISTPL_DEVICEGEO_A   0x1f
#define CISTPL_MANFID        0x20
#define CISTPL_FUNCID        0x21
#define CISTPL_FUNCE         0x22
#define	CISTPL_END	     0xff

/* Function Configuration Register Offsets */

#define COR_OFFSET         0x0000
#define CSR_OFFSET         0x0002
#define PRR_OFFSET         0x0004
#define SCR_OFFSET         0x0006
#define ESR_OFFSET         0x0008

/* Bit Masks for COR */

#define COR_SRESET           0x80
#define COR_LevlREQ          0x40
#define COR_INDEX            0x3f

