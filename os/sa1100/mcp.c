//
// Multimedia Communications Port
//
// interface to the external UCB1100, UCB1200, and CRYSTAL4271 codecs
// both touchscreen and telecom
//

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"io.h"

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

	 //
	 // UCB1100/UCB1200 registers
	 //
	UCBData=	0x0,		// IO port data register
	 IO_DATA_m=	0x3FF,		//  io pin data

	UCBDir=		0x1, 		// I/O direction (1==output)
	 IO_DIR_m=	0x3FF,		//  1: pin is output, 0: pin is input
	 SIB_ZERO=	BIT(15),	//  1: SIBDOUT = 0 during 2nd SIB word
					//  0: SIBDOUT is tri-stated

	UCBRising=	0x2,		// Rising edge interrupt enable reg
  	 ADC_RIS_INT=	BIT(11),	//  1: Rising edge of adc_ready enabled
	 TSPX_RIS_INT=	BIT(12),	//  1: Rising edge int of TSPX enabled
	 TSMX_RIS_INT=	BIT(13),	//  1: Rising edge int of TSMX enabled
	 TCLIP_RIS_INT=	BIT(14),	//  1: Rising edge of telecom clip ena
	 ACLIP_RIS_INT=	BIT(15),	//  1: Rising edge of audio clip ena

	UCBFalling=	0x3, 		// Falling edge interrupt enable reg
  	 ADC_FAL_INT=	BIT(11),	//  1: Falling edge int of adc ena
	 TSPX_FAL_INT=	BIT(12),	//  1: Falling edge int of TSPX ena
	 TSMX_FAL_INT=	BIT(13),	//  1: Falling edge int of TSMX ena
	 TCLIP_FAL_INT=	BIT(14),	//  1: Falling edge of telecom clip ena
	 ACLIP_FAL_INT=	BIT(15),	//  1: Falling edge of audio clip ena

	UCBIntStat=	0x4,		// Interrupt status register
  	 ADC_INT_STAT=	BIT(11),	//  clr int of adc
	 TSPX_INT_STAT=	BIT(12),	//  status of TSPX int
	 TSMX_INT_STAT=	BIT(13),	//  status of TSMX int
	 TCLIP_INT_STAT=BIT(14),	//  status of telecom clip int
	 ACLIP_INT_STAT=BIT(15),	//  status of audio clip int
	UCBIntClear=	0x4, 		// Interrupt clear reg
  	 ADC_INT_CLR=	BIT(11),	//  clr adc int
	 TSPX_INT_CLR=	BIT(12),	//  clr TSPX int
	 TSMX_INT_CLR=	BIT(13),	//  clr TSMX int
	 TCLIP_INT_CLR=	BIT(14),	//  clr telecom clip int
	 ACLIP_INT_CLR=	BIT(15),	//  clr audio clip int

	UCBTelCtlA=	0x5,		// Telecom Control A
	 TEL_DIV_m=	BITS(0,6),	//  telecom sample rate divisor 16..127
	 TEL_LOOP=	BIT(7),		//  enable voice band filter

	UCBTelCtlB=	0x6,		// Telecom Control B
	 TEL_VOICE_ENA= BIT(3),		//  enable voice band filter
	 TEL_CLIP_STAT=	BIT(4),		//  telecom clip detection status
	 TEL_CLIP_ENA=	BIT(4),		//  enable telecom clip detection
	 TEL_ATT= 	BIT(6),		//  enable telecom attenuation
	 TEL_SIDE_ENA= 	BIT(11),	//  enable sidetone suppression circuit
	 TEL_MUTE= 	BIT(13),	//  enable mute
	 TEL_IN_ENA= 	BIT(14),	//  enable telecom input path
	 TEL_OUT_ENA= 	BIT(15),	//  enable telecom output path

	UCBAudCtlA=	0x7,		// Audio Control A
	 AUD_DIV_m=	BITS(0,6),	//  audio sample rate divisor 6 .. 127
	 AUD_GAIN_s=	7,		//  audio input gain setting 0 .. 22.5db
	 AUD_GAIN_m=	BITS(7,11),	//  audio input gain shift

	UCBAudCtlB=	0x8,		// Audio Control B
	 AUD_ATT_m=	BITS(0,4),	//  audio output attenuation 0 .. 69db
	 AUD_CLIP_STAT=	BIT(6),		//  audio clip detection status
	 AUD_CLIP_CLR=	BIT(6),		//  clear audio clip detection
	 AUD_LOOP=	BIT(8),		//  enable audio loopback
	 AUD_MUTE=	BIT(13),	//  enable audio mute
	 AUD_IN_ENA=	BIT(14),	//  enable audio input path
	 AUD_OUT_ENA=	BIT(15),	//  enable audio output path

	UCBTouchCtl=	0x9,		// Touch Screen control register
	 TSMX_POW=	BIT(0),		//  TSMX pin is powered
	 TSPX_POW=	BIT(1),		//  TSPX pin is powered
	 TSMY_POW=	BIT(2),		//  TSMY pin is powered
	 TSPY_POW=	BIT(3),		//  TSPY pin is powered
	 TSMX_GND=	BIT(4),		//  TSMX pin is grounded
	 TSPX_GND=	BIT(5),		//  TSPX pin is grounded
	 TSMY_GND=	BIT(6),		//  TSMY pin is grounded
	 TSPY_GND=	BIT(7),		//  TSPY pin is grounded
	 TSC_MODE_INT=	0<<8,		//  interrupt operation mode
	 TSC_MODE_PRES=	1<<8,		//  pressure measurement mode
	 TSC_MODE_POS=	2<<8,		//  position measurement mode
	 TSC_BIAS_ENA=	BIT(11),	//  enable touch screen bias circuit
	 TSPX_LOW=	BIT(12),	//  TSPX: 0 == pen up, 1 == pen down
	 TSMX_LOW=	BIT(13),	//  TSMX: 0 == pen up, 1 == pen down

	UCBAdcCtl=	0xa, 		// ADC control register
	 ADC_SYNC_ENA=	BIT(0),		//  enable adc sync mode
	 VREFBYP_CON=	BIT(1),		//  internal ref voltage to VREFBYP
	 ADC_INPUT_TSPX=0<<2,		//  select TSPX
	 ADC_INPUT_TSMX=1<<2,		//  select TSMX
	 ADC_INPUT_TSPY=2<<2,		//  select TSPY
	 ADC_INPUT_TSMY=3<<2,		//  select TSMY
	 ADC_INPUT_AD0=	4<<2,		//  select AD0
	 ADC_INPUT_AD1=	5<<2,		//  select AD1
	 ADC_INPUT_AD2=	6<<2,		//  select AD2
	 ADC_INPUT_AD3=	7<<2,		//  select AD3
	 EXT_REF_ENA=	BIT(5),		//  externel ref voltage to VREFBYP
	 ADC_START=	BIT(7),		//  begin ADC conversion sequence
	 ADC_ENA=	BIT(15),	//  activate the ADC circuit

	UCBAdcData=	0xb, 		// ADC data register
	 ADC_DATA_s=	5,		//  ADC data shift
	 ADC_DATA_m=	BITS(5,14),	//  ADC data mask
	 ADC_DAT_VAL=	BIT(15),	//  1: ADC conversion complet

 	UCBId=		0xc,		// ID register
	 UCBVERSION_m=	BITS(0, 5),	//  version mask
	 UCBDEVICE_s=	6,		//  device shift
	 UCBDEVICE_m=	BITS(6,11),	//  device mask
	 UCBSUPPLIER_s=	12,		//  supplier shift
	 UCBSUPPLIER_m=	BITS(12,15),	//  supplier mask

	UCBMode=0xd,			// Mode register
	 AUD_TEST=	BIT(0),		//  1: audio test mode activated
	 TEL_TEST=	BIT(1),		//  1: telecom test mode activated
	 PROD_TEST_MODE_s=2,		//  product test shift
	 PROD_TEST_MODE_m=BITS(2,5),	//  product test mask
	 DYN_VFLAG_ENA=	BIT(12),	//  1: dynamic data valid mode active
					//     for both audio and telecom data
					//     valid flag
	 AUD_OFF_CAN=	BIT(13),	//  1: offset cancelling circuit in
					//     audio input path is enabled

	UCBreserved=	0xe,		// Reserved Register

	UCBNull=	0xf,		// Null register, always returns 0xFFFF
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
#undef MCPREG
#define MCPREG(reg)	((ulong*)((MCPBASE) + (reg)))

static void
codecwrite(uchar reg, ushort val)
{
	int s, tries;

	s = splhi();
	*MCPREG(RegMCDR2) = (reg << MCDR2addrshft) | MCDR2write |
			    (val & MCDR2dataMsk);
	tries = 0;
	while(!(*MCPREG(RegMCSR) & MCSRcwc))
		if (++tries > 10000000) {
			iprint("codecwrite failed\n");
			panic("codecwrite");
		}
	splx(s);
}

static ushort
codecread(uchar reg)
{
	ushort v;
	int s, tries;

	s = splhi();
	*MCPREG(RegMCDR2) = (reg << MCDR2addrshft);
	tries = 0;
	while(!(*MCPREG(RegMCSR) & MCSRcrc))
		if (++tries > 10000000) {
			iprint("codecread failed\n");
			panic("codecread");
		}
	v = *MCPREG(RegMCDR2) & MCDR2dataMsk;
	splx(s);

	return v;
}

//
// enable the MCP
//
void
mcpinit(void)
{
	*MCPREG(RegMCCR) = MCCRmce;
}

void
mcptelecomsetup(ulong tfreq, uchar adm, uchar xmtint, uchar rcvint)
{
	uchar tsd = (MCPclk / (32 * tfreq)) & TEL_DIV_m;
	ulong v = TEL_SIDE_ENA | TEL_ATT;

	codecwrite(UCBAudCtlB, AUD_MUTE);		// disable audio

	 //
	 // set the codec telecom sample rate divisor the same the MCP.
	 //
	codecwrite(UCBTelCtlA, tsd);

	*MCPREG(RegMCCR) = (tsd << MCCRtsdShft) |	// sample rate divisor
			   MCCRmce |			// enable MCP
			   (adm << MCCRadmShft) |	// Data Sampling mode
			   (xmtint << MCCRttmShft) |	// xmt int enable
			   (rcvint << MCCRtrmShft);	// rcv int enable

	if(xmtint)
		v |= TEL_OUT_ENA;
	if(rcvint)
		v |= TEL_IN_ENA;

	codecwrite(UCBTelCtlB, v);
}

void
mcpspeaker(int on, int vol)
{
	int ctl;

	if (on && vol != 0) {
		ctl = AUD_OUT_ENA;
		if (vol < 2)
			ctl |= 4;		/* -12dB? */
	}
	else
		ctl = AUD_MUTE;
	codecwrite(UCBAudCtlB, ctl);
}

void
mcpsettfreq(ulong tfreq)
{
	uchar tsd = (MCPclk / (32 * tfreq)) & TEL_DIV_m;

	 //
	 // set codec tsd
	 //
	codecwrite(UCBTelCtlA, tsd);

	 //
	 // set MCP tsd
	 //
	*MCPREG(RegMCCR) &= ~MCCRtsdMask;
	*MCPREG(RegMCCR) |= tsd << MCCRtsdShft;
}

ulong
mcpgettfreq(void)
{
	return MCPclk/(32*((*MCPREG(RegMCCR)&MCCRtsdMask)>>MCCRtsdShft));
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
mcptouchintrenable(void)
{
	codecwrite(UCBTouchCtl, TSC_MODE_INT
			|TSPX_POW|TSMX_POW|TSMY_GND|TSPY_GND);
	codecwrite(UCBIntClear, 0);
	codecwrite(UCBIntClear, TSPX_INT_CLR);
	codecwrite(UCBFalling, codecread(UCBFalling) | TSPX_FAL_INT);
}

void
mcptouchintrdisable(void)
{
	codecwrite(UCBFalling, codecread(UCBFalling) & ~TSPX_FAL_INT);
	codecwrite(UCBIntClear, TSPX_INT_CLR);
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

