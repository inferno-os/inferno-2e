/*
 *  kernel disk file system
 */
#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

#include "../kfs/fdat.h"
#include "../kfs/ffns.h"

enum {
	Kfscons = 32,	/* Chan.dev for console; should probably assign separate devchar */

	Qdir = 0,
	Qcons,
	Qctl,
};

extern	void	mntdirfix(uchar*, Chan*);

static	Dirtab	kfsdir[] =
{
	{"kfscons",	{Qcons},	0,	0660},
	{"kfsctl",	{Qctl}, 0, 0220},
};
#define	NKFSDIR	(sizeof(kfsdir)/sizeof(Dirtab))

static void
kfsinit(void)
{
	kfsmain(0, 0);
}

static int
fcall(void (*f)(FChan*, Fcall*, Fcall*), FChan *cp, Fcall *in, Fcall *ou)
{
	ou->ename[0] = 0;
	rlock(&mainlock);
	rlock(&cp->reflock);
	if(waserror()){
		/* shouldn't happen */
		print("fcall err: %s\n", up->env->error);
		strcpy(ou->ename, up->env->error);
	} else {
		f(cp, in, ou);
		poperror();
	}
	runlock(&cp->reflock);
	runlock(&mainlock);
	return ou->ename[0] != 0;
}

/*
 * spec=="cons" switches to the special console/ctl file system:
 *	#Kcons/kfsctl	init/ream a file system
 *	#Kcons/kfscons	kfs console i/o
 * but all other #Kx access the file system named x
 *
 * the console should be split into a separate devchar
 * when things settle down
 */
static Chan *
kfsattach(char *spec)
{
	Chan *c;
	Fcall in, ou;
	Filsys *fs;

	if(strcmp(spec, "cons") == 0){
		c = devattach('K', spec);
		c->dev = Kfscons;
		return c;
	}

	c = devattach('K', spec);
	c->aux = malloc(sizeof(FChan));
	if(c->aux == nil) {
		cclose(c);
		error(Enomem);
	}

	in.fid = c->fid;
	if(up->env)
		strcpy(in.uname, up->env->user);
	else
		strcpy(in.uname, eve);
	strncpy(in.aname, spec, sizeof(in.aname));

	fcall(f_attach, c->aux, &in, &ou);

	if(ou.ename[0]) {
		cclose(c);
		error(ou.ename);
	}
	
	fs = fsstr(in.aname);
	if(fs == nil)
		panic("kfsattach");	/* shouldn't happen */
	c->dev = fs - filsys;

	return c;
}

static Chan *
kfsclone(Chan *c, Chan *nc)
{
	Fcall in, ou;

	if(c->dev == Kfscons)
		return devclone(c, nc);

	nc = devclone(c, nc);

	in.fid = c->fid;
	in.newfid = nc->fid;

	fcall(f_clone, c->aux, &in, &ou);

	if(ou.ename[0]) {
		cclose(nc);
		error(ou.ename);
	}

	return nc;
}

static int
kfswalk(Chan *c, char *name)
{
	Fcall in, ou;
	Path *op;

	if(c->dev == Kfscons)
		return devwalk(c, name, kfsdir, NKFSDIR, devgen);

	in.fid = c->fid;
	strcpy(in.name, name);

	fcall(f_walk, c->aux, &in, &ou);

	if(ou.ename[0]){
		strncpy(up->env->error, ou.ename, sizeof(up->env->error));
		return 0;
	}

	c->qid = ou.qid;
	op = c->path;
	c->path = ptenter(&syspt, op, name);
	decref(op);

	return 1;
}

static void
kfsstat(Chan *c, char *db)
{
	Fcall in, ou;

	if(c->dev == Kfscons){
		devstat(c, db, kfsdir, NKFSDIR, devgen);
		return;
	}

	in.fid = c->fid;

	fcall(f_stat, c->aux, &in, &ou);

	if(ou.ename[0])
		error(ou.ename);

	memmove(db, ou.stat, DIRLEN);
	mntdirfix((uchar*)db, c);
}

static Chan *
kfsopen(Chan *c, int omode)
{
	Fcall in, ou;

	if(c->dev == Kfscons){
		switch(c->qid.path & ~CHDIR){
		case Qctl:
			if(!iseve())
				error(Eperm);
			break;
		case Qcons:
			qlock(&fscons);
			if(waserror()){
				qunlock(&fscons);
				nexterror();
			}
			if(fscons.opened)
				error(Einuse);
			c = devopen(c, omode, kfsdir, NKFSDIR, devgen);
			if(fscons.out == nil)
				fscons.out = qopen(8*1024, 0, 0, 0);
			else
				qreopen(fscons.out);
			fscons.opened = 1;
			qunlock(&fscons);
			poperror();
			return c;
		}
		return devopen(c, omode, kfsdir, NKFSDIR, devgen);
	}

	in.fid = c->fid;
	in.mode = omode;

	fcall(f_open, c->aux, &in, &ou);

	if(ou.ename[0])
		error(ou.ename);

	c->offset = 0;
	c->qid = ou.qid;
	if((c->qid.path&CHDIR) && omode!=OREAD)
		error(Eperm);
	c->mode = openmode(omode);
	c->flag |= COPEN;
	return c;
}

static void
kfscreate(Chan *c, char *name, int omode, ulong perm)
{
	Fcall in, ou;

	if(c->dev == Kfscons)
		error(Eperm);

	in.fid = c->fid;
	in.mode = omode;
	in.perm = perm;
	strcpy(in.name, name);

	fcall(f_create, c->aux, &in, &ou);

	if(ou.ename[0])
		error(ou.ename);
	c->qid = ou.qid;
	if((c->qid.path&CHDIR) && omode!=OREAD)
		error(Eperm);
	c->offset = 0;
	c->flag |= COPEN;
	c->mode = openmode(omode);
}

static void
kfsremove(Chan *c)
{
	Fcall in, ou;

	if(c->dev == Kfscons)
		error(Eperm);

	in.fid = c->fid;

	fcall(f_remove, c->aux, &in, &ou);

	if(ou.ename[0])
		error(ou.ename);
}

static void
kfswstat(Chan *c, char *dp)
{
	Fcall in, ou;

	if(c->dev == Kfscons)
		error(Eperm);

	in.fid = c->fid;
	memmove(in.stat, dp, DIRLEN);

	fcall(f_wstat, c->aux, &in, &ou);

	if(ou.ename[0])
		error(ou.ename);
}

static void
kfsclose(Chan *c)
{
	Fcall in, ou;


	if(c->dev == Kfscons){
		if((c->flag & COPEN) == 0)
			return;
		if(c->qid.path == Qcons){
			qclose(fscons.out);
			fscons.opened = 0;
		}
		return;
	}

	in.fid = c->fid;

	fcall(f_clunk, c->aux, &in, &ou);

	if(ou.ename[0])
		error(ou.ename);
}

static long
kfsread(Chan *c, void *a, long n, ulong offset)
{
	Fcall in, ou;
	uchar *p, *e;
	int isdir;

	if(c->dev == Kfscons){
		if(n <= 0)
			return n;
		switch(c->qid.path & ~CHDIR){
		default:
			return 0;
		case Qdir:
			return devdirread(c, a, n, kfsdir, NKFSDIR, devgen);
		case Qcons:
			return qread(fscons.out, a, n);
		}
	}

	if(n < 0)
		error(Ebadarg);

	in.fid = c->fid;
	in.data = a;	/* actually uses ou.data */
	in.count = n;
	in.offset = offset;
	ou.data = a;

	isdir = c->qid.path & CHDIR;
	fcall(f_read, c->aux, &in, &ou);

	if(ou.ename[0])
		error(ou.ename);
	n = ou.count;
	if(isdir){
		p = a;
		for(e = &p[n]; p < e; p += DIRLEN)
			mntdirfix(p, c);
	}

	return n;
}

static long
kfswrite(Chan *c, char *a, long n, ulong offset)
{
	Fcall in, ou;

	if(c->dev == Kfscons){
		char buf[100];

		if(n <= 0)
			return n;
		switch(c->qid.path & ~CHDIR){
		default:
			error(Egreg);

		case Qctl:
		case Qcons:
			if(n >= sizeof(buf))
				n = sizeof(buf)-1;
			strncpy(buf, a, n);
			buf[n] = 0;
			if(n > 0 && buf[n-1] == '\n')
				buf[n-1] = 0;
			if(c->qid.path == Qctl)
				fsctl(buf);
			else if(!cmd_exec(buf))
				error("kfs: invalid command");
			break;
		}
		return n;
	}

	if(n < 0)
		error(Ebadarg);
	in.fid = c->fid;
	in.data = a;
	in.count = n;
	in.offset = offset;

	fcall(f_write, c->aux, &in, &ou);

	if(ou.ename[0])
		error(ou.ename);

	return ou.count;
}

Dev kfsdevtab = {
	'K',
	"kfs",

	devreset,
	kfsinit,
	kfsattach,
	devdetach,
	kfsclone,
	kfswalk,
	kfsstat,
	kfsopen,
	kfscreate,
	kfsclose,
	kfsread,
	devbread,
	kfswrite,
	devbwrite,
	kfsremove,
	kfswstat,
};
