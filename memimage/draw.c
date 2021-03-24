#include <lib9.h>
#include <image.h>
#include <memimage.h>

/*
 * This version handles all operations including conversions between
 * ldepths.  Limited to ldepths 0, 1, 2, 3, and treats masks as booleans
 * (0 or not-0); alpha blending would be nice one day.
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
	int		ismask;	/* 1 if mask, 0 if source */
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
static uchar*	loadconvdn(Param*, int);
static uchar*	loadconvdnrepl(Param*, int);
static uchar*	loadconvup(Param*, int);
static uchar*	loadconvuprepl(Param*, int);

static uchar*	(*loadfn[3][2][2])(Param*, int) = {
	loadconvdn,		/* [0][0][0]: convert down, no repl, ldepth=3 */
	loadconvdn,		/* [0][0][1]: convert down, no repl, ldepth!=3 */
	loadconvdnrepl,	/* [0][1][0]: convert down, repl, ldepth=3 */
	loadconvdnrepl,	/* [0][1][1]: convert down, repl, ldepth!=3 */
	load3,			/* [1][0][0]: no convert, no repl, ldepth=3 */
	loadn,			/* [1][0][1]: no convert, no repl, ldepth!=3 */
	load3repl,			/* [1][1][0]: no convert, repl, ldepth=3 */
	loadnrepl,			/* [1][1][1]: no convert, repl, ldepth!=3 */
	loadconvup,		/* [2][0][0]: convert up, no repl, ldepth=3 */
	loadconvup,		/* [2][0][1]: convert up, no repl, ldepth!=3 */
	loadconvuprepl,	/* [2][1][0]: convert up, repl, ldepth=3 */
	loadconvuprepl,	/* [2][1][1]: convert up, repl, ldepth!=3 */
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
static uchar conv12[256][2];
static uchar conv13[256][4];
static uchar conv23[256][2];
static uchar *convmn[4][4] = {
	0, &conv01[0][0], &conv02[0][0], &conv03[0][0],
	0, 0,  &conv12[0][0], &conv13[0][0],
	0, 0,  0, &conv23[0][0],
};

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
 * Up-conversion routines:
 * For the most part, we ignore destination ldepth.
 * Instead we build source bytes suitably aligned and, at
 * the last minute, convert them into destination bytes.
 * To keep things simple, we have enough room on ends of
 * buffer to build full source bytes into full destination multibytes,
 * and the only adjustment needed is the buffer address we return.
 */

/*
 * converting, non-replicating, source ldepth<ldepth.
 * since loops do end masking, we don't need to here.
 */
static
uchar*
loadconvup(Param *p, int y)
{
	uchar *a, *b, *conv, *c, *ret;
	int v, sh, ddx, dx, sx, j, ld, maxdx, np, sx7, nc;
	Memimage *i;

	i = p->i;
	ld = i->ldepth;
	b = p->buf[0];
	conv = convmn[ld][p->dldepth];
	nc = 1<<(p->dldepth-ld);
	/* adjust beginning x to start of byte */
	dx = p->dr.min.x & ~p->pm;
	sx = p->sr.min.x - (p->dr.min.x&p->pm);
	a = byteaddr(i, Pt(sx, y));
	/* adjust return pointer to byte containing first destination pixel */
	ret = &b[(p->dr.min.x-dx)>>(3-p->dldepth)];
	/* convert destination to bit coordinates, with dx defined to be zero */
	maxdx = (p->dr.max.x - dx) << ld;
	dx = 0;
	sx <<= ld;
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
		c = conv+(v<<(p->dldepth-ld));
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
loadconvuprepl(Param *p, int y)
{
	uchar *a, *b, *ba, *conv, *c, *ret, *buf;
	int n, j, v, sh, ddx, dx, sx, ix, iy, ld, minsx, maxsx, maxdx, np, sx7, nc;
	Memimage *i;

	i = p->i;
	n = p->bufno++;
	buf = p->buf[n % p->nbuf];
	ld = i->ldepth;
	b = buf;
	nc = 1<<(p->dldepth-ld);
	/* adjust beginning x to start of byte */
	dx = p->dr.min.x & ~p->pm;
	sx = p->sr.min.x - (p->dr.min.x&p->pm);
	/* adjust return pointer to byte containing first destination pixel */
	ret = &b[(p->dr.min.x-dx)>>(3-p->dldepth)];
	if(n>=p->nbuf && p->nbuf==Dy(i->r))	/* already done the job? */
		return ret;
	/* compute starting coordinates in source */
	ix = drawsetxy(i->r.min.x, i->r.max.x, sx);
	iy = drawsetxy(i->r.min.y, i->r.max.y, y);
	/* convert destination to bit coordinates, with dx defined to be zero */
	maxdx = (p->dr.max.x - dx) << ld;
	dx = 0;
	/* convert source to bit coordinates, with *ba pointing to (0, 0)  */
	sx = (ix - (i->r.min.x & ~p->pm)) << ld;
	minsx = (i->r.min.x & p->pm) << ld;
	maxsx = (i->r.max.x - (i->r.min.x & ~p->pm)) << ld;
	conv = convmn[ld][p->dldepth];
	a = byteaddr(i, Pt(ix, iy));
	ba = byteaddr(i, Pt(i->r.min.x, iy));
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
		c = conv+(v<<(p->dldepth-ld));
		for(j=0; j<nc; j++)
			*b++ = *c++;
		dx += 8;
	}
	return ret;
}

/*
 * Down-conversion routines:
 * For the most part, we ignore destination ldepth.
 * Instead we build source bytes suitably aligned and, at
 * the last minute, convert them into destination bytes.
 * To keep things simple, we have enough room on ends of
 * buffer to build full source bytes into full destination multibytes,
 * and the only adjustment needed is the buffer address we return.
 */

/*
 * converting from high to low ldepth, non-replicating, general ldepths.
 * since loops do end masking, we don't need to here.
 */
static
uchar*
loadconvdn(Param *p, int y)
{
	uchar *a, *b, *end;
	int v, sh, ddx, dx, sx, ld, maxdx, sx7, spm, dpm, nbit, sbpp, dbpp;
	Memimage *i;

	i = p->i;
	ld = i->ldepth;
	b = p->buf[0];
	a = byteaddr(i, Pt(p->sr.min.x, y));
	/* sx is in bit coordinates */
	sx = (p->sr.min.x&p->pm) << ld;
	/* convert destination to bit coordinates, with dx set to starting bit in first byte */
	dx = (p->dr.min.x&(7>>p->dldepth)) << p->dldepth;
	maxdx = ((p->dr.max.x-p->dr.min.x) << p->dldepth) + dx;
	/* run the loop assembling one byte of destination data */
	ddx = dx;
	nbit = 8-ddx;
	spm = 0xFF&~(0xFF>>(1<<ld));
	dpm = 0xFF&~(0xFF>>(1<<p->dldepth));
	sbpp = 1<<ld;
	dbpp = 1<<p->dldepth;
	end = byteaddr(i, Pt(i->r.max.x-1, y));
	while(dx < maxdx){
		/* assemble 8 bits into v, dest aligned, one source pixel at a time */
		v = 0;
		/*
		 * Straight conversion will throw away low bits, so if it's a mask,
		 * build boolean value by testing all bits.  This code is not as
		 * optimized as most other cases, because it's relatively rare.
		 */
		while(ddx < 8){
			sx7 = sx&7;
			sh = ddx - sx7;
			if(p->ismask){
				if(*a & ((spm>>sx7)))
					v |= dpm>>ddx;
			}else{
				if(sh <= 0)
					v |= (*a&((dpm>>sx7))) << (-sh);
				else
					v |= (*a&((dpm>>sx7))) >> sh;
			}
			sx += sbpp;
			if((sx&7) == 0){
				a++;
				if(a > end)	/* annoying but necessary */
					break;
			}
			ddx += dbpp;
		}
		*b++ = v;
		dx += nbit;
		ddx = 0;
		nbit = 8;
	}
	return p->buf[0];
}


static
uchar*
loadconvdnrepl(Param *p, int y)
{
	uchar *a, *b, *ba, *buf;
	int n, v, sh, ddx, dx, sx, ix, iy, ld, minsx, maxsx, maxdx, sx7, dbpp, sbpp, nbit, spm, dpm;
	Memimage *i;

	i = p->i;
	n = p->bufno++;
	buf = p->buf[n % p->nbuf];
	ld = i->ldepth;
	b = buf;
	if(n>=p->nbuf && p->nbuf==Dy(i->r))	/* already done the job? */
		return buf;
	/* compute starting coordinates in source */
	ix = drawsetxy(i->r.min.x, i->r.max.x, p->sr.min.x);
	iy = drawsetxy(i->r.min.y, i->r.max.y, y);
	/* convert destination to bit coordinates, with dx set to starting bit in first byte */
	dx = (p->dr.min.x&(7>>p->dldepth)) << p->dldepth;
	maxdx = ((p->dr.max.x-p->dr.min.x) << p->dldepth) + dx;
	/* convert source to bit coordinates, with *ba pointing to origin of tile on this line  */
	sx = ix << ld;
	minsx = i->r.min.x << ld;
	maxsx = i->r.max.x << ld;
	a = byteaddr(i, Pt(ix, iy));
	ba = byteaddr(i, Pt(i->r.min.x, iy));
	/* run the loop assembling one byte of destination data */
	ddx = dx;
	nbit = 8-ddx;
	spm = 0xFF&~(0xFF>>(1<<ld));
	dpm = 0xFF&~(0xFF>>(1<<p->dldepth));
	sbpp = 1<<ld;
	dbpp = 1<<p->dldepth;
	while(dx < maxdx){
		/* assemble 8 bits into v, dest aligned, one source pixel at a time */
		v = 0;
		/*
		 * Straight conversion will throw away low bits, so if it's a mask,
		 * build boolean value by testing all bits.
		 */
		while(ddx < 8){
			sx7 = sx&7;
			sh = ddx - sx7;
			if(p->ismask){
				if(*a & ((spm>>sx7)))
					v |= dpm>>ddx;
			}else{
				if(sh <= 0)
					v |= (*a&((dpm>>sx7))) << (-sh);
				else
					v |= (*a&((dpm>>sx7))) >> sh;
			}
			ddx += dbpp;
			sx += sbpp;
			if(sx >= maxsx){
				sx = minsx;
				a = ba;
			}else{
				if((sx&7) == 0)
					a++;
			}
		}
		*b++ = v;
		dx += nbit;
		ddx = 0;
		nbit = 8;
	}
	return buf;
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

	/* converting tables; ldepth 0 to 1, 2, 3*/
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

	/* converting tables; ldepth 1 to 2, 3*/
	bp2 = &conv12[0][0];
	bp3 = &conv13[0][0];
	for(v=0; v<256; v++){
		for(i=0; i<8; i++)
			if(v & (0x80>>i)){
				bp2[i>>2] |= 0xA0 >> ((i&1)+(2*(i&2)));
				bp3[i>>1] |= 0xAA >> (i&1);
			}
		bp2 += 2;
		bp3 += 4;
	}

	/* converting tables; ldepth 2 to 3*/
	bp3 = &conv23[0][0];
	for(v=0; v<256; v++){
		for(i=0; i<8; i++)
			if(v & (0x80>>i))
				bp3[i>>2] |= 0x88 >> (i&3);
		bp3 += 2;
	}
	tablesbuilt = 1;
}

/*
 * Fill entire byte with replicated (if necessary) copy of source pixel,
 * assuming destination ldepth is >= source ldepth.
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

	if(r->min.x>=r->max.x || r->min.y>=r->max.y)
		return 0;
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

static int
sgn(int dld, int sld)
{
	if(dld < sld)
		return 0;
	if(dld == sld)
		return 1;
	return 2;
}

void
memimagedraw(Memimage *dst, Rectangle r, Memimage *src, Point p0, Memimage *mask, Point p1)
{
	uchar *d, *s, *m, *d1, *m1, *s1, *bitmask;
	Rectangle sr;
	int dw, x0, x1, x2, x3, pm, lm, rm;
	int i, n, x, y, state, srcval, y0, y1, dy, dd, nbytes;
	Param sparam, mparam;
	uchar *(*srcfn)(Param*, int), *(*maskfn)(Param*, int);

	/* compute tables */
	if(tablesbuilt == 0)
		buildtables();

	if(drawclip(dst, &r, src, &p0, mask, &p1, &sr) == 0)
		return;
	/* p0 now == sr.min */
	/* some checking of ldepths; more will follow */
	x = dst->ldepth | src->ldepth | mask->ldepth;
	if(x<0 || x>3)
		return;
	pm = ~(7>>dst->ldepth);
	state = 0;
	if(mask->repl){
		state |= Replmask;
		if(Dx(mask->r)==1 && Dy(mask->r)==1){
			if(membyteval(mask) == 0)
				return;
			state |= Simplemask;
		}
	}

	srcval = 0;
	if(src->repl){
		state |= Replsrc;
		if(Dx(src->r)==1 && Dy(src->r)==1){
			srcval = membyteval(src);
			if(dst->ldepth < src->ldepth){
				/* must convert by decimation and repacking */
				srcval &= ~(0xFF>>(1<<dst->ldepth));
				for(i=1<<dst->ldepth; i<8; i<<=1)
					srcval |= srcval>>i;
			}
			state |= Simplesrc;
		}
	}

	/*
	 * common character drawing case
	 */
	if(state==(Simplesrc|Replsrc) && dst->ldepth == 3 && mask->ldepth == 0){
		d = byteaddr(dst, r.min);
		m = byteaddr(mask, p1);
		y0 = p1.y;
		y1 = p1.y+Dy(r);
		dd = dst->width*sizeof(ulong);
		x0 = sr.min.x + p1.x - p0.x;
		x0 = 0x80 >> (x0 & 7);
		x1 = r.max.x - r.min.x;
		dw = mask->width*sizeof(ulong);
		/* y goes forward because mask->data != dst->data */
		for(y=y0; y!=y1; y++, d+=dd, m+=dw){
			m1 = m;
			i = *m1++;
			x = x0;
			d1 = d;
			s = d1 + x1;
			for(; d1<s; d1++){
				if(!x){
					x = 0x80;
					i = *m1++;
				}
				if(i & x)
					*d1 = srcval;
				x >>= 1;
			}
		}
		return;
	}

	nbytes = bytesperline(r, dst->ldepth);

	x = nbytes+Nslop;
	n = 0;
	srcfn = nil;
	sparam.nbuf = 0;
	if((state & (Simplesrc|Replsrc)) != (Simplesrc|Replsrc)){
		sparam.i = src;
		sparam.y0addr = byteaddr(src, Pt(p0.x, 0));
		sparam.ismask = 0;
		sparam.bwidth = src->width*sizeof(ulong);
		sparam.needbuf = 1;
		sparam.dldepth = dst->ldepth;
		sparam.nbytes = nbytes;
		sparam.pm = 7>>src->ldepth;
		sparam.dr = r;
		sparam.sr = sr;

		srcfn = loadfn[sgn(dst->ldepth, src->ldepth)][src->repl!=0][src->ldepth!=3];
		n += setbuf(x, &sparam, state&Simplesrc, state&Replsrc);
	}

	maskfn = nil;
	mparam.nbuf = 0;
	if((state & (Simplemask|Replmask)) != (Simplemask|Replmask)){
		mparam.i = mask;
		mparam.y0addr = byteaddr(mask, Pt(p1.x, 0));
		mparam.ismask = 1;
		mparam.bwidth = mask->width*sizeof(ulong);
		mparam.needbuf = 1;
		mparam.dldepth = dst->ldepth;
		mparam.nbytes = nbytes;
		mparam.pm = 7>>mask->ldepth;
		mparam.dr = r;
		mparam.sr = rectaddpt(sr, subpt(p1, p0));

		maskfn = loadfn[sgn(dst->ldepth, mask->ldepth)][mask->repl!=0][mask->ldepth!=3];
		n += setbuf(x, &mparam, state&Simplemask, state&Replmask);
	}

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
		/*
		 * If source and dest are both byte-aligned,
		 * memmove has enough protection for the
		 * overlapped case.
		 */
		if(src->ldepth==3 && dst->ldepth==3)
			sparam.needbuf = 0;
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
				s = d1 + (x3-x0);
				for(; d1<s; d1++, m1++)
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

