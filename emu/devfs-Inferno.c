/*
 * Brazil file system interface
 */
#include	"dat.h"
#include	"fns.h"
#include	"error.h"

char	rootdir[MAXROOT] = ROOT;
static	int	rootdirlen;
static	void	fspath(Path*, char*, char*);
static	int	isstream(char*);

enum
{
	MAXPATH	= 1024,
	MAXCOMP	= 128
};

void
fsinit(void)
{
	rootdirlen = strlen(rootdir)+1;	/* +1 for slash */
}

Chan*
fsattach(void *spec)
{
	Chan *c;
	char ebuf[ERRLEN], buf[DIRLEN];

	if(stat(rootdir, buf) < 0) {
		oserrstr(ebuf);
		error(ebuf);
	}

	c = devattach('U', spec);
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
	Dir dir;
	char path[MAXPATH];

	fspath(c->path, name, path);
/*
	print("** ufs walk '%s' -> %s\n", path, name);
*/
	if(dirstat(path, &dir) < 0)
		return 0;

	c->qid = dir.qid;

	op = c->path;
	c->path = ptenter(&syspt, op, name);
	decref(&op->r);

	return 1;
}

void
fsstat(Chan *c, char *buf)
{
	char path[MAXPATH], ebuf[ERRLEN];

	fspath(c->path, 0, path);
	if(stat(path, buf) < 0) {
		oserrstr(ebuf);
		error(ebuf);
	}
}

Chan*
fsopen(Chan *c, int mode)
{
	char path[MAXPATH], ebuf[ERRLEN];

	fspath(c->path, 0, path);
	c->u.uif.stream = isstream(path);
	osenter();
	c->u.uif.fd = open(path, mode);
	osleave();
	if(c->u.uif.fd < 0) {
		oserrstr(ebuf);
		error(ebuf);
	}
	c->mode = openmode(mode);
	c->offset = 0;
	c->u.uif.offset = 0;
	c->flag |= COPEN;
	return c;
}

void
fscreate(Chan *c, char *name, int mode, ulong perm)
{
	Dir d;
	Path *op;
	char path[MAXPATH], ebuf[ERRLEN];

	fspath(c->path, name, path);
	c->u.uif.fd = create(path, mode, perm);

	if(c->u.uif.fd < 0) {
		oserrstr(ebuf);
		error(ebuf);
	}
	if(dirfstat(c->u.uif.fd, &d) < 0) {
		oserrstr(ebuf);
		close(c->u.uif.fd);
		error(ebuf);
	}

	c->qid = d.qid;

	op = c->path;
	c->path = ptenter(&syspt, op, name);
	decref(&op->r);

	c->mode = openmode(mode);
	c->offset = 0;
	c->u.uif.offset = 0;
	c->flag |= COPEN;
}

void
fsclose(Chan *c)
{
	if((c->flag & COPEN) == 0)
		return;

	osenter();
	close(c->u.uif.fd);
	osleave();
}

static int
seekdir(Chan *c, ulong offset)
{
	int n, l, fd;
	char path[MAXPATH];
	static char junk[8192];

	fspath(c->path, 0, path);
	fd = open(path, OREAD);
	if(fd < 0)
		return -1;
	l = 0;
	while(offset > 0) {
		n = sizeof(junk);
		if(n > offset)
			n = offset;
		n = read(fd, junk, n);
		if(n < 0) {
			close(fd);
			return -1;
		}
		if(n == 0)
			break;
		offset -= n;
		l += n;
	}
	close(c->u.uif.fd);
	c->u.uif.fd = fd;
	c->u.uif.offset	= l;
	return l;
}

long
fsread(Chan *c, void *va, long n, ulong offset)
{
	int r;
	char ebuf[ERRLEN];

	if(c->u.uif.stream) {
		osenter();
		r = read(c->u.uif.fd, va, n);
		osleave();
		if(r < 0) {
			oserrstr(ebuf);
			error(ebuf);
		}
		c->u.uif.offset += r;
		return r;
	}

	qlock(&c->u.uif.oq);
	if(waserror()) {
		qunlock(&c->u.uif.oq);
		nexterror();
	}
	if(c->u.uif.offset != offset) {
		if(c->qid.path & CHDIR)
			r = seekdir(c, offset);
		else
			r = seek(c->u.uif.fd, offset, 0);
		if(r < 0) {
			oserrstr(ebuf);
			error(ebuf);
		}
		c->u.uif.offset = offset;
	}
	osenter();
	r = read(c->u.uif.fd, va, n);
	osleave();
	if(r < 0) {
		oserrstr(ebuf);
		error(ebuf);
	}
	c->u.uif.offset += r;

	qunlock(&c->u.uif.oq);
	poperror();

	return r;
}

long
fswrite(Chan *c, void *va, long n, ulong offset)
{
	int fd, r;
	char ebuf[ERRLEN];

	if(c->u.uif.stream) {
		osenter();
		r = write(c->u.uif.fd, va, n);
		osleave();
		if(r < 0) {
			oserrstr(ebuf);
			error(ebuf);
		}
		c->u.uif.offset += r;
		return r;
	}

	qlock(&c->u.uif.oq);
	if(waserror()) {
		qunlock(&c->u.uif.oq);
		nexterror();
	}
	fd = c->u.uif.fd;
	if(c->u.uif.offset != offset) {
		r = seek(fd, offset, 0);
		if(r < 0) {
			oserrstr(ebuf);
			error(ebuf);
		}
		c->u.uif.offset = offset;
	}
	osenter();
	r = write(fd, va, n);
	osleave();
	if(r < 0) {
		oserrstr(ebuf);
		error(ebuf);
	}
	c->u.uif.offset += r;

	qunlock(&c->u.uif.oq);
	poperror();
	return r;
}

void
fsremove(Chan *c)
{
	char path[MAXPATH], ebuf[MAXPATH];

	fspath(c->path, 0, path);
	if(remove(path) < 0) {
		oserrstr(ebuf);
		error(ebuf);
	}
}

void
fswstat(Chan *c, char *buf)
{
	char path[MAXPATH], ebuf[MAXPATH];

	fspath(c->path, 0, path);
	if(wstat(path, buf) < 0) {
		oserrstr(ebuf);
		error(ebuf);
	}
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
 * Stream means you dont have to lock the fd because we
 * don't need to seek on a network connection. This is
 * not needed in emu's that use devip.c
 */
static int
isstream(char *path)
{
	int n;

	n = strlen(path);
	if(n < rootdirlen+14)
		return 0;
	n -= rootdirlen;
	path += rootdirlen;
	if(strncmp(path, "net/udp/", 8) != 0 &&
	   strncmp(path, "net/tcp/", 8) != 0)
			return 0;
	if(strcmp(path+n-5, "/data") != 0)
		return 0;
	return 1;
}

void
setid(char *name)
{
	strncpy(up->env->user, name, NAMELEN-1);
	up->env->user[NAMELEN-1] = 0;
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
