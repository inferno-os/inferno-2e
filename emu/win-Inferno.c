#include	"dat.h"
#include	"fns.h"
#include	"kernel.h"
#include	"error.h"

#include	<image.h>
#include	<memimage.h>
#include	<cursor.h>

enum
{
	Margin	= 4,
	Lsize		= 100,
};

extern Memimage screenimage;

void	brazilreadmouse(void*);
void	brazilreadkeybd(void*);
ulong*	attachwindow(Rectangle*, int*, int*);

int	pixels = 1;

static int		datafd;
static int		ctlfd;
static int		mousefd;
static int		keybdfd;
static int		mousepid = -1;
static int		keybdpid = -1;
static Rectangle	tiler;
static ulong*		data;
static uchar*		loadbuf;
static int		cursfd;
static int		imageid;
static Rectangle	imager;
static int		imageldepth;
static char	winname[64];
static uchar	*chunk;

#define	NINFO	12*12
#define	CHUNK	6000
#define	HDR		21

void
killrefresh(void)
{
	if(mousepid < 0)
		return;
	close(mousefd);
	close(ctlfd);
	close(datafd);
	postnote(PNPROC, mousepid, Eintr);
	postnote(PNPROC, keybdpid, Eintr);
}

ulong*
attachscreen(Rectangle *r, int *ld, int *width, int *softscreen)
{
	int fd, lfd;
	char *p, buf[128], info[NINFO+1];

	p = getenv("wsys");
	if(p == nil)
		return nil;

	fd = open(p, ORDWR);
	if(fd < 0) {
		fprint(2, "attachscreen: can't open window manager: %r\n");
		return nil;
	}
	sprint(buf, "N0,0,0,%d,%d", Xsize+2*Margin, Ysize+2*Margin);
	if(mount(fd, "/mnt/wsys", MREPL, buf) < 0) {
		fprint(2, "attachscreen: can't mount window manager: %r\n");
		return nil;
	}

	cursfd = open("/mnt/wsys/cursor", OWRITE);
	if(cursfd < 0) {
		fprint(2, "attachscreen: open cursor: %r\n");
		return nil;
	}
	
	/* Set up graphics window console (chars->gkbdq) */
	keybdfd = open("/mnt/wsys/cons", OREAD);
	if(keybdfd < 0) {
		fprint(2, "attachscreen: open keyboard: %r\n");
		return nil;
	}
	mousefd = open("/mnt/wsys/mouse", OREAD);
	if(mousefd < 0){
		fprint(2, "attachscreen: can't open mouse: %r\n");
		return nil;
	}
	fd = open("/mnt/wsys/consctl", OWRITE);
	if(fd < 0)
		fprint(2, "attachscreen: open /mnt/wsys/consctl: %r\n");
	if(write(fd, "rawon", 5) != 5)
		fprint(2, "attachscreen: write /mnt/wsys/consctl: %r\n");

	/* Set up graphics files */
	ctlfd = open("/dev/draw/new", ORDWR);
	if(ctlfd < 0){
		fprint(2, "attachscreen: can't open graphics control file: %r\n");
		return nil;
	}
	if(read(ctlfd, info, sizeof info) < NINFO){
		close(ctlfd);
		fprint(2, "attachscreen: can't read graphics control file: %r\n");
		return nil;
	}
	sprint(buf, "/dev/draw/%d/data", atoi(info+0*12));
	datafd = open(buf, ORDWR|OCEXEC);
	if(datafd < 0){
		close(ctlfd);
		fprint(2, "attachscreen: can't read graphics data file: %r\n");
		return nil;
	}

	if(attachwindow(r, ld, width) == nil)
		return nil;
	
	mousepid = kproc("readmouse", brazilreadmouse, nil);
	keybdpid = kproc("readkbd", brazilreadkeybd, nil);
	bind("/mnt/wsys", "/dev", MBEFORE);

	lfd = open("/dev/label", OWRITE);
	if (lfd >= 0) {
		write(lfd, "inferno", 7);
		close(lfd);
	}

	*softscreen = 1;
	return data;
}

ulong*
attachwindow(Rectangle *r, int *ld, int *width)
{
	int n, fd;
	char buf[256];
	uchar ubuf[128];

	/*
	 * Discover name of window
	 */
	fd = open("/mnt/wsys/winname", OREAD);
	if(fd<0 || (n=read(fd, winname, sizeof winname))<=0){
		fprint(2, "attachscreen: can only run inferno under rio, not stand-alone\n");
		return nil;
	}
	close(fd);

	/*
	 * Allocate image pointing to window, and discover its ID
	 */
	ubuf[0] = 'n';
	++imageid;
	BPLONG(ubuf+1, imageid);
	ubuf[5] = n;
	memmove(ubuf+6, winname, n);
	if(write(datafd, ubuf, 6+n) != 6+n){
		fprint(2, "attachscreen: cannot find window id: %r\n");
		return nil;
	}
	if(read(ctlfd, buf, sizeof buf) < 12*12){
		fprint(2, "attachscreen: cannot read window id: %r\n");
		return nil;
	}

	/*
	 * Report back
	 */
	if(r != nil){
		Xsize = atoi(buf+6*12)-atoi(buf+4*12)-2*Margin;
		Ysize = atoi(buf+7*12)-atoi(buf+5*12)-2*Margin;
		r->min.x = 0;
		r->min.y = 0;
		r->max.x = Xsize;
		r->max.y = Ysize;
		data = malloc(Xsize*Ysize);
		loadbuf = malloc(Xsize*Lsize+1);
		chunk = malloc(HDR+CHUNK);
	}
	imageldepth = atoi(buf+2*12);
	if(ld != nil)
		*ld = imageldepth;
	imager.min.x = atoi(buf+4*12);
	imager.min.y = atoi(buf+5*12);
	imager.max.x = atoi(buf+6*12);
	imager.max.y = atoi(buf+7*12);

	if(width != nil)
		*width = Xsize/4;

	tiler.min.x = atoi(buf+4*12)+Margin;
	tiler.min.y = atoi(buf+5*12)+Margin;
	tiler.max.x = atoi(buf+6*12)-Margin;
	tiler.max.y = atoi(buf+7*12)-Margin;

	return data;
}

int
brazilloadimage(Rectangle r, uchar *data, int ndata)
{
	long dy;
	int n, bpl;

	if(!rectinrect(r, imager)){
		werrstr("loadimage: bad rectangle");
		return -1;
	}
	bpl = bytesperline(r, imageldepth);
	n = bpl*Dy(r);
	if(n > ndata){
		werrstr("loadimage: insufficient data");
		return -1;
	}
	ndata = 0;
	while(r.max.y > r.min.y){
		dy = r.max.y - r.min.y;
		if(dy*bpl > CHUNK)
			dy = CHUNK/bpl;
		n = dy*bpl;
		chunk[0] = 'w';
		BPLONG(chunk+1, imageid);
		BPLONG(chunk+5, r.min.x);
		BPLONG(chunk+9, r.min.y);
		BPLONG(chunk+13, r.max.x);
		BPLONG(chunk+17, r.min.y+dy);
		memmove(chunk+21, data, n);
		ndata += n;
		data += n;
		r.min.y += dy;
		n += 21;
		if(r.min.y >= r.max.y)	/* flush to screen */
			chunk[n++] = 'v';
		if(write(datafd, chunk, n) != n)
			return -1;
	}
	return ndata;
}

void
flushmemscreen(Rectangle r)
{
	int n, dy;
	Rectangle rr;

	if(data == nil || loadbuf == nil || chunk==nil)
		return;
	if(!rectclip(&r, Rect(0, 0, Xsize, Ysize)))
		return;
	if(!rectclip(&r, Rect(0, 0, Dx(tiler), Dy(tiler))))
		return;
	if(Dx(r)<=0 || Dy(r)<=0)
		return;

	while(r.min.y < r.max.y){
		dy = Dy(r);
		if(dy > Lsize)
			dy = Lsize;
		rr = r;
		rr.max.y = rr.min.y+dy;
		n = unloadmemimage(&screenimage, rr, loadbuf, Dx(r)*dy);
		/* offset from (0,0) to window */
		rr.min.x += tiler.min.x;
		rr.min.y += tiler.min.y;
		rr.max.x += tiler.min.x;
		rr.max.y += tiler.min.y;
		if(brazilloadimage(rr, loadbuf, n) != n)
			fprint(2, "flushmemscreen: %r\n", n);
		r.min.y += dy;
	}
}

void
drawcursor(Drawcursor *c)
{
	int j, i, h, w, bpl;
	uchar *bc, *bs, *cclr, *cset, curs[2*4+2*2*16];

	/* Set the default system cursor */
	if(c->data == nil) {
		write(cursfd, curs, 0);
		return;
	}

	BPLONG(curs+0*4, c->hotx);
	BPLONG(curs+1*4, c->hoty);

	w = (c->maxx-c->minx);
	h = (c->maxy-c->miny)/2;

	cclr = curs+2*4;
	cset = curs+2*4+2*16;
	bpl = bytesperline(Rect(c->minx, c->miny, c->maxx, c->maxy), 0);
	bc = c->data;
	bs = c->data + h*bpl;

	if(h > 16)
		h = 16;
	if(w > 16)
		w = 16;
	w /= 8;
	for(i = 0; i < h; i++) {
		for(j = 0; j < w; j++) {
			cclr[j] = bc[j];
			cset[j] = bs[j];
		}
		bc += bpl;
		bs += bpl;
		cclr += 2;
		cset += 2;
	}
	write(cursfd, curs, sizeof curs);
}

int
checkmouse(char *buf, int n)
{
	int x, y, tick;
	static int lastb, lastt, lastx, lasty, lastclick;
	Pointer mouse;		/* XXX temporary hack */

	switch(n){
	default:
		kwerrstr("atomouse: bad count");
		return -1;

	case 1+4*12:
		if(buf[0] == 'r'){
			if(attachwindow(nil, nil, nil) == nil)
				return -1;
			flushmemscreen(Rect(0, 0, Xsize, Ysize));
		}
		x = atoi(buf+1+0*12) - tiler.min.x;
		if(x < 0)
			x = 0;
		y = atoi(buf+1+1*12) - tiler.min.y;
		if(y < 0)
			y = 0;
		mouse.x = x;
		mouse.y = y;
		mouse.b = atoi(buf+1+2*12);
		tick = atoi(buf+1+3*12);
		if(mouse.b && lastb == 0){	/* button newly pressed */
			if(mouse.b==lastclick && tick-lastt<400
			   && abs(mouse.x-lastx)<10 && abs(mouse.y-lasty)<10)
				mouse.b |= (1<<4);
			lastt = tick;
			lastclick = mouse.b&7;
			lastx = mouse.x;
			lasty = mouse.y;
		}
		lastb = mouse.b&7;
		mouse.modify = 1;
/*		Wakeup(&mouse.r);	*/		/* XXX research does this */
		if(ptrq != nil)				/* XXX we do this -- for now */
	  		qproduce(ptrq, &mouse, sizeof(mouse));		/* XXX we do this -- for now */
		return n;
	}
}

void
brazilreadmouse(void *v)
{
	int n;
	char buf[128];

	USED(v);
	for(;;){
		n = read(mousefd, buf, sizeof(buf));
		if(n < 0)	/* probably interrupted */
			_exits(0);
		checkmouse(buf, n);
	}
}

void
brazilreadkeybd(void *v)
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
