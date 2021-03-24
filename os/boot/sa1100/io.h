/*
 * Memory Map (upated from SA1100 v2.2 Specification)
 */


#define PCMCIAbase	0x20000000
#define PCMCIAsize	0x10000000
#define PCMCIAcard(n)	(PCMCIAbase+((n)*PCMCIAsize))
#define INTERNALbase	0x80000000
#define PERPHbase	(INTERNALbase+0x0)
#define SYSCTLbase	(INTERNALbase+0x10000000)
#define MEMCTLbase	(INTERNALbase+0x20000000)
#define DMAbase		(INTERNALbase+0x30000000)
#define DRAMbase	(0xC0000000)
#define ZERObase	(DRAMbase+0x20000000)

#define SERIALbase(n)	(PERPHbase+0x10000*(n))

#define PCMCIAIO(n)	(PCMCIAcard(n)+0x0)		/* I/O space */
#define PCMCIAAttr(n)	(PCMCIAcard(n)+0x8000000) /* Attribute space*/
#define PCMCIAMem(n)	(PCMCIAcard(n)+0xC000000) /* Memory space */




#define INTRREG 	((IntrReg*)(SYSCTLbase+0x50000))
typedef struct IntrReg IntrReg;
struct IntrReg {
	ulong	icip;	// IRQ pending
	ulong	icmr;	// mask
	ulong	iclr;	// level
	ulong	iccr;	// control
	ulong	icfp;	// FIQ pending
	ulong	_;
	ulong	_;
	ulong	_;
	ulong	icpr;	// pending
};

#define GPIOvec(n)	((n)<11 ? n : n+(32-11))
#define GPIObit(n)	((n))			/* GPIO Edge Detect bits */
#define LCDbit		(12)			/* LCD Service Request */
#define UDCbit		(13)			/* UDC Service Request */
#define SDLCbit		(14)			/* SDLC Service Request */
#define UARTbit(n)	(15+((n)-1))		/* UART Service Request */
#define HSSPbit		(16)			/* HSSP Service Request */
#define MCPbit		(18)			/* MCP Service Request */
#define SSPbit		(19)			/* SSP Serivce Request */
#define DMAbit(chan)	(20+(chan))		/* DMA channel Request */
#define OSTimerbit(n)	(26+(n))		/* OS Timer Request */
#define RTCticbit	(30)			/* One Hz tic occured */
#define RTCalarmbit	(31)			/* RTC = alarm register */
#define MaxIRQbit	31			/* Maximum IRQ */
#define MaxGPIObit	27			/* Maximum GPIO */


#define GPIOREG		((GpioReg*)(SYSCTLbase+0x40000))
typedef struct GpioReg GpioReg;
struct GpioReg {
	union { ulong gplr; ulong pinlevel; };	// pin level
	union { ulong gpdr; ulong pindir; };	// pin direction
	union { ulong gpsr; ulong pinset; };	// output set
	union { ulong gpcr; ulong pinclear; };	// output clear
	union { ulong grer; ulong pinred; };	// rising edge
	union { ulong gfer; ulong pinfed; };	// falling edge
	union { ulong gedr; ulong pineds; };	// edge status
	union { ulong gafr; ulong altfunc; };	// alternate function
};

#define RTCREG		((RtcReg*)(SYSCTLbase+0x10000))
typedef struct RtcReg RtcReg;
struct RtcReg {
	ulong	rtar;	// alarm
	ulong	rcnr;	// count
	ulong	rttr;	// trim
	ulong	rtsr;	// status
};

#define OSTMRREG	((OstmrReg*)(SYSCTLbase+0x00000))
typedef struct OstmrReg OstmrReg;
struct OstmrReg {
	ulong	osmr[4];	// match
	ulong	oscr;		// counter
	ulong	ossr;		// status
	ulong	ower;		// watchdog
	ulong	oier;		// interrupt enable
};

#define PMGRREG		((PmgrReg*)(SYSCTLbase+0x20000))
typedef struct PmgrReg PmgrReg;
struct PmgrReg {
	ulong	pmcr;	// ctl register
	ulong	pssr;	// sleep status
	ulong	pspr;	// scratch pad
	ulong	pwer;	// wakeup enable
	ulong	pcfr;	// general conf
	ulong	ppcr;	// PLL configuration
	ulong	pgsr;	// GPIO sleep state
	ulong	posr;	// oscillator status
};

#define RESETREG	((ResetReg*)(SYSCTLbase+0x30000))
typedef struct ResetReg ResetReg;
struct ResetReg {
	ulong	rsrr;	// software reset
	ulong	rcsr;	// status
	ulong	tucr;	// reserved for test
};


#define PPCREG		((PpcReg*)(SYSCTLbase+0x60000))
typedef struct PpcReg PpcReg;
struct PpcReg {
	ulong	ppdr;	// pin direction
	ulong	ppsr;	// pin state
	ulong	ppar;	// pin assign
	ulong	psdr;	// sleep mode
	ulong	ppfr;	// pin flag reg
	uchar	_[0x1c]; // pad to 0x30
	ulong	mccr1;	// MCP control register 1
};

enum {
	PPC_V_SPR= 18,
};


#define MEMCFGREG	((MemcfgReg*)(MEMCTLbase))
typedef struct MemcfgReg MemcfgReg;
struct MemcfgReg {
	ulong	mdcnfg;		// DRAM config
	ulong	mdcas[3];	// CAS waveform
	ushort	msc[4];		// static banks
	ulong	mecr;		// expansion bus
};


#define DMAREG(n)	((DmaReg*)(DMAbase+0x20*(n)))
typedef struct DmaReg DmaReg;
struct DmaReg {
	ulong	ddar;	// DMA device address
	ulong	dcsr_s;	// set 
	ulong	dcsr_c; // clear 
	ulong	dcsr;   // read 
	ulong	dbsa;	// Buffer A start address 
	ulong	dbta;	// Buffer A transfer count
	ulong	dbsb;	// Buffer B start address
	ulong	dbtb;	// Buffer B transfer count
};

enum {
	DCSR_RUN= 0x01,
	DCSR_IE= 0x02,
	DCSR_ERROR= 0x04,
	DCSR_DONEA= 0x08,
	DCSR_STRTA= 0x10,
	DCSR_DONEB= 0x20,
	DCSR_STRTB= 0x40,
	DCSR_BIU= 0x80,
};


#define LCDREG		((LcdReg*)(DMAbase+0x100000))
typedef struct LcdReg LcdReg;
struct LcdReg {
	ulong	lccr0;	// control 0
	ulong	lcsr;	// status 
	ulong	_;
	ulong	_;
	ulong	dbar1;	// DMA chan 1, base
	ulong	dcar1;	// DMA chan 1, count
	ulong	dbar2;	// DMA chan 2, base
	ulong	dcar2;	// DMA chan 2, count
	ulong	lccr1;	// control 1
	ulong	lccr2;	// control 2
	ulong	lccr3;	// control 3
};

enum {
	LCD0_M_LEN= 0x00000001,
	LCD0_M_CMS= 0x00000002,
	LCD0_M_SDS= 0x00000004,
	LCD0_M_PAS= 0x00000080,
	LCD0_M_BLE= 0x00000100,
	LCD0_M_DPD= 0x00000200,
	LCD0_M_FDD= 0x000FF000,
	LCD0_V_LEN= 0,
	LCD0_V_CMS= 1,
	LCD0_V_SDS= 2,
	LCD0_V_PAS= 7,
	LCD0_V_BLE= 8,
	LCD0_V_DPD= 9,
	LCD0_V_FDD= 12,

	LCD1_M_PPL= 0x000003FF,
	LCD1_M_HSW= 0x0000FC00,
	LCD1_M_ELW= 0x00FF0000,
	LCD1_M_BLW= 0xFF000000,
	LCD1_V_PPL= 0,
	LCD1_V_HSW= 10,
	LCD1_V_ELW= 16,
	LCD1_V_BLW= 24,

	LCD2_M_LPP= 0x000003FF,
	LCD2_M_VSW= 0x0000FC00,
	LCD2_M_EFW= 0x00FF0000,
	LCD2_M_BFW= 0xFF000000,
	LCD2_V_LPP= 0,
	LCD2_V_VSW= 10,
	LCD2_V_EFW= 16,
	LCD2_V_BFW= 24,

	LCD3_M_PCD= 0x000000FF,
	LCD3_M_ACB= 0x0000FF00,
	LCD3_M_API= 0x000F0000,
	LCD3_M_VSP= 0x00100000,
	LCD3_M_HSP= 0x00200000,
	LCD3_M_PCP= 0x00400000,
	LCD3_M_OEP= 0x00800000,
	LCD3_V_PCD= 0,
	LCD3_V_ACB= 8,
	LCD3_V_API= 16,
	LCD3_V_VSP= 20,
	LCD3_V_HSP= 21,
	LCD3_V_PCP= 22,
	LCD3_V_OEP= 23,
};


/* Serial devices:
 *	0	USB		Serial Port 0
 *	1	UART		\_ Serial Port 1
 *	2	SDLC		/
 *	3	UART		\_ Serial Port 2 (eia1)
 *	4	ICP/HSSP	/
 *	5	ICP/UART	Serial Port 3 (eia0)
 *	6	MPC		\_ Serial Port 4
 *	7	SSP		/
 */ 

#define USBREG	((UsbReg*)(SERIALbase(0)))
typedef struct UsbReg UsbReg;
struct UsbReg {
	ulong	udccr;	// control
	ulong	udcar;	// address
	ulong	udcomp;	// out max packet
	ulong	udcimp;	// in max packet
	ulong	udccs0;	// endpoint 0 control/status
	ulong	udccs1;	// endpoint 1(out) control/status
	ulong	udccs2;	// endpoint 2(int) control/status
	ulong	udcd0;	// endpoint 0 data register
	ulong	udcwc;	// endpoint 0 write control register
	ulong	_;
	ulong	udcdr;	// transmit/receive data register (FIFOs)
	ulong	_;
	ulong	dcsr;	// status/interrupt register
};

/* UARTs 1, 2, 3 are mapped to serial devices 1, 3, and 5 */
#define UARTREG(n)	((UartReg*)(SERIALbase(2*(n)-1)))
typedef struct UartReg UartReg;
struct UartReg {
	ulong	utcr0;	// control 0 (bits, parity, clocks)
	ulong	utcr1;	// control 1 (bps div hi)
	ulong	utcr2;	// control 2 (bps div lo)
	ulong	utcr3;	// control 3
	ulong	utcr4;	// control 4 (only serial port 2 (device 3))
	ulong	utdr;	// data
	ulong	_;
	ulong	utsr0;	// status 0
	ulong	utsr1;	// status 1
};

enum {
	UTCR0_PE=	0x01,
	UTCR0_OES=	0x02,
	UTCR0_SBS=	0x04,
	UTCR0_DSS=	0x08,
	UTCR0_SCE=	0x10,
	UTCR0_RCE=	0x20,
	UTCR0_TCE=	0x40,

	UTCR3_RXE=	0x01,
	UTCR3_TXE=	0x02,
	UTCR3_BRK=	0x04,
	UTCR3_RIM=	0x08,
	UTCR3_TIM=	0x10,
	UTCR3_LBM=	0x20,

	UTSR0_TFS=	0x01,
	UTSR0_RFS=	0x02,
	UTSR0_RID=	0x04,
	UTSR0_RBB=	0x08,
	UTSR0_REB=	0x10,
	UTSR0_EIF=	0x20,

	UTSR1_TBY=	0x01,
	UTSR1_RNE=	0x02,
	UTSR1_TNF=	0x04,
	UTSR1_PRE=	0x08,
	UTSR1_FRE=	0x10,
	UTSR1_ROR=	0x20,
};

#define SDLCREG	((SdlcReg*)(SERIALbase(2)))
typedef struct SdlcReg SdlcReg;
struct SdlcReg {
	uchar	_[0x60];
	ulong	sdcr0;	// control 0
	ulong	sdcr1;	// control 1
	ulong	sdcr2;	// control 2
	ulong	sdcr3;	// control 3
	ulong	sdcr4;	// control 4
	ulong	_;
	ulong	sddr;	// data
	ulong	_;
	ulong	sdsr0;	// status 0
	ulong	sdsr1;	// status 1
};


#define HSSPREG		((HsspReg*)(SERIALbase(4)))
typedef struct HsspReg HsspReg;
struct HsspReg {
	uchar	_[0x60];
	ulong	hscr0;	// control 0
	ulong	hscr1;	// control 1
	ulong	_;
	ulong	hsdr;	// data
	ulong	_;
	ulong	hssr0;	// status 0
	ulong	hssr1;	// status 1
};

#define MCPREG		((McpReg*)(SERIALbase(6)))
typedef struct McpReg McpReg;
struct McpReg {
	ulong	mccr;
	ulong	_;
	ulong	mcdr0;
	ulong	mcdr1;
	ulong	mcdr2;
	ulong	_;
	ulong	mcsr;
};

enum {
	MCCR_M_LBM= 0x800000,
	MCCR_M_ARM= 0x400000,
	MCCR_M_ATM= 0x200000,
	MCCR_M_TRM= 0x100000,
	MCCR_M_TTM= 0x080000,
	MCCR_M_ADM= 0x040000,
	MCCR_M_ECS= 0x020000,
	MCCR_M_MCE= 0x010000,
	MCCR_V_TSD= 8,
	MCCR_V_ASD= 0,

	MCDR2_M_nRW= 0x010000,
	MCDR2_V_RN= 17,

	MCSR_M_TCE= 0x8000,
	MCSR_M_ACE= 0X4000,
	MCSR_M_CRC= 0x2000,
	MCSR_M_CWC= 0x1000,
	MCSR_M_TNE= 0x0800,
	MCSR_M_TNF= 0x0400,
	MCSR_M_ANE= 0x0200,
	MCSR_M_ANF= 0x0100,
	MCSR_M_TRO= 0x0080,
	MCSR_M_TTU= 0x0040,
	MCSR_M_ARO= 0x0020,
	MCSR_M_ATU= 0x0010,
	MCSR_M_TRS= 0x0008,
	MCSR_M_TTS= 0x0004,
	MCSR_M_ARS= 0x0002,
	MCSR_M_ATS= 0x0001,
};

#define SSPREG		((SspReg*)(SERIALbase(7)))
typedef struct SspReg SspReg;
struct SspReg {
	uchar	_[0x60];
	ulong	sscr0;	// control 0
	ulong	sscr1;	// control 1
	ulong	_;
	ulong	ssdr;	// data
	ulong	_;
	ulong	sssr;	// status
};


enum {
	SSCR0_V_SCR= 0x08,
	SSCR0_V_SSE= 0x07,
	SSCR0_V_ECS= 0x06,
	SSCR0_V_FRF= 0x04,

	SSPCR0_M_DSS= 0x0000000F,
	SSPCR0_M_FRF= 0x00000030,
	SSPCR0_M_SSE= 0x00000080,
	SSPCR0_M_SCR= 0x0000FF00,
	SSPCR0_V_DSS= 0,
	SSPCR0_V_FRF= 4,
	SSPCR0_V_SSE= 7,
	SSPCR0_V_SCR= 8,

	SSPCR1_M_RIM= 0x00000001,
	SSPCR1_M_TIN= 0x00000002,
	SSPCR1_M_LBM= 0x00000004,
	SSPCR1_V_RIM= 0,
	SSPCR1_V_TIN= 1,
	SSPCR1_V_LBM= 2,

	SSPSR_M_TNF= 0x00000002,
	SSPSR_M_RNE= 0x00000004,
	SSPSR_M_BSY= 0x00000008,
	SSPSR_M_TFS= 0x00000010,
	SSPSR_M_RFS= 0x00000020,
	SSPSR_M_ROR= 0x00000040,
	SSPSR_V_TNF= 1,
	SSPSR_V_RNE= 2,
	SSPSR_V_BSY= 3,
	SSPSR_V_TFS= 4,
	SSPSR_V_RFS= 5,
	SSPSR_V_ROR= 6,
};




/*
 *	PC compatibility support for PCMCIA drivers
 */

ushort _ins(ulong);
void _outs(ulong, int);
#define inb(addr)	(*((uchar*)(addr)))
#define ins(addr)	_ins(addr)
#define inl(addr)	(*((ulong*)(addr)))
#define outb(addr, val)	*((uchar*)(addr)) = (val)
#define outs(addr, val)	_outs((addr), val)
#define outl(addr, val)	*((ulong*)(addr)) = (val)

void inss(ulong, void*, int);
void outss(ulong, void*, int);

#define _ins(p)	(*(ushort*)(p))
#define _outs(p,v) (*(ushort*)(p) = (v))

#define MaxEISA		2

