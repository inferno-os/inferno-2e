#include <sys/types.h>
#define _POSIX_SOURCE
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include "math.h"
#include <fcntl.h>
#include <setjmp.h>
#include <float.h>
#include <sys/bsd_types.h>

#define FLT_ROUNDS 1
#define Storeinc(a,b,c) (*a++ = b << 16 | c & 0xffff)
#define	setbinmode()

#define	nil		((void*)0)

extern	int	msize(void*);
extern	void*	mallocz(int, int);

typedef unsigned char	uchar;
typedef signed char	schar;
typedef unsigned short	Rune;
typedef long long int	vlong;
typedef	unsigned long long int	uvlong;

#define	USED(x)		if(x);else
#define	SET(x)

#define nelem(x)	(sizeof(x)/sizeof((x)[0]))

enum
{
	UTFmax		= 3,		/* maximum bytes per rune */
	Runesync	= 0x80,		/* cannot represent part of a UTF sequence (<) */
	Runeself	= 0x80,		/* rune and UTF sequences are the same (<) */
	Runeerror	= 0x80		/* decoding error in UTF */
};

/*
 * new rune routines
 */
extern	int	runetochar(char*, Rune*);
extern	int	chartorune(Rune*, char*);
extern	int	runelen(long);
extern	int	fullrune(char*, int);
extern	int	runestrlen(Rune*);
extern	int	runenlen(Rune*, int);

/*
 * rune routines from converted str routines
 */
extern	long	utflen(char*);
extern	char*	utfrune(char*, long);
extern	char*	utfrrune(char*, long);

enum
{
	NAMELEN	= 28,
	ERRLEN	= 64,
	DIRLEN	= 116
};
#define CHDIR		0x80000000	/* mode bit for directories */
#define CHAPPEND	0x40000000	/* mode bit for append only files */
#define CHEXCL		0x20000000	/* mode bit for exclusive use files */
#define CHMOUNT		0x10000000	/* mode bit for mounted channel */
#define CHREAD		0x4		/* mode bit for read permission */
#define CHWRITE		0x2		/* mode bit for write permission */
#define CHEXEC		0x1		/* mode bit for execute permission */

#define	MORDER		0x0003		/* mask for bits defining order of mounting */
#define	MREPL		0x0000		/* mount replaces object */
#define	MBEFORE		0x0001		/* mount goes before others in union directory */
#define	MAFTER		0x0002		/* mount goes after others in union directory */
#define	MCREATE		0x0004		/* permit creation in mounted directory */
#define	MMASK		0x0007		/* all bits on */

#define	OREAD		0		/* open for read */
#define	OWRITE		1		/* write */
#define	ORDWR		2		/* read and write */
#define	OEXEC		3		/* execute, == read but check execute permission */
#define	OTRUNC		16		/* or'ed in (except for exec), truncate file first */
#define	OCEXEC		32		/* or'ed in, close on exec */
#define	ORCLOSE		64		/* or'ed in, remove on close */

typedef
struct Qid
{
	ulong	path;
	ulong	vers;
} Qid;

typedef
struct Dir
{
	char	name[NAMELEN];
	char	uid[NAMELEN];
	char	gid[NAMELEN];
	Qid	qid;
	ulong	mode;
	int	atime;
	int	mtime;
	ulong	length;
	ushort	type;
	ushort	dev;
} Dir;

int	dirfstat(int, Dir*);
int	dirstat(char*, Dir*);
int	dirfwstat(int, Dir*);
int	dirwstat(char*, Dir*);
void	exits(const char*);
void	_exits(const char*);
int	create(char*, int, int);

char*	getuser(void);
int	getfields(char*, char**, int);
long	seek(int, long, int);
char*	setfields(char*);

typedef struct Tm Tm;
struct Tm {
	int	sec;
	int	min;
	int	hour;
	int	mday;
	int	mon;
	int	year;
	int	wday;
	int	yday;
	char	zone[4];
};	

/*
 * print routines
 */
typedef	struct	Fconv	Fconv;
struct	Fconv
{
	char*	out;		/* pointer to next output */
	char*	eout;		/* pointer to end */
	int	f1;
	int	f2;
	int	f3;
	int	chr;
};
extern	char*	doprint(char*, char*, char*, va_list);
extern	int	print(char*, ...);
extern	int	snprint(char*, int, char*, ...);
extern	int	sprint(char*, char*, ...);
extern	int	fprint(int, char*, ...);
extern	char*	strdup(const char*);
extern	vlong	strtoll(const char*, char**, int);

extern	int	fmtinstall(int, int (*)(va_list*, Fconv*));
extern	int	numbconv(va_list*, Fconv*);
extern	void	strconv(char*, Fconv*);
extern	int	fltconv(va_list*, Fconv*);

extern	int	isNaN(double);
extern	int	isInf(double, int);

enum
{
	ICOSSCALE = 1024
};

extern	double	charstod(int(*)(void*), void*);
extern	double	pow10(int);
extern	double	ipow10(int);
extern	void	icossin(int, int*, int*);
extern	void	icossin2(int, int, int*, int*);

extern	ulong	getcallerpc(void*);

extern	int	errstr(char*);
extern	void	werrstr(char*, ...);

#define	ARGBEGIN	for(argv++,argc--;\
			    argv[0] && argv[0][0]=='-' && argv[0][1];\
			    argc--, argv++) {\
				char *_args, *_argt;\
				char _argc;\
				_args = &argv[0][1];\
				if(_args[0]=='-' && _args[1]==0){\
					argc--; argv++; break;\
				}\
				_argc = 0;\
				while((_argc = *_args++)!='\0')\
				switch(_argc)
#define	ARGEND		}
#define	ARGF()		(_argt=_args, _args="",\
				(*_argt? _argt: argv[1]? (argc--, *++argv): 0))
#define	ARGC()		_argc


/*
 *	Extensions for emu kernel emulation
 */
#ifdef	EMU

extern	Proc**	Xup;
#define	up	(*Xup)

typedef	struct	FPU	FPU;

/*
 * This structure must agree with FPsave and FPrestore asm routines
 */
struct FPU
{
	ulong	fcr31;
};

#include <dirent.h>
#define DIRTYPE	struct dirent

#define	BIGEND

typedef sigjmp_buf osjmpbuf;
#define	ossetjmp(buf)	sigsetjmp(buf, 1)

int	logopen(char *);
void	logmsg(int, char *);
void	logclose(void);

#endif
