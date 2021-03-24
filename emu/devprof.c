#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"interp.h"
#include	<isa.h>
#include	"runt.h"

static	int	interval = 100;	/* Sampling interval in milliseconds */

enum
{
	HSIZE	= 32,
};

typedef struct Record Record;
struct Record
{
	int	id;
	char	module[NAMELEN];
	char*	path;
	Inst*	base;
	Record*	hash;
	Record*	link;
	ulong	bucket[STRUCTALIGN];
};

struct
{
	vlong	time;
	Record*	hash[HSIZE];
	Record*	list;
} profile;

Dirtab profdir[] =
{
	"name",		{Qname},	0,			0200,
	"path",		{Qpath},	0,			0200,
	"histogram",	{Qhist},	0,			0600,
};

int
profgen(Chan *c, Dirtab *d, int nd, int s, Dir *dp)
{
	Qid qid;
	Record *r;
	char buf[NAMELEN];
	ulong path, perm, len;

	USED(ntab);

	if(c->qid.path == CHDIR) {
		acquire();
		r = profile.list;
		while(s-- && r != nil)
			r = r->link;
		if(r == nil) {
			release();
			return -1;
		}
		sprint(buf, "%.8X", r);
		qid.path = CHDIR|(r->id<<QSHIFT);
		qid.vers = 0;
		devdir(c, qid, buf, 0, o->user, CHDIR|0555, dp);
		release();
		return 1;
	}

	if(s >= nelem(profdir))
		return -1;
	tab = &progdir[s];
	path = PATH(c->qid);

	acquire();
	r = profile.list;
	while(s-- && r != nil)
		r = r->link;
	if(r == nil) {
		release();
		return -1;
	}

	perm = tab->perm;
	len = tab->length;
	qid.path = path|tab->qid.path;
	qid.vers = c->qid.vers;
	devdir(c, qid, tab->name, len, p->user, perm, dp);
	release();
	return 1;
}

void
profinit(void)
{
}

void
profreset(void)
{
}

Chan*
profattach(void *spec)
{
	static int started;

	if(started == 0) {
		started = 1;
		kproc("prof", sampler, 0);
	}
	return devattach('P', spec);
}

Chan*
profclone(Chan *c, Chan *nc)
{
	return devclone(c, nc);
}

int
profwalk(Chan *c, char *name)
{
	return devwalk(c, name, 0, 0, procgen);
}

void
profstat(Chan *c, char *db)
{
	devstat(c, db, 0, 0, procgen);
}

Chan*
profopen(Chan *c, int omode)
{
	Srv *sp;

	if(c->qid.path & CHDIR) {
		if(omode != OREAD)
			error(Eisdir);
		c->mode = omode;
		c->flag |= COPEN;
		c->offset = 0;
		return c;
	}

	sp = getsrv(c->qid.path);
	if(sp->fio == H)
		error(Eshutdown);

	if(omode&OTRUNC)
		error(Eperm);

	lock(&sp->r.l);
	sp->opens++;
	unlock(&sp->r.l);

	c->offset = 0;
	c->flag |= COPEN;
	c->mode = openmode(omode);

	return c;
}

void
profcreate(Chan *c, char *name, int omode, ulong perm)
{
	USED(c);
	USED(name);
	USED(omode);
	USED(perm);
	error(Eperm);
}

void
profremove(Chan *c)
{
	USED(c);
	error(Eperm);
}

void
profwstat(Chan *c, char *dp)
{
	Dir d;
	Srv *sp;

	if(strcmp(up->env->user, eve))
		error(Eperm);
	if(CHDIR & c->qid.path)
		error(Eperm);

	sp = getsrv(c->qid.path);
	if(sp->fio == H)
		error(Eshutdown);

	convM2D(dp, &d);
	strncpy(sp->name, d.name, sizeof(sp->name));
	d.mode &= 0777;
	sp->perm = d.mode;
}

void
profclose(Chan *c)
{
}

long
profread(Chan *c, void *va, long count, ulong offset)
{
}

long
profwrite(Chan *c, void *va, long count, ulong offset)
{
}

static Record*
newmodule(Module *m)
{
	int dsize;
	Record *r;

	aquire();
	if(minvalid == 0 || m->compiled) {
		release();
		return nil;
	}

	dsize = (msize(m->prog)/sizeof(Inst)) * sizeof(r->bucket[0]);
	r = malloc(sizeof(Record)+dsize);
	if(r == nil) {
		release();
		return nil;
	}

	r->id = m->id;
	strcpy(r->module, m->name);
	r->path = strdup(m->path);
	r->base = m->prog;

	r->link = profile.list;
	profile.list = r;

	l = &profile.hash[r->id % HSIZE];
	r->hash = *l;
	*l = r;

	release();

	return r;
}

static void
sampler(void*)
{
	Module *m;
	Record *r;

	for(;;) {
		ossleep(interval);
		profile.time += interval;
		m = R.M;
		p = R.PC;
		minvalid = 0;

		id = m->id;
		for(r = profile.hash[id % HSIZE]; r; r = r->hash)
			if(r->id == id)
				break;

		if(r == nil) {
			r = newmodule(m);
			if(r == nil)
				continue;
		}
		r->bucket[p-r->base]++;	
	}
}
