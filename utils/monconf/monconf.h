
#define MONCTAB_MAGIC	0xfeedb0ba

typedef struct MonMisc MonMisc;
struct MonMisc {
	uchar	montype;	/* 1=demon, 3=angel, 5=styxmon */
	uchar	_rsvd1;
	uchar	_rsvd2;
	uchar	_rsvd3;
	ulong	monname;
	ulong	monname_size;
	uchar	monver_major;
	uchar	monver_minor;
	uchar	monver_patch;
	uchar	_rsvd4;
	ulong	_rsvd5;
	ulong	cpuspeed_ppcr;
	ulong	cpuspeed_hz;
	ulong	_rsvd6;
	ulong	flashbase;
	ulong	_rsvd7;
	ulong	noauto_addr;
	ulong	noauto_mask;
	ulong	noauto_val;
	ulong	gpdr;
	ulong	gafr;
	ulong	gpsr;		/* ~gpsr will be applied to gpcr */
};

typedef struct MmapConf MmapConf;
struct MmapConf {
	ulong	va;
	ulong	vs;
	ulong	pa;
	ulong	fl;
};


// SA1100-specific stuff:
typedef struct DramConf DramConf;
struct DramConf {
	ulong	mdcnfg;
	ulong	mdcas[3];
	ushort	msc[4];
	ulong	mecr;
};

