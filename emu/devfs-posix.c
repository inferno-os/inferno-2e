/*
 * Unix file system interface
 */
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

#include	<sys/stat.h>
#include	<sys/types.h>
#include	<sys/fcntl.h>
#include	<utime.h>
#include	"styx.h"
#include	"stdio.h"
#include	"pwd.h"
#include 	"grp.h"

enum
{
	IDSHIFT	= 8,
	NID	= 1 << IDSHIFT,
	IDMASK	= NID - 1,
	MAXPATH	= 1024,
	MAXCOMP	= 128
};

typedef struct Pass Pass;
struct Pass
{
	int	id;
	int	gid;
	char*	name;
	Pass*	next;
};

char	rootdir[MAXROOT] = "/usr/inferno/";

static	Pass*	uid[NID];
static	Pass*	gid[NID];
static	Pass*	member[NID];
static	RWlock	idl;

static	Qid	fsqid(struct stat *);
static	void	fspath(Path*, char*, char*);
static	void	id2name(Pass**, int, char*);
static	int ingroup(int id, int gid);
static	void	fsperm(Chan*, int);
static	ulong	fsdirread(Chan*, uchar*, int, ulong);
static	int	fsomode(int);
static	Pass*	name2pass(Pass**, char*);
static	void	getpwdf(void);
static	void	getgrpf(void);

/* Unix libc */

extern	struct passwd *getpwent(void);
extern	struct group *getgrent(void);

void
fsinit(void)
{
}

Chan*
fsattach(void *spec)
{
	Chan *c;
	struct stat stbuf;
	char err[ERRLEN];

	getpwdf();
	getgrpf();

	if(stat(rootdir, &stbuf) < 0) {
		oserrstr(err);
		error(err);
	}

	c = devattach('U', spec);
	c->qid = fsqid(&stbuf);
	c->u.uif.gid = stbuf.st_gid;
	c->u.uif.uid = stbuf.st_uid;
	c->u.uif.mode = stbuf.st_mode;
	c->dev = 0;

	return c;
}

Chan*
fsclone(Chan *c, Chan *nc)
{
	return devclone(c, nc);
}

int
fswalk(Chan *c, char *name)
{
	Path *op;
	struct stat stbuf;
	char path[MAXPATH];

	fspath(c->path, name, path);
/*
	print("** ufs walk '%s' -> %s\n", path, name);
*/
	if(stat(path, &stbuf) < 0)
		return 0;

	c->u.uif.gid = stbuf.st_gid;
	c->u.uif.uid = stbuf.st_uid;
	c->u.uif.mode = stbuf.st_mode;

	c->qid = fsqid(&stbuf);

	op = c->path;
	c->path = ptenter(&syspt, op, name);
	decref(&op->r);

	return 1;
}

void
fsstat(Chan *c, char *buf)
{
	Dir d;
	struct stat stbuf;
	char path[MAXPATH], err[ERRLEN];

	fspath(c->path, 0, path);
	if(stat(path, &stbuf) < 0) {
		oserrstr(err);
		error(err);
	}

	strncpy(d.name, c->path->elem, NAMELEN);
	rlock(&idl);
	id2name(uid, stbuf.st_uid, d.uid);
	id2name(gid, stbuf.st_gid, d.gid);
	runlock(&idl);
	d.qid = c->qid;
	d.mode = (c->qid.path&CHDIR)|(stbuf.st_mode&0777);
	d.atime = stbuf.st_atime;
	d.mtime = stbuf.st_mtime;
	if(S_ISDIR(stbuf.st_mode) | S_ISREG(stbuf.st_mode))
		d.length = stbuf.st_size;
	else
		d.length = 0;
	d.type = 'U';
	d.dev = c->dev;
	convD2M(&d, buf);
}

Chan*
fsopen(Chan *c, int mode)
{
	int m, trunc, isdir;
	char path[MAXPATH], err[ERRLEN];

	m = mode & (OTRUNC|3);
	switch(m) {
	case 0:
		fsperm(c, 4);
		break;
	case 1:
	case 1|16:
		fsperm(c, 2);
		break;
	case 2:	
	case 0|16:
	case 2|16:
		fsperm(c, 4);
		fsperm(c, 2);
		break;
	case 3:
		fsperm(c, 1);
		break;
	default:
		error(Ebadarg);
	}

	isdir = c->qid.path & CHDIR;

	if(isdir && mode != OREAD)
		error(Eperm);

	m = fsomode(m & 3);
	c->mode = openmode(mode);

	fspath(c->path, 0, path);
	if(isdir) {
		c->u.uif.dir = opendir(path);
		if(c->u.uif.dir == 0) {
			oserrstr(err);
			error(err);
		}
	}	
	else {
		if(mode & OTRUNC)
			m |= O_TRUNC;
		c->u.uif.fd = open(path, m, 0666);
		if(c->u.uif.fd < 0) {
			oserrstr(err);
			error(err);
		}
	}

	c->offset = 0;
	c->u.uif.offset = 0;
	c->flag |= COPEN;
	return c;
}

void
fscreate(Chan *c, char *name, int mode, ulong perm)
{
	Path *op;
	int fd, m;
	struct stat stbuf;
	char path[MAXPATH], err[ERRLEN];

	fsperm(c, 2);

	m = fsomode(mode&3);

	fspath(c->path, name, path);

	if(perm & CHDIR) {
		if(m)
			error(Eperm);

		if(mkdir(path, 0777) < 0) {
    Error:
			oserrstr(err);
			error(err);
		}

		fd = open(path, 0);
		if(fd >= 0) {
			fchmod(fd, c->u.uif.mode & perm & 0777);
			fchown(fd, up->env->uid, c->u.uif.gid);
		}
		close(fd);
		c->u.uif.dir = opendir(path);
		if(c->u.uif.dir == 0)
			goto Error;
	}
	else {
		fd = creat(path, 0666);
		if(fd >= 0) {
			if(m != 1) {
				close(fd);
				fd = open(path, m);
			}
			fchmod(fd, (perm & 0111) | (c->u.uif.mode & perm & 0666));
			fchown(fd, up->env->uid, c->u.uif.gid);
		}
		if(fd < 0)
			goto Error;
		c->u.uif.fd = fd;
	}
	if(stat(path, &stbuf) < 0) {
		close(fd);
		goto Error;
	}
	c->qid = fsqid(&stbuf);
	c->u.uif.gid = stbuf.st_gid;
	c->u.uif.uid = stbuf.st_uid;
	c->u.uif.mode = stbuf.st_mode;
	c->mode = openmode(mode);
	c->offset = 0;
	c->u.uif.offset = 0;
	c->flag |= COPEN;
	op = c->path;
	c->path = ptenter(&syspt, op, name);
	decref(&op->r);
}

void
fsclose(Chan *c)
{
	if((c->flag & COPEN) == 0)
		return;

	if(c->flag & CRCLOSE) {
		if(!waserror()) {
			fsremove(c);
			poperror();
		}
	}

	if(c->qid.path & CHDIR)
		closedir(c->u.uif.dir);
	else
		close(c->u.uif.fd);
}

long
fsread(Chan *c, void *va, long n, ulong offset)
{
	int fd, r;
	char err[ERRLEN];

	qlock(&c->u.uif.oq);
	if(waserror()) {
		qunlock(&c->u.uif.oq);
		nexterror();
	}

	if(c->qid.path & CHDIR)
		n = fsdirread(c, va, n, offset);
	else {
		fd = c->u.uif.fd;
		if(c->u.uif.offset != offset) {
			r = lseek(fd, offset, 0);
			if(r < 0) {
				oserrstr(err);
				error(err);
			}
			c->u.uif.offset = offset;
		}

		n = read(fd, va, n);
		if(n < 0) {
			oserrstr(err);
			error(err);
		}
		c->u.uif.offset += n;
	}

	qunlock(&c->u.uif.oq);
	poperror();

	return n;
}

long
fswrite(Chan *c, void *va, long n, ulong offset)
{
	int fd, r;
	char err[ERRLEN];

	qlock(&c->u.uif.oq);
	if(waserror()) {
		qunlock(&c->u.uif.oq);
		nexterror();
	}
	fd = c->u.uif.fd;
	if(c->u.uif.offset != offset) {
		r = lseek(fd, offset, 0);
		if(r < 0) {
			oserrstr(err);
			error(err);
		}
		c->u.uif.offset = offset;
	}

	n = write(fd, va, n);
	if(n < 0) {
		oserrstr(err);
		error(err);
	}

	c->u.uif.offset += n;
	qunlock(&c->u.uif.oq);
	poperror();

	return n;
}

void
fswchk(char *path)
{
	struct stat stbuf;
	char err[ERRLEN];

	if(stat(path, &stbuf) < 0) {
		oserrstr(err);
		error(err);
	}

	if(stbuf.st_uid == up->env->uid)
		stbuf.st_mode >>= 6;
	else
	if(stbuf.st_gid == up->env->gid || ingroup(up->env->uid, stbuf.st_gid))
		stbuf.st_mode >>= 3;

	if(stbuf.st_mode & S_IWOTH)
		return;

	error(Eperm);
}

void
fsremove(Chan *c)
{
	int n;
	char path[MAXPATH], err[ERRLEN];

	fspath(c->path->parent, 0, path);
	fswchk(path);		
	fspath(c->path, 0, path);
	if(c->qid.path & CHDIR)
		n = rmdir(path);
	else
		n = remove(path);
	if(n < 0) {
		oserrstr(err);
		error(err);
	}
}

void
fswstat(Chan *c, char *buf)
{
	Dir d;
	Pass *p;
	Path *ph;
	struct stat stbuf;
	struct utimbuf utbuf;
	char old[MAXPATH], new[MAXPATH], dir[MAXPATH], err[ERRLEN];

	convM2D(buf, &d);
	
	fspath(c->path, 0, old);
	if(stat(old, &stbuf) < 0) {
    Error:
		oserrstr(err);
		error(err);
	}

	if(strcmp(d.name, c->path->elem) != 0) {
		fspath(c->path->parent, 0, dir);
		fswchk(dir);		
		fspath(c->path, 0, old);
		ph = ptenter(&syspt, c->path->parent, d.name);
		fspath(ph, 0, new);
		if(rename(old, new) < 0)
			goto Error;

		decref(&c->path->r);
		c->path = ph;
	}

	fspath(c->path, 0, old);
	if((d.mode&0777) != (stbuf.st_mode&0777)) {
		if(up->env->uid != stbuf.st_uid)
			error(Eowner);
		if(chmod(old, d.mode&0777) < 0)
			goto Error;
		c->u.uif.mode &= ~0777;
		c->u.uif.mode |= d.mode&0777;
	}
	if((d.mtime != stbuf.st_mtime) ||
	   (d.atime != stbuf.st_atime) ) {
		if(up->env->uid != stbuf.st_uid)
			error(Eowner);
		utbuf.modtime = d.mtime;
		utbuf.actime  = d.atime;
		if(utime(old, &utbuf) < 0)
			goto Error;
	}

	rlock(&idl);
	p = name2pass(gid, d.gid);
	if(p == 0) {
		runlock(&idl);
		error(Eunknown);
	}

	if(p->id != stbuf.st_gid) {
		if(up->env->uid != stbuf.st_uid) {
			runlock(&idl);
			error(Eowner);
		}
		if(chown(old, stbuf.st_uid, p->id) < 0) {
			runlock(&idl);
			goto Error;
		}

		c->u.uif.gid = p->id;
	}
	runlock(&idl);
}

#define MAXDEV 128

static Qid
fsqid(struct stat *st)
{
	Qid q;
	int dev, idev;
	static int nqdev = 0;
	static ushort *qdev = 0;
	static Lock l;

	/*
	 * There is no need to lock qdev, since we are single-threaded
	 * when we first execute here.
	 */
	if(qdev == 0) {
		qdev = malloc(MAXDEV * sizeof(ushort));
		if(qdev == 0)
			panic("fsqid: no memory");
	}

	q.path = 0;
	if(S_ISDIR(st->st_mode))
		q.path = CHDIR;

	dev = st->st_dev & 0xFFFFUL;
	lock(&l);
	for(idev = 0; idev < nqdev; idev++)
		if(qdev[idev] == (ushort)dev)
			break;
	if(idev == nqdev) {
		if(nqdev == MAXDEV) {
			unlock(&l);
			error("too many devices");
		}
		qdev[nqdev++] = dev;
	}
	unlock(&l);

	if (idev & 0xffffff00) {
		error("device number too large");
	}

	if (st->st_ino & 0xff000000UL) {
		error("inode number too large");
	}

	q.path |= idev<<24;
	q.path |= st->st_ino & 0x00FFFFFFUL;
	q.vers = st->st_mtime;

	return q;
}

static void
fspath(Path *p, char *ext, char *path)
{
	int i, n;
	char *comp[MAXCOMP];

	strcpy(path, rootdir);
	if(p == 0) {
		if(ext)
			strcpy(path+1, ext);
		return;
	}
	i = strlen(rootdir);

	n = 0;
	if(ext)
		comp[n++] = ext;
	while(p->parent) {
		comp[n++] = p->elem;
		p = p->parent;
	}

	while(n) {
		 if(path[i-1] != '/')
		    	path[i++] = '/';

		strcpy(path+i, comp[--n]);
		i += strlen(comp[n]);
	}
	path[i] = '\0';
}

/*
 * Assuming pass is one of the static arrays protected by idl, caller must
 * hold idl in writer mode.
 */
static void
freepass(Pass **pass)
{
	int i;
	Pass *p, *np;

	for(i=0; i<NID; i++){
		for(p = pass[i]; p; p = np){
			np = p->next;
			free(p);
		}
		pass[i] = 0; 
	}
}

static void
getpwdf(void)
{
	unsigned i;
	Pass *p;
	static mtime;		/* serialized by idl */
	struct stat stbuf;
	struct passwd *pw;

	if(stat("/etc/passwd", &stbuf) < 0)
		panic("can't read /etc/passwd");

	/*
	 * Unlocked peek is okay, since the check is a heuristic (as is
	 * the function).
	 */
	if(stbuf.st_mtime <= mtime)
		return;

	wlock(&idl);
	if(stbuf.st_mtime <= mtime) {
		/*
		 * If we lost a race on updating the database, we can
		 * avoid some work.
		 */
		wunlock(&idl);
		return;
	}
	mtime = stbuf.st_mtime;
	freepass(uid);
	setpwent();
	while(pw = getpwent()){
		i = pw->pw_uid;
		i = (i&IDMASK) ^ ((i>>IDSHIFT)&IDMASK);
		p = realloc(0, sizeof(Pass));
		if(p == 0)
			panic("getpwdf");

		p->next = uid[i];
		uid[i] = p;
		p->id = pw->pw_uid;
		p->gid = pw->pw_gid;
		p->name = strdup(pw->pw_name);
		if(p->name == 0)
			panic("no memory");
	}

	wunlock(&idl);
	endpwent();
}

static void
getgrpf(void)
{
	static mtime;		/* serialized by idl */
	struct stat stbuf;
	struct group *pw;
	unsigned i;
	int j;
	Pass *p, *q;

	if(stat("/etc/group", &stbuf) < 0)
		panic("can't read /etc/group");

	/*
	 * Unlocked peek is okay, since the check is a heuristic (as is
	 * the function).
	 */
	if(stbuf.st_mtime <= mtime)
		return;

	wlock(&idl);
	if(stbuf.st_mtime <= mtime) {
		/*
		 * If we lost a race on updating the database, we can
		 * avoid some work.
		 */
		wunlock(&idl);
		return;
	}
	mtime = stbuf.st_mtime;
	freepass(gid);
	freepass(member);
	/*
	 *	Pass one -- group name to gid mapping.
	 */
	setgrent();
	while(pw = getgrent()){
		i = pw->gr_gid;
		i = (i&IDMASK) ^ ((i>>IDSHIFT)&IDMASK);
		p = realloc(0, sizeof(Pass));
		if(p == 0)
			panic("getpwdf");
		p->next = gid[i];
		gid[i] = p;
		p->id = pw->gr_gid;
		p->gid = 0;
		p->name = strdup(pw->gr_name);
		if(p->name == 0)
			panic("no memory");
	}
	/*
	 *	Pass two -- group memberships.
	 */
	setgrent();
	while(pw = getgrent()){
		for (j = 0;; j++) {
			if (pw->gr_mem[j] == nil)
				break;
			q = name2pass(gid, pw->gr_mem[j]);
			if (q == nil)
				continue;
			i = q->id + pw->gr_gid;
			i = (i&IDMASK) ^ ((i>>IDSHIFT)&IDMASK);
			p = realloc(0, sizeof(Pass));
			if(p == 0)
				panic("getpwdf");
			p->next = member[i];
			member[i] = p;
			p->id = q->id;
			p->gid = pw->gr_gid;
		}
	}

	wunlock(&idl);
	endgrent();
}

/* Caller must hold idl.  Does not raise an error. */
static Pass*
name2pass(Pass **pw, char *name)
{
	int i;
	static Pass *p;
	static Pass **pwdb;

	if(p && (pwdb == pw) && (strcmp(name, p->name) == 0))
		return p;

	for(i=0; i<NID; i++)
		for(p = pw[i]; p; p = p->next)
			if(strcmp(name, p->name) == 0) {
				pwdb = pw;
				return p;
			}

	return 0;
}

/* Caller must hold idl.  Does not raise an error. */
static void
id2name(Pass **pw, int id, char *name)
{
	int i;
	Pass *p;
	char *s;

	s = nil;
	/* use last on list == first in file */
	i = (id&IDMASK) ^ ((id>>IDSHIFT)&IDMASK);
	for(p = pw[i]; p; p = p->next)
		if(p->id == id)
			s = p->name;
	if(s != nil)
		strncpy(name, s, NAMELEN);
	else
		snprint(name, NAMELEN, "%d", id);
}

/* Caller must hold idl.  Does not raise an error. */
static int
ingroup(int id, int gid)
{
	int i;
	Pass *p;

	i = id+gid;
	i = (id&IDMASK) ^ ((id>>IDSHIFT)&IDMASK);
	for(p = member[i]; p; p = p->next)
		if(p->id == id && p->gid == gid)
			return 1;
	return 0;
}

static void
fsperm(Chan *c, int mask)
{
	int m;

	m = c->u.uif.mode;
/*
	print("fsperm: %o %o uuid %d ugid %d cuid %d cgid %d\n",
		m, mask, up->env->uid, up->env->gid, c->u.uif.uid, c->u.uif.gid);
*/
	if(c->u.uif.uid == up->env->uid)
		m >>= 6;
	else
	if(c->u.uif.gid == up->env->gid || ingroup(up->env->uid, c->u.uif.gid))
		m >>= 3;

	m &= mask;
	if(m == 0)
		error(Eperm);
}

static int
isdots(char *name)
{
	if(name[0] != '.')
		return 0;
	if(name[1] == '\0')
		return 1;
	if(name[1] != '.')
		return 0;
	if(name[2] == '\0')
		return 1;
	return 0;
}

static ulong
fsdirread(Chan *c, uchar *va, int count, ulong offset)
{
	int i;
	Dir d;
	long n, o;
	DIRTYPE *de;
	Path *p, *np;
	struct stat stbuf;
	char path[MAXPATH], dirpath[MAXPATH];
	int sf;

	count = (count/DIRLEN)*DIRLEN;

	i = 0;

	if(c->u.uif.offset != offset) {
		c->u.uif.offset = offset;  /* sync offset */
		seekdir(c->u.uif.dir, 0);
		for(n=0; n<offset; ) {
			de = readdir(c->u.uif.dir);
			if(de == 0)
				break;
			if(de->d_ino==0 || de->d_name[0]==0 || isdots(de->d_name))
				continue;
			n += DIRLEN;
		}
	}

	fspath(c->path, 0, dirpath);

	/*
	 * Take idl on behalf of id2name.  Stalling attach, which is a
	 * rare operation, until the readdir completes is probably
	 * preferable to adding lock round-trips.
	 */
	rlock(&idl);
	while(i < count) {
		de = readdir(c->u.uif.dir);
		if(de == 0)
			break;

		if(de->d_ino==0 || de->d_name[0]==0 || isdots(de->d_name))
			continue;

		strncpy(d.name, de->d_name, NAMELEN-1);
		d.name[NAMELEN-1] = 0;
		sprint(path, "%s/%s", dirpath, de->d_name);
		memset(&stbuf, 0, sizeof stbuf);

		sf = 1;
		if(stat(path, &stbuf) < 0) {
			fprintf(stderr, "dir: bad path %s\n", path);
			sf = 0;
			/* but continue... probably a bad symlink */
		}
		id2name(uid, stbuf.st_uid, d.uid);
		id2name(gid, stbuf.st_gid, d.gid);
		d.qid = fsqid(&stbuf);
		d.mode = (d.qid.path&CHDIR)|(stbuf.st_mode&0777);
		d.atime = stbuf.st_atime;
		d.mtime = stbuf.st_mtime;
		if(sf && (S_ISDIR(stbuf.st_mode) | S_ISREG(stbuf.st_mode)))
			d.length = stbuf.st_size;
		else
			d.length = 0;
		d.type = 'U';
		d.dev = c->dev;
		convD2M(&d, va+i);
		i += DIRLEN;
	}
	runlock(&idl);
	return i;
}

static int
fsomode(int m)
{
	switch(m) {
	case 0:			/* OREAD */
	case 3:			/* OEXEC */
		return 0;
	case 1:			/* OWRITE */
		return 1;
	case 2:			/* ORDWR */
		return 2;
	}
	error(Ebadarg);
	return 0;
}

void
setid(char *name)
{
	Pass *p;

	strncpy(up->env->user, name, NAMELEN-1);
	up->env->user[NAMELEN-1] = 0;

	rlock(&idl);
	p = name2pass(uid, name);
	if(p == nil){
		runlock(&idl);
		up->env->uid = -1;
		up->env->gid = -1;
		return;
	}

	up->env->uid = p->id;
	up->env->gid = p->gid;
	runlock(&idl);
}

Dev fsdevtab = {
	'U',
	"fs",

	fsinit,
	fsattach,
	fsclone,
	fswalk,
	fsstat,
	fsopen,
	fscreate,
	fsclose,
	fsread,
	devbread,
	fswrite,
	devbwrite,
	fsremove,
	fswstat
};
