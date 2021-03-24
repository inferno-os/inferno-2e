#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"ureg.h"

extern int main_pool_pcnt;
extern int heap_pool_pcnt;
extern int image_pool_pcnt;

Proc *up;
Mach *m;

static  uchar *sp;	/* stack pointer for /boot */

/*
 * Where configuration info is left for the loaded programme.
 * This will turn into a structure as more is done by the boot loader
 * (e.g. why parse the .ini file twice?).
 * There are 1024 bytes available at CONFADDR.
 */
#define BOOTLINE	((char*)CONFADDR)
#define BOOTLINELEN	64
#define BOOTARGS	((char*)(CONFADDR+BOOTLINELEN))
#define	BOOTARGSLEN	(1024-BOOTLINELEN)
#define	MAXCONF		32

char bootdisk[NAMELEN];
char *confname[MAXCONF];
char *confval[MAXCONF];
int nconf;

extern void ns16552install(void);	/* botch: config */

static int isoldbcom;

static int
getcfields(char* lp, char** fields, int n, char* sep)
{
	int i;

	for(i = 0; lp && *lp && i < n; i++){
		while(*lp && strchr(sep, *lp) != 0)
			*lp++ = 0;
		if(*lp == 0)
			break;
		fields[i] = lp;
		while(*lp && strchr(sep, *lp) == 0){
			if(*lp == '\\' && *(lp+1) == '\n')
				*lp++ = ' ';
			lp++;
		}
	}

	return i;
}

static void
options(void)
{
	uchar *bda;
	long i, n;
	char *cp, *line[MAXCONF], *p, *q;

	if(strncmp(BOOTARGS, "ZORT 0\r\n", 8)){
		isoldbcom = 1;

		memmove(BOOTARGS, KADDR(1024), BOOTARGSLEN);
		memmove(BOOTLINE, KADDR(0x100), BOOTLINELEN);

		bda = KADDR(0x400);
		bda[0x13] = 639;
		bda[0x14] = 639>>8;
	}

	/*
	 *  parse configuration args from dos file plan9.ini
	 */
	cp = BOOTARGS;	/* where b.com leaves its config */
	cp[BOOTARGSLEN-1] = 0;

	/*
	 * Strip out '\r', change '\t' -> ' '.
	 */
	p = cp;
	for(q = cp; *q; q++){
		if(*q == '\r')
			continue;
		if(*q == '\t')
			*q = ' ';
		*p++ = *q;
	}
	*p = 0;

	n = getcfields(cp, line, MAXCONF, "\n");
	for(i = 0; i < n; i++){
		if(*line[i] == '#')
			continue;
		cp = strchr(line[i], '=');
		if(cp == 0)
			continue;
		*cp++ = 0;
		if(cp - line[i] >= NAMELEN+1)
			*(line[i]+NAMELEN-1) = 0;
		confname[nconf] = line[i];
		confval[nconf] = cp;
		nconf++;
	}
}

static void
doc(char *m)
{
	int i;
	print("%s...\n", m);
	for(i = 0; i < 10*1024*1024; i++)
		i++;
}


void
main(void)
{
	outb(0x3F2, 0x00);			/* botch: turn off the floppy motor */

	/*
	 * There is a little leeway here in the ordering but care must be
	 * taken with dependencies:
	 *	function		dependencies
	 *	========		============
	 *	machinit		depends on: m->machno, m->pdb
	 *	cpuidentify		depends on: m
	 *	confinit		calls: meminit
	 *	meminit			depends on: cpuidentify (needs to know processor
	 *				  type for caching, etc.)
	 *	archinit		depends on: meminit (MP config table may be at the
	 *				  top of system physical memory);
	 *				conf.nmach (not critical, mpinit will check);
	 *	arch->intrinit		depends on: trapinit
	 *	mathinit		depends on: cpuidentify (IRQ13)
	 */
	conf.nmach = 1;
	MACHP(0) = (Mach*)CPU0MACH;
	m->pdb = (void*)CPU0PDB;
	machinit();
	options();
	cpuidentify();
	confinit();
	archinit();
	xinit();
	poolsizeinit();
	trapinit();
	mmuinit();
	printinit();

	screeninit();
	if(isoldbcom)
		print("    ****OLD B.COM - UPGRADE****\n");
	print("conf.npage0=%d conf.npage1=%d\n",
		conf.npage0, conf.npage1);
	if(arch->intrinit){
		doc("intrinit");
		arch->intrinit();
	}
	doc("ns16552install");
	ns16552install();			/* botch: config */
	doc("mathinit");
	mathinit();
	doc("kbdinit");
	kbdinit();
	if(arch->clockenable){
		doc("clockinit");
		arch->clockenable();
	}
	doc("procinit");
	procinit();
	cpuidprint();
	doc("links");
	links();
	doc("chandevreset");
	chandevreset(0);
	doc("userinit");
	userinit();
	schedinit();
}

void
machinit(void)
{
	int machno;
	void *pdb;

	machno = m->machno;
	pdb = m->pdb;
	memset(m, 0, sizeof(Mach));
	m->machno = machno;
	m->pdb = pdb;
}

void
init0(void)
{
	Osenv *o;

	up->nerrlab = 0;

	spllo();

	if(waserror())
		panic("init0: %r");
	/*
	 * These are o.k. because rootinit is null.
	 * Then early kproc's will have a root and dot.
	 */
	o = up->env;
	o->pgrp->slash = namec("#/", Atodir, 0, 0);
	o->pgrp->dot = cclone(o->pgrp->slash, 0);

	chandevinit();
	poperror();

	disinit("/osinit.dis");
}

void
userinit(void)
{
	Proc *p;
	Osenv *o;

	p = newproc();
	o = p->env;

	o->fgrp = newfgrp();

	o->pgrp = newpgrp();
	strcpy(o->user, eve);

	strcpy(p->text, "interp");

	p->fpstate = FPINIT;
	fpoff();

	/*
	 * Kernel Stack
	 *
	 * N.B. The -12 for the stack pointer is important.
	 *	4 bytes for gotolabel's return PC
	 */
	p->sched.pc = (ulong)init0;
	p->sched.sp = (ulong)p->kstack+KSTACK;

	ready(p);
}

Conf	conf;

char*
getconf(char *name)
{
	int i;

	for(i = 0; i < nconf; i++)
		if(cistrcmp(confname[i], name) == 0)
			return confval[i];
	return 0;
}

void
confinit(void)
{
	char *p;
	int i, pcnt;
	ulong maxmem;
	extern int defmaxmsg;
	extern int consoleprint;

	if(p = getconf("*maxmem"))
		maxmem = strtoul(p, 0, 0);
	else
		maxmem = 0;
	if(p = getconf("*kernelpercent"))
		pcnt = 100 - strtol(p, 0, 0);
	else
		pcnt = 0;
	if(p = getconf("*defmaxmsg")){
		i = strtol(p, 0, 0);
		if(i < defmaxmsg && i >=128)
			defmaxmsg = i;
	}

	meminit(maxmem);

	conf.npage = conf.npage0 + conf.npage1;
	if(pcnt < 10)
		pcnt = 70;
	conf.ialloc = (((conf.npage*(100-pcnt))/100)/2)*BY2PG;

	conf.nproc = 100 + ((conf.npage*BY2PG)/MB)*5;
	conf.interps = 5;

	strcpy(eve, "inferno");

	bpoverride("consoleprint", &consoleprint);
}

char*
bpenumenv(int n)
{
	static char c[512];
	if(n < nconf) {
		sprint(c, "%s=%s", confname[n], confval[n]);
		return c;
	}
	return nil;
}

int
bpoverride(char *s, int *p)
{
	char *v = getconf(s);
	if(v) {
		*p = strtol(v, 0, 0);
		return 1;
	} else
		return 0;
}

void
poolsizeinit(void)
{
	ulong nb;

	nb = conf.npage*BY2PG;
	poolsize(mainmem, (nb*main_pool_pcnt)/100, 0);
	poolsize(heapmem, (nb*heap_pool_pcnt)/100, 0);
	poolsize(imagmem, (nb*image_pool_pcnt)/100, 1);
}

char *mathmsg[] =
{
	"invalid",
	"denormalized",
	"div-by-zero",
	"overflow",
	"underflow",
	"precision",
	"stack",
	"error",
};

/*
 *  math coprocessor error
 */
void
matherror(Ureg* ureg, void* arg)
{
	ulong status;
	int i;
	char *msg;
	char note[ERRLEN];

	USED(arg);

	/*
	 *  a write cycle to port 0xF0 clears the interrupt latch attached
	 *  to the error# line from the 387
	 */
	outb(0xF0, 0xFF);

	/*
	 *  save floating point state to check out error
	 */
	FPsave(&up->fpsave.env);
	status = up->fpsave.env.status;

	msg = 0;
	for(i = 0; i < 8; i++)
		if((1<<i) & status){
			msg = mathmsg[i];
			sprint(note, "sys: fp: %s fppc=0x%lux", msg, up->fpsave.env.pc);
			error(note);
			break;
		}
	if(msg == 0){
		sprint(note, "sys: fp: unknown fppc=0x%lux", up->fpsave.env.pc);
		error(note);
	}
	if(ureg->pc & KZERO)
		panic("fp: status %lux fppc=0x%lux pc=0x%lux", status,
			up->fpsave.env.pc, ureg->pc);
}

/*
 *  math coprocessor emulation fault
 */
void
mathemu(Ureg* ureg, void* arg)
{
	USED(ureg, arg);
	switch(up->fpstate){
	case FPINIT:
		fpinit();
		up->fpstate = FPACTIVE;
		break;
	case FPINACTIVE:
		fprestore(&up->fpsave);
		up->fpstate = FPACTIVE;
		break;
	case FPACTIVE:
		panic("math emu", 0);
		break;
	}
}

/*
 *  math coprocessor segment overrun
 */
void
mathover(Ureg* ureg, void* arg)
{
	USED(arg);
	print("sys: fp: math overrun pc 0x%lux pid %d\n", ureg->pc, up->pid);
	pexit("math overrun", 0);
}

void
mathinit(void)
{
	intrenable(VectorCERR, matherror, 0, BUSUNKNOWN);
	if(X86FAMILY(m->cpuidax) == 3)
		intrenable(VectorIRQ13, matherror, 0, BUSUNKNOWN);
	intrenable(VectorCNA, mathemu, 0, BUSUNKNOWN);
	intrenable(VectorCSO, mathover, 0, BUSUNKNOWN);
}

/*
 *  Save the mach dependent part of the process state.
 */
void
procsave(Proc *p)
{
	if(p->fpstate == FPACTIVE){
		if(p->state == Moribund)
			fpoff();
		else
			fpsave(&up->fpsave);
		p->fpstate = FPINACTIVE;
	}
}

void
exit(int ispanic)
{
	USED(ispanic);

	up = 0;
	delay(2000);
	print("exiting\n");

	/* Shutdown running devices */
	chandevreset(1);

	delay(2000);

	arch->reset();
}

void
reboot(void)
{
	exit(0);
}

void
halt(void)
{
	print("cpu halted\n");
	microdelay(1000);
	for(;;);		/* just hang out doing nothing */
}

int
isaconfig(char *class, int ctlrno, ISAConf *isa)
{
	char cc[NAMELEN], *p, *q, *r;
	int n;

	sprint(cc, "%s%d", class, ctlrno);
	for(n = 0; n < nconf; n++){
		if(cistrncmp(confname[n], cc, NAMELEN))
			continue;
		isa->nopt = 0;
		p = confval[n];
		while(*p){
			while(*p == ' ' || *p == '\t')
				p++;
			if(*p == '\0')
				break;
			if(cistrncmp(p, "type=", 5) == 0){
				p += 5;
				for(q = isa->type; q < &isa->type[NAMELEN-1]; q++){
					if(*p == '\0' || *p == ' ' || *p == '\t')
						break;
					*q = *p++;
				}
				*q = '\0';
			}
			else if(cistrncmp(p, "port=", 5) == 0)
				isa->port = strtoul(p+5, &p, 0);
			else if(cistrncmp(p, "irq=", 4) == 0)
				isa->irq = strtoul(p+4, &p, 0);
			else if(cistrncmp(p, "dma=", 4) == 0)
				isa->dma = strtoul(p+4, &p, 0);
			else if(cistrncmp(p, "mem=", 4) == 0)
				isa->mem = strtoul(p+4, &p, 0);
			else if(cistrncmp(p, "size=", 5) == 0)
				isa->size = strtoul(p+5, &p, 0);
			else if(cistrncmp(p, "freq=", 5) == 0)
				isa->freq = strtoul(p+5, &p, 0);
			else if(isa->nopt < NISAOPT){
				r = isa->opt[isa->nopt];
				while(*p && *p != ' ' && *p != '\t'){
					*r++ = *p++;
					if(r-isa->opt[isa->nopt] >= ISAOPTLEN-1)
						break;
				}
				*r = '\0';
				isa->nopt++;
			}
			while(*p && *p != ' ' && *p != '\t')
				p++;
		}
		return 1;
	}
	return 0;
}

int
cistrcmp(char *a, char *b)
{
	int ac, bc;

	for(;;){
		ac = *a++;
		bc = *b++;
	
		if(ac >= 'A' && ac <= 'Z')
			ac = 'a' + (ac - 'A');
		if(bc >= 'A' && bc <= 'Z')
			bc = 'a' + (bc - 'A');
		ac -= bc;
		if(ac)
			return ac;
		if(bc == 0)
			break;
	}
	return 0;
}

int
cistrncmp(char *a, char *b, int n)
{
	unsigned ac, bc;

	while(n > 0){
		ac = *a++;
		bc = *b++;
		n--;

		if(ac >= 'A' && ac <= 'Z')
			ac = 'a' + (ac - 'A');
		if(bc >= 'A' && bc <= 'Z')
			bc = 'a' + (bc - 'A');

		ac -= bc;
		if(ac)
			return ac;
		if(bc == 0)
			break;
	}

	return 0;
}
