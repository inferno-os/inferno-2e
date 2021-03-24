/* BootParam structure-- taken from the SOLO boot loader package
 * (ftp://ftp.csh.rit.edu/pub/csh/shaggy/shagware/solo-0.97.2.tar.gz)
 * and modified, with the author's permission.
 */

#define BootParam_ver_major	0
#define BootParam_ver_minor	4


/* important note:
 *	If any changes are made to BootParam that 
 *	alter the offsets of existing attributes,
 *	then backwards compatability with older
 *	kernels or older boot loaders is lost.
 *	(for existing compiled binaries only)
 *	Therefore, unless absolutely necessary, all changes
 *	should be made in a manner that does not rearrange any
 *	attributes or change their sizes.
 *
 *	Especially don't change anything up to, and including the boot loader
 *	version and name, because it may be critical to obtain this
 *	information, to determinate the compatability of the interface.
 *
 *	Reserved spaces allow for future expansion without
 *	changing the layout extensively.
 */


typedef struct BpStat BpStat;
struct BpStat {
	ulong	size;	/* total file size */
	ulong	flags;	/* various file-type flags */
};

enum {
	BPSTAT_TTY = 0x0010,	/* device is a TTY */
};


/* BootParam interface format v0.3: */
typedef struct BootParam BootParam;
struct BootParam {

	/* Generic, (mostly) portable interface: */

	uchar	bootver_major;	/* major number of boot loader version */
	uchar	bootver_minor;	/* minor number of boot loader version */
	uchar	bootparam_major;/* major number of bootparam version */
	uchar	bootparam_minor;/* minor number of bootparam version */
	char	*bootname;	/* name of boot loader (without the version) */
	int	argc;		/* argc for main() */
	char	**argv;		/* *argv[] for main() */
	char	**envp;		/* *envp[] for main() (currently unused) */
	ulong	flags;
	ulong	reserved2;
	ulong	reserved3;
	ulong	entry;		/* starting execution address */
	ulong	lomem;		/* lowest virtual address available */
	ulong	himem;		/* highest virtual address available + 1 */
	ulong	resv4;

	ulong	msgbuf;		/* kernel message buffer */
	ushort	msgsize;	/* size of message buffer */
	ushort	msgptr;		/* current pointer into message */
	ulong	resv5;
	ulong	resv6;

	ulong	resv7[14];

	int	console_fd;	/* file descriptor for console */
	int	resv8;		/* self_fd - not used */

	void	(*exit)(ulong code);		/* exit from kernel */
	void	(*reboot)(int cold);		/* warm/cold reboot */
	int	(*open)				/* open file and */
		(char *name, int oflag);	/* return 0 or fd */
	int	(*close)(int);			/* close a file */
	void	*resv9;				/* readdir */
	long	(*read)				/* read from a file/device */
		(int, void *adr, ulong len);
	long	(*write)			/* write to a file/device */
		(int, void *adr, ulong len);
	int	(*fstat)
		(int, BpStat *stbuf);
	int	(*seek)				/* set file position */
		(int, ulong ofs);
	void	*resv10;			/* tell */
	void	*resv11;
	void	*resv12;
	int	(*exec)				/* execute binary at addr */
		(void *adr, BootParam *, int flags);
	void	*resv11[3];


	/* StrongARM-specific Interface: */

	ulong	flashbase;	/* base address of flash memory */
	ulong	cpuspeed;	/* CPU speed, in HZ */
	ulong	pagetable;	/* page table virtual address */
};

enum {
	BP_FLAG_DEBUG = 0x0001,
	BP_FLAG_HARDWARE_RESET = 0x0010,
	BP_FLAG_SOFTWARE_RESET = 0x0020,
	BP_FLAG_WATCHDOG_RESET = 0x0040,
	BP_FLAG_SLEEPMODE_RESET = 0x0080,
};

enum {
	BP_EXEC_FLAG_SYSMODE = 0x0001,	/* exec in system/kernel mode */
};

enum {
	BP_O_RDONLY = 1,	/* rb */
	BP_O_RDWR = 3, 		/* r+b */
	BP_O_WRONLY = 5,	/* w+ */
};


extern BootParam *bootparam;

