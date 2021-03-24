#include <lib9.h>
#include <image.h>
#include <memimage.h>
#include "xmem.h"

/*
 * Relabel and hide the standard implementation, and call it as a fallback
 * if we can't implement the operation directly in X calls.
 */
static void _memimagedraw(Memimage*, Rectangle, Memimage*, Point,
		Memimage*, Point, Rectangle);

int	xgcfillcolor = 0;
int	xgcfillcolor0 = 0;
int	xgczeropm = 0;
int	xgczeropm0 = 0;
int	xgcsimplecolor = 0;
int	xgcsimplecolor0 = 0;
int	xgcsimplepm = 0; 
int	xgcsimplepm0 = 0;
int	xgcreplsrctile = 0;
int	xgcreplsrctile0 = 0;

#ifdef out
Lock	x24getl;
Lock	x24putl;
#endif

/*
 * This version requires one of the following for each of source and mask:
 * 1. same ldepth as destination
 * 2. ldepth is 0
 * 3. ldepth is arbitrary but image is a single pixel of value all zeros or all ones.
 * In case 3, because mask is boolean, the condition is relaxed somewhat.
 */

enum statebits
{
	Simplemask = 1,
	Simplesrc = 2,
	Replmask = 4,
	Replsrc = 8
};

enum
{
	Nscan = 16,	/* maximum number of scan lines buffered in a repl'd image */
	Nslop = 16	/* number of extra bytes to allocate per scan line, for overflow on ends */
};

typedef struct Param Param;
struct Param
{
	Memimage	*i;		/* image holding scan lines */
	uchar	*y0addr;	/* byte holding (x, 0), in src coords */
	int		bwidth;	/* width of scan line of storage, in bytes */
	int		needbuf;	/* if false, buffering not necessary */
	int		dldepth;	/* ldepth of destination */
	int		nbytes;	/* in scan line of destination rectangle */
	int		pm;		/* pixel mask: x&pm gives coord of x within byte */
	Rectangle	dr;		/* destination rectangle, in destination coordinates */
	Rectangle	sr;		/* source rectangle, in source coordinates */
	uchar	*buf[Nscan];	/* buffers for aligning source */
	int		nbuf;		/* number of buffers active */
	int		bufno;		/* next buffer to use, modulo nbuf */
};

static uchar*	load3(Param*, int);
static uchar*	loadn(Param*, int);
static uchar*	load3repl(Param*, int);
static uchar*	loadnrepl(Param*, int);
static uchar*	loadconv(Param*, int);
static uchar*	loadconv(Param*, int);
static uchar*	loadconvrepl(Param*, int);
static uchar*	loadconvrepl(Param*, int);

static uchar*	(*loadfn[2][2][2])(Param*, int) = {
	load3,		/* [0][0][0]: no convert, no repl, ldepth=3 */
	loadn,		/* [0][0][1]: no convert, no repl, ldepth!=3 */
	load3repl,		/* [0][1][0]: no convert, repl, ldepth=3 */
	loadnrepl,		/* [0][1][1]: no convert, repl, ldepth!=3 */
	loadconv,		/* [1][0][0]: convert, no repl, ldepth=3 */
	loadconv,		/* [1][0][1]: convert, no repl, ldepth!=3 */
	loadconvrepl,	/* [1][1][0]: convert, repl, ldepth=3 */
	loadconvrepl,	/* [1][1][1]: convert, repl, ldepth!=3 */
};

/*
 * The bitmasks convert pixel values into mask values doing the
 * pixelwise operation
 * if(pixel != 0)
 *	pixel = ~0;
 * They are not needed when ldepth==3 because the loop is
 * based on bytes, without using insert under mask operations.
 */
static uchar bitmasks[3][256];
static int tablesbuilt;

/*
 * The conversion tables are indexed by an ldepth 0 value
 * and produce 2, 4, or 8 bytes of result
 */
static uchar conv01[256][2];
static uchar conv02[256][4];
static uchar conv03[256][8];
static uchar *conv0n[4] = { 0, &conv01[0][0], &conv02[0][0], &conv03[0][0] };

static uchar *drawbuf;
static int drawbufsize;

static
int
setbuf(int nbytes, Param *p, int simple, int repl)
{
	if(!repl){
		p->nbuf = 1;
		return nbytes;
	}
	if(simple){
		p->nbuf = 0;
		return 0;
	}
	p->bufno = 0;
	p->nbuf = Dy(p->i->r);
	if(p->nbuf > Nscan){
		p->nbuf = 1;
		return nbytes;
	}
	return p->nbuf * nbytes;
}


int
drawsetxy(int minx, int maxx, int x)
{
	int sx;

	sx = (x-minx) % (maxx-minx);
	if(sx < 0)
		sx += maxx-minx;
	return sx+minx;
}

/*
 * non-converting, non-replicating, ldepth 3
 */
static
uchar*
load3(Param *p, int y)
{
	uchar *a;

	a = p->y0addr+y*p->bwidth;
	if(p->needbuf){
		memmove(p->buf[0], a, p->nbytes);
		return p->buf[0];
	}
	return a;
}

/*
 * non-converting, non-replicating, ldepth not 3.
 * since loops do end masking, we don't need to here.
 */
static
uchar*
loadn(Param *p, int y)
{
	uchar *a, *b, sv, v;
	int n, deltax, ld;
	Memimage *i;

	a = p->y0addr+y*p->bwidth;
	b = p->buf[0];
	deltax = (p->dr.min.x&p->pm) - (p->sr.min.x&p->pm);
	if(deltax == 0){	/* aligned */
		if(p->needbuf){
			memmove(b, a, p->nbytes);
			return b;
		}
		return a;
	}
	i = p->i;
	ld = i->ldepth;
	if(deltax < 0){
		deltax = (-deltax)<<ld;
		sv = *a++;
		for(n=p->nbytes; --n>=0; ){
			v = sv << deltax;
			sv = *a++;		/* BUG: can probe beyond end */
			*b++ = v | (sv>>(8-deltax));
		}
	}else{
		deltax <<= ld;
		v = 0;
		for(n=p->nbytes; --n>=0; ){
			sv = *a++;		/* BUG: can probe beyond end */
			*b++ = v | (sv>>deltax);
			v = sv<<(8-deltax);
		}
	}
	return p->buf[0];
}

/*
 * non-converting, replicating, ldepth 3
 */
static
uchar*
load3repl(Param *p, int y)
{
	uchar *a, *ba, *ea, *b, *buf;
	int n, x, ix, iy;
	Memimage *i;

	i = p->i;
	n = p->bufno++;
	buf = p->buf[n % p->nbuf];
	if(n>=p->nbuf && p->nbuf==Dy(i->r))	/* already done the job? */
		return buf;
	b = buf;
	ix = drawsetxy(i->r.min.x, i->r.max.x, p->sr.min.x);
	iy = drawsetxy(i->r.min.y, i->r.max.y, y);
	a = byteaddr(i, Pt(ix, iy));
	ba = byteaddr(i, Pt(i->r.min.x, iy));
	ea = byteaddr(i, Pt(i->r.max.x, iy));
	for(x=p->sr.min.x; x<p->sr.max.x; x++){
		*b++ = *a++;
		if(a >= ea)
			a = ba;
	}
	return buf;
}

/*
 * non-converting, replicating, ldepth not 3.
 * since loops do end masking, we don't need to here.
 */
static
uchar*
loadnrepl(Param *p, int y)
{
	uchar *a, *b, *ba, *buf;
	int n, v, sh, ddx, dx, sx, ix, iy, ld, minsx, maxsx, maxdx, np, sx7;
	Memimage *i;

	i = p->i;
	n = p->bufno++;
	buf = p->buf[n % p->nbuf];
	if(n>=p->nbuf && p->nbuf==Dy(i->r))	/* already done the job? */
		return buf;
	ld = i->ldepth;
	b = buf;
	/* adjust beginning x to start of byte */
	dx = p->dr.min.x & ~p->pm;
	sx = p->sr.min.x - (p->dr.min.x&p->pm);
	/* compute starting coordinates in source */
	ix = drawsetxy(i->r.min.x, i->r.max.x, sx);
	iy = drawsetxy(i->r.min.y, i->r.max.y, y);
	a = byteaddr(i, Pt(ix, iy));
	ba = byteaddr(i, Pt(i->r.min.x, iy));
	/* convert destination to bit coordinates, with dx defined to be zero */
	maxdx = (p->dr.max.x - dx) << ld;
	dx = 0;
	/* convert source to bit coordinates, with *ba pointing to (0, 0)  */
	sx = (ix - (i->r.min.x & ~p->pm)) << ld;
	minsx = (i->r.min.x & p->pm) << ld;
	maxsx = (i->r.max.x - (i->r.min.x & ~p->pm)) << ld;
	/* run the loop moving one byte to destination each time */
	while(dx < maxdx){
		/* assemble 8 bits into v, source aligned */
		v = 0;
		ddx = 0;
		while(ddx < 8){
			sx7 = sx&7;
			sh = ddx - sx7;
			if(sh <= 0){
				v |= (*a++&(0xFF>>sx7)) << (-sh);
				np = 8-(sx&7);
			}else{
				v |= (*a&(0xFF>>sx7)) >> sh;
				np = 8-ddx;
			}
			if(sx+np > maxsx){
				ddx += maxsx-sx;
				v &= ~(0xFF>>ddx);
				sx = minsx;
				a = ba;
			}else{
				sx += np;
				ddx += np;
			}
		}
		*b++ = v;
		dx += 8;
	}
	return buf;
}

/*
 * Conversion routines:
 * For the most part, we ignore destination ldepth.
 * Instead we build source bytes suitably aligned and, at
 * the last minute, convert them into destination bytes.
 * To keep things simple, we have enough room on ends of
 * buffer to build full source bytes into full destination multibytes,
 * and the only adjustment needed is the buffer address we return.
 */

/*
 * converting, non-replicating, source ldepth==0, dest ldepth>0.
 * since loops do end masking, we don't need to here.
 */
static
uchar*
loadconv(Param *p, int y)
{
	uchar *a, *b, *conv, *c, *ret;
	int v, sh, ddx, dx, sx, j, ld, maxdx, np, sx7, nc;
	Memimage *i;

	i = p->i;
	ld = i->ldepth;
	b = p->buf[0];
	conv = conv0n[p->dldepth];
	nc = 1<<p->dldepth;
	/* adjust beginning x to start of byte */
	dx = p->dr.min.x & ~p->pm;
	sx = p->sr.min.x - (p->dr.min.x&p->pm);
	a = byteaddr(i, Pt(sx, y));
	/* adjust return pointer to byte containing first destination pixel */
	ret = &b[(p->dr.min.x-dx)>>(3-p->dldepth)];
	/* convert destination to bit coordinates, with dx defined to be zero */
	maxdx = (p->dr.max.x - dx) << ld;
	dx = 0;
	/* run the loop moving one byte to destination each time */
	while(dx < maxdx){
		/* assemble 8 bits into v, source aligned */
		v = 0;
		ddx = 0;
		while(ddx < 8){
			sx7 = sx&7;
			sh = ddx - sx7;
			if(sh <= 0){
				v |= (*a++&(0xFF>>sx7)) << (-sh);
				np = 8-(sx&7);
			}else{
				v |= (*a&(0xFF>>sx7)) >> sh;
				np = 8-ddx;
			}
			sx += np;
			ddx += np;
		}
		c = conv+(v<<p->dldepth);
		for(j=0; j<nc; j++)
			*b++ = *c++;
		dx += 8;
	}
	return ret;
}

/*
 * converting, replicating, source ldepth==0, dest ldepth>0.
 * since loops do end masking, we don't need to here.
 */
static
uchar*
loadconvrepl(Param *p, int y)
{
	uchar *a, *b, *ba, *conv, *c, *ret, *buf;
	int n, j, v, sh, ddx, dx, sx, ix, iy, ld, minsx, maxsx, maxdx, np, sx7, nc;
	Memimage *i;

	i = p->i;
	n = p->bufno++;
	buf = p->buf[n % p->nbuf];
	if(n>=p->nbuf && p->nbuf==Dy(i->r))	/* already done the job? */
		return buf;
	ld = i->ldepth;
	b = buf;
	conv = conv0n[p->dldepth];
	nc = 1<<p->dldepth;
	/* adjust beginning x to start of byte */
	dx = p->dr.min.x & ~p->pm;
	sx = p->sr.min.x - (p->dr.min.x&p->pm);
	/* compute starting coordinates in source */
	ix = drawsetxy(i->r.min.x, i->r.max.x, sx);
	iy = drawsetxy(i->r.min.y, i->r.max.y, y);
	a = byteaddr(i, Pt(ix, iy));
	ba = byteaddr(i, Pt(i->r.min.x, iy));
	/* adjust return pointer to byte containing first destination pixel */
	ret = &b[(p->dr.min.x-dx)>>(3-p->dldepth)];
	/* convert destination to bit coordinates, with dx defined to be zero */
	maxdx = (p->dr.max.x - dx) << ld;
	dx = 0;
	/* convert source to bit coordinates, with *ba pointing to (0, 0)  */
	sx = (ix - (i->r.min.x & ~p->pm)) << ld;
	minsx = (i->r.min.x & p->pm) << ld;
	maxsx = (i->r.max.x - (i->r.min.x & ~p->pm)) << ld;
	/* run the loop moving one byte to destination each time */
	while(dx < maxdx){
		/* assemble 8 bits into v, source aligned */
		v = 0;
		ddx = 0;
		while(ddx < 8){
			sx7 = sx&7;
			sh = ddx - sx7;
			if(sh <= 0){
				v |= (*a++&(0xFF>>sx7)) << (-sh);
				np = 8-(sx&7);
			}else{
				v |= (*a&(0xFF>>sx7)) >> sh;
				np = 8-ddx;
			}
			if(sx+np > maxsx){
				ddx += maxsx-sx;
				v &= ~(0xFF>>ddx);
				sx = minsx;
				a = ba;
			}else{
				sx += np;
				ddx += np;
			}
		}
		c = conv+(v<<p->dldepth);
		for(j=0; j<nc; j++)
			*b++ = *c++;
		dx += 8;
	}
	return ret;
}

static
void
buildtables(void)
{
	uchar *bp, *bp1, *bp2, *bp3;
	int ldepth, ppB, bpp, i, m, pm, v, mask;

	/* bitmasking tables */
	for(ldepth=0; ldepth<=2; ldepth++){
		ppB = 8>>ldepth;
		bpp = 1<<ldepth;
		pm = 0xFF^(0xFF>>bpp);
		bp = bitmasks[ldepth];
		for(v=0; v<256; v++){
			m = 0;
			mask = pm;
			for(i=0; i<ppB; i++, mask>>=bpp)
				if(v & mask)
					m |= mask;
			*bp++ = m;
		}
	}
	/* converting tables */
	bp1 = &conv01[0][0];
	bp2 = &conv02[0][0];
	bp3 = &conv03[0][0];
	for(v=0; v<256; v++){
		for(i=0; i<8; i++)
			if(v & (0x80>>i)){
				bp1[i>>2] |= 0xC0 >> (2*(i&3));
				bp2[i>>1] |= 0xF0 >> (4*(i&1));
				bp3[i>>0] |= 0xFF >> 0;
			}
		bp1 += 2;
		bp2 += 4;
		bp3 += 8;
	}
	tablesbuilt = 1;
}

/*
 * Because we know if dst->ldepth!=src->ldepth, then
 * src->ldepth is zero or src must be all zeros or all ones,
 * this code works regardless.
 */
int
membyteval(Memimage *src)
{
	int i, val, bpp;
	uchar uc;

	unloadmemimage(src, src->r, &uc, 1);
	bpp = 1<<src->ldepth;
	uc <<= (src->r.min.x&(7>>src->ldepth))<<src->ldepth;
	uc &= ~(0xFF>>bpp);
	/* pixel value is now in high part of byte. repeat throughout byte */
	val = uc;
	for(i=bpp; i<8; i<<=1)
		val |= val>>i;
	return val;
}

int
drawclip(Memimage *dst, Rectangle *r, Memimage *src, Point *p0, Memimage *mask, Point *p1, Rectangle *sr)
{
	Point rmin, delta;
	int splitcoords;
	Rectangle mr, omr;

	splitcoords = (p0->x!=p1->x) || (p0->y!=p1->y);
	/* clip to destination */
	rmin = r->min;
	if(!rectclip(r, dst->r) || !rectclip(r, dst->clipr))
		return 0;
	/* move mask point */
	p1->x += r->min.x-rmin.x;
	p1->y += r->min.y-rmin.y;
	/* map destination rectangle into source */
	p0->x += r->min.x-rmin.x;
	p0->y += r->min.y-rmin.y;
	sr->min = *p0;
	sr->max.x = p0->x+Dx(*r);
	sr->max.y = p0->y+Dy(*r);
	/* sr is r in source coordinates; clip to source */
	if(!src->repl && !rectclip(sr, src->r))
		return 0;
	if(!rectclip(sr, src->clipr))
		return 0;
	/* compute and clip rectangle in mask */
	if(splitcoords){
		p1->x += sr->min.x-p0->x;
		p1->y += sr->min.y-p0->y;
		mr.min = *p1;
		mr.max.x = p1->x+Dx(*sr);
		mr.max.y = p1->y+Dy(*sr);
		omr = mr;
		/* mr is now rectangle in mask; clip it */
		if(!mask->repl && !rectclip(&mr, mask->r))
			return 0;
		if(!rectclip(&mr, mask->clipr))
			return 0;
		/* reflect any clips back to source */
		sr->min.x += mr.min.x-omr.min.x;
		sr->min.y += mr.min.y-omr.min.y;
		sr->max.x += mr.max.x-omr.max.x;
		sr->max.y += mr.max.y-omr.max.y;
		*p1 = mr.min;
	}else{
		if(!mask->repl && !rectclip(sr, mask->r))
			return 0;
		if(!rectclip(sr, mask->clipr))
			return 0;
		*p1 = sr->min;
	}

	/* move source clipping back to destination */
	delta.x = r->min.x - p0->x;
	delta.y = r->min.y - p0->y;
	*p0 = sr->min;
	r->min.x = sr->min.x + delta.x;
	r->min.y = sr->min.y + delta.y;
	r->max.x = sr->max.x + delta.x;
	r->max.y = sr->max.y + delta.y;
	return 1;
}

XImage*
getXdata(Memimage *m, Rectangle r)
{
	XImage *xi;
	int ws, l, nw, offset, x, y, dx, dy;
	int src_x, src_y, dest_x, dest_y;
	uchar *p, *base, *tbase;
	Xmem *xm;
	Point delta;
	int bpl;
	static uchar *x24getbuf = (uchar*) 0;
	static ulong x24getsz = (ulong) 0;

	xm = m->X;
	ws = (8*sizeof(ulong))>>m->ldepth;
	l = wordsperline(m->r, m->ldepth);
	nw = l*Dy(m->r);
	m->data->base = malloc((nw)*sizeof(ulong));
	if(m->data->base == nil)
		return 0;
	m->data->data = m->data->base;
	/* NOTE: not &m->data->base[1], because p must point to allocated storage */
	p = (uchar*)m->data->base;

	delta = subpt(r.min, m->r.min);
	offset = m->r.min.x&(31>>m->ldepth);

	src_x = delta.x;
	src_y = delta.y;
	dest_x = delta.x + offset;
	dest_y = delta.y;

	xi = XCreateImage(xdisplay, xvis, m->ldepth==3 ? xscreendepth : 1<<m->ldepth, ZPixmap, 0, 
		(char*)p, Dx(m->r)+offset, Dy(m->r), 32, m->width*sizeof(ulong));
	if(xi == nil) {
    Error:
		free(p);
		m->data->base = nil;
		m->data->data = xm->wordp;
		return 0;
	}

	/*
	 * Set the parameters of the XImage so its memory looks exactly like a
	 * Memimage, so we can call _memdrawimage on the same data.  All frame
	 * buffers we've seen, and Inferno's graphics code, require big-endian
	 * bits within bytes.  Because the accesses are all bytewise, we want
	 * big-endian byte order within a word regardless of the CPU's byte order.
	 */
	xi->bitmap_unit = 32;
	xi->byte_order = MSBFirst;
	xi->bitmap_bit_order = MSBFirst;
	xi->bitmap_pad = 32;

	dx = r.max.x-r.min.x;
	dy = r.max.y-r.min.y;
	if(Dy(r)==0 && Dx(r)==0)
		return xi;

	if(xtblbit && xscreendepth==24 && m->ldepth==3) {

		if(x24getsz < (dx*sizeof(ulong)*dy)) {
			x24getbuf = realloc(x24getbuf, dx*sizeof(ulong)*dy);
			if(!x24getbuf) 
				goto Error;
			x24getsz = dx*sizeof(ulong)*dy;
		}

		tbase = xi->data;
		bpl = xi->bytes_per_line; 
		xi->data = x24getbuf;
		xi->bytes_per_line = dx*sizeof(ulong);
		dest_x = 0;
		dest_y = 0;
	}

	XGetSubImage(xdisplay, xm->pmid, src_x, src_y,
		dx, dy, AllPlanes, ZPixmap, xi, dest_x, dest_y);

	if(xtblbit && m->ldepth==3){
		/* map pixel values from x11 color map to rgbv */
		base = byteaddr(m, r.min);

		if(xscreendepth == 24) {
		ulong c;
		int last, index;

			/* prepare for run length duplicates */
			last = 0;
			index = rgb2cmap(last, last, last);
			for(y=0; y<dy; y++){
				p = base; 
				for(x=0; x<dx; x++){
					c = XGetPixel(xi, x, y);
					if(c==last)	/* reuse last value */
						*p++ = index;
					else {
						/* the rgb2cmap function is alot faster than
						* any hash function/table lookup I could think of.
						* Anybody who can come up with one faster than the
						* straight function should do so here.
						*/
						if(x24bitswap)
							index = rgb2cmap(c&0xff,(c>>8)&0xff,(c>>16)&0xff);
						else
							index = rgb2cmap((c>>16)&0xff,(c>>8)&0xff,c&0xff);
						/* index = rgb2cmap(r,g,b);                 */
						/*                  | | |                   */
						/*                  | | + b = c&0xff;       */
						/*                  | +-- g = (c>>8)&0xff;  */
						/*                  +---- r = (c>>16)&0xff; */
						last = c;
						*p++ = index;
					}
				}
				base += m->width*sizeof(ulong);
			}
			xi->data = tbase;
			xi->bytes_per_line = bpl;
		}
		else {
			for(y=0; y<dy; y++){
				p = base; 
				for(x=0; x<dx; x++){
					*p = x11toinferno[*p];
					p++;
				}
				base += m->width*sizeof(ulong);
			}
		}
	}
	return xi;
}

void
putXdata(Memimage *m, XImage *xi, Rectangle r)
{
	GC g;
	int offset;
	Xmem *xm;
	int x, y, dx, dy;
	int src_x, src_y, dest_x, dest_y;
	uchar *p, *base, *tbase;
	int ymax, xmax, yoff;
	int bpl;
	int x24set = 0;
	static uchar *x24putbuf = (uchar*) 0;
	static ulong x24putsz = (ulong) 0;

	xm = m->X;
	g = (xi->depth == 1) ? xgccopy0 : xgccopy;
	offset = m->r.min.x&(31>>m->ldepth);
	dx = r.max.x-r.min.x;
	dy = r.max.y-r.min.y;

	src_x = r.min.x-m->r.min.x+offset;
	src_y = r.min.y-m->r.min.y;
	dest_x = r.min.x-m->r.min.x;
	dest_y = r.min.y-m->r.min.y;

	if(xtblbit && m->ldepth==3){
		/* map pixel values back to x11 color map */
		base = byteaddr(m, r.min);

		if(xscreendepth == 24) {

			/* resize buf if necessary */
			if(x24putsz < (dx*sizeof(ulong)*dy)) {
				x24putbuf = realloc(x24putbuf, dx*sizeof(ulong)*dy);
				if(!x24putbuf) 
					return ;
				x24putsz = dx*sizeof(ulong)*dy;
			}

			src_x = 0;
			src_y = 0;
			x24set = 1;

			tbase = xi->data;
			bpl = xi->bytes_per_line;

			xi->data = x24putbuf;
			xi->bytes_per_line = dx*sizeof(ulong);
			for(y=0; y<dy; y++){
				p = base; 
				for(x=0; x<dx; x++){
					if(!XPutPixel(xi, x, y, infernotox11[*p++])) {
						print("putXdata: XPutPixel failed\n");
						xi->data = tbase;
						xi->bytes_per_line = bpl;
						return;
					}
				}
				base += m->width*sizeof(ulong);
			}
		}
		else {
			for(y=0; y<dy; y++){
				p = base; 
				for(x=0; x<dx; x++){
					*p = infernotox11[*p];
					p++;
				}
				base += m->width*sizeof(ulong);
			}
		} 
	}

	XPutImage(xdisplay, xm->pmid, g, xi, src_x, src_y, dest_x, dest_y, dx, dy);

	if(x24set) {
		xi->data = tbase;
		xi->bytes_per_line = bpl;
	}
}

int
xmembyteval(Memimage *src)
{
	Xmem *xm;
	XImage *xsrc;
	int word;

	xm = src->X;
	if((xm->flag&XXonepixel) == 0){
		xsrc = getXdata(src, src->r);
		if(xsrc==0)
			return 0;
		word = membyteval(src);
		xm->word = word | (word<<8) | (word<<16) | (word<<24);
		src->data->data = &xm->word;
		src->data->base = nil;
		xm->flag |= XXonepixel;
		XDestroyImage(xsrc);
	}

	return xm->word&0xFF;
}

static
int
mergebox(Rectangle *r, Rectangle *r1, Rectangle *r2)
{
	int a1, a2;

	a1 = Dx(*r1)*Dy(*r1);
	a2 = Dx(*r2)*Dy(*r2);

	*r = *r1;
	if(r->min.x > r2->min.x)
		r->min.x = r2->min.x;
	if(r->min.y > r2->min.y)
		r->min.y = r2->min.y;
	if(r->max.x < r2->max.x)
		r->max.x = r2->max.x;
	if(r->max.y < r2->max.y)
		r->max.y = r2->max.y;
	if(a1+a2 < Dx(*r)*Dy(*r))
		return 0;
	return 1;
}

int
getpmid0(Memimage *i)
{
	XImage *xi, *xi0;
	Xmem *xm, *xm0;
	Memimage *i0;
	int m;
	int xmax, ymax, y_offset, y0_offset;
	int x, y;
	uchar *p, *p0, *pbase, *p0base;

	xm = i->X;
	if(xm->pmid0 != PMundef)
		return 1;
	if(i->ldepth != 3)
		return 0;
	i0 = allocmemimage(i->r, 0);
	if(i0 == nil)
		return 0;
	memfillcolor(i0, 0);
	xm0 = i0->X;

	xi = getXdata(i, i->r);
	if(xi==0)
		return 0;
	xi0 = getXdata(i0, i0->r);
	if(xi0==0)
		return 0;

	pbase = (uchar*)(i->data->data + i->zero + i->r.min.y*i->width);
	p0base = (uchar*)(i0->data->data + i0->zero + i->r.min.y*i0->width);
	xmax = i->r.max.x;
	ymax = i->r.max.y;
	y_offset = i->width*sizeof(ulong);
	y0_offset = i0->width*sizeof(ulong);

	for(x=i->r.min.x; x<xmax; x++){
		p = pbase + (x>>(3-i->ldepth));
		p0 = p0base + (x>>(3-i0->ldepth)); 
		/* p0 = byteaddr(i0, Pt(x, i->r.min.y)); */
		m = 0x80 >> (x&7);
		for(y=i->r.min.y; y<ymax; y++){
			if(*p)
				*p0 |= m;
			p += y_offset;
			p0 += y0_offset;
		}
	}
	if(xi0 != nil){
		putXdata(i0, xi0, i0->r);
		XDestroyImage(xi0);
	}
	if(xi != nil){
		XDestroyImage(xi);
		i->data->data = xm->wordp;
		i->data->base = nil;
	}
	xm->pmid0 = xm0->pmid0;
	xm0->pmid = PMundef;
	xm0->pmid0 = PMundef;
	freememimage(i0);
	return 1;
}

void
memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point p0, Memimage *mask, Point p1)
{
	GC g;
	int x, state, srcval;
	Rectangle sr, ssr, mmr, tr;
	Point deltad, deltas, deltam, p;
	XImage *xdst, *xsrc, *xmask, *xtmp;
	Xmem *dstxm, *srcxm, *maskxm;

	if(drawclip(dst, &r, src, &p0, mask, &p1, &sr) == 0)
		return;

	/* p0 now == sr.min */
	/* some checking of ldepths; more will follow */
	x = dst->ldepth | src->ldepth | mask->ldepth;
	if(x<0 || x>3)
		return;

	state = 0;
	if(mask->repl){
		state |= Replmask;
		if(Dx(mask->r)==1 && Dy(mask->r)==1){
			if(xmembyteval(mask) == 0)
				return;
			state |= Simplemask;
		}
	}else if(mask->ldepth!=dst->ldepth && mask->ldepth!=0)
		return;

	srcval = 0;
	if(src->repl){
		state |= Replsrc;
		if(Dx(src->r)==1 && Dy(src->r)==1){
			srcval = xmembyteval(src);
			state |= Simplesrc;
			if(src->ldepth>dst->ldepth && (srcval!=0x00 && srcval!=0xFF))
				return;
		}
	}else if(src->ldepth!=dst->ldepth && src->ldepth!=0)
		return;

	dstxm = dst->X;
	srcxm = src->X;
	maskxm = mask->X;
	deltad = subpt(r.min, dst->r.min);
	deltas = subpt(p0, src->r.min);
	deltam = subpt(p1, mask->r.min);

	switch(state){
	case Simplemask|Replmask|Simplesrc|Replsrc:
		if(dst->ldepth == 0){
			g = xgcfill0;
			srcval = srcval!=0;
			if(xgcfillcolor0 != srcval){
				XSetForeground(xdisplay, g, srcval);
				xgcfillcolor0 = srcval;
			}
		}else{ 
			g = xgcfill;
			if(xtblbit)
				srcval = infernotox11[srcval];
			if(xgcfillcolor != srcval){
				XSetForeground(xdisplay, g, srcval);
				xgcfillcolor = srcval;
			}
		}

		XFillRectangle(xdisplay, dstxm->pmid, g, deltad.x,
			deltad.y, Dx(r), Dy(r));
		goto Return;
	case Simplemask|Replmask:
		if(src->ldepth != dst->ldepth)
			break;
		if(dst->ldepth == 0)
			g = xgccopy0;
		else  
			g = xgccopy;
		XCopyArea(xdisplay, srcxm->pmid, dstxm->pmid, g,
			deltas.x, deltas.y,
			Dx(r), Dy(r), deltad.x, deltad.y);
		goto Return;
	case Simplesrc|Replsrc:
		if(getpmid0(mask) == 0)
			break;
		p = subpt(deltad, deltam);
		if(dst->ldepth == 0){
			g = xgcsimplesrc0;
			srcval = srcval!=0;
			XSetTSOrigin(xdisplay, g, p.x, p.y);
			if(xgcsimplecolor0 != srcval){
				XSetForeground(xdisplay, g, srcval);
				xgcsimplecolor0 = srcval;
			}
			if(xgcsimplepm0 != maskxm->pmid0){
				XSetStipple(xdisplay, g, maskxm->pmid0);
				xgcsimplepm0 = maskxm->pmid0;
			}
		}else{
			g = xgcsimplesrc;
			XSetTSOrigin(xdisplay, g, p.x, p.y);
			if(xtblbit)
				srcval = infernotox11[srcval];
			if(xgcsimplecolor != srcval){
				XSetForeground(xdisplay, g, srcval);
				xgcsimplecolor = srcval;
			}
			if(xgcsimplepm != maskxm->pmid0){
				XSetStipple(xdisplay, g, maskxm->pmid0);
				xgcsimplepm = maskxm->pmid0;
			}
		}
		XFillRectangle(xdisplay, dstxm->pmid, g, deltad.x,
			deltad.y, Dx(r), Dy(r));
		goto Return;
	case 0:
		if(src->ldepth != dst->ldepth)
			break;
		if(getpmid0(mask) == 0)
			break;
		p = subpt(deltad, deltam);
		if(dst->ldepth == 0){
			g = xgczero0;
			XSetClipOrigin(xdisplay, g, p.x, p.y);
			if(xgczeropm0!=maskxm->pmid0){
				XSetClipMask(xdisplay, g, maskxm->pmid0);
				xgczeropm0 = maskxm->pmid0;
			}
		}else{
			g = xgczero;
			XSetClipOrigin(xdisplay, g, p.x, p.y);
			if(xgczeropm!=maskxm->pmid0){
				XSetClipMask(xdisplay, g, maskxm->pmid0);
				xgczeropm = maskxm->pmid0;
			}
		}

		XCopyArea(xdisplay, srcxm->pmid, dstxm->pmid, g,
			deltas.x, deltas.y,
			Dx(r), Dy(r), deltad.x, deltad.y);
		goto Return;
	case 1000 + Replsrc|Simplemask|Replmask: /* turned off because i don't understand tile coordinate system */
		if(src->ldepth!=dst->ldepth)
			break;
		if(dst->ldepth == 0){
			g = xgcreplsrc0;
			if(xgcreplsrctile0 != srcxm->pmid0){
				XSetTile(xdisplay, g, srcxm->pmid);
				xgcreplsrctile0 = srcxm->pmid0;
			}
		}else{
			g = xgcreplsrc;
			if(xgcreplsrctile != srcxm->pmid0){
				XSetTile(xdisplay, g, srcxm->pmid);
				xgcreplsrctile = srcxm->pmid0;
			}
		}
		p = subpt(deltad, deltas);
		XSetTSOrigin(xdisplay, g, p.x, p.y);
		XFillRectangle(xdisplay, dstxm->pmid, g, deltad.x,
			deltad.y, Dx(r), Dy(r));
		goto Return;
	}

if(0)print("hard draw state %d\n", state);
	/*
	 * Destination may be same Image as source or mask, so for efficient
	 * operation we try not to load pixels multiple times.
	 */
	if(mask->repl)
		mmr = mask->r;
	else
		mmr = Rect(p1.x, p1.y, p1.x+Dx(r), p1.y+Dy(r));
	if(src->repl)
		ssr = src->r;
	else
		ssr = sr;
	if(dst!=src && dst!=mask){	/* commonest case first */
		xdst = getXdata(dst, r);
		if(src==mask && src->repl==0 && mergebox(&tr, &ssr, &mmr)){
			xsrc = getXdata(src, tr);
			xmask = nil;
		}else{
			xsrc = getXdata(src, ssr);
			if(xsrc==nil)
				goto Return;
			if(src==mask){
				mask = clonememimage(mask);
				if(mask==nil)
					goto Return;
				maskxm = mask->X;
			}
			xmask = getXdata(mask, mmr);
			if(xmask==nil)
				goto Return;
		}
		if(xsrc==nil)
			goto Return;
	}else
	if(dst==src){
			if(src->repl==0 && mergebox(&tr, &r, &ssr)){
				xdst = getXdata(dst, tr);
				xsrc = nil;
			}else{
				xdst = getXdata(dst, r);
				src = clonememimage(src);
				if(src==nil)
					goto Return;
				srcxm = src->X;
				xsrc = getXdata(src, ssr);
				if(xsrc==nil)
					goto Return;
			}
			if(mask==dst){
				mask = clonememimage(mask);
				if(mask==nil)
					goto Return;
				maskxm = mask->X;
			}
			xmask = getXdata(mask, mmr);
			if(xmask==nil)
				goto Return;
	}else
	{	/* dst == mask != src*/
			if(mask->repl==0 && mergebox(&tr, &r, &mmr)){
				xdst = getXdata(dst, tr);
				xmask = nil;
			}else{
				xdst = getXdata(dst, r);
				mask = clonememimage(mask);
				if(mask==nil)
					goto Return;
				maskxm = mask->X;
				xmask = getXdata(mask, mmr);
				if(xmask==nil)
					goto Return;
			}
			xsrc = getXdata(src, ssr);
			if(xsrc==nil)
				goto Return;
	}
	if(xdst==nil)
		goto Return;


	/* do op */
	_memimagedraw(dst, r, src, p0, mask, p1, sr);
	/* unload to server */
	putXdata(dst, xdst, r);
	/* clean up */
	XDestroyImage(xdst);
	dst->data->base = nil;
	dst->data->data = dstxm->wordp;
	if(xsrc){
		XDestroyImage(xsrc);
		if(srcxm->flag&XXcloned)
			freememimage(src);
		else{
			src->data->base = nil;
			src->data->data = srcxm->wordp;
		}
	}
	if(xmask){
		XDestroyImage(xmask);
		if(maskxm->flag&XXcloned)
			freememimage(mask);
		else{
			mask->data->base = nil;
			mask->data->data = maskxm->wordp;
		}
	}

    Return:
	xdirtied(dst);
}

static void
_memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point p0, Memimage *mask, Point p1, Rectangle sr)
{
	uchar *d, *s, *m, *d1, *m1, *s1, *bitmask;
	int dw, x0, x1, x2, x3, pm, lm, rm;
	int i, n, x, y, state, srcval, y0, y1, dy, dd, nbytes;
	Param sparam, mparam;
	uchar *(*srcfn)(Param*, int), *(*maskfn)(Param*, int);

	/* compute tables */
	if(tablesbuilt == 0)
		buildtables();

	state = 0;
	if(mask->repl){
		state |= Replmask;
		if(Dx(mask->r)==1 && Dy(mask->r)==1){
			if(membyteval(mask) == 0)
				return;
			state |= Simplemask;
		}
	}else if(mask->ldepth!=dst->ldepth && mask->ldepth!=0)
		return;

	srcval = 0;
	if(src->repl){
		state |= Replsrc;
		if(Dx(src->r)==1 && Dy(src->r)==1){
			srcval = membyteval(src);
			state |= Simplesrc;
			if(src->ldepth>dst->ldepth && (srcval!=0x00 && srcval!=0xFF))
				return;
		}
	}else if(src->ldepth!=dst->ldepth && src->ldepth!=0)
		return;

	nbytes = bytesperline(r, dst->ldepth);

	pm = ~(7>>dst->ldepth);
	sparam.i = src;
	sparam.y0addr = byteaddr(src, Pt(p0.x, 0));
	sparam.bwidth = src->width*sizeof(ulong);
	sparam.needbuf = 1;
	sparam.dldepth = dst->ldepth;
	sparam.nbytes = nbytes;
	sparam.pm = 7>>src->ldepth;
	sparam.dr = r;
	sparam.sr = sr;

	mparam.i = mask;
	mparam.y0addr = byteaddr(mask, Pt(p1.x, 0));
	mparam.bwidth = mask->width*sizeof(ulong);
	mparam.needbuf = 1;
	mparam.dldepth = dst->ldepth;
	mparam.nbytes = nbytes;
	mparam.pm = 7>>mask->ldepth;
	mparam.dr = r;
	mparam.sr = rectaddpt(sr, subpt(p1, p0));

	srcfn = loadfn[dst->ldepth!=src->ldepth][src->repl!=0][src->ldepth!=3];
	maskfn = loadfn[dst->ldepth!=mask->ldepth][mask->repl!=0][mask->ldepth!=3];

	/* initialize buffers */
	x = nbytes+Nslop;
	n = setbuf(x, &sparam, state&Simplesrc, state&Replsrc);
	n += setbuf(x, &mparam, state&Simplemask, state&Replmask);
	if(n > drawbufsize){
		d = realloc(drawbuf, n);
		if(d == 0)
			return;
		drawbuf = d;
		drawbufsize = n;
	}
	d = drawbuf;
	for(i=0; i<sparam.nbuf; i++){
		sparam.buf[i] = d;
		d += x;
	}
	for(i=0; i<mparam.nbuf; i++){
		mparam.buf[i] = d;
		d += x;
	}
	
	/* useful parameters */
	x0 = r.min.x;
	x3 = r.max.x;
	x1 = (x0+(7>>dst->ldepth)) & pm;
	x2 = x3 & pm;
	n = (x2-x1)>>(3-dst->ldepth);
	lm = (1<<((x1-x0)<<dst->ldepth))-1;
	rm = 0xFF ^ (0xFF>>((x3-x2)<<dst->ldepth));

	bitmask = bitmasks[dst->ldepth];

	dw = dst->width*sizeof(ulong);
	switch(state){
	case Simplemask|Replmask|Simplesrc|Replsrc:
		d = byteaddr(dst, r.min);
		if(dst->ldepth == 3){
			for(y=r.min.y; y<r.max.y; y++, d+=dw)
				memset(d, srcval, n);
			return;
		}
		if(((x3-1)&pm) == (x0&pm)){	/* all one byte */
			if(lm == 0)
				lm = 0xFF;
			if(rm != 0)
				lm &= rm;
			for(y=r.min.y; y<r.max.y; y++, d+=dw)
				*d ^= (*d^srcval) & lm;
			return;
		}
		for(y=r.min.y; y<r.max.y; y++, d+=dw){
			d1 = d;
			/* left */
			if(lm){
				*d1 ^= (*d1^srcval) & lm;
				d1++;
			}
			/* middle strip, as bytes */
			if(n > 0)
				memset(d1, srcval, n);
			/* right */
			if(rm){
				d1 += n;
				*d1 ^= (*d1^srcval) & rm;
			}
		}
		return;

	case Simplemask|Replmask:
	case Simplemask|Replmask|Replsrc:
		d = byteaddr(dst, r.min);
		s = byteaddr(src, p0);
		if(src->data!=dst->data && src->ldepth==3)
			sparam.needbuf = 0;
		if(sparam.needbuf==0 || s > d){
			y0 = sr.min.y;
			y1 = sr.max.y;
			dy = 1;
			dd = dst->width*sizeof(ulong);
		}else{
			y0 = sr.max.y-1;
			y1 = sr.min.y-1;
			dy = -1;
			dd = -dst->width*sizeof(ulong);
			d -= dd*(Dy(r)-1);
		}
		if(((x3-1)&pm) == (x0&pm)){	/* all one byte */
			if(lm == 0)
				lm = 0xFF;
			if(rm != 0)
				lm &= rm;
			for(y=y0; y!=y1; y+=dy, d+=dd){
				s1 = (*srcfn)(&sparam, y);
				*d ^= (*d^*s1) & lm;
			}
			return;
		}
		for(y=y0; y!=y1; y+=dy, d+=dd){
			s1 = (*srcfn)(&sparam, y);
			d1 = d;
			/* left */
			if(lm){
				*d1 ^= (*d1^*s1) & lm;
				d1++;
				s1++;
			}
			/* middle strip, as bytes */
			if(n > 0)
				memmove(d1, s1, n);
			/* right */
			if(rm){
				s1 += n;
				d1 += n;
				*d1 ^= (*d1^*s1) & rm;
			}
		}
		return;

	case Simplesrc|Replsrc:
	case Simplesrc|Replsrc|Replmask:
		d = byteaddr(dst, r.min);
		m = byteaddr(mask, p1);
		mparam.needbuf = (mask->data == dst->data);
		if(mparam.needbuf==0 || m>d){
			y0 = p1.y;
			y1 = p1.y+Dy(r);
			dy = 1;
			dd = dst->width*sizeof(ulong);
		}else{
			y0 = p1.y+Dy(r)-1;
			y1 = p1.y-1;
			dy = -1;
			dd = -dst->width*sizeof(ulong);
			d -= dd*(Dy(r)-1);
		}
		if(dst->ldepth == 3){
			for(y=y0; y!=y1; y+=dy, d+=dd){
				m1 = (*maskfn)(&mparam, y);
				d1 = d;
				for(x=r.min.x; x<r.max.x; x++, d1++, m1++)
					if(*m1)
						*d1 = srcval;
			}
			return;
		}
		if(((x3-1)&pm) == (x0&pm)){	/* all one byte */
			if(lm == 0)
				lm = 0xFF;
			if(rm != 0)
				lm &= rm;
			for(y=y0; y!=y1; y+=dy, d+=dd){
				m1 = (*maskfn)(&mparam, y);
				*d ^= (*d^srcval) & (bitmask[*m1] & lm);
			}
			return;
		}
		for(y=y0; y!=y1; y+=dy, d+=dd){
			m1 = (*maskfn)(&mparam, y);
			d1 = d;
			/* left */
			if(lm){
				*d1 ^= (*d1^srcval) & (bitmask[*m1] & lm);
				m1++;
				d1++;
			}
			for(i=0; i<n; i++, d1++, m1++)
				if(*m1)
					*d1 ^= (*d1^srcval) & bitmask[*m1];
			/* right */
			*d1 ^= (*d1^srcval) & (bitmask[*m1] & rm);
		}
		return;

	case 0:
	case Replsrc:
	case Replmask:
	case Replsrc|Replmask:
		d = byteaddr(dst, r.min);
		s = byteaddr(src, p0);
		/*
		 * BUG: in pathological cases, just using scan-line buffers
		 * and reordering isn't good enough, but such cases are
		 * a) stupid (they involve writing a mask while you're using it);
		 * b) legitimately undefined in meaning and so
		 * c) not worth the trouble to get 'right'.
		 */
		mparam.needbuf = (mask->data==dst->data);
		sparam.needbuf = (src->data == dst->data);
		if(s > d){
			y0 = sr.min.y;
			y1 = sr.max.y;
			dy = 1;
			dd = dst->width*sizeof(ulong);
		}else{
			y0 = sr.max.y-1;
			y1 = sr.min.y-1;
			dy = -1;
			dd = -dst->width*sizeof(ulong);
			d -= dd*(Dy(r)-1);
		}
		if(dst->ldepth == 3){
			for(y=y0; y!=y1; y+=dy, d+=dd){
				m1 = (*maskfn)(&mparam, y+(p1.y-p0.y));
				s1 = (*srcfn)(&sparam, y);
				d1 = d;
				for(x=r.min.x; x<r.max.x; x++, d1++, m1++, s1++)
					if(*m1)
						*d1 = *s1;
			}
			return;
		}
		if(((x3-1)&pm) == (x0&pm)){	/* all one byte */
			if(lm == 0)
				lm = 0xFF;
			if(rm != 0)
				lm &= rm;
			for(y=y0; y!=y1; y+=dy, d+=dd){
				m1 = (*maskfn)(&mparam, y+(p1.y-p0.y));
				s1 = (*srcfn)(&sparam, y);
				*d ^= (*d^*s1) & (bitmask[*m1] & lm);
			}
			return;
		}
		for(y=y0; y!=y1; y+=dy, d+=dd){
			m1 = (*maskfn)(&mparam, y+(p1.y-p0.y));
			s1 = (*srcfn)(&sparam, y);
			d1 = d;
			
			/* left */
			if(lm){
				*d1 ^= (*d1^*s1) & (bitmask[*m1] & lm);
				d1++;
				s1++;
				m1++;
			}
			for(i=0; i<n; i++, d1++, s1++, m1++)
				if(*m1)
					*d1 ^= (*d1^*s1) & bitmask[*m1];
			/* right */
			*d1 ^= (*d1^*s1) & (bitmask[*m1] & rm);
		}
		return;
	}
}
