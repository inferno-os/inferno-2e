#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

typedef	struct Fid	Fid;
typedef	struct Export	Export;
typedef	struct Exq	Exq;

enum
{
	Nfidhash	= 32,
	MAXRPC		= MAXMSG+MAXFDATA,
	MAXDIRREAD	= (MAXFDATA/DIRLEN)*DIRLEN
};

struct Export
{
	Ref	r;
	Exq*	work;
	QLock	fidlock;
	Fid*	fid[Nfidhash];
	Chan*	io;
	Chan*	root;
	Pgrp*	pgrp;
	int	async;
	int	excl;		/* exclusive attach */
	int	gen;		/* attach generation */
	int	npart;
	char	part[MAXRPC];
};

struct Fid
{
	Fid*	next;
	Fid**	last;
	Chan*	chan;
	ulong	offset;
	int	fid;
	int	ref;		/* fcalls using the fid; locked by Export.Lock */
	int	attached;	/* fid attached or cloned but not clunked */
	int	gen;		/* attach generation */
};

struct Exq
{
	Exq*	next;
	int	shut;		/* has been noted for shutdown */
	Export*	export;
	Proc*	slave;
	Fcall	rpc;
	char	buf[MAXRPC];
};

struct
{
	Lock	l;
	QLock	qwait;
	Rendez	rwait;
	Exq*	head;		/* work waiting for a slave */
	Exq*	tail;
}exq;

void	exshutdown(Export*);
void	exflush(Export*, int);
void	exslave(void*);
void	exfree(Export*);

char*	Exattach(Export*, Fcall*);
char*	Exclone(Export*, Fcall*);
char*	Exclunk(Export*, Fcall*);
char*	Excreate(Export*, Fcall*);
char*	Exnop(Export*, Fcall*);
char*	Exopen(Export*, Fcall*);
char*	Exread(Export*, Fcall*);
char*	Exremove(Export*, Fcall*);
char*	Exstat(Export*, Fcall*);
char*	Exwalk(Export*, Fcall*);
char*	Exwrite(Export*, Fcall*);
char*	Exwstat(Export*, Fcall*);

char	*(*fcalls[Tmax])(Export*, Fcall*);

char	Enofid[]   = "no such fid";
char	Eseekdir[] = "can't seek on a directory";
char	Ereaddir[] = "unaligned read of a directory";
int	exdebug = 0;

int
export(int fd, int async, int excl)
{
	Chan *c;
	Pgrp *pg;
	Export *fs;

	if(waserror())
		return -1;

	c = fdtochan(up->env->fgrp, fd, ORDWR, 1, 1);
	poperror();
	c->flag |= CMSG;

	fs = malloc(sizeof(Export));
	fs->r.ref = 1;

	pg = up->env->pgrp;
	fs->pgrp = pg;
	incref(pg);
	fs->root = pg->slash;
	incref(fs->root);
	fs->root = domount(fs->root);
	fs->io = c;
	fs->async = async;
	fs->excl = excl;
	fs->gen = 0;
	if(async)
		kproc("exportfs", exportproc, fs);
	else
		exportproc(fs);

	return 0;
}

static void
exportinit(void)
{
	lock(&exq.l);
	if(fcalls[Tnop] != nil) {
		unlock(&exq.l);
		return;
	}

	fcalls[Tnop] = Exnop;
	fcalls[Tattach] = Exattach;
	fcalls[Tclone] = Exclone;
	fcalls[Twalk] = Exwalk;
	fcalls[Topen] = Exopen;
	fcalls[Tcreate] = Excreate;
	fcalls[Tread] = Exread;
	fcalls[Twrite] = Exwrite;
	fcalls[Tclunk] = Exclunk;
	fcalls[Tremove] = Exremove;
	fcalls[Tstat] = Exstat;
	fcalls[Twstat] = Exwstat;
	unlock(&exq.l);
}

void
exportproc(Export *fs)
{
	Exq *q;
	int async;
	char *buf;
	int n, cn, len;

	exportinit();

	for(;;){
		q = smalloc(sizeof(Exq));
		q->rpc.data = q->buf + MAXMSG;

		buf = q->buf;
		len = MAXRPC;
		if(fs->npart) {
			memmove(buf, fs->part, fs->npart);
			buf += fs->npart;
			len -= fs->npart;
			goto chk;
		}
		for(;;) {
			if(waserror())
				goto bad;

			n = devtab[fs->io->type]->read(fs->io, buf, len, 0);
			poperror();

			if(n <= 0)
				goto bad;

			buf += n;
			len -= n;
	chk:
			n = buf - q->buf;

			/* convM2S returns size of correctly decoded message */
			cn = convM2S(q->buf, &q->rpc, n);
			if(cn < 0){
				print("bad message type (%d) in exportproc\n", q->rpc.type);
				goto bad;
			}
			if(cn > 0) {
				n -= cn;
				if(n < 0){
					print("negative size in exportproc\n");
					goto bad;
				}
				fs->npart = n;
				if(n != 0)
					memmove(fs->part, q->buf+cn, n);
				break;
			}
		}
		if(exdebug)
			print("export <- %F\n", &q->rpc);

		if(q->rpc.type == Tflush){
			exflush(fs, q->rpc.oldtag);
			free(q);
			continue;
		}

		q->export = fs;
		incref(&fs->r);

		lock(&exq.l);
		if(exq.head == nil)
			exq.head = q;
		else
			exq.tail->next = q;
		q->next = nil;
		exq.tail = q;
		unlock(&exq.l);
		if(exq.qwait.head == nil)
			kproc("exslave", exslave, nil);
		wakeup(&exq.rwait);
	}
bad:
	async = fs->async;

	free(q);
	exshutdown(fs);
	exfree(fs);

	if(async == 0)
		return;

	pexit("mount shut down", 0);
}

void
exflush(Export *fs, int tag)
{
	Exq *q, **last;
	int n;

	lock(&exq.l);
	last = &exq.head;
	for(q = exq.head; q != nil; q = q->next){
		if(q->export == fs && q->rpc.tag == tag){
			*last = q->next;
			unlock(&exq.l);

			q->rpc.type = Rerror;
			strncpy(q->rpc.ename, "interrupted", ERRLEN);
			n = convS2M(&q->rpc, q->buf);
			if(n < 0)
				panic("bad message type in exflush");
			if(!waserror()){
				devtab[fs->io->type]->write(fs->io, q->buf, n, 0);
				poperror();
			}
			exfree(fs);
			free(q);
			return;
		}
		last = &q->next;
	}
	unlock(&exq.l);

	lock(&fs->r);
	for(q = fs->work; q != nil; q = q->next){
		if(q->rpc.tag == tag){
			unlock(&fs->r);
			swiproc(q->slave);
			return;
		}
	}
	unlock(&fs->r);
}

void
exshutdown(Export *fs)
{
	Exq *q, **last;

	lock(&exq.l);
	last = &exq.head;
	for(q = exq.head; q != nil; q = *last){
		if(q->export == fs){
			*last = q->next;
			exfree(fs);
			free(q);
			continue;
		}
		last = &q->next;
	}
	unlock(&exq.l);

	lock(&fs->r);
	q = fs->work;
	while(q != nil){
		if(q->shut){
			q = q->next;
			continue;
		}
		q->shut = 1;
		unlock(&fs->r);
		swiproc(q->slave);
		lock(&fs->r);
		q = fs->work;
	}
	unlock(&fs->r);
}

void
exfreefids(Export *fs)
{
	Fid *f, *n;
	int i;

	for(i = 0; i < Nfidhash; i++){
		for(f = fs->fid[i]; f != nil; f = n){
			n = f->next;
			f->attached = 0;
			if(f->ref == 0) {
				if(f->chan != nil)
					cclose(f->chan);
				free(f);
			}
		}
	}
}

void
exfree(Export *fs)
{
	if(decref(&fs->r) != 0)
		return;
	closepgrp(fs->pgrp);
	cclose(fs->root);
	cclose(fs->io);
	exfreefids(fs);
	free(fs);
}

void
exdettach(Export *fs)
{
	qlock(&fs->fidlock);
	exshutdown(fs);
	exfreefids(fs);
	memset(fs->fid, 0, sizeof(fs->fid));
	fs->gen++;
	qunlock(&fs->fidlock);
}

int
exwork(void*)
{
	return exq.head != nil;
}

void
exslave(void*)
{
	Export *fs;
	Exq *q, *t, **last;
	char *err;
	int n;

	closepgrp(up->env->pgrp);
	up->env->pgrp = nil;
	for(;;){
		qlock(&exq.qwait);
		sleep(&exq.rwait, exwork, nil);

		lock(&exq.l);
		q = exq.head;
		if(q == nil) {
			unlock(&exq.l);
			qunlock(&exq.qwait);
			continue;
		}
		exq.head = q->next;
		q->slave = up;
		unlock(&exq.l);

		qunlock(&exq.qwait);

		fs = q->export;
		lock(&fs->r);
		q->next = fs->work;
		fs->work = q;
		unlock(&fs->r);

		up->env->pgrp = q->export->pgrp;

		if(exdebug > 1)
			print("exslave dispatch %F\n", &q->rpc);

		if(q->rpc.type >= Tmax || !fcalls[q->rpc.type])
			err = "bad fcall type";
		else
			err = (*fcalls[q->rpc.type])(fs, &q->rpc);

		lock(&fs->r);
		last = &fs->work;
		for(t = fs->work; t != nil; t = t->next){
			if(t == q){
				*last = q->next;
				break;
			}
			last = &t->next;
		}
		unlock(&fs->r);

		q->rpc.type++;
		if(err){
			q->rpc.type = Rerror;
			strncpy(q->rpc.ename, err, ERRLEN);
		}
		n = convS2M(&q->rpc, q->buf);
		if(n < 0)
			panic("bad message type in exslave");

		if(exdebug)
			print("exslave -> %F\n", &q->rpc);

		if(!waserror()){
			devtab[fs->io->type]->write(fs->io, q->buf, n, 0);
			poperror();
		}
		if(exdebug > 1)
			print("exslave written %d\n", q->rpc.tag);

		exfree(q->export);
		free(q);
	}
	print("exslave shut down");
	pexit("exslave shut down", 0);
}

Qid
Exrmtqid(Chan *c)
{
	Qid q;
	ulong qid;

	qid = c->qid.path^(c->dev<<16)^(c->type<<24);
	qid &= ~CHDIR;
	q.path = (c->qid.path&CHDIR)|qid;
	q.vers = c->qid.vers;
	return q;
}

Fid*
Exmkfid(Export *fs, int fid)
{
	ulong h;
	Fid *f, *nf;

	nf = malloc(sizeof(Fid));
	if(nf == nil)
		return nil;
	qlock(&fs->fidlock);
	h = fid % Nfidhash;
	for(f = fs->fid[h]; f != nil; f = f->next){
		if(f->fid == fid){
			qunlock(&fs->fidlock);
			free(nf);
			return nil;
		}
	}

	nf->next = fs->fid[h];
	if(nf->next != nil)
		nf->next->last = &nf->next;
	nf->last = &fs->fid[h];
	fs->fid[h] = nf;

	nf->fid = fid;
	nf->ref = 1;
	nf->attached = 1;
	nf->offset = 0;
	nf->chan = nil;
	nf->gen = fs->gen;
	qunlock(&fs->fidlock);
	return nf;
}

Fid*
Exgetfid(Export *fs, int fid)
{
	Fid *f;
	ulong h;

	qlock(&fs->fidlock);
	h = fid % Nfidhash;
	for(f = fs->fid[h]; f; f = f->next) {
		if(f->fid == fid){
			if(f->attached == 0)
				break;
			f->ref++;
			qunlock(&fs->fidlock);
			return f;
		}
	}
	qunlock(&fs->fidlock);
	return nil;
}

void
Exputfid(Export *fs, Fid *f)
{
	Chan *c;

	qlock(&fs->fidlock);
	f->ref--;
	if(f->ref == 0 && f->attached == 0){
		c = f->chan;
		f->chan = nil;
		if(f->gen == fs->gen) {
			*f->last = f->next;
			if(f->next != nil)
				f->next->last = f->last;
		}
		qunlock(&fs->fidlock);
		if(c != nil)
			cclose(c);
		free(f);
		return;
	}
	qunlock(&fs->fidlock);
}

char*
Exnop(Export *, Fcall *)
{
	return nil;
}

char*
Exattach(Export *fs, Fcall *rpc)
{
	Fid *f;

	if(fs->excl)
		exdettach(fs);
	f = Exmkfid(fs, rpc->fid);
	if(f == nil)
		return Einuse;
	if(waserror()){
		f->attached = 0;
		Exputfid(fs, f);
		return up->env->error;
	}
	f->chan = cclone(fs->root, nil);
	poperror();
	rpc->qid = Exrmtqid(f->chan);
	Exputfid(fs, f);
	return nil;
}

char*
Exclone(Export *fs, Fcall *rpc)
{
	Fid *f, *nf;

	if(rpc->fid == rpc->newfid)
		return Einuse;
	f = Exgetfid(fs, rpc->fid);
	if(f == nil)
		return Enofid;
	nf = Exmkfid(fs, rpc->newfid);
	if(nf == nil){
		Exputfid(fs, f);
		return Einuse;
	}
	if(waserror()){
		Exputfid(fs, f);
		Exputfid(fs, nf);
		return up->env->error;
	}
	nf->chan = cclone(f->chan, nil);
	poperror();
	Exputfid(fs, f);
	Exputfid(fs, nf);
	return nil;
}

char*
Exclunk(Export *fs, Fcall *rpc)
{
	Fid *f;

	f = Exgetfid(fs, rpc->fid);
	if(f != nil){
		f->attached = 0;
		Exputfid(fs, f);
	}
	return nil;
}

char*
Exwalk(Export *fs, Fcall *rpc)
{
	Fid *f;
	Chan *c;

	f = Exgetfid(fs, rpc->fid);
	if(f == nil)
		return Enofid;
	if(waserror()){
		Exputfid(fs, f);
		return up->env->error;
	}
	c = walk(f->chan, rpc->name, 1);
	if(c == nil)
		error(Enonexist);
	poperror();

	f->chan = c;
	rpc->qid = Exrmtqid(c);
	Exputfid(fs, f);
	return nil;
}

char*
Exopen(Export *fs, Fcall *rpc)
{
	Fid *f;
	Chan *c;

	f = Exgetfid(fs, rpc->fid);
	if(f == nil)
		return Enofid;
	if(waserror()){
		Exputfid(fs, f);
		return up->env->error;
	}
	c = f->chan;
	c = devtab[c->type]->open(c, rpc->mode);
	poperror();

	f->chan = c;
	f->offset = 0;
	rpc->qid = Exrmtqid(c);
	Exputfid(fs, f);
	return nil;
}

char*
Excreate(Export *fs, Fcall *rpc)
{
	Fid *f;
	Chan *c;

	f = Exgetfid(fs, rpc->fid);
	if(f == nil)
		return Enofid;
	if(waserror()){
		Exputfid(fs, f);
		return up->env->error;
	}
	c = f->chan;
	if(c->mnt && !(c->flag&CCREATE))
		c = createdir(c);
	devtab[c->type]->create(c, rpc->name, rpc->mode, rpc->perm);
	poperror();

	f->chan = c;
	rpc->qid = Exrmtqid(c);
	Exputfid(fs, f);
	return nil;
}

char*
Exread(Export *fs, Fcall *rpc)
{
	Fid *f;
	Chan *c;
	long off;
	int dir, n, seek;

	f = Exgetfid(fs, rpc->fid);
	if(f == nil)
		return Enofid;
	c = f->chan;
	dir = c->qid.path & CHDIR;
	if(dir){
		rpc->count -= rpc->count%DIRLEN;
		if(rpc->offset%DIRLEN || rpc->count==0){
			Exputfid(fs, f);
			return Ereaddir;
		}
		if(rpc->offset < c->offset){
			Exputfid(fs, f);
			return Eseekdir;
		}
	}

	if(waserror()) {
		Exputfid(fs, f);
		return up->env->error;
	}

	for(;;){
		n = rpc->count;
		seek = 0;
		off = rpc->offset;
		if(dir && f->offset != off){
			off = f->offset;
			n = rpc->offset - off;
			if(n > MAXDIRREAD)
				n = MAXDIRREAD;
			seek = 1;
		}
		if(dir && c->mnt != nil)
			n = unionread(c, rpc->data, n);
		else {
			c->offset = off;
			n = devtab[c->type]->read(c, rpc->data, n, off);
			lock(c);
			c->offset += n;
			unlock(c);
		}
		if(n == 0 || !seek)
			break;
		f->offset = off + n;
	}
	rpc->count = n;
	poperror();
	Exputfid(fs, f);
	return nil;
}

char*
Exwrite(Export *fs, Fcall *rpc)
{
	Fid *f;
	Chan *c;

	f = Exgetfid(fs, rpc->fid);
	if(f == nil)
		return Enofid;
	if(waserror()){
		Exputfid(fs, f);
		return up->env->error;
	}
	c = f->chan;
	if(c->qid.path & CHDIR)
		error(Eisdir);
	rpc->count = devtab[c->type]->write(c, rpc->data, rpc->count, rpc->offset);
	c->offset += rpc->count;
	poperror();
	Exputfid(fs, f);
	return nil;
}

char*
Exstat(Export *fs, Fcall *rpc)
{
	Fid *f;
	Chan *c;

	f = Exgetfid(fs, rpc->fid);
	if(f == nil)
		return Enofid;
	if(waserror()){
		Exputfid(fs, f);
		return up->env->error;
	}
	c = f->chan;
	devtab[c->type]->stat(c, rpc->stat);
	poperror();
	Exputfid(fs, f);
	return nil;
}

char*
Exwstat(Export *fs, Fcall *rpc)
{
	Fid *f;
	Chan *c;

	f = Exgetfid(fs, rpc->fid);
	if(f == nil)
		return Enofid;
	if(waserror()){
		Exputfid(fs, f);
		return up->env->error;
	}
	c = f->chan;
	devtab[c->type]->wstat(c, rpc->stat);
	poperror();
	Exputfid(fs, f);
	return nil;
}

char*
Exremove(Export *fs, Fcall *rpc)
{
	Fid *f;
	Chan *c;

	f = Exgetfid(fs, rpc->fid);
	if(f == nil)
		return Enofid;
	if(waserror()){
		Exputfid(fs, f);
		return up->env->error;
	}
	c = f->chan;
	devtab[c->type]->remove(c);
	poperror();

	/*
	 * chan is already clunked by remove.
	 * however, we need to recover the chan,
	 * and follow sysremove's lead in making to point to root.
	 */
	c->type = 0;

	f->attached = 0;
	Exputfid(fs, f);
	return nil;
}
