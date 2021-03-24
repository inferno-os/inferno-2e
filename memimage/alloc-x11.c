#include <lib9.h>
#include <image.h>
#include <memimage.h>

#include "xmem.h"

#include <pool.h>	/* #defines parent, used as a field in X includes */

extern	Pool*	imagmem;
int	isX = 1;

Memimage*
allocmemimage(Rectangle r, int ld)
{
	ulong l, ws, nw;
	int pmid;
	Memimage *i;
	Memdata *md;
	Xmem *xm;

	i = nil;
	xm = nil;
	md = nil;
	if((ld<0 || ld>3) || (ld!=0 && !((1<<ld)&xscreendepth))){
    Error:
		free(i);
		free(xm);
		free(md);
		return nil;
	}

	ws = (8*sizeof(ulong))>>ld;
	l = wordsperline(r, ld);
	md = malloc(sizeof(Memdata));
	if(md == nil)
		goto Error;
	xm = malloc(sizeof(Xmem));
	if(xm == 0)
		goto Error;
	pmid = XCreatePixmap(xdisplay, xscreenid, Dx(r), Dy(r), ld==3 ? xscreendepth : 1<<ld);
	if(ld == 0)
		xm->pmid0 = pmid;
	else
		xm->pmid0 = PMundef;
	xm->pmid = pmid;
	xm->flag = 0;
	xm->wordp = &xm->word;
	md->base = nil;
	md->data = xm->wordp;

	i = malloc(sizeof(Memimage));
	if(i == nil)
		goto Error;
	i->data = md;
	i->X = xm;
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

	return i;
}

Memimage*
clonememimage(Memimage *m)
{
	ulong l, ws, nw;
	int pmid;
	Memimage *i;
	Memdata *md;
	Xmem *xm, *mxm;

	i = nil;
	xm = nil;
	md = malloc(sizeof(Memdata));
	if(md == nil) {
    Error:
		free(i);
		free(xm);
		free(md);
		return nil;
	}
	xm = malloc(sizeof(Xmem));
	if(xm == 0)
		goto Error;
	mxm = (Memdata*)m->X;
	xm->pmid = mxm->pmid;
	xm->pmid0 = PMundef;
	xm->flag = XXcloned;
	xm->wordp = &xm->word;
	md->base = nil;
	md->data = xm->wordp;

	i = malloc(sizeof(Memimage));
	if(i == nil)
		goto Error;
	i->data = md;
	i->X = xm;
	i->zero = m->zero;
	i->width = m->width;
	i->ldepth = m->ldepth;
	i->r = m->r;
	i->clipr = m->clipr;
	i->repl = 0;
	i->layer = nil;

	return i;
}

int*
xpmids[] =
{
	&xgczeropm,
	&xgczeropm0,
	&xgcsimplepm,
	&xgcsimplepm0,
	&xgcreplsrctile,
	&xgcreplsrctile0,
	0
};

void
cleanpmids(int pmid)
{
	int i;

	for(i=0; xpmids[i]; i++)
		if(pmid == *xpmids[i])
			*xpmids[i] = PMundef;
}

void
xdirtied(Memimage *i)
{
	Xmem *xm;

	xm = i->X;
	if(xm->flag&XXonepixel)
		xm->flag &= ~XXonepixel;
	if(xm->pmid != PMundef)
		cleanpmids(xm->pmid);
	if(i->ldepth == 0)
		return;
	if(xm->pmid0 == PMundef)
		return;
	cleanpmids(xm->pmid0);
	/* release ldepth 0 copy of picture */
	XFreePixmap(xdisplay, xm->pmid0);
	xm->pmid0 = PMundef;
}

void
freememimage(Memimage *i)
{
	Xmem *xm;

	if(i == 0)
		return;
	xm = i->X;
	if(xm->pmid != PMundef){
		if(!(xm->flag&XXcloned))
			XFreePixmap(xdisplay, xm->pmid);
		cleanpmids(xm->pmid);
	}
	if(xm->pmid0!=xm->pmid && xm->pmid0!=PMundef){
		XFreePixmap(xdisplay, xm->pmid0);
		cleanpmids(xm->pmid0);
	}
	free(xm);
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
	GC g;
	Xmem *xm;

	xm = i->X;
	if(i->ldepth == 0){
		g = xgcfill0;
		val = val!=0;
		if(xgcfillcolor0 != val){
			XSetForeground(xdisplay, g, val);
			xgcfillcolor0 = val;
		}
	}else{
		g = xgcfill;
		if(xtblbit && i->ldepth==3)
			val = infernotox11[val];
		if(xgcfillcolor != val){
			XSetForeground(xdisplay, g, val);
			xgcfillcolor = val;
		}
	}
	XFillRectangle(xdisplay, xm->pmid, g, 0, 0, Dx(i->r), Dy(i->r));
}
