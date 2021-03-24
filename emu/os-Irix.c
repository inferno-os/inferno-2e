/* Link with -lfpe. See man pages for fpc
 * and /usr/include/sigfpe.h, sys/fpu.h.
 */
#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	<time.h>
#include	<ulocks.h>
#include	<termios.h>
#include 	<sigfpe.h>
#include	<sys/prctl.h>
#include 	<sys/fpu.h>
#include	<sys/cachectl.h>
#undef _POSIX_SOURCE		/* SGI incompetence */
#include	<signal.h>
#define _BSD_TIME
/* for gettimeofday(), which isn't POSIX,
 * but is fairly common
 */
#include 	<sys/time.h> 
#define _POSIX_SOURCE
#include 	<pwd.h>

extern	int	rebootargc;
extern	char**	rebootargv;

int	gidnobody = -1;
int	uidnobody = -1;
Proc**	Xup;

#define MAXSPROC 30000	/* max procs == MAXPID */
static int	sproctbl[MAXSPROC];

enum
{
	KSTACK	= 64*1024,
	DELETE	= 0x7F
};

extern Dev	rootdevtab, srvdevtab, fsdevtab, mntdevtab,
		condevtab, ssldevtab, drawdevtab, cmddevtab,
		progdevtab, ipdevtab, pipedevtab,
		audiodevtab;

Dev*	devtab[] =
{
	&rootdevtab,
	&condevtab,
	&srvdevtab,
	&fsdevtab,
	&mntdevtab,
	&ssldevtab,
	&drawdevtab,
	&cmddevtab,
	&progdevtab,
	&ipdevtab,
	&pipedevtab,
	&audiodevtab,
	nil
};

static void daemonize(void);

static char	progname[] = "emu";
static char	*envnames[] = {                 /* canned environment for daemon started from init */
			"HOME=/",
			"SHELL=/sbin/sh",
			"PATH=/bin:usr/sbin:/usr/bin",
			"LD_LIBRARY_PATH=/usr/lib:/usr/openwin/lib:/usr/ucblib",
			 0
};

extern int dflag;

void
pexit(char *msg, int t)
{
	Osenv *e;

	lock(&procs.l);
	if(up->prev) 
		up->prev->next = up->next;
	else
		procs.head = up->next;

	if(up->next)
		up->next->prev = up->prev;
	else
		procs.tail = up->prev;

	sproctbl[getpid()] = -1;

	unlock(&procs.l);

/*	print("pexit: %s: %s\n", up->text, msg); /**/
	e = up->env;
	if(e != nil) {
		closefgrp(e->fgrp);
		closepgrp(e->pgrp);
	}
	free(up->prog);
	free(up);
	exit(0);
}

static void
tramp(void *p, size_t stacksz)
{
	up = p;
	up->sigid = getpid();
	up->func(up->arg);
	pexit("", 0);
}

int
kproc(char *name, void (*func)(void*), void *arg)
{
	Proc *p;
	Pgrp *pg;
	Fgrp *fg;
	int pid;
	int id;
	int i;

	p = newproc();

	pg = up->env->pgrp;
	p->env->pgrp = pg;
	fg = up->env->fgrp;
	p->env->fgrp = fg;
	incref(&pg->r);
	incref(&fg->r);

	p->env->uid = up->env->uid;
	p->env->gid = up->env->gid;
	memmove(p->env->user, up->env->user, NAMELEN);

	strcpy(p->text, name);

	p->func = func;
	p->arg = arg;

	lock(&procs.l);
	if(procs.tail != nil) {
		p->prev = procs.tail;
		procs.tail->next = p;
	}
	else {
		procs.head = p;
		p->prev = nil;
	}
	procs.tail = p;

	for(i = 1; i < MAXSPROC; i++) {
		if(sproctbl[i] == -1) {
			break;
		}
	}

	if(i==MAXSPROC)
		return -1;

	sproctbl[i] = -i - 1; /* temporary hold of table index outside of lock */

	unlock(&procs.l);

	pid = sprocsp(tramp, PR_SALL, p, 0, KSTACK);

	if(-1 < pid)
		sproctbl[i] = pid;
	else
		sproctbl[i] = -1;

	return pid;
}

void
trapUSR1(void)
{
	if(up->type != Interp)		/* Used to unblock pending I/O */
		return;
	if(up->intwait == 0)		/* Not posted so its a sync error */
		disfault(nil, Eintr);	/* Should never happen */

	up->intwait = 0;		/* Clear it so the process can continue */
}

void
trapILL(void)
{
	disfault(nil, "Illegal instruction");
}

void
trapBUS(void)
{
	disfault(nil, "Bus error");
}

void
trapSEGV(void)
{
	disfault(nil, "Segmentation violation");
}

/*
 * This is not a signal handler but rather a vector from real/FPcontrol-Irix.c
 */
void
trapFPE(unsigned exception[5], int value[2])
{
	disfault(nil, "Floating point exception");
}

void
oshostintr(Proc *p)
{
	kill(p->sigid, SIGUSR1);
}

void
oslongjmp(void *regs, osjmpbuf env, int val)
{
	USED(regs);
	siglongjmp(env, val);
}

static struct termios tinit;

void
termset(void)
{
	struct termios t;

	tcgetattr(0, &t);
	tinit = t;
	t.c_lflag &= ~(ICANON|ECHO|ISIG);
	t.c_cc[VMIN] = 1;
	t.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &t);
}

void
termrestore(void)
{
	tcsetattr(0, TCSANOW, &tinit);

/*	if(sproctbl[0] < 0)
		panic("corrupt sproc tbl");

	kill(sproctbl[0], SIGUSR2);
	sginap(10000); */
}

void
trapUSR2(void)
{
	int i;

	for(i = MAXSPROC - 1; i > 0; i--) {
		if(sproctbl[i] != -1) 
			kill(sproctbl[i], SIGKILL);
		sproctbl[i] = -1;
	}

	execvp(rebootargv[0], rebootargv);
	panic("reboot failure");
}

void
cleanexit(int x)
{
	USED(x);

	if(up->intwait) {
		up->intwait = 0;
		return;
	}

	if(dflag) {
		logmsg(1, "Inferno terminating on SIGTERM");
		logclose();
	}
	else
		termrestore();
	kill(0, SIGKILL);
	exit(0);
}

void
getnobody(void)
{
        struct passwd *pwd;
 
	pwd = getpwnam("nobody");
	if(pwd != nil) {
                uidnobody = pwd->pw_uid;
                gidnobody = pwd->pw_gid;
        }
}

extern	int	rebootok;	/* is shutdown -r supported */

void
libinit(char *imod)
{
	struct sigaction act;
	struct passwd *pw;
	int i;

	rebootok = 1;
	setsid();

	for(i=0; i<MAXSPROC; i++)
		sproctbl[i] = -1;

	sproctbl[0] = getpid();

	gethostname(ossysname, sizeof(ossysname));

	if(dflag)
		daemonize();
	else
		termset();

	signal(SIGINT, cleanexit);
	signal(SIGUSR2, trapUSR2);
	/* For the correct functioning of devcmd in the
	 * face of exiting slaves
	 */
	signal(SIGCLD, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	memset(&act, 0 , sizeof(act));
	act.sa_handler=trapUSR1;
	sigaction(SIGUSR1, &act, nil);
	if(sflag == 0) {
		act.sa_handler=trapBUS;
		sigaction(SIGBUS, &act, nil);
		act.sa_handler=trapILL;
		sigaction(SIGILL, &act, nil);
		act.sa_handler=trapSEGV;
		sigaction(SIGSEGV, &act, nil);
	}

	if(usconfig(CONF_INITUSERS, 1000) < 0)
		panic("usconfig");

	Xup = (Proc**)PRDA->usr_prda.fill;
	up = newproc();

	pw = getpwuid(getuid());
	if(pw != nil) {
		if (strlen(pw->pw_name) + 1 <= NAMELEN)
			strcpy(eve, pw->pw_name);
		else
			print("pw_name too long\n");
	}
	else
		print("cannot getpwuid\n");

	/* after setting up, since this takes locks */
	getnobody();
	up->env->uid = getuid();
	up->env->gid = getgid();
	emuinit(imod);
}

int
readkbd(void)
{
	int n;
	char buf[1], ebuf[ERRLEN];

	n = read(0, buf, sizeof(buf));
	if(n != 1) {
		oserrstr(ebuf);
		print("keyboard close (n=%d, %s)\n", ebuf);
		pexit("keyboard thread", 0);
	}
	switch(buf[0]) {
	case '\r':
		buf[0] = '\n';
		break;
	case DELETE:
		cleanexit(0);
		break;
	}
	return buf[0];
}

enum
{
	NHLOG	= 7,
	NHASH	= (1<<NHLOG)
};

typedef struct Tag Tag;
struct Tag
{
	void*	tag;
	ulong	val;
	int	pid;
	Tag*	hash;
	Tag*	free;
};

static	Tag*	ht[NHASH];
static	Tag*	ft;
static	Lock	hlock;

int
rendezvous(void *tag, ulong value)
{
	int h;
	ulong rval;
	Tag *t, *f, **l;

	h = (ulong)tag & (NHASH-1);

	lock(&hlock);
	l = &ht[h];
	for(t = *l; t; t = t->hash) {
		if(t->tag == tag) {
			rval = t->val;
			t->val = value;
			t->tag = 0;
			unlock(&hlock);
			unblockproc(t->pid);
			return rval;		
		}
	}

	t = ft;
	if(t == 0) {
		t = malloc(sizeof(Tag));
		if(t == 0)
			panic("rendezvous: no memory");
	} else
		ft = t->free;

	t->tag = tag;
	t->val = value;
	t->pid = PRDA->sys_prda.prda_sys.t_pid;
	t->hash = *l;
	*l = t;
	unlock(&hlock);

	blockproc(t->pid);

	lock(&hlock);
	rval = t->val;
	for(f = *l; f; f = f->hash) {
		if(f == t) {
			*l = f->hash;
			break;
		}
		l = &f->hash;
	}
	t->free = ft;
	ft = t;
	unlock(&hlock);

	return rval;
}

typedef struct Targ Targ;
struct Targ
{
	int	fd;
	int*	spin;
	char*	cmd;
};

void
exectramp(Targ *targ)
{
	int fd, i, nfd;
	char *argv[4], buf[KSTACK];
	int error;

	fd = targ->fd;
	strncpy(buf, targ->cmd, sizeof(buf)-1);
	*targ->spin = 0;

	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = buf;
	argv[3] = nil;

	print("devcmd: '%s' pid %d\n", buf, getpid());

	nfd = getdtablesize();
	for(i = 0; i < nfd; i++)
		if(i != fd)
			close(i);

	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	close(fd);


        error=0;
	if(up->env->gid != -1)
	        error=setgid(up->env->gid);
	else
	        error=setgid(gidnobody);
 
	if((error)&&(geteuid()==0)){
	        print(
	        "devcmd: root can't set gid: %d or gidnobody: %d\n",
	        up->env->gid,gidnobody);
	        exit(0);
	}
 
	error=0;
	if(up->env->uid != -1)
	        error=setuid(up->env->uid);
	else
	        error=setuid(uidnobody);
 
	if((error)&&(geteuid()==0)){
	        print(
	        "devcmd: root can't set uid: %d or uidnobody: %d\n",
	        up->env->uid,uidnobody);
	        exit(0);
	}
	execv(argv[0], argv);
	exit(0);
}

int
oscmd(char *cmd, int *rfd, int *sfd)
{
	Dir dir;
	Targ targ;
	int r, spin, *spinptr, fd[2];

	if(bipipe(fd) < 0)
		return -1;

	signal(SIGCLD, SIG_IGN);

	spinptr = &spin;
	spin = 1;

	targ.fd = fd[0];
	targ.cmd = cmd;
	targ.spin = spinptr;

	sprocsp(exectramp, PR_SADDR, &targ, 0, 2*KSTACK);
	while(*spinptr)
	    ;
	close(fd[0]);

	*rfd = fd[1];
	*sfd = fd[1];
	return 0;
}

int
segflush(void *a, ulong n)
{
	cacheflush(a, n, BCACHE);
	return 0;
}

ulong
getcallerpc(void *arg)
{
	return 0;
}

/*
 * Return an abitrary millisecond clock time
 */
long
osmillisec(void)
{
	static long sec0 = 0, usec0;
	struct timeval t;

	if(gettimeofday(&t,(struct timezone*)0)<0)
		return(0);
	if(sec0==0){
		sec0 = t.tv_sec;
		usec0 = t.tv_usec;
	}
	return((t.tv_sec-sec0)*1000+(t.tv_usec-usec0+500)/1000);
}

/*
 * Return the time since the epoch in microseconds
 * The epoch is defined at 1 Jan 1970
 */
vlong
osusectime(void)
{
	struct timeval t;
 
       	gettimeofday(&t, nil);
       	return (vlong)t.tv_sec * 1000000 + t.tv_usec;

}

int
osmillisleep(ulong milsec)
{
	static int tick;

	/*
	 * Posix-conforming CLK_TCK implementations tend to call sysconf,
	 * and we don't need the overhead.
	 */
	if(!tick)
		tick = CLK_TCK;
	sginap((tick*milsec)/1000);
	return 0;
}

void
osyield(void)
{
	sginap(0);
}

void
ospause(void)
{
        for(;;) {
                sleep(1000000);
	}
}

static Rb rb;
extern int rbnotfull(void*);

void
osspin(Rendez *prod)
{
        for(;;){
                if((rb.randomcount & 0xffff) == 0 && !rbnotfull(0)) {
                        Sleep(prod, rbnotfull, 0);
                }
                rb.randomcount++;
        }
}
 
Rb*
osraninit(void)
{
	return &rb;
}

void
oswakeupproducer(Rendez *rendez)
{
	Wakeup(rendez);
}

static void
daemonize(void)
{
	char **e;

	if (!logopen(progname)) {
		fprint(2, "emu: warning: can't open logging facility, using standard output\n");
		dflag = 0;
	}
	else {
		close(1);
		close(2);
	}
	close(0);

	if (getppid() != 1) {
		switch (fork()) {
		case -1:
			panic("fork 1 failed");
		default:
			exit(0);	/* parent exits */
		case 0:
			break;	/* child continues */
		}

		if (setpgrp() == -1)		/* become process group leader */
			panic("setpgrp 1 failed");

		signal(SIGHUP, SIG_IGN);	/* ignore pgrp leader death */

		switch (fork()) {
		case -1:
			panic("fork 2 failed");
		default:
			exit(0);	/* parent exits */
		case 0:
			break;	/* child continues */
		}
	}
	else {
		umask(022);
		for (e = envnames; *e != nil; e++)
			if(putenv(*e) != 0) 
				panic("configenv: putenv failed");
	}
	errno = 0;
}
