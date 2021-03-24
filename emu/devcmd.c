#include	"dat.h"
#include	"fns.h"
#include	"error.h"

enum
{
	Qtopdir		= 1,	/* top level directory */
	Qprotodir,
	Qclonus,
	Qconvdir,
	Qdata,
	Qctl,
	Qstatus,

	MAXPROTO	= 1
};
#define TYPE(x) 	((x).path & 0xf)
#define CONV(x) 	(((x).path >> 4)&0xfff)
#define PROTO(x) 	(((x).path >> 16)&0xff)
#define QID(p, c, y) 	(((p)<<16) | ((c)<<4) | (y))

typedef struct Proto	Proto;
typedef struct Conv	Conv;
struct Conv
{
	int	x;
	Ref	r;
	int	rfd;
	int	wfd;
	int	perm;
	char	owner[NAMELEN];
	char*	state;
	char*	cmd;
	Proto*	p;
};

struct Proto
{
	Lock	l;
	int	x;
	int	stype;
	char	name[NAMELEN];
	int	nc;
	int	maxconv;
	Conv**	conv;
	Qid	qid;
};

static	int	np;
static	Proto	proto[MAXPROTO];
static	Conv*	protoclone(Proto*, char*);


int
cmdgen(Chan *c, Dirtab *d, int nd, int s, Dir *dp)
{
	Qid q;
	Conv *cv;
	char name[16], *p;

	USED(nd);
	USED(d);

	q.vers = 0;
	switch(TYPE(c->qid)) {
	case Qtopdir:
		if(s >= np)
			return -1;

		q.path = QID(s, 0, Qprotodir)|CHDIR;
		devdir(c, q, proto[s].name, 0, "cmd", CHDIR|0555, dp);
		return 1;
	case Qprotodir:
		if(s < proto[PROTO(c->qid)].nc) {
			cv = proto[PROTO(c->qid)].conv[s];
			sprint(name, "%d", s);
			q.path = QID(PROTO(c->qid), s, Qconvdir)|CHDIR;
			devdir(c, q, name, 0, cv->owner, CHDIR|0555, dp);
			return 1;
		}
		s -= proto[PROTO(c->qid)].nc;
		switch(s) {
		default:
			return -1;
		case 0:
			p = "clone";
			q.path = QID(PROTO(c->qid), 0, Qclonus);
			break;
		}
		devdir(c, q, p, 0, "cmd", 0555, dp);
		return 1;
	case Qconvdir:
		cv = proto[PROTO(c->qid)].conv[CONV(c->qid)];
		switch(s) {
		default:
			return -1;
		case 0:
			q.path = QID(PROTO(c->qid), CONV(c->qid), Qdata);
			devdir(c, q, "data", 0, cv->owner, cv->perm, dp);
			return 1;
		case 1:
			q.path = QID(PROTO(c->qid), CONV(c->qid), Qctl);
			devdir(c, q, "ctl", 0, cv->owner, cv->perm, dp);
			return 1;
		case 2:
			p = "status";
			q.path = QID(PROTO(c->qid), CONV(c->qid), Qstatus);
			break;
		}
		devdir(c, q, p, 0, cv->owner, 0444, dp);
		return 1;
	}
	return -1;
}

static void
newproto(char *name, int maxconv)
{
	int l;
	Proto *p;

	if(np >= MAXPROTO) {
		print("no %s: increase MAXPROTO", name);
		return;
	}

	p = &proto[np];
	strcpy(p->name, name);
	p->qid.path = CHDIR|QID(np, 0, Qprotodir);
	p->x = np++;
	p->maxconv = maxconv;
	l = sizeof(Conv*)*(p->maxconv+1);
	p->conv = malloc(l);
	if(p->conv == 0)
		panic("no memory");
}

void
cmdinit(void)
{
	newproto("cmd", 100);
}

Chan *
cmdattach(void *spec)
{
	Chan *c;

	c = devattach('C', spec);
	c->qid.path = QID(0, 0, Qtopdir)|CHDIR;
	c->qid.vers = 0;
	return c;
}

Chan*
cmdclone(Chan *c, Chan *nc)
{
	return devclone(c, nc);
}

int
cmdwalk(Chan *c, char *name)
{
	return devwalk(c, name, 0, 0, cmdgen);
}

void
cmdstat(Chan *c, char *db)
{
	devstat(c, db, 0, 0, cmdgen);
}

Chan *
cmdopen(Chan *c, int omode)
{
	Proto *p;
	int perm;
	Conv *cv;
	char *user;

	perm = 0;
	omode &= 3;
	switch(omode) {
	case OREAD:
		perm = 4;
		break;
	case OWRITE:
		perm = 2;
		break;
	case ORDWR:
		perm = 6;
		break;
	}

	switch(TYPE(c->qid)) {
	default:
		break;
	case Qtopdir:
	case Qprotodir:
	case Qconvdir:
	case Qstatus:
		if(omode != OREAD)
			error(Eperm);
		break;
	case Qclonus:
		p = &proto[PROTO(c->qid)];
		cv = protoclone(p, up->env->user);
		if(cv == 0)
			error(Enodev);
		c->qid.path = QID(p->x, cv->x, Qctl);
		c->qid.vers = 0;
		break;
	case Qdata:
	case Qctl:
		p = &proto[PROTO(c->qid)];
		lock(&p->l);
		cv = p->conv[CONV(c->qid)];
		lock(&cv->r.l);
		user = up->env->user;
		if((perm & (cv->perm>>6)) != perm) {
			if(strcmp(user, cv->owner) != 0 ||
		 	  (perm & cv->perm) != perm) {
				unlock(&cv->r.l);
				unlock(&p->l);
				error(Eperm);
			}
		}
		cv->r.ref++;
		if(cv->r.ref == 1) {
			cv->state = "Open";
			memmove(cv->owner, user, NAMELEN);
			cv->perm = 0660;
		}
		unlock(&cv->r.l);
		unlock(&p->l);
		break;
	}
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

void
cmdclose(Chan *c)
{
	Conv *cc;

	if((c->flag & COPEN) == 0)
		return;

	switch(TYPE(c->qid)) {
	case Qctl:
	case Qdata:
		cc = proto[PROTO(c->qid)].conv[CONV(c->qid)];
		if(decref(&cc->r) != 0)
			break;
		strcpy(cc->owner, "cmd");
		cc->perm = 0666;
		cc->state = "Closed";
		free(cc->cmd);
		cc->cmd = nil;
		close(cc->rfd);
		close(cc->wfd);
		cc->rfd = -1;
		cc->wfd = -1;
		break;
	}
}

long
cmdread(Chan *ch, void *a, long n, ulong offset)
{
	int r;
	Conv *c;
	Proto *x;
	char buf[128], ebuf[ERRLEN], *p, *cmd;

	USED(offset);

	p = a;
	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qprotodir:
	case Qtopdir:
	case Qconvdir:
		return devdirread(ch, a, n, 0, 0, cmdgen);
	case Qctl:
		sprint(buf, "%d", CONV(ch->qid));
		return readstr(offset, p, n, buf);
	case Qstatus:
		x = &proto[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		cmd = "";
		if(c->cmd != nil)
			cmd = c->cmd;
		snprint(buf, sizeof(buf), "%s/%d %d %s %s\n",
			c->p->name, c->x, c->r.ref, c->state, cmd);
		return readstr(offset, p, n, buf);
	case Qdata:
		c = proto[PROTO(ch->qid)].conv[CONV(ch->qid)];
		osenter();
		r = read(c->rfd, a, n);
		osleave();
		if(r == 0)
			error(Ehungup);
		if(r < 0) {
			oserrstr(ebuf);
			error(ebuf);
		}
		return r;
	}
}

long
cmdwrite(Chan *ch, void *a, long n, ulong offset)
{
	int r;
	Conv *c;
	Proto *x;
	char buf[MAXDEVCMD], ebuf[ERRLEN];

	USED(offset);

	switch(TYPE(ch->qid)) {
	default:
		error(Eperm);
	case Qctl:
		x = &proto[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		if(n > sizeof(buf)-1)
			n = sizeof(buf)-1;
		memmove(buf, a, n);
		buf[n] = '\0';

		if(strncmp(buf, "exec", 4) == 0) {
			if(c->wfd != -1)
				error(Einuse);
			if(oscmd(buf+5, &c->rfd, &c->wfd) == -1) {
				oserrstr(ebuf);
				error(ebuf);
			}
			free(c->cmd);
			c->cmd = strdup(buf);
			c->state = "Execute";
			return n;
		}
		error("bad control message");
	case Qdata:
		x = &proto[PROTO(ch->qid)];
		c = x->conv[CONV(ch->qid)];
		osenter();
		r = write(c->wfd, a, n);
		osleave();
		if(r == 0)
			error(Ehungup);
		if(r < 0) {
			oserrstr(ebuf);
			error(ebuf);
		}
		return r;
	}
	return n;
}

static Conv*
protoclone(Proto *p, char *user)
{
	Conv *c, **pp, **ep;

	c = 0;
	lock(&p->l);
	if(waserror()) {
		unlock(&p->l);
		nexterror();
	}
	ep = &p->conv[p->maxconv];
	for(pp = p->conv; pp < ep; pp++) {
		c = *pp;
		if(c == 0) {
			c = malloc(sizeof(Conv));
			if(c == 0)
				error(Enomem);
			lock(&c->r.l);
			c->r.ref = 1;
			c->p = p;
			c->x = pp - p->conv;
			p->nc++;
			*pp = c;
			break;
		}
		lock(&c->r.l);
		if(c->r.ref == 0) {
			c->r.ref++;
			break;
		}
		unlock(&c->r.l);
	}
	if(pp >= ep) {
		unlock(&p->l);
		poperror();
		return 0;
	}

	strcpy(c->owner, user);
	c->perm = 0660;
	c->state = "Closed";
	c->rfd = -1;
	c->wfd = -1;

	unlock(&c->r.l);
	unlock(&p->l);
	poperror();
	return c;
}

Dev cmddevtab = {
	'C',
	"cmd",

	cmdinit,
	cmdattach,
	cmdclone,
	cmdwalk,
	cmdstat,
	cmdopen,
	devcreate,
	cmdclose,
	cmdread,
	devbread,
	cmdwrite,
	devbwrite,
	devremove,
	devwstat
};
