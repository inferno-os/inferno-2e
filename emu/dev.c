#include	"dat.h"
#include	"fns.h"
#include	"error.h"

extern ulong	kerndate;

int
devno(int c, int user)
{
	int i;

	for(i = 0; devtab[i] != nil; i++) {
		if(devtab[i]->dc == c)
			return i;
	}
	if(user == 0)
		panic("devno %C 0x%ux", c, c);

	return -1;
}

void
devdir(Chan *c, Qid qid, char *n, long length, char *user, long perm, Dir *db)
{
	strcpy(db->name, n);
	db->qid = qid;
	db->type = devtab[c->type]->dc;
	db->dev = c->dev;
	db->mode = perm;
	if(qid.path & CHDIR)
		db->mode |= CHDIR;
	if(c->flag&CMSG)
		db->mode |= CHMOUNT;
	db->atime = time(0);
	db->mtime = kerndate;
	db->length = length;
	memmove(db->uid, user, NAMELEN);
	memmove(db->gid, eve, NAMELEN);
}

int
devgen(Chan *c, Dirtab *tab, int ntab, int i, Dir *dp)
{
	if(tab==0 || i>=ntab)
		return -1;
	tab += i;
	devdir(c, tab->qid, tab->name, tab->length, eve, tab->perm, dp);
	return 1;
}

Chan*
devattach(int tc, char *spec)
{
	Chan *c;
	char buf[NAMELEN+4];

	c = newchan();
	c->qid.path = CHDIR;
	c->qid.vers = 0;

	c->type = devno(tc, 0);
	if(tc != 'M') {
		sprint(buf, "#%C%s", tc, spec);
		c->path = ptenter(&syspt, 0, buf);
	}
	return c;
}

Chan *
devclone(Chan *c, Chan *nc)
{
	if(c->flag & COPEN)
		panic("clone of open file type %C\n", devtab[c->type]->dc);

	if(nc == 0)
		nc = newchan();

	nc->type = c->type;
	nc->dev = c->dev;
	nc->mode = c->mode;
	nc->qid = c->qid;
	nc->offset = c->offset;
	nc->flag = c->flag;
	nc->mnt = c->mnt;
	nc->mountid = c->mountid;
	nc->u = c->u;
	nc->mchan = c->mchan;
	nc->mqid = c->mqid;
	nc->path = c->path;
	incref(&nc->path->r);
	return nc;
}

int
devwalk(Chan *c, char *name, Dirtab *tab, int ntab, Devgen *gen)
{
	long i;
	Dir dir;
	Path *op;

	isdir(c);
	if(name[0]=='.' && name[1]==0)
		return 1;

	for(i=0;; i++) {
		switch((*gen)(c, tab, ntab, i, &dir)){
		case -1:
			strncpy(up->env->error, Enonexist, NAMELEN);
			return 0;
		case 0:
			continue;
		case 1:
			if(strcmp(name, dir.name) == 0) {
				c->qid = dir.qid;
				op = c->path;
				c->path = ptenter(&syspt, op, name);
				decref(&op->r);
				return 1;
			}
			continue;
		}
	}
	return 0;
}

void
devstat(Chan *c, char *db, Dirtab *tab, int ntab, Devgen *gen)
{
	int i;
	Dir dir;

	for(i=0;; i++)
		switch((*gen)(c, tab, ntab, i, &dir)){
		case -1:
			if(c->qid.path & CHDIR){
				devdir(c, c->qid, c->path->elem, i*DIRLEN, eve, CHDIR|0555, &dir);
				convD2M(&dir, db);
				return;
			}
			print("%s: devstat %C %lux\n",
				up->env->user, devtab[c->type]->dc, c->qid.path);
			error(Enonexist);
		case 0:
			break;
		case 1:
			if(eqqid(c->qid, dir.qid)) {
				if(c->flag&CMSG)
					dir.mode |= CHMOUNT;
				convD2M(&dir, db);
				return;
			}
			break;
		}
}

long
devdirread(Chan *c, char *d, long n, Dirtab *tab, int ntab, Devgen *gen)
{
	long k, m;
	Dir dir;

	k = c->offset/DIRLEN;
	for(m=0; m<n; k++) {
		switch((*gen)(c, tab, ntab, k, &dir)){
		case -1:
			return m;

		case 0:
			c->offset += DIRLEN;
			break;

		case 1:
			convD2M(&dir, d);
			m += DIRLEN;
			d += DIRLEN;
			break;
		}
	}

	return m;
}

Chan *
devopen(Chan *c, int omode, Dirtab *tab, int ntab, Devgen *gen)
{
	int i;
	Dir dir;
	ulong t, mode;
	static int access[] = { 0400, 0200, 0600, 0100 };

	for(i=0;; i++) {
		switch((*gen)(c, tab, ntab, i, &dir)){
		case -1:
			goto Return;
		case 0:
			break;
		case 1:
			if(eqqid(c->qid, dir.qid)) {
				if(strcmp(up->env->user, dir.uid) == 0)
					mode = dir.mode;
				else
				if(strcmp(up->env->user, eve) == 0)
					mode = dir.mode<<3;
				else
					mode = dir.mode<<6;

				t = access[omode&3];
				if((t & mode) == t)
					goto Return;
				error(Eperm);
			}
			break;
		}
	}
Return:
	c->offset = 0;
	if((c->qid.path&CHDIR) && omode!=OREAD)
		error(Eperm);
	c->mode = openmode(omode);
	c->flag |= COPEN;
	return c;
}

Block*
devbread(Chan *c, long n, ulong offset)
{
	Block *bp;

	bp = allocb(n);
	if(waserror()) {
		freeb(bp);
		nexterror();
	}
	bp->wp += devtab[c->type]->read(c, bp->wp, n, offset);
	poperror();
	return bp;
}

long
devbwrite(Chan *c, Block *bp, ulong offset)
{
	long n;

	n = devtab[c->type]->write(c, bp->rp, BLEN(bp), offset);
	freeb(bp);

	return n;	
}

void
devcreate(Chan *c, char *name, int mode, ulong perm)
{
	USED(c);
	USED(name);
	USED(mode);
	USED(perm);
	error(Eperm);
}

void
devremove(Chan *c)
{
	USED(c);
	error(Eperm);
}

void
devwstat(Chan *c, char *buf)
{
	USED(c);
	USED(buf);
	error(Eperm);
}

int
readstr(ulong off, char *buf, ulong n, char *str)
{
	int size;

	size = strlen(str);
	if(off >= size)
		return 0;
	if(off+n > size)
		n = size-off;
	memmove(buf, str+off, n);
	return n;
}

int
readnum(ulong off, char *buf, ulong n, ulong val, int size)
{
	char tmp[64];

	if(size > 64) size = 64;

	snprint(tmp, sizeof(tmp), "%*.0ud ", size, val);
	if(off >= size)
		return 0;
	if(off+n > size)
		n = size-off;
	memmove(buf, tmp+off, n);
	return n;
}

int
parsefields(char *lp, char **fields, int n, char *sep)
{
	int i;

	for(i=0; lp && *lp && i<n; i++){
		while(*lp && strchr(sep, *lp) != 0)
			*lp++=0;
		if(*lp == 0)
			break;
		fields[i]=lp;
		while(*lp && strchr(sep, *lp) == 0)
			lp++;
	}
	return i;
}
