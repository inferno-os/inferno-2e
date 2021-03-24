#include	"all.h"

#define	CHAR(x)		*p++ = f->x
#define	SHORT(x)	p[0] = f->x; p[1] = f->x>>8; p += 2
#define	SLONG(q)	p[0] = (q); p[1] = (q)>>8; p[2] = (q)>>16; p[3] = (q)>>24; p += 4
#define	LONG(x)		SLONG(f->x)
#define	VLONG(x)	SLONG(f->x); p += 4
#define	STRING(x,n)	memmove(p, f->x, n); p += n

/*
 * buggery to give false qid for
 * the top 2 levels of the dump fs
 */
ulong
fakeqid(Dentry *f)
{
	ulong q;
	int c;

	q = f->qid.path;
	if(q == (QPROOT|QPDIR)) {
		c = f->name[0];
		if(c >= '0' && c <= '9') {
			q = 3|QPDIR;
			c = (c-'0')*10 + (f->name[1]-'0');
			if(c >= 1 && c <= 12)
				q = 4|QPDIR;
		}
	}
	return q;
}

int
fsconvD2M(Dentry *f, char *ap)
{
	uchar *p;
	ulong q;

	p = (uchar*)ap;
	STRING(name, sizeof(f->name));

	uidtostr((char*)p, f->uid);
	p += NAMELEN;

	uidtostr((char*)p, f->gid);
	p += NAMELEN;

	LONG(qid.path);
	LONG(qid.vers);
	{
		q = f->mode & 0x0fff;
		if(f->mode & DDIR)
			q |= PDIR;
		if(f->mode & DAPND)
			q |= PAPND;
		if(f->mode & DLOCK)
			q |= PLOCK;
		SLONG(q);
	}
	LONG(atime);
	LONG(mtime);
	LONG(size);
	SLONG(0);
	SLONG(0);
	return p - (uchar*)ap;
}

#undef	CHAR
#undef	SHORT
#undef	SLONG
#undef	LONG
#undef	VLONG
#undef	STRING

#define	CHAR(x)		f->x = *p++
#define	SHORT(x)	f->x = (p[0] | (p[1]<<8)); p += 2
#define	SLONG(q)	q = (p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24)); p += 4
#define	LONG(x)		SLONG(f->x)
#define	VLONG(x)	SLONG(f->x); p += 4
#define	STRING(x,n)	memmove(f->x, p, n); p += n

int
fsconvM2D(char *ap, Dentry *f)
{
	uchar *p;
	char str[28];

	p = (uchar*)ap;
	STRING(name, sizeof(f->name));

	memmove(str, p, NAMELEN);
	p += NAMELEN;
	f->uid = strtouid(str);

	memmove(str, p, NAMELEN);
	p += NAMELEN;
	f->gid = strtouid(str);

	LONG(qid.path);
	LONG(qid.vers);
	{
		LONG(atime);
		f->mode = (f->atime & 0x0fff) | DALLOC;
		if(f->atime & PDIR)
			f->mode |= DDIR;
		if(f->atime & PAPND)
			f->mode |= DAPND;
		if(f->atime & PLOCK)
			f->mode |= DLOCK;
	}
	LONG(atime);
	LONG(mtime);
	LONG(size); p += 4;
	p += 4;
	return p - (uchar*)ap;
}
