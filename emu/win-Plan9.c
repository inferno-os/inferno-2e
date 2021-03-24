#include "dat.h"
#include "fns.h"
#include "kernel.h"

#include <image.h>
#include <memimage.h>
#include <cursor.h>

enum {
	Margin		= 4,
	Xfer		= 7*1024,

	NBitmap		= 5,
};

static int bitbltfd = -1;
static int consctlfd = -1;
static int keybdfd = -1;
static int keybdpid = -1;
static int mousefd = -1;
static int pixels = 1;
static int refreshpid = -1;

static ulong* data;
static uchar* wrbitmap;

typedef struct Bitmap {
	int	id;
	Rectangle r;
} Bitmap;
static Bitmap bitmap[NBitmap];
static Rectangle spot;

extern void flushmemscreen(Rectangle);

static void
readkeybd(void* v)
{
	int n;
	char buf[128];

	USED(v);
	for(;;){
		n = read(keybdfd, buf, sizeof(buf));
		if(n < 0)	/* probably interrupted */
			_exits(0);
		qproduce(gkbdq, buf, n);
	}
}

static Rectangle
initrect(void)
{
	uchar buf[34];
	Rectangle r;

	if(write(bitbltfd, "i", 1) != 1)
		kwerrstr("initrect: write /dev/bitblt");
	if(read(bitbltfd, buf, sizeof buf) != sizeof buf || buf[0] != 'I')
		kwerrstr("initrect: init read");
	r.min.x = BGLONG(buf+2)+Margin;
	r.min.y = BGLONG(buf+6)+Margin;
	r.max.x = BGLONG(buf+10)+Margin;
	r.max.y = BGLONG(buf+14)+Margin;

	return r;
}

static void
readmouse(void* v)
{
	ulong b, ms, n;
	Pointer m;
	static int lastb, lastms;
	uchar buf[32];

	USED(v);
	for(;;){
		n = read(mousefd, buf, sizeof(buf));
		if(n < 0)
			_exits(0);
		if(n != 14 || buf[0] != 'm'){
			kwerrstr("readmouse: bad count");
			continue;
		}
		if(buf[1] & 0x80){
			spot = initrect();
			flushmemscreen(spot);
		}

		m.b = buf[1];
		if((m.x = BGLONG(buf+2)-spot.min.x) < 0)
			m.x = 0;
		if((m.y = BGLONG(buf+6)-spot.min.y) < 0)
			m.y = 0;
		ms = BGLONG(buf+10);

		b = lastb^m.b;
		lastb = m.b;
		if(b && (b & m.b)){
			if(ms - lastms < 400)
				m.b |= (1<<4);
			lastms = ms;
		}
		m.modify = 1;
		if(ptrq != nil)
	  		qproduce(ptrq, &m, sizeof(m));
	}
}

static void
bitmapfree(void)
{
	Bitmap *bp;
	uchar buf[3];

	for(bp = bitmap; bp < &bitmap[NBitmap]; bp++){
		if(bp->id == 0)
			continue;
		buf[0] = 'f';
		BPSHORT(buf+1, bp->id);
		write(bitbltfd, buf, 3);
		bp->id = 0;
	}
}

static int
bitmapalloc(Rectangle r, int ld)
{
	int n, x;
	uchar buf[18];
	Bitmap *bp;

	for(n = 0; n < NBitmap; n++){
		bp = &bitmap[n];
		switch(n){

		default:
			return n;

		case 4:
			x = 32;
			break;

		case 3:
			x = r.max.x/4+32;
			break;

		case 2:
			x = r.max.x/2+32;
			break;

		case 1:
			x = (r.max.x*3)/4+32;
			break;

		case 0:
			x = r.max.x;
			break;
		}

		buf[0] = 'a';
		buf[1] = ld;
		BPLONG(buf+2, 0);
		BPLONG(buf+6, 0);
		BPLONG(buf+10, x);
		BPLONG(buf+14, r.max.y);
		if(write(bitbltfd, buf, 18) < 0)
			break;
		if(read(bitbltfd, buf, 3) < 0 || buf[0] != 'A')
			break;

		bp->id = BGSHORT(buf+1);
		bp->r = Rect(0, 0, x, r.max.y);
	}

	return n;
}

static ulong*
attachscreenerr(char* err)
{
	if(err)
		fprint(2, "attachscreen: %s: %r\n", err);

	if(wrbitmap){
		free(wrbitmap);
		wrbitmap = 0;
	}
	if(data){
		free(data);
		data = 0;
	}
	bitmapfree();
	if(mousefd >= 0){
		close(mousefd);
		mousefd = -1;
	}
	if(bitbltfd >= 0){
		close(bitbltfd);
		bitbltfd = -1;
	}
	if(consctlfd >= 0){
		close(consctlfd);
		consctlfd = -1;
	}
	if(keybdfd >= 0){
		close(keybdfd);
		keybdfd = -1;
	}

	return nil;
}

ulong*
attachscreen(Rectangle* r, int* ld, int* width)
{
	int fd;
	char *p, init[128];

	p = getenv("8½srv");
	if(p == nil)
		return nil;

	if((fd = open(p, ORDWR)) < 0)
		return attachscreenerr(p);
	sprint(init, "N0,0,0,%d,%d", Xsize+2*Margin, Ysize+2*Margin);
	if(mount(fd, "/mnt/8½", MREPL, init) < 0)
		return attachscreenerr("/mnt/8½");
	close(fd);
	
	if((keybdfd = open("/mnt/8½/cons", OREAD)) < 0)
		return attachscreenerr("/mnt/8½/cons");
	if((consctlfd = open("/mnt/8½/consctl", OWRITE)) < 0)
		return attachscreenerr("/mnt/8½/consctl");
	if(write(consctlfd, "rawon", 5) < 0)
		return attachscreenerr("/mnt/8½/consctl");
	if((mousefd = open("/mnt/8½/mouse", OREAD)) < 0)
		return attachscreenerr("/mnt/8½/mouse");
	if((bitbltfd = open("/mnt/8½/bitblt", ORDWR)) < 0)
		return attachscreenerr("/mnt/8½/bitblt");

	r->min.x = 0;
	r->min.y = 0;
	r->max.x = Xsize;
	r->max.y = Ysize;
	*ld = 3;
	*width = Xsize/4;

	spot = Rect(Margin, Margin, Margin+Xsize, Margin+Ysize);

	if(bitmapalloc(*r, *ld) < NBitmap)
		return attachscreenerr("initialize /dev/bitblt: can't allocate temporary space");

	if((data = malloc(Xsize*Ysize)) == nil)
		return attachscreenerr("allocate bitmap");
	if((wrbitmap = malloc(Xfer+11)) == nil)
		return attachscreenerr("allocate wrbitmap");

	refreshpid = kproc("readmouse", readmouse, nil);
	keybdpid = kproc("readkbd", readkeybd, nil);
	bind("/mnt/8½", "/dev", MBEFORE);

	return data;
}

static void
wrbitmapx(Bitmap* bp, Rectangle* r, Rectangle *rs)
{
	uchar *d, *wp;
	int chunky, dx, dy, n, ppl, ppr, y;

	ppl = Xsize*pixels;
	ppr = Dx(bp->r)*pixels;
	d = ((uchar*)data) + ppl*(r->min.y) + r->min.x;
	dy = Dy(*r);
	dx = Dx(*r);
	for(y = 0; y < dy; y += chunky){
		chunky = r->max.y - y;
		if(chunky*ppr > Xfer)
			chunky = Xfer/ppr;

		wrbitmap[0] = 'w';
		BPSHORT(wrbitmap+1, bp->id);
		BPLONG(wrbitmap+3, y);
		BPLONG(wrbitmap+7, y+chunky);
		wp = wrbitmap+11;

		for(n = 0; n < chunky; n++){
			memmove(wp, d, dx);
			wp += ppr;
			d += ppl;
		}
		if(write(bitbltfd, wrbitmap, 11+(chunky*ppr)) < 0)
			break;
	}

	*rs = Rect(0, 0, dx, dy);
}

static void
wrbitmap0(Bitmap* bp, Rectangle* r, Rectangle* rs)
{
	uchar *d;
	int dy, n, ppl, y;

	ppl = Xsize*pixels;
	d = ((uchar*)data) + ppl*(r->min.y);
	for(y = r->min.y; y < r->max.y; y += dy){
		dy = r->max.y - y;
		if(dy*ppl > Xfer)
			dy = Xfer/ppl;
		n = dy*ppl;
		wrbitmap[0] = 'w';
		BPSHORT(wrbitmap+1, bp->id);
		BPLONG(wrbitmap+3, y);
		BPLONG(wrbitmap+7, y+dy);

		memmove(wrbitmap+11, d, n);
		if(write(bitbltfd, wrbitmap, 11+n) < 0)
			break;
		d += n;
	}

	*rs = *r;
}

void
flushmemscreen(Rectangle r)
{
	uchar buf[31];
	Bitmap *bp;
	Rectangle rs;
	Point p;

	if(data == nil || wrbitmap == nil || !rectclip(&r, Rect(0, 0, Xsize, Ysize)))
		return;

	for(bp = &bitmap[NBitmap-1]; bp >= bitmap; bp--){
		if(Dx(bp->r) >= Dx(r))
			break;
	}

	if(bp == bitmap)
		wrbitmap0(bp, &r, &rs);
	else
		wrbitmapx(bp, &r, &rs);

	buf[0] = 'b';
	BPSHORT(buf+1, 0);
	p = addpt(spot.min, r.min);
	BPLONG(buf+3, p.x);
	BPLONG(buf+7, p.y);
	BPSHORT(buf+11, bp->id);
	BPLONG(buf+13, rs.min.x);
	BPLONG(buf+17, rs.min.y);
	BPLONG(buf+21, rs.max.x);
	BPLONG(buf+25, rs.max.y);
	BPSHORT(buf+29, 0xC);				/* S */
	write(bitbltfd, buf, 31);
}

void
drawcursor(Drawcursor* c)
{
	int j, i, h, w, bpl;
	uchar *bc, *bs, *cclr, *cset, curs[1+2*4+2*16+2*16];

	curs[0] = 'c';
	if(c->data == nil){
		write(bitbltfd, curs, 1);
		return;
	}

	BPLONG(curs+1, c->hotx);
	BPLONG(curs+1+4, c->hoty);

	w = (c->maxx-c->minx);
	h = (c->maxy-c->miny)/2;

	cclr = curs+1+2*4;
	cset = curs+1+2*4+2*16;
	bpl = bytesperline(Rect(c->minx, c->miny, c->maxx, c->maxy), 0);
	bc = c->data;
	bs = c->data + h*bpl;

	if(h > 16)
		h = 16;
	if(w > 16)
		w = 16;
	w /= 8;
	for(i = 0; i < h; i++) {
		for(j = 0; j < w; j++){
			cclr[j] = bc[j];
			cset[j] = bs[j];
		}
		bc += bpl;
		bs += bpl;
		cclr += 2;
		cset += 2;
	}
	write(bitbltfd, curs, sizeof(curs));
}

void
killrefresh(void)
{
	if(refreshpid < 0)
		return;

	bitmapfree();

	close(mousefd);
	close(bitbltfd);

	postnote(PNPROC, refreshpid, "die");
	postnote(PNPROC, keybdpid, "die");
}
