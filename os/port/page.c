#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

#define	pghash(daddr)	palloc.hash[(daddr>>PGSHIFT)&(PGHSIZE-1)]

static	Lock pglock;
struct	Palloc palloc;

void
pageinit(void)
{
	int color;
	Page *p;
	ulong np;

	np = palloc.np0+palloc.np1;
	palloc.head = xalloc(np*sizeof(Page));
	if(palloc.head == 0)
		panic("pageinit");

	color = 0;
	p = palloc.head;
	while(palloc.np0 > 0) {
		p->prev = p-1;
		p->next = p+1;
		p->pa = palloc.p0;
		p->color = color;
		palloc.freecount++;
		color = (color+1)%NCOLOR;
		palloc.p0 += BY2PG;
		palloc.np0--;
		p++;
	}
	while(palloc.np1 > 0) {
		p->prev = p-1;
		p->next = p+1;
		p->pa = palloc.p1;
		p->color = color;
		palloc.freecount++;
		color = (color+1)%NCOLOR;
		palloc.p1 += BY2PG;
		palloc.np1--;
		p++;
	}
	palloc.tail = p - 1;
	palloc.head->prev = 0;
	palloc.tail->next = 0;

	palloc.user = p - palloc.head;
}

Page*
newpage(int clear, Segment **s, ulong va)
{
	Page *p;
	KMap *k;
	uchar ct;
	int i, dontalloc, color;


	lock(&palloc);
retry:
	color = getpgcolor(va);
	for(;;) {
		if(palloc.freecount > 0)
			break;
		if(up->kp && palloc.freecount > 0)
			break;

		unlock(&palloc);
		dontalloc = 0;
		if(s && *s) {
			qunlock(&((*s)->lk));
			*s = 0;
			dontalloc = 1;
		}
		qlock(&palloc.pwait);	/* Hold memory requesters here */

		panic("no pages");
	}

	/* First try for our colour */
	for(p = palloc.head; p; p = p->next)
		if(p->color == color)
			break;

	ct = PG_NOFLUSH;
	if(p == 0) {
		p = palloc.head;
		p->color = color;
		ct = PG_NEWCOL;
	}

	if(p->prev) 
		p->prev->next = p->next;
	else
		palloc.head = p->next;

	if(p->next)
		p->next->prev = p->prev;
	else
		palloc.tail = p->prev;

	palloc.freecount--;
	unlock(&palloc);

	lock(p);
	if(p->ref != 0) {	/* lookpage has priority on steal */
		unlock(p);
		print("stolen\n");
		lock(&palloc);
		palloc.freecount++;
		goto retry;
	}

	p->ref++;
	p->va = va;
	p->modref = 0;
	for(i = 0; i < MAXMACH; i++)
		p->cachectl[i] = ct;
	unlock(p);

	if(clear) {
		k = kmap(p);
		memset((void*)VA(k), 0, BY2PG);
		kunmap(k);
	}

	return p;
}
