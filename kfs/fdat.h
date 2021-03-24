/*
 * fundamental constants
 */
#define	ERRREC		64		/* size of a ascii erro message */
#define	DIRREC		116		/* size of a directory ascii record */
#define	NDBLOCK		6		/* number of direct blocks in Dentry */
#define NTLOCK		200		/* number of active file Tlocks */

/*#define	QPDIR	0x80000000L*/
#define	QPDIR	((unsigned long)0x80000000)
#define	QPNONE	0
#define	QPROOT	1
#define	QPSUPER	2

typedef	struct	Fbuf	Fbuf;
typedef	struct	Super1	Super1;
typedef	struct	Superb	Superb;
typedef	struct	Dentry	Dentry;
typedef	struct	Tag	Tag;

typedef	struct	FChan	FChan;
typedef struct	Command	Command;
typedef	struct	Fsconf	Fsconf;
typedef	struct	Cons	Cons;
typedef struct	Devcall	Devcall;

typedef struct	Device	Device;
typedef	struct	File	File;
typedef	struct	Filsys	Filsys;
typedef	struct	Filta	Filta;
typedef	struct	Filter	Filter;
typedef		ulong	Float;
typedef	struct	Hiob	Hiob;
typedef	struct	Iobuf	Iobuf;
typedef	struct	Tlock	Tlock;
typedef	struct	Uid	Uid;
typedef	struct	Wpath	Wpath;

/*
 * DONT TOUCH -- data structures stored on disk
 */

/* DONT TOUCH, this is the disk structure */
struct	Dentry
{
	char	name[NAMELEN];
	short	uid;
	short	gid;
	ushort	mode;
		#define	DALLOC	0x8000
		#define	DDIR	0x4000
		#define	DAPND	0x2000
		#define	DLOCK	0x1000
		#define	DREAD	0x4
		#define	DWRITE	0x2
		#define	DEXEC	0x1
	Qid	qid;
	long	size;
	long	dblock[NDBLOCK];
	long	iblock;
	long	diblock;
	long	atime;
	long	mtime;
};

/* DONT TOUCH, this is the disk structure */
struct	Tag
{
	short	pad;
	short	tag;
	long	path;
};

/* DONT TOUCH, this is the disk structure */
struct	Super1
{
	long	fstart;
	long	fsize;
	long	tfree;
	long	qidgen;		/* generator for unique ids */

	long	fsok;		/* file system ok */

	/*
	 * garbage for WWC device
	 */
	long	roraddr;	/* dump root addr */
	long	last;		/* last super block addr */
	long	next;		/* next super block addr */
};

/* DONT TOUCH, this is the disk structure */
struct	Fbuf
{
	long	nfree;
	long	free[1];		/* changes based on BUFSIZE */
};

/* DONT TOUCH, this is the disk structure */
struct	Superb
{
	Super1  sup;
	Fbuf	fbuf;
};

struct	Device
{
	char	type;
	char	ctrl;
	char	unit;
	char	part;
};

/*
 * for load stats
 */
struct	Filter
{
	ulong	count;			/* count and old count kept separate */
	ulong	oldcount;		/* so interrput can read them */
	Float	filter[3];		/* filters for 1m 10m 100m */ 
};

struct	Filta
{
	Filter*	f;
	int	scale;
};

/*
 * array of qids that are locked
 */
struct	Tlock
{
	Device	dev;
	long	time;
	long	qpath;
	File*	file;
};

struct	File
{
	QLock   qlock;
	Qid	qid;
	Wpath*	wpath;
	FChan*	cp;		/* null means a free slot */
	Tlock*	tlock;		/* if file is locked */
	File*	next;		/* in cp->flist */
	Filsys*	fs;
	long	addr;
	long	slot;
	long	lastra;		/* read ahead address */
	short	fid;
	short	uid;
	char	open;
		#define	FREAD	1
		#define	FWRITE	2
		#define	FREMOV	4
};

struct	Filsys
{
	char*	name;		/* name of filsys */
	char*	devname;	/* name of device */
	Device	dev;		/* device that filsys is on */
	int	flags;
		#define	FREAM		(1<<1)	/* mkfs */
		#define	FACTIVE		(1<<2)	/* in use; can't ream */
};

struct	Hiob
{
	Iobuf*	link;
	Lock	lock;
};

struct	Iobuf
{
	QLock	qlock;
	Device	dev;
	Iobuf	*next;		/* for hash */
	Iobuf	*fore;		/* for lru */
	Iobuf	*back;		/* for lru */
	char	*iobuf;		/* only active while locked */
	char	*xiobuf;	/* "real" buffer pointer */
	long	addr;
	int	flags;
};


struct	Uid
{
	short	uid;		/* user id */
	short	lead;		/* leader of group */
	short	offset;		/* byte offset in uidspace */
};

struct	Wpath
{
	Wpath	*prev;		/* pointer upwards in path */
	long	addr;		/* directory entry addr */
	long	slot;		/* directory entry slot */
	short	refs;		/* number of files using this structure */
};


/*
 * error codes generated from the file server
 */
enum
{
	Ebadspc = 1,
	Efid,
	Echar,
	Eopen,
	Ecount,
	Ealloc,
	Eqid,
	Eauth,
	Eaccess,
	Eentry,
	Emode,
	Edir1,
	Edir2,
	Ephase,
	Eexist,
	Edot,
	Eempty,
	Ebadu,
	Enotu,
	Enotg,
	Ename,
	Ewalk,
	Eronly,
	Efull,
	Eoffset,
	Elocked,
	Ebroken,

	MAXERR
};

/*
 * devnone block numbers
 */
enum
{
	Cuidbuf 	= 1
};

/*
 * tags on block
 */
enum
{
	Tnone		= 0,
	Tsuper,			/* the super block */
	Tdir,			/* directory contents */
	Tind1,			/* points to blocks */
	Tind2,			/* points to Tind1 */
	Tfile,			/* file contents */
	Tfree,			/* in free list */
	Tbuck,			/* cache fs bucket */
	Tvirgo,			/* fake worm virgin bits */
	Tcache,			/* cw cache things */
	MAXTAG
};

/*
 * flags to getbuf
 */
enum
{
	Bread	= (1<<0),	/* read the block if miss */
	Bprobe	= (1<<1),	/* return null if miss */
	Bmod	= (1<<2),	/* set modified bit in buffer */
	Bimm	= (1<<3),	/* set immediate bit in buffer */
	Bres	= (1<<4)	/* reserved, never renamed */
};

/*
 * open modes passed into P9 open/create
 */
enum
{
	MREAD	= 0,
	MWRITE,
	MBOTH,
	MEXEC,
	MTRUNC	= (1<<4),	/* truncate on open */
	MCEXEC	= (1<<5),	/* close on exec (host) */
	MRCLOSE	= (1<<6)	/* remove on close */
};

/*
 * check flags
 */
enum
{
	Crdall	= (1<<0),	/* read all files */
	Ctag	= (1<<1),	/* rebuild tags */
	Cpfile	= (1<<2),	/* print files */
	Cpdir	= (1<<3),	/* print directories */
	Cfree	= (1<<4),	/* rebuild free list */
	Cream	= (1<<6),	/* clear all bad tags */
	Cbad	= (1<<7),	/* clear all bad blocks */
	Ctouch	= (1<<8),	/* touch old dir and indir */
	Cquiet	= (1<<9)	/* report just nasty things */
};

/*
 * buffer size variables
 *
 * BUG: should probably be in Filsys
 */
extern int	RBUFSIZE;
extern int	BUFSIZE;
extern int	DIRPERBUF;
extern int	INDPERBUF;
extern int	INDPERBUF2;
extern int	FEPERBUF;

struct	FChan
{
	int	type;
	int	chan;			/* fd request came in on */
	char	whoname[NAMELEN];
	int	flags;
	long	whotime;
	File*	flist;			/* base of file structures */
	Lock	flock;			/* manipulate flist */
	RWlock	reflock;		/* lock for Tflush */
};

/*
 * console cons.flag flags
 */
enum
{
	Fchat	= (1<<0),	/* print out filsys rpc traffic */
	Fuid	= (1<<2)	/* print out uids */
				/* debugging flags for drivers */
};

struct	Cons
{
	QLock   qlock;		/* for cmd_exec */
	int	flags;		/* overall flags for all channels */
	int	uid;		/* botch -- used to get uid on cons_create */
	int	gid;		/* botch -- used to get gid on cons_create */
	int	allow;		/* no-protection flag */
	long	offset;		/* used to read files, c.f. fchar */
	char*	arg;		/* pointer to remaining line */

	FChan*	chan;
	Queue*	out;	/* output from console commands */
	int	opened;	/* console in use */

	Filter	work;		/* thruput in messages */
	Filter	rate;		/* thruput in bytes */
	Filter	bhit;		/* getbufs that hit */
	Filter	bread;		/* getbufs that miss and read */
	Filter	binit;		/* getbufs that miss and dont read */
	Filter	tags[MAXTAG];	/* reads of each type of block */
};

struct	Fsconf
{
	ulong	niobuf;		/* number of iobufs to allocate */
	ulong	nuid;		/* distinct uids */
	ulong	uidspace;	/* space for uid names -- derived from nuid */
	ulong	gidspace;	/* space for gid names -- derved from nuid */
	ulong	nserve;		/* server processes */
	ulong	nfile;		/* number of fid -- system wide */
	ulong	nwpath;		/* number of active paths, derrived from nfile */
	ulong	bootsize;	/* number of bytes reserved for booting */
};

struct	Command
{
	char	*string;
	void	(*func)(void);
	char	*args;
};

struct Devcall
{
	char*	(*init)(Device, char*, int);
	void	(*ream)(Device);
	char*	(*check)(Device);
	long	(*super)(Device);
	long	(*root)(Device);
	long	(*size)(Device);
	int	(*read)(Device, long, void*);
	int	(*write)(Device, long, void*);
};

/*
 * device types
 */
enum
{
	Devnone 	= 0,
	Devwren,
	MAXDEV
};

/*
 * file systems
 */
enum
{
	MAXFILSYS = 4
};

/*
 * perm argument in p9 create
 */

#define	PDIR	((unsigned long)(1<<31))	/* is a directory */
#define	PAPND	((unsigned long)(1<<30))	/* is append only */
#define	PLOCK	((unsigned long)(1<<29))	/* is locked on open */

#define	NOF	(-1)

#define	FID1		1
#define	FID2		2

#define SECOND(n) 	(n)
#define MINUTE(n)	(n*SECOND(60))
#define HOUR(n)		(n*MINUTE(60))
#define DAY(n)		(n*HOUR(24))
#define	TLOCK		MINUTE(5)

#define	CHAT(cp)	(kfschat)
#define	QID(a,b)	(Qid){a,b}

extern	Uid*	uid;
extern	char*	uidspace;
extern	short*	gidspace;
extern	File*	files;
extern	Wpath*	wpaths;
extern	Lock	wpathlock;
extern	char*	fserrstr[MAXERR];
extern	FChan*	chans;
extern	RWlock	mainlock;
extern	Lock	newfplock;
extern	long	boottime;
extern	Tlock	*tlocks;
extern	Device	devnone;
extern	Filsys	filesys[];
extern	char	service[];
extern	char*	tagnames[];
extern	Fsconf	fsconf;
extern	Cons	fscons;
extern	Command	command[];
extern	FChan*	chan;
extern	Devcall	devcall[];
extern	long	niob;
extern	long	nhiob;
extern	Hiob*	hiob;
extern	int	kfschat;
extern	int	writeallow;
extern	int	wstatallow;
