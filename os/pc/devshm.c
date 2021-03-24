/*
 *  Shared Memory device driver
 */
#include 	"u.h"
#include 	"../port/lib.h"
#include	"mem.h"
#include	"../port/error.h"
#include	"dat.h"
#include	"fns.h"
#include	<interp.h>


enum{
	Qdir,
	Qctl,
};

static
Dirtab shmtab[]={
	"shmctl",		{Qctl, 0},	0,	0600,
};

	/* Configurable parameters */
enum
{
	SHM_DevChar 			= 'x',
	SHM_DbgLvl 			= 1,			/* Default debug level */
	SHM_ShmDescTblSize	= 5,			/* Shared memory descriptor table size */
	SHM_ShmMinSize 		= 512,		/* Minimum size for shared mem */
	SHM_BUFSIZE			= 300		/* Size of buffer to return shmem details */
};

#define SHM_DevStr		"#x"

typedef struct ShmDescTbl ShmDescTbl;

struct ShmDescTbl
{
	int		status;
	char		name[28];
	ulong 		shmstart;
	int		shmsize;
};

static ShmDescTbl	shmdesctbl[SHM_ShmDescTblSize];
static ShmDescTbl *pshmdesctbl = &shmdesctbl[0];


enum
{
	SHM_StatusFree	= 1,	/* entry is unused */
	SHM_StatusBusy	= 2
};

	/* Debug related stuff */

/* #define dbprint	if (shmdbg) print */
#define dbnprint	if (1) print
#define sbprint		if (1) print
#define sbnprint	if (1) print
#define dbprint		if (1) print
#define ndbprint	if (0) print

int	shmdbg = SHM_DbgLvl;

void *allocshmem(int size);

static void
shminit(void)						
{
	int			i;
	ShmDescTbl	*psdt;

	ndbprint("shminit()\n");
	for (i = 0; i < SHM_ShmDescTblSize; i++)
	{
		psdt = pshmdesctbl + i;
		psdt->status = SHM_StatusFree;
		strcpy(psdt->name, "");
		psdt->shmstart = 0;
		psdt->shmsize = 0;
	}
}

static Chan*
shmattach(char* spec)
{
	ndbprint("shmattach()\n");
	return devattach(SHM_DevChar, spec);
}

static Chan*
shmclone(Chan* c, Chan* nc)				/* default in dev.c */
{
	ndbprint("shmclone()\n");
	return devclone(c, nc);
}

static int
shmwalk(Chan* c, char* name)
{
	ndbprint("shmwalk()\n");
	return devwalk(c, name, shmtab, nelem(shmtab), devgen);
}

static void
shmstat(Chan* c, char* db)
{
	ndbprint("shmstat()\n");
	devstat(c, db, shmtab, nelem(shmtab), devgen);
}

static Chan*
shmopen(Chan* c, int omode)
{
	ndbprint("shmopen()\n");
	return devopen(c, omode, shmtab, nelem(shmtab), devgen);
}

static void
shmcreate(Chan* c, char* name, int omode, ulong perm)	/* default in dev.c */
{
	ndbprint("shmcreate()\n");
	USED(c, name, omode, perm);
	error(Eperm);
}

static void
shmremove(Chan* c)					/* default in dev.c */
{
	ndbprint("shmremove()\n");
	USED(c);
	error(Eperm);
}

static void
shmwstat(Chan* c, char* dp)				/* default in dev.c */
{
	ndbprint("shmwstat()\n");
	USED(c, dp);
	error(Eperm);
}

static void
shmclose(Chan* c)
{
	ndbprint("shmclose()\n");
	USED(c);
}

static long
shmread(Chan* c, void* a, long n, ulong offset)
{
	int	i;
	char	buf[SHM_BUFSIZE];
	char	tmp[200];
	int	bufpos;
	ShmDescTbl	*psdt;

	ndbprint("shmread()\n");
	USED(offset);

	switch(c->qid.path & ~CHDIR){
	case Qdir:
		return devdirread(c, a, n, shmtab, nelem(shmtab), devgen);
	case Qctl:
		bufpos = 0;
		for (i = 0; i < SHM_ShmDescTblSize; i++)
		{
			psdt = pshmdesctbl + i;
			if (psdt->status == SHM_StatusFree) continue;
			sprint(buf+bufpos, "index %.2d name %.28s size %d start %lud end %lud \n", i, psdt->name,
										psdt->shmsize, psdt->shmstart, psdt->shmstart + psdt->shmsize);
			bufpos += strlen(tmp);
		}
		n = strlen(buf);
		strcpy(a, buf);
		break;
	default:
		n=0;
		break;
	}
	return n;
}

static Block*
shmbread(Chan* c, long n, ulong offset)			/* default in dev.c */
{
	ndbprint("shmbread()\n");
	return devbread(c, n, offset);
}

static long
shmwrite(Chan* c, char* a, long n, ulong offset)
{
	int 			nf;
	char 			*fields[5], buf[512];
	char 			*pshmem;
	int			size;
	int			type;
	ushort		len;
	ShmDescTbl	*psdt;
	int			i;
	int			m;

	ndbprint("shmwrite()\n");
	USED(offset);

	m = n;
	switch(c->qid.path & ~CHDIR) {
	default:
		error(Ebadusefd);
		break;
	case Qctl:
		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, a, n);
		buf[n] = '\0';

		nf = parsefields(buf, fields, 5, " ");
		if(strcmp(fields[0], "create") == 0){
			switch(nf) {
			default:
				error("bad args to create");
				break;
			case 3:
				for (i = 0; i < SHM_ShmDescTblSize; i++)
				{
					psdt = pshmdesctbl + i;
					if (psdt->status == SHM_StatusFree) break;
				}
				if (i == SHM_ShmDescTblSize) {
					error("shared memory desc tbl is full");
					break;
				}
				size = atoi(fields[2]);
				dbprint("size %d\n", size);
				if((size <= 0) || (size < SHM_ShmMinSize)) {
					error(Ebadarg);
					break;
				}
				pshmem = allocshmem(size);
				 	if (pshmem == nil) {
					error(Enomem);
					break;
				}
				psdt->status = 	SHM_StatusBusy;
				strcpy(psdt->name, fields[1]);
				psdt->shmstart = (ulong)pshmem;
				psdt->shmsize = size;
				dbprint("after allocshmem ptr 0x%lux size %d\n", pshmem, size);
				break;
			}
		}
		else if (strcmp(fields[0], "remove") == 0) {
			switch(nf){
			default:
				error("bad args to nuke");
				break;
			case 1:
				for (i = 0; i < SHM_ShmDescTblSize; i++)
				{
					psdt = pshmdesctbl + i;
					if (strcmp(psdt->name, fields[1]) == 0) break;
				}
				if (i == SHM_ShmDescTblSize) {
					error("name doesnt match the entry");
					poperror();
					return -1;
				}
				memset((void *)psdt->shmstart, 0, psdt->shmsize);
				free((void *)psdt->shmstart);
				strcpy(psdt->name, "");
				psdt->shmstart = 0;
				psdt->shmsize = 0;
				psdt->status = 	SHM_StatusFree;
				break;
			}
		}
		else if(strcmp(fields[0], "setdbg") == 0) {
			switch(nf){
			default:
				error("bad args to nuke");
				break;
			case 1:
				shmdbg = 1;
				break;
			case 2:
				shmdbg = atoi(fields[1]);
				break;
			}
		}
		else if(strcmp(fields[0], "clrdbg") == 0) {
			shmdbg = 0;
			break;
		}
		else {
			error("bad control message");
			break;
		}
	}
	/* poperror(); */
	return m;
}

static long
shmbwrite(Chan* c, Block* bp, ulong offset)
{
	ndbprint("shmbwrite()\n");
	return devbwrite(c, bp, offset);
}

void *
allocshmem(int size)
{
	void	*p;
	ulong   shmaddr;

	ndbprint("allocshmem()\n");
	p = mallocz(size, 0);
	if (p == nil) return p;
	/* check if it is word-aligned (32-bit) */
	shmaddr = (ulong) p;
	if (shmaddr == ((ulong) p & (ulong)~0x03))
	{
		return p;
	}
	free(p);
	dbprint("smi: error in shared memory allocation\n");
	return p;
}

Dev shmdevtab = {
	SHM_DevChar,
	"shm",

	devreset,
	shminit,
	shmattach,
	devdetach,
	shmclone,
	shmwalk,
	shmstat,
	shmopen,
	shmcreate,
	shmclose,
	shmread,
	shmbread,
	shmwrite,
	shmbwrite,
	shmremove,
	shmwstat,
};
