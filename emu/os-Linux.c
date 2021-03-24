#include	<time.h>
#include	<termios.h>
#include	<signal.h>
#include 	<pwd.h>
#include	<sys/ipc.h>
#include	<sys/sem.h>
#include	<asm/unistd.h>
#include	<sys/time.h>

#include	"dat.h"
#include	"fns.h"
#include	"error.h"

/*
 * DANGER - This is extracted from linux/sched.h
 */
/* Cloning flags.  */
#define CSIGNAL       0x000000ff  /* Signal mask to be sent at exit.  */
#define CLONE_VM      0x00000100  /* Set if VM shared between processes.  */
#define CLONE_FS      0x00000200  /* Set if fs info shared between processes.*/
#define CLONE_FILES   0x00000400  /* Set if open files shared between processes*/
#define CLONE_SIGHAND 0x00000800  /* Set if signal handlers shared.  */
#define CLONE_PID     0x00001000  /* Set if pid shared.  */

static inline _syscall2(int, clone, unsigned long, flags, void*, childsp);

enum
{
	NR_TASKS = 4096,
	KSTACK  = 32*1024,
	DELETE	= 0x7f,
	CTRLC	= 'C'-'@',
	NSEMA	= 32
};

extern Dev	rootdevtab, srvdevtab, fsdevtab, mntdevtab,
		condevtab, ssldevtab, drawdevtab, cmddevtab,
		progdevtab, ipdevtab, pipedevtab,
		audiodevtab, eiadevtab, kfsdevtab;

Dev*    devtab[] =
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
	&eiadevtab,
	&kfsdevtab,
	nil
};

static void daemonize(void);

static char	progname[] = "emu";
static char	*envnames[] = {                 /* canned environment for daemon started from init */
			"HOME=/",
			"SHELL=/sbin/sh",
			"PATH=/bin:/usr/sbin:/usr/bin",
			"LD_LIBRARY_PATH=/lib:/usr/lib",
			 0
};

extern int dflag;

/* information about the allocated semaphore blocks */
typedef	struct sem_block sem_block;
struct sem_block
{
	int		semid;
	int		cnt;
	sem_block	*next;
};
static sem_block *sema = NULL;

int	gidnobody = -1;
int	uidnobody = -1;
struct 	termios tinit;
Proc*	uptab[NR_TASKS];

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
	unlock(&procs.l);

	/* print("pexit: %s: %s\n", up->text, msg);	/**/
	e = up->env;
	if(e != nil) {
		closefgrp(e->fgrp);
		closepgrp(e->pgrp);
	}
	free(up->prog);
	free(up);
	exit(0);
}

int
kproc(char *name, void (*func)(void*), void *arg)
{
	int pid;
	Proc *p;
	Pgrp *pg;
	Fgrp *fg;
	ulong *nsp;
	static int sched;

	p = newproc();
	nsp = malloc(KSTACK);
	if(p == nil || nsp == nil) {
		print("kproc(%s): no memory", name);
		return;
	}

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

	nsp += (KSTACK - sizeof(Proc*))/sizeof(*nsp);
	*nsp = (ulong)p;

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
	unlock(&procs.l);

	/* print("clone: p=%lux sp=%lux\n", p, nsp);	/**/

	sched = 1;

	switch(clone(CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND, nsp)) {
	case -1:
		panic("kproc: clone failed");
		break;
	case 0:
		__asm__(	"movl	(%%esp), %%eax\n\t"
				"movl	%%eax, (%%ebx)\n\t"
				: /* no output */
				: "bx"	(&uptab[gettss()])
				: "eax"
		);

		/* print("child %d/%d up=%lux\n", NR_TASKS, gettss(), up);	/**/

		if(gettss() > NR_TASKS)
			panic("kproc: tss > NR_TASKS");

		up->sigid = getpid();
		sched = 0;
		up->func(up->arg);
		pexit("(main)", 0);
	default:
		while(sched)
			sched_yield();
	}

	return 0;
}


void
trapILL(int signal_number)
{
	(void)signal_number;
	disfault(nil, "Illegal instruction");
}

void
trapBUS(int signal_number)
{
	(void)signal_number;
	disfault(nil, "Bus error");
}

void
trapSEGV(int signal_number)
{
	(void)signal_number;
	disfault(nil, "Segmentation violation");
}

#include <fpuctl.h>
void
trapFPE(int signal_number)
{
	(void)signal_number;

	print("FPU status=0x%.4lux", getfsr());
	disfault(nil, "Floating exception");
}

void
trapUSR1(int signal_number)
{
	(void)signal_number;

	if(up->type != Interp)		/* Used to unblock pending I/O */
		return;

	if(up->intwait == 0)		/* Not posted so its a sync error */
		disfault(nil, Eintr);	/* Should never happen */

	up->intwait = 0;		/* Clear it so the proc can continue */
}

/* called to wake up kproc blocked on a syscall */
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
	sync(); 
}

void
termrestore(void)
{
	tcsetattr(0, TCSANOW, &tinit);
	sync(); 
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
	
	/* clean up the semaphores */
	{
		sem_block *s = sema;
		while (s) {
			union semun su;
			semctl(s->semid, 0, IPC_RMID, su);
			s = s->next;
		}
	}

	kill(0, SIGKILL);
	exit(0);
}

extern	int	rebootok;	/* is shutdown -r supported */

void
libinit(char *imod)
{
	struct termios t;
	struct sigaction act;
	struct passwd *pw;

	rebootok = 1;
	setsid();

	gethostname(ossysname, sizeof(ossysname));
	pw = getpwnam("nobody");
	if(pw != nil) {
		uidnobody = pw->pw_uid;
		gidnobody = pw->pw_gid;
	}

	if(dflag)
		daemonize();
	else
		termset();

	memset(&act, 0 , sizeof(act));
	act.sa_handler=trapUSR1;
	sigaction(SIGUSR1, &act, nil);

	/* For the correct functioning of devcmd in the
	 * face of exiting slaves
	 */
	signal(SIGPIPE, SIG_IGN);
	if(sflag == 0) {
		act.sa_handler=trapBUS;
		sigaction(SIGBUS, &act, nil);
		act.sa_handler=trapILL;
		sigaction(SIGILL, &act, nil);
		act.sa_handler=trapSEGV;
		sigaction(SIGSEGV, &act, nil);
		act.sa_handler = trapFPE;
		sigaction(SIGFPE, &act, nil);

		signal(SIGINT, cleanexit);
	}

	uptab[gettss()] = newproc();

	pw = getpwuid(getuid());
	if(pw != nil) {
		if (strlen(pw->pw_name) + 1 <= NAMELEN)
			strcpy(eve, pw->pw_name);
		else
			print("pw_name too long\n");
	}
	else
		print("cannot getpwuid\n");

	up->env->uid = getuid();
	up->env->gid = getgid();

	emuinit(imod);
}

int
readkbd(void)
{
	int n;
	char buf[1];

	n = read(0, buf, sizeof(buf));
	if(n != 1) {
		print("keyboard close (n=%d, %s)\n", n, strerror(errno));
		pexit("keyboard thread", 0);
	}

	switch(buf[0]) {
	case '\r':
		buf[0] = '\n';
		break;
	case DELETE:
		buf[0] = 'H' - '@';
		break;
	case CTRLC:
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
	Tag*	hash;
	Tag*	free;
	int	semid;		/* id of semaphore block */
	int	sema;		/* offset into semaphore block */
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
	union semun sun;
	struct sembuf sop;

	sop.sem_flg = 0;

	h = (ulong)tag & (NHASH-1);

	lock(&hlock);
	l = &ht[h];
	for(t = *l; t; t = t->hash) {
		if(t->tag == tag) {
			rval = t->val;
			t->val = value;
			t->tag = 0;
			unlock(&hlock);

			sop.sem_num = t->sema;
			sop.sem_op = 1;
			semop(t->semid, &sop, 1);
			return rval;		
		}
	}

	/* create a tag if there is none in the free list */
	t = ft;
	if(t == nil) {

		/* create a new block of semaphores if needed */
		if (!sema || sema->cnt >= (NSEMA-1)) {
			sem_block *s;

			s = malloc(sizeof(sem_block));
			if(s == nil)
				panic("rendezvous: no memory");
			s->semid = semget(IPC_PRIVATE, NSEMA, IPC_CREAT|0700);
			if(s->semid < 0)
				panic("rendezvous: failed to allocate semaphore pool: %r");
			s->cnt = 0;

			s->next = sema;
			sema = s;
		}

		/* create the tag */
		t = malloc(sizeof(Tag));
		if(t == nil)
			panic("rendezvous: no memory");
		t->semid = sema->semid;
		t->sema = sema->cnt++;		/* allocate next from block */

	} else
		ft = t->free;			/* get tag from free list */

	/* setup tag */
	t->tag = tag;
	t->val = value;
	t->hash = *l;
	*l = t;

	sun.val = 0;
	if(semctl(t->semid, t->sema, SETVAL, sun) < 0)
		panic("semctl: %r");
	unlock(&hlock);

	/* wait on semaphore ignoring all EINTR's */
	sop.sem_num = t->sema;
	sop.sem_op = -1;
	while (t->tag) {
		if (semop(t->semid, &sop, 1) < 0) {
			if(errno == EIDRM)
				exit(0);
			if(errno != EINTR)
				panic("semctl: %r"); 
		}
	}

	lock(&hlock);
	rval = t->val;
	for(f = *l; f; f = f->hash) {
		if(f == t) {
			*l = f->hash;
			break;
		}
		l = &f->hash;
	}

	/* add tag to free list */
	t->free = ft;
	ft = t;

	unlock(&hlock);

	return rval;
}

typedef struct Targ Targ;
struct Targ
{
	int     fd;
	int*    spin;
	char*   cmd;
};

int
exectramp(Targ *targ)
{
	int fd, i, nfd, error, uid, gid;
	char *argv[4], buf[MAXDEVCMD];

	fd = targ->fd;

	strncpy(buf, targ->cmd, sizeof(buf)-1);
	buf[sizeof(buf)-1] = '\0';

	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = buf;
	argv[3] = nil;

	print("devcmd: '%s'", buf);
	gid=up->env->gid;
	uid=up->env->gid;

	switch(fork()) {
	case -1:
		print("%s\n",strerror(errno));
		return -1;
	default:
		print(" pid %d\n", getpid());
		return 0;
	case 0:
		nfd = getdtablesize();
		for(i = 0; i < nfd; i++)
			if(i != fd)
				close(i);

		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
		error=0;
		if(gid != -1)
			error=setgid(gid);
		else
			error=setgid(gidnobody);

		if((error)&&(geteuid()==0)){
			print("devcmd: root can't set gid: %d or gidnobody: %d\n",
				up->env->gid,gidnobody);
			exit(0);
		}
		
		error=0;
		if(uid != -1)
			error=setuid(uid);
		else
			error=setuid(uidnobody);

		if((error)&&(geteuid()==0)){
			print( "devcmd: root can't set uid: %d or uidnobody: %d\n",
				up->env->uid,uidnobody);
			exit(0);
                }
		
		execv(argv[0], argv);
		print("%s\n",strerror(errno));
		/* don't flush buffered i/o twice */
		_exit(0);
	}
}

int
oscmd(char *cmd, int *rfd, int *sfd)
{
	Dir dir;
	Targ targ;
	int r, fd[2];

	if(bipipe(fd) < 0)
		return -1;

	signal(SIGCLD, SIG_DFL);

	targ.fd = fd[0];
	targ.cmd = cmd;
	
	r = 0;
	if (exectramp(&targ) < 0) {
		r = -1;
	}

	close(fd[0]);
	*rfd = fd[1];
	*sfd = fd[1];
	return r;
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
		return 0;

	if(sec0 == 0) {
		sec0 = t.tv_sec;
		usec0 = t.tv_usec;
	}
	return (t.tv_sec-sec0)*1000+(t.tv_usec-usec0+500)/1000;
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
        struct  timespec time;

        time.tv_sec = milsec/1000;
        time.tv_nsec= (milsec%1000)*1000000;
        nanosleep(&time,nil);

	return 0;
}

ulong
getcallerpc(void *arg)
{
	USED(arg);
	return 0;
}

void
osyield(void)
{
	sched_yield();
}

void
ospause(void)
{
        for(;;)
                sleep(1000000);
}

ulong
umult(ulong m1, ulong m2, ulong *hi)
{
	ulong lo;

	__asm__(	"mull	%%ecx\n\t"
			"movl	%%edx, (%%ebx)\n\t"
			: "=a" (lo)
			: "eax" (m1),
			  "ecx" (m2),
			  "ebx" (hi)
			: "edx"
	);
	return lo;
}

int
canlock(Lock *l)
{
	int	v;
	
	__asm__(	"movl	$1, %%eax\n\t"
			"xchgl	%%eax,(%%ebx)"
			: "=a" (v)
			: "ebx" (&l->key)
	);
	switch(v) {
	case 0:		return 1;
	case 1: 	return 0;
	default:	panic("canlock: corrupted 0x%lux\n", v);
	}
	return 0;
}

void
FPsave(void *f)
{
	__asm__(	"fstenv	(%%eax)\n\t"
			: /* no output */
			: "eax"	(f)
	);
}

void
FPrestore(void *f)	
{
	__asm__(	"fldenv (%%eax)\n\t"
			: /* no output */
			: "eax"	(f)
	);
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
