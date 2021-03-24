/*
 *	Driver for the AltoCom software modem.  Links against pieces pulled in
 *	from /os/smarm/...
 */

#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"io.h"

//#define MODEMSTATS

#define UNCACHED(v)	((void*)PADDR((v)))

/* set to 9600 for fixed rate softmodem, and 0 otherwise */
#define FIXEDRATE	9600

#define DEFRATE		(FIXEDRATE ? FIXEDRATE : 7200)

#define SoftModemTypes		// some typedefs clash (not in a bad way)
typedef unsigned char Boolean;	// because of the line above
#include "../smarm/SoftModem.h"
extern uchar smodem_code[];

enum {
	Qdir,
	Qsoftmodem,
	Qsoftmodemctl,
	Qsoftmodemstat,
};

static Dirtab softmodemdir[] = {
	{ "modem",	{Qsoftmodem, 0},	0,	0660 },
	{ "modemctl",	{Qsoftmodemctl, 0},	0,	0660 },
	{ "modemstat",	{Qsoftmodemstat, 0},	0,	0444 },
};

void	smstatus_callback_stub(modemStatusStruct*);
void	(*softmodemdebug)(void*, char*);


enum {
	 // to softmodem
	CircuitRTS=	105,
	CircuitDTR=	108,

	 // from softmodem
	CircuitCTS=	106,
	CircuitDSR=	107,
	CircuitDCD=	109,
	CircuitCI=	125,

	 // Misc
	Sqlen=		4*1024,			// status queue len
	Buflen=		48,			// length in shorts

	 // dma channels to use for transmit and receive
	DmaRcvChan=	0,
	DmaXmtChan=	1,
};

typedef struct Softmodem Softmodem;
typedef struct SMIOStatus SMIOStatus;
typedef struct SMCodecStatus SMCodecStatus;
typedef struct SMDMAStatus SMDMAStatus;
typedef struct SMOpenStatus SMOpenStatus;
typedef struct SMStatus SMStatus;
typedef struct BlockInfo BlockInfo;
typedef struct SMShutdownStatus SMShutdownStatus;
typedef struct SMFlags SMFlags;


// This is the near-end echo delay
// from in Bell-Labs, 0x23 seems to be the right value for dialing outside,
// 0x1e seems to be right for inside, and 0x26 seems to be right for
// dialing back inside from *9,582xxxx
// The softmodem will compute possible values for this when it connects

extern int smodem_HybridDelay;	/* default is now set in arch*.c */


struct SMIOStatus
{
	Rendez;
	int ready;
	int wakeup;
	Lock l;
};

struct SMCodecStatus
{
	int curfreq;
	short *in[2];
	int inext;
	short *out[2];
	int onext;
};

struct SMOpenStatus
{
	QLock;
	int nopen;
};

struct SMStatus
{
	 // misc statistics
	ulong nxmt;			// number of bytes transmitted
	ulong nrcv;			// number of bytes received

	int oerr;

	int fakespeed;			// speed to report in Qsoftmodemstat

	int offhook;
	int xmtrate;
	int rcvrate;
	struct {
		Lock;
		int cts;		// clear to send
		int dsr;		// data set ready	(softmodem)
		int dtr;		// data terminal ready	(inferno)
		int dcd;		// data carrier detect
		int ci;			// calling indicator (ring signal)
	};
};

struct SMShutdownStatus			// on close, ready to reset hardware
{
	Rendez;
	int shutdown;
};

struct SMFlags
{
	Lock;
	int hungup;		// Detect if we lost carrier detect
	int eof;		// EOF count (to emulate qio semantics)
};

struct Softmodem
{
	Queue *sq;			// status queue
	SMIOStatus write;
	SMIOStatus read;
	SMCodecStatus codec;
	SMOpenStatus open;		// refcount on opens
	SMFlags flags;			// misc event flags

	SMShutdownStatus shutdown;	// kproc shutdown

	NVRAMConfiguration nvram;	// XXX - Tad: currently unused

	SMStatus stats;			// misc stats
};

struct BlockInfo
{
	SMIOStatus *ios;
	SMStatus *stats;
	SMFlags *flags;
};

volatile Softmodem smstatus;	// holds state for the softmodem
ulong smodem_base;	// set by softmodeminit()

static void	smstatusproc(Softmodem *sm);
static void	setcircuit(Softmodem *sm, int c, int on);
static int	getrate(int rate);

//
// The dma controller has a bug: it can't do xfers across 256 byte boundaries
//
// XXX - Tad: this problem is supposed to be fixed in the Rev 2.0 version of
//	      the processor
//
static short*
dmahackalloc(int xfersize)
{
	static char buf[768+0x100];
	static char *cur = buf;
	static char *end = buf + (sizeof(buf)-1);

	int distance;
	short *r;

	distance = (((ulong)cur + 0x100) & 0xFFFFFF00) - (ulong)cur;

	if(distance < xfersize)
		cur = cur + distance;

	if(cur > end || cur + xfersize > end)
		return nil;

	r = (short*)cur;

	cur += xfersize;

	return r;
}


static void
load_nvram(NVRAMConfiguration*)
{
	// not implemented
}

static void
save_nvram(NVRAMConfiguration*)
{
	// not implemented
}

void
smstatus_callback(modemStatusStruct *s)
{
	 //
	 // XXX - Tad: doesn't work for supporting multiple softmodems!
	 //
	SMCodecStatus *c = &smstatus.codec;
	Block *bp;

	// some things to ignore
	if(s->code == kFramingInfo) {
		return;
	} 

	 //
	 // handle kSetSampleRate immediately.
	 // everything else gets sent to the kproc.
	 //
	if(s->code == kSetSampleRate) {
		if(c->curfreq != (int) s->param.value) {
			c->curfreq = (int)s->param.value;
			mcpsettfreq(c->curfreq);
		}
	}

	bp = iallocb(sizeof(*s));
	if(bp == nil)
		return;
	memmove(bp->wp, s, sizeof(*s));
	bp->wp += sizeof(*s);
	qpass(smstatus.sq, bp);
}

static void
rcvintr(Ureg*, Softmodem *sm)
{
	int n;
	SMCodecStatus *c = &sm->codec;

	if(dmaerror(DmaRcvChan))
		panic("softmodem: rcvintr: dmaerror");

	/* by this point, we can safely assume that the point where
	 * a transmit interrupt would occur has just gone by
	 * so we set up the next transmit (following the currently
	 * pending buffer) with the address of the buffer 
	 * we're about to put data into, knowing that we will be
	 * done putting data there by the time it reaches that point.
	 * (if we're not, then we're taking more than 1/200th of
	 * a second to do our processing, and we're dead anyway)
 	 */

	/* apparently we can't always safely assume that due to
	 * some weird problem when restarting the softmodem,
	 * so we need to catch the lack of synchronization,
	 * and synchronize them
 	 */
	n = Buflen*sizeof(short);
	while(dmacontinue(DmaXmtChan, c->out[c->onext], n) < 0) {
		/* wait a sample's worth of time, and decrease
		 * the amount of time the next transmit takes,
		 * so that we get back in sync
		 */
		n -= sizeof(short);
		microdelay(1000000/DEFRATE);
		/* XXX - incorrect rate for variable freq */
	}

	/* we're also going to schedule a read into the buffer we're
	 * currently using, knowing that there's another pending buffer
	 * in progress, and we have to finish using the data before 
	 * it starts reading into there (again, 1/200th of a second)
	 */
	dmacontinue(DmaRcvChan, c->in[c->inext], Buflen*sizeof(short));

	SoftModemLineHandler(Buflen, c->in[c->inext], c->out[c->onext]);
	c->inext ^= 1;		// toggle input buffers
	c->onext ^= 1;		// toggle output buffers
}



//
// XXX - Tad: only works for a single softmodem instance
//
static void
sminit(void)
{
	SMCodecStatus *c;
	int i;

	if(waserror()) {
		nexterror();
	}

	smodem_base = aifinit(smodem_code);

	 // load the modem's NVRAM
	load_nvram(&smstatus.nvram);

	SoftModemSetMemoryPtr(nil);

	c = &smstatus.codec;

	 //
	 // Allocate DMA buffers
	 //
	 // XXX - Tad: this is a hack because the version of the StrongArm
	 //	       we have has a bug: DMA xfer's can't span 256 byte
	 //	       boundaries.
	 //
	for(i=0; i < nelem(c->in); i++) {
		short *a = dmahackalloc(Buflen*sizeof(short));
		if(a == nil)
			error(Enomem);
		c->in[i] = (a = UNCACHED(a));
		memset(a, 0, Buflen*sizeof(short));
	}

	for(i=0; i < nelem(c->out); i++) {	
		short *a = dmahackalloc(Buflen*sizeof(short));
		if(a == nil)
			error(Enomem);
		c->out[i] = (a = UNCACHED(a));
		memset(a, 0, Buflen*sizeof(short));
	}

	dmasetup(DmaXmtChan, DmaMCPtelecom, DmaOUT, DmaLittle);
	dmasetup(DmaRcvChan, DmaMCPtelecom, DmaIN, DmaLittle);
	intrenable(DMAbit(DmaRcvChan), rcvintr, &smstatus, BusCPU);

	poperror();
}

static Chan*
smattach(char *spec)
{
	return devattach('O', spec);
}

static int
smwalk(Chan *c, char *name)
{
	return devwalk(c, name, softmodemdir, nelem(softmodemdir), devgen);
}

static void
smstat(Chan *c, char *dp)
{
	devstat(c, dp, softmodemdir, nelem(softmodemdir), devgen);
}

static Queue*
statusqinit(Queue *sq)
{
	if (sq == nil)
	{
		sq = qopen(Sqlen, 0, 0, 0);
		if(sq == nil)
			error(Enomem);
		qnoblock(sq, 1);
	}
	else
		qreopen(sq);

	return sq;
}

static void
statusqshutdown(Softmodem *sm)
{
	if(sm->sq != nil) {
		qclose(sm->sq);
	}
}

static void
smlibinit(void)
{
	char *b;
	modemCommandStruct *cmd;

	b = malloc(SoftModemGetMemorySize());
	if(b == nil)
		error(Enomem);
	SoftModemSetMemoryPtr(b);
	SoftModemInit();

	cmd = malloc(sizeof(*cmd));
	if(cmd == nil)
		error(Enomem);
	 // setup status handler
	cmd->command = kSetStatusHandlerCmd;
	cmd->param.handlerSpec.statusHandlerPtr= smstatus_callback_stub;
	SoftModemCommandHandler(cmd);

	 // setup HAYES AT command mode
	cmd->command = kStartATModeCmd;
	cmd->param.nvramConfigurationPtr = nil;
	SoftModemCommandHandler(cmd);

	 // set hybrid sample delay -- constant for given hardware
	cmd->command = kSetHybridDelayCmd;
	cmd->param.hybridDelayQ4ms = smodem_HybridDelay;
	SoftModemCommandHandler(cmd);

	if(FIXEDRATE) {
		cmd->command = kSetModemSampleRateCmd;
		cmd->param.modemSampleRate = DEFRATE;
		SoftModemCommandHandler(cmd);
	} else {
		cmd->command = kSetATRegister;
		cmd->param.atRegister.code = 505;
		cmd->param.atRegister.value =
			 k1200Hz
			|k1600Hz
			|k2400Hz
			// |k2743Hz	(8229 != 8320)
			// |k2800Hz	(8400 != 8320)
			// |k3000Hz	(9000 != 8914)
			|k3200Hz
			// |k3429Hz	(10287 != 10400)
			;
		SoftModemCommandHandler(cmd);
	}

	free(cmd);
}

static void
smlibshutdown(void)
{
	char *b;

	b = SoftModemGetMemoryPtr();
	if(b != nil) {
/*		SoftModemReset(); */
		free(b);
		SoftModemSetMemoryPtr(nil);
	}
}

static void
codecinit(SMCodecStatus *codec)
{
	int i;
	int x;

	for(i = 0; i < nelem(codec->in); i++)
		memset(codec->in[i], 0, Buflen*sizeof(short));
	for(i = 0; i < nelem(codec->out); i++)
		memset(codec->out[i], 0, Buflen*sizeof(short));

	mcptelecomsetup(DEFRATE, 1, 1, 1);

	x = splhi();	/* the following two DMA starts must occur
			 * immediately in sequence, with no interruptions,
			 * to ensure proper synchronization between the
			 * input and output sample buffers
			 */
	codec->onext = 0;
	dmastart(DmaXmtChan, codec->out[0], Buflen*sizeof(short),
		 codec->out[1], Buflen*sizeof(short));
	// wait long enough for one sample, to avoid
	// race-conditions with send/receive synchronization:
	microdelay(1000000/DEFRATE);
	codec->inext = 0;
	dmastart(DmaRcvChan, codec->in[0], Buflen*sizeof(short),
		 codec->in[1], Buflen*sizeof(short));
	splx(x);
}

static void
codecshutdown(SMCodecStatus */*codec*/)
{
	int x;
	x = splhi();	/* just to be safe */
	dmastop(DmaRcvChan);
	dmastop(DmaXmtChan);
	splx(x);
	mcptelecomsetup(DEFRATE, 0, 0, 0);
}

static void
statusreset(SMStatus *s)
{
	lock(s);
	s->nxmt=0;
	s->nrcv=0;
	s->oerr=0;
	s->fakespeed=0;
	s->offhook=0;
	s->xmtrate=0;
	s->rcvrate=0;
	s->cts=0;
	s->dsr=0;
	s->dtr=0;
	s->dcd=0;
	s->ci=0;

	unlock(s);
}

static void
flagsreset(SMFlags *f)
{
	lock(f);
	f->hungup = 0;
	f->eof = 0;
	unlock(f);
}

static Chan*
smopen(Chan *c, int omode)
{
	 //
	 // XXX - Tad: eventually pick the appropriate smstatus.
	 //
	Softmodem *sm = &smstatus;

	omode = openmode(omode);

	qlock(&sm->open);
	if(waserror()) {
		qunlock(&sm->open);
		nexterror();
	}
	c->aux = sm;
	if((c->qid.path & ~CHDIR) == Qsoftmodem && ++sm->open.nopen == 1) {
		statusreset(&sm->stats);
		flagsreset(&sm->flags);
		if(waserror()) {
			codecshutdown(&sm->codec);
			statusqshutdown(sm);
			smlibshutdown();
			statusreset(&sm->stats);
			flagsreset(&sm->flags);
			nexterror();
		}
		sm->sq = statusqinit(sm->sq);
		smlibinit();
		kproc("SoftModem", smstatusproc, sm);
		setcircuit(sm, CircuitDTR, 1);		// Enable DTR
		setcircuit(sm, CircuitRTS, 1);		// Enable RTS
		codecinit(&sm->codec);
		poperror();
	}
	poperror();
	qunlock(&sm->open);

	return devopen(c, omode, softmodemdir, nelem(softmodemdir), devgen);
}

static int
sm_block(BlockInfo *b)
{
	if(b->ios->ready) return 1;
	if(!b->stats->dtr) return 1;
	if(b->flags->hungup) return 1;

	b->ios->wakeup = 1;

	return 0;
}

static long
smgetstat(Softmodem *sm, void *buf, long n, ulong offset)
{
	char *msg = malloc(256);
	SMStatus *s = &sm->stats;
	long r;

	if(msg == nil)
		error(Enomem);

	lock(s);
	r = sprint(msg, "opens %d oerr %d baud %d",
			sm->open.nopen, s->oerr, s->fakespeed);

	if(s->cts)
		r += sprint(msg+r, " cts");
	if(s->dsr)
		r += sprint(msg+r, " dsr");
	if(s->ci)
		r += sprint(msg+r, " ring");
	if(s->dcd)
		r += sprint(msg+r, " dcd");
	if(s->dtr)
		r += sprint(msg+r, " dtr");
	if(s->offhook) {
		r += sprint(msg+r, " offhook xmtrate %d rcvrate %d",
			getrate(s->xmtrate), getrate(s->rcvrate));
	}
	unlock(s);
	sprint(msg+r, "\n");

	r = readstr(offset, buf, n, msg);

	free(msg);

	return r;
}

static long
smread(Chan *c, void *buf, long n, ulong offset)
{
	int nbytes;
	Softmodem *sm;

	static QLock rlock;

	sm = c->aux;
	switch(c->qid.path & ~CHDIR) {
	case Qdir:
		return devdirread(c, buf, n, softmodemdir,
				  nelem(softmodemdir), devgen);
	case Qsoftmodem:
		qlock(&rlock);
		if(waserror()) {
			qunlock(&rlock);
			nexterror();
		}
		for(nbytes = 0; nbytes == 0; ) {
			BlockInfo b = {&sm->read, &sm->stats, &sm->flags};
			sleep(&sm->read, sm_block, &b);
			lock(&sm->read.l);
			lock(&sm->flags);
			if(sm->flags.hungup) {
				if(++sm->flags.eof > 3) {
					unlock(&sm->flags);
					unlock(&sm->read.l);
					error(Ehungup);
				}
				sm->read.ready = 0;	// return EOF
			}
			unlock(&sm->flags);
			if(sm->read.ready) {
				nbytes = SoftModemRead(n, buf);
				if(nbytes == -1) {
					unlock(&sm->read.l);
					error(Eio);
				} else if(nbytes == 0) {
					sm->read.ready = 0;
				}
				sm->stats.nrcv += nbytes;
			} else {
				nbytes = 0;
				unlock(&sm->read.l);
				break;
			}
			unlock(&sm->read.l);
		}
		poperror();
		qunlock(&rlock);
		return nbytes;
	case Qsoftmodemctl:
		if(softmodemdebug)
			print("softmodemread: Qsoftmodemctl: unimplemented\n");
		return 0;
	case Qsoftmodemstat:
		return smgetstat(sm, buf, n, offset);
	}

	error(Egreg);
	return 0;
}

void
smhangup(Softmodem *sm)
{
	lock(&sm->read.l);
	sm->read.ready = 0;
	if(sm->read.wakeup)
		wakeup(&sm->read);
	unlock(&sm->read.l);

	lock(&sm->write.l);
	sm->write.ready = 0;
	if(sm->write.wakeup)
		wakeup(&sm->write);
	unlock(&sm->write.l);
}

//
// set circuit status to on (1) or off (0)
//
static void
setcircuit(Softmodem *sm, int circuit, int on)
{
	modemCommandStruct *c;
	SMStatus *s = &sm->stats;

	c = malloc(sizeof(*c));
	if(c == nil)
		error(Enomem);

	c->command = kV24CircuitChangeCmd;
	c->param.v24Circuit.code = circuit;
	switch(circuit) {
	case CircuitDTR:
		lock(s);
		s->dtr = on;
		unlock(s);
		c->param.v24Circuit.value = on;
		SoftModemCommandHandler(c);
		if(!on)
			smhangup(sm);
		break;
	case CircuitRTS:
		c->param.v24Circuit.value = on;
		SoftModemCommandHandler(c);
		break;
	default:
		error(Ebadarg);
	}
	free(c);
}

static void
smsetctl(Softmodem *sm, char *cmd)
{
	int n;

	n = strtol(cmd+1, nil, 0);

	switch(*cmd) {
	case 'B': case 'b': sm->stats.fakespeed = n; break;
	case 'D': case 'd': setcircuit(sm, CircuitDTR, n); break;
	case 'F': case 'f': SoftModemWriteFlush(); break;
	case 'H': case 'h': smhangup(sm); break;
	case 'L': case 'l':
	case 'M': case 'm':
	case 'N': case 'n':
	case 'P': case 'p':
	case 'K': case 'k':
	case 'R': case 'r':
	case 'Q': case 'q':
	case 'W': case 'w':
	case 'X': case 'x':
		break;			// XXX - Tad: these cmds unimplemented
	default:
		error(Ebadarg);
	}
}

static long
smwrite(Chan *c, void *buf, long n, ulong)
{
	int nbytes;
	int size;
	char cmd[32];
	Softmodem *sm;

	static QLock wlock;

	if(c->qid.path & CHDIR)
		error(Eperm);

	sm = c->aux;
	switch(c->qid.path) {
	case Qsoftmodem:
		qlock(&wlock);
		if(waserror()) {
			qunlock(&wlock);
			nexterror();
		}
		size = n;
		nbytes = 0;
		while(n > 0) {
			BlockInfo b = {&sm->write, &sm->stats, &sm->flags};
			sleep(&sm->write, sm_block, &b);
			lock(&sm->write.l);
			if(sm->write.ready && !sm->flags.hungup) {
				if(!sm->stats.cts) {
					sm->write.ready = 0;
					unlock(&sm->write.l);
					continue;
				}
				buf = (char*)buf + nbytes;
				nbytes = SoftModemWrite(n, buf);
				if(nbytes == -1) {
					unlock(&sm->write.l);
					error(Eio);
				}
				sm->stats.nxmt += nbytes;
				n -= nbytes;
				if(n > 0) {
					sm->write.ready = 0;
					unlock(&sm->write.l);
					continue;
				}
			} else {
				unlock(&sm->write.l);
				error(Ehungup);
			}
			unlock(&sm->write.l);
		}
		poperror();
		qunlock(&wlock);
		return size;
	case Qsoftmodemctl:
		if(n >= sizeof(cmd))
			n = sizeof(cmd)-1;
		memmove(cmd, buf, n);
		cmd[n] = 0;
		smsetctl(sm, cmd);
		return n;
	}

	error(Egreg);
	return 0;
}

static int
sm_clean(SMShutdownStatus *s)
{
	if(s->shutdown) return 1;

	return 0;
}

static void
smclose(Chan *c)
{
	Softmodem *sm;

	/* If the file isn't open then don't do anything */

	if ((c->flag & COPEN) == 0)
		return;

	sm = c->aux;

	qlock(&sm->open);
	if(waserror()) {
		qunlock(&sm->open);
		nexterror();
	}
	if((c->qid.path & ~CHDIR) == Qsoftmodem && --sm->open.nopen == 0) {
		setcircuit(sm, CircuitDTR, 0);		// Disable DTR
		setcircuit(sm, CircuitRTS, 0);		// Disable RTS
		archhooksw(0);
		sleep(&sm->shutdown, sm_clean, &sm->shutdown);
		codecshutdown(&sm->codec);
		statusqshutdown(sm);
		smlibshutdown();
		statusreset(&sm->stats);
		sm->shutdown.shutdown = 0;
		flagsreset(&sm->flags);
	}
	poperror();
	qunlock(&sm->open);
}

Dev smdevtab = {
	'O',
	"softmodem",

	devreset,
	sminit,
	smattach,
	devdetach,
	devclone,
	smwalk,
	smstat,
	smopen,
	devcreate,
	smclose,
	smread,
	devbread,
	smwrite,
	devbwrite,
	devremove,
	devwstat,
};



static void
stat_iostatus(Softmodem *sm, modemStatusStruct *s)
{
	SMIOStatus *ios;

	switch(s->param.ioStatus) {
	case kTxSpaceAvailable:
	case kTxBufferEmpty:
		ios = &sm->write;
		goto unblock;
	case kRxBufferOverflow:
		sm->stats.oerr++;
	case kRxDataReady:
		if(SoftModemCountReadPending() <= 0) 
			return;
		ios = &sm->read;
	unblock:
		lock(&ios->l);
		ios->ready = 1;
		if(ios->wakeup) {
			ios->wakeup = 0;
			wakeup(ios);
		}
		unlock(&ios->l);
		break;
	default:
		break;
	}
}

static void
stat_circuit(Softmodem *sm, modemStatusStruct* s)
{
	SMIOStatus *c = &sm->write;
	SMIOStatus *r = &sm->read;

	switch(s->param.v24Circuit.code) {
	case CircuitCTS:
		lock(&c->l);
		if(s->param.v24Circuit.value)
			c->ready = 1;
		if(c->wakeup && c->ready) {
			c->wakeup = 0;
			wakeup(c);
		}
		lock(&sm->stats);
		sm->stats.cts = s->param.v24Circuit.value;
		unlock(&sm->stats);
		unlock(&c->l);
		break;
	case CircuitDSR:
		lock(&sm->stats);
		sm->stats.dsr = s->param.v24Circuit.value;
		unlock(&sm->stats);
		break;
	case CircuitDCD:
		lock(&sm->stats);
		if (sm->stats.dcd && !(s->param.v24Circuit.value)) {
			print("Hangup detected\n");
			sm->stats.dcd = 0;
			lock(&sm->flags);
			sm->flags.hungup = 1;
			unlock(&sm->flags);
			if (r->wakeup && r->ready) {
				r->wakeup = 0;
				wakeup(r);
			}
		}
		sm->stats.dcd = s->param.v24Circuit.value;
		unlock(&sm->stats);
		break;
	case CircuitCI:
		lock(&sm->stats);
		sm->stats.ci = s->param.v24Circuit.value;
		unlock(&sm->stats);
		break;
	default:
		break;
	}
}

static void
stat_hook(Softmodem *sm, modemStatusStruct *s)
{
	sm->stats.offhook = !s->param.value;

	if(archhooksw(!s->param.value)) {
		if(softmodemdebug)
			print("stat_hook: ERROR\n");
	}
}

static int
getrate(int rate)
{
	switch (rate) {
        case k75bps: return 75;
        case k300bps: return 300;
        case k600bps: return 600;
        case k1200bps: return 1200;
        case k2400bps: return 2400;
        case k4800bps: return 4800;
        case k7200bps: return 7200;
        case k9600bps: return 9600;
        case k12000bps: return 12000;
        case k14400bps: return 14400;
        case k16800bps: return 16800;
        case k19200bps: return 19200;
        case k21600bps: return 21600;
        case k24000bps: return 24000;
        case k26400bps: return 26400;
        case k28800bps: return 28800;
        case k31200bps: return 31200;
        case k33600bps: return 33600;
        case k36000bps: return 36000;
        default: return 0;
        }
}

static void
stat_rate(Softmodem *sm, modemStatusStruct *s)
{

	switch(s->code) {
	case kTxRateKnown: sm->stats.xmtrate = s->param.value; break;
	case kRxRateKnown: sm->stats.rcvrate = s->param.value; break;
	default:
		break;
	}
}

static void
smstatusproc(Softmodem *sm)
{
	static modemStatusStruct *s;
	char *msg = nil;

	if(softmodemdebug) {
		msg = malloc(1024);
		if(msg == nil)
			pexit("msg malloc", 0);
	}

	s = mallocz(sizeof(*s), 0);
	if(s == nil)
		pexit("status mallocz", 0);

	setpri(PriLocodec);

	for(;;) {
		if(sm->sq == nil || qread(sm->sq, s, sizeof(*s)) <= 0)
			break;

		if(softmodemdebug)
			softmodemdebug(s, msg);

		switch(s->code) {
		default:
			/* print("code %d\n", s->code); /* */
			break;
		case kSpeakerStatus:
			archspeaker(s->param.speakerStatus.enabled, s->param.speakerStatus.volume);
			break;
		case kIOStatus:
			stat_iostatus(sm, s);
			break;
		case kRxRateKnown:
			stat_rate(sm, s);
			break;
		case kTxRateKnown:
			stat_rate(sm, s);
			break;
		case kHookStateChange:
			stat_hook(sm, s);
			break;
		case kATProfileChanged:
			save_nvram(&sm->nvram);
			break;
		case kV24CircuitStatusChange:
			stat_circuit(sm, s);
			break;
		case kResetHardware:
			sm->shutdown.shutdown = 1;
			wakeup(&sm->shutdown);
			break;
		case kConnectionInfo:
			/* if(s->param.connectionInfo.code == kNearEndDelay)
				print("softmodem NearEndDelay: 0x%lux\n", 
					s->param.connectionInfo.value); */
			break;
		}
	}
	free(s);
	pexit("", 0);
}


#ifdef MODEMSTATS

static ulong stats_t0;

void
printmodemstats()
{
	SMStatus *s = &smstatus.stats;
	int nrcv = s->nrcv;
	int nxmt = s->nxmt;
	int rcvrate = getrate(s->rcvrate);
	int xmtrate = getrate(s->xmtrate);
	int rcvbps = 0;
	int xmtbps = 0;
	int rcvpct = 0;
	int xmtpct = 0;
	ulong t = MACHP(0)->ticks-stats_t0;
	t = TK2SEC(t*100);
	if(t) {
		rcvbps = nrcv*1000/t;
		xmtbps = nxmt*1000/t;
		if(rcvrate)
			rcvpct = rcvbps*100/rcvrate;
		if(xmtrate)
			xmtpct = xmtbps*100/xmtrate;
	}
	print("time:%d.%2.2d rcv:%d (%dbps, %d%% of %d)"
			" xmt:%d (%dbps, %d%% of %d)\n",
			t/100, t%100,
			nrcv, rcvbps, rcvpct, rcvrate,
			nxmt, xmtbps, xmtpct, xmtrate);
}


void
clearmodemstats()
{
	SMStatus *s = &smstatus.stats;
	s->nrcv = 0;
	s->nxmt = 0;
	stats_t0 = MACHP(0)->ticks;
}

#endif

