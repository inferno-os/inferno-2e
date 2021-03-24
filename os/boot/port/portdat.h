typedef struct Block Block;
typedef struct Queue Queue;
typedef struct Istream Istream;
typedef struct Ostream Ostream;
typedef struct StreamDev StreamDev;
typedef struct Lock Lock;

/* since const is banned in our code, we'll make it go away
 * for now, but in a way that doesn't make us redo all
 * the work if that decision changes later 
 */
#define const

struct Block
{
	int nbytes;
	char *data;
	char *rp;

	Block *link;
};

struct Queue
{
	Block *head;
	Block *tail;
};

struct Istream {
	int 	(*read)(Istream *, void *, int);
	int	(*close)(Istream *);
	uchar*	(*mmap)(Istream *);
	ulong	pos;
	ulong	size;
};

struct Ostream {
	int 	(*write)(Ostream *, const void *, int);
	int	(*close)(Ostream *);
	uchar*	(*mmap)(Istream *);
	ulong	pos;
	ulong	size;
};

struct StreamDev {
	char 	*name;
	Istream* (*openi)(char*);
	Ostream* (*openo)(char*);
	StreamDev *next;
};

struct Lock {
	ulong	empty;
};

extern int radix;

extern Istream *stdin;
extern Ostream *stdout;

extern Istream *conin;
extern Istream *rconin;
extern Ostream *conout;
extern Istream *dbgin;
extern Ostream *dbgout;

extern const char **environ;

extern char lasterr[];
extern char gzip_name[];	// a bit of a hack

extern Conf conf;

