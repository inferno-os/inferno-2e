#include	<windows.h>  
#include	<winsock.h>
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

extern			int	cflag;

DWORD			PlatformId;
static			char*	path;
static			HANDLE	kbdh = INVALID_HANDLE_VALUE;
static			HANDLE	conh = INVALID_HANDLE_VALUE;

_declspec(thread)       Proc    *up;

HANDLE	ntfd2h(int);
int	nth2fd(HANDLE);

extern Dev	rootdevtab, srvdevtab, fsdevtab, mntdevtab,
		condevtab, ssldevtab, drawdevtab, cmddevtab,
		progdevtab, ipdevtab, pipedevtab,
		eiadevtab, audiodevtab, kfsdevtab;

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
	&eiadevtab,
	&audiodevtab,
	&kfsdevtab,
	nil
};

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
	ExitThread(0);
}

DWORD WINAPI
tramp(LPVOID p)
{
	up = p;
	up->func(up->arg);
	pexit("", 0);
	return 0;
}

int
kproc(char *name, void (*func)(void*), void *arg)
{
	DWORD h;
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

	p->env->ui = up->env->ui;
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

	p->pid = (int)CreateThread(0, 16384, tramp, p, 0, &h);
	return p->pid;
}

void
oshostintr(Proc *p)
{
	p->intwait = 0;
}

void
oslongjmp(void *regs, osjmpbuf env, int val)
{
	USED(regs);
	longjmp(env, val);
}

int
readkbd(void)
{
	DWORD r;
	char buf[1];

	if(ReadConsole(kbdh, buf, sizeof(buf), &r, 0) == FALSE)
		panic("keyboard fail");

	if(buf[0] == '\r')
		buf[0] = '\n';
	return buf[0];
}

void
cleanexit(int x)
{
	sleep(2);		/* give user a chance to see message */
	FreeConsole();
	ExitProcess(x);
}

struct ecodes {
	DWORD	code;
	char*	name;
} ecodes[] = {
	EXCEPTION_ACCESS_VIOLATION,		"Segmentation violation",
	EXCEPTION_DATATYPE_MISALIGNMENT,	"Data Alignment",
	EXCEPTION_BREAKPOINT,                	"Breakpoint",
	EXCEPTION_SINGLE_STEP,               	"SingleStep",
	EXCEPTION_ARRAY_BOUNDS_EXCEEDED,	"Array Bounds Check",
	EXCEPTION_FLT_DENORMAL_OPERAND,		"Denormalized Float",
	EXCEPTION_FLT_DIVIDE_BY_ZERO,		"Floating Point Divide by Zero",
	EXCEPTION_FLT_INEXACT_RESULT,		"Inexact Floating Point",
	EXCEPTION_FLT_INVALID_OPERATION,	"Invalid Floating Operation",
	EXCEPTION_FLT_OVERFLOW,			"Floating Point Result Overflow",
	EXCEPTION_FLT_STACK_CHECK,		"Floating Point Stack Check",
	EXCEPTION_FLT_UNDERFLOW,		"Floating Point Result Underflow",
	EXCEPTION_INT_DIVIDE_BY_ZERO,		"Divide by Zero",
	EXCEPTION_INT_OVERFLOW,			"Integer Overflow",
	EXCEPTION_PRIV_INSTRUCTION,		"Privileged Instruction",
	EXCEPTION_IN_PAGE_ERROR,		"Page-in Error",
	EXCEPTION_ILLEGAL_INSTRUCTION,		"Illegal Instruction",
	EXCEPTION_NONCONTINUABLE_EXCEPTION,	"Non-Continuable Exception",
	EXCEPTION_STACK_OVERFLOW,		"Stack Overflow",
	EXCEPTION_INVALID_DISPOSITION,		"Ivalid Disposition",
	EXCEPTION_GUARD_PAGE,			"Guard Page Violation",
	0,					nil
};

LONG
TrapHandler(LPEXCEPTION_POINTERS ureg)
{
	int i;
	char *name;
	DWORD code;
	char buf[ERRLEN];

	code = ureg->ExceptionRecord->ExceptionCode;

	name = nil;
	for(i = 0; i < nelem(ecodes); i++) {
		if(ecodes[i].code == code) {
			name = ecodes[i].name;
			break;
		}
	}

	if(name == nil) {
		snprint(buf, sizeof(buf), "Unrecognized Machine Trap (%.8lux)\n", code);
		name = buf;
	}

	disfault(nil, name);
	return 0;		/* not reached */
}

static void
termset(void)
{
	DWORD flag;

	if(conh != INVALID_HANDLE_VALUE)
		return;
	FreeConsole();
	AllocConsole();
	conh = CreateFile("CONOUT$", GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);

	kbdh = CreateFile("CONIN$", GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);

	GetConsoleMode(kbdh, &flag);
	flag = flag & ~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT);
	SetConsoleMode(kbdh, flag);
}

void
termrestore(void)
{
	if(conh != INVALID_HANDLE_VALUE)
		CloseHandle(conh);
	conh = INVALID_HANDLE_VALUE;

	if(kbdh != INVALID_HANDLE_VALUE)
		CloseHandle(kbdh);
	kbdh = INVALID_HANDLE_VALUE;

	FreeConsole();
}

extern	int	rebootok;	/* is shutdown -r supported */

void
libinit(char *imod)
{
	WSADATA wasdat;
	DWORD evelen, lasterror;
	OSVERSIONINFO os;

	os.dwOSVersionInfoSize = sizeof(os);
	if(!GetVersionEx(&os))
		panic("can't get os version");
	PlatformId = os.dwPlatformId;
	if (PlatformId == VER_PLATFORM_WIN32_NT) {
		rebootok = 1;
	} else {
		rebootok = 0;
	}
	termset();

	if((int)INVALID_HANDLE_VALUE != -1 || sizeof(HANDLE) != sizeof(int))
		panic("invalid handle value or size");

	if(WSAStartup(MAKEWORD(1, 1), &wasdat) != 0)
		panic("no winsock.dll");

	gethostname(ossysname, sizeof(ossysname));

	if(sflag == 0)
		SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)TrapHandler);

	path = getenv("PATH");
	if(path == nil)
		path = ".";

	up = newproc();

	evelen = NAMELEN;
	if(GetUserName(eve, &evelen) != TRUE) {
		lasterror = GetLastError();	
		if(PlatformId == VER_PLATFORM_WIN32_NT || lasterror != ERROR_NOT_LOGGED_ON)
			print("cannot GetUserName: %d\n", lasterror);
	}

	emuinit(imod);
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
	HANDLE	pid;
	Tag*	next;
};

static	Tag*	ht[NHASH];
static	Tag*	ft;
static	Lock	hlock;
static	int	nsema;

int
rendezvous(void *tag, ulong value)
{
	int h;
	ulong rval;
	Tag *t, **l, *f;


	h = (ulong)tag & (NHASH-1);

	lock(&hlock);
	l = &ht[h];
	for(t = ht[h]; t; t = t->next) {
		if(t->tag == tag) {
			rval = t->val;
			t->val = value;
			t->tag = 0;
			unlock(&hlock);
			if(SetEvent(t->pid) == FALSE)
				panic("Release failed\n");
			return rval;		
		}
	}

	t = ft;
	if(t == 0) {
		t = malloc(sizeof(Tag));
		if(t == nil)
			panic("rendezvous: no memory");
		t->pid = CreateEvent(0, 0, 0, 0);
	}
	else
		ft = t->next;

	t->tag = tag;
	t->val = value;
	t->next = *l;
	*l = t;
	unlock(&hlock);

	if(WaitForSingleObject(t->pid, INFINITE) != WAIT_OBJECT_0)
		panic("WaitForSingleObject failed\n");

	lock(&hlock);
	rval = t->val;
	for(f = *l; f; f = f->next) {
		if(f == t) {
			*l = f->next;
			break;
		}
		l = &f->next;
	}
	t->next = ft;
	ft = t;
	unlock(&hlock);

	return rval;
}

int
canlock(Lock *l)
{
	int v;
	int *la;

	la = &l->key;

	_asm {
		mov	eax, la
		mov	ebx, 1
		xchg	ebx, [eax]
		mov	v, ebx
	}
	switch(v){
	case 0:
		return 1;
	case 1:
		return 0;
	default:
		print("canlock currupted 0x%lux\n", v);
	}
}

void
FPsave(void *fptr)
{
	_asm {
		mov	eax, fptr
		fstenv	[eax]
	}
}

void
FPrestore(void *fptr)
{
	_asm {
		mov	eax, fptr
		fldenv	[eax]
	}
}

ulong
umult(ulong a, ulong b, ulong *high)
{
	ulong lo, hi;

	_asm {
		mov	eax, a
		mov	ecx, b
		MUL	ecx
		mov	lo, eax
		mov	hi, edx
	}
	*high = hi;
	return lo;
}

int
close(int fd)
{
	if(fd == -1)
		return 0;
	CloseHandle(ntfd2h(fd));
}

int
read(int fd, void *buf, uint n)
{
	if(!ReadFile(ntfd2h(fd), buf, n, &n, NULL))
		return -1;
	return n;
}

int
write(int fd, void *buf, uint n)
{
	int w;

	if(fd == 1 || fd == 2){
		if(conh == INVALID_HANDLE_VALUE){
			termset();
			if(conh == INVALID_HANDLE_VALUE)
				return -1;
		}
		if(!WriteConsole(conh, buf, n, &w, NULL) || n != w)
			abort();
		return n;
	}
	if(!WriteFile(ntfd2h(fd), buf, n, &n, NULL))
		return -1;
	return n;
}

/*
 * map handles and fds.
 * this code assumes sizeof(HANDLE) == sizeof(int),
 * that INVALID_HANDLE_VALUE is -1, and assumes
 * that all tests of invalid fds check only for -1, not < 0
 */
int
nth2fd(HANDLE h)
{
	return (int)h;
}

HANDLE
ntfd2h(int fd)
{
	return (HANDLE)fd;
}

int
ntexists(char *file)
{
	return GetFileAttributes(file) != 0xFFFFFFFF;
}

/* - obc
Command filter -- translate os command and args for 95/Nt. Turns "/" into "\"
except for unmutable sections delimited as strings or escaped as follows:
	'\/' for unmutable '/' character
	'\ ' for non delimiting white space character.
This enables:
+ on 95/Nt -- commands with spaces to try and execute (on Nt Access denied
bug yet to be fixed)
+ on 95/Nt -- commands with arguments requiring '/' characters
to be preserved (e.g. as "http://xxx").
Example:
original cmd: d:/foo\ gonk/"foo zonk"/bar/goo "k://bing/bong/bang/" /"foo / \"bar / goo\" gazonk" \/"a b"\/c/
result   cmd: d:\foo gonk\foo zonk\bar\goo "k://bing/bong/bang/" \"foo / \"bar / goo\" gazonk" /"a b"/c\
cmd   length: 28
*/
int
ntescapecmd(char **cmdp)
{
	int shift = 0;
	int skip = 0;
	int strip = 1;	/* required to find commands with spaces in path */
	char *p, *p0, *s, *s0, *cmd, *tmp;
	int cmdlen;

	cmd = *cmdp;
	for(p = p0 = cmd; *p; p0 = p, p++) {
	  if(*p0 != '\\') {
	    if(*p == '"')
	      skip = !skip;
	    else if(!skip && p[0] == '$' && p[1] == 'r') {
	      int l0, l1;
	      extern char rootdir[];
	      l0 = p - cmd;
	      l1 = strlen(rootdir);
	      p += 2;
	      s = malloc(l0 + l1 + strlen(p) + 1);
	      if(s == nil) {
		p--;
		continue;
	      }
	      memmove(s, cmd, l0);
	      memmove(s + l0, rootdir, l1);
	      strcpy(s + l0 + l1, p);
	      free(cmd);
	      *cmdp = s;
	      cmd = s;
	      p = s + l0 + l1 - 1;
	    }
	  }
	}

	cmdlen = strlen(cmd);
	tmp = (char *) malloc(cmdlen * sizeof(char) + 1);
	if(tmp == nil)
		return -1;
	strcpy(tmp, cmd);

	for(p = p0 = cmd, s = s0 = tmp; *p; p0 = p, p++, s0 = s, s++) {
	  if(*s == '\"' && *s0 != '\\')
	    skip = !skip;
	  else
	  if(*p == '/' && *p0 != '\\' && !skip) 
	    *p = '\\';
	}
	
	cmdlen = skip = 0;
	for(p = p0 = cmd, s = s0 = tmp; *p; p0 = p, p++, s0 = s, s++) {
	  if(!cmdlen && !skip && *p0 != '\\' && (*p == ' ' || *p == '\t'))
	    cmdlen = p - shift - cmd;
	  else
	    if(*s == '\"' && *s0 != '\\') {
	      skip = !skip;
	      if(!cmdlen && strip) {	/* Strip  double quotes from command */
		*(p - shift) = *p;
		shift++;
		continue;
	      }
	    }
	  else
	  if(!skip && *p0 == '\\' && (*p == '/' || *p == ' '))
	    shift++;
	  *(p - shift) = *p;
	}
	*(p - shift) = 0;
	free(tmp);
	if (!cmdlen)
	  return strlen(cmd);
	else
	  return cmdlen;
}

static 
char*
searchfor(char *cmd)
{
	int dirlen, cmdlen;
	char *v, *lv, *p, *where, *suffix;
	int suffixoff;

	cmd = strdup(cmd);
	if(cmd == nil)
		return nil;
	cmdlen = ntescapecmd(&cmd);
	if(cmdlen < 0) {
		free(cmd);
		return nil;
	}
	p = cmd + cmdlen;

	lv = path;
	if(cmd[1] == ':' && cmd[2] == '\\')
		lv = "";

	where = nil;
	for(;;) {
		v = strchr(lv, ';');
		if(v == nil)
			v = lv+strlen(lv);
		dirlen = v - lv;

		/* 6 = '/' + ".exe" + EOT */
		where = realloc(where, dirlen+cmdlen+6);
		if(where == nil) {
			free(cmd);
			return nil;
		}

		memmove(where, lv, dirlen);
		if(dirlen != 0)
			where[dirlen++] = '\\';
		memmove(where+dirlen, cmd, cmdlen);
		suffix = where+dirlen+cmdlen;
		strcpy(suffix, ".exe");
		if(ntexists(where))
			break;
		strcpy(suffix, ".com");
		if(ntexists(where))
			break;
		if(*v == '\0') {
			free(cmd);
			return nil;
		}
		lv = v+1;
	}
	// now tack on the arguments to the program
	suffixoff = suffix-where;
	where = realloc(where, suffixoff+5+strlen(p));
	if(where != nil)
		strcpy((where+suffixoff)+4, p);
	free(cmd);
	return where;
}

int
oscmd(char *cmd, int *rpfd, int *wpfd)
{
	STARTUPINFO si;
	SECURITY_ATTRIBUTES sec;
	HANDLE rh, wh, srh, swh;
	PROCESS_INFORMATION pinfo;

	// dup it so we can munge on the ptr
	cmd = searchfor(cmd);
	if(cmd == nil)
		return -1;

	sec.nLength = sizeof(sec);
	sec.lpSecurityDescriptor = 0;
	sec.bInheritHandle = 1;
	if(!CreatePipe(&rh, &swh, &sec, 0)) {
		print("can't create pipe\n");
		free(cmd);
		return -1;
	}
	if(!CreatePipe(&srh, &wh, &sec, 0)) {
		print("can't create pipe\n");
		CloseHandle(rh);
		CloseHandle(swh);
		free(cmd);
		return -1;
	}

	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;
	si.wShowWindow = SW_SHOW;
	si.hStdInput = rh;
	si.hStdOutput = wh;
	si.hStdError = wh;

	if(!CreateProcess(0, cmd, 0, 0, 1,
	   CREATE_NEW_PROCESS_GROUP|CREATE_DEFAULT_ERROR_MODE,
	   0, 0, &si, &pinfo)){
		print("can't create process '%s' %d\n", cmd, GetLastError());
		CloseHandle(rh);
		CloseHandle(swh);
		CloseHandle(wh);
		CloseHandle(srh);
		free(cmd);
		return -1;
	}

	*rpfd = nth2fd(srh);
	*wpfd = nth2fd(swh);
	if(*wpfd == 1 || *wpfd == 2)
		panic("invalid mapping of handle to fd");
	CloseHandle(rh);
	CloseHandle(wh);
	// since we forget about the spawned process
	// we must close its process and thread handles
	// now, else we leak handle's
	CloseHandle(pinfo.hProcess); 
	CloseHandle(pinfo.hThread);   
	free(cmd);
	return 0;
}

static Rb rb;
extern int rbnotfull(void*);

void
osspin(Rendez *prod)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        for(;;){
                if((rb.randomcount & 0xffff) == 0 && !rbnotfull(0)) {
                        Sleep(prod, rbnotfull, 0);
                }
                rb.randomcount++;
        }
}

/* Resolve system header name conflict */
#undef Sleep
void
sleep(int secs)
{
	Sleep(secs*1000);
}

void*
sbrk(int size)
{
	void *brk;

	brk = VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE); 	
	if(brk == 0)
		return (void*)-1;

	return brk;
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
	return GetTickCount();
}

#define SEC2MIN 60L
#define SEC2HOUR (60L*SEC2MIN)
#define SEC2DAY (24L*SEC2HOUR)

/*
 *  days per month plus days/year
 */
static	int	dmsize[] =
{
	365, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
static	int	ldmsize[] =
{
	366, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/*
 *  return the days/month for the given year
 */
static int*
yrsize(int yr)
{
	/* a leap year is a multiple of 4, excluding centuries
	 * that are not multiples of 400 */
	if( (yr % 4 == 0) && (yr % 100 != 0 || yr % 400 == 0) )
		return ldmsize;
	else
		return dmsize;
}

static long
tm2sec(SYSTEMTIME *tm)
{
	long secs;
	int i, *d2m;

	secs = 0;

	/*
	 *  seconds per year
	 */
	for(i = 1970; i < tm->wYear; i++){
		d2m = yrsize(i);
		secs += d2m[0] * SEC2DAY;
	}

	/*
	 *  seconds per month
	 */
	d2m = yrsize(tm->wYear);
	for(i = 1; i < tm->wMonth; i++)
		secs += d2m[i] * SEC2DAY;

	/*
	 * secs in last month
	 */
	secs += (tm->wDay-1) * SEC2DAY;

	/*
	 * hours, minutes, seconds
	 */
	secs += tm->wHour * SEC2HOUR;
	secs += tm->wMinute * SEC2MIN;
	secs += tm->wSecond;

	return secs;
}

long
time(long *tp)
{
	SYSTEMTIME tm;
	long t;

	GetSystemTime(&tm);
	t = tm2sec(&tm);
	if(tp != nil)
		*tp = t;
	return t;
}

/*
 * Return the time since the epoch in microseconds
 * The epoch is defined at 1 Jan 1970
 */
vlong
osusectime(void)
{
	SYSTEMTIME tm;
	vlong secs;

	GetSystemTime(&tm);
	secs = tm2sec(&tm);
	return secs * 1000000 + tm.wMilliseconds * 1000;
}

int
osmillisleep(ulong milsec)
{
	SleepEx(milsec, FALSE);
	return 0;
}

void
osyield(void)
{	
	sleep(0);
}

void
ospause(void)
{
      for(;;)
              sleep(1000000);
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

/*
 * these should never be called, and are included
 * as stubs since we are linking against a library which defines them
 */
int
open(const char *path, int how, ...)
{
	panic("open");
	return -1;
}

int
creat(const char *path, int how)
{
	panic("creat");
	return -1;
}

int 
stat(const char *path, struct stat *sp)
{
	panic("stat");
	return -1;
}

int 
chown(const char *path, int uid, int gid)
{
	panic("chown");
	return -1;
}

int 
chmod(const char *path, int mode)
{
	panic("chmod");
	return -1;
}

DIR*
opendir(char *p)
{
	panic("opendir");
	return nil;
}

void
closedir(DIR *d)
{
	panic("closedir");
}

struct dirent*
readdir(DIR *d)
{
	panic("readdir");
	return nil;
}

void
rewinddir(DIR *d)
{
	panic("rewinddir");
}

void
link(char *path, char *next)
{
	panic("link");
}
