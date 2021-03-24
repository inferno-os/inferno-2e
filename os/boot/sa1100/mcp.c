//
// Multimedia Communications Port for SA1100
//
// interface to the external UCB1100, UCB1200, and CRYSTAL4271 codecs
// both touchscreen and telecom
//

#include <lib9.h>
#include "dat.h"
#include "fns.h"
#include "mem.h"
#include "mcp.h"

enum {
	MCPclk=		11980800,	// MCP clock freq (3.6864Mhz * 13)/4

	 // MCP register offsets
	RegMCCR=	0x00,		// MCP control register
	 MCCRasdMask=	0x7F,		//  audio sample rate divisior mask
	 MCCRtsdMask=	0x7F << 8,	//  tsd bit mask
	 MCCRtsdShft=	8,		//  tsd shift
	 MCCRmce=	1 << 16,	//  MCP enable
	 MCCRecs=	1 << 17,	//  external clock select
	 MCCRadmShft=	18,		//  a/d sampling mode
	 MCCRttm=	1 << 19,	//  telecom xmt fifo interrupt
	 MCCRttmShft=	19,		//  telecom xmt fifo interrupt shift
	 MCCRtrm=	1 << 20,	//  telecom rcv fifo interrupt
	 MCCRtrmShft=	20,		//  telecom rcv fifo interrupt shift
	 MCCRatm=	1 << 21,	//  audio xmt fifo interrupt
	 MCCRarm=	1 << 22,	//  audio rcv fifo interrupt
	 MCCRlbm=	1 << 23,	//  loopback mode
	 MCCRecpMask=	3 << 24,	//  external clock prescaler mask
	 MCCRecpShft=	24,

	RegMCDR0=	0x08,		// MCP data register 0 (audio fifo)

	RegMCDR1=	0x0C,		// MCP data register 1 (telecom fifo)

	RegMCDR2=	0x10,		// MCP data register 2
	 MCDR2addrshft=	17,		//  address shift
	 MCDR2write=	1 << 16,	//  write bit
	 MCDR2dataMsk=	0xFFFF,		//  data mask

	RegMCSR=	0x18,		// mcp status register
	 MCSRats=	1 << 0,		//  audio xmt fifo service request
	 MCSRars=	1 << 1,		//  audio rcv fifo service request
	 MCSRtts=	1 << 2,		//  telecom xmt fifo service request
	 MCSRtrs=	1 << 3,		//  telecom rcv fifo service request
	 MCSRatu=	1 << 4,		//  audio xmt fifo underrun
	 MCSRaro=	1 << 5,		//  audio rcv fifo overrun
	 MCSRttu=	1 << 6,		//  telecom xmt fifo underrun
	 MCSRtro=	1 << 7,		//  telecom rcv fifo overrun
	 MCSRanf=	1 << 8,		//  audio xmt fifo not full
	 MCSRane=	1 << 9,		//  audio rcv fifo not empty
	 MCSRtnf=	1 << 10,	//  telecom xmt fifo not full
	 MCSRtne=	1 << 11,	//  telecom rcv fifo not empty
	 MCSRcwc=	1 << 12,	//  codec write completed
	 MCSRcrc=	1 << 13,	//  codec read completed
	 MCSRace=	1 << 14,	//  audio codec enabled
	 MCSRtce=	1 << 15,	//  telecom codec enabled
};


/* UCB1200 registers */

enum {
  UCBData =	0x0,	/* I/O data bits */
  UCBDir =	0x1, 	/* I/O direction (1==output) */
	SIB_ZERO = 	BIT(15),	//  1: SIBDOUT = 0 during 2nd SIB word
					//  0: SIBDOUT is tri-stated
  UCBRising =   0x2, 	/* rising interrupt enable */
  	ADC_RIS_INT = 	BIT(11),
	TSPX_RIS_INT = 	BIT(12),
	TSMX_RIS_INT = 	BIT(13),
	TCLIP_RIS_INT = BIT(14),
	ACLIP_RIS_INT = BIT(15),
  UCBFalling =   0x3, 	/* falling interrupt enable */
  	ADC_FAL_INT = 	BIT(11),
	TSPX_FAL_INT = 	BIT(12),
	TSMX_FAL_INT = 	BIT(13),
	TCLIP_FAL_INT = BIT(14),
	ACLIP_FAL_INT = BIT(15),
  UCBIntStat =  0x4, 	/* Interrupt status register */
  	ADC_INT_STAT = 	BIT(11),
	TSPX_INT_STAT = BIT(12),
	TSMX_INT_STAT = BIT(13),
	TCLIP_INT_STAT = BIT(14),
	ACLIP_INT_STAT = BIT(15),
  UCBIntClear =  0x4, 	/* Interrupt clear register */
  	ADC_INT_CLR = 	BIT(11),
	TSPX_INT_CLR = 	BIT(12),
	TSMX_INT_CLR = 	BIT(13),
	TCLIP_INT_CLR = BIT(14),
	ACLIP_INT_CLR = BIT(15),
  UCBTelCtlA = 0x5,	/* Telecom Control A */
	TEL_DIV_m =	BITS(0,6),
	TEL_LOOP =	BIT(7),
  UCBTelCtlB = 0x6,	/* Telecom Control A */
	TEL_VOICE_ENA = BIT(3),
	TEL_CLIP_STAT = BIT(4),
	TEL_CLIP_CLR =	BIT(4),
	TEL_ATT	= 	BIT(6),
	TEL_SIDE_ENA = 	BIT(11),
	TEL_MUTE = 	BIT(13),
	TEL_IN_ENA = 	BIT(14),
	TEL_OUT_ENA = 	BIT(15),
  UCBAudCtlA = 0x7,	/* Audio Control A */
	AUD_DIV_s = 	0,		/* audio divisor shift */
	AUD_DIV_m =	BITS(0,6),	/* audio divisor mask */
	AUD_GAIN_s =	7,		/* audio gain shift */
	AUD_GAIN_m =	BITS(7,11),	/* audo gain mask */
  UCBAudCtlB = 0x8,	/* Audio Control A */
	AUD_ATT_s =	0,
	AUD_CLIP_STAT = BIT(6),
	AUD_CLIP_CLR = 	BIT(6),
	AUD_LOOP =	BIT(8),
	AUD_MUTE =	BIT(13),
	AUD_IN_ENA =	BIT(14),
	AUD_OUT_ENA = 	BIT(15),
  UCBTouchCtl=	0x9, 	/* Touch Screen control register */
	TSMX_POW = 	BIT(0),
	TSPX_POW = 	BIT(1),
	TSMY_POW = 	BIT(2),
	TSPY_POW = 	BIT(3),
	TSMX_GND =	BIT(4),
	TSPX_GND = 	BIT(5),
	TSMY_GND = 	BIT(6),
	TSPY_GND = 	BIT(7),
	TSC_MODE_INT = 	0<<8,
	TSC_MODE_PRES = 1<<8,
	TSC_MODE_POS = 	2<<8,
	TSC_BIAS_ENA = 	BIT(11),
	TSPX_LOW =	BIT(12),
	TSMX_LOW = 	BIT(13),
  UCBAdcCtl =	0xa, 	/* ADC control register */
	ADC_SYNC_ENA = 	BIT(0),
	VREFBYP_CON = 	BIT(1),
	ADC_INPUT_TSPX = 0<<2,
	ADC_INPUT_TSMX = 1<<2,
	ADC_INPUT_TSPY = 2<<2,
	ADC_INPUT_TSMY = 3<<2,
 	EXT_REF_ENA =	BIT(5),
	ADC_START =	BIT(7),
	ADC_ENA =	BIT(15),
  UCBAdcData =	0xb, 	/* ADC data register */
	ADC_DATA_s = 	5,		/* ADC data shift */
	ADC_DATA_m = 	BITS(5,14),	/* ADC data mask */
	ADC_DAT_VAL = 	BIT(15),	/* ADC valid */
  UCBId =	0xc,	/* ID register */
	UCBVERSION_s =	0,
	UCBVERSION_m =	BITS(0, 5),
	UCBDEVICE_s =	6,
	UCBDEVICE_m =	BITS(6,11),
	UCBSUPPLIER_s =	12,
	UCBSUPPLIER_m =	BITS(12,15),
  UCBMode =	0xd,	/* Mode register */
	AUD_TEST =	BIT(0),
	TEL_TEST =	BIT(1),
	PROD_TEST_MODE_s = 2,
	PROD_TEST_MODE_m = BITS(2,5),
	DYN_VFLAG_ENA =	BIT(12),
	AUD_OFF_CAN =	BIT(13),
  UCBNull =	0xf,	/* Null register */
};


static int touch_ctl[] = {
	[TOUCH_READ_X1] TSC_MODE_POS|TSC_BIAS_ENA|TSPX_GND|TSMX_POW,
	[TOUCH_READ_X2] TSC_MODE_POS|TSC_BIAS_ENA|TSPX_GND|TSMX_POW,
	[TOUCH_READ_X3] TSC_MODE_POS|TSC_BIAS_ENA|TSPX_POW|TSMX_GND,
	[TOUCH_READ_X4] TSC_MODE_POS|TSC_BIAS_ENA|TSPX_POW|TSMX_GND,
	[TOUCH_READ_Y1] TSC_MODE_POS|TSC_BIAS_ENA|TSPY_POW|TSMY_GND,
	[TOUCH_READ_Y2] TSC_MODE_POS|TSC_BIAS_ENA|TSPY_GND|TSMY_POW,
	[TOUCH_READ_Y3] TSC_MODE_POS|TSC_BIAS_ENA|TSPY_POW|TSMY_GND,
	[TOUCH_READ_Y4] TSC_MODE_POS|TSC_BIAS_ENA|TSPY_GND|TSMY_POW,
	[TOUCH_READ_P1] TSC_MODE_PRES|TSC_BIAS_ENA|
				TSPX_POW|TSMX_POW|TSPY_GND|TSMY_GND,
	[TOUCH_READ_P2] TSC_MODE_PRES|TSC_BIAS_ENA|
				TSPX_GND|TSMX_GND|TSPY_POW|TSMY_POW,
	[TOUCH_READ_RX1] TSC_MODE_PRES|TSC_BIAS_ENA|TSPX_POW|TSMX_GND,
	[TOUCH_READ_RX2] TSC_MODE_PRES|TSC_BIAS_ENA|TSPX_GND|TSMX_POW,
	[TOUCH_READ_RY1] TSC_MODE_PRES|TSC_BIAS_ENA|TSPY_POW|TSMY_GND,
	[TOUCH_READ_RY2] TSC_MODE_PRES|TSC_BIAS_ENA|TSPY_GND|TSMY_POW,
};

static int touch_inp[] = {
	[TOUCH_READ_X1] ADC_INPUT_TSPY,
	[TOUCH_READ_X2] ADC_INPUT_TSMY,
	[TOUCH_READ_X3] ADC_INPUT_TSPY,
	[TOUCH_READ_X4] ADC_INPUT_TSMY,
	[TOUCH_READ_Y1] ADC_INPUT_TSPX,
	[TOUCH_READ_Y2] ADC_INPUT_TSPX,
	[TOUCH_READ_Y3] ADC_INPUT_TSMX,
	[TOUCH_READ_Y4] ADC_INPUT_TSMX,
	[TOUCH_READ_P1] 0,
	[TOUCH_READ_P2] 0,
	[TOUCH_READ_RX1] 0,
	[TOUCH_READ_RX2] 0,
	[TOUCH_READ_RY1] 0,
	[TOUCH_READ_RY2] 0,
};



#define MCPBASE	0x80060000
#define MCPREG(reg)	((ulong*)((MCPBASE) + (reg)))


static void
codecwrite(uchar reg, ulong val)
{
	*MCPREG(RegMCDR2) = (reg << MCDR2addrshft) | MCDR2write | val;
	while(!(*MCPREG(RegMCSR) & MCSRcwc))
		;	
}

static ushort
codecread(uchar reg)
{
	*MCPREG(RegMCDR2) = (reg << MCDR2addrshft);
	while(!(*MCPREG(RegMCSR) & MCSRcrc))
		;
	return *MCPREG(RegMCDR2) & MCDR2dataMsk;
}


//
// enable the MCP
//
void
mcpinit(void)
{
	*MCPREG(RegMCCR) = MCCRmce;
}


ushort
mcpadcread(int ts)
{
	int input = touch_inp[ts];
	ulong v;
	codecwrite(UCBAdcCtl, ADC_ENA|VREFBYP_CON|input);
	codecwrite(UCBAdcCtl, ADC_START |ADC_ENA|VREFBYP_CON|input);
	do {
		v = codecread(UCBAdcData);
	} while(!(v&ADC_DAT_VAL));
	return (v&ADC_DATA_m)>>ADC_DATA_s;
}

void
mcptouchsetup(int ts)
{	
	codecwrite(UCBTouchCtl, touch_ctl[ts]);
}

void
mcpgpiowrite(ushort mask, ushort data)
{
	codecwrite(UCBData, (codecread(UCBData)&~mask)|data);
}

void
mcpgpiosetdir(ushort mask, ushort dir)
{
	codecwrite(UCBDir, (codecread(UCBDir)&~mask)|dir);
}

ushort
mcpgpioread(void)
{
	return codecread(UCBData);
}

