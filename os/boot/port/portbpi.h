/* BPI (BootParam Interface) -- taken from the SOLO boot loader package
 * (ftp://ftp.csh.rit.edu/pub/csh/shaggy/shagware/solo-0.97.2.tar.gz)
 * (http://www.csh.rit.edu/~shaggy/software.html)
 * and slightly modified for Inferno, with the author's permission.
 */

#define Bpi_ver_major	1
#define Bpi_ver_minor	0


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
 *	changing the layout extensively.  Note that the portable
 *	portion is already padded out to be an exact 192 bytes,
 *	so any changes in the portable area must used reserved parts
 *	so that the offset of the platform-specific area doesn't change.
 *	It's probably also a good idea to pad out the machine-specific
 *	portion, so that unused fields are currently 0'd to make
 *	it easier to test for their presence for future expansion.
 *
 *	Platform-specific extensions belong in (platform)/bpi.h
 */


typedef struct BpStat BpStat;
typedef struct BPChan BPChan;
typedef struct BPDirtab BPDirtab;

struct BpStat
{
	ulong	size;	/* total file size */
	ulong	flags;	/* various file-type flags */
};

enum {
	BPSTAT_TTY = 0x0010,	/* device is a TTY */
	BPCHAN_BLOCKED = -2,	/* BPChan->read/write can return this */
};

struct BPChan
{
	Dir d;
	int	(*open)(BPChan*, int mode);	/* OREAD, OWRITE, etc */
	void	(*clunk)(BPChan*);	
	int	(*read)(BPChan*, uchar *buf, long count, long offset);
	int	(*write)(BPChan*, uchar *buf, long count, long offset);
	void *aux;

	char *err;		/* points to error string if an error occurs */
	BPChan *link;
};

typedef struct Bpi Bpi;

typedef struct PortBpi PortBpi;
struct PortBpi {
	uchar	bootver_major;	/* major number of boot loader version */
	uchar	bootver_minor;	/* minor number of boot loader version */
	uchar	bpi_major;	/* major number of BPI version */
	uchar	bpi_minor;	/* minor number of BPI version */
	char	*bootname;	/* name of boot loader (without the version) */
	int	argc;		/* argc for main() */
	char	**argv;		/* *argv[] for main() */
	char	**envp;		/* *envp[] for main() (currently unused) */
	ulong	flags;
	ulong	_;
	ulong	_;
	ulong	entry;		/* starting execution address */
	ulong	lomem;		/* lowest virtual address available */
	ulong	himem;		/* highest virtual address available + 1 */
	ulong	_;

	ulong	msgbuf;		/* circular message buffer (for logging) */
	ushort	msgsize;	/* size of message buffer */
	ushort	msgptr;		/* current pointer into message */
	ulong	_;
	ulong	_;

	ulong	_[14];

	int	console_fd;	/* file descriptor for console */
	int	_;		/* was self_fd - not used now */

	void	(*exit)(ulong code);		/* exit from kernel */
	void	(*reboot)(int cold);		/* warm/cold reboot */
	int	(*open)				/* open file and */
		(const char *name, int oflag);	/* return 0 or fd */
	int	(*close)(int);			/* close a file */
	void	*_;				/* readdir */
	long	(*read)				/* read from a file/device */
		(int, void *adr, ulong len);
	long	(*write)			/* write to a file/device */
		(int, const void *adr, ulong len);
	int	(*fstat)
		(int, BpStat *stbuf);
	int	(*seek)				/* set file position */
		(int, ulong ofs);
	void	(*file2chan)(BPChan*);		/* register a file callback */
	int	(*poll)(int ms);		/* wait for events on file2chans. */
	void	*_;
	int	(*exec)				/* execute binary at addr */
		(void *adr, Bpi*, int flags);
	void	*_[3];
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


