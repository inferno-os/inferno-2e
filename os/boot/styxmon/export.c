#include <lib9.h>
#include <styx.h>
#include "mem.h"
#include "mdstyxmon.h"

// char	Eunimplemented[] =	"unimplemented";
char	Enofid[] =		"no more fid storage";
char	Einuse[] =		"fid in use";
char	Eunknownfid[] =		"unknown fid";
char	Eperm[] =		"permission denied";
char	Etoobig[] =		"count too large";
char	Eopen[] =		"open failed";
char	Eread[] =		"read failed";
char	Ewrite[] =		"write failed";
char	Eisadirectory[] =	"is a directory";
char	Enotadirectory[] =	"not a directory";
char	Enosuchfile[] =		"file does not exist";
char	Enothandled[] =		"message not handled";


#define	PCHAR(x)	*p++ = x
#define	PSHORT(x)	{ ulong v = x; p[0] = v;\
			p[1] = v>>8;\
			p += 2; }
#define	PLONG(x)	plong(p, x); p += 4


static void block(Fcall *fc);


static Fid*
getfid(int fid, int create)
{
	Global *g = G;
	Fid *f = g->fidpool;
	Fid *fe = f+nelem(g->fidpool);
	Fid *e = nil;

	for(; f < fe; f++) {
		if(f->node == nil)
			e = f;
		else if(f->fid == fid)
			return f;
	}
	if(e == nil || !create)
		return nil;

	if(g->nfids++ == 0)
		firstattach(fid);

	e->fid = fid;

	return e;
}

void
Exattach(uchar *buf, Fcall *fc)
{
	Global *g = G;
	Fid *f;
	uchar *p;
	BPChan *node;

	f = getfid(fc->fid, 1);
	if(f == nil) {
		rerror(buf, fc, Enofid, sizeof(Enofid));
		return;
	}
	node = &g->rootdir;
	f->node = node;

	buf += sizeof(G->buf)-(1+2+2+8);
	p = buf;
	PCHAR(Rattach);
	PSHORT(fc->tag);
	PSHORT(fc->fid);
	PLONG(node->d.qid.path);
	PLONG(node->d.qid.vers);
	USED(p);
	send(buf, 1+2+2+8);
}

void
Exclone(uchar *buf, Fcall *fc)
{
	uchar *p;
	Fid *f, *n;

	if(fc->fid == fc->newfid) {
inuse:
		rerror(buf, fc, Einuse, sizeof(Einuse));
		return;
	}

	f = getfid(fc->fid, 0);
	if(f == nil || f->node == nil) {
		rerror(buf, fc, Eunknownfid, sizeof(Eunknownfid));
		return;
	}
	n = getfid(fc->newfid, 1);
	if(n == nil) {
		rerror(buf, fc, Enofid, sizeof(Enofid));
		return;
	}
	if(n->node)
		goto inuse;
	n->node = f->node;

	buf += sizeof(G->buf)-(1+2+2);
	p = buf;
	PCHAR(Rclone);
	PSHORT(fc->tag);
	PSHORT(fc->fid);
	USED(p);
	send(buf, 1+2+2);
}

void
Exclunk(uchar *buf, Fcall *fc)
{
	Fid *f;
	Global *g = G;
	uchar *p;
	int tag = fc->tag;
	int fid = fc->fid;

	f = getfid(fid, 0);
	if(f == nil) {
		rerror(buf, fc, Eunknownfid, sizeof(Eunknownfid));
		return;
	}
	if(f->open && f->node->clunk) 
		f->node->clunk(f->node);
	f->open = 0;
	f->node = nil;
	if(--(g->nfids) <= 0)
		lastclunk();
	buf += sizeof(G->buf)-(1+2+2);
	p = buf;
	PCHAR(Rclunk);
	PSHORT(tag);
	PSHORT(fid);
	USED(p);
	send(buf, 1+2+2);
}

void
Exflush(uchar *buf, Fcall *fc)
{
	Global *g = G;
	Blocked *b = g->blocked;
	Blocked *be = b+nelem(g->blocked);
	uchar *p;

	for(; b < be; b++) 
		if(b->type && b->tag == fc->oldtag)
			b->type = 0;

	buf += sizeof(G->buf)-(1+2);
	p = buf;
	PCHAR(Rflush);
	PSHORT(fc->tag);
	USED(p);
	send(buf, 1+2);
}

void
Exopen(uchar *buf, Fcall *fc)
{
	uchar *p;
	Fid *f;
	BPChan *node;
	int tag = fc->tag;
	int fid = fc->fid;

	f = getfid(fid, 0);
	if(f == nil) {
		rerror(buf, fc, Eunknownfid, sizeof(Eunknownfid));
		return;
	}
	node = f->node;

	if((node->d.qid.path & CHDIR) && fc->mode != OREAD) {
		rerror(buf, fc, Eperm, sizeof(Eperm));
		return;
	}

	node->err = nil;
	if(node->open && (*node->open)(node, fc->mode) == -1) {
		if(node->err != nil)
			rerror(buf, fc, node->err, strlen(node->err));
		else
			rerror(buf, fc, Eopen, sizeof(Eopen));
		return;
	}
	++f->open;

	buf += sizeof(G->buf)-(1+2);
	p = buf;
	PCHAR(Ropen);
	PSHORT(tag);
	PSHORT(fid);
	PLONG(node->d.qid.path);
	PLONG(node->d.qid.vers);
	USED(p);
	send(buf, 1+2+2+4+4);
}

void
Exread(uchar *buf, Fcall *fc)
{
	uchar *nbuf;
	uchar *p;
	Fid *f;
	BPChan *node;
	long n;
	int fid = fc->fid;
	int tag = fc->tag;

	f = getfid(fid, 0);
	if(f == nil) {
		rerror(buf, fc, Eunknownfid, sizeof(Eunknownfid));
		return;
	}
	n = fc->count;
	if(n > 8192) {
		rerror(buf, fc, Etoobig, sizeof(Etoobig));
		return;
	}
	node = f->node;
	if(node->read == nil) {
		rerror(buf, fc, Eperm, sizeof(Eperm));
		return;
	}

	nbuf = buf+sizeof(G->buf)-(1+2+2+2+1+n);
	node->err = nil;
	n = node->read(f->node, nbuf+1+2+2+2+1, n, fc->offset);
	if(n == BPCHAN_BLOCKED) {
		block(fc);
		return;
	}
	if(n == -1) {
		if(node->err != nil)
			rerror(buf, fc, node->err, strlen(node->err));
		else
			rerror(buf, fc, Eread, sizeof(Eread));
		return;
	}

	p = nbuf;
	PCHAR(Rread);
	PSHORT(tag);
	PSHORT(fid);
	PSHORT(n);
	PCHAR(0);
	USED(p);
	send(nbuf, 1+2+2+2+1+n);
}


void
Exwrite(uchar *buf, Fcall *fc)
{
	uchar *p;
	BPChan *node;
	long n;
	Fid *f;
	int fid = fc->fid;
	int tag = fc->tag;

	f = getfid(fid, 0);
	if(f == nil) {
		rerror(buf, fc, Eunknownfid, sizeof(Eunknownfid));
		return;
	}
	node = f->node;
	if(node->d.qid.path & CHDIR) {
		rerror(buf, fc, Eisadirectory, sizeof(Eisadirectory));
		return;
	}
	if(node->write == nil) {
		rerror(buf, fc, Eperm, sizeof(Eperm));
		return;
	}
	node->err = nil;
	n = node->write(node, buf+1+2+2+8+2+1, fc->count, fc->offset);
	if(n == BPCHAN_BLOCKED) {
		block(fc);
		return;
	}
	if(n == -1) {
		if(node->err != nil)
			rerror(buf, fc, node->err, strlen(node->err));
		else
			rerror(buf, fc, Ewrite, sizeof(Ewrite));
		return;
	}

	buf += sizeof(G->buf)-(1+2+2+2);
	p = buf;
	PCHAR(Rwrite);
	PSHORT(tag);
	PSHORT(fid);
	PSHORT(n);
	USED(p);
	send(buf, 1+2+2+2);
}

void
Exstat(uchar *buf, Fcall *fc)
{
	uchar *p;
	BPChan *node;
	Fid *f;

	f = getfid(fc->fid, 0);
	if(f == nil) {
		rerror(buf, fc, Eunknownfid, sizeof(Eunknownfid));
		return;
	}
	node = f->node;
	buf += sizeof(G->buf)-(1+2+2+DIRLEN);
	p = buf;
	PCHAR(Rstat);
	PSHORT(fc->tag);
	PSHORT(fc->fid);
	convD2M(&node->d, (char*)p);
	send(buf, 1+2+2+DIRLEN);
}

void
Exwalk(uchar *buf, Fcall *fc)
{
	Global *g = G;
	BPChan *c;
	Fid *f;
	uchar *p;

	f = getfid(fc->fid, 0);
	if(f == nil) {
		rerror(buf, fc, Eunknownfid, sizeof(Eunknownfid));
		return;
	}
	if(f->node->d.qid.path & CHDIR == 0) {
		rerror(buf, fc, Enotadirectory, sizeof(Enotadirectory));
		return;
	}
	if(fc->name[0] == '.' && fc->name[1] == 0) {
		c = f->node;
		goto sendit;
	}
	for(c=g->head; c != nil; c = c->link) {
		if(strcmp(fc->name, c->d.name) == 0) {
sendit:
			f->node = c;
			buf += sizeof(G->buf)-(1+2+2+4+4);
			p = buf;
			PCHAR(Rwalk);
			PSHORT(fc->tag);
			PSHORT(fc->fid);
			PLONG(c->d.qid.path);
			PLONG(c->d.qid.vers);
			USED(p);
			send(buf, 1+2+2+4+4);
			return;
		}
	}
	rerror(buf, fc, Enosuchfile, sizeof(Enosuchfile));
}

void
Exnop(uchar *buf, Fcall *fc)
{
	uchar *p;
	buf += sizeof(G->buf)-(1+2);
	p = buf;
	PCHAR(Rnop);
	PSHORT(fc->tag);
	USED(p);
	send(buf, 1+2);
}


void
rerror(uchar *buf, Fcall *fc, char *err, int elen)
{
	uchar *p;
	buf += sizeof(G->buf)-(3+64);
	p = buf;

	*p++ = Rerror;
	PSHORT(fc->tag);

	if(elen > 64)
		elen = 64;
	memmove(p, err, elen);
	while(elen < 64)
		p[elen++] = 0;

	send(buf, 3+64);
}


int
rootdirread(BPChan*, uchar *buf, long n, long offset)
{
	Global *g = G;
	int skip;
	BPChan *chan;
	int i;

	chan = g->head;

	if(chan->link == nil)
		return 0;
	chan = chan->link;		// skip the chan for the dir

	for(skip=0; skip < offset && chan != nil; skip += DIRLEN, chan = chan->link) ;
	if(chan == nil)
		return 0;
	for(i=0; i  < n && chan != nil; i += DIRLEN, chan = chan->link)
		buf += convD2M(&chan->d, (char*)buf);

	return i;
}

static void (*fcalls[Tmax])(uchar*, Fcall*) =
{
	[Tnop] = Exnop,
	[Tattach] = Exattach,
	[Tclone] = Exclone,
	[Twalk] = Exwalk,
	[Topen] = Exopen,
	[Tread] = Exread,
	[Twrite] = Exwrite,
	[Tclunk] = Exclunk,
	[Tstat] = Exstat,
	[Tflush] = Exflush,
};

int
fcall(uchar *buf, Fcall *fc)
{
	if(fc->type >= nelem(fcalls))
		return 0;
	if(fcalls[fc->type] != nil) {
		fcalls[fc->type](buf, fc);
		return 1;
	} else {
		rerror(buf, fc, Enothandled, sizeof Enothandled);
		return 0;
	}
}

static void
block(Fcall *fc)
{
	Global *g = G;
	Blocked *b = g->blocked;
	Blocked *be = b+nelem(g->blocked);
	while(b->type) 
		if(++b == be)
			return;
	b->type = fc->type;
	b->fid = fc->fid;
	b->tag = fc->tag;
	b->qid = fc->qid;
	b->offset = fc->offset;
	b->count = fc->count;
}

int
unblock(uchar *buf)
{
	Global *g = G;
	Blocked *b = g->blocked;
	Blocked *be = b+nelem(g->blocked);
	Fcall *fc = &g->fc;
	int r = 0;
	for(; b < be; b++) {
		if(b->type) {
			fc->type = b->type;
			fc->fid = b->fid;
			fc->tag = b->tag;
			fc->qid = b->qid;
			fc->offset = b->offset;
			fc->count = b->count;
			b->type = 0;
			fcall(buf, fc);
			++r;
		}
	}
	return r;
}

