typedef struct Alarms	Alarms;
typedef struct Block	Block;
typedef struct Brk Brk;
typedef struct BrkCond BrkCond;
typedef struct Chan	Chan;
typedef struct Cmdbuf	Cmdbuf;
typedef struct Crypt	Crypt;
typedef struct Dev	Dev;
typedef struct Dirtab	Dirtab;
typedef struct Fgrp	Fgrp;
typedef struct List	List;
typedef struct Mntcache Mntcache;
typedef struct Mount	Mount;
typedef struct Mntrpc	Mntrpc;
typedef struct Mntwalk	Mntwalk;
typedef struct Mnt	Mnt;
typedef struct Mhead	Mhead;
typedef struct Osenv	Osenv;
typedef struct Path	Path;
typedef struct Pgrp	Pgrp;
typedef struct Pointer	Pointer;
typedef struct Pool	Pool;
typedef struct Proc	Proc;
typedef struct Pthash	Pthash;
typedef struct QLock	QLock;
typedef struct Queue	Queue;
typedef struct Ref	Ref;
typedef struct Rendez	Rendez;
typedef struct Rootdata	Rootdata;
typedef struct RWlock	RWlock;
typedef struct Talarm	Talarm;
typedef struct Waitq	Waitq;
typedef int    Devgen(Chan*, Dirtab*, int, int, Dir*);

#include <styx.h>
#include <pool.h>

#define nelem(n)	(sizeof(n)/sizeof(n[0]))

struct Ref
{
	Lock;
	long	ref;
};

struct Rendez
{
	Lock;
	Proc	*p;
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
	int	uid;		/* Numeric user id for system */
	int	gid;		/* Numeric group id for system */
	char	user[NAMELEN];	/* Inferno user name */
	FPenv	fpu;		/* Floating point thread state */
};

enum
{
	Nopin =	-1
};

struct QLock
{
	Lock	use;			/* to access Qlock structure */
	Proc	*head;			/* next process waiting for object */
	Proc	*tail;			/* last process waiting for object */
	int	locked;			/* flag */
	void*	owner;			/* debug */
};

struct Pointer
{
	int	x;
	int	y;
	int	b;
	int	modify;
	Rendez	r;
	Ref	ref;
	QLock	q;
};

struct RWlock
{
	Lock;				/* Lock modify lock */
	QLock	x;			/* Mutual exclusion lock */
	QLock	k;			/* Lock for waiting writers */
	int	readers;		/* Count of readers in lock */
};

struct Talarm
{
	Lock;
	Proc*	list;
};

struct Alarms
{
	QLock;
	Proc*	head;
};

struct Rootdata
{
	int	dotdot;
	void	*ptr;
	int	size;
	int	*sizep;
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
	CRCLOSE	= 0x0020,		/* remove on close */
};

struct Path
{
	Ref;
	Path*	hash;
	Path*	parent;
	Pthash*	pthash;
	char	elem[NAMELEN];
};

enum
{
	BINTR		=	(1<<0),
	BFREE		=	(1<<1),
	BMORE		=	(1<<2),		/* continued in next block */
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
#define BALLOC(s) ((s)->lim - (s)->base)

struct Chan
{
	Ref;
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
	Mntcache *mcp;			/* Mount cache pointer */
	union {
		void*	aux;
		Qid	pgrpid;		/* for #p/notepg */
		Mnt*	mntptr;		/* for devmnt */
		ulong	mid;		/* for ns in devproc */
		char	tag[4];		/* for iproute */
	};
	Chan*	mchan;			/* channel to mounted server */
	Qid	mqid;			/* qid of root of mount point */
};

struct Dev
{
	int	dc;
	char*	name;

	void	(*reset)(void);
	void	(*init)(void);
	Chan*	(*attach)(char*);
	void	(*detach)(void);
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
	NSCACHE	=	(1<<NSLOG),
};

struct Pthash
{
	QLock;
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
	Ref;			/* Count of attached channels */
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
	int	npart;		/* Partial read count */
	uchar	part[1];	/* Partial read buffer, MUST be last */
};

enum
{
	RENDHASH =	32,		/* Hash to lookup rendezvous tags */
	MNTHASH	=	32,		/* Hash to walk mount table */
	NFD =		100,		/* per process file descriptors */
};
#define REND(p,s)	((p)->rendhash[(s)%RENDHASH])
#define MOUNTH(p,s)	((p)->mnthash[(s)->qid.path%MNTHASH])

struct Pgrp
{
	Ref;				/* also used as a lock when mounting */
	ulong	pgrpid;
	QLock	debug;			/* single access via devproc.c */
	RWlock	ns;			/* Namespace n read/one write lock */
	QLock	nsh;
	Mhead*	mnthash[MNTHASH];
	int	progmode;
	Chan*	dot;
	Chan*	slash;
	int	nodevs;
	int	pin;
};

struct Fgrp
{
	Ref;
	Chan	*fd[NFD];
	int	maxfd;			/* highest fd in use */
};

enum
{
	Dead = 0,		/* Process states */
	Moribund,
	Ready,
	Scheding,
	Running,
	Queueing,
	Wakeme,
	Broken,
	Stopped,
	Rendezvous,

	Proc_stopme = 1, 	/* devproc requests */
	Proc_exitme,
	Proc_traceme,
	Proc_exitbig,

	NERR		= 30,

	Unknown		= 0,
	IdleGC,
	Interp,
	BusyGC,

	PriLock		= 0,	/* Holding Spin lock */
	PriRealtime,		/* Video telephony */
	PriHicodec,		/* MPEG codec */
	PriLocodec,		/* Audio codec */
	PriHi,			/* Important task */
	PriNormal,
	PriLo,
	PriBackground,
	Nrq
};

struct Proc
{
	Label		sched;		/* known to l.s */
	char*		kstack;		/* known to l.s */
	Mach*		mach;		/* machine running this proc */
	char		text[NAMELEN];
	Proc*		rnext;		/* next process in run queue */
	Proc*		qnext;		/* next process on queue for a QLock */
	QLock*		qlock;		/* addrof qlock being queued for DEBUG */
	int		state;
	int		type;
	void*		prog;		/* Dummy Prog for interp release */
	void*		iprog;
	Osenv*		env;
	Osenv		defenv;
	int		swipend;	/* software interrupt pending for Prog */
	Lock		sysio;		/* note handler lock */
	char*		psstate;	/* What /proc/#/status reports */
	ulong		pid;
	int		fpstate;
	int		procctl;	/* Control for /proc debugging */
	ulong		pc;		/* DEBUG only */
	Rendez*		r;		/* rendezvous point slept on */
	Rendez		sleep;		/* place for syssleep/debug */
	int		kp;		/* true if a kernel process */
	ulong		alarm;		/* Time of call */
	int		pri;		/* scheduler priority */
	ulong		twhen;
	Rendez*		trend;
	Proc*		tlink;
	int		(*tfn)(void*);
	void		(*kpfun)(void*);
	void*		arg;
	FPU		fpsave;
	int		scallnr;
	int		nerrlab;
	Label		errlab[NERR];
	char		elem[NAMELEN];	/* last name element from namec */
	Mach*		mp;		/* machine this process last ran on */
	void*		dbgreg;		/* User registers for devproc */
 	int		dbgstop;		/* don't run this kproc */
};

enum {
	BrkSched,
	BrkNoSched,
};

struct BrkCond
{
	uchar op;
	ulong val;
	BrkCond *next;
};

struct Brk
{
	int id;
	ulong addr;
	BrkCond *conditions;
	Instr instr;
	void (*handler)(Brk*);
	void *aux;

	Brk *next;

	Brk *link;
};

enum
{
	PRINTSIZE =	256,
	MAXCRYPT = 	127,
	NUMSIZE	=	12,		/* size of formatted number */
	MB =		(1024*1024),
	READSTR =	1000,		/* temporary buffer size for device reads */
};

extern	Conf	conf;
extern	char*	conffile;
extern	Dev*	devtab[];
extern  char	eve[NAMELEN];
extern	int	hwcurs;
extern	FPU	initfp;
extern  Queue	*kbdq;
extern  Queue	*kscanq;
extern  Ref	noteidalloc;
extern	int	nrdy;
extern  Queue	*printq;
extern	char*	statename[];
extern	char	sysname[NAMELEN];
extern	Pthash	syspt;
extern	Talarm	talarm;

enum
{
	CHDIR =		0x80000000L,
	CHAPPEND = 	0x40000000L,
	CHEXCL =	0x20000000L,
	CHMOUNT	=	0x10000000L,
};

/*
 * auth messages
 */
enum
{
	FScchal	= 1,
	FSschal,
	FSok,
	FSctick,
	FSstick,
	FSerr,

	RXschal	= 0,
	RXstick	= 1,

	AUTHLEN	= 8,
};

extern	int 	nsyscall;
extern	QLock	vmach;
extern	int	rdypri;
extern	Pointer	mouse;

enum
{
	MAXPOOL		= 8,
};

extern Pool*	mainmem;
extern Pool*	heapmem;
extern Pool*	imagmem;

/* Things scsi */
enum
{
	STnomem		=-4,		/* buffer allocation failed */
	STtimeout	=-3,		/* bus timeout */
	STownid		=-2,		/* playing with myself */
	STharderr	=-1,		/* controller error of some kind */
	STok		= 0,		/* good */
	STcheck		= 0x02,		/* check condition */
	STcondmet	= 0x04,		/* condition met/good */
	STbusy		= 0x08,		/* busy */
	STintok		= 0x10,		/* intermediate/good */
	STintcondmet	= 0x14,		/* intermediate/condition met/good */
	STresconf	= 0x18,		/* reservation conflict */
	STterminated	= 0x22,		/* command terminated */
	STqfull		= 0x28,		/* queue full */

	SCSIread	= 0,
	SCSIwrite,

	SCSImaxxfer	= 2048*1024,
};

struct Cmdbuf
{
	char	buf[128];
	char	*f[16];
	int	nf;
};
