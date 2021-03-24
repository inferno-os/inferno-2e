typedef struct Global	Global;
typedef struct PortGlobal PortGlobal;
typedef struct Fid	Fid;
typedef struct Blocked	Blocked;

enum
{
	NFID =	20,
};

struct Fid
{
	int		fid;
	int		open;
	BPChan	*node;
};

struct Blocked
{
	uchar		type;
	short		fid;
	ushort		tag;
	Qid		qid;
	long		offset;
	long		count;
};

struct PortGlobal				// because we're in ROM
{
	ulong		nextqid;
	int		nfids;

	BPChan		*head;
	BPChan		*tail;

	Bpi		bp;
	Fcall		fc;

	Fid		fidpool[NFID];
	Blocked		blocked[32];
	uchar		buf[8192+16+512]; // 512 is extra safety padding
};

 // I/O
void	putc(int);
// void	puts(char *str);
// void	putn(ulong val, int radix);
// void	puthex(ulong val);
// void	putdec(ulong val);

 // debugging routines
void	dump(ulong *addr, int n);

 // protocol stuff
int	send(void *buf, int nbytes);
int	recv(void *buf, int nbytes);

// misc
int	segflush(void *addr, ulong len);

int	fcall(uchar*, Fcall*);
int	unblock(uchar*);

int	rootdirread(BPChan*, uchar *buf, long n, long offset);

void	rerror(uchar *buf, Fcall*, char *err, int elen);

ulong	glong(uchar *);
void	plong(uchar *, ulong);
void	pvlong(uchar *, ulong);

void	firstattach(int);
void	lastclunk(void);

