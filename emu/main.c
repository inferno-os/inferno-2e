#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"interp.h"
#include	"kernel.h"
#include	"image.h"
#include	"version.h"

int		rebootargc = 0;
char**		rebootargv;
static	char	imod[128] = "/dis/sh.dis";
static	char	dmod[128] = "/dis/lib/srv.dis";
extern	char*	tkfont;
extern	int	mflag;
	int	dflag;
	Ref	pgrpid;
	Ref	mountid;
	ulong	kerndate;
	Procs	procs;
	char	eve[NAMELEN] = "inferno";
	int	Xsize	= 640;
	int	Ysize	= 480;
	int	sflag;
	int	qflag;
	int	xtblbit;
	int	IXsize	= MaxImageWidth;
	int	IYsize	= MaxImageHeight;
	int	IMsize	= MaxImageSize;

static void
savestartup(int argc, char *argv[])
{
int i;
	rebootargc = argc;

	rebootargv = (char**) malloc((argc+1)*sizeof(char*));
	if(!rebootargv)
		panic("alloc failed saving startup");

	for(i = 0; i < argc; i++) {
		rebootargv[i] = strdup(argv[i]);
		if(!rebootargv[i])
			panic("alloc failed saving startup arg");
	}
	rebootargv[i] = nil;
}

static void
usage(void)
{
	fprint(2, "Usage: emu [options...] [file.dis]\n"
		"\t-gXxY\n"
		"\t-iXxYxM\n"
		"\t-c[0-9]\n"
		"\t-d[012]\n"
		"\t-m[0-9]\n"
		"\t-s\n"
		"\t-p<poolname>=maxsize\n"
		"\t-f<fontpath>\n"
		"\t-r<rootpath>\n"
		"\t-7\n");

	exits("usage");
}

static void
geom(char *val)
{
	char *p;

	if (val == '\0') 
		usage();
	Xsize = strtoul(val, &p, 0);
	if(Xsize < 64) 
		Xsize = 640;
	if (p == '\0') {
		Ysize = 480;
		return;
	}
	Ysize = strtoul(p+1, 0, 0);
	if(Ysize < 48)
		Ysize = 480;
}

static void
imag(char *val)
{
	char *p;
	uint h, w, s;

	if (val == '\0') 
		usage();
	w = strtoul(val, &p, 0);
	if(w > 0)
		IXsize = w;
	if (p == '\0') 
		return;
	h = strtoul(p+1, &p, 0);
	if(h > 0)
		IYsize = h;
	if (p == '\0') 
		return;
	s = strtoul(p+1, 0, 0);
	if(s > 0)
		IMsize = s * 1024;
		
	return;
}

static void
poolopt(char *str)
{
	char *var;

	var = str;
	while(*str && *str != '=')
		str++;
	if(*str != '=')
		usage();
	*str++ = '\0';
	if(poolsetsize(var, atoi(str)) == 0)
		usage();
}

static int
option(char *str)
{
	int c, i, done;

	while(*str == ' ')
		str++;
	if(*str == '\0')
		return 0;
	if(str[0] != '-') {
		strncpy(imod, str, sizeof(imod));
		return 0;
	}

	done = 0;
	for(i = 1; str[i] != '\0' && !done; i++) {
		switch(str[i]) {
		default:
			usage();
		case 'i':		/* Image geometry */
			done = 1;
			imag(&str[i+1]);
			break;
		case 'g':		/* Window geometry */
			done = 1;
			geom(&str[i+1]);
			break;
		case 'c':		/* Compile on the fly */
			done = 1;
			c = str[i+1];
			if(c < '0' || c > '9')
				usage();
			cflag = atoi(&str[i+1]);
			break;
		case 'd':		/* run as a daemon */
			done = 1;
			c = str[i+1];
			if(c < '0' || c > '2')
				usage();
			dflag = atoi(&str[i+1]);
			strncpy(imod, dmod, sizeof(imod));
			break;
		case 's':		/* No trap handling */
			sflag++;
			break;
		case 'm':		/* gc mark and sweep */
			done = 1;
			c = str[i+1];
			if(c < '0' || c > '9')
				usage();
			mflag = atoi(&str[i+1]);
			break;
		case 'p':		/* pool option */
			done = 1;
			poolopt(&str[i+1]);
			break;
		case 'f':		/* Set font path */
			done = 1;
			if(str[i+1] == '\0')
				usage();
			tkfont = &str[i+1];
			break;
		case 'r':		/* Set inferno root */
			done = 1;
			if(str[i+1] == '\0')
				usage();
			strncpy(rootdir, &str[i+1], sizeof(rootdir)-1);
			break;
		case '7':		/* use 7 bit colormap in X */
			xtblbit = 1;
			break;
		}
	}
	return 1;
}

void
main(int argc, char *argv[])
{
	int i, done;
	char *opt, *p;

	savestartup(argc, argv);
	opt = getenv("EMU");
	if(opt != nil && *opt != '\0') {
		done = 0;
		while(done == 0) {
			p = opt;
			while(*p && *p != ' ')
				p++;
			if(*p != '\0')
				*p = '\0';
			else
				done = 1;
			if(!option(opt))
				break;
			opt = p+1;
		}
	}
	for(i = 1; i < argc; i++)
		if(!option(argv[i]))
			break;


	kerndate = time(0);

	opt = "interp";
	if(cflag)
		opt = "compile";

	print("Inferno %s main (pid=%d) %s\n", VERSION, getpid(), opt);

	libinit(imod);
}

void
emuinit(void *imod)
{
	Osenv *e;

	e = up->env;
	e->pgrp = newpgrp();
	e->fgrp = newfgrp();

	chandevinit();

	if(waserror())
		panic("setting root and dot");

	e->pgrp->slash = namec("#/", Atodir, 0, 0);
	e->pgrp->dot = cclone(e->pgrp->slash, nil);
	poperror();

	strcpy(up->text, "main");

	if(kopen("#c/cons", OREAD) != 0)
		fprint(2, "failed to make fd0 from #c/cons: %r\n");
	kopen("#c/cons", OWRITE);
	kopen("#c/cons", OWRITE);

	/* the setid cannot precede the bind of #U */
	kbind("#U", "/", MAFTER|MCREATE);
	setid(eve);
	kbind("#D", "/dev", MBEFORE);
	kbind("#c", "/dev", MBEFORE);
	kbind("#p", "/prog", MREPL);
	kbind("#I", "/net", MAFTER);	/* will fail on Brazil and Plan 9 */

	/* BUG: we actually only need to do these on Brazil and Plan 9 */
	kbind("#U/dev", "/dev", MAFTER);
	kbind("#U/net", "/net", MAFTER);

	kproc("main", disinit, imod);

	for(;;)
		ospause(); 
}

void
modinit(void)
{
	sysmodinit();
	drawmodinit();
	prefabmodinit();
	tkmodinit();
	mathmodinit();
	srvrtinit();
	keyringmodinit();
	loadermodinit();
}

void
error(char *err)
{
	strncpy(up->env->error, err, ERRLEN);
	nexterror();
}

void
exhausted(char *resource)
{
	char buf[ERRLEN];

	snprint(buf, sizeof(buf), "no free %s", resource);
	error(buf);
}

void
nexterror(void)
{
	oslongjmp(nil, up->estack[--up->nerr], 1);
}

Pgrp*
newpgrp(void)
{
	Pgrp *p;

	p = malloc(sizeof(Pgrp));
	if(p == nil)
		error(Enomem);
	p->r.ref = 1;
	p->pgrpid = incref(&pgrpid);
	p->pin = Nopin;
	p->progmode = 0644;
	return p;
}

Fgrp*
newfgrp(void)
{
	Fgrp *f;

	f = malloc(sizeof(Fgrp));
	if(f == nil)
		error(Enomem);
	f->r.ref = 1;

	return f;
}

void
closepgrp(Pgrp *p)
{
	Mhead **h, **e, *f, *next;

	if(decref(&p->r) != 0)
		return;

	p->pgrpid = -1;
	e = &p->mnthash[MNTHASH];
	for(h = p->mnthash; h < e; h++) {
		for(f = *h; f; f = next) {
			cclose(f->from);
			mountfree(f->mount);
			next = f->hash;
			free(f);
		}
	}
	cclose(p->slash);
	cclose(p->dot);
	free(p);
}

Fgrp*
dupfgrp(Fgrp *f)
{
	int i;
	Chan *c;
	Fgrp *new;

	new = newfgrp();

	lock(&f->r.l);
	new->maxfd = f->maxfd;
	for(i = 0; i <= f->maxfd; i++) {
		if(c = f->fd[i]){
			incref(&c->r);
			new->fd[i] = c;
		}
	}
	unlock(&f->r.l);

	return new;
}

void
closefgrp(Fgrp *f)
{
	int i;
	Chan *c;

	if(decref(&f->r) == 0) {
		for(i = 0; i <= f->maxfd; i++)
			if(c = f->fd[i])
				cclose(c);

		free(f);
	}
}

void
pgrpinsert(Mount **order, Mount *m)
{
	Mount *f;

	m->order = 0;
	if(*order == 0) {
		*order = m;
		return;
	}
	for(f = *order; f; f = f->order) {
		if(m->mountid < f->mountid) {
			m->order = f;
			*order = m;
			return;
		}
		order = &f->order;
	}
	*order = m;
}

/*
 * pgrpcpy MUST preserve the mountid allocation order of the parent group
 */
void
pgrpcpy(Pgrp *to, Pgrp *from)
{
	int i;
	Mount *n, *m, **link, *order;
	Mhead *f, **tom, **l, *mh;

	rlock(&from->ns);
	order = 0;
	tom = to->mnthash;
	for(i = 0; i < MNTHASH; i++) {
		l = tom++;
		for(f = from->mnthash[i]; f; f = f->hash) {
			mh = malloc(sizeof(Mhead));
			if(mh == nil) {
				runlock(&from->ns);
				error(Enomem);
			}
			mh->from = f->from;
			incref(&mh->from->r);
			*l = mh;
			l = &mh->hash;
			link = &mh->mount;
			for(m = f->mount; m; m = m->next) {
				n = malloc(sizeof(Mount));
				if(n == nil) {
					runlock(&from->ns);
					error(Enomem);
				}
				n->to = m->to;
				incref(&n->to->r);
				n->head = mh;
				n->flag = m->flag;
				if(m->spec != 0)
					strcpy(n->spec, m->spec);
				m->copy = n;
				pgrpinsert(&order, m);
				*link = n;
				link = &n->next;	
			}
		}
	}
	/*
	 * Allocate mount ids in the same sequence as the parent group
	 */
	lock(&mountid.l);
	for(m = order; m; m = m->order)
		m->copy->mountid = mountid.ref++;
	unlock(&mountid.l);

	to->pin = from->pin;

	to->slash = cclone(from->slash, nil);
	to->dot = cclone(from->dot, nil);
	to->nodevs = from->nodevs;
	runlock(&from->ns);
}

Mount*
newmount(Mhead *mh, Chan *to, int flag, char *spec)
{
	Mount *m;

	m = malloc(sizeof(Mount));
	if(m == nil)
		error(Enomem);
	m->to = to;
	m->head = mh;
	incref(&to->r);
	m->mountid = incref(&mountid);
	m->flag = flag;
	if(spec != 0)
		strcpy(m->spec, spec);

	return m;
}

void
mountfree(Mount *m)
{
	Mount *f;

	while(m) {
		f = m->next;
		cclose(m->to);
		m->mountid = 0;
		free(m);
		m = f;
	}
}

Proc*
newproc(void)
{
	Proc *p;

	p = malloc(sizeof(Proc));
	if(p == nil)
		return nil;

	p->type = Unknown;
	p->env = &p->defenv;
	addprog(p);

	return p;
}

void
panic(char *fmt, ...)
{
	va_list arg;
	char buf[512];

	va_start(arg, fmt);
	doprint(buf, buf+sizeof(buf), fmt, arg);
	va_end(arg);
	fprint(2, "panic: %s\n", buf);
	if(sflag)
		abort();

	cleanexit(0);
}
