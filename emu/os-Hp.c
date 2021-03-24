#include	<pthread.h>
#include	<signal.h>
#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include 	<sys/socket.h>
#include	<time.h>
#include	<sys/time.h>
#include	<termios.h>
#include        <sys/sem.h>
#include	<pwd.h>
#include	<errno.h>

enum
{
	BUNCHES = 5000,
	DELETE  = 0x7F
};

extern Dev	rootdevtab, srvdevtab, fsdevtab, mntdevtab,
		condevtab, ssldevtab, drawdevtab, cmddevtab,
		progdevtab, ipdevtab, pipedevtab,
		audiodevtab;

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
        nil
};

static void daemonize(void);

static pthread_key_t	prdakey;

static char	progname[] = "emu";
static char	*envnames[] = {                 /* canned environment for daemon started from init */
			"HOME=/",
			"SHELL=/sbin/sh",
			"PATH=/bin:usr/sbin:/usr/bin",
			"LD_LIBRARY_PATH=/usr/lib:/usr/openwin/lib:/usr/ucblib",
			 0
};

extern int dflag;

Lock mulock = {1, 0};
 
int
canlock(Lock *l)
{
        int retval;
        int hashkey;
 
	while(!(mutexlock(&mulock)))
		pthread_yield();
 
        if(l->key)
                retval=0;
        else {
                l->key=1;
                retval=1;
        }
	mulock.key = 1;
        return retval;
}



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

	e = up->env;
	if(e != nil) {
		closefgrp(e->fgrp);
		closepgrp(e->pgrp);
	}
	free(up->prog);
	free(up);
	pthread_exit(0);
}

void
trapBUS(int signo, siginfo_t *info, void *context)
{
	if(info)
		print("trapBUS: signo: %d code: %d addr: %lx\n",
		info->si_signo, info->si_code, info->si_addr);
	else
		print("trapBUS: no info\n"); 
	disfault(nil, "Bus error");
}

void
trapUSR1(void)
{
	if(up->type != Interp)		/* Used to unblock pending I/O */
		return;
	if(up->intwait == 0)		/* Not posted so its a sync error */
		disfault(nil, Eintr);	/* Should never happen */

	up->intwait = 0;		/* Clear it so the proc can continue */
}

void
trapILL(void)
{
	disfault(nil, "Illegal instruction");
}

void
trapSEGV(void)
{
	disfault(nil, "Segmentation violation");
}

sigset_t set;
setsigs()
{
	struct sigaction act;

	memset(&act, 0 , sizeof(act));
	sigemptyset(&set);
	
	act.sa_handler=SIG_IGN;
	if(sigaction(SIGPIPE, &act, nil))
	        panic("can't ignore sig pipe");

	if(sigaddset(&set,SIGUSR1)== -1)
		panic("sigaddset SIGUSR1");

	if(sigaddset(&set,SIGUSR2)== -1)
                panic("sigaddset SIGUSR2");

	/* For the correct functioning of devcmd in the
	 * face of exiting slaves
	 */
	if(sflag == 0) {
		act.sa_handler=trapBUS;
		act.sa_flags|=SA_SIGINFO;
		if(sigaction(SIGBUS, &act, nil))
			panic("sigaction SIGBUS");
		act.sa_handler=trapILL;
		if(sigaction(SIGILL, &act, nil))
                        panic("sigaction SIGILL");
		act.sa_handler=trapSEGV;
		if(sigaction(SIGSEGV, &act, nil))
                        panic("sigaction SIGSEGV");
		if(sigaddset(&set,SIGINT)== -1)
			panic("sigaddset");
	}
	if(sigprocmask(SIG_BLOCK,&set,nil)!= 0)
		panic("sigprocmask");
}

static void *
tramp(void *v)
{
	struct Proc *Up;
	pthread_t thread;
	struct sigaction oldact;

	setsigs();
	if(sigaction(SIGBUS, nil, &oldact))
                panic("sigaction failed");
        if(oldact.sa_handler!=trapBUS && sflag==0)
                panic("3rd old act sa_handler");

	if(pthread_setspecific(prdakey,v)) {
		print("set specific data failed in tramp\n");
		pthread_exit(0);
	}
	Up = v;
 	thread = pthread_self();
	Up->sigid = cma_thread_get_unique(&thread);
	/* attempt to catch signals again */
	setsigs();
	Up->func(Up->arg);
	pexit("", 0);
}

pthread_t active_threads[BUNCHES]; /* this should be more than enuf */

int
kproc(char *name, void (*func)(void*), void *arg)
{
	pthread_t thread;
	pthread_attr_t attr;
	int id;
	Proc *p;
	Pgrp *pg;
	Fgrp *fg;
	struct sigaction oldact;

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
	unlock(&procs.l);
	if((pthread_attr_create(&attr))== -1)
		panic("pthread_attr_create failed");

	errno=0;
	pthread_attr_setsched(&attr,SCHED_OTHER);
	if(errno)
		panic("pthread_attr_setsched failed");

	if(pthread_create(&thread, attr, tramp, p))
		panic("thr_create failed\n");
        if(sigaction(SIGBUS, nil, &oldact))
                panic("sigaction failed");
        if(oldact.sa_handler!=trapBUS && sflag == 0)
                panic("2nd old act sa_handler");

	if((id=cma_thread_get_unique(&thread))>=BUNCHES)
		panic("id too big");
	active_threads[id]=thread;
	return id;
}

void
oshostintr(Proc *p)
{
	pthread_cancel(active_threads[p->sigid]);
}

void
oslongjmp(void *regs, osjmpbuf env, int val)
{
	USED(regs);
	siglongjmp(env, val);
}

struct termios tinit;
extern int wakeupkey;

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
	if(wakeupkey)
		if(semctl(wakeupkey,IPC_RMID,0))
			print("failed to clean up semaphore: %s\n",
			strerror(errno));
	tcsetattr(0, TCSANOW, &tinit);
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

int gidnobody= -1, uidnobody= -1;
void
getnobody()
{
	struct passwd *pwd;
	
	if(pwd = getpwnam("nobody")) {
		uidnobody = pwd->pw_uid;
		gidnobody = pwd->pw_gid;
	}
}

static	pthread_mutex_t rendezvouslock;
extern	int	rebootok;	/* is shutdown -r supported */

void
libinit(char *imod)
{
	struct passwd *pw;
	struct Proc *Up;
	struct sigaction oldact;
	int ii;
	int retval;
	int *pidptr;

	rebootok = 1;
	cma_init();
	setsid();
	/* mulock.key = 1; */ /* initialize to unlock */
	if(pthread_mutex_init(&rendezvouslock,pthread_mutexattr_default))
		panic("pthread_mutex_init");

	gethostname(ossysname, sizeof(ossysname));
	getnobody();

	if(dflag)
		daemonize();
	else
		termset();

	setsigs();
	if(sigaction(SIGBUS, nil, &oldact)) {
                panic("sigaction failed");
	}
        if(oldact.sa_handler!=trapBUS && sflag == 0)
                panic("1st old act sa_handler");

	if(pthread_keycreate(&prdakey,NULL))
		print("keycreate failed\n");

	Up = newproc();
	if(pthread_setspecific(prdakey,Up))
		panic("set specific thread data failed\n");

	pw = getpwuid(getuid());
        if(pw != nil)
                if (strlen(pw->pw_name) + 1 <= NAMELEN)
                        strcpy(eve, pw->pw_name);
                else
                        print("pw_name too long\n");
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
	pthread_cond_t cv;
};

static	Tag*	ht[NHASH];
static	Tag*	ft;

int
rendezvous(void *tag, ulong value)
{
	int h;
	ulong rval;
	Tag *t, *f, **l;
	int ii=0;

	h = (ulong)tag & (NHASH-1);

	if(pthread_mutex_lock(&rendezvouslock))
		panic("pthread_mutex_lock");

	l = &ht[h];
	for(t = *l; t; t = t->hash) {
		if(t->tag == tag) {
			rval = t->val;
			t->val = value;
			t->tag = 0;
			if(pthread_mutex_unlock(&rendezvouslock))
				panic("pthread_mutex_unlock");
			if(pthread_cond_signal(&(t->cv)))
				panic("pthread_cond_signal");
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
	t->hash = *l;
	if(pthread_cond_init(&(t->cv),pthread_condattr_default)) {
		print("pthread_cond_init (errno: %s) \n", strerror(errno));
		panic("pthread_cond_init");
	}
	*l = t;

	while(t->tag)
		pthread_cond_wait(&(t->cv),&rendezvouslock);

	rval = t->val;
	for(f = *l; f; f = f->hash){
		if(f == t) {
			*l = f->hash;
			break;
		}
		l = &f->hash;
	}
	t->free = ft;
	ft = t;
	if(pthread_mutex_unlock(&rendezvouslock))
		panic("pthread_mutex_unlock");

	return rval;
}

static char*
month[] =
{
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

int
timeconv(va_list *arg, Fconv *f)
{
	struct tm *tm;
	time_t t;
	struct tm rtm;
	char buf[64];

	t = va_arg(*arg, long);
	tm =localtime_r(&t, &rtm);

	sprint(buf, "%s %2d %-.2d:%-.2d",
		month[tm->tm_mon], tm->tm_mday, tm->tm_hour, tm->tm_min);

	strconv(buf, f);
	return sizeof(long);
}

static char*
modes[] =
{
	"---",
	"--x",
	"-w-",
	"-wx",
	"r--",
	"r-x",
	"rw-",
	"rwx",
};

static void
rwx(long m, char *s)
{
	strncpy(s, modes[m], 3);
}

int
dirmodeconv(va_list *arg, Fconv *f)
{
	static char buf[16];
	ulong m;

	m = va_arg(*arg, ulong);

	if(m & CHDIR)
		buf[0]='d';
	else if(m & CHAPPEND)
		buf[0]='a';
	else
		buf[0]='-';
	if(m & CHEXCL)
		buf[1]='l';
	else
		buf[1]='-';
	rwx((m>>6)&7, buf+2);
	rwx((m>>3)&7, buf+5);
	rwx((m>>0)&7, buf+8);
	buf[11] = 0;

	strconv(buf, f);
	return(sizeof(ulong));
}

typedef struct Targ Targ;
struct Targ
{
	int     fd;
	int*    spin;
	char*   cmd;
};

void
closeall(int fd) 
{
	int nfd, i;

	nfd = getdtablesize();
	for(i = 0; i < nfd; i++)
		if(i != fd)
			close(i);
}

int
exectramp(Targ *targ)
{
	int fd;
	char *argv[4], buf[MAXDEVCMD];

	fd = targ->fd;

	strncpy(buf, targ->cmd, sizeof(buf)-1);
	buf[sizeof(buf)-1] = '\0';

	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = buf;
	argv[3] = nil;

	print("devcmd: '%s'", buf);

	switch(fork()) {
	int error;
	case -1:
		print("%s\n",strerror(errno));
		return -1;
	default:
		print(" pid %d\n", getpid());
		return 0;
	case 0:
		closeall(fd);
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		close(fd);
		error=0;
		if(up->env->gid != -1)
                        error=setgid(up->env->gid);
                else
                        error=setgid(gidnobody);
 
                if((error)&&(geteuid()==0)) {
                        print(
                        "devcmd: root can't set gid: %d or gidnobody: %d\n",
                        up->env->gid,gidnobody);
                        _exit(0);
                }
 
                error=0;
                if(up->env->uid != -1)
                        error=setuid(up->env->uid);
                else
                        error=setuid(uidnobody);
 
                if((error)&&(geteuid()==0)) {
                        print(
                        "devcmd: root can't set uid: %d or uidnobody: %d\n",
                        up->env->uid,uidnobody);
                        _exit(0);
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

	signal(_SIGCLD, SIG_IGN);

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
		return(0);
	if(sec0==0) {
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
        struct  timespec time;
	time.tv_sec = milsec/1000;
       	time.tv_nsec= (milsec%1000)*1000000;
       	if(pthread_delay_np(&time)== -1)
		panic("nanosleep failed\n");
	return 0;
}
	
Proc *
getup(void)
{
	void *vp;

	vp=nil;
	pthread_getspecific(prdakey,&vp);
	return(vp);
}

ulong
getcallerpc(void *arg)
{
	return 0 ;
}

void
osyield(void)
{
	pthread_yield();
}

void
ospause(void)
{
	int s;

	while(1) {
		switch(s=sigwait(&set)) {
		case SIGUSR1:
			trapUSR1();
		case SIGINT:
			cleanexit(0);
		default:
			print("signal: %d %s\n",s, strerror(errno));
			panic("sigwait");
		}
	}
}

int wakeupkey = 0;
extern int rbnotfull(void*);

static int pid;
static Rb *rbptr;

void
osspin(Rendez *prod)
{
	switch(pid=fork()){
	case -1:
		print("%s\n",strerror(errno));
	default:
		pthread_exit(0);
	case 0:
		closeall(1);
		nice(20);
	}

        for(;;){
                if(!rbnotfull(0)) {
			struct sembuf sop;

			sop.sem_num = 0;
			sop.sem_op  = -1;
			sop.sem_flg = 0;
			shmctl(wakeupkey,0,SETVAL,0);
			if(semop(wakeupkey,&sop,1) == -1) 
				print("semop failed errno=%d\n",errno);
                }
                rbptr->randomcount++;
        }
}


void
oswakeupproducer(Rendez *rendez)
{
	struct sembuf sop[1];

	sop[0].sem_num = 0;
	sop[0].sem_op  = 1;
	sop[0].sem_flg = 0;
	if(semop(wakeupkey,sop,1) == -1) {
		print("wakeup failed errno=%d wakeupkey: %d\n",errno,wakeupkey);
		panic("oswakeupproducer");
	}
	sleep(0);
}

Rb*
osraninit(void)
{
	int shmidrb;
	struct sembuf sop[1];
	
	if((shmidrb=shmget(IPC_PRIVATE,sizeof(Rb),0666)) == -1)
                panic("could not get share memory for randombuf");

	if((rbptr=shmat(shmidrb,0,0)) == -1) {
		print("errno=%d\n",errno);
                panic("could not attach share memory for randombuf");
	}
	if((wakeupkey=semget(IPC_PRIVATE,1,IPC_CREAT|0666)) == -1) {
		print("errno=%d\n",errno);
                panic("could not get semop key");
        }
	return rbptr;
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
