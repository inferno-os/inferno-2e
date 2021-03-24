/*
 * devmodem -
 *
 * An attempt at a fast interrupt driven driver for 16550A compatible
 * uarts/modems.
 */
#include        "u.h"
#include        "../port/lib.h"
#include        "mem.h"
#include        "dat.h"
#include        "fns.h"
#include	"io.h"
#include        "../port/error.h"
#include        "../port/netif.h"

#define GPIO_MDM 0x00000001

/*
 *  Driver for the uart.
 */
enum {
	/* interrupt enable */
	 Ircv=  (1<<0),         /*  for char rcv'd */
	 Ixmt=  (1<<1),         /*  for xmit buffer empty */
	 Irstat=(1<<2),         /*  for change in rcv'er status */
	 Imstat=(1<<3),         /*  for change in modem status */
	/* interrupt flag (read) */
	 Fenabd=(3<<6),         /*  on if fifo's enabled */
	/* fifo control (write) */
	 Fena=  (1<<0),         /*  enable xmit/rcv fifos */
	 Ftrig= (1<<7),         /*  trigger after 8 input characters */
	 Fclear=(3<<1),         /*  clear xmit & rcv fifos */
	/* byte format */
	 Bits8= (3<<0),         /*  8 bits/byte */
	 Stop2= (1<<2),         /*  2 stop bits */
	 Pena=  (1<<3),         /*  generate parity */
	 Peven= (1<<4),         /*  even parity */
	 Pforce=(1<<5),         /*  force parity */
	 Break= (1<<6),         /*  generate a break */
	 Dra=   (1<<7),         /*  address the divisor */
	/* modem control */
	 Dtr=   (1<<0),         /*  data terminal ready */
	 Rts=   (1<<1),         /*  request to send */
	 Ri=    (1<<2),         /*  ring */
	 Inton= (1<<3),         /*  turn on interrupts */
	 Loop=  (1<<4),         /*  loop back */
	/* line status */
	 Inready=(1<<0),        /*  receive buffer full */
	 Oerror=(1<<1),         /*  receiver overrun */
	 Perror=(1<<2),         /*  receiver parity error */
	 Ferror=(1<<3),         /*  rcv framing error */
	 Outready=(1<<5),       /*  output buffer full */
	/* modem status */
	 Ctsc=  (1<<0),         /*  clear to send changed */
	 Dsrc=  (1<<1),         /*  data set ready changed */
	 Rire=  (1<<2),         /*  rising edge of ring indicator */
	 Dcdc=  (1<<3),         /*  data carrier detect changed */
	 Cts=   (1<<4),         /*  complement of clear to send line */
	 Dsr=   (1<<5),         /*  complement of data set ready line */
	 Ring=  (1<<6),         /*  complement of ring indicator line */
	 Dcd=   (1<<7),         /*  complement of data carrier detect line */

	Stagesize= 1024,        /* Interrupt ring buffers .... */
	Nuart=  1,              /* max per machine */
	UartFREQ= 1843200
};

/*
 * This structure is designed so as to overlay the uart registers
 *
 * eg   Uartregs *regs = (Uartregs *)0x0302b800;
 *      regs->thr = data;
 */
typedef struct Uartregs Uartregs;
struct Uartregs {
	union {
		volatile ulong  rbr;    /* receive buffer (R/O) */
		volatile ulong  thr;    /* transmitter holding (W/O) */
		volatile ulong  dll;    /* lsb divisor latch (R/W) */
	} r0;
	union {
		volatile ulong  ier;
		volatile ulong  dlm;
	} r1;
	union {
		volatile ulong  iir;    /* interrupt id (R/O) */
		volatile ulong  fcr;    /* FIFO control (W/O) */
	} r2;
	volatile ulong  lcr;
	volatile ulong  mcr;
	volatile ulong  lsr;
	volatile ulong  msr;
	volatile ulong  scr;
};


typedef struct Uart Uart;
struct Uart
{
	QLock   ql;

	char    name[NAMELEN];

	Uartregs *regs;         /* registers */
	uchar   opens;          /* number of times opened */
	uchar   enabled;        /* uart enabled and configured? */

	int     frame;          /* framing errors */
	int     overrun;        /* rcvr overruns */

	/* flow control */
	int     blocked;
	int     ctsbackoff;
	Rendez  r;

	/* buffers */
	Queue   *iq;
	Queue   *oq;

	/* staging areas to avoid some of the per character costs */
	uchar   istage[Stagesize], ostage[Stagesize];
	uchar   *ip, *ie;
	uchar   *op, *oe;
};

/* global data */
static Uart *uart[Nuart];
static int nuart = 0;

/* Exported functions */
void modemspecial(int, int, Queue **, Queue **, int (*)(Queue*, int)); 

/*
 *  set the baud rate by calculating and setting the baudrate
 *  generator constant.  This will work with fairly non-standard
 *  baud rates.
 */
static void
uartsetbaud(Uart *p, int rate)
{
	ulong brconst;
	Uartregs *r = p->regs;

	if(rate <= 0 | rate > 115200)
		return;

	brconst = (UartFREQ+8*rate-1)/(16*rate);

	r->lcr |= Dra;
	r->r0.dll = brconst;
	r->r1.dlm = brconst >> 8;       /* & 0xff implicit */
	r->lcr &= ~Dra;
}

/*
 *  toggle RTS
 */
static void
uartrts(Uart *p, int n)
{
	int x;
	Uartregs *r = p->regs;

	x = splhi();

	if (n)
		r->mcr |= Rts;
	else
		r->mcr &= ~Rts;

	splx(x);
}

/*
 *  modem flow control on/off (rts/cts)
 */
static void
uartmflow(Uart *p, int n)
{
	if(n){
		p->regs->r1.ier |= Imstat;
		p->regs->r2.fcr = 0x87;  /* Ftrig|Fclear|Fena; */
	} else {
		p->regs->r1.ier &= ~Imstat;
		p->regs->r2.fcr = 0; /* turn off fifo's */
	}
}

/*
 *  turn on a port's interrupts.  set DTR and RTS
 */
static void
uartenable(Uart *p)
{
	Uartregs *r = p->regs;

	if (p->enabled)
		return;

	/*
	 *  turn on DTR and RTS
	 */
	uartrts(p, 1);
	uartmflow(p, 1); 

	/*
	 *  assume we can send
	 */
	p->blocked = 0;
	p->enabled = 1;

	/*
	 *  turn on recieve interrupts
	 */
	r->r1.ier |= Ircv | Irstat;
}

/*
 *  turn off a port's interrupts.  reset DTR and RTS
 */
static void
uartdisable(Uart *p)
{
	Uartregs *r = p->regs;

	/*
	 *  turn off interrupts
	 */
	r->r1.ier = 0;

	/*
	 *  turn off DTR, RTS, hardware flow control & fifo's
	 */
	uartrts(p, 0);
	uartmflow(p, 0);
	p->blocked = 0;
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

	n = qconsume(p->oq, p->ostage, Stagesize);
	if(n <= 0)
		n = 0;
	p->op = p->ostage;
	p->oe = p->ostage + n;
	return n;
}

/*
 *  (re)start output
 */
static void
uartkick(Uart *p)
{
	if ((int)p->op == (int)p->ostage)       /* get some data if needed */
		stageoutput(p);

	p->regs->r1.ier |= Ixmt;        /* Enable Xmit ints */
}

/*
 *  restart input if its off
 */
static void
uartflow(Uart *p)
{
	uartrts(p, 1);
}

/*
 *  called by uartinstall() to create a new uart
 */
static void
uartsetup(Uartregs *r, char *name)
{
	Uart *p;

	/* detect uart */
	print("\nAdding uart \"%s\" at 0x%x: ", name, r);
	do {
		uchar x;

		/* see if the LCR is there */
		r->lcr = 0x1b;
		if ((r->lcr & 0xff) != 0x1b)
			print("WARNING LCR not found");
		r->lcr = 0x03;
		if ((r->lcr & 0xff) != 0x03)
			print("WARNING! LCR not found");

		/* next thing to do is look for the scratch register */
		r->scr = 0x55;
		if ((r->scr & 0xff) != 0x55) {
			print("1");
			break;
		}
		r->scr = 0xAA;
		if ((r->scr & 0xff) != 0xAA) {
			print("1a");
			break;
		}
		
		/* then check if there is a FIFO */
		r->r2.fcr = 0x01;
		x = r->r2.fcr;
		r->r2.fcr = 0x00;
		if ((x & 0x80) == 0) {
			print("Detected 16450 Compatable");
			break;
		}
		if ((x & 0x40) == 0) {
			print("Detected 16550 Compatable");
			break;
		}
		print("Detected 16550A Compatable");

	} while (0);


	/* Is there any space for more uarts? */
	if(nuart >= Nuart)
		panic("devuart: too many uarts");

	p = xalloc(sizeof(Uart));
	uart[nuart++] = p;
	strcpy(p->name, name);
	p->regs = r;

	/*
	 *  set rate to 115200 baud.
	 *  8 bits/character.
	 *  1 stop bit.
	 */
	uartsetbaud(p, 115200);
	p->regs->lcr = Bits8;

	p->iq = qopen(4*1024, 0, (void (*)(void *))uartflow, p);
	p->oq = qopen(4*1024, 0, (void (*)(void *))uartkick, p);

	p->ip = p->istage;
	p->ie = &p->istage[Stagesize];
	p->op = p->oe = p->ostage;
}

/************************************************************/
/*
 * Interrupt handling code
 */
/************************************************************/

/* Transmitter Interrupt Handler */
static void
xmitintr(Uart *p)
{
	int i;
	Uartregs *r = p->regs;

	for (i=0; i<16; i++) { /* fill tx fifo */
		if ((p->op >= p->oe) && !stageoutput(p)) {
			r->r1.ier &= ~Ixmt;
			break; 
		}
		r->r0.thr = *(p->op)++;
	}
}

/* Reciever Interrupt Handler */
static void
recvintr(Uart *p)
{
	do {
		*p->ip++ = (uchar)p->regs->r0.rbr;
	} while (p->regs->lsr & Inready);
}

static void
lineintr(Uart *p)
{
	uchar l;

	l = (uchar)p->regs->lsr;
	if(l & Ferror)
		p->frame++;
	if(l & Oerror)
		p->overrun++;
}

static void
modemintr(Uart *p)
{
	uchar ch;

	ch = (uchar)p->regs->msr;
	if(ch & Ctsc)
		p->ctsbackoff++;
}


static void
uartintr(int dev)
{
	ulong loops;
	Uart *p = uart[dev];
	static void (*ifunc[4])(Uart *) =
		{ modemintr, xmitintr, recvintr, lineintr };
	
	for(loops = 0; loops < 10; loops++){
		int s = p->regs->r2.iir;
		if (s & 1) {    /* is there anything to do ? */
#ifdef DEBUGGING_ONLY 
			if (!loops) { /* bug? interrupt caused but no reason */
				dbg_putchar('!');
				dbg_putbyte(s);
			}
#endif
			return;
		} else
			(ifunc[(s & 0x07) >> 1])(p);
	}

	panic("uartintr");
}

/*
 *  handle an interrupt to a single uart
 */
static void
uartintr0(Ureg *, void *)
{
	*GEDR=GPIO_MDM;	/* ack interrupt in sa1100 */
	uartintr(0);
}

/*
 *  we save up input characters till clock time
 *
 *  There's also a bit of code to get a stalled print going.
 *  It shouldn't happen, but it does.  Obviously I don't
 *  understand something.  Since it was there, I bundled a
 *  restart after flow control with it to give some histeresis
 *  to the hardware flow control.  This makes compressing
 *  modems happier but will probably bother something else.
 *       -- presotto
 */
static void
uartclock(void)
{
	int i;

	for (i=0; i<nuart; i++) {

		int n;
		Uart *p = uart[i];

		if (!p->enabled)
			continue;

		/* process recieved data */
		n = p->ip - p->istage;
		if (n > 0) {
			if (n > (Stagesize))
				panic("uart overrun");
			
			if (p->iq) {     /* add to standard file */
				if (qproduce(p->iq, p->istage, n) < 0) {
					uartrts(p, 0);
					continue;
				}
				print("*");
			}

			/* reset buffer */
			p->ip = p->istage;
		}

		if (p->ctsbackoff) {
			p->ctsbackoff = 0;
			uartkick(p);
		}
	}
}

/*********************************************************************/
/*
 * Installation ...
 */
/*********************************************************************/
static void
uartinstall(void)
{
	static int already = 0;

	if(already)             /* have we already been configured */
		return;
	already = 1;

	/* Add modem uart */
	uartsetup((Uartregs *)0x0a400000, "modem");
	intrenable(0, uartintr0, nil, BusCPU);
	*GRER |= GPIO_MDM;                     /* rising edge */
	*GEDR |= GPIO_MDM;                     /* clear */
	*GPDR &= ~GPIO_MDM;                    /* direction ==> input */
	*GAFR &= ~GPIO_MDM;                    /* alternative */

	/* add clock rutine */
	addclock0link(uartclock);
}

/*********************************************************************/
/*
 * General Inferno Device Interface
 */
/*********************************************************************/
static Dirtab *uartdir;
static int ndir;

static void
setlength(int i)
{
	Uart *p;

	if(i > 0){
		p = uart[i];
		if(p && p->opens && p->iq)
			uartdir[3*i].length = qlen(p->iq);
	} else for(i = 0; i < nuart; i++){
		p = uart[i];
		if(p && p->opens && p->iq)
			uartdir[3*i].length = qlen(p->iq);
	}
		
}

/*
 *  all uarts must be uartsetup() by this point or inside of uartinstall()
 */
static void
modemreset(void)
{
	int i;
	Dirtab *dp;

	uartinstall();  /* architecture specific */

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
modemattach(char *spec)
{
	return devattach('m', spec);
}

static int
modemwalk(Chan *c, char *name)
{
	return devwalk(c, name, uartdir, ndir, devgen);
}

static void
modemstat(Chan *c, char *dp)
{
	if(NETTYPE(c->qid.path) == Ndataqid)
		setlength(NETID(c->qid.path));
	devstat(c, dp, uartdir, ndir, devgen);
}

static Chan*
modemopen(Chan *c, int omode)
{
	Uart *p;

	c = devopen(c, omode, uartdir, ndir, devgen);

	switch(NETTYPE(c->qid.path)){
	case Nctlqid:
	case Ndataqid:
		p = uart[NETID(c->qid.path)];
		qlock(&p->ql);
		if(p->opens++ == 0){
			uartenable(p);
			qreopen(p->iq);
			qreopen(p->oq);
		}
		qunlock(&p->ql);
		break;
	}

	return c;
}

static void
modemclose(Chan *c)
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
		qlock(&p->ql);
		if(--(p->opens) == 0){
			uartdisable(p);
			qclose(p->iq);
			qclose(p->oq);
			p->ip = p->istage;
		}
		qunlock(&p->ql);
		break;
	}
}

static long
uartstatus(Chan *c, Uart *p, void *buf, long n, long offset)
{
	uchar mstat;
	uchar tstat;
	char str[256];

	USED(c);

	str[0] = 0;
	tstat = (uchar)p->regs->mcr;
	mstat = (uchar)p->regs->msr;
	sprint(str, "opens %d ferr %d oerr %d baud %d", p->opens,
		p->frame, p->overrun, 115200);
	if(mstat & Cts)
		strcat(str, " cts");
	if(mstat & Dsr)
		strcat(str, " dsr");
	if(mstat & Ring)
		strcat(str, " ring");
	if(mstat & Dcd)
		strcat(str, " dcd");
	if(tstat & Dtr)
		strcat(str, " dtr");
	if(tstat & Rts)
		strcat(str, " rts");

	strcat(str, "\n");
	return readstr(offset, buf, n, str);
}

static long
modemread(Chan *c, void *buf, long n, ulong offset)
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

	/* let output drain for a while */
	for(i = 0; i < 16 && qlen(p->oq); i++)
		tsleep(&p->r, (int (*)(void *))qlen, p->oq, 125);

	n = atoi(cmd+1);
	switch(*cmd){
	case 'B':
	case 'b':
		uartsetbaud(p, n);
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
	case 'm':
	case 'M':
		uartmflow(p, n);
		break;
	case 'n':
	case 'N':
		qnoblock(p->oq, n);
		break;
	case 'R':
	case 'r':
		uartrts(p, n);
		break;
	case 'Q':
	case 'q':
		qsetlimit(p->iq, n);
		qsetlimit(p->oq, n);
		break;
	}
}

static long
modemwrite(Chan *c, void *buf, long n, ulong offset)
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
	return 0;
}

static void
modemwstat(Chan *c, char *dp)
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


Dev modemdevtab = {
	'm',
	"modem",
	
	modemreset,
	devinit,
	modemattach,
	devdetach,
	devclone,
	modemwalk,
	modemstat,
	modemopen,
	devcreate,
	modemclose,
	modemread,
	devbread,
	modemwrite,
	devbwrite,
	devremove,
	modemwstat,
};
