#include <lib9.h>
#include "io.h"
#include "mem.h"
#include "../port/flash.h"
#include "../port/flashptab.h"
#include "mmap.h"
#include <a.out.h>
#include <styx.h>
#include "mdstyxmon.h"
#include "monconf.h"

enum {
	DEFBPS=	9600,
	LOMEM=	0x8000,
	BOOTADDR= LOMEM,

	CONSOLE_FD= 0,
	OP_NOP= 0xe1a00000,

	SER_PORT= 3,	// standard UART
	DMA_CHAN= 3,	// out of the way; 4 and 5 are risky on older SA1100's
	DMA_MAX= 8191,	// max # of bytes per DMA xfer
};


extern MonMisc monmisc;
extern void reset(void);

static void	hex(char *str, ulong val);
static void 	call(void *addr, Bpi *bp);
static void	bpexit(ulong code);
static void	bpreboot(int cold);
static int	bpunimp(void);
static int	bpexec(void *addr, Bpi *bp, int flags);
static int	bppoll(int timeout);
static void	bpfile2chan(BPChan *nc);
static long	bpwrite(int fd, void *addr, ulong len);
static long	bpread(int fd, void *addr, ulong len);

static char *roargv[] ={
	"?",
	0,
};


// to optimize for size:
long
strlen(char *s)
{
	uchar *e;
	for(e = (uchar*)s; *e; e++)
		;
	return e-(uchar*)s;
}


static void
msgwrite(char *b, int n)
{
	Global *g = G;
	char *buf = (char*)g->bp.msgbuf;
	int p = g->bp.msgptr;
	while(n-- > 0) {
		buf[p++] = *b++;
		if(p >= g->bp.msgsize)
			p = 0;
		buf[p] = '\0';

		// we'll try to let the output flush, but if nobody's
		// ready to read it, we'll just go on and let the
		// buffer wrap around-- we can't wait forever here
		if(p == g->msgpos)
			g->bp.poll(1);
	}
	g->bp.msgptr = p;
}

static int
conscread(BPChan *c, uchar *b, long n, long)
{	
	Global *g = G;
	char *buf = (char*)g->bp.msgbuf;
	int p = g->msgpos;
	int r = 0;
	while(p != g->bp.msgptr && n-- > 0) {
		*b++ = buf[p++];
		if(p >= g->bp.msgsize)
			p = 0;
		r++;
	}
	g->msgpos = p;
	return (r == 0 && c->d.type != 0) ? BPCHAN_BLOCKED : r;
}

static int
conscwrite(BPChan *, uchar *b, long n, long)
{
	Global *g = G;
	uchar *buf = (uchar*)g->inpbuf;
	int p = g->inpin;
	int r = 0;
	while(n-- > 0) {
		buf[p++] = *b++;
		if(p >= sizeof g->inpbuf)
			p = 0;
		++r;
	}
	g->inpin = p;
	return r;
}

static int
memcread(BPChan *, uchar *b, long n, long ofs)
{
	memcpy(b, (uchar*)ofs, n);
	return n;
}

static int
memcwrite(BPChan *, uchar *b, long n, long ofs)
{
	memcpy((char*)ofs, b, n);
	return n;
}

static void
msgbufinit(int show)
{
	Global *g = G;
	Bpi *bp = &g->bp;
	uchar *buf = (uchar*)g->msgbuf;
	int p = strlen((char*)buf);
	if(p > sizeof g->msgbuf)
		p = 0;
	bp->msgbuf = (ulong)g->msgbuf;
	bp->msgsize = sizeof g->msgbuf;
	bp->msgptr = p;
	if(show) {
		if(++p >= sizeof g->msgbuf)
			p = 0;
	}
	g->msgpos = p;
}

static void fsinit(int attached);

static int
ctlcwrite(BPChan *, uchar *buf, long len, long)
{
	Global *g = G;
	int cmd;
	int r = 10;
	long x = 0;
	int n = len;
	uchar *b = buf;
	if(n < 1)
		return -1;
	cmd = *b++;
	while(--n > 0) {
		int ch = *b++;
		if(ch <= 32)
			break;
		if(ch == 'x') {
			r = 16;
			continue;
		}
		ch -= '0';
		if(ch > 9)
			ch = (ch&0x7)+9;
		x = x*r + ch;
	}
	switch(cmd) {
	case 'E':
	case 'S':
		if(x == 0)
			x = *(ulong*)(g->bp.flashbase+FLASH_BOOT_OFS_OFS);
		x += g->bp.flashbase;
		cmd += 'a'-'A';
	case 'e':
	case 's':
	case 'r':
	case 'x':
		g->cmd = cmd;
		g->val = x;
		break;
	case 'b':
		g->bps = x;
		break;
	case 'C':
		msgbufinit(1);
		break;
	/* temporary, for debugging:
	case 'p':
		g->bp.write(CONSOLE_FD, buf+1, len-1);
		break;
	case '=':
		print("\1 ", *(ulong*)x);
	case '.':
		print("\1\n", x);
		break;
	*/
	default:
		return -1;
	}
	return len;
}

static int
bootcopen(BPChan *, int)
{
	G->bootsize = 0;
	return 0;
}

static int
bootcwrite(BPChan *, uchar *b, long n, long ofs)
{
	Global *g = G;
	char *bp = (char*)BOOTADDR;
	memcpy(bp+ofs, b, n);
	ofs += n;
	if(ofs > g->bootsize)
		g->bootsize = ofs;
	return n;
}

static void
bootcclunk(BPChan *)
{
	Global *g = G;
/*
	ulong sum = 0;
	uchar *p = (uchar*)BOOTADDR;
	uchar *e = p+g->bootsize;
*/
	g->cmd = 'e';
	g->val = BOOTADDR;
/*
	while(p < e)
		sum += *p++;
	sum = (sum+(sum>>16))&0xffff;
	print("sum: \2\n", sum);
*/
}

static BPChan	rorootdir = {
	{"/", "none", "none", {0}, CHDIR|0555},
	0, 0, rootdirread, 0, 0,
};

static BPChan	roconschan = {
	{"cons", "none", "none", {0}, 0666, 4737620, 108950400, .type 1},
	0, 0, conscread, conscwrite, 0,
};

static BPChan	roctlchan = {
	{"ctl", "none", "none", {0}, 0222, 4343110, 41212800},
	0, 0, conscread, ctlcwrite, 0,
};

static BPChan	romemchan = {
	{"mem", "none", "none", {0}, 0666, 4539716, 48038400 },
	0, 0, memcread, memcwrite, 0,
};

static BPChan	robootchan = {
	{"boot", "none", "none", {0}, 0222, 5197897, 941068800},
	bootcopen, bootcclunk, 0, bootcwrite, 0,
};


static void
rofile2chan(BPChan *nc, BPChan *ro)
{
	memmove(nc, ro, sizeof(BPChan));
	bpfile2chan(nc);
}

static void
fsinit(int attached)
{
	Global *g = G;
	Fid *fid = g->fidpool;
	Fid *efid = fid+nelem(g->fidpool);

	g->head = g->tail = nil;
	rofile2chan(&g->rootdir, &rorootdir);
	g->rootdir.d.qid.path |= CHDIR;
	rofile2chan(&g->conschan, &roconschan);
	rofile2chan(&g->ctlchan, &roctlchan);
	rofile2chan(&g->memchan, &romemchan);
	rofile2chan(&g->bootchan, &robootchan);

	// clear out invalid fids:
	print("fids: ");
	for(; fid < efid; fid++) {
		BPChan *fn = nil;
		if(fid->node != nil) {
			if(attached) {
				BPChan *p;
				for(p=g->head; p != nil; p = p->link)
					if(p == fid->node) {
						fn = p;
						break;
					}
			}
			if(!fn)
				putc('!');
			print("\1:\4 ", fid->fid, fid->node->d.name); 
		}
		fid->node = fn;
	}
	putc('\n');
}

static void
serinit(void)
{
	UartReg *uart = UARTREG(SER_PORT);
	int bdiv;
	int r = TIMER_HZ;
	int bps;
	Global *g = G;

	while(uart->utsr1 & UTSR1_TBY)
		;			// finish flushing TX FIFO

	bps = g->bps;
	if(!bps)			// bps set to 0 signals hard reboot
		bpreboot(g->baddr);
	g->bp.bps = bps;
	bps <<= 4;
	for(bdiv = -1; r; bdiv++)
		r -= bps;

	uart->utcr3 = 0;		// disable ints/RX/TX
	uart->utsr0 = 0xff;		// clear sticky bits in status register
	uart->utcr0 = 0x08;		// 8 bit data, no parity, 1 stop bit
	uart->utcr1 = bdiv>>8;		// bps div high
	uart->utcr2 = bdiv&0xff;	// bps div low
	uart->utcr3 = 0x3;		// enable TX/RX
}

static int
serwrite(void *buf, int n)
{
	UartReg *uart = UARTREG(SER_PORT);
	uchar *b = buf;
	uchar *e = b+n;
	while(b < e) {
		while(!(uart->utsr1 & 4))
			;
		uart->utdr = *b++;
	}
	return n;
}

void status(ulong v)
{
	uchar bb[3];
	bb[0] = '\0';
	bb[1] = v;
	bb[2] = v>>8;
	serwrite(bb, 3);
}	

static int
serrecv(void *buf, int n, int ms)
{
	UartReg *uart = UARTREG(SER_PORT);
	uchar *b = buf;
	uchar *e = b+n;
	OstmrReg *ost = OSTMRREG;
	ulong t0 = ost->oscr;
	int td = ms*(TIMER_HZ/1000);
	ulong t;
	DmaReg *dma = DMAREG(DMA_CHAN);

	t = dma->dcsr;
	dma->dcsr_c = DCSR_RUN|DCSR_IE|DCSR_ERROR|DCSR_STRTA|DCSR_DONEA
				|DCSR_STRTB|DCSR_DONEB|DCSR_BIU;
	if(t & (DCSR_STRTA|DCSR_DONEA)) {
		ulong ds = dma->dbsb;
		t = dma->dbsa-ds;
		if(t) {
			memmove(b, (void*)ds, t);
			b += t;
			td = 0;
		}
	}
	while(b < e) {
		while((uart->utsr1 & 2) == 0)
			if(td && (ost->oscr-t0) > td) {
				n = 0;
				e = b;
				goto done;
			}
		td = 0;
		*b++ = uart->utdr;
	}
done:
	dma->ddar = 0x81400591;		/* serial port 3 - UART, read */
	dma->dbsb = (ulong)e;
	dma->dbsa = (ulong)b;
	dma->dbta = DMA_MAX;
	dma->dcsr_s = DCSR_STRTA|DCSR_RUN;

	return n;
}

/*
static void
serputs(char *str)
{
	char *e;
	for(e=str; *e; e++) ;
	serwrite(str, e-str);
}
*/

int
send(void *buf, int n)
{
	return G->bp.send(buf, n);
}

int
recv(void *buf, int n)
{
	return G->bp.recv(buf, n, 0);
}


static void
hangout(void)
{
	Bpi *bp = &G->bp;
	print("*\n");
	bp->flags = BP_FLAG_DEBUG;
	for(;;)
		bppoll(0);
}

// report an error if the Global structure grows into the page table:
struct error_check {
	char global_too_large[(ulong)(G+1) < PAGETABLE_OFS];
};

extern char monname[];

void
main(ulong himem)
{
	Global *g = G;
	ulong flashbase;
	ulong bootaddr = 0;
	Bpi *bp;
	GpioReg *gpio = GPIOREG;

	gpio->gafr = monmisc.gafr;
	gpio->gpdr = monmisc.gpdr;
	gpio->gpsr = monmisc.gpsr;
	gpio->gpcr = ~monmisc.gpsr;

	memset(g, 0, (sizeof *g) - (sizeof g->msgbuf));

	g->bps = DEFBPS;
	serinit();

	 // bootparam initialization
	bp = &g->bp;
	bp->bootver_major = monmisc.monver_major;
	bp->bootver_minor = monmisc.monver_minor;
	bp->bpi_major = Bpi_ver_major;
	bp->bpi_minor = Bpi_ver_minor;
	bp->bootname = monname;
	bp->argc = nelem(roargv)-1;
	bp->argv = roargv;
	bp->envp = roargv+nelem(roargv)-1;
	bp->lomem = LOMEM;
	bp->himem = himem;
	bp->exit = bpexit;
	bp->reboot = bpreboot;
	bp->open = (int (*)(const char*, int))bpunimp;
	bp->close = (int (*)(int))bpunimp;
	bp->write = bpwrite;
	bp->read = bpread;
	bp->fstat = (int (*)(int, BpStat*))bpunimp;
	bp->seek = (int (*)(int, ulong))bpunimp;
	bp->file2chan = bpfile2chan;
	bp->poll = bppoll;
	bp->exec = bpexec;
	bp->console_fd = CONSOLE_FD;

	msgbufinit(0);

	 // plat-bootparam members
	flashbase = monmisc.flashbase;
	if(*(ulong*)(flashbase+FLASHPTAB_MAGIC_OFS) != FLASHPTAB_MAGIC)
		flashbase = (ulong)reset;

	bp->flashbase = flashbase;
	bp->cpuspeed = monmisc.cpuspeed_hz;
	bp->pagetable = 0x4000;
	bp->send = serwrite;
	bp->recv = serrecv;

	if(*(ulong*)(flashbase+FLASHPTAB_MAGIC_OFS) == FLASHPTAB_MAGIC)
		bootaddr = *(ulong*)(flashbase+FLASH_AUTOBOOT_OFS_OFS);

	print("\n\4 \2.\2\3  \2MHz \2/\2MB himem:\1 flash:\1 auto:\1\n",
		monname, monmisc.monver_major, monmisc.monver_minor,
		monmisc.monver_patch,
		(((monmisc.cpuspeed_hz+(1<<9))>>10)*1073+(1<<19)) >> 20,
		(himem>>20)+1, *((ulong*)RAMTESTED_SIZE_OFS),
		himem, flashbase, bootaddr);

	if((*(ulong*)(monmisc.noauto_addr) & monmisc.noauto_mask)
			== monmisc.noauto_val
		|| *(ulong*)(DEBUGBOOT_MAGIC_OFS) == DEBUGBOOT_MAGIC)
		print("noauto\n");
	else if(bootaddr) 
		bpexec((void*)(flashbase+bootaddr), bp, 0);

	hangout();
}

void
firstattach(int fid)
{
	G->bp.rootfid = fid;
	fsinit(0);
}

void
lastclunk(void)
{	
	G->bp.bps = 0;		// causes serial re-init
}

static int
bppoll(int ms)
{
	Global *g = G;
	ulong x;
	int c;

	if(!(g->bp.flags & BP_FLAG_DEBUG))
		return 0;

	for(;;) {
		uchar *buf = g->buf+(((ulong*)g->bp.pagetable)[0] & 0xfff00000);
		if(!g->bp.recv(buf, 1, 1)) {
			unblock(buf);
			if(!g->bp.recv(buf, 1, ms))
				break;
		}
		convM2S((char*)buf, &g->fc, 1);
		g->bp.recv(buf, 0, 0);
		if(!fcall(buf, &g->fc)) {
			g->bps = DEFBPS;
			g->bp.bps = 0;
		}
		ms = 1;
	}

	x = g->val;
	c = g->cmd;
	g->cmd = 0;
	if(c == 'x')
		bpexit(x);
	if(c == 'r') {
		g->baddr = x;
		g->bps = 0;
	}
	if(c == 'e')
		g->bp.exec((void*)x, &g->bp, BP_EXEC_FLAG_SYSMODE);
	if(c == 's')
		call((void*)x, &g->bp);

	if(!g->bp.bps) 
		serinit();
	return 0;
}

static void
bpfile2chan(BPChan *nc)
{
	Global *g = G;
	BPChan *c = g->head;
	BPChan *p = 0;

	if(nc->read == nil && nc->write == nil) {		// delete?
		for(;c != nc; p = c, c = c->link) 
			if(c == nil)
				return;
		if(p == nil)
			g->head = c->link;
		else {
			p->link = c->link;
			if(c == g->tail)
				g->tail = p;
		}
		return;
	}

	nc->link = nil;
	if(g->head == nil)
		g->head = g->tail = nc;
	else {
		g->tail->link = nc;
		g->tail = nc;
	}

	nc->d.qid.path = ++g->nextqid;
	nc->d.dev = 'X';
}

static void	
bpexit(ulong)
{
	fsinit(1);
	hangout();
}

static void	
bpreboot(int cold)
{
	if(cold > 1) {
		// set up debug alternate boot vector:
		ulong *b = (ulong*)DBGALTBOOT_MAGIC_OFS;
		*b = DBGALTBOOT_MAGIC;
		b[(ulong*)DBGALTBOOT_ADDR_OFS-(ulong*)DBGALTBOOT_MAGIC_OFS] = cold;
	}
	if(cold)
		RESETREG->rsrr = 1;
	else 
		main(G->bp.himem);
}

static int	
bpunimp(void)
{
	return -1;
}

static void
call(void *addr, Bpi *bp)
{
	print("\1(\1)\n", (ulong)addr, (ulong)bp);
	bp->entry = (long)addr;

	// wait "long enough" for any expected Styx messages
	// (such as the Tread that follows the last Twrite we're about
	// to send)
	bp->poll(500); 

	// Note: DMA is still enabled on the UART when this program
	// is invoked...  the program needs to disable it if it
	// won't be using StyxMon

	// and off we go...
	((void (*)(Bpi*))addr)(bp);
}

static ulong
lget(uchar *p)
{
	return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|p[3];
}

static int
bpexec(void *addr, Bpi *bp, int)
{
	uchar *p = (uchar*)addr;
	ulong *dest;
	ulong *src = (ulong*)addr;
	ulong n, len;
	ulong text, data, bss;
	ulong entry;
	if((n = lget(p)) == E_MAGIC) {
		print("p9: ");
		text = lget(p+4);
		data = lget(p+8);
		bss = lget(p+16);
		entry = lget(p+20);
		n = 32+text+data;
		len = n+bss;
		dest = (ulong*)bp->lomem;
	} else if(*(ulong*)addr == OP_NOP) {
		ulong *hdr = (ulong*)addr;
		print("aif: ");
		text = hdr[5];
		data = hdr[6];
		bss = hdr[8];
		dest = (ulong*)hdr[10];
		n = text+data;
		len = n+bss;
		entry = (ulong)dest;
	} else {
		print("\1?\n", n);
		return -1;
	}
	if(bp->lomem+len > bp->himem) 
		return -1;

	print("\1: \1+\1+\1\n", dest, text, data, bss);

	memmove(dest, src, n);
	memset((uchar*)dest+n, bss, 0);
	segflush((void*)bp->lomem, len);

	fsinit(1);

	call((void*)entry, bp);
	return -1;
}

static long
bpwrite(int fd, void *addr, ulong len)
{
	if(fd == CONSOLE_FD) {
		msgwrite((char*)addr, len);
	} else
		return -1;
}

static long
bpread(int fd, void *addr, ulong len)
{
	Global *g = G;
	if(fd == CONSOLE_FD) {
		uchar *b = (uchar*)addr;
		int p = g->inpout;
		int r = 0;
		while(g->inpin == p)
			g->bp.poll(1); 
		while(len-- > 0 && p != g->inpin) {
			*b++ = g->inpbuf[p++];
			if(p >= sizeof g->inpbuf)
				p = 0;
			r++;
		}
		g->inpout = p;
		return r;
	} else
		return -1;
}

void
putc(int c)
{
	char ch = c;
	msgwrite(&ch, 1);
}

static ulong _putdec(ulong v, ulong m)
{
	ulong b = m*10;
	int d;
	if(v >= b)
		v = _putdec(v, b);
	for(d = '0'; v >= m; v -= m)
		++d;
	putc(d);
	return v;
}

static void putdec(ulong v)
{
	/* note: this code will break for v >= 1,000,000,000 */
	_putdec(v, 1);
}

static void
puthex(ulong v)
{
	int i;
	print("0x");
	for(i=28; i>=0; i-=4)
		putc("0123456789abcdef"[(v>>i)&0xf]);
}

int
print(char *f, ...)
{
	va_list arg;
	char *s;
	static void (*fp[4])(ulong) = {
		puthex,putdec,(void (*)(ulong))putc,(void (*)(ulong))print
	};
	va_start(arg, f);
	for(s=f; ; f++) {
		int c = *f;
		if(c <= 4) {
			msgwrite(s, f-s);
			if(!c)
				break;
			fp[c-1](va_arg(arg, ulong));
			s = f+1;
		}
	}
	va_end(arg);
	return 0;
}


void
swi(int n)
{
	print("swi: \1\n", n);
}

