#include <lib9.h>
#include <image.h>

Subfont		*defaultsubfont;
Font		*defaultfont;

enum{
	Black = 255,
	Blue = 252,
	Red = 31,
	Yellow = 3,
	Green = 195,

	Colorsize = 1,
};

Image 	*blue, *red, *yellow, *green, *black, *plaid, *xplaid, *box, *ones;

Screen	*screen;

int
rgb(int r, int g, int b)
{
	int c;

	if(r+0x08 <= 255)
		r += 0x08;
	if(g+0x08 <= 255)
		g += 0x08;
	if(b+0x10 <= 255)
		b += 0x10;
	c = (r & 0xE0);
	c |= (g & 0xE0) >> 3;
	c |= (b & 0xC0) >> 6;
	return ~c & 0xFF;
}

void
drawred(Image *i, Rectangle r, void*)
{
	lockdisplay(i->display);
	draw(i, r, red, ones, r.min);
	unlockdisplay(i->display);
}

typedef struct QLock QLock;
struct QLock
{
	int	rel;
	int	acq;
};

void
qlock(QLock *q)
{
	char buf[1];

	read(q->acq, buf, 1);
}

void
qunlock(QLock *q)
{
	write(q->rel, "o", 1);
}

QLock*
newqlock(void)
{
	QLock *q;
	int p[2];

	q = malloc(sizeof(QLock));
	if(q == 0){
		fprint(2, "qinit: out of memory\n");
		exits("out of memory");
	}
	if(pipe(p) < 0){
		fprint(2, "qinit: can't make pipe: %r\n");
		exits("qinit pipe");
	}
	q->rel = p[0];
	q->acq = p[1];
	qunlock(q);
	return q;
}

void
lockdisplay(Display *d)
{
	if(d->lock){
		qlock(d->lock);
		d->lockptr = "";
	}
}

void
unlockdisplay(Display *d)
{
	if(d->lock){
		d->lockptr = 0;
		qunlock(d->lock);
	}
}

void
allocatedisplaylock(Display *d)
{
	d->lock = newqlock();
	d->lockptr = 0;
}

void
freedisplaylock(Display *d)
{
	d->lockptr = 0;
	free(d->lock);
}

int id;

Rectangle
R(int minx, int miny, int maxx, int maxy)
{
	Rectangle r;

	r.min.x = minx;
	r.min.y = miny;
	r.max.x = maxx;
	r.max.y = maxy;
	if(id)
		r = rectaddpt(r, Pt(250, 150));
	return r;
}

void
main(int argc, char *argv[])
{
	Font *f1, *f2;
	char *f1name = "/lib/font/bit/lucidasans/latin1.10.font";
	Image *lr, *lg, *ly;
	Image *cross;
	Rectangle r;
	Display *display;
	int i;

	id = 0;
	if(argc > 1)
		id = atoi(argv[1]);
	display = initdisplay(0);
	if(display == 0){
		fprint(2, "init failed: %r\n");
		exits("init");
	}
	ones = display->ones;
/*
	defaultsubfont = getdefont();
	defaultfont = mkfont(defaultsubfont, 0x0000, 0);
*/



	f1 = openfont(display, f1name, 3);
	if(f1 == 0){
		fprint(2, "test: can't open font %s: %r\n", f1name);
		exits("font");
	}
	f2 = openfont(display, f1name, 3);
	if(f2 == 0){
		fprint(2, "test: can't open font %s: %r\n", f1name);
		exits("font");
	}

	blue = allocimage(display, Rect(0, 0, Colorsize, 1), 3, 1, Blue);
	yellow = allocimage(display, Rect(0, 0, Colorsize, 1), 3, 1, Yellow);
	red = allocimage(display, Rect(0, 0, Colorsize, 1), 3, 1, Red);
	green = allocimage(display, Rect(0, 0, Colorsize, 1), 3, 1, Green);
	black = allocimage(display, Rect(0, 0, Colorsize, 1), 3, 1, Black);
	if(blue==0 || yellow==0 || red==0 || green==0 || black==0){
		fprint(2, "test: can't allocate colors: %r\n");
		exits("colors");
	}

	plaid = allocimage(display, Rect(0, 0, 13, 5), 3, 1, Red);
	/* horizontal yellow line */
	draw(plaid, Rect(0, 1, 13, 2), yellow, ones, Pt(0, 0));
	/* vertical black line */
	draw(plaid, Rect(2, 0, 4, 5), black, ones, Pt(0, 0));

	xplaid = allocimage(display, Rect(0, 0, 640, 480), 3, 0, Red);
	draw(xplaid, xplaid->r, plaid, ones, Pt(0,0));

	cross = allocimage(display, Rect(0, 0, 7, 6), 3, 1, 0);
	/* horizontal line */
	draw(cross, Rect(0, 0, 7, 2), ones, ones, Pt(0, 0));
	/* vertical line */
	draw(cross, Rect(0, 0, 2, 6), ones, ones, Pt(0, 0));


	box = allocimage(display, Rect(0, 0, 50, 50), 3, 1, 0xFF);

	if(id){
		screen = publicscreen(display, id, 3);
		if(screen == 0){
			fprint(2, "screen attach failed: %r\n");
			exits("screen");
		}
		lg = allocwindow(screen, Rect(200, 150, 400, 400), 0, 0);
		if(lg == 0){
			fprint(2, "lalloc green failed: %r\n");
			exits("lalloc");
		}
		draw(lg, lg->r, plaid, ones, lg->r.min);
		flushimage(display, 1);
if(0)		sleep(1000);
	}else{
		screen = allocscreen(display->image, blue, 1);
		if(screen == 0){
			fprint(2, "screen allocation failed: %r\n");
			exits("screen");
		}
		draw(display->image, display->image->r, blue, ones, display->image->r.min);
		flushimage(display, 1);
		print("screen id is %d\n", screen->id);
if(0)		sleep(1000);
	}


	lr = allocwindow(screen, R(75, 75, 640-75, 480-75), drawred, 0);
	if(lr == 0){
		fprint(2, "lalloc red failed: %r\n");
		exits("lalloc");
	}

	draw(lr, lr->r, red, ones, lr->r.min);
	flushimage(display, 1);
	sleep(1000);

#ifdef asdf
	lg = allocwindow(screen, R(100, 100, 320-25, 240-25), 0, 0);
	if(lg == 0){
		fprint(2, "lalloc green failed: %r\n");
		exits("lalloc");
	}
	draw(lg, lg->r, green, ones, lg->r.min);
	flushimage(display, 1);
	sleep(1000);

	ly = allocwindow(screen, R(10, 10, 320-15, 240-95), 0, 0);
	if(ly == 0){
		fprint(2, "lalloc yellow failed: %r\n");
		exits("lalloc");
	}
	draw(ly, ly->r, yellow, ones, ly->r.min);
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(1000);

	lockdisplay(display);
	topwindow(lr);
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(1000);

	lockdisplay(display);
	string(lg, addpt(lg->r.min, Pt(25,35)), plaid, f1, "Hello world man");
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(1000);

	lockdisplay(display);
	string(lg, addpt(lg->r.min, Pt(25,50)), plaid, f2, "You dog!");
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(1000);

	lockdisplay(display);
	draw(lg, lg->r, (Image*)lg, ones, subpt(lg->r.min, Pt(5, 10)));
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(1000);

	lockdisplay(display);
	draw(ly, rectaddpt(ly->r, Pt(10, 10)), (Image*)lg, ones, lg->r.min);
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(1000);

	lockdisplay(display);
	topwindow(ly);
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(1000);

	lockdisplay(display);
	freeimage(ly);
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(1000);

	lockdisplay(display);
	r = insetrect(lr->r, 10);
	draw(lr, r, plaid, cross, r.min);
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(1000);

	lockdisplay(display);
	topwindow(lr);
	flushimage(display, 1);
	unlockdisplay(display);
	sleep(1000);

	lockdisplay(display);
	bottomwindow(lr);
	flushimage(display, 1);
	unlockdisplay(display);
#endif

	for(i=0; i<10000; i++){
if(0)		line(lr, Pt(100, 100), Pt(500, 200), 5, 0, yellow);
if(0)		line(lr, Pt(100, 50), Pt(200, 450), 5, 0, green);
if(0)		line(lr, Pt(100, 100), Pt(500, 200), 5, 0, plaid);
if(0)		line(lr, Pt(100, 50), Pt(200, 450), 5, 0, plaid);
if(0)		line(lr, Pt(100, 100), Pt(500, 200), 5, 0, xplaid);
if(0)		line(lr, Pt(100, 50), Pt(200, 450), 5, 0, xplaid);

if(1)		draw(lr, Rect(200,200,210,210), blue, ones, Pt(200,200));
if(1)		line(lr, Pt(200, 200), Pt(202, 200), 0, 0, yellow);
if(1)		line(lr, Pt(200, 200), Pt(200, 200), 0, 0, green);
if(1)		line(lr, Pt(202, 200), Pt(202, 200), 0, 0, green);
		if(i == 0)
			flushimage(display, 1);
break;
	}
	flushimage(display, 1);
if(1)	sleep(100000);
	postnote(PNPROC, display->refpid, "kill");
}
