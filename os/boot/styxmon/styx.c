#include <lib9.h>
#include <styx.h>

static char msglen[Tmax] =
{
	/*Tnop*/	3,
	/*Rnop*/	3,
	/*Terror*/	0,
	/*Rerror*/	67,
	/*Tflush*/	5,
	/*Rflush*/	3,
	/*Tclone*/	7,
	/*Rclone*/	5,
	/*Twalk*/	33,
	/*Rwalk*/	13,
	/*Topen*/	6,
	/*Ropen*/	13,
	/*Tcreate*/	38,
	/*Rcreate*/	13,
	/*Tread*/	15,
	/*Rread*/	8,		/* header only; excludes data */
	/*Twrite*/	16,		/* header only; excludes data */
	/*Rwrite*/	7,
	/*Tclunk*/	5,
	/*Rclunk*/	5,
	/*Tremove*/	5,
	/*Rremove*/	5,
	/*Tstat*/	5,
	/*Rstat*/	121,
	/*Twstat*/	121,
	/*Rwstat*/	5,
	/*Tsession?!*/  0,
	/*Rsession?!*/  0,
	/*Tattach*/	5+2*NAMELEN,
	/*Rattach*/	13,
};


#define	PCHAR(x)	*p++ = f->x
#define	PSHORT(x)	{ ulong v = f->x; p[0] = v;\
			p[1] = v>>8;\
			p += 2; }
#define	PLONG(x)	plong(p, f->x); p += 4
#define	PVLONG(x)	pvlong(p, f->x); p += 8
#define	PSTRING(x,n)	memmove(p, f->x, n); p += n

#define	GCHAR(x)	f->x = *p++
#define	GSHORT(x)	f->x = (p[0]|(p[1]<<8)); p += 2
#define GLONG(x)	f->x = glong(p); p += 4
#define GVLONG(x)	f->x = glong(p); p += 8
#define	GSTRING(x,n)	memmove(f->x, p, n); p += n

extern int recv(void*, int);


ulong
glong(uchar *p)
{
	return p[0]|(p[1]<<8)|(p[2]<<16)|(p[3]<<24);
}

void
plong(uchar *p, long v)
{
	p[0] = v;
	p[1] = v>>8;
	p[2] = v>>16;
	p[3] = v>>24;
}

void
pvlong(uchar *p, long v)
{
	plong(p, v);
	plong(p+4, 0);
}


int
convD2M(Dir *f, char *ap)
{
	uchar *p;

	p = (uchar*)ap;
	PSTRING(name, sizeof(f->name));
	PSTRING(uid, sizeof(f->uid));
	PSTRING(gid, sizeof(f->gid));
	PLONG(qid.path);
	PLONG(qid.vers);
	PLONG(mode);
	PLONG(atime);
	PLONG(mtime);
	PVLONG(length);
	PSHORT(type);
	PSHORT(dev);
	return p - (uchar*)ap;
}

int
convM2S(char *ap, Fcall *f, int len)
{
	uchar *p;
	int n;
	
	p = (uchar*)ap;

	GCHAR(type);
	n = *(uchar*)&f->type;
	if(n >= nelem(msglen))
		return -1;
	n = msglen[n];
	if(n > len)
		recv(ap+len, n-len);
	GSHORT(tag);
	switch(f->type) {
	case Tnop:
	case Rnop:
	case Rflush:
		break;
	case Tflush:
		GSHORT(oldtag);
		break;
	case Tclone:
		GSHORT(fid);
		GSHORT(newfid);
		break;
	case Twalk:
		GSHORT(fid);
		GSTRING(name, sizeof(f->name));
		break;
	case Topen:
		GSHORT(fid);
		GCHAR(mode);
		break;
	case Tcreate:
		GSHORT(fid);
		GSTRING(name, sizeof(f->name));
		GLONG(perm);
		GCHAR(mode);
		break;
	case Tread:
		GSHORT(fid);
		GVLONG(offset);
		GSHORT(count);
		break;
	case Twrite:
		GSHORT(fid);
		GVLONG(offset);
		GSHORT(count);
		recv(ap+msglen[f->type], f->count);
		p++;	/* pad(1) */
		f->data = (char*)p; p += f->count;
		break;
	case Tclunk:
	case Tremove:
	case Tstat:
	case Rclunk:
	case Rremove:
	case Rwstat:
	case Rclone:
		GSHORT(fid);
		break;
	case Twstat:
	case Rstat:
		GSHORT(fid);
		GSTRING(stat, sizeof(f->stat));
		break;
	case Tattach:
		GSHORT(fid);
		GSTRING(uname, sizeof(f->uname));
		GSTRING(aname, sizeof(f->aname));
		break;

	/* Rnop same as Tnop */
	case Rerror:
		GSTRING(ename, sizeof(f->ename));
		break;
	/* Rflush same as Tnop */
	/* Rclone same as Tclunk */
	case Rwalk:
	case Ropen:
	case Rcreate:
	case Rattach:
		GSHORT(fid);
		GLONG(qid.path);
		GLONG(qid.vers);
		break;
	case Rread:
		GSHORT(fid);
		GSHORT(count);
		recv(ap+msglen[f->type], f->count);
		p++;			/* pad(1) */
		f->data = (char*)p;
		p += f->count;
		break;
	case Rwrite:
		GSHORT(fid);
		GSHORT(count);
		break;
	/* Rclunk same as Tclunk */
	/* Rremove same as Tclunk */
	/* Rstat same as Twstat */
	/* Rwstat same as Tclunk */
	/* Rattach same as Rwalk */

	default:
		return -1;
	}
	return (char*)p - ap;
}

