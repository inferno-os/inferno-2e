typedef struct Block	Block;
typedef struct Chan	Chan;
typedef struct Dev	Dev;
typedef struct Dirtab	Dirtab;
typedef struct Fgrp	Fgrp;
typedef struct Lock	Lock;
typedef struct Mount	Mount;
typedef struct Mntrpc	Mntrpc;
typedef struct Mntwalk	Mntwalk;
typedef struct Mnt	Mnt;
typedef struct Mhead	Mhead;
typedef struct Osenv	Osenv;
typedef struct Path	Path;
typedef struct Pgrp	Pgrp;
typedef struct Proc	Proc;
typedef struct Pthash	Pthash;
typedef struct QLock	QLock;
typedef struct Queue	Queue;
typedef struct Ref	Ref;
typedef struct Rendez	Rendez;
typedef struct RWlock	RWlock;
typedef	struct Ufsinfo	Ufsinfo;
typedef struct Pointer	Pointer;
typedef struct Procs	Procs;
typedef struct Rb	Rb;

#include "lib9.h"
#include "pool.h"

typedef int    Devgen(Chan*, Dirtab*, int, int, Dir*);

enum
{
	MAXROOT		= 5*NAMELEN, 	/* Maximum root pathname len of devfs-* */
	NUMSIZE		= 11,
	MAXDEVCMD 	= 128,		/* Maximum size for command */
	PRINTSIZE	= 256
};


struct Lock
{
	int	key;
	int	pid;
};

struct Ref
{
	Lock	l;
	long	ref;
};

struct Rendez
{
	Lock	l;
	Proc*	p;
};

struct QLock
{
	Lock	use;			/* to access Qlock structure */
	Proc	*head;			/* next process waiting for object */
	Proc	*tail;			/* last process waiting for object */
	int	locked;			/* flag */
	void	*owner;			/* unused by qlock */
};

struct RWlock
{
	Lock	l;			/* Lock modify lock */
	QLock	x;			/* Mutual exclusion lock */
	QLock	k;			/* Lock for waiting writers */
	int	readers;		/* Count of readers in lock */
};

/*
 * Access types in namec & channel flags
 */
enum
{
	Aaccess,			/* as in access, stat */
	Atodir,				/* as in chdir */
	Aopen,				/* for i/o */
	Amount,				/* to be mounted upon */
	Acreate,			/* file is to be created */

	COPEN	= 0x0001,		/* for i/o */
	CMSG	= 0x0002,		/* the message channel for a mount */
	CCREATE	= 0x0004,		/* permits creation if c->mnt */
	CCEXEC	= 0x0008,		/* close on exec */
	CFREE	= 0x0010,		/* not in use */
	CRCLOSE	= 0x0020		/* remove on close */
};

struct Path
{
	Ref	r;
	Path*	hash;
	Path*	parent;
	Pthash*	pthash;
	char	elem[NAMELEN];
};

struct Ufsinfo
{
	int	uid;
	int	gid;
	int	mode;
	int	fd;
	DIR*	dir;
	ulong	offset;
	int	stream;
	QLock	oq;
};

struct Chan
{
	Ref	r;
	Chan*	next;			/* allocation */
	Chan*	link;
	ulong	offset;			/* in file */
	ushort	type;
	ulong	dev;
	ushort	mode;			/* read/write */
	ushort	flag;
	Qid	qid;
	int	fid;			/* for devmnt */
	Path*	path;
	Mount*	mnt;			/* mount point that derived Chan */
	Mount*	xmnt;			/* Last mount point crossed */
	ulong	mountid;
	union {
		void*	aux;
		Mnt*	mntptr;		/* for devmnt */
		Ufsinfo	uif;
	} u;
	Chan*	mchan;			/* channel to mounted server */
	Qid	mqid;			/* qid of root of mount point */
};

struct Dev
{
	int	dc;
	char*	name;

	void	(*init)(void);
	Chan*	(*attach)(void*);
	Chan*	(*clone)(Chan*, Chan*);
	int	(*walk)(Chan*, char*);
	void	(*stat)(Chan*, char*);
	Chan*	(*open)(Chan*, int);
	void	(*create)(Chan*, char*, int, ulong);
	void	(*close)(Chan*);
	long	(*read)(Chan*, void*, long, ulong);
	Block*	(*bread)(Chan*, long, ulong);
	long	(*write)(Chan*, void*, long, ulong);
	long	(*bwrite)(Chan*, Block*, ulong);
	void	(*remove)(Chan*);
	void	(*wstat)(Chan*, char*);
};

enum
{
	BINTR		=	(1<<0),
	BFREE		=	(1<<1),
	BMORE		=	(1<<2)		/* continued in next block */
};

struct Block
{
	Block*	next;
	Block*	list;
	uchar*	rp;			/* first unconsumed byte */
	uchar*	wp;			/* first empty byte */
	uchar*	lim;			/* 1 past the end of the buffer */
	uchar*	base;			/* start of the buffer */
	void	(*free)(Block*);
	ulong	flag;
};
#define BLEN(s)	((s)->wp - (s)->rp)

struct Dirtab
{
	char	name[NAMELEN];
	Qid	qid;
	long	length;
	long	perm;
};

enum
{
	NSMAX	=	1000,
	NSLOG	=	7,
	NSCACHE	=	(1<<NSLOG)
};

struct Pthash
{
	QLock	ql;
	int	npt;
	Path*	root;
	Path*	hash[NSCACHE];
};

struct Mntwalk				/* state for /proc/#/ns */
{
	ulong	id;
	Mhead*	mh;
	Mount*	cm;
};

struct Mount
{
	ulong	mountid;
	Mount*	next;
	Mhead*	head;
	Mount*	copy;
	Mount*	order;
	Chan*	to;			/* channel replacing channel */
	int	flag;
	char	spec[NAMELEN];
};

struct Mhead
{
	Chan*	from;			/* channel mounted upon */
	Mount*	mount;			/* what's mounted upon it */
	Mhead*	hash;			/* Hash chain */
};

struct Mnt
{
	Ref	r;		/* Count of attached channels */
	Chan*	c;		/* Channel to file service */
	Proc*	rip;		/* Reader in progress */
	Mntrpc*	queue;		/* Queue of pending requests on this channel */
	ulong	id;		/* Multiplexor id for channel check */
	Mnt*	list;		/* Free list */
	int	flags;		/* recover/cache */
	char	recprog;	/* Recovery in progress */
	int	blocksize;	/* read/write block size */
	ushort	flushtag;	/* Tag to send flush on */
	ushort	flushbase;	/* Base tag of flush window for this buffer */
	Pthash	tree;		/* Path names from this mount point */
	int	npart;		/* Partial buffer count */
	uchar	part[1];	/* Partial buffer MUST be last */
};

enum
{
	MNTHASH	=	32,		/* Hash to walk mount table */
	NFD =		100		/* per process file descriptors */
};
#define MOUNTH(p,s)	((p)->mnthash[(s)->qid.path%MNTHASH])

struct Pgrp
{
	Ref	r;			/* also used as a lock when mounting */
	ulong	pgrpid;
	RWlock	ns;			/* Namespace n read/one write lock */
	QLock	nsh;
	Mhead*	mnthash[MNTHASH];
	int	progmode;
	Chan*	dot;
	Chan*	slash;
	int	nodevs;
	int	pin;
};

enum
{
	Nopin =	-1
};

struct Fgrp
{
	Ref	r;
	Chan*	fd[NFD];
	int	maxfd;			/* highest fd in use */
};

struct Pointer
{
	int	x;
	int	y;
	int	b;
	int	modify;
	Rendez	r;
};

struct Osenv
{
	char	error[ERRLEN];	/* Last system error */
	Pgrp*	pgrp;		/* Ref to namespace, working dir and root */
	Fgrp*	fgrp;		/* Ref to file descriptors */
	Rendez*	rend;		/* Synchro point */
	Queue*	waitq;		/* Info about dead children */
	Queue*	childq;		/* Info about children for debuggers */
	void*	debug;		/* Debugging master */
	char	user[NAMELEN];	/* Inferno user name */
	FPU	fpu;		/* Floating point thread state */
	int	uid;		/* Numeric user id for host system */
	int	gid;		/* Numeric group id for host system */
	void	*ui;		/* User info for NT */
};

enum
{
	Unknown = 0xdeadbabe,
	IdleGC	= 0x16,
	Interp	= 0x17
};

struct Proc
{
	int	type;		/* interpreter or not */
	char	text[NAMELEN];
	char	elem[NAMELEN];	/* temporary path component for namec */
	Proc*	qnext;		/* list of processes waiting on a Qlock */
	int	pid;
	Proc*	next;		/* list of created processes */
	Proc*	prev;
	Rendez*	r;		/* rendezvous we are Sleeping on */
	Rendez	sleep;		/* place to sleep */
	int	swipend;	/* software interrupt pending for Prog */
	int	syscall;	/* set true under sysio for interruptable syscalls */
	int	intwait;	/* spin wait for note to turn up */
	int	sigid;		/* handle used for signal/note/exception */
	Lock	sysio;		/* note handler lock */
	int	nerr;		/* error stack SP */
	char*	kstack;
	osjmpbuf	estack[32];	/* vector of error jump labels */
	void	(*func)(void*);	/* saved trampoline pointer for kproc */
	void*	arg;		/* arg for invoked kproc function */
	void*	iprog;		/* work for Prog after release */
	void*	prog;		/* fake prog for slaves eg. exportfs */
	Osenv*	env;		/* effective operating system environment */
	Osenv	defenv;		/* default env for slaves with no prog */
	osjmpbuf	privstack;	/* private stack for making new kids */
	osjmpbuf	sharestack;
	Proc	*kid;
	void	*kidsp;
	DIRTYPE	dir;
};

#define poperror()	up->nerr--
#define	waserror()	(up->nerr++, ossetjmp(up->estack[up->nerr-1]))

struct Procs
{
	Lock	l;
	Proc*	head;
	Proc*	tail;
};

struct Rb
{
	QLock	l;
	Rendez	producer;
	Rendez	consumer;
	Rendez	clock;
	ulong	randomcount;
	uchar	buf[4096];
	uchar	*ep;
	uchar	*rp;
	uchar	*wp;
	uchar	next;
	uchar	bits;
	uchar	wakeme;
	uchar	filled;
	int	target;
	int	kprocstarted;
};

extern	Dev*	devtab[];
extern	char	ossysname[3*NAMELEN];
extern	char	eve[NAMELEN];
extern	Pthash	syspt;
extern	Queue*	kbdq;
extern	Queue*	gkbdq;
extern	Queue*	gkscanq;
extern	Queue*	ptrq;
extern	int	Xsize;
extern	int	Ysize;
extern	Pool*	mainmem;
extern	char	rootdir[MAXROOT];
extern	Procs	procs;
extern	int	sflag;
extern	int	xtblbit;

/*
 * floating point control and status register masks
 */
enum
{
	INVAL		= 0x0001,
	ZDIV		= 0x0002,
	OVFL		= 0x0004,
	UNFL		= 0x0008,
	INEX		= 0x0010,
	RND_NR		= 0x0000,
	RND_NINF	= 0x0100,
	RND_PINF	= 0x0200,
	RND_Z		= 0x0300,
	RND_MASK	= 0x0300
};

