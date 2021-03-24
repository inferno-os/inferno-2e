#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

static Ref pgrpid;
static Ref mountid;

Pgrp*
newpgrp(void)
{
	Pgrp *p;

	p = smalloc(sizeof(Pgrp));
	p->ref = 1;
	p->pgrpid = incref(&pgrpid);
	p->pin = Nopin;
	p->progmode = 0644;
	return p;
}

void
closepgrp(Pgrp *p)
{
	Mhead **h, **e, *f, *next;
	
	if(decref(p) != 0)
		return;

	p->pgrpid = -1;

	e = &p->mnthash[MNTHASH];
	for(h = p->mnthash; h < e; h++) {
		for(f = *h; f; f = next) {
			cclose(f->from);
			mountfree(f->mount);
			next = f->hash;
			free(f);
		}
	}
	cclose(p->dot);
	cclose(p->slash);
	free(p);
}

void
pgrpinsert(Mount **order, Mount *m)
{
	Mount *f;

	m->order = 0;
	if(*order == 0) {
		*order = m;
		return;
	}
	for(f = *order; f; f = f->order) {
		if(m->mountid < f->mountid) {
			m->order = f;
			*order = m;
			return;
		}
		order = &f->order;
	}
	*order = m;
}

/*
 * pgrpcpy MUST preserve the mountid allocation order of the parent group
 */
void
pgrpcpy(Pgrp *to, Pgrp *from)
{
	int i;
	Mount *n, *m, **link, *order;
	Mhead *f, **tom, **l, *mh;

	wlock(&from->ns);
	order = 0;
	tom = to->mnthash;
	for(i = 0; i < MNTHASH; i++) {
		l = tom++;
		for(f = from->mnthash[i]; f; f = f->hash) {
			mh = smalloc(sizeof(Mhead));
			mh->from = f->from;
			incref(mh->from);
			*l = mh;
			l = &mh->hash;
			link = &mh->mount;
			for(m = f->mount; m; m = m->next) {
				n = smalloc(sizeof(Mount));
				n->to = m->to;
				incref(n->to);
				n->head = mh;
				n->flag = m->flag;
				if(m->spec[0] != 0)
					strncpy(n->spec, m->spec, NAMELEN);
				m->copy = n;
				pgrpinsert(&order, m);
				*link = n;
				link = &n->next;	
			}
		}
	}
	/*
	 * Allocate mount ids in the same sequence as the parent group
	 */
	lock(&mountid);
	for(m = order; m; m = m->order)
		m->copy->mountid = mountid.ref++;
	unlock(&mountid);

	to->pin = from->pin;

	to->slash = cclone(from->slash, nil);
	to->dot = cclone(from->dot, nil);
	to->nodevs = from->nodevs;

	wunlock(&from->ns);
}

Fgrp*
newfgrp(void)
{
	Fgrp *new;

	new = smalloc(sizeof(Fgrp));
	new->ref = 1;
	return new;
}

Fgrp*
dupfgrp(Fgrp *f)
{
	int i;
	Chan *c;
	Fgrp *new;

	new = newfgrp();

	lock(f);
	new->maxfd = f->maxfd;
	for(i = 0; i <= f->maxfd; i++) {
		if(c = f->fd[i]){
			incref(c);
			new->fd[i] = c;
		}
	}
	unlock(f);

	return new;
}

void
closefgrp(Fgrp *f)
{
	int i;
	Chan *c;

	if(f == 0)
		return;

	if(decref(f) != 0)
		return;

	for(i = 0; i <= f->maxfd; i++)
		if(c = f->fd[i])
			cclose(c);

	free(f);
}

Mount*
newmount(Mhead *mh, Chan *to, int flag, char *spec)
{
	Mount *m;

	m = smalloc(sizeof(Mount));
	m->to = to;
	m->head = mh;
	incref(to);
	m->mountid = incref(&mountid);
	m->flag = flag;
	if(spec != 0)
		strncpy(m->spec, spec, NAMELEN);

	return m;
}

void
mountfree(Mount *m)
{
	Mount *f;

	while(m) {
		f = m->next;
		cclose(m->to);
		m->mountid = 0;
		free(m);
		m = f;
	}
}

void
resrcwait(char *reason)
{
	char *p;

	if(up == 0)
		panic("resrcwait");

	p = up->psstate;
	if(reason) {
		up->psstate = reason;
		print("%s\n", reason);
	}

	tsleep(&up->sleep, return0, 0, 300);
	up->psstate = p;
}
