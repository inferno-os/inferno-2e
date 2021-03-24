/*
 * values for RPXLite AW
 */
enum {
	/* CS assignment */
	BOOTCS = 0,
	DRAM1 = 1,
	DRAM2 = 2,
	BCSRCS = 3,
	SDRAM = 4,
};

/*
 * BCSR bits (there's only one register)
 */
enum {
	EnableEnet =	IBIT(0),
	EnableXcrLB=	IBIT(1),
	DisableColTest=	IBIT(2),
	DisableFullDplx=IBIT(3),
	LedOff=		IBIT(4),
	DisableUSB=	IBIT(5),
	HighSpdUSB=	IBIT(6),
	EnableUSBPwr=	IBIT(7),
	/* 8,9,10 unused */
	PCCVCCMask=	IBIT(12)|IBIT(13),
	PCCVPPMask=	IBIT(14)|IBIT(15),
	PCCVCC0V=	0,
	PCCVCC5V=	IBIT(13),
	PCCVCC3V=	IBIT(12),
	PCCVPP0V=	0,
	PCCVPP5V=	IBIT(14),
	PCCVPP12V=	IBIT(15),
	PCCVPPHiZ=	IBIT(14)|IBIT(15),
	/* 16-23 NYI */
	DipSwitchMask=	IBIT(24)|IBIT(25)|IBIT(26)|IBIT(27),
	DipSwitch0=	IBIT(24),
	DipSwitch1=	IBIT(25),
	DipSwitch2=	IBIT(26),
	DipSwitch3=	IBIT(27),
	/* bit 28 RESERVED */
	FlashComplete=	IBIT(29),
	NVRAMBattGood=	IBIT(30),
	RTCBattGood=	IBIT(31),
};
