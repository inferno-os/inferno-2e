 typedef struct List {
	void	*next;
} List;


typedef struct Alarm {
	List;
	int	busy;
	long	dt;
	void	(*f)(void*);
	void	*arg;
} Alarm;


typedef struct Block Block;
struct Block {
	Block*	next;
	uchar*	rp;			/* first unconsumed byte */
	uchar*	wp;			/* first empty byte */
	uchar*	lim;			/* 1 past the end of the buffer */
	uchar*	base;			/* start of the buffer */
	ulong	flag;
};
#define BLEN(s)	((s)->wp - (s)->rp)

typedef struct IOQ IOQ;
typedef struct IOQ {
	uchar	buf[4096];
	uchar	*in;
	uchar	*out;
	int	state;
	int	(*getc)(IOQ*);
	int	(*putc)(IOQ*, int);
	void	*ptr;
};

enum {
	Eaddrlen	= 6,
	ETHERMINTU	= 60,		/* minimum transmit size */
	ETHERMAXTU	= 1514,		/* maximum transmit size */
	ETHERHDRSIZE	= 14,		/* size of an ethernet header */

	MaxEther	= 4,
};

typedef struct {
	uchar	d[Eaddrlen];
	uchar	s[Eaddrlen];
	uchar	type[2];
	uchar	data[1500];
	uchar	crc[4];
} Etherpkt;

extern uchar broadcast[Eaddrlen];

enum {
	Maxxfer		= 16*1024,	/* maximum transfer size/cmd */
	Npart		= 8+2,		/* 8 sub partitions, disk, and partition */
};

typedef struct {
	ulong	start;
	ulong	end;
	char	name[NAMELEN+1];
} Partition;

typedef struct {
	int	online;
	int	npart;		/* number of real partitions */
	Partition p[Npart];
	ulong	offset;
	Partition *current;	/* current partition */

	ulong	cap;		/* total sectors */
	int	bytes;		/* bytes/sector */
	int	sectors;	/* sectors/track */
	int	heads;		/* heads/cyl */
	long	cyl;		/* cylinders/drive */

	char	lba;		/* true if drive has logical block addressing */
	char	multi;		/* non-zero if drive does multiple block xfers */
} Disc;

enum {
	ScsiTestunit	= 0x00,
	ScsiExtsens	= 0x03,
	ScsiInquiry	= 0x12,
	ScsiModesense	= 0x1a,
	ScsiStartunit	= 0x1B,
	ScsiStopunit	= 0x1B,
	ScsiGetcap	= 0x25,
	ScsiRead	= 0x08,
	ScsiWrite	= 0x0a,
	ScsiExtread	= 0x28,
	ScsiExtwrite	= 0x2a,

	/* data direction */
	ScsiIn		= 1,
	ScsiOut		= 0,
};

typedef struct Scsibuf Scsibuf;
typedef struct Scsibuf {
	void*		virt;
	void*		phys;
	Scsibuf*	next;
};

typedef struct Scsidata {
	uchar*		base;
	uchar*		lim;
	uchar*		ptr;
} Scsidata;

typedef struct Ureg Ureg;

typedef struct Scsi {
	ulong		pid;
	ushort		target;
	ushort		lun;
	ushort		rflag;
	ushort		status;
	Scsidata 	cmd;
	Scsidata 	data;
	Scsibuf*	b;
	uchar*		save;
	uchar		cmdblk[16];
} Scsi;

typedef struct {
	ulong	link;			/* link (old TSS selector) */
	ulong	esp0;			/* privilege level 0 stack pointer */
	ulong	ss0;			/* privilege level 0 stack selector */
	ulong	esp1;			/* privilege level 1 stack pointer */
	ulong	ss1;			/* privilege level 1 stack selector */
	ulong	esp2;			/* privilege level 2 stack pointer */
	ulong	ss2;			/* privilege level 2 stack selector */
	ulong	cr3;			/* page directory base register */
	ulong	eip;			/* instruction pointer */
	ulong	eflags;			/* flags register */
	ulong	eax;			/* general registers */
	ulong 	ecx;
	ulong	edx;
	ulong	ebx;
	ulong	esp;
	ulong	ebp;
	ulong	esi;
	ulong	edi;
	ulong	es;			/* segment selectors */
	ulong	cs;
	ulong	ss;
	ulong	ds;
	ulong	fs;
	ulong	gs;
	ulong	ldt;			/* selector for task's LDT */
	ulong	iomap;			/* I/O map base address + T-bit */
} Tss;

typedef struct Segdesc {
	ulong	d0;
	ulong	d1;
} Segdesc;

typedef struct Mach {
	int	machno;			/* physical id of processor */
	ulong	splpc;			/* pc of last caller to splhi */

	void*	pdb;			/* page directory base for this processor (va) */
	Tss*	tss;			/* tss for this processor */
	Segdesc	gdt[6];			/* gdt for this processor */

	ulong	ticks;			/* of the clock since boot time */
	void*	alarm;			/* alarms bound to this clock */
} Mach;

extern Mach *m;

#define I_MAGIC		((((4*11)+0)*11)+7)

typedef struct Exec Exec;
struct	Exec
{
	uchar	magic[4];		/* magic number */
	uchar	text[4];	 	/* size of text segment */
	uchar	data[4];	 	/* size of initialized data */
	uchar	bss[4];	  		/* size of uninitialized data */
	uchar	syms[4];	 	/* size of symbol table */
	uchar	entry[4];	 	/* entry point */
	uchar	spsz[4];		/* size of sp/pc offset table */
	uchar	pcsz[4];		/* size of pc/line number table */
};

/*
 *  bootline passed by boot program
 *  and where we leave configuration info.
 *  these are the old values:
#define BOOTLINE	((char *)0x80000100)
#define BOOTARGS	((char*)(KZERO|1024))
#define	BOOTARGSLEN	1024
 */
#define BOOTLINE	((char*)CONFADDR)
#define BOOTLINELEN	64
#define BOOTARGS	((char*)(CONFADDR+BOOTLINELEN))
#define	BOOTARGSLEN	(4096-0x200-BOOTLINELEN)
#define	MAXCONF		32

/*
 *  a parsed .ini line
 */
#define ISAOPTLEN	16
#define NISAOPT		8

typedef struct  ISAConf {
	char	type[NAMELEN];
	ulong	port;
	ulong	irq;
	ulong	mem;
	ulong	size;
	uchar	ea[6];

	int	nopt;
	char	opt[NISAOPT][ISAOPTLEN];
} ISAConf;

typedef struct Pcidev Pcidev;

typedef struct PCMmap PCMmap;

enum {
	MB =		(1024*1024),
};
#define ROUND(s, sz)	(((s)+((sz)-1))&~((sz)-1))

typedef struct Type Type;
typedef struct Medium Medium;

enum {
	Maxdev		= 7,
	Dany			= -1,
	Nmedia		= 16,
	Nini			= 10,
};

enum {					/* type */
	Tnone		= 0x00,

	Tfloppy		= 0x01,
	Thard		= 0x02,
	Tscsi			= 0x03,
	Tether		= 0x04,

	Tany		= -1,
};

enum {					/* flag and name */
	Fnone		= 0x00,

	Fdos		= 0x01,
	Ndos		= 0x00,
	Fbootp		= 0x02,
	Nbootp		= 0x01,
	NName		= 0x03,

	Fany		= Fbootp|Fdos,

	Fini		= 0x10,
	Fprobe		= 0x80,
};

typedef struct Type {
	int	type;
	int	flag;
	int	(*init)(void);
	long	(*read)(int, void*, long);
	long	(*seek)(int, long);
	int 	(*boot)( Medium *, char *);
	Partition* (*setpart)(int, char*);
	char*	name[NName];
	int	mask;
	Medium*	media;
} Type;

#include "dosfs.h"

typedef struct Medium {
	Type*	type;
	int	flag;
	Partition* partition;
	Dos;

	Medium*	next;
} Medium;

