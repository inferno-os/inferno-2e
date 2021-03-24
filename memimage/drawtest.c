#include <lib9.h>
#include <image.h>
#include <memimage.h>

/*
 * This program tests the 'memimagedraw' primitive stochastically.
 * It tests the combination aspects of it thoroughly, but since the
 * three images it uses are disjoint, it makes no check of the
 * correct behavior when images overlap.  That is, however, much
 * easier to get right and to test.
 */

int	onepixelbinary(int, int, int);
int	onepixelbitwise(int, int, int);
void	verifyone(void);
void	verifyline(void);
void	verifyrect(void);
void	verifyrectrepl(int, int);
int	(*onepixel)(int, int, int);

int	dld;
int	sld;
int	mld;
int	seed;
int	niters = 100;
int	dbpp;	/* bits per pixel in destination */
int	sbpp;	/* bits per pixel in src */
int	mbpp;	/* bits per pixel in mask */
int	dpm;	/* pixel mask at high part of byte, in destination */
int	nbytes;	/* in destination */

enum
{
	Xrange	= 64,
	Yrange	= 8,
};


Memimage	*dst;
Memimage	*src;
Memimage	*mask;
Memimage	*stmp;
Memimage	*mtmp;
Memimage	*ones;
uchar	*dstbits;
uchar	*srcbits;
uchar	*maskbits;
ulong	*savedstbits;

void
main(int argc, char *argv[])
{
	seed = time(0);
	onepixel = onepixelbinary;

	ARGBEGIN{
	case 'b':
		onepixel = onepixelbitwise;
		break;
	case 'n':
		niters = atoi(ARGF());
		break;
	case 's':
		seed = atoi(ARGF());
		break;
	}ARGEND

	switch(argc){
	case 0:
		dld = 3;
		sld = 3;
		mld = 3;
		break;
	case 1:
		dld = atoi(argv[0]);
		sld = dld;
		mld = dld;
		break;
	case 2:
		dld = atoi(argv[0]);
		sld = atoi(argv[1]);
		mld = sld;
		break;
	case 3:
		dld = atoi(argv[0]);
		sld = atoi(argv[1]);
		mld = atoi(argv[2]);
		break;
	default:
	Usage:
		fprint(2, "usage: dtest [dld (<=3) [sld [mld]]]\n");
		exits("usage");
	}

	if(dld<0 || 3<dld || sld<0 || 3<sld)
		goto Usage;

	if(sld>dld || (sld<dld && sld!=0) || mld>dld || (mld<dld && mld!=0)){
		fprint(2, "dtest: can only do upward conversions from ldepth 1\n");
		exits("ldepth");
	}
	print("%s -s 0x%lux %d %d %d\n", argv0, seed, dld, sld, mld);
	srand(seed);

	dst = allocmemimage(Rect(0, 0, Xrange, Yrange), dld);
	src = allocmemimage(Rect(0, 0, Xrange, Yrange), sld);
	mask = allocmemimage(Rect(0, 0, Xrange, Yrange), mld);
	stmp = allocmemimage(Rect(0, 0, Xrange, Yrange), sld);
	mtmp = allocmemimage(Rect(0, 0, Xrange, Yrange), mld);
	ones = allocmemimage(Rect(0, 0, Xrange, Yrange), mld);
	nbytes = dst->width * sizeof(ulong) * Yrange;
	dstbits = malloc(nbytes+1);	/* plus 1 because we fill with ushorts */
	srcbits = malloc(nbytes+1);
	maskbits = malloc(nbytes+1);
	savedstbits = malloc(nbytes+1);
	if(dst==0 || src==0 || mask==0 || stmp==0 || mtmp==0 || dstbits==0 ||
		srcbits==0 || maskbits==0 || savedstbits==0){
		fprint(2, "dtest: allocation failed: %r\n");
		exits("alloc");
	}
	dbpp = 1<<dld;
	sbpp = 1<<sld;
	mbpp = 1<<mld;
	dpm = 0xFF ^ (0xFF>>dbpp);
	memset(ones->base, 0xFF, ones->width*sizeof(ulong)*Yrange);

	print("dtest: verify single pixel operation\n");
	verifyone();

	print("dtest: verify full line non-replicated\n");
	verifyline();

	print("dtest: verify full rectangle non-replicated\n");
	verifyrect();

	print("dtest: verify full rectangle source replicated\n");
	verifyrectrepl(1, 0);

	print("dtest: verify full rectangle mask replicated\n");
	verifyrectrepl(0, 1);

	print("dtest: verify full rectangle source and mask replicated\n");
	verifyrectrepl(1, 1);

	exits(0);
}

/*
 * Return result of one pixel operation.  All values are
 * in high part of byte, but may need masking.
 * Two versions: binary uses mask as zero or non-zero only;
 * bitwise does full insert under mask.
 * This code knows that under conversion, source and
 * mask must be ldepth 0.
 */
int
onepixelbinary(int dv, int sv, int mv)
{
	if(mld != dld){
		mv &= 0xFF ^ (0xFF>>mbpp);
		if(mv)
			mv = dpm;
	}
	if((mv&dpm) == 0)
		return dv&dpm;
	if(sld != dld){
		sv &= 0xFF ^ (0xFF>>sbpp);
		if(sv)
			sv = dpm;
	}
	return sv & dpm;
}

int
onepixelbitwise(int dv, int sv, int mv)
{
	if(mld != dld)
		mv &= 0xFF ^ (0xFF>>(1<<mld));
	if(sld != dld){
		/* we know sld is 0; pixel is either 0 or 1 */
		if(sv&0x80)
			sv = 0x80;
		else
			sv = 0;
	}
	dv ^= (dv^sv) & mv;
	return dv;
}

/*
 * Verify that the destination pixel has the specified value.
 * The value is in the high bits of v, suitably masked, but must
 * be extracted from the destination Memimage.
 */
void
checkone(Point p, Point sp, Point mp, int v)
{
	uchar *dp;
	int dv, dx;

	dp = byteaddr(dst, p);
	dv = *dp;
	dx = p.x & (7>>dld);
	dv <<= dx*dbpp;
	dv &= dpm;
	if(dv != v){
		fprint(2, "dtest: one bad pixel at [%d,%d] from [%d, %d][%d, %d]: %.2ux should be %.2ux\n", p.x, p.y, sp.x, sp.y, mp.x, mp.y, dv, v);
		fprint(2, "addresses: %lux %lux %lux\n", dp, byteaddr(src, sp), byteaddr(mask, sp));
		fprint(2, "data: %.2ux %.2ux %.2ux\n", *dp, *byteaddr(src, sp), *byteaddr(mask, mp));
		abort();
	}
}

/*
 * Verify that the destination line has the same value as the saved line.
 */
void
checkline(Rectangle r, Point sp, Point mp, int y, ulong *saved)
{
	ulong *dp;
	int x, yy;

	dp = wordaddr(dst, Pt(0, y));
	saved += y*dst->width;
	if(memcmp(dp, saved, dst->width*sizeof(ulong)) != 0){
		fprint(2, "dtest: bad line at y=%d\n", y);
		fprint(2, "geometry [[%d, %d], [%d, %d]], [%d, %d][%d, %d]\n", r.min.x, r.min.y, r.max.x, r.max.y, sp.x, sp.y, mp.x, mp.y);
		fprint(2, "result:\n\t");
		for(x=0; x<Xrange; x+=32>>dld)
			fprint(2, "%.8lux ", *dp++);
		fprint(2, "\nexpected:\n\t");
		for(x=0; x<Xrange; x+=32>>dld)
			fprint(2, "%.8lux ", *saved++);
		fprint(2, "\n");
		yy = y+(sp.y-r.min.y);
		fprint(2, "src: repl=%d; r=[[%d, %d], [%d, %d]], clipr=[[%d, %d], [%d, %d]]; sr.min.y=%d\n",
			src->repl,
			src->r.min.x, src->r.min.y, src->r.max.x, src->r.max.y,
			src->clipr.min.x, src->clipr.min.y, src->clipr.max.x, src->clipr.max.y,
			yy);
		fprint(2, "mask: repl=%d; r=[[%d, %d], [%d, %d]], clipr=[[%d, %d], [%d, %d]]\n",
			mask->repl, mask->r.min.x, mask->r.min.y, mask->r.max.x, mask->r.max.y,
			mask->clipr.min.x, mask->clipr.min.y, mask->clipr.max.x, mask->clipr.max.y);
		abort();
	}
}

void
fill(uchar *ucbits)
{
	ushort *bits;
	int i;

	bits = (ushort*)ucbits;
	for(i=0; i<(nbytes+1)/2; i++)
		*bits++ = lrand()>>7;	/* need 16 good bits */
}

/*
 * Mask is preset; do the rest
 */
void
verifyonemask(void)
{
	int v, dv, sv, mv;
	Point dp, sp, mp;

	fill(dstbits);
	fill(srcbits);
	memmove(dst->base, dstbits, dst->width*sizeof(ulong)*Yrange);
	memmove(src->base, srcbits, src->width*sizeof(ulong)*Yrange);
	memmove(mask->base, maskbits, mask->width*sizeof(ulong)*Yrange);

	dp.x = nrand(Xrange);
	dp.y = nrand(Yrange);

	sp.x = nrand(Xrange);
	sp.y = nrand(Yrange);

	mp.x = nrand(Xrange);
	mp.y = nrand(Yrange);

	dv = 0xFF & (*byteaddr(dst, dp) << (dp.x & (7>>dld))*dbpp);
	sv = 0xFF & (*byteaddr(src, sp) << (sp.x & (7>>sld))*sbpp);
	mv = 0xFF & (*byteaddr(mask, mp) << (mp.x & (7>>mld))*mbpp);
	v = onepixel(dv, sv, mv);
	memimagedraw(dst, Rect(dp.x, dp.y, dp.x+1, dp.y+1), src, sp, mask, mp);
	checkone(dp, sp, mp, v);
}

void
verifyone(void)
{
	int i;

	/* mask all ones */
	memset(maskbits, 0xFF, nbytes);
	for(i=0; i<niters; i++)
		verifyonemask();

	/* mask all zeros */
	memset(maskbits, 0, nbytes);
	for(i=0; i<niters; i++)
		verifyonemask();

	/* random mask */
	for(i=0; i<niters; i++){
		fill(maskbits);
		verifyonemask();
	}
}

/*
 * Mask is preset; do the rest
 */
void
verifylinemask(void)
{
	Point sp, mp, tp, up;
	Rectangle dr;
	int x;

	fill(dstbits);
	fill(srcbits);
	memmove(dst->base, dstbits, dst->width*sizeof(ulong)*Yrange);
	memmove(src->base, srcbits, src->width*sizeof(ulong)*Yrange);
	memmove(mask->base, maskbits, mask->width*sizeof(ulong)*Yrange);

	dr.min.x = nrand(Xrange-1);
	dr.min.y = nrand(Yrange-1);
	dr.max.x = dr.min.x + 1 + nrand(Xrange-1-dr.min.x);
	dr.max.y = dr.min.y + 1;

	sp.x = nrand(Xrange);
	sp.y = nrand(Yrange);

	mp.x = nrand(Xrange);
	mp.y = nrand(Yrange);

	tp = sp;
	up = mp;
	for(x=dr.min.x; x<dr.max.x && tp.x<Xrange && up.x<Xrange; x++,tp.x++,up.x++)
		memimagedraw(dst, Rect(x, dr.min.y, x+1, dr.min.y+1), src, tp, mask, up);
	memmove(savedstbits, dst->base, dst->width*sizeof(ulong)*Yrange);

	memmove(dst->base, dstbits, dst->width*sizeof(ulong)*Yrange);

	memimagedraw(dst, dr, src, sp, mask, mp);
	checkline(dr, sp, mp, dr.min.y, savedstbits);
}

void
verifyline(void)
{
	int i;

	/* mask all ones */
	memset(maskbits, 0xFF, nbytes);
	for(i=0; i<niters; i++)
		verifylinemask();

	/* mask all zeros */
	memset(maskbits, 0, nbytes);
	for(i=0; i<niters; i++)
		verifylinemask();

	/* random mask */
	for(i=0; i<niters; i++){
		fill(maskbits);
		verifylinemask();
	}
}

/*
 * Mask is preset; do the rest
 */
void
verifyrectmask(void)
{
	Point sp, mp, tp, up;
	Rectangle dr;
	int x, y;

	fill(dstbits);
	fill(srcbits);
	memmove(dst->base, dstbits, dst->width*sizeof(ulong)*Yrange);
	memmove(src->base, srcbits, src->width*sizeof(ulong)*Yrange);
	memmove(mask->base, maskbits, mask->width*sizeof(ulong)*Yrange);

	dr.min.x = nrand(Xrange-1);
	dr.min.y = nrand(Yrange-1);
	dr.max.x = dr.min.x + 1 + nrand(Xrange-1-dr.min.x);
	dr.max.y = dr.min.y + 1 + nrand(Yrange-1-dr.min.y);

	sp.x = nrand(Xrange);
	sp.y = nrand(Yrange);

	mp.x = nrand(Xrange);
	mp.y = nrand(Yrange);

	tp = sp;
	up = mp;
	for(y=dr.min.y; y<dr.max.y && tp.y<Yrange && up.y<Yrange; y++,tp.y++,up.y++){
		for(x=dr.min.x; x<dr.max.x && tp.x<Xrange && up.x<Xrange; x++,tp.x++,up.x++)
			memimagedraw(dst, Rect(x, y, x+1, y+1), src, tp, mask, up);
		tp.x = sp.x;
		up.x = mp.x;
	}
	memmove(savedstbits, dst->base, dst->width*sizeof(ulong)*Yrange);

	memmove(dst->base, dstbits, dst->width*sizeof(ulong)*Yrange);

	memimagedraw(dst, dr, src, sp, mask, mp);
	for(y=0; y<Yrange; y++)
		checkline(dr, sp, mp, y, savedstbits);
}

void
verifyrect(void)
{
	int i;

	/* mask all ones */
	memset(maskbits, 0xFF, nbytes);
	for(i=0; i<niters; i++)
		verifyrectmask();

	/* mask all zeros */
	memset(maskbits, 0, nbytes);
	for(i=0; i<niters; i++)
		verifyrectmask();

	/* random mask */
	for(i=0; i<niters; i++){
		fill(maskbits);
		verifyrectmask();
	}
}

Rectangle
randrect(void)
{
	Rectangle r;

	r.min.x = nrand(Xrange-1);
	r.min.y = nrand(Yrange-1);
	r.max.x = r.min.x + 1 + nrand(Xrange-1-r.min.x);
	r.max.y = r.min.y + 1 + nrand(Yrange-1-r.min.y);
	return r;
}

/*
 * Return coordinate corresponding to x withing range [minx, maxx)
 */
int
tilexy(int minx, int maxx, int x)
{
	int sx;

	sx = (x-minx) % (maxx-minx);
	if(sx < 0)
		sx += maxx-minx;
	return sx+minx;
}

void
replicate(Memimage *i, Memimage *tmp)
{
	Rectangle r, r1;
	int x, y;

	memset(tmp->base, 0, tmp->width*sizeof(ulong)*Yrange);
	r.min.x = nrand(Xrange-1);
	r.min.y = nrand(Yrange-1);
	/* make it trivial more often than pure chance allows */
	if((lrand()&7) == 0){
		r.max.x = r.min.x + 1;
		r.max.y = r.min.y + 1;
	}else{
		r.max.x = r.min.x + 1 + nrand(Xrange-1-r.min.x);
		r.max.y = r.min.y + 1 + nrand(Yrange-1-r.min.y);
	}
	memimagedraw(tmp, r, i, r.min, ones, r.min);
	memmove(i->base, tmp->base, tmp->width*sizeof(ulong)*Yrange);
	/* i is now a non-replicated instance of the replication */
	/* replicate it by hand through tmp */
	memset(tmp->base, 0, tmp->width*sizeof(ulong)*Yrange);
	x = -(tilexy(r.min.x, r.max.x, 0)-r.min.x);
	for(; x<Xrange; x+=Dx(r)){
		y = -(tilexy(r.min.y, r.max.y, 0)-r.min.y);
		for(; y<Yrange; y+=Dy(r)){
			/* set r1 to instance of tile by translation */
			r1.min.x = x;
			r1.min.y = y;
			r1.max.x = r1.min.x+Dx(r);
			r1.max.y = r1.min.y+Dy(r);
			memimagedraw(tmp, r1, i, r.min, ones, r.min);
		}
	}
	i->repl = 1;
	i->r = r;
	i->clipr = randrect();
	tmp->clipr = i->clipr;
}

/*
 * Mask is preset; do the rest
 */
void
verifyrectmaskrepl(int srcrepl, int maskrepl)
{
	Point sp, mp, tp, up;
	Rectangle dr;
	int x, y;
	Memimage *s, *m;

	src->repl = 0;
	src->r = Rect(0, 0, Xrange, Yrange);
	src->clipr = src->r;
	stmp->repl = 0;
	stmp->r = Rect(0, 0, Xrange, Yrange);
	stmp->clipr = src->r;
	mask->repl = 0;
	mask->r = Rect(0, 0, Xrange, Yrange);
	mask->clipr = mask->r;
	mtmp->repl = 0;
	mtmp->r = Rect(0, 0, Xrange, Yrange);
	mtmp->clipr = mask->r;

	fill(dstbits);
	fill(srcbits);

	memmove(dst->base, dstbits, dst->width*sizeof(ulong)*Yrange);
	memmove(src->base, srcbits, src->width*sizeof(ulong)*Yrange);
	memmove(mask->base, maskbits, mask->width*sizeof(ulong)*Yrange);

	if(srcrepl){
		replicate(src, stmp);
		s = stmp;
	}else
		s = src;
	if(maskrepl){
		replicate(mask, mtmp);
		m = mtmp;
	}else
		m = mask;

	dr = randrect();

	sp.x = nrand(Xrange);
	sp.y = nrand(Yrange);

	mp.x = nrand(Xrange);
	mp.y = nrand(Yrange);

	for(tp.y=sp.y,up.y=mp.y,y=dr.min.y; y<dr.max.y && tp.y<Yrange && up.y<Yrange; y++,tp.y++,up.y++)
		for(tp.x=sp.x,up.x=mp.x,x=dr.min.x; x<dr.max.x && tp.x<Xrange && up.x<Xrange; x++,tp.x++,up.x++)
			memimagedraw(dst, Rect(x, y, x+1, y+1), s, tp, m, up);
	memmove(savedstbits, dst->base, dst->width*sizeof(ulong)*Yrange);

	memmove(dst->base, dstbits, dst->width*sizeof(ulong)*Yrange);

	memimagedraw(dst, dr, src, sp, mask, mp);
	for(y=0; y<Yrange; y++)
		checkline(dr, sp, sp, y, savedstbits);
}

void
verifyrectrepl(int srcrepl, int maskrepl)
{
	int i;

	/* mask all ones */
	memset(maskbits, 0xFF, nbytes);
	for(i=0; i<niters; i++)
		verifyrectmaskrepl(srcrepl, maskrepl);

	/* mask all zeros */
	memset(maskbits, 0, nbytes);
	for(i=0; i<niters; i++)
		verifyrectmaskrepl(srcrepl, maskrepl);

	/* random mask */
	for(i=0; i<niters; i++){
		fill(maskbits);
		verifyrectmaskrepl(srcrepl, maskrepl);
	}
}

void *imagmem;
void*
poolalloc(void*, int size)
{
	return malloc(size);
}

void
poolfree(void*, void *p)
{
	free(p);
}
