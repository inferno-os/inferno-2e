#include "lib9.h"
#include "image.h"
#include "memimage.h"
#include "pool.h"

extern	Pool*	imagmem;

int	isX = 0;

void
memimagemove(void *from, void *to)
{
	Memdata *md;

	md = *(Memdata**)to;
	if(md->base != from){
		print("compacted data not right: #%.8lux\n", md->base);
		abort();
	}
	md->base = to;
	md->data = &md->base[1];
}

Memimage*
allocmemimage(Rectangle r, int ld)
{
	ulong l, ws, nw;
	Memdata *md;
	Memimage *i;

	poolsetcompact(imagmem, memimagemove);

	i = nil;
	if(ld<0 || ld>3){
    Error:
		free(i);
		return nil;
	}

	ws = (8*sizeof(ulong))>>ld;
	l = wordsperline(r, ld);
	nw = l*Dy(r);
	md = malloc(sizeof(Memdata));
	if(md == nil)
		goto Error;
	md->base = poolalloc(imagmem, (1+nw)*sizeof(ulong));
	if(md->base == nil){
    Error1:
		free(md);
		goto Error;
	}

	if(i == nil){
		i = malloc(sizeof(Memimage));
		if(i == nil) {
			poolfree(imagmem, md->data);
			goto Error1;
		}
	}
	md->base[0] = (ulong)md;
	md->data = &md->base[1];
	i->data = md;
	i->zero = l*r.min.y;
	if(r.min.x >= 0)
		i->zero += r.min.x/ws;
	else
		i->zero -= (-r.min.x+ws-1)/ws;
	i->zero = -i->zero;
	i->width = l;
	i->ldepth = ld;
	i->r = r;
	i->clipr = r;
	i->repl = 0;
	i->layer = nil;
	i->X = nil;
	return i;
}

void
freememimage(Memimage *i)
{
	if(i == nil)
		return;
	if(i->data->base)
		poolfree(imagmem, i->data->base);
	free(i->data);
	free(i);
}

ulong*
wordaddr(Memimage *i, Point p)
{
	return i->data->data+i->zero+(p.y*i->width)+(p.x>>(5-i->ldepth));
}

uchar*
byteaddr(Memimage *i, Point p)
{
	return (uchar*)(i->data->data+i->zero+(p.y*i->width))+(p.x>>(3-i->ldepth));
}

void
memfillcolor(Memimage *i, int val)
{
	memset(i->data->data, val, sizeof(ulong)*i->width*Dy(i->r));
}
