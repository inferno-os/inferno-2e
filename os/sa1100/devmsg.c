#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"
#include	"io.h"
#include	"devtad.h"
#include	"dtad6471c.h"

/*
 * TAD message flash file system
 *
 * TO DO:
 * 	- should eventually access TAD through lower-level device
 *	- check ID mapping rules
 *	- decide whether this clone-style interface is really appropriate
 *	- synchronise playing/paused/idle states with devtad.c
 *	- garbage collection
 *	- data pump
 */

typedef struct Tflash Tflash;
typedef struct Tmsg Tmsg;

#define DPRINT	if(taddebug) print

enum
{
	Qtopdir		= 1,	/* top level directory */
	Q2nd,		/* directory for a protocol */
	Qclone,
	Q3rd,		/* directory for a conversation */
	Qdata,			/* this is body for message & prompt */
	Qctl,
	Qstatus,		/* this is type for message & prompt */
	Qremote,
	Qlocal,
	Qevent,			/* for tel, prompt, msg flash, & cons */

	MAXCONV		= 256	/* power of two */
};

#define TYPE(x) 	((x).path & 0xf)
#define QSHIFT	16
#define CONV(x) 	(((x).path >> QSHIFT)&(MAXCONV-1))
#define PROTO(x) 	(((x).path >> 4)&0xff)
#define QID(p, c, y) 	(((c)<<QSHIFT) | ((p)<<4) | (y))

#define	CLIENTPATH(p)	((p)>>QSHIFT)

enum {
	MaxMessage = 128,	/* EasyTAD's limit; could be higher on other devices */

	/* the time stamp is currently (flags<<24)|(((seconds()-EPOCH)/60)&TimeMask */
	Epoch = 915148800,	/* Fri Jan  1 00:00:00 GMT 1999  */
	TimeMask = 0xFFFFFF,
	ZeroEpoch = 1<<31,	/* system time wasn't beyond Epoch; used 0 */
	UnusedTime = ~0&~ZeroEpoch&~TimeMask,	/* unused flag bits set to 1 allowing overwrite */
};

struct Tmsg {
	Ref	r;	/* references to this structure */
	Ref	dataref;	/* opens of data */
	Ref	eventref;	/* opens of event */
	int	busy;	/* ctl file is open (exclusive use) */
	int	n;		/* device's idea of message's number + 1*/
	int	outgoing;
	int	state;
	long	size;
	long	mtime;
	long	atime;
	ulong	clientid;
	int	slot;	/* index in Tflash.msg (directory index) */
	Queue*	event;
};

enum {	/* Tmsg.state */
	Midle,
	Mplaying,
	Mpaused,
	Mrecording,
	Mdeleted,
};

static char *msgstates[] = {
[Midle] "Idle",
[Mplaying] "Playing",
[Mpaused] "Paused",
[Mrecording] "Recording",
[Mdeleted] "Deleted",
};

struct Tflash {
	QLock;
	int	nmsg;
	int	nclient;
	Tmsg	*msg[MaxMessage];
	int	needgc;
	int	full;
	int	timeavail;
	int	recording;
	int	loaded;
	ulong	clientid;
};
static	Tflash	msgflash;
static char Eunimp[] = "unimplemented";
static char Ebadreq[] = "bad control message";
int	taddebug;

static Tmsg*
msgnew(Tflash *f)
{
	Tmsg *m;
	int i;

	qlock(f);
	for(i=0;; i++){
		if(i >= nelem(f->msg)){
			qunlock(f);
			return nil;
		}
		m = f->msg[i];
		if(m == nil)
			break;
	}
	m = malloc(sizeof(*m));
	if(m != nil){
		memset(m, 0, sizeof(*m));
		m->state = Midle;
		m->slot = i;
		m->clientid = f->clientid++;
		f->msg[i] = m;
		if(i >= f->nclient)
			f->nclient = i+1;
	}
	qunlock(f);
	return m;
}

static void
msgfree(Tmsg *m)
{
	if(m!=nil && decref(&m->r)==0){
		msgflash.msg[m->slot] = 0;
		if(m->event != nil)
			qfree(m->event);
		if(m->slot+1 == msgflash.nclient)
			msgflash.nclient--;
		free(m);
	}
}

static Tmsg *
msgfetch(Tflash *f, int i)
{
	Tmsg *m;
	long t;

	if(i < 0 || i >= nelem(f->msg))
		return nil;
	m = malloc(sizeof(*m));
	if(m == nil)
		error(Enovmem);
	memset(m, 0, sizeof(*m));
	m->state = Midle;
	m->slot = i;
	m->clientid = f->clientid++;
	f->msg[i] = m;
	if(i >= f->nclient)
		f->nclient = i+1;
	incref(&m->r);	/* permanent */
	m->n = i+1;
	m->size = 0;		/* EasyTAD doesn't provide it */
	t = msg_getstamp(i);
	if(t == -1)
		t = ZeroEpoch;
	if(t & ZeroEpoch)
		m->mtime = (t&TimeMask)*60;
	else
		m->mtime = (t&TimeMask)*60 + Epoch;
	m->atime = m->mtime;
	DPRINT("msg %lud: n=%d stamp=%8.8lux mtime=%lud\n", m->clientid, m->n, t, m->mtime);
	return m;
}

static char *
msgstopped(Tmsg *m)
{
	Tflash *f;
	int n, r;

	f = &msgflash;
	switch(m->state){
	case Mrecording:
		m->state = Midle;
		r = stoprecording(&n);
		DPRINT("msgstopped: r=%x n=%d f->nmsg=%d\n", r, n, f->nmsg);
		if(n < f->nmsg)
			return "message flash phase error";
		if(n == f->nmsg)
			return "no recording";
		m->n = n;
		incref(&m->r);	/* make it permanent */
		break;
	case Mplaying:
	case Mpaused:
		m->state = Midle;
		r = stopplaying();
		DPRINT("stopplaying: r=%x\n", r);
		break;
	}
	return nil;
}

static Tmsg*
msgclient(Chan *c)
{
	Tflash *f;
	Tmsg *m;
	int slot;

	f = &msgflash;
	slot = CLIENTPATH(c->qid.path);
	if(slot == 0)
		return nil;
	m = f->msg[slot-1];
	if(m == nil)
		error("no such message");
	return m;
}

static void
msgrefresh(Tflash *f, int)
{
	int s, n;
	Tmsg *m;

	qlock(f);
	if(f->loaded){
		qunlock(f);
		return;
	}
	if(waserror()){
		qunlock(f);
		nexterror();
	}
	s = memory_status(1);
	if(s == 0){
		DPRINT("msgbuild: Idle\n");
		s = memory_status(1);
	}
	n = s & 0x7F;
	if(n == 0)
		n |= s & 0x80;
	f->full = (s & (1<<7))!=0;
	f->needgc = (s & (1<<8)) != 0;
	DPRINT("msgbuild #%4.4ux nmsg %d was %d needgc %d full %d\n", s, n, f->nmsg, f->needgc, f->full);
	for(; f->nmsg < n; f->nmsg++){
		m = msgfetch(f, f->nmsg);
		if(m == nil)
			error(Eio);
	}
	f->loaded = 1;
	poperror();
	qunlock(f);
}

static void
msgdelete(Tflash *f, Tmsg *m)
{
	Tmsg *o;
	int i, s;

	if(m->state==Mdeleted || m->n == 0 || f->nmsg <= 0)
		error("no such message");
	if(msg_delete(0, 0, m->n-1) < 0)
		error("device rejected delete request");
	m->state = Mdeleted;
	for(i=0; i<f->nclient; i++)
		if((o=f->msg[i]) != nil && o->n > m->n)
			o->n--;	/* shuffle down one */
	m->n = 0;
	f->nmsg--;
	decref(&m->r);	/* no longer on flash */
	s = msg_delete(1, 0, 0);
	DPRINT("msg flash collected: %4.4ux\n", s);
}

static void
msgerase(Tflash *f, Tmsg *m)
{
	Tmsg *o;
	int i;

	if(msg_delete(0, 1, 0) < 0)
		error("device rejected erase request");
	for(i=0; i<f->nclient; i++)
		if((o = f->msg[i]) != nil && o->n != 0){
			o->state = Mdeleted;
			o->n = 0;
			f->nmsg--;
			if(o == m)
				decref(&m->r);	/* it will msgfree itself on close */
			else
				msgfree(o);
		}
	if(f->nmsg < 0)
		panic("devmsg.c:/^msgerase");
	DPRINT("msg flash erased\n");
}

static Chan*
msgattach(char *spec)
{
	msgrefresh(&msgflash, 0);
	return devattach('R', spec);
}

static int
msggen(Chan *c, Dirtab*, int, int s, Dir *dp)
{
	int t;
	Qid q;
	ulong path;
	Tmsg *m;
	char buf[NAMELEN];

	q.vers = 0;

	/*
	 * Top level directory contains the name of the device.
	 */
	if(c->qid.path == CHDIR){
		switch(s){
		case 0:
			q = (Qid){CHDIR|Q2nd, 0};
			devdir(c, q, "msg", 0, eve, 0555, dp);
			break;
		default:
			return -1;
		}
		return 1;
	}

	/*
	 * Second level contains "clone" plus all the clients.
	 */
	t = TYPE(c->qid);
	if(t == Q2nd || t == Qclone){
		if(s == 0){
			q = (Qid){Qclone, 0};
			devdir(c, q, "clone", 0, eve, 0666, dp);
		}
		else if(s <= msgflash.nclient){
			m = msgflash.msg[s-1];
			if(m == nil || m->state == Mdeleted)
				return 0;
			sprint(buf, "%lud", m->clientid);
			q = (Qid){CHDIR|(s<<QSHIFT)|Q3rd, 0};
			devdir(c, q, buf, 0, eve, 0555, dp);
			dp->mtime = m->mtime;
			return 1;
		}
		else
			return -1;
		return 1;
	}

	/*
	 * Third level.
	 */
	path = c->qid.path&~(CHDIR|((1<<QSHIFT)-1));	/* slot component */
	q.vers = c->qid.vers;
	switch(s){
	case 0:
		q = (Qid){path|Qctl, c->qid.vers};
		devdir(c, q, "ctl", 0, eve, 0600, dp);
		break;
	case 1:
		q = (Qid){path|Qdata, c->qid.vers};
		devdir(c, q, "data", 0, eve, 0600, dp);
		break;
	case 2:
		q = (Qid){path|Qstatus, c->qid.vers};
		devdir(c, q, "status", 0, eve, 0400, dp);
		break;
	case 3:
		q = (Qid){path|Qevent, c->qid.vers};
		devdir(c, q, "event", 0, eve, 0400, dp);
		break;
	default:
		return -1;
	}
	return 1;
}

static int
msgwalk(Chan *c, char *name)
{
	Path *op;

	if(strcmp(name, "..") == 0){
		switch(TYPE(c->qid)){
		case Qtopdir:
			return 1;
		case Q2nd:
			c->qid = (Qid){CHDIR|Qtopdir, 0};
			break;
		case Q3rd:
			c->qid = (Qid){CHDIR|Q2nd, 0};
			break;
		default:
			panic("msgwalk %lux", c->qid.path);
		}
		op = c->path;
		c->path = ptenter(&syspt, op, name);
		decref(op);
		return 1;
	}
	return devwalk(c, name, 0, 0, msggen);
}

static void
msgstat(Chan *c, char *db)
{
	devstat(c, db, 0, 0, msggen);
}

static Chan*
msgopen(Chan *c, int omode)
{
	Tmsg *m;

	if(c->qid.path & CHDIR)
		return devopen(c, omode, 0, 0, msggen);

	c->mode = openmode(omode);

	if(TYPE(c->qid) == Qclone){
		m = msgnew(&msgflash);
		if(m == nil)
			error(Enodev);
		c->qid.path = Qctl|((m->slot+1)<<QSHIFT);
	}

	qlock(&msgflash);
	if(waserror()){
		qunlock(&msgflash);
		nexterror();
	}

	m = msgclient(c);
	switch(TYPE(c->qid)){
	case Qstatus:
		incref(&m->r);
		break;

	case Qctl:
		if(m->busy)
			error(Einuse);
		m->busy = 1;
		incref(&m->r);
		break;

	case Qdata:
		incref(&m->r);
		incref(&m->dataref);
		break;

	case Qevent:
		incref(&m->r);
		incref(&m->eventref);
		if(m->event == nil)
			m->event = qopen(1024, 1, 0, 0);
		else
			qreopen(m->event);
		break;
	}

	poperror();
	qunlock(&msgflash);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

static void
msgclose(Chan *c)
{
	Tmsg *m;

	if(c->qid.path & CHDIR)
		return;
	if((c->flag & COPEN) == 0)
		return;

	qlock(&msgflash);
	if(waserror()){
		qunlock(&msgflash);
		nexterror();
	}

	m = msgclient(c);
	switch(TYPE(c->qid)){
	case Qctl:
		m->busy = 0;
		break;
	case Qdata:
		if(decref(&m->dataref) == 0)
			msgstopped(m);
		break;
	case Qevent:
		if(decref(&m->eventref) == 0 && m->event != nil)
			qclose(m->event);
		break;
	}
	msgfree(m);

	poperror();
	qunlock(&msgflash);
}

static long
msgread(Chan *c, void *a, long n, ulong offset)
{
	Tmsg *m;
	char buf[160];

	if(c->qid.path & CHDIR)
		return devdirread(c, a, n, 0, 0, msggen);
	m = msgclient(c);
	m->atime = seconds();
	switch(TYPE(c->qid)){
	case Qctl:
		return readnum(offset, a, n, m->clientid, 1);

	case Qdata:
		/* TO DO: data pump */
		return 0;

	case Qevent:
		return qread(m->event, a, n);

	case Qstatus:
		snprint(buf, sizeof(buf), "%lud %d %s\n", m->clientid, m->n, msgstates[m->state]);
		return readstr(offset, a, n, buf);
	}
}

static long
msgwrite(Chan *c, void *a, long n, ulong offset)
{
	Tmsg *m;
	int nf;
	long t;
	ulong stamp;
	char buf[128];
	char *fields[10], *err;

	USED(offset);
	if(c->qid.path & CHDIR)
		error(Eisdir);

	qlock(&msgflash);
	if(waserror()){
		qunlock(&msgflash);
		nexterror();
	}

	m = msgclient(c);
	m->atime = seconds();
	switch(TYPE(c->qid)) {
	case Qctl:
		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, a, n);
		buf[n] = '\0';
		nf = parsefields(buf, fields, 3, " \n");
		if(strcmp(fields[0], "connect") == 0){
			/* TO DO (can it be done on EasyTAD?) */
			error(Eunimp);
			break;
		}
		if(strcmp(fields[0], "play") == 0) {
			if(m->state == Mdeleted)
				error("has been deleted");
			if(m->n == 0)
				error("not yet recorded");
			if(m->event != nil)
				qflush(m->event);
			startplaying(m->n-1, m->event);
			m->state = Mplaying;
			break;
		}
		if(strcmp(fields[0], "pause") == 0) {
			if(m->state == Mpaused)
				break;
			if(m->state != Mplaying)
				error("not playing");
			pauseplaying();
			m->state = Mpaused;
			break;
		}
		if(strcmp(fields[0], "continue") == 0) {
			if(m->state == Mplaying)
				break;
			if(m->state != Mpaused)
				error("not paused");
			continueplaying();
			m->state = Mplaying;
			break;
		}
		if(strcmp(fields[0], "record") == 0) {
			if(m->n != 0)
				error("already recorded");
			/* TO DO? check for active call to select input? */
			if(m->event != nil)
				qflush(m->event);
			t = seconds();
			m->mtime = m->atime = t;
			if(t < Epoch)
				stamp = ((t/60)&TimeMask) | ZeroEpoch;
			else
				stamp = (t/60)&TimeMask;
			startrecording(fields, nf, stamp | UnusedTime, m->event);
			m->state = Mrecording;
			break;
		}
		if(strcmp(fields[0], "stop") == 0) {
			err = msgstopped(m);
			if(err != nil)
				error(err);
			break;
		}
		if(strcmp(fields[0], "delete") == 0) {
			msgdelete(&msgflash, m);
			break;
		}
		if(strcmp(fields[0], "refresh") == 0){
			/* reload & renumber */
			error(Eunimp);
			break;
		}
		if(strcmp(fields[0], "erase") == 0){
			msgerase(&msgflash, m);
			break;
		}
		error(Ebadreq);
		break;
	case Qdata:
		/* TO DO: data pump */
		n=0;
		break;
	default:
		error(Ebadusefd);
	}

	poperror();
	qunlock(&msgflash);
	return n;
}

Dev msgdevtab = {
	'R',
	"msg",

	devreset,
	devinit,
	msgattach,
	devdetach,
	devclone,
	msgwalk,
	msgstat,
	msgopen,
	devcreate,
	msgclose,
	msgread,
	devbread,
	msgwrite,
	devbwrite,
	devremove,
	devwstat,
};
