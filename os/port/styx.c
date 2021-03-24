#include	"u.h"
#include	"lib.h"
#include	<styx.h>

#define	PCHAR(x)	*p++ = f->x
#define	PSHORT(x)	p[0] = f->x;\
			p[1] = f->x>>8;\
			p += 2
#define	PLONG(x)	p[0] = f->x;\
			p[1] = f->x>>8;\
			p[2] = f->x>>16;\
			p[3] = f->x>>24;\
			p += 4
#define	PVLONG(x)	p[0] = f->x;\
			p[1] = f->x>>8;\
			p[2] = f->x>>16;\
			p[3] = f->x>>24;\
			p[4] = 0;\
			p[5] = 0;\
			p[6] = 0;\
			p[7] = 0;\
			p += 8
#define	PSTRING(x,n)	memmove(p, f->x, n); p += n

#define	GCHAR(x)	f->x =	*p++
#define	GSHORT(x)	f->x =	(p[0]|(p[1]<<8));\
			p += 2
#define	GLONG(x)	f->x =	(p[0]|\
			      	(p[1]<<8)|\
			      	(p[2]<<16)|\
			      	(p[3]<<24));\
			      	p += 4
#define	GVLONG(x)	f->x =	(p[0]|\
				(p[1]<<8)|\
				(p[2]<<16)|\
				(p[3]<<24));\
			p += 8
#define	GSTRING(x,n)	memmove(f->x, p, n); p += n

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
convM2D(char *ap, Dir *f)
{
	uchar *p;

	p = (uchar*)ap;
	GSTRING(name, sizeof(f->name));
	GSTRING(uid, sizeof(f->uid));
	GSTRING(gid, sizeof(f->gid));
	GLONG(qid.path);
	GLONG(qid.vers);
	GLONG(mode);
	GLONG(atime);
	GLONG(mtime);
	GVLONG(length);
	GSHORT(type);
	GSHORT(dev);
	return p - (uchar*)ap;
}

int
convM2S(char *ap, Fcall *f, int n)
{
	uchar *p;

	p = (uchar*)ap;
	if(n < 3)
		return 0;

	GCHAR(type);
	GSHORT(tag);

	if(n < msglen[f->type])
		return 0;

	switch(f->type) {
	default:
		return -1;

	case Tnop:
		break;
	case Tflush:
		GSHORT(oldtag);
		break;
	case Tattach:
		GSHORT(fid);
		GSTRING(uname, sizeof(f->uname));
		GSTRING(aname, sizeof(f->aname));
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
		if(n < msglen[f->type]+f->count)
			return 0;
		p++;	/* pad(1) */
		f->data = (char*)p; p += f->count;
		break;
	case Tclunk:
		GSHORT(fid);
		break;
	case Tremove:
		GSHORT(fid);
		break;
	case Tstat:
		GSHORT(fid);
		break;
	case Twstat:
		GSHORT(fid);
		GSTRING(stat, sizeof(f->stat));
		break;

	case Rnop:
		break;
	case Rerror:
		GSTRING(ename, sizeof(f->ename));
		break;
	case Rflush:
		break;
	case Rattach:
		GSHORT(fid);
		GLONG(qid.path);
		GLONG(qid.vers);
		break;
	case Rclone:
		GSHORT(fid);
		break;
	case Rwalk:
		GSHORT(fid);
		GLONG(qid.path);
		GLONG(qid.vers);
		break;
	case Ropen:
		GSHORT(fid);
		GLONG(qid.path);
		GLONG(qid.vers);
		break;
	case Rcreate:
		GSHORT(fid);
		GLONG(qid.path);
		GLONG(qid.vers);
		break;
	case Rread:
		GSHORT(fid);
		GSHORT(count);
		if(n < msglen[f->type]+f->count)
			return 0;
		p++;			/* pad(1) */
		f->data = (char*)p;
		p += f->count;
		break;
	case Rwrite:
		GSHORT(fid);
		GSHORT(count);
		break;
	case Rclunk:
		GSHORT(fid);
		break;
	case Rremove:
		GSHORT(fid);
		break;
	case Rstat:
		GSHORT(fid);
		GSTRING(stat, sizeof(f->stat));
		break;
	case Rwstat:
		GSHORT(fid);
		break;
	}
	return (char*)p - ap;
}

int
convS2M(Fcall *f, char *ap)
{
	uchar *p;

	p = (uchar*)ap;
	PCHAR(type);
	PSHORT(tag);
	switch(f->type) {
	default:
		return -1;

	case Tnop:
		break;
	case Tflush:
		PSHORT(oldtag);
		break;
	case Tattach:
		PSHORT(fid);
		PSTRING(uname, sizeof(f->uname));
		PSTRING(aname, sizeof(f->aname));
		break;
	case Tclone:
		PSHORT(fid);
		PSHORT(newfid);
		break;
	case Twalk:
		PSHORT(fid);
		PSTRING(name, sizeof(f->name));
		break;
	case Topen:
		PSHORT(fid);
		PCHAR(mode);
		break;
	case Tcreate:
		PSHORT(fid);
		PSTRING(name, sizeof(f->name));
		PLONG(perm);
		PCHAR(mode);
		break;
	case Tread:
		PSHORT(fid);
		PVLONG(offset);
		PSHORT(count);
		break;
	case Twrite:
		PSHORT(fid);
		PVLONG(offset);
		PSHORT(count);
		p++;	/* pad(1) */
		PSTRING(data, f->count);
		break;
	case Tclunk:
		PSHORT(fid);
		break;
	case Tremove:
		PSHORT(fid);
		break;
	case Tstat:
		PSHORT(fid);
		break;
	case Twstat:
		PSHORT(fid);
		PSTRING(stat, sizeof(f->stat));
		break;

	case Rnop:
		break;
	case Rerror:
		PSTRING(ename, sizeof(f->ename));
		break;
	case Rflush:
		break;
	case Rattach:
		PSHORT(fid);
		PLONG(qid.path);
		PLONG(qid.vers);
		break;
	case Rclone:
		PSHORT(fid);
		break;
	case Rwalk:
		PSHORT(fid);
		PLONG(qid.path);
		PLONG(qid.vers);
		break;
	case Ropen:
		PSHORT(fid);
		PLONG(qid.path);
		PLONG(qid.vers);
		break;
	case Rcreate:
		PSHORT(fid);
		PLONG(qid.path);
		PLONG(qid.vers);
		break;
	case Rread:
		PSHORT(fid);
		PSHORT(count);
		p++;	/* pad(1) */
		PSTRING(data, f->count);
		break;
	case Rwrite:
		PSHORT(fid);
		PSHORT(count);
		break;
	case Rclunk:
		PSHORT(fid);
		break;
	case Rremove:
		PSHORT(fid);
		break;
	case Rstat:
		PSHORT(fid);
		PSTRING(stat, sizeof(f->stat));
		break;
	case Rwstat:
		PSHORT(fid);
		break;
	}
	return p - (uchar*)ap;
}

static void dumpsome(char*, char*, long);
static void fdirconv(char*, Dir*);

int
fcallconv(va_list *arg, Fconv *f1)
{
	Dir d;
	Fcall *f;
	char buf[512];
	int fid, type, tag, n;

	f = va_arg(*arg, Fcall*);
	type = f->type;
	fid = f->fid;
	tag = f->tag;
	switch(type){
	case Tnop:
		sprint(buf, "Tnop tag %ud", tag);
		break;
	case Rnop:
		sprint(buf, "Rnop tag %ud", tag);
		break;
	case Rerror:
		sprint(buf, "Rerror tag %ud error %.64s", tag, f->ename);
		break;
	case Tflush:
		sprint(buf, "Tflush tag %ud oldtag %d", tag, f->oldtag);
		break;
	case Rflush:
		sprint(buf, "Rflush tag %ud", tag);
		break;
	case Tattach:
		sprint(buf, "Tattach tag %ud fid %d uname %.28s",
			tag, f->fid, f->uname);
		break;
	case Rattach:
		sprint(buf, "Rattach tag %ud fid %d qid 0x%lux|0x%lux",
			tag, fid, f->qid.path, f->qid.vers);
		break;
	case Tclone:
		sprint(buf, "Tclone tag %ud fid %d newfid %d", tag, fid, f->newfid);
		break;
	case Rclone:
		sprint(buf, "Rclone tag %ud fid %d", tag, fid);
		break;
	case Twalk:
		sprint(buf, "Twalk tag %ud fid %d name %.28s", tag, fid, f->name);
		break;
	case Rwalk:
		sprint(buf, "Rwalk tag %ud fid %d qid 0x%lux|0x%lux",
			tag, fid, f->qid.path, f->qid.vers);
		break;
	case Topen:
		sprint(buf, "Topen tag %ud fid %d mode %d", tag, fid, f->mode);
		break;
	case Ropen:
		sprint(buf, "Ropen tag %ud fid %d qid 0x%lux|0x%lux",
			tag, fid, f->qid.path, f->qid.vers);
		break;
	case Tcreate:
		sprint(buf, "Tcreate tag %ud fid %d name %.28s perm 0x%lux mode %d",
			tag, fid, f->name, f->perm, f->mode);
		break;
	case Rcreate:
		sprint(buf, "Rcreate tag %ud fid %d qid 0x%lux|0x%lux",
			tag, fid, f->qid.path, f->qid.vers);
		break;
	case Tread:
		sprint(buf, "Tread tag %ud fid %d offset %ld count %d",
			tag, fid, f->offset, f->count);
		break;
	case Rread:
		n = sprint(buf, "Rread tag %ud fid %d count %d ", tag, fid, f->count);
			dumpsome(buf+n, f->data, f->count);
		break;
	case Twrite:
		n = sprint(buf, "Twrite tag %ud fid %d offset %ld count %d ",
			tag, fid, f->offset, f->count);
		dumpsome(buf+n, f->data, f->count);
		break;
	case Rwrite:
		sprint(buf, "Rwrite tag %ud fid %d count %d", tag, fid, f->count);
		break;
	case Tclunk:
		sprint(buf, "Tclunk tag %ud fid %d", tag, fid);
		break;
	case Rclunk:
		sprint(buf, "Rclunk tag %ud fid %d", tag, fid);
		break;
	case Tremove:
		sprint(buf, "Tremove tag %ud fid %d", tag, fid);
		break;
	case Rremove:
		sprint(buf, "Rremove tag %ud fid %d", tag, fid);
		break;
	case Tstat:
		sprint(buf, "Tstat tag %ud fid %d", tag, fid);
		break;
	case Rstat:
		n = sprint(buf, "Rstat tag %ud fid %d", tag, fid);
		convM2D(f->stat, &d);
		sprint(buf+n, " stat ");
		fdirconv(buf+n+6, &d);
		break;
	case Twstat:
		convM2D(f->stat, &d);
		n = sprint(buf, "Twstat tag %ud fid %d stat ", tag, fid);
		fdirconv(buf+n, &d);
		break;
	case Rwstat:
		sprint(buf, "Rwstat tag %ud fid %d", tag, fid);
		break;
	default:
		sprint(buf,  "unknown type %d", type);
	}
	strconv(buf, f1);
	return 0;
}

int
dirconv(va_list *arg, Fconv *f)
{
	char buf[160];

	fdirconv(buf, va_arg(*arg, Dir*));
	strconv(buf, f);
	return 0;
}

static void
fdirconv(char *buf, Dir *d)
{
	sprint(buf, "'%s' '%s' '%s' q %#lux|%#lux m %#luo at %ld mt %ld l %ld t %d d %d\n",
			d->name, d->uid, d->gid,
			d->qid.path, d->qid.vers, d->mode,
			d->atime, d->mtime, d->length,
			d->type, d->dev);
}
#define DUMPL 24

static void
dumpsome(char *ans, char *buf, long count)
{
	int i, printable;
	char *p;
	uchar c;

	printable = 1;
	if(count > DUMPL)
		count = DUMPL;
	for(i=0; i<count && printable; i++) {
		c = buf[i];
		if((c < 32 && c !='\n' && c !='\t') || c > 127)
			printable = 0;
	}

	p = ans;
	*p++ = '\'';
	if(printable){
		memmove(p, buf, count);
		p += count;
	}else{
		for(i=0; i<count; i++){
			if(i>0 && i%4==0)
				*p++ = ' ';
			sprint(p, "%2.2ux", buf[i]);
			p += 2;
		}
	}
	*p++ = '\'';
	*p = 0;
}
