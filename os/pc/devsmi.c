
/* 	Shared Memory Interface (SMI) driver
	dharani@bell-labs.com
	Feb 4, 99
	port to x86 kernel by peter wang
	Note: Revision of code required. Debugging stuff and lot of 'print's may be removed in final code
*/
#include	"u.h"
#include 	"../port/lib.h"
#include 	"mem.h"
#include 	"../port/error.h"
#include	"dat.h"
#include	"fns.h"
#include	<interp.h>

#define NETTYPE(x)	((x)&0x1f)
#define NETID(x)		(((x)&~CHDIR)>>5)
#define NETQID(i,t)		(((i)<<5)|(t))

#define PTR(baseptr, offset)	((uchar *)baseptr + offset)


typedef struct Smi		Smi;
typedef struct SmiDesc	SmiDesc;

struct Smi
{
	Lock		l;
	Smi*		next;
	int		ref;
	int		qref;
	ulong	path;

	uint		usertype;		/* Participant type: Primary or Secondary */
	uint		shmtype;		/* shared memory type Internal (allocated by SMI) or External */

	uint		size;			/* size of shared memory */
	uchar	*pstart;		/* Start of shared memory: must be word(32-bit)-aligned */
	SmiDesc		*psd;		/* ptr to SMI Descriptor */

	int		pollinterval;	/* Poll interval in msecs */
	int		txfull;		/* Tx Full ? */
	int		rxempty;		/* Rx Empty ? */
	int		windup;		/* when set, pollp knows it has to exit */

	Lock		txl;
	uint		txsize;		/* Tx size */
	uint		txstart;		/* Tx start */
	uint		txend;		/* Tx end */
	uint		*ptxrp;		/* points to where Tx read ptr is in shared memory */
	uint		*ptxwp;		/* points to where Tx write ptr is in shared memory */

	Lock		rxl;
	uint		rxsize;		/* Rx size */
	uint		rxstart;		/* Rx start */
	uint		rxend;		/* Rx end */
	uint		*prxrp;		/* points to where Rx read ptr is in shared memory */
	uint		*prxwp;		/* points to where Rx write ptr is in shared memory */

	Rendez	vouzread;		/* Read proc rendez */
	Rendez	vouzwrite;	/* Write proc rendez */
	Rendez	vouzpoll;		/* Poll proc rendez */
	Rendez	vouzpollpexit;	/* Poll proc rendez */
};

	/* Note: All the values in the foll. structure must be written in LSB format */
	/* Note: All pointers are in offset values relative to the start of shared memory */
struct SmiDesc
{
	uint	signature;		/* signature */
	uint	status;		/* status */
	uint	size;			/* size */
	uint	txrp;			/* Tx read ptr */	
	uint	txwp;		/* Tx write ptr */
	uint	rxrp;			/* Rx read ptr */
	uint	rxwp;		/* Rx write ptr */	
	uint	rsvd;			/* reserved */
};

	/* Configurable parameters */
enum
{
	SMI_DevChar 			= 'X',
	SMI_ShmHdrSize	 	= 256,		/* Space allocated at start for SMI mgmt. Must be > sizeof(Smi) */
	SMI_ShmMinSize 		= 512,		/* Minimum size for SMI */
	SMI_PollSleepTime 		= 200,		/* Poll Sleep Time: configurable parameter */
	SMI_Debug			= 1,			/* Debug ON */
	SMI_NoDebug			= 0,			/* Debug OFF */
	SMI_DefDbgLvl 		= 1			/* Default debug level */
};

#define	SMI_DevStr		"#X"

	/* Debug related stuff */

#define dbnprint	if (0) print
#define sbprint		print
#define sbnprint	if (0) print
#define dbprint		if (smidbg && (smidbglvl >= 1)) print

#define dbprintfunc	dbprint80

#define dbprint10	if (smidbg && (smidbglvl >= 10)) print
#define dbprint20	if (smidbg && (smidbglvl >= 20)) print
#define dbprint30	if (smidbg && (smidbglvl >= 30)) print
#define dbprint40	if (smidbg && (smidbglvl >= 40)) print
#define dbprint50	if (smidbg && (smidbglvl >= 50)) print
#define dbprint60	if (smidbg && (smidbglvl >= 60)) print
#define dbprint70	if (smidbg && (smidbglvl >= 70)) print
#define dbprint80	if (smidbg && (smidbglvl >= 80)) print
#define dbprint90	if (smidbg && (smidbglvl >= 90)) print
#define dbprint100	if (smidbg && (smidbglvl >= 100)) print

	/* General Definitions */
enum
{
	SMI_signature 		= 0xBEADBEAD,	/* SMI signature */

	SMI_StatFree		= 0x01,		/* Free, someone can use it */
	SMI_StatInit		= 0x02,		/* Initialization in progress */
	SMI_StatReady		= 0x03,		/* Ready */
	SMI_StatPriRel		= 0x04,		/* Primary Released (and soon secondary should release 
											and make it SMI_StatFree) */
	SMI_StatSecRel		= 0x05,		/* Secondary Released (and soon primary should release 
											and make it SMI_StatFree) */
		/* Various types of shared memory */
	SMI_ShmIntern		= 1,		/* Internal: memory is within the Inferno node  */
	SMI_ShmDPRAM		= 2,		/* Dual-port RAM */
	SMI_ShmMPRAM		= 3,		/* Multi-port RAM */
	SMI_ShmPCI			= 4,		/* PCI or CompactPCI */
	SMI_ShmVME			= 5,		/* VME */
	SMI_ShmUSB			= 6,		/* USB */
	SMI_ShmIEEE1394		= 7,		/* IEEE1394 */
	SMI_ShmSBSPPShMem	= 8,		/* SBS-bit-3's peer-to-peer shared memory */
	SMI_ShmSBSBroadMem	= 9,		/* SBS-bit-3's broadcast memory */
	SMI_ShmEmuSol		= 10,		/* Solaris EMU */
	SMI_ShmEmuWin		= 11,		/* Windows EMU */

	SMI_PriUser			= 1,		/* Primary user */
	SMI_SecUser			= 2		/* Secondary user */
};

struct
{
	Lock	l;
	ulong	path;
} smialloc;

enum
{
	Qdir,
	Qdata,
	Qctl
};

Dirtab smidir[] =
{
	"data",	{Qdata},	0,	0666,
	"ctl",		{Qctl},	0,	0666,
};

#define NSMIDIR 2

static int initsmi(Smi *ps);
static int setsmidesc(Smi *ps, void *pshmem, int size, int shmtype, int usertype);
static int smiqread(Smi	*ps, char *buf, int n, int drain);
static int smiqwrite(Smi *ps, char *buf, int n);
static void smipollp(void *ps);
uint	get2(char *p);
uint 	get4(uint *P);
uint	put4(uint *p,uint val);
uint 	htolsb2(ushort val);
uint	htolsb4(uint   val);


int smidbg 	= SMI_Debug;			/* turns debugging ON or OFF */
int smidbglvl 	= SMI_DefDbgLvl;		/* sets the debug level */

static void
smiinit(void)
{
	dbprintfunc("smiinit()\n");
}

static Chan*
smiattach(char *spec)
{
	Smi 		*ps;
	Chan 		*c;

	dbprintfunc("smiattach()\n");
	c = devattach(SMI_DevChar, spec);
	if(ps == 0)
		error(Enomem);
	ps = mallocz(sizeof(Smi), 1);
	initsmi(ps);
	ps->ref = 1;

	lock(&smialloc.l);
	ps->path = ++smialloc.path;
	unlock(&smialloc.l);

	c->qid.path = CHDIR|NETQID(2*ps->path, Qdir);
	c->qid.vers = 0;
	c->aux = ps;
	c->dev = 0;
	return c;
}

static Chan*
smiclone(Chan *c, Chan *nc)
{
	Smi 	*ps;

	dbprintfunc("smiclone()\n");
	ps = c->aux;
	nc = devclone(c, nc);
	lock(&ps->l);
	ps->ref++;
	if(c->flag & COPEN){
		switch(NETTYPE(c->qid.path)){
		case Qdata:
			ps->qref++;
			break;
		}
	}
	unlock(&ps->l);
	return nc;
}

static int
smigen(Chan *c, Dirtab *tab, int ntab, int i, Dir *dp)
{
	int 		id;
	Qid 	qid;

	dbprintfunc("smigen()\n");
	id = NETID(c->qid.path);
	if(i > 1)
		id++;
	if(tab==0 || i>=ntab)
		return -1;
	tab += i;
	qid.path = NETQID(id, tab->qid.path);
	qid.vers = 0;
	devdir(c, qid, tab->name, tab->length, eve, tab->perm, dp);
	return 1;
}


static int
smiwalk(Chan *c, char *name)
{
	dbprintfunc("smiwalk()\n");
	return devwalk(c, name, smidir, NSMIDIR, smigen);
}

static void
smistat(Chan *c, char *db)
{
	Smi 		*ps;
	Dir 			dir;

	dbprintfunc("smistat()\n");

	ps = c->aux;

	switch(NETTYPE(c->qid.path)){
	default:
		panic("smistat");
	case Qdir:
		devdir(c, c->qid, SMI_DevStr, 2*DIRLEN, eve, CHDIR|0555, &dir);
		break;
	case Qdata:
		devdir(c, c->qid, "data", ps->size, eve, 0666, &dir);
		break;
	case Qctl:
		devdir(c, c->qid, "ctl", 0, eve, 0666, &dir);
		break;
	}
	convD2M(&dir, db);
}

static Chan*
smiopen(Chan *c, int omode)
{
	Smi 		*ps;
	uint		status;

	dbprintfunc("smiopen()\n");
	if(c->qid.path & CHDIR){
		if(omode != OREAD)
			error(Ebadarg);
		c->mode = omode;
		c->flag |= COPEN;
		c->offset = 0;
		return c;
	}

	ps = c->aux;
	lock(&ps->l);
	switch(NETTYPE(c->qid.path)) {
	case Qdata:
		ps->qref++;
		break;
	}
	unlock(&ps->l);

	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

static void
smiclose(Chan *c)
{
	Smi 		*ps;
	SmiDesc 	*psd;

	dbprintfunc("smiclose()\n");
	ps = c->aux;
	psd = ps->psd;

	lock(&ps->l);

	if(c->flag & COPEN){
		/*
		 *  closing either side hangs up the stream
		 */
		switch(NETTYPE(c->qid.path)) {
		case Qdata:
			ps->qref--;
			if (ps->qref == 0)
			{
				switch(ps->usertype) {
				default:
					error("unknown user type");
					break;
				case SMI_PriUser:
					if (get4(&psd->status) == SMI_StatSecRel)
						psd->status = htolsb4(SMI_StatFree);
					else 
						psd->status = htolsb4(SMI_StatPriRel);
					break;
				case SMI_SecUser:
					if (get4(&psd->status) == SMI_StatPriRel)
						psd->status = htolsb4(SMI_StatFree);
					else 
						psd->status = htolsb4(SMI_StatSecRel);
					break;
				}
			}
			break;
		case Qctl:
			break;
		}
	}

	/*
	 *  free the Smi structure on last close
	 */
	ps->ref--;
	if (ps->ref == 0) {
		ps->windup = 1;
		wakeup(&ps->vouzpoll);
		sleep(&ps->vouzpollpexit, return0, 0);
		unlock(&ps->l);
		free(ps);
	}
	unlock(&ps->l);
	return;
}

static long
smiread(Chan *c, void *va, long n, ulong junk)
{
	Smi 		*ps;
	SmiDesc 	*psd;
	char		buf[200];
	char		*pb;
	int			status;
	int			len;

	dbprintfunc("smiread()\n");
	ps = c->aux;

	USED(junk);
	switch(NETTYPE(c->qid.path)) {
	default:
		panic("smiread");
	case Qdir:
		return devdirread(c, va, n, smidir, NSMIDIR, smigen);
	case Qdata:
		lock(&ps->rxl);
			/* Get the length and then data */
		smiqread(ps, buf, 2, 0);	
		len = get2(buf);
		dbprint100("smiread: len is %d\n", len);
		if (len <= n)
		{
			unlock(&ps->rxl);
			return smiqread(ps, va, len, 0);
		}
			/* Message length is more than reqd: need to drain extra data */
		smiqread(ps, va, n, 0);
		smiqread(ps, va, len - n, 1);
		unlock(&ps->rxl);
		return n;
	case Qctl:
		pb = buf;
		if (ps->psd == nil)
		{
			error("SMI not set");
			return 0;
		}
		psd = ps->psd;
		status = get4(&(psd->status));
		switch(status)	{
		default:
			sprint(pb, "unknown SMI status: 0x%x", status);
			break;
		case SMI_StatFree:
			sprint(pb, "status Free\n");
			memmove(va, buf, strlen(pb));
			return (strlen(pb));
			break;
		case SMI_StatInit:
			sprint(pb, "status Init\n");
			break;
		case SMI_StatReady:
			sprint(pb, "status Ready\n");
			break;
		case SMI_StatPriRel:
			sprint(pb, "status PriReleased\n");
			break;
		case SMI_StatSecRel:
			sprint(pb, "status SecReleased\n");
			break;
		}
		pb += strlen(pb);
		sprint(pb, "\nshmem ptr %lud size %d\n"
			"txsize %d, txstartp %d, txendp %d, txrp %d, txwp %d\n"
			"rxsize %d, rxstartp %d, rxendp %d, rxrp %d, rxwp %d\n",
			ps->pstart, ps->size, 
			ps->txsize, ps->txstart, ps->txend, get4(ps->ptxrp), get4(ps->ptxwp), 
			ps->rxsize, ps->rxstart, ps->rxend, get4(ps->prxrp), get4(ps->prxwp));
		memmove(va, buf, strlen(buf));
		return strlen(buf);
	}
	return 0;	/* not reached */
}

static Block*
smibread(Chan *c, long n, ulong offset)
{
	dbprintfunc("smibread()\n");
	return devbread(c, n, offset);
}

/*
 *  a write to a closed smi causes an exception to be sent to
 *  the prog.
 */
static long
smiwrite(Chan *c, void *va, long n, ulong junk)
{
	Smi 		*ps;
	Prog 	*r;
	int 		nf;
	char 		*fields[12], buf[128];
	char		*pshmem;
	int		size;
	int		shmtype;
	int		usertype;
	ushort	len;

	dbprintfunc("smiwrite()\n");
	USED(junk);
	if(waserror()) {
		/* avoid exceptions when smi is a mounted queue */
		if ((c->flag & CMSG) == 0) {
			r = up->iprog;
			r->kill = "write on closed smi";
		}
		error(Ehungup);
	}

	ps = c->aux;

	switch(NETTYPE(c->qid.path)){
	default:
		panic("smiwrite");
	case Qdata:
		lock(&ps->txl);
			/* Write len first, then msg */
		len = htolsb2(n);
		smiqwrite(ps, (void *) &len, 2);
		smiqwrite(ps, va, n);
		dbnprint("writing msg of size %d\n", n);

		unlock(&ps->txl);
		break;
	case Qctl:
		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, va, n);
		buf[n] = '\0';

		nf = parsefields(buf, fields, 12, " ");
		dbprint100("nf = %d\n", nf);
		if ((strcmp(fields[0], "init") == 0) ||
				(strcmp(fields[0], "join") == 0)) {
			switch(nf){
			default:
				error("bad args to init");
				break;
			case 4:
				pshmem =(char *) atoi(fields[2]);
				size = atoi(fields[3]);
				if((size <= 0) || (size < SMI_ShmMinSize))
					error(Ebadarg);
				if (strcmp(fields[1], "internal") == 0)
					shmtype = SMI_ShmIntern;
				else {
					error("shared memory type unknown or unsupported");
					break;
				}
				if (strcmp(fields[0], "init") == 0) {
					usertype =  SMI_PriUser;
				} else {
					usertype = SMI_SecUser;
				}
				if (setsmidesc(ps, (void *)pshmem, size, shmtype, usertype) != 0)
				{
					error("SMI setup failed");
					break;
				}
				break;
			}
		}
		else if (strcmp(fields[0], "pollinterval") == 0) {
			switch(nf){
			default:
				error("bad args to pollinterval");
				break;
			case 1:
				ps->pollinterval = SMI_PollSleepTime;
				break;
			case 2:
				ps->pollinterval = atoi(fields[1]);
				break;
			}
		}
		else if (strcmp(fields[0], "setdbg") == 0) {
			switch(nf){
			default:
				error("bad args to setdbg");
				break;
			case 1:
				smidbg = 1;
				break;
			case 2:
				smidbg = 1;
				smidbglvl = atoi(fields[1]);
				dbprint("smidbg = %d smidbglvl = %d\n", smidbg, smidbglvl);
				break;
			}
		}
		else if (strcmp(fields[0], "clrdbg") == 0) {
			smidbg = 0;
			break;
		}
		else error("bad control message");
	}

	poperror();
	return n;
}

static long
smibwrite(Chan *c, Block *bp, ulong offset)
{
	dbprintfunc("smibwrite()\n");
	return devbwrite(c, bp, offset);
}

static int 
initsmi(Smi *ps)
{
	dbprintfunc("initsmi()\n");
	ps->windup = 0;

	return 0;
}

static int
setsmidesc(Smi *ps, void *pshmem, int size, int shmtype, int usertype)
{
	SmiDesc	*psd;
	int		chunksize;
	uint 		signature;
	uint 		status;

	dbprintfunc("setsmidesc()\n");
	dbprint("shmtype = %d usertype = %d\n", shmtype, usertype);
		/* Set signature and status to say SMI is not ready yet */
	psd = pshmem;
	if (usertype == SMI_PriUser)
	{
		psd->signature = htolsb4(0);
		psd->status = htolsb4(SMI_StatInit);
	}
	else
	{
		signature = get4(&psd->signature);
		status = get4(&psd->status);

		if (signature != SMI_signature)
		{
			dbprint("signature mismatch. Expected 0x%ux. Found 0x%ux\n", SMI_signature, signature);
			return -1;
		}
		else if (status != SMI_StatInit)
		{
			dbprint("SMI not ready yet. State 0x%x\n", status);
			return -1;
		}
	}
	dbprint100("pos of status is 0x%lux val is 0x%lux\n", &(psd->status), get4(&psd->status));

	chunksize = (size - SMI_ShmHdrSize) / 2;

	ps->shmtype = shmtype;
	ps->usertype = SMI_PriUser;
	ps->pstart = pshmem;
	ps->size = size;
	ps->psd =(SmiDesc *) ps->pstart;
	ps->pollinterval = SMI_PollSleepTime;
	ps->txfull = 0;
	ps->rxempty = 0;
	ps->windup = 0;

	if (usertype == SMI_PriUser)
	{
		ps->txsize = chunksize;
		ps->txstart = SMI_ShmHdrSize;
		ps->txend = SMI_ShmHdrSize + ps->txsize;
		ps->ptxrp = &(psd->txrp);
		ps->ptxwp = &(psd->txwp);
		ps->rxsize = chunksize;
		ps->rxstart = SMI_ShmHdrSize + ps->txsize;
		ps->rxend = SMI_ShmHdrSize + ps->txsize + ps->rxsize;
		ps->prxrp = &(psd->rxrp);
		ps->prxwp = &(psd->rxwp);

	}
	else	/* Tx and Rx must be swapped */
	{
		ps->rxsize = chunksize;
		ps->rxstart = SMI_ShmHdrSize;
		ps->rxend = SMI_ShmHdrSize + ps->rxsize;
		ps->prxrp = &(psd->txrp);
		ps->prxwp = &(psd->txwp);
		ps->txsize = chunksize;
		ps->txstart = SMI_ShmHdrSize + ps->rxsize;
		ps->txend = SMI_ShmHdrSize + ps->rxsize + ps->txsize;
		ps->ptxrp = &(psd->rxrp);
		ps->ptxwp = &(psd->rxwp);
	}

		/* Set generic SMI Desc */
	if (usertype == SMI_PriUser)
	{
		psd = pshmem;
		psd->signature = htolsb4(SMI_signature);
		psd->size = htolsb4(size);
		psd->txrp = htolsb4(ps->txstart);
		psd->txwp = htolsb4(ps->txstart);
		psd->rxrp = htolsb4(ps->rxstart);
		psd->rxwp = htolsb4(ps->rxstart);
		psd->status = htolsb4(SMI_StatInit);
	}
	else
	{
		psd->status = htolsb4(SMI_StatReady);
	}
		
	dbprint40("spawning kproc\n");
	kproc("smipollp", smipollp, ps);

	dbprint100("pos of status is 0x%lux val is 0x%lux\n", &(psd->status), get4(&psd->status));
	return 0;
}

static int
smiqread(Smi *ps, char *buf, int n, int drain)
{
	char	*p;
	int		reqd;
	int		count;
	int		lrp, lwp, lep;

	dbprintfunc("smiqread()\n");
	dbprint100("smiqread: n is %d\n", n);
	p = buf;
	reqd = n;

	while (reqd > 0)
	{
		lrp = get4(ps->prxrp);
		lwp = get4(ps->prxwp);
	
		if (lrp == lwp) {
			ps->rxempty = 1;
			wakeup(&ps->vouzpoll);
			dbprint100("smiqread: after poll wakeup and before read sleep\n");
			sleep(&ps->vouzread, return0, 0);
			dbprint100("smiqread: after read wakeup\n");
			continue;
		}	
		if (lrp < lwp)	/* Then there is only one segment */
			lep = lwp;
		else lep = ps->rxend;
		count = lep - lrp;
		if (reqd < count) count = reqd;
		if (!drain) memmove(p, PTR(ps->pstart, lrp) , count);
		dbprint100("read: memcpying dest loc %d src ptr %d count %d\n", p, (ulong) PTR(ps->pstart, lrp), count);
		reqd -= count;
		if ((lrp + count) >= ps->rxend) put4(ps->prxrp, ps->rxstart);
		else put4(ps->prxrp, (lrp + count));
		p += count;
	}
	return (n - reqd);
}

static int
smiqwrite(Smi *ps, char *buf, int n)
{
	char	*p;
	int		reqd;
	int		count;
	int		lrp, lwp, lep;

	dbprintfunc("smiqwrite()\n");
	p = buf;
	reqd = n;
	while (reqd > 0)
	{
		lrp = get4(ps->ptxrp);
		lwp = get4(ps->ptxwp);
		dbprint100("smi: tx rp %d wp %d\n", lrp, lwp);
	
		if (((lwp+1) == lrp) ||
			((lwp == (ps->txend - 1)) && (lrp == ps->txstart)))
		{	/* Full */
			ps->txfull = 1;
			wakeup(&ps->vouzpoll);
			dbprint100("smiqwrite: after poll wakeup and before write sleep\n");
			sleep(&ps->vouzwrite, return0, 0);
			dbprint100("smiqwrite: after write wakeup\n");
			continue;
		}	
		if (lwp < lrp)	/* Then there is only one segment */
			lep = lrp - 1;
		else if (lrp == ps->txstart) lep = ps->txend - 1;
		else lep = ps->txend;
		count = lep - lwp;
		if (reqd < (lep - lwp)) count = reqd;
		dbprint100("write: memcpying dest loc %d src ptr %d count %d\n", (ulong) PTR(ps->pstart, lwp), lwp, count);
		memmove(PTR(ps->pstart, lwp), p, count);
		reqd -= count;
		if ((lwp + count) >= ps->txend) put4(ps->ptxwp, ps->txstart);
		else put4(ps->ptxwp, (lwp + count));
		p += count;
	}
	return (n - reqd);
}

static void
smipollp(void *pv)
{
	Smi			*ps;
	SmiDesc	*psd;
	uint		status;
	uint		lrp;
	uint		lwp;
	
	dbprintfunc("smipollp()\n");

	ps = pv;
	psd = ps->psd;

	for ( ; ; )
	{
		sleep(&ps->vouzpoll, return0, 0);
		for ( ; ; )
		{
			if (ps == nil) {
				pexit("smipollp", 0);
			}
			if (ps->windup == 1) {
				ps->windup = 0;
				wakeup(&ps->vouzpollpexit);
				break;
			}
			status = get4(&(psd->status));
			if (status != SMI_StatReady) {
				microdelay(ps->pollinterval);
				continue;
			}
			if (ps->rxempty == 1) {
				lrp = get4(ps->prxrp);
				lwp = get4(ps->prxwp);
				dbprint100("utype = %d rxrp = %d rxwp = %d\n", ps->usertype, lrp, lwp);
				if (lrp != lwp) {	/* Not empty */
					ps->rxempty = 0;
					dbprint100("before waking up read\n");
					wakeup(&ps->vouzread);
					dbprint100("after waking up read\n");
				}
			}
			if (ps->txfull == 1) {
				lrp = get4(ps->ptxrp);
				lwp = get4(ps->ptxwp);
				dbprint100("utype = %d txrp = %d txwp = %d\n", ps->usertype, lrp, lwp);
				if (((lwp+1) != lrp) &&
					((lwp != (ps->txend - 1)) || (lrp != ps->txstart))) { /* Not full */
					ps->txfull = 0;
					dbprint100("before waking up write\n");
					wakeup(&ps->vouzwrite);
					dbprint100("after waking up write\n");
				}
			}
			if (ps->rxempty || ps->txfull) {
				microdelay(ps->pollinterval);
				continue;
			}
			break;
		}
	}
	
}

uint
get2(char *p)
{
	ushort val;

	val = *p;	
	val = (((val & 0xFF) << 8) |
			(((val >> 8) & 0xFF)));
	return val;
}

uint
get4(uint *p)
{
	uint val;

	val = *p;	
	val = (((val & 0xFF) << 24) |
			(((val >> 8) & 0xFF) << 16) |
			(((val >> 16) & 0xFF) << 8) |
			((val >> 24) & 0xFF));
	return val;
}

uint
put4(uint *p, uint val)
{
	uint tmp;
	tmp = (((val >> 24) & 0xFF) |
			(((val >> 16) & 0xFF) << 8) |
			(((val >> 8) & 0xFF) << 16) |
			((val & 0xFF) << 24));
	*p = tmp;
	return 0;
}

uint
htolsb2(ushort val)
{
	ushort tmp;
	tmp = (((val >> 8) & 0xFF) |
			((val & 0xFF) << 8));
	return tmp;
}

uint
htolsb4(uint val)
{
	uint tmp;
	tmp = (((val >> 24) & 0xFF) |
			(((val >> 16) & 0xFF) << 8) |
			(((val >> 8) & 0xFF) << 16) |
			((val & 0xFF) << 24));
	return tmp;
}

Dev smidevtab = {
	SMI_DevChar,
	"smi",

	devreset,
	smiinit,
	smiattach,
	devdetach,
	smiclone,
	smiwalk,
	smistat,
	smiopen,
	devcreate,
	smiclose,
	smiread,
	smibread,
	smiwrite,
	smibwrite,
	devremove,
	devwstat,
};
