#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"../port/error.h"
#include	"../port/netif.h"

/*
 * currently no DMA or flow control (hardware or software)
 */

/*
 * problems fixed from previous vsn:
 *
 *	- no kick on queue's, so redirections weren't getting
 *	  started until the clock tick
 *
 *	- lots of unnecessary overhead
 *
 *	- initialization sequencing
 *
 *	- uart[n] no longer indexed before calling uartinstall()
 */

static void uartintr(Ureg*, void*);

enum
{
	Stagesize= 1024,
	Dmabufsize=Stagesize/2,
	Nuart=7,		/* max per machine */
};

typedef struct Uart Uart;
struct Uart
{
	QLock;

	int	opens;

	int	enabled;

	int	frame;		/* framing errors */
	int	overrun;	/* rcvr overruns */
	int	perror;		/* parity error */
	int	bps;		/* baud rate */
	uchar	bits;
	char	parity;

	int	inters;		/* total interrupt count */
	int	rinters;	/* interrupts due to read */
	int	winters;	/* interrupts due to write */

	int	rcount;		/* total read count */
	int	wcount;		/* total output count */

	/* buffers */
	int	(*putc)(Queue*, int);
	Queue	*iq;
	Queue	*oq;

	UartReg	*reg;

	/* staging areas to avoid some of the per character costs */
	uchar	*ip;
	uchar	*ie;
	uchar	*op;
	uchar	*oe;

	/* put large buffers last to aid register-offset optimizations: */
	char	name[NAMELEN];
	uchar	istage[Stagesize];
	uchar	ostage[Stagesize];
};


static Uart *uart[Nuart];
static int nuart;


static void
uartset(Uart *p)
{
	UartReg *reg = p->reg;
	ulong ocr3;
	ulong brdiv;
	int n;

	brdiv = TIMER_HZ/16/p->bps - 1;
	ocr3 = reg->utcr3;
	reg->utcr3 = ocr3&~(UTCR3_RXE|UTCR3_TXE);
	reg->utcr1 = brdiv >> 8;
	reg->utcr2 = brdiv & 0xff;
	/* set PE and OES appropriately for o/e/n: */
	reg->utcr0 = ((p->parity&3)^UTCR0_OES)|(p->bits&UTCR0_DSS);
	reg->utcr3 = ocr3;

	/* set buffer length according to speed, to allow
	 * at most a 200ms delay before dumping the staging buffer
	 * into the input queue
	 */
	n = p->bps/(10*1000/200);
	p->ie = &p->istage[n < Stagesize ? n : Stagesize];
}

/*
 *  send break
 */
static void
uartbreak(Uart *p, int ms)
{
	UartReg *reg = p->reg;
	if(ms == 0)
		ms = 200;
	reg->utcr3 |= UTCR3_BRK;
	tsleep(&up->sleep, return0, 0, ms);
	reg->utcr3 &= ~UTCR3_BRK;
}

/*
 *  turn on a port
 */
static void
uartenable(Uart *p)
{
	UartReg *reg = p->reg;

	if(p->enabled)
		return;

	uartset(p);
	reg->utsr0 = 0xff;		// clear all sticky status bits
	// enable receive, transmit, and receive interrupt:
	reg->utcr3 = UTCR3_RXE|UTCR3_TXE|UTCR3_RIM;
	p->enabled = 1;
}

/*
 *  turn off a port
 */
static void
uartdisable(Uart *p)
{
	p->reg->utcr3 = 0;		// disable TX, RX, and ints
	p->enabled = 0;
}

/*
 *  put some bytes into the local queue to avoid calling
 *  qconsume for every character
 */
static int
stageoutput(Uart *p)
{
	int n;
	Queue *q = p->oq;

	if(!q)
		return 0;
	n = qconsume(q, p->ostage, Stagesize);
	if(n <= 0)
		return 0;
	p->op = p->ostage;
	p->oe = p->ostage + n;
	return n;
}

static void
uartxmit(Uart *p)
{
	UartReg *reg = p->reg;
	ulong e = 0;
	while(p->op < p->oe || stageoutput(p)) {	
		if(reg->utsr1 & UTSR1_TNF) {
			reg->utdr = *(p->op++);
			p->wcount++;
		} else {
			e = UTCR3_TIM;
			break;
		}
	}
	reg->utcr3 = (reg->utcr3&~UTCR3_TIM)|e;
}

static void
uartrecvq(Uart *p)
{
	uchar *cp = p->istage;
	int n = p->ip - cp;

	if(n == 0)
		return;
	if(p->putc)
		while(n-- > 0) 
			p->putc(p->iq, *cp++);
	else if(p->iq) 
		if(qproduce(p->iq, p->istage, n) < n)
			print("qproduce flow control");
	p->ip = p->istage;
}

static void
uartrecv(Uart *p)
{
	UartReg *reg = p->reg;
	ulong n;
	while(reg->utsr1 & UTSR1_RNE) {
		int c;
		n = reg->utsr1;
		c = reg->utdr;
		if(n & (UTSR1_PRE|UTSR1_FRE|UTSR1_ROR)) {
			if(n & UTSR1_PRE) 
				p->perror++;
			if(n & UTSR1_FRE) 
				p->frame++;
			if(n & UTSR1_ROR) 
				p->overrun++;
			continue;
		}
		*p->ip++ = c;
		if(p->ip >= p->ie)
			uartrecvq(p);
		p->rcount++;
	}
	if(reg->utsr0 & UTSR0_RID) {
		reg->utsr0 = UTSR0_RID;
		uartrecvq(p);
	}
}

static void
uartkick(Uart *p)
{
	int x = splhi();
	uartxmit(p);
	splx(x);
}

/*
 *  UART Interrupt Handler
 */
static void
uartintr(Ureg*, void* arg)
{
	Uart *p = arg;			
	UartReg *reg = p->reg;
	ulong m = reg->utsr0;

	p->inters++;
	if(m & (UTSR0_RFS|UTSR0_RID|UTSR0_EIF)) {
		p->rinters++;
		uartrecv(p);
	}
	if((m & UTSR0_TFS) && (reg->utcr3&UTCR3_TIM)) {
		p->winters++;
		uartxmit(p);
	}

	if(m & (UTSR0_RBB|UTSR0_REB)) {
		print("<BREAK>");
		reg->utsr0 = UTSR0_RBB|UTSR0_REB;

		/* hangup?  this could adversely affect some things,
		    like the IR keyboard... what is appropriate to do here? 
			qhangup(p->iq, 0);
		 */
	}
}


static void
uartsetup(ulong port, char *name)
{
	Uart *p;

	if(nuart >= Nuart)
		return;

	p = xalloc(sizeof(Uart));
	uart[nuart++] = p;
	strcpy(p->name, name);

	p->reg = UARTREG(port);
	p->bps = 9600;
	p->bits = 8;
	p->parity = 'n';

	p->iq = qopen(4*1024, 0, 0 , p);
	p->oq = qopen(4*1024, 0, uartkick, p);

	p->ip = p->istage;
	p->ie = &p->istage[Stagesize];
	p->op = p->ostage;
	p->oe = p->ostage;

	intrenable(UARTbit(port), uartintr, p, BusCPU); 
}

static void
uartinstall(void)
{
	static int already;

	if(already)
		return;
	already = 1;

	uartsetup(3, "eia0");
	uartsetup(2, "eia1");
}

/*
 *  called by main() to configure a duart port as a console or a mouse
 */
void
uartspecial(int port, int bps, char parity, Queue **in, Queue **out, int (*putc)(Queue*, int))
{
	Uart *p;

	uartinstall();
	if(port >= nuart) 
		return;
	p = uart[port];
	if(bps) 
		p->bps = bps;
	if(parity)
		p->parity = parity;
	uartenable(p);
	p->putc = putc;
	if(in)
		*in = p->iq;
	if(out)
		*out = p->oq;
	p->opens++;
}

Dirtab *uartdir;
int ndir;

static void
setlength(int n)
{
	Uart *p;
	int i = n;
	if(n < 0) {
		i = 0;
		n = nuart;
	}
	for(; i <= n; i++) {
		p = uart[i];
		if(p && p->opens && p->iq)
			uartdir[3*i].length = qlen(p->iq);
	}
}

/*
 *  all uarts must be uartsetup() by this point or inside of uartinstall()
 */
static void
uartreset(void)
{
	int i;
	Dirtab *dp;

	uartinstall();

	ndir = 3*nuart;
	uartdir = xalloc(ndir * sizeof(Dirtab));
	dp = uartdir;
	for(i = 0; i < nuart; i++){
		/* 3 directory entries per port */
		strcpy(dp->name, uart[i]->name);
		dp->qid.path = NETQID(i, Ndataqid);
		dp->perm = 0660;
		dp++;
		sprint(dp->name, "%sctl", uart[i]->name);
		dp->qid.path = NETQID(i, Nctlqid);
		dp->perm = 0660;
		dp++;
		sprint(dp->name, "%sstat", uart[i]->name);
		dp->qid.path = NETQID(i, Nstatqid);
		dp->perm = 0444;
		dp++;
	}
}

static Chan*
uartattach(char *spec)
{
	return devattach('t', spec);
}

static int
uartwalk(Chan *c, char *name)
{
	return devwalk(c, name, uartdir, ndir, devgen);
}

static void
uartstat(Chan *c, char *dp)
{
	if(NETTYPE(c->qid.path) == Ndataqid)
		setlength(NETID(c->qid.path));
	devstat(c, dp, uartdir, ndir, devgen);
}

static Chan*
uartopen(Chan *c, int omode)
{
	Uart *p;

	c = devopen(c, omode, uartdir, ndir, devgen);

	switch(NETTYPE(c->qid.path)){
	case Nctlqid:
	case Ndataqid:
		p = uart[NETID(c->qid.path)];
		qlock(p);
		if(p->opens++ == 0){
			uartenable(p);
			qreopen(p->iq);
			qreopen(p->oq);
		}
		qunlock(p);
		break;
	}

	return c;
}

static void
uartclose(Chan *c)
{
	Uart *p;

	if(c->qid.path & CHDIR)
		return;
	if((c->flag & COPEN) == 0)
		return;
	switch(NETTYPE(c->qid.path)){
	case Ndataqid:
	case Nctlqid:
		p = uart[NETID(c->qid.path)];
		qlock(p);
		if(--(p->opens) == 0){
			uartdisable(p);
			qclose(p->iq);
			qclose(p->oq);
			p->ip = p->istage;
		}
		qunlock(p);
		break;
	}
}

static long
uartstatus(Chan *c, Uart *p, void *buf, long n, long offset)
{
	char str[256];
	USED(c);

	str[0] = 0;
	sprint(str, "opens %d ferr %d oerr %d perr %d baud %d parity %c"
			" intr %d rintr %d wintr %d"
			" rcount %d wcount %d",
		p->opens, p->frame, p->overrun, p->perror, p->bps, p->parity,
		p->inters, p->rinters, p->winters,
		p->rcount, p->wcount);

	strcat(str, "\n");
	return readstr(offset, buf, n, str);
}

static long
uartread(Chan *c, void *buf, long n, ulong offset)
{
	Uart *p;

	if(c->qid.path & CHDIR){
		setlength(-1);
		return devdirread(c, buf, n, uartdir, ndir, devgen);
	}

	p = uart[NETID(c->qid.path)];
	switch(NETTYPE(c->qid.path)){
	case Ndataqid:
		return qread(p->iq, buf, n);
	case Nctlqid:
		return readnum(offset, buf, n, NETID(c->qid.path), NUMSIZE);
	case Nstatqid:
		return uartstatus(c, p, buf, n, offset);
	}

	return 0;
}

static void
uartctl(Uart *p, char *cmd)
{
	int i, n;

	/* let output drain for a while (up to 4 secs) */
	for(i = 0; i < 200 && (qlen(p->oq) || p->reg->utsr1 & UTSR1_TBY); i++)
		tsleep(&up->sleep, return0, 0, 20);

	if(strncmp(cmd, "break", 5) == 0){
		uartbreak(p, 0);
		return;
	}

	n = atoi(cmd+1);
	switch(*cmd){
	case 'B':
	case 'b':
		if(n <= 0) 
			error(Ebadarg);
		p->bps = n;
		uartset(p);
		break;
	case 'f':
	case 'F':
		qflush(p->oq);
		break;
	case 'H':
	case 'h':
		qhangup(p->iq, 0);
		qhangup(p->oq, 0);
		break;
	case 'L':
	case 'l':
		if(n < 7 || n > 8)
			error(Ebadarg);
		p->bits = n;
		uartset(p);
		break;
	case 'n':
	case 'N':
		qnoblock(p->oq, n);
		break;
	case 'P':
	case 'p':
		p->parity = *(cmd+1);
		uartset(p);
		break;
	case 'K':
	case 'k':
		uartbreak(p, n);
		break;
	case 'Q':
	case 'q':
		qsetlimit(p->iq, n);
		qsetlimit(p->oq, n);
		break;
	}
}

static long
uartwrite(Chan *c, void *buf, long n, ulong offset)
{
	Uart *p;
	char cmd[32];

	USED(offset);

	if(c->qid.path & CHDIR)
		error(Eperm);

	p = uart[NETID(c->qid.path)];

	switch(NETTYPE(c->qid.path)){
	case Ndataqid:
		return qwrite(p->oq, buf, n);
	case Nctlqid:

		if(n >= sizeof(cmd))
			n = sizeof(cmd)-1;
		memmove(cmd, buf, n);
		cmd[n] = 0;
		uartctl(p, cmd);
		return n;
	}
}

static void
uartwstat(Chan *c, char *dp)
{
	Dir d;
	Dirtab *dt;

	if(!iseve())
		error(Eperm);
	if(CHDIR & c->qid.path)
		error(Eperm);
	if(NETTYPE(c->qid.path) == Nstatqid)
		error(Eperm);

	dt = &uartdir[3 * NETID(c->qid.path)];
	convM2D(dp, &d);
	d.mode &= 0666;
	dt[0].perm = dt[1].perm = d.mode;
}

Dev uartdevtab = {
	't',
	"uart",

	uartreset,
	devinit,
	uartattach,
	devdetach,
	devclone,
	uartwalk,
	uartstat,
	uartopen,
	devcreate,
	uartclose,
	uartread,
	devbread,
	uartwrite,
	devbwrite,
	devremove,
	uartwstat,
};
