#include	"dat.h"
#include	"fns.h"
#include	"error.h"

void
lock(Lock *l)
{
	while(!canlock(l))
		osyield();
}

void
unlock(Lock *l)
{
	l->key = 0;
}

void
qlock(QLock *q)
{
	Proc *p;

	lock(&q->use);
	if(!q->locked) {
		q->locked = 1;
		unlock(&q->use);
		return;
	}
	p = q->tail;
	if(p == 0)
		q->head = up;
	else
		p->qnext = up;
	q->tail = up;
	up->qnext = 0;
	unlock(&q->use);
	rendezvous(up, 0);
}

int
canqlock(QLock *q)
{
	if(!canlock(&q->use))
		return 0;
	if(q->locked){
		unlock(&q->use);
		return 0;
	}
	q->locked = 1;
	unlock(&q->use);
	return 1;
}

void
qunlock(QLock *q)
{
	Proc *p;

	lock(&q->use);
	p = q->head;
	if(p) {
		q->head = p->qnext;
		if(q->head == 0)
			q->tail = 0;
		unlock(&q->use);
		rendezvous(p, 0);
		return;
	}
	q->locked = 0;
	unlock(&q->use);
}

void
rlock(RWlock *l)
{
	qlock(&l->x);		/* wait here for writers and exclusion */
	lock(&l->l);
	l->readers++;
	canqlock(&l->k);	/* block writers if we are the first reader */
	unlock(&l->l);
	qunlock(&l->x);
}

void
runlock(RWlock *l)
{
	lock(&l->l);
	if(--l->readers == 0)	/* last reader out allows writers */
		qunlock(&l->k);
	unlock(&l->l);
}

void
wlock(RWlock *l)
{
	qlock(&l->x);		/* wait here for writers and exclusion */
	qlock(&l->k);		/* wait here for last reader */
}

void
wunlock(RWlock *l)
{
	qunlock(&l->k);
	qunlock(&l->x);
}
