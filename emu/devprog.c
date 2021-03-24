#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	<interp.h>
#include	<isa.h>
#include	"runt.h"

/*
 * Enable the heap device for environments that allow debugging =>
 * Must be 1 for a production environment.
 */
int	SECURE = 0;

enum
{
	Qctl,
	Qdbgctl,
	Qheap,
	Qns,
	Qnsgrp,
	Qpgrp,
	Qstack,
	Qstatus,
	Qtext,
	Qwait,
	Qfd,
	Qdir
};

/*
 * must be in same order as enum
 */
Dirtab progdir[] =
{
	"ctl",		{Qctl},		0,			0200,
	"dbgctl",	{Qdbgctl},	0,			0600,
	"heap",		{Qheap},	0,			0600,
	"ns",		{Qns},		0,			0400,
	"nsgrp",	{Qnsgrp},	0,			0444,
	"pgrp",		{Qpgrp},	0,			0444,
	"stack",	{Qstack},	0,			0400,
	"status",	{Qstatus},	0,			0444,
	"text",		{Qtext},	0,			0000,
	"wait",		{Qwait},	0,			0400,
	"fd",		{Qfd},		0,			0400,
};

typedef struct Heapqry Heapqry;
struct Heapqry
{
	char	fmt;
	ulong	addr;
	ulong	module;
	int	count;
};

typedef struct Bpt	Bpt;

struct Bpt
{
	Bpt	*next;
	int	pc;
	char	*file;
	char	path[1];
};

typedef struct Progctl Progctl;
struct Progctl
{
	Rendez	r;
	int	ref;
	Proc	*debugger;	/* waiting for dbgxec */
	char	*msg;		/* reply from dbgxec */
	int	step;		/* instructions to try */
	int	stop;		/* stop running the program */
	Bpt*	bpts;		/* active breakpoints */
	Queue*	q;		/* status queue */
};

#define	QSHIFT		4		/* location in qid of pid */
#define	QID(q)		(((q).path&0x0000000F)>>0)
#define QPID(pid)	(((pid)<<QSHIFT)&~CHDIR)
#define	PID(q)		((q).vers)
#define PATH(q)		((q).path&~(CHDIR|((1<<QSHIFT)-1)))

static char *progstate[] =			/* must correspond to include/interp.h */
{
	"alt",				/* blocked in alt instruction */
	"send",				/* waiting to send */
	"recv",				/* waiting to recv */
	"debug",			/* debugged */
	"ready",			/* ready to be scheduled */
	"release",			/* interpreter released */
	"exiting",			/* exit because of kill or error */
	"broken",			/* thread crashed */
};

static	void	dbgstep(Progctl*, Prog*, int);
static	void	dbgstart(Prog*);
static	void	freebpts(Bpt*);
static	Bpt*	delbpt(Bpt*, char*, int);
static	Bpt*	setbpt(Bpt*, char*, int);
static	void	mntscan(Mntwalk*, Pgrp*);
extern	Module*	modules;
static  char 	Emisalign[] = "misaligned address";

int
proggen(Chan *c, Dirtab *tab, int ntab, int s, Dir *dp)
{
	Qid qid;
	Prog *p;
	Osenv *o;
	char buf[NAMELEN];
	ulong path, perm, len;

	USED(ntab);

	if(c->qid.path == CHDIR) {
		acquire();
		p = progn(s);
		if(p == nil) {
			release();
			return -1;
		}
		o = p->osenv;
		sprint(buf, "%d", p->pid);
		qid.path = CHDIR|(p->pid<<QSHIFT);
		qid.vers = p->pid;
		devdir(c, qid, buf, 0, o->user, CHDIR|0555, dp);
		release();
		return 1;
	}

	if(s >= nelem(progdir))
		return -1;
	tab = &progdir[s];
	path = PATH(c->qid);

	acquire();
	p = progpid(PID(c->qid));
	if(p == nil) {
		release();
		return -1;
	}

	o = p->osenv;

	perm = tab->perm;
	if((perm & 7) == 0)
		perm = (perm|(perm>>3)|(perm>>6)) & o->pgrp->progmode;

	len = tab->length;
	qid.path = path|tab->qid.path;
	qid.vers = c->qid.vers;
	devdir(c, qid, tab->name, len, o->user, perm, dp);
	release();
	return 1;
}

void
proginit(void)
{
}

void
progreset(void)
{
}

Chan*
progattach(void *spec)
{
	return devattach('p', spec);
}

Chan*
progclone(Chan *c, Chan *nc)
{
	return devclone(c, nc);
}

int
progwalk(Chan *c, char *name)
{
	int pid;
	char *e;
	Path *op;

	if(strcmp(name, "..") == 0) {
		c->qid.path = CHDIR;
		return 1;
	}

	/*
	 * do the walk to pid directory by hand
	 * to avoid races with progn
	 */
	if(c->qid.path == CHDIR) {
		pid = strtoul(name, &e, 0);
		acquire();
		if(*e == '\0' && progpid(pid) != nil) {
			release();
			c->qid.path = CHDIR|QPID(pid);
			c->qid.vers = pid;
			op = c->path;
			c->path = ptenter(&syspt, op, name);
			decref(&op->r);
			return 1;
		}
		release();
		return 0;
	}

	return devwalk(c, name, 0, 0, proggen);
}

void
progstat(Chan *c, char *db)
{
	devstat(c, db, 0, 0, proggen);
}

Chan *
progopen(Chan *c, int omode)
{
	Prog *p;
	Osenv *o;
	Progctl *ctl;
	int perm, ask;

	if(c->qid.path & CHDIR) {
		if(omode != OREAD)
			error(Eperm);
		c->offset = 0;
		c->mode = openmode(omode);
		c->flag |= COPEN;
		return c;
	}

	acquire();
	if (waserror()) {
		release();
		nexterror();
	}
	p = progpid(PID(c->qid));
	if(p == nil)
		error(Ethread);

	o = p->osenv;
	perm = progdir[QID(c->qid)].perm;
	if((perm & 7) == 0)
		perm = (perm|(perm>>3)|(perm>>6)) & o->pgrp->progmode;
	if(strcmp(up->env->user, o->user) == 0)
		perm >>= 6;
	else
	if(strcmp(up->env->user, eve) == 0)
		perm >>= 3;

	ask = 7;
	switch(omode) {
	case OREAD:
		ask = 4;
		break;
	case OWRITE:
		ask = 2;
		break;
	case ORDWR:
		ask = 6;
		break;
	}
	if((ask & perm) != ask)
		error(Eperm);

	omode = openmode(omode);

	switch(QID(c->qid)){
	default:
		error(Egreg);
	case Qnsgrp:
	case Qpgrp:
	case Qtext:
	case Qstatus:
	case Qstack:
	case Qctl:
	case Qfd:
		break;
	case Qwait:
		c->u.aux = qopen(1024, 1, nil, nil);
		if(c->u.aux == nil)
			error(Enomem);
		o = p->osenv;
		o->childq = c->u.aux;
		break;
	case Qns:
		c->u.aux = malloc(sizeof(Mntwalk));
		if(c->u.aux == nil)
			error(Enomem);
		break;
	case Qheap:
		if(SECURE || omode != ORDWR)
			error(Eperm);
		c->u.aux = malloc(sizeof(Heapqry));
		if(c->u.aux == nil)
			error(Enomem);
		break;
	case Qdbgctl:
		if(SECURE || omode != ORDWR)
			error(Eperm);
		ctl = malloc(sizeof(Progctl));
		if(ctl == nil)
			error(Enomem);
		ctl->q = qopen(1024, 1, nil, nil);
		if(ctl->q == nil) {
			free(ctl);
			error(Enomem);
		}
		ctl->bpts = nil;
		ctl->ref = 1;
		c->u.aux = ctl;
		break;
	}
	if(p->state != Pexiting)
		c->qid.vers = p->pid;

	release();
	poperror();
	c->offset = 0;
	c->mode = omode;
	c->flag |= COPEN;
	return c;
}

void
progwstat(Chan *c, char *db)
{
	Dir d;
	Prog *p;
	char *u;
	Osenv *o;

	if(c->qid.path&CHDIR)
		error(Eperm);
	acquire();
	p = progpid(PID(c->qid));
	if(p == nil) {
		release();
		error(Ethread);
	}

	u = up->env->user;
	o = p->osenv;
	if(strcmp(u, o->user) != 0 && strcmp(u, eve) != 0) {
		release();
		error(Eperm);
	}

	convM2D(db, &d);
	o->pgrp->progmode = d.mode&0777;
	release();
}

static void
closedbgctl(Progctl *ctl, Prog *p)
{
	Osenv *o;

	if(ctl->ref-- > 1)
		return;
	freebpts(ctl->bpts);
	if(p != nil){
		o = p->osenv;
		if(o->debug == ctl){
			o->debug = nil;
			p->xec = xec;
		}
	}
	qfree(ctl->q);
	free(ctl);
}

void
progclose(Chan *c)
{
	int i;
	Prog *f;
	Osenv *o;
	Progctl *ctl;

	switch(QID(c->qid)) {
	case Qns:
	case Qheap:
		free(c->u.aux);
		break;
	case Qdbgctl:
		if((c->flag & COPEN) == 0)
			return;
		ctl = c->u.aux;
		acquire();
		closedbgctl(ctl, progpid(PID(c->qid)));
		release();
		break;
	case Qwait:
		acquire();
		i = 0;
		for(;;) {
			f = progn(i++);
			if(f == nil)
				break;
			o = f->osenv;
			if(o->waitq == c->u.aux)
				o->waitq = nil;
			if(o->childq == c->u.aux)
				o->childq = nil;
		}
		release();
		qfree(c->u.aux);
	}
}

int
progsize(Prog *p)
{
	int size;
	Frame *f;
	uchar *fp;
	Modlink *m;

	m = p->R.M;
	size = 0;
	if(m->MP != H)
		size += msize(D2H(m->MP));
	if(m->prog != nil)
		size += msize(m->prog);

	fp = p->R.FP;
	while(fp != nil) {
		f = (Frame*)fp;
		fp = f->fp;
		if(f->mr != nil) {
			if(f->mr->MP != H)
				size += msize(D2H(f->mr->MP));
			if(f->mr->prog != nil)
				size += msize(f->mr->prog);
		}
		if(f->t == nil)
			size += msize(SEXTYPE(f));
	}
	return size/1024;
}

static int
progfds(Fgrp *f, char *va, int count, long offset)
{
	Chan *c;
	int n, i;

	n = 0;
	for(i = 0; i <= f->maxfd; i++) {
		c = f->fd[i];
		if(c == nil)
			continue;
		n += snprint(va+n, count-n, "%3d %.2s %.8lux.%.8d %8d ",
			i,
			&"r w rw"[(c->mode&3)<<1],
			c->qid.path, c->qid.vers,
			c->offset);
		n += ptpath(c->path, va+n, count-n);
		n += snprint(va+n, count-n, "\n");
		if(offset > 0) {
			offset -= n;
			if(offset < 0) {
				memmove(va, va+n+offset, -offset);
				n = -offset;
			}
			else
				n = 0;
		}
	}
	return n;
}

static void *
progpc(Exception *exsp, Inst *pc, uchar *fp)
{
	Exception *l;

	for(l = exsp; l != nil; l = l->link)
		if(l->R.FP == fp)
			return l->lr;
	return pc;
}

static int
progstack(REG *reg, int state, Exception *exsp, char *va, int count, long offset)
{
	int n;
	Frame *f;
	Inst *pc;
	uchar *fp;
	Modlink *m;

	n = 0;
	m = reg->M;
	fp = reg->FP;
	pc = reg->PC;

	/*
	 * all states other than debug and ready block,
	 * but interp has already advanced the PC
	 */
	if(!m->compiled && state != Pready && state != Pdebug && pc > m->prog)
		pc--;

	while(fp != nil) {
		f = (Frame*)fp;
		n += snprint(va+n, count-n, "%.8lux %.8lux %.8lux %.8lux %d %s\n",
				f,		/* FP */
				pc - m->prog,	/* PC in dis instructions */
				m->MP,		/* MP */
				m->prog,	/* Code for module */
				m->compiled,	/* True if native assembler */
				m->m->path);	/* File system path */

		if(offset > 0) {
			offset -= n;
			if(offset < 0) {
				memmove(va, va+n+offset, -offset);
				n = -offset;
			}
			else
				n = 0;
		}

		pc = f->lr;
		if(pc == m->m->eclr)
			pc = progpc(exsp, pc, fp);
		fp = f->fp;
		if(f->mr != nil)
			m = f->mr;
		if(!m->compiled)
			pc--;
	}
	return n;
}

static int
calldepth(REG *reg)
{
	int n;
	uchar *fp;

	n = 0;
	for(fp = reg->FP; fp != nil; fp = ((Frame*)fp)->fp)
		n++;
	return n;
}

int
progheap(Heapqry *hq, char *va, int count, ulong offset)
{
	WORD *w;
	void *p;
	List *hd;
	Array *a;
	char *fmt, *str;
	Module *m;
	ulong addr;
	String *ss;
	union { REAL r; LONG l; WORD w[2]; } rock;
	int i, s, n, len, signed_off;

	n = 0;
	s = 0;
	signed_off = offset;
	addr = hq->addr;
	for(i = 0; i < hq->count; i++) {
		switch(hq->fmt) {
		case 'W':
			if(addr & 3)
				return -1;
			n += snprint(va+n, count-n, "%d\n", *(WORD*)addr);
			s = sizeof(WORD);
			break;
		case 'B':
			n += snprint(va+n, count-n, "%d\n", *(BYTE*)addr);
			s = sizeof(BYTE);
			break;
		case 'V':
			if(addr & 3)
				return -1;
			w = (WORD*)addr;
			rock.w[0] = w[0];
			rock.w[1] = w[1];
			n += snprint(va+n, count-n, "%lld\n", rock.l);
			s = sizeof(LONG);
			break;
		case 'R':
			if(addr & 3)
				return -1;
			w = (WORD*)addr;
			rock.w[0] = w[0];
			rock.w[1] = w[1];
			n += snprint(va+n, count-n, "%g\n", rock.r);
			s = sizeof(REAL);
			break;
		case 'I':
			if(addr & 3)
				return -1;
			for(m = modules; m != nil; m = m->link)
				if(m == (Module*)hq->module)
					break;
			if(m == nil)
				error(Ebadctl);
			addr = (ulong)(m->prog+addr);
			n += snprint(va+n, count-n, "%D\n", addr);
			s = sizeof(Inst);
			break;
		case 'P':
			if(addr & 3)
				return -1;
			p = *(void**)addr;
			fmt = "nil\n";
			if(p != H)
				fmt = "%lux\n";
			n += snprint(va+n, count-n, fmt, p);
			s = sizeof(WORD);
			break;
		case 'L':
			if(addr & 3)
				return -1;
			hd = *(List**)addr;
			if(hd == H || D2H(hd)->t != &Tlist)
				return -1;
			n += snprint(va+n, count-n, "%lux.%lux\n", &hd->tail, hd->data);
			s = sizeof(WORD);
			break;
		case 'A':
			if(addr & 3)
				return -1;
			a = *(Array**)addr;
			if(a == H)
				n += snprint(va+n, count-n, "nil\n");
			else {
				if(D2H(a)->t != &Tarray)
					return -1;
				n += snprint(va+n, count-n, "%d.%lux\n", a->len, a->data);
			}
			s = sizeof(WORD);
			break;
		case 'C':
			if(addr & 3)
				return -1;
			ss = *(String**)addr;
			if(ss == H)
				ss = &snil;
			else
			if(D2H(ss)->t != &Tstring)
				return -1;
			n += snprint(va+n, count-n, "%d.", abs(ss->len));
			str = string2c(ss);
			len = strlen(str);
			if(count-n < len)
				len = count-n;
			if(len > 0) {
				memmove(va+n, str, len);
				n += len;
			}
			break;
		}
		addr += s;
		if(signed_off > 0) {
			signed_off -= n;
			if(signed_off < 0) {
				memmove(va, va+n+signed_off, -signed_off);
				n = -signed_off;
			}
			else
				n = 0;
		}
	}
	return n;
}

WORD
modstatus(REG *r, char *ptr, int len)
{
	Inst *PC;
	Frame *f;

	if(r->M->m->name[0] == '$') {
		f = (Frame*)r->FP;
		snprint(ptr, len, "%s[%s]", f->mr->m->name, r->M->m->name);
		if(f->mr->compiled)
			return (WORD)f->lr;
		return f->lr - f->mr->prog;
	}
	memmove(ptr, r->M->m->name, len);
	if(r->M->compiled)
		return (WORD)r->PC;
	PC = r->PC;
	/* should really check for blocked states */
	if(PC > r->M->prog)
		PC--;
	return PC - r->M->prog;
}

long
progread(Chan *c, void *va, long n, ulong offset)
{
	int i;
	Prog *p;
	Osenv *o;
	Mntwalk *mw;
	ulong grpid;
	char *a = va;
	Progctl *ctl;
	char buf[128], mbuf[2*NAMELEN];

	if(c->qid.path & CHDIR) {
		n = devdirread(c, a, n, 0, 0, proggen);
		return n;
	}

	switch(QID(c->qid)){
	case Qdbgctl:
		ctl = c->u.aux;
		return qread(ctl->q, va, n);
	case Qstatus:
		acquire();
		p = progpid(PID(c->qid));
		if(p == nil || p->state == Pexiting) {
			release();
			error(Ethread);
		}
		modstatus(&p->R, mbuf, sizeof(mbuf));
		o = p->osenv;
		snprint(buf, sizeof(buf), "%8d %8d %10s %10s %5dK %s",
			p->pid,
			p->grp,
			o->user,
			progstate[p->state],
			progsize(p),
			mbuf);
		release();
		return readstr(offset, va, n, buf);
	case Qwait:
		return qread(c->u.aux, va, n);
	case Qns:
		acquire();
		p = progpid(PID(c->qid));
		if(p == nil) {
			release();
			error(Ethread);
		}
		mw = c->u.aux;
		o = p->osenv;
		mntscan(mw, o->pgrp);
		if(mw->mh == 0) {
			release();
			return 0;
		}
		if(n < NAMELEN+11) {
			release();
			error(Etoosmall);
		}
		i = sprint(a, "%s %d ", mw->cm->spec, mw->cm->flag);
		n -= i;
		a += i;
		i = ptpath(mw->mh->from->path, a, n);
		n -= i;
		a += i;
		if(n > 0) {
			*a++ = ' ';
			n--;
		}
		i = ptpath(mw->cm->to->path, a, n);
		release();
		n -= i;
		a += i;
		if(n > 0)
			*a++ = '\n';
		return a - (char*)va;
	case Qnsgrp:
		acquire();
		p = progpid(PID(c->qid));
		if(p == nil) {
			release();
			error(Ethread);
		}
		grpid = ((Osenv *)p->osenv)->pgrp->pgrpid;
		release();
		return readnum(offset, va, n, grpid, NUMSIZE);
	case Qpgrp:
		acquire();
		p = progpid(PID(c->qid));
		if(p == nil) {
			release();
			error(Ethread);
		}
		grpid = p->grp;
		release();
		return readnum(offset, va, n, grpid, NUMSIZE);
	case Qstack:
		acquire();
		p = progpid(PID(c->qid));
		if(p == nil || p->state == Pexiting) {
			release();
			error(Ethread);
		}
		if(p->state == Pready) {
			release();
			error(Estopped);
		}
		n = progstack(&p->R, p->state, p->exsp, va, n, offset);
		release();
		return n;
	case Qheap:
		acquire();
		n = progheap(c->u.aux, va, n, offset);
		release();
		if(n == -1)
			error(Emisalign);
		return n;
	case Qfd:
		acquire();
		p = progpid(PID(c->qid));
		if(waserror()) {
			release();
			nexterror();
		}
		if(p == nil)
			error(Ethread);
		o = p->osenv;
		n = progfds(o->fgrp, va, n, offset);
		release();
		poperror();
		return n;
	}
	error(Egreg);
	return 0;
}

Block*
progbread(Chan *c, long n, ulong offset)
{
	return devbread(c, n, offset);
}

static void
mntscan(Mntwalk *mw, Pgrp *pg)
{
	Mount *t;
	Mhead *f;
	int nxt, i;
	ulong last, bestmid;

	rlock(&pg->ns);

	nxt = 0;
	bestmid = ~0;

	last = 0;
	if(mw->mh)
		last = mw->cm->mountid;

	for(i = 0; i < MNTHASH; i++) {
		for(f = pg->mnthash[i]; f; f = f->hash) {
			for(t = f->mount; t; t = t->next) {
				if(mw->mh == 0 ||
				  (t->mountid > last && t->mountid < bestmid)) {
					mw->cm = t;
					mw->mh = f;
					bestmid = mw->cm->mountid;
					nxt = 1;
				}
			}
		}
	}
	if(nxt == 0)
		mw->mh = 0;

	runlock(&pg->ns);
}

long
progwrite(Chan *c, void *va, long n, ulong offset)
{
	Prog *p, *f;
	Heapqry *hq;
	char buf[512];
	Progctl *ctl;
	char *b, *a = va;
	int i, grp, ngrp, pc, *pids;

	USED(offset);
	USED(va);

	if(c->qid.path & CHDIR)
		error(Eisdir);

	acquire();
	if(waserror()) {
		release();
		nexterror();
	}
	p = progpid(PID(c->qid));
	if(p == nil)
		error(Ethread);

	switch(QID(c->qid)){
	case Qctl:
		grp = p->grp;
		if((n==7)&&(strncmp(a, "killgrp", 7) == 0)){
			pids = malloc(nprog() * sizeof(int));
			if(pids == nil)
				error(Enomem);
			i = 0;
			ngrp = 0;
			for(;;) {
				f = progn(i++);
				if(f == nil)
					break;
				if(f->grp == grp)
					pids[ngrp++] = f->pid;
			}
			if(waserror()) {
				free(pids);
				nexterror();
			}
			for(i = 0; i < ngrp; i++) {
				f = progpid(pids[i]);
				if(f != nil && f != currun())
					killprog(f, "killed");
			}
			poperror();
			free(pids);
			n = i;
			break;
		}
		if((n==4)&&(strncmp(a, "kill", 4) == 0)){
			killprog(p, "killed");
			break;
		}
		error(Ebadctl);
	case Qdbgctl:
		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, a, n);
		buf[n] = '\0';
		if(strncmp(buf, "step", 4) == 0) {
			i = strtoul(buf+4, nil, 0);
			dbgstep(c->u.aux, p, i);
			break;
		}
		if(strncmp(buf, "toret", 5) == 0) {
			f = currun();
			i = calldepth(&p->R);
			while(f->kill == nil) {
				dbgstep(c->u.aux, p, 1024);
				if(i > calldepth(&p->R))
					break;
			}
			break;
		}
		if(strncmp(buf, "cont", 4) == 0) {
			f = currun();
			while(f->kill == nil)
				dbgstep(c->u.aux, p, 1024);
			break;
		}
		if(strncmp(buf, "start", 5) == 0) {
			dbgstart(p);
			break;
		}
		if(strncmp(buf, "stop", 4) == 0) {
			ctl = c->u.aux;
			ctl->stop = 1;
			break;
		}
		if(strncmp(buf, "unstop", 6) == 0) {
			ctl = c->u.aux;
			ctl->stop = 0;
			break;
		}
		if(strncmp(buf, "bpt ", 4) == 0) {
			b = strchr(buf+8, ' ');
			if(b == nil)
				error(Ebadctl);
			*b++ = '\0';
			pc = strtoul(b, nil, 10);
			ctl = c->u.aux;
			if(strncmp(buf+4, "set ", 4) == 0)
				ctl->bpts = setbpt(ctl->bpts, buf+8, pc);
			else if(strncmp(buf+4, "del ", 4) == 0)
				ctl->bpts = delbpt(ctl->bpts, buf+8, pc);
			break;
		}
		if(strncmp(buf, "maim", 4) == 0) {
			p->kill = "maim";
			break;
		}
		error(Ebadctl);
	case Qheap:
		/*
		 * Heap query:
		 *	addr.Fn
		 *	pc+module.In
		 */
		i = n;
		if(i > sizeof(buf)-1)
			i = sizeof(buf)-1;
		memmove(buf, va, i);
		buf[i] = '\0';
		hq = c->u.aux;
		hq->addr = strtoul(buf, &b, 0);
		if(*b == '+')
			hq->module = strtoul(b, &b, 0);
		if(*b++ != '.')
			error(Ebadctl);
		hq->fmt = *b++;
		hq->count = strtoul(b, nil, 0);
		break;
	default:
		print("unknown qid in procwrite\n");
		error(Egreg);
	}
	release();
	poperror();
	return n;
}

long
progbwrite(Chan *c, Block *bp, ulong offset)
{
	return devbwrite(c, bp, offset);
}

static Bpt*
setbpt(Bpt *bpts, char *path, int pc)
{
	int n;
	Bpt *b;

	n = strlen(path);
	b = mallocz(sizeof *b + n, 0);
	if(b == nil)
		return bpts;
	b->pc = pc;
	memmove(b->path, path, n+1);
	b->file = b->path;
	path = strrchr(b->path, '/');
	if(path != nil)
		b->file = path + 1;
	b->next = bpts;
	return b;
}

static Bpt*
delbpt(Bpt *bpts, char *path, int pc)
{
	Bpt *b, **last;

	last = &bpts;
	for(b = bpts; b != nil; b = b->next){
		if(b->pc == pc && strcmp(b->path, path) == 0) {
			*last = b->next;
			free(b);
			break;
		}
		last = &b->next;
	}
	return bpts;
}

static void
freebpts(Bpt *b)
{
	Bpt *next;

	for(; b != nil; b = next){
		next = b->next;
		free(b);
	}
}

static void
telldbg(Progctl *ctl, char *msg)
{
	strncpy(ctl->msg, msg, ERRLEN);
	ctl->debugger = nil;
	Wakeup(&ctl->r);
}

static void
dbgstart(Prog *p)
{
	Osenv *o;
	Progctl *ctl;

	o = p->osenv;
	ctl = o->debug;
	if(ctl != nil && ctl->debugger != nil)
		error("prog debugged");
	if(p->state == Pdebug)
		addrun(p);
	o->debug = nil;
	p->xec = xec;
}

static int
xecdone(void *vc)
{
	Progctl *ctl = vc;

	return ctl->debugger == nil;
}

static void
dbgstep(Progctl *vctl, Prog *p, int n)
{
	volatile struct {Osenv *o;} o;
	volatile struct {Progctl *ctl;} ctl;
	char buf[ERRLEN+20], *msg;

	if(p == currun())
		error("cannot step yourself");

	if(p->state == Pbroken)
		error("prog broken");

	ctl.ctl = vctl;
	if(ctl.ctl->debugger != nil)
		error("prog already debugged");

	o.o = p->osenv;
	if(o.o->debug == nil) {
		o.o->debug = ctl.ctl;
		p->xec = dbgxec;
	}else if(o.o->debug != ctl.ctl)
		error("prog already debugged");

	ctl.ctl->step = n;
	if(p->state == Pdebug)
		addrun(p);
	ctl.ctl->debugger = up;
	strcpy(buf, "child: ");
	msg = buf+7;
	ctl.ctl->msg = msg;

	/*
	 * wait for reply from dbgxec; release is okay because prog is now
	 * debugged, cannot exit.
	 */
	if(waserror()){
		acquire();
		ctl.ctl->debugger = nil;
		ctl.ctl->msg = nil;
		o.o->debug = nil;
		p->xec = xec;
		nexterror();
	}
	release();
	Sleep(&ctl.ctl->r, xecdone, ctl.ctl);
	poperror();
	acquire();
	if(msg[0] != '\0')
		error(buf);
}

void
dbgexit(Prog *kid, int broken, char *estr)
{
	int n;
	Osenv *e;
	Progctl *ctl;
	char buf[2*ERRLEN];

	e = kid->osenv;
	ctl = e->debug;
	e->debug = nil;
	kid->xec = xec;

	if(broken)
		n = snprint(buf, sizeof(buf), "broken: %s", estr);
	else
		n = snprint(buf, sizeof(buf), "exited");
	if(ctl->debugger)
		telldbg(ctl, buf);
	qproduce(ctl->q, buf, n);
}

static void
dbgaddrun(Prog *p)
{
	Osenv *o;

	p->state = Pdebug;
	p->addrun = nil;
	o = p->osenv;
	if(o->rend != nil)
		Wakeup(o->rend);
	o->rend = nil;
}

static int
bdone(void *vp)
{
	Prog *p = vp;

	return p->addrun == nil;
}

static void
dbgblock(Prog *p)
{
	Osenv *o;
	Progctl *ctl;

	o = p->osenv;
	ctl = o->debug;
	qproduce(ctl->q, progstate[p->state], strlen(progstate[p->state]));
	pushrun(p);
	p->addrun = dbgaddrun;
	o->rend = &up->sleep;

	/*
	 * bdone(p) is safe after release because p is being debugged,
	 * so cannot exit.
	 */
	if(waserror()){
		acquire();
		nexterror();
	}
	release();
	Sleep(o->rend, bdone, p);
	poperror();
	acquire();
	if(p->kill != nil)
		error(p->kill);
	ctl = o->debug;
	if(ctl != nil)
		qproduce(ctl->q, "run", 3);
}

void
dbgxec(Prog *p)
{
	Bpt *b;
	Prog *kid;
	Osenv *env;
	Progctl *ctl;
	int op, pc, n;
	char buf[2*ERRLEN];
	extern void (*dec[])(void);

	env = p->osenv;
	ctl = env->debug;
	ctl->ref++;
	if(waserror()){
		closedbgctl(ctl, p);
		nexterror();
	}

	R = p->R;
	R.MP = R.M->MP;

	R.IC = p->quanta;
	if((ulong)R.IC > ctl->step)
		R.IC = ctl->step;
	if(ctl->stop)
		R.IC = 0;


	buf[0] = '\0';

	if(R.IC != 0 && R.M->compiled) {
		comvec();
		if(p != currun())
			dbgblock(p);
		goto save;
	}

	while(R.IC != 0) {
		if(0)
			print("step: %lux: %s %4d %D\n",
				p, R.M->m->name, R.PC-R.M->prog, R.PC);

		dec[R.PC->add]();
		op = R.PC->op;
		R.PC++;
		optab[op]();

		/*
		 * check notification about new progs
		 */
		if(op == ISPAWN || op == IMSPAWN) {
			/* pick up the kid from the end of the run queue */
			kid = delruntail(Pdebug);
			n = snprint(buf, ERRLEN, "new %d", kid->pid);
			qproduce(ctl->q, buf, n);
			buf[0] = '\0';
		}

		/*
		 * check for returns at big steps
		 */
		if(op == IRET)
			R.IC = 1;

		/*
		 * check for blocked progs
		 */
		if(R.IC == 1 && p != currun())
			dbgblock(p);
		if(ctl->stop)
			R.IC = 1;
		R.IC--;

		if(ctl->bpts == nil)
			continue;

		pc = R.PC - R.M->prog;
		for(b = ctl->bpts; b != nil; b = b->next) {
			if(pc == b->pc &&
			  (strcmp(R.M->m->path, b->path) == 0 ||
			   strcmp(R.M->m->path, b->file) == 0)) {
				strcpy(buf, "breakpoint");
				goto save;
			}
		}
	}
save:
	if(ctl->stop)
		strcpy(buf, "stopped");

	p->R = R;

	if(env->debug == nil)
	{
		poperror();
		return;
	}

	if(p == currun())
		delrun(Pdebug);

	telldbg(env->debug, buf);
	poperror();
	closedbgctl(env->debug, p);
}

Dev progdevtab = {
	'p',
	"prog",

	proginit,
	progattach,
	progclone,
	progwalk,
	progstat,
	progopen,
	devcreate,
	progclose,
	progread,
	progbread,
	progwrite,
	progbwrite,
	devremove,
	progwstat
};
