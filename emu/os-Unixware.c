#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#undef _POSIX_C_SOURCE 
#include	<unistd.h>
#include	<synch.h>
#include	<thread.h>
#include	<time.h>
#include	<termios.h>
#include	<signal.h>
#include 	<pwd.h>

/*
#define _BSD_TIME
*/
/* for gettimeofday(), which isn't POSIX,
 * but is fairly common
 */
#include	<sys/time.h>

enum
{
	FILENAME = 256,
	DELETE  = 0x7F
};

extern Dev      rootdevtab, srvdevtab, fsdevtab, mntdevtab,
		condevtab, ssldevtab, drawdevtab, cmddevtab,
		progdevtab, ipdevtab, pipedevtab,
		audiodevtab, kfsdevtab, eiadevtab;

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
	&kfsdevtab,
	&eiadevtab,
	nil
};

static void daemonize(void);

static thread_key_t	prdakey;

static char	progname[] = "emu";
static char	*envnames[] = {                 /* canned environment for daemon started from init */
			"HOME=/",
			"SHELL=/sbin/sh",
			"PATH=/bin:usr/sbin:/usr/bin",
			"LD_LIBRARY_PATH=/usr/lib:/usr/openwin/lib:/usr/ucblib",
			 0
};

static siginfo_t siginfo;

extern int dflag;
extern int usingnis;
extern char ypfile[];

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

	/*print("pexit: %s: %s\n", up->text, msg);*/
	e = up->env;
	if(e != nil) {
		closefgrp(e->fgrp);
		closepgrp(e->pgrp);
	}
	free(up->prog);
	free(up);
	thr_exit(0);
}

static void *
tramp(void *v)
{
	struct Proc *Up;

	if(thr_setspecific(prdakey,v))
	{
		print("set specific data failed in tramp\n");
		thr_exit(0);
	}
	Up = v;
	Up->sigid = thr_self();
	Up->func(Up->arg);
	pexit("", 0);
}


int
kproc(char *name, void (*func)(void*), void *arg)
{
	thread_t thread;
	Proc *p;
	Pgrp *pg;
	Fgrp *fg;

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

	if(thr_create(0, 0, &tramp, p, THR_BOUND, &thread))
		panic("thr_create failed\n");
	thr_yield();
	return(thread);
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
printILL(int sig, siginfo_t *siginfo, void *v)
{
	panic(
	"Illegal instruction with code=%d at address=%x, opcode=%x.\n"
	,siginfo->si_code, siginfo->si_addr,*(char*)siginfo->si_addr);
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

void
trapFPE(void)
{
	disfault(nil, "Floating point exception");
}

void
oshostintr(Proc *p)
{
	thr_kill(p->sigid, SIGUSR1);
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
	kill(0, SIGKILL);
	exit(0);
}

int gidnobody= -1, uidnobody= -1;

void
getnobody(void)
{
	struct passwd *pwd;
	
	if (pwd=getpwnam("nobody")) {
		uidnobody = pwd->pw_uid;
		gidnobody = pwd->pw_gid;
	}
}

extern	int	rebootok;	/* is shutdown -r supported */

void
libinit(char *imod)
{
	struct Proc *Up;
	struct sigaction act;
	struct passwd *pw;

	rebootok = 1;
	setsid();

	if(dflag)
		daemonize();
	else
		termset();

	gethostname(ossysname, sizeof(ossysname));
	getnobody();

	memset(&act, 0 , sizeof(act));
	act.sa_handler=trapUSR1;
	sigaction(SIGUSR1, &act, nil);
	/* For the correct functioning of devcmd in the
	 * face of exiting slaves
	 */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGTERM, cleanexit);
	if(sflag == 0) 
	{
		act.sa_handler=trapBUS;
		sigaction(SIGBUS, &act, nil);
		act.sa_sigaction=trapILL;
		sigaction(SIGILL, &act, nil);
		act.sa_handler=trapSEGV;
		sigaction(SIGSEGV, &act, nil);
		act.sa_handler=trapFPE;
		sigaction(SIGFPE, &act, nil);
		signal(SIGINT, cleanexit);
	}
	else{
		act.sa_sigaction=printILL;
		act.sa_flags=SA_SIGINFO;
		sigaction(SIGILL, &act, nil);
	}	

	if(thr_keycreate(&prdakey,NULL))
		print("keycreate failed\n");

	Up = newproc();
	if(thr_setspecific(prdakey,Up))
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
	sema_t	sema;
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
			sema_post(&t->sema);
			return rval;		
		}
	}

	t = ft;
	if(t == 0) {
		t = malloc(sizeof(Tag));
		if(t == 0)
			panic("rendezvous: no memory");
	}
	else {
		ft = t->free;
	}

	t->tag = tag;
	t->val = value;
	t->hash = *l;
	*l = t;
	sema_init(&t->sema,0,USYNC_THREAD,0);
	unlock(&hlock);

	while(sema_wait(&t->sema))
		; /* sig usr1 may catch us */

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

/* because of differences between the Irix and Solaris multi-threading 
 * environments this routine must differ from its Irix counter part.
 * In irix sprocsp() starts this routine as a seperate process, so the 
 * parent must spin waiting for the command to be copy out of targ->cmd.
 * In Solaris the exec cannot be done from within a thread because
 * they all share the process, so it has to fork1() first.
 * vfork() is not MT safe and so cannot be used here.
 */
int
exectramp(Targ *targ)
{
	int fd, i, nfd;
	char *argv[4], buf[2+MAXROOT+2+MAXDEVCMD];
	extern char rootdir[MAXROOT];

	fd = targ->fd;

	sprint(buf, "r=%s; ", rootdir);
	i = strlen(rootdir);
	strncpy(buf+2+i+2, targ->cmd, sizeof(buf)-2-i-2-1);
	buf[sizeof(buf)-1] = '\0';

	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = buf;
	argv[3] = nil;

	print("devcmd: '%s'", buf);

	switch(fork1()) 
	{
	int error;
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
		if(up->env->gid != -1)
			error = setgid(up->env->gid);
		else
			error = setgid(gidnobody);

		if((error)&&(geteuid()==0)){
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

		if((error)&&(geteuid()==0)){
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

	signal(SIGCLD, SIG_IGN);

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
        struct  timespec time;

        time.tv_sec = milsec/1000;
        time.tv_nsec= (milsec%1000)*1000000;
       	usleep(milsec);

	return 0;
}

Proc *
getup(void)
{
	void *vp;

	if (thr_getspecific(prdakey,&vp))
		return nil;
	return vp;
}

void
osyield(void)
{
	thr_yield();
}

void
ospause(void)
{
        for(;;)
                sleep(1000000);
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
