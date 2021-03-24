#include <u.h>

typedef struct MHdr MHdr;

struct MHdr
{
	ulong nword;
	MHdr *next;
	MHdr *prev;
};

MHdr *freelist;

void*
malloc(ulong size)
{
	MHdr *c;
	ulong *data;
	ulong nword = (size + sizeof(*c) + 3) >> 2;

	if(size == 0)
		return nil;

	c = freelist;
	while(c && c->nword < nword)
		c = c->next;
	if(c == nil)
		return nil;
	if(c->nword == nword) {
		if(c->prev == nil)
			freelist = c->next;
		else
			c->prev->next = c->next;
		data = (ulong*)c;
		nword += (sizeof(*c) >> 2);
	} else {
		c->nword -= nword;
		data = ((ulong*)&c[1])+c->nword;
	}
	data[0] = nword;
        memset((void *)&data[1], 0, size);
	return &data[1];
}

void
free(void *ptr)
{
	ulong *data = (ulong*)ptr-1;
	MHdr *c, *p, *n;
	ulong nword;
	ulong *e;

	nword = *data;
	n = (MHdr*)data;
	if(freelist == nil) {
		freelist = n;
		freelist->nword = nword - (sizeof(*freelist) >> 2);
		freelist->next = freelist->prev = nil;
		return;
	}
	for(p=nil, c=freelist; c != nil && c < n; p=c, c=c->next) ;
	n->nword = nword - (sizeof(*n) >> 2);
	if(c == nil) {
		n->next = nil;
		n->prev = p;
		p->next = n;
	} else if(p == nil) {
		n->next = freelist;
		n->prev = nil;
		freelist = n;
	} else {
		n->next = p->next;
		n->next->prev = n;
		p->next = n;
	}
	for(c = freelist ; c != nil; c = c->next) {
		e = ((ulong*)&c[1])+c->nword;
		n = c->next;
		while(n != nil && (ulong*)n == e) {
			c->nword += n->nword + (sizeof(*n)>>2);
			c->next = n->next;
			n = n->next;
			e = ((ulong*)&c[1])+c->nword;
		}
	}
}

void
mallocinit(ulong low, ulong high)
{
	freelist = (MHdr*)low;
	freelist->nword = (high - low - sizeof(*freelist))>>2;
	freelist->next = freelist->prev = nil;
}
