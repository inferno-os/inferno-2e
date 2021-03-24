#include <lib9.h>
#include "dat.h"
#include "fns.h"

int
qempty(Queue *q)
{
	return q->head == nil;
}

int
qlen(Queue *q)
{
	Block *b = q->head;
	int n = 0;

	while(b != nil) {
		n += b->nbytes;
		b = b->link;
	}

	return n;
}

int
qread(Queue *q, char *buf, long n, long offset)
{
	Block *b = q->head;
	Block *next;
	int nread;

	if(b == nil)
		return 0;

	nread = 0;
	while(b && n) {
		if(offset) {
			if(b->nbytes <= offset) {
				offset -= b->nbytes;
				goto nextblock;
			} else {
				b->rp += offset;
				b->nbytes -= offset;
				offset = 0;
			}
		}
		if(b->nbytes <= n) {
			memmove(buf, b->rp, b->nbytes);
			nread += b->nbytes;
			buf += b->nbytes;
			n -= b->nbytes;
		} else {
			memmove(buf, b->rp, n);
			b->rp += n;
			b->nbytes -= n;
			nread += n;
			break;
		}
nextblock:
		next = b->link;
		free(b);
		b = next;
		q->head = b;
	}
	if(b == nil)
		q->tail = nil;
	return nread;
}

int
qwrite(Queue *q, void *buf, long n)
{
	Block *b = newblock(buf, n);

	if(b == nil)
		return -1;

	if(q->tail == nil)
		q->head = q->tail = b;
	else {
		q->tail->link = b;
		q->tail = b;
	}

	return n;
}

void
qflush(Queue *q)
{
	Block *b = q->head;
	Block *n;

	while(b != nil) {
		n = b->link;
		free(b);
		b = n;
	}
	q->head = q->tail = nil;
}

Block *newblock(void *buf, int n)
{
	Block *b = malloc(sizeof(*b)+n);

	if(b == nil)
		return nil;

	b->nbytes = n;
	b->data = b->rp = (char*)b+sizeof(*b);
	b->link = nil;
	memmove(b->data, buf, n);
	
	return b;
}


