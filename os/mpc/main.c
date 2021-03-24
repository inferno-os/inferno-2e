#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"io.h"
#include	"ureg.h"
#include	<ctype.h>
#include "version.h"

/* where b.com or qboot leaves configuration info */
#define BOOTARGS	((char*)CONFADDR)
#define	BOOTARGSLEN	1024
#define	MAXCONF		32

extern ulong kerndate;
extern int cflag;
int	remotedebug;

extern int main_pool_pcnt;
extern int heap_pool_pcnt;
extern int image_pool_pcnt;

char	bootargs[BOOTARGSLEN+1];
char bootdisk[NAMELEN];
char *confname[MAXCONF];
char *confval[MAXCONF];
int nconf;

extern void addconf(char *, char *);

/*
 *  arguments passed to initcode and /boot
 */
char argbuf[128];

static void
options(void)
{
	long i, n;
	char *cp, *line[MAXCONF], *p, *q;

	/*
	 *  parse configuration args from bootstrap
	 */
	memmove(bootargs, BOOTARGS, BOOTARGSLEN);	/* where b.com leaves its config */
	cp = bootargs;
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

	n = parsefields(cp, line, MAXCONF, "\n");
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

void
doc(char *m)
{
	USED(m);
	//print("%s...\n", m); uartwait();
}

static void
poolsizeinit(void)
{
	ulong nb;

	nb = conf.npage*BY2PG;
	poolsize(mainmem, (nb*main_pool_pcnt)/100, 0);
	poolsize(heapmem, (nb*heap_pool_pcnt)/100, 0);
	poolsize(imagmem, (nb*image_pool_pcnt)/100, 1);
}

static void
serialconsole(void)
{
	char *p;
	int port;

	p = getconf("console");
	if(p == nil)
		p = "0";
	if(p != nil && !remotedebug){
		port = strtol(p, nil, 0);
		uartspecial(port, 9600, &kbdq, &printq, kbdcr2nl);
	}
}

void
main(void)
{
	machinit();
	options();
	archinit();
	confinit();
	cpminit();
	xinit();
	//poolsizeinit();
	trapinit();
	mmuinit();
	printinit();
	doc("uartinstall");
	uartinstall();
	serialconsole();
	doc("screeninit");
	screeninit();
	doc("kbdinit");
	kbdinit();
	doc("clockinit");
	clockinit();
	doc("procinit");
	procinit();
	cpuidprint();
	doc("links");
	links();
	doc("chandevreset");
	chandevreset(0);

	print("\nInferno Network Operating System\n");
	print("%s-%s (%lud)\n\n",VERSION, conffile, kerndate);
	print("JIT Compilation Mode = %d\n",cflag);

	doc("userinit");
	userinit();
	doc("schedinit");
	schedinit();
}

void
machinit(void)
{
	int n;

	n = m->machno;
	memset(m, 0, sizeof(Mach));
	m->machno = n;
	m->mmask = 1<<m->machno;
	m->iomem = KADDR(getimmr() & ~0xFFFF);
	m->cputype = getpvr()>>16;
	m->delayloop = 20000;	/* initial estimate only; set by clockinit */
	m->speed = 50;	/* initial estimate only; set by archinit */
}

void
init0(void)
{
	Osenv *o;

	up->nerrlab = 0;

	spllo();

	if(waserror())
		panic("init0");
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

FPU	initfp;

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

	fpsave(&initfp);
	p->fpstate = FPINIT;

	/*
	 * Kernel Stack
	 */
	p->sched.pc = (ulong)init0;
	p->sched.sp = (ulong)p->kstack+KSTACK;

	ready(p);
}

Conf	conf;

void
addconf(char *name, char *val)
{
	if(nconf >= MAXCONF)
		return;
	confname[nconf] = name;
	confval[nconf] = val;
	nconf++;
}

char*
getconf(char *name)
{
	int i;

	for(i = 0; i < nconf; i++)
		if(strcmp(confname[i], name) == 0)
			return confval[i];
	return 0;
}

void
confinit(void)
{
	char *p;
	int i, pcnt;
	extern int defmaxmsg;

	if(p = getconf("*kernelpercent"))
		pcnt = 100 - strtol(p, 0, 0);
	else
		pcnt = 0;
	if(p = getconf("*defmaxmsg")){
		i = strtol(p, 0, 0);
		if(i < defmaxmsg && i >=128)
			defmaxmsg = i;
	}

	archconfinit();
	
	conf.npage = conf.npage0 + conf.npage1;
	if(pcnt < 10)
		pcnt = 70;
	conf.ialloc = (((conf.npage*(100-pcnt))/100)/2)*BY2PG;

	conf.nproc = 100 + ((conf.npage*BY2PG)/MB)*5;
	conf.nmach = MAXMACH;
	conf.interps = 5;

	strcpy(eve, "inferno");
}

void
exit(int ispanic)
{
	up = 0;
	spllo();
	print("cpu %d exiting\n", m->machno);

	/* Shutdown running devices */
	chandevreset(1);

	delay(1000);
	splhi();
	if(ispanic)
		for(;;);
	archreboot();
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
		if(strncmp(confname[n], cc, NAMELEN))
			continue;
		isa->nopt = 0;
		p = confval[n];
		while(*p){
			while(*p == ' ' || *p == '\t')
				p++;
			if(*p == '\0')
				break;
			if(strncmp(p, "type=", 5) == 0){
				p += 5;
				for(q = isa->type; q < &isa->type[NAMELEN-1]; q++){
					if(*p == '\0' || *p == ' ' || *p == '\t')
						break;
					*q = *p++;
				}
				*q = '\0';
			}
			else if(strncmp(p, "port=", 5) == 0)
				isa->port = strtoul(p+5, &p, 0);
			else if(strncmp(p, "irq=", 4) == 0)
				isa->irq = strtoul(p+4, &p, 0);
			else if(strncmp(p, "mem=", 4) == 0)
				isa->mem = strtoul(p+4, &p, 0);
			else if(strncmp(p, "size=", 5) == 0)
				isa->size = strtoul(p+5, &p, 0);
			else if(strncmp(p, "freq=", 5) == 0)
				isa->freq = strtoul(p+5, &p, 0);
			else if(strncmp(p, "dma=", 4) == 0)
				isa->dma = strtoul(p+4, &p, 0);
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
