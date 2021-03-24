/*
 * Styx file protocol definitions
 */

enum STYXtype
{
	Tnop,		/*  0 */
	Rnop,		/*  1 */
	Terror,		/*  2, illegal */
	Rerror,		/*  3 */
	Tflush,		/*  4 */
	Rflush,		/*  5 */
	Tclone,		/*  6 */
	Rclone,		/*  7 */
	Twalk,		/*  8 */
	Rwalk,		/*  9 */
	Topen,		/* 10 */
	Ropen,		/* 11 */
	Tcreate,	/* 12 */
	Rcreate,	/* 13 */
	Tread,		/* 14 */
	Rread,		/* 15 */
	Twrite,		/* 16 */
	Rwrite,		/* 17 */
	Tclunk,		/* 18 */
	Rclunk,		/* 19 */
	Tremove,	/* 20 */
	Rremove,	/* 21 */
	Tstat,		/* 22 */
	Rstat,		/* 23 */
	Twstat,		/* 24 */
	Rwstat,		/* 25 */
	Tsession,	/* 26 */
	Rsession,	/* 27 */
	Tattach,	/* 28 */
	Rattach,	/* 29 */
	Tmax
};

typedef	struct	Fcall	Fcall;
struct	Fcall
{
	enum STYXtype	type;
	short		fid;
	ushort		tag;
	ushort		oldtag;		/* T-Flush */
	Qid		qid;		/* R-Attach, R-Walk, R-Open, R-Create */
	char		uname[NAMELEN];	/* T-Attach */
	char		aname[NAMELEN];	/* T-Attach */
	char		ename[ERRLEN];	/* R-Error */
	long		perm;		/* T-Create */ 
	short		newfid;		/* T-Clone */
	char		name[NAMELEN];	/* T-Walk, T-Create */
	char		mode;		/* T-Create, T-Open */
	long		offset;		/* T-Read, T-Write */
	long		count;		/* T-Read, T-Write, R-Read */
	char*		data;		/* T-Write, R-Read */
	char		stat[DIRLEN];	/* T-Wstat, R-Stat */
};

#define MAXFDATA	8192
#define MAXMSG		128	/* max header sans data */
#define NOTAG		0xFFFF	/* Dummy tag */

int	convM2S(char*, Fcall*, int);
int	convS2M(Fcall*, char*);

int	convM2D(char*, Dir*);
int	convD2M(Dir*, char*);

int	fcallconv(va_list*, Fconv*);
int	dirconv(va_list*, Fconv*);

#pragma	varargck	type	"F"	Fcall*
