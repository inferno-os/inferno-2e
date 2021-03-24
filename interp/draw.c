#include <lib9.h>
#include <kernel.h>
#include "interp.h"
#include "isa.h"
#include "runt.h"
#include "raise.h"
#include "drawmod.h"
#include "image.h"
#include "drawif.h"
#include "memimage.h"
#include "memlayer.h"

/*
 * When a Display is remote, it must be locked to synchronize the
 * outgoing message buffer with the refresh demon, which runs as a
 * different process.  When it is local, the refresh demon does nothing
 * and it is sufficient to use the interpreter's own acquire/release protection
 * to lock the buffer.
 *
 * Most action to the buffer is caused by calls from Limbo, so locking at
 * the top before going into the library is good enough.  However, the
 * garbage collector can call the free routines at other times, so they
 * need to protect themselves whether called through the Draw module
 * or not; hence the need for check against recursive locking in lockdisplay().
 * This also means that we needn't lock around calls to destroy if it's
 * extra work to do so.
 */

typedef struct Cache Cache;
typedef struct DRef DRef;
typedef struct DDisplay DDisplay;
typedef struct DImage DImage;
typedef struct DScreen DScreen;
typedef struct DFont DFont;

struct Cache
{
	int	ref;
	char*	name;
	Display*display;
	union{
		Subfont*	sf;
		Font*		f;
		void*		ptr;
	}u;
	Cache*	next;
};

/* not visible to Limbo; used only for internal reference counting */
struct DRef
{
	int		ref;
	Display*	display;
};

struct DDisplay
{
	Draw_Display	drawdisplay;
	Display*	display;
	DRef*		dref;
};

struct DImage
{
	Draw_Image	drawimage;
	Image*		image;
	void*		refreshptr;
	DRef*		dref;
	int		flush;
};

struct DScreen
{
	Draw_Screen	drawscreen;
	Screen*		screen;
	DRef*		dref;
};

struct DFont
{
	Draw_Font	drawfont;
	Font*		font;
	DRef*		dref;
};

Cache*	sfcache[BIHASH];
Cache*	fcache[BIHASH];
void*	cacheqlock;

static	Cache	*cachelookup(Cache**, Display*, char*);

uchar fontmap[] = Draw_Font_map;
uchar imagemap[] = Draw_Image_map;
uchar screenmap[] = Draw_Screen_map;
uchar displaymap[] = Draw_Display_map;

Type*	TFont;
Type*	TImage;
Type*	TScreen;
Type*	TDisplay;

Draw_Image*	allocdrawimage(DDisplay*, Draw_Rect, int, Image*, int, int);
Draw_Image*	color(DDisplay*, int);

char		deffontname[] = "*default*";
void		subfont_close(Subfont*);
void		freeallsubfonts(Display*);

void
drawmodinit(void)
{
	TFont = dtype(freedrawfont, sizeof(DFont), fontmap, sizeof(fontmap));
	TImage = dtype(freedrawimage, sizeof(DImage), imagemap, sizeof(imagemap));
	TScreen = dtype(freedrawscreen, sizeof(DScreen), screenmap, sizeof(screenmap));
	TDisplay = dtype(freedrawdisplay, sizeof(DDisplay), displaymap, sizeof(displaymap));
	builtinmod("$Draw", Drawmodtab);
}

static int
drawhash(char *s)
{
	int h;

	h = 0;
	while(*s){
		h += *s++;
		h <<= 1;
		if(h & (1<<8))
			h |= 1;
	}
	return (h&0xFFFF)%BIHASH;
}

static Cache*
cachelookup(Cache *cache[], Display *d, char *name)
{
	Cache *c;

	libqlock(cacheqlock);
	c = cache[drawhash(name)];
	while(c!=nil && (d!=c->display || strcmp(name, c->name)!=0))
		c = c->next;
	libqunlock(cacheqlock);
	return c;
}

Cache*
cacheinstall(Cache **cache, Display *d, char *name, void *ptr, char *type)
{
	Cache *c;
	int hash;

	USED(type);
	c = cachelookup(cache, d, name);
	if(c){
/*		print("%s %s already in cache\n", type, name); /**/
		return nil;
	}
	c = malloc(sizeof(Cache));
	if(c == nil)
		return nil;
	hash = drawhash(name);
	c->ref = 0;	/* will be incremented by caller */
	c->display = d;
	c->name = strdup(name);
	c->u.ptr = ptr;
	libqlock(cacheqlock);
	c->next = cache[hash];
	cache[hash] = c;
	libqunlock(cacheqlock);
	return c;
}

void
cacheuninstall(Cache **cache, Display *d, char *name, char *type)
{
	Cache *c, *prev;
	int hash;

	hash = drawhash(name);
	libqlock(cacheqlock);
	c = cache[hash];
	if(c == nil){
   Notfound:
		libqunlock(cacheqlock);
		print("%s not in %s cache\n", name, type);
		return;
	}
	prev = nil;
	while(c!=nil && (d!=c->display || strcmp(name, c->name)!=0)){
		prev = c;
		c = c->next;
	}
	if(c == nil)
		goto Notfound;
	if(prev == 0)
		cache[hash] = c->next;
	else
		prev->next = c->next;
	libqunlock(cacheqlock);
	free(c->name);
	free(c);
}

Image*
lookupimage(Draw_Image *di)
{
	Display *disp;
	Image *i;
	int locked;

	if(di == H || D2H(di)->t != TImage)
		return nil;
	i = ((DImage*)di)->image;
	if(!eqrect(IRECT(di->clipr), i->clipr) || di->repl!=i->repl){
		disp = i->display;
		locked = lockdisplay(disp, 1);
		replclipr(i, di->repl, IRECT(di->clipr));
		if(locked)
			unlockdisplay(disp);
	}
	return i;
}

Screen*
lookupscreen(Draw_Screen *ds)
{
	if(ds == H || D2H(ds)->t != TScreen)
		return nil;
	return ((DScreen*)ds)->screen;
}

Font*
lookupfont(Draw_Font *df)
{
	if(df == H || D2H(df)->t != TFont)
		return nil;
	return ((DFont*)df)->font;
}

Display*
lookupdisplay(Draw_Display *dd)
{
	if(dd == H || D2H(dd)->t != TDisplay)
		return nil;
	return ((DDisplay*)dd)->display;
}

Image*
checkimage(Draw_Image *di)
{
	Image *i;

	if(di == H)
		error("nil Image");
	i = lookupimage(di);
	if(i == nil)
		error(exType);
	return i;
}

Screen*
checkscreen(Draw_Screen *ds)
{
	Screen *s;

	if(ds == H)
		error("nil Screen");
	s = lookupscreen(ds);
	if(s == nil)
		error(exType);
	return s;
}

Font*
checkfont(Draw_Font *df)
{
	Font *f;

	if(df == H)
		error("nil Font");
	f = lookupfont(df);
	if(f == nil)
		error(exType);
	return f;
}

Display*
checkdisplay(Draw_Display *dd)
{
	Display *d;

	if(dd == nil)
		error("nil Display");
	d = lookupdisplay(dd);
	if(d == nil)
		error(exType);
	return d;
}

void
Display_allocate(void *fp)
{
	F_Display_allocate *f;
	char buf[128], *dev;
	Subfont *df;
	Display *display;
	DDisplay *dd;
	Heap *h;
	Draw_Rect r;
	DRef *dr;
	Cache *c;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;
	if(cacheqlock == nil){
		cacheqlock = libqlalloc();
		if(cacheqlock == nil)
			return;
	}
	dev = string2c(f->dev);
	if(dev[0] == 0)
		dev = 0;
	display = initdisplay(dev);
	if(display == 0)
		return;

	dr = malloc(sizeof(DRef));
	if(dr == nil)
		return;
	h = heap(TDisplay);
	if(h == H){
		closedisplay(display);
		return;
	}
	dd = H2D(DDisplay*, h);
	dd->display = display;
	*f->ret = &dd->drawdisplay;
	dd->dref = dr;
	display->limbo = dr;
	dr->display = display;
	dr->ref = 1;
	df = getdefont(display);
	if(df){
		display->defaultsubfont = df;
		sprint(buf, "%d %d\n0 %d\t%s\n", df->height, df->ascent,
			df->n-1, deffontname);
		display->defaultfont = buildfont(display, buf, deffontname, 0);
		if(display->defaultfont){
			c = cacheinstall(fcache, display, deffontname, display->defaultfont, "font");
			if(c)
				c->ref++;
			/* else BUG? */
		}
	}

	R2R(r, display->image->r);
	dd->drawdisplay.image = allocdrawimage(dd, r, display->image->ldepth, display->image, 0, 0);
	R2R(r, display->ones->r);
	dd->drawdisplay.ones = allocdrawimage(dd, r, display->ones->ldepth, display->ones, 1, 0);
	dd->drawdisplay.zeros = allocdrawimage(dd, r, display->zeros->ldepth, display->zeros, 1, 0);

	/* don't call unlockdisplay because the qlock was left up by initdisplay */
	libqunlock(display->qlock);
}

void
Display_startrefresh(void *fp)
{
	F_Display_startrefresh *f;
	Display *disp;

	f = fp;
	disp = checkdisplay(f->d);
	refreshslave(disp);
}

void
display_dec(void *v)
{
	DRef *dr;
	Display *d;
	int locked;

	dr = v;
	if(dr->ref-- != 1)
		return;

	d = dr->display;
	locked = lockdisplay(d, 1);
	font_close(d->defaultfont);
	subfont_close(d->defaultsubfont);
	if(locked)
		unlockdisplay(d);
	freeallsubfonts(d);
	closedisplay(d);
	free(dr);
}

void
freedrawdisplay(Heap *h, int swept)
{
	DDisplay *dd;
	Display *d;

	dd = H2D(DDisplay*, h);

	if(!swept) {
		destroy(dd->drawdisplay.image);
		destroy(dd->drawdisplay.ones);
		destroy(dd->drawdisplay.zeros);
	}
	d = dd->display;
	/* we've now released dd->image etc.; make sure they're not freed again */
	d->image = nil;
	d->ones = nil;
	d->zeros = nil;
	display_dec(dd->dref);
	/* Draw_Display header will be freed by caller */
}

void
Display_color(void *fp)
{
	F_Display_color *f;
	Display *d;
	int locked;

	f = fp;
	d = checkdisplay(f->d);
	locked = lockdisplay(d, 0);
	destroy(*f->ret);
	*f->ret = color((DDisplay*)f->d, f->color);
	if(locked)
		unlockdisplay(d);
}

void
Image_flush(void *fp)
{
	F_Image_flush *f;
	Image *d;
	DImage *di;
	int locked;

	f = fp;
	d = checkimage(f->win);
	di = (DImage*)f->win;
	switch(f->func){
	case 0:	/* Draw->Flushoff */
		di->flush = 0;
		break;
	case 1:	/* Draw->Flushon */
		di->flush = 1;
		/* fall through */
	case 2:	/* Draw->Flushnow */
		locked = lockdisplay(d->display, 0);
		if(d->id==0 || d->screen!=0)
			flushimage(d->display, 1);
		if(locked)
			unlockdisplay(d->display);
		break;
	default:
		error(exInval);
	}
}

void
checkflush(Draw_Image *dst)
{
	DImage  *di;

	di = (DImage*)dst;
	if(di->flush && (di->image->id==0 || di->image->screen!=nil))
		flushimage(di->image->display, 1);
}

void
Image_draw(void *fp)
{
	F_Image_draw *f;
	Image *d, *s, *m;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(f->mask == H)
		m = d->display->ones;
	else
		m = checkimage(f->mask);
	if(d->display!=s->display || d->display!=m->display)
		return;
	locked = lockdisplay(d->display, 0);
	draw(d, IRECT(f->r), s, m, IPOINT(f->p));
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_gendraw(void *fp)
{
	F_Image_gendraw *f;
	Image *d, *s, *m;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(f->mask == H)
		m = d->display->ones;
	else
		m = checkimage(f->mask);
	if(d->display!=s->display || d->display!=m->display)
		return;
	locked = lockdisplay(d->display, 0);
	gendraw(d, IRECT(f->r), s, IPOINT(f->p0), m, IPOINT(f->p1));
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_line(void *fp)
{
	F_Image_line *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display || f->radius < 0)
		return;
	locked = lockdisplay(d->display, 0);
	line(d, IPOINT(f->p0), IPOINT(f->p1), f->end0, f->end1, f->radius, s, IPOINT(f->sp));
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_splinepoly(void *fp, int smooth)
{
	F_Image_poly *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display|| f->radius < 0)
		return;
	locked = lockdisplay(d->display, 0);
	/* sleazy: we know that Draw_Points have same shape as Points */
	if(smooth)
		bezspline(d, (Point*)f->p->data, f->p->len,
			f->end0, f->end1, f->radius, s, IPOINT(f->sp));
	else
		poly(d, (Point*)f->p->data, f->p->len, f->end0,
			f->end1, f->radius, s, IPOINT(f->sp));
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_poly(void *fp)
{
	Image_splinepoly(fp, 0);
}

void
Image_bezspline(void *fp)
{
	Image_splinepoly(fp, 1);
}

void
Image_bezier(void *fp)
{
	F_Image_bezier *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display || f->radius < 0)
		return;
	locked = lockdisplay(d->display, 0);
	bezier(d, IPOINT(f->a), IPOINT(f->b), IPOINT(f->c),
		  IPOINT(f->d), f->end0, f->end1, f->radius, s, IPOINT(f->sp));
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_fillbezier(void *fp)
{
	F_Image_fillbezier *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display)
		return;
	locked = lockdisplay(d->display, 0);
	fillbezier(d, IPOINT(f->a), IPOINT(f->b), IPOINT(f->c),
			IPOINT(f->d), f->wind, s, IPOINT(f->sp));
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_fillsplinepoly(void *fp, int smooth)
{
	F_Image_fillpoly *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display)
		return;
	locked = lockdisplay(d->display, 0);
	/* sleazy: we know that Draw_Points have same shape as Points */
	if(smooth)
		fillbezspline(d, (Point*)f->p->data, f->p->len,
			f->wind, s, IPOINT(f->sp));
	else
		fillpoly(d, (Point*)f->p->data, f->p->len,
			f->wind, s, IPOINT(f->sp));
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_fillpoly(void *fp)
{
	Image_fillsplinepoly(fp, 0);
}

void
Image_fillbezspline(void *fp)
{
	Image_fillsplinepoly(fp, 1);
}

void
arcellipse(void *fp, int isarc, int alpha, int phi)
{
	F_Image_arc *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display || f->thick < 0 || f->a<0 || f->b<0)
		return;

	locked = lockdisplay(d->display, 0);
	if(isarc)
		arc(d, IPOINT(f->c), f->a, f->b, f->thick, s,
			IPOINT(f->sp), alpha, phi);
	else
		ellipse(d, IPOINT(f->c), f->a, f->b, f->thick, s,
			IPOINT(f->sp));
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_ellipse(void *fp)
{
	arcellipse(fp, 0, 0, 0);
}

void
Image_arc(void *fp)
{
	F_Image_arc *f;

	f = fp;
	arcellipse(fp, 1, f->alpha, f->phi);
}

void
fillarcellipse(void *fp, int isarc, int alpha, int phi)
{
	F_Image_fillarc *f;
	Image *d, *s;
	int locked;

	f = fp;
	d = checkimage(f->dst);
	s = checkimage(f->src);
	if(d->display != s->display || f->a<0 || f->b<0)
		return;

	locked = lockdisplay(d->display, 0);
	if(isarc)
		fillarc(d, IPOINT(f->c), f->a, f->b, s, IPOINT(f->sp), alpha, phi);
	else
		fillellipse(d, IPOINT(f->c), f->a, f->b, s, IPOINT(f->sp));
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
}

void
Image_fillellipse(void *fp)
{
	fillarcellipse(fp, 0, 0, 0);
}

void
Image_fillarc(void *fp)
{
	F_Image_fillarc *f;

	f = fp;
	fillarcellipse(fp, 1, f->alpha, f->phi);
}

void
Image_text(void *fp)
{
	F_Image_text *f;
	Font *font;
	char *str;
	Point pt;
	Image *s, *d;
	int locked;

	f = fp;
	if(f->dst == H || f->src == H)
		goto Return;
	if(f->font == H || f->str == H)
		goto Return;
	str = string2c(f->str);
	d = checkimage(f->dst);
	s = checkimage(f->src);
	font = checkfont(f->font);
	if(d->display!=s->display || d->display!=font->display)
		return;
	locked = lockdisplay(d->display, 0);
	pt = string(d, IPOINT(f->p), s, IPOINT(f->sp), font, str);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(d->display);
    Return:
	P2P(*f->ret, pt);
}

void
Display_newimage(void *fp)
{
	F_Display_newimage *f;
	Display *d;
	int locked;

	f = fp;
	d = checkdisplay(f->d);
	locked = lockdisplay(d, 0);
	destroy(*f->ret);
	*f->ret = H;
	*f->ret = allocdrawimage((DDisplay*)f->d, f->r, f->ldepth,
				nil, f->repl, f->color);
	if(locked)
		unlockdisplay(d);
}

void
Image_readpixels(void *fp)
{
	F_Image_readpixels *f;
	Rectangle r;
	Image *i;
	int locked;

	f = fp;
	R2R(r, f->r);
	i = checkimage(f->src);
	locked = lockdisplay(i->display, 0);
	*f->ret = unloadimage(i, r, f->data->data, f->data->len);
	if(locked)
		unlockdisplay(i->display);
}

void
Image_writepixels(void *fp)
{
	Rectangle r;
	F_Image_writepixels *f;
	Image *i;
	int locked;

	f = fp;
	R2R(r, f->r);
	i = checkimage(f->dst);
	locked = lockdisplay(i->display, 0);
	*f->ret = loadimage(i, r, f->data->data, f->data->len);
	checkflush(f->dst);
	if(locked)
		unlockdisplay(i->display);
}

void
Image_arrow(void *fp)
{
	F_Image_arrow *f;

	f = fp;
	*f->ret = ARROW(f->a, f->b, f->c);
}


#define CHUNK 8000

Image*
display_open(Display *disp, char *name)
{
	Image *i;
	int fd;

	fd = libopen(name, OREAD);
	if(fd < 0)
		return nil;

	i = readimage(disp, fd, 1);
	libclose(fd);
	return i;
}

void
Display_open(void *fp)
{
	Image *i;
	Display *disp;
	F_Display_open *f;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;
	disp = lookupdisplay(f->d);
	if(disp == nil)
		return;
	i = display_open(disp, string2c(f->name));
	if(i == nil)
		return;
	*f->ret = allocdrawimage((DDisplay*)f->d, DRECT(i->r), i->ldepth, i, 0, 0);
}

void
Display_readimage(void *fp)
{
	Image *i;
	Display *disp;
	F_Display_readimage *f;
	Sys_FD *fd;
	int locked;

	f = fp;
	destroy(*f->ret);
	*f->ret = H;
	fd = f->fd;
	if(fd == H)
		return;
	disp = checkdisplay(f->d);
	i = readimage(disp, fd->fd, 1);
	if(i == nil)
		return;
	*f->ret = allocdrawimage((DDisplay*)f->d, DRECT(i->r), i->ldepth, i, 0, 0);
	if(*f->ret == H){
		locked = lockdisplay(disp, 0);
		freeimage(i);
		if(locked)
			unlockdisplay(disp);
	}
}

void
Display_writeimage(void *fp)
{
	Image *i;
	F_Display_writeimage *f;
	Sys_FD *fd;

	f = fp;
	*f->ret = -1;
	fd = f->fd;
	if(fd == H)
		return;
	i = checkimage(f->i);
	if(checkdisplay(f->d) != i->display)
		return;
	*f->ret = writeimage(fd->fd, i);
}

void
Display_cursor(void *fp)
{
	Image *i;
	F_Display_cursor *f;

	f = fp;
	*f->ret = -1;
	
	i = checkimage(f->i);
	   
	if(checkdisplay(f->d) != i->display)	
		return;
	/* sleazy: we know that Draw_Points have same shape as Points */
	cursor(*(Point *)f->p, i);
	*f->ret = 0;	
}

void
Screen_allocate(void *fp)
{
	F_Screen_allocate *f;
	Heap *h;
	Screen *s;
	DScreen *ds;
	Image *image, *fill;
	int locked;

	f = fp;
	image = checkimage(f->image);
	fill = checkimage(f->fill);
	locked = lockdisplay(image->display, 0);
	destroy(*f->ret);
	*f->ret = H;
	s = allocscreen(image, fill, f->public);
	if(s == 0)
		goto Return;
	h = heap(TScreen);
	if(h == H)
		goto Return;
	ds = H2D(DScreen*, h);
	ds->screen = s;
	ds->drawscreen.fill = f->fill;
	D2H(f->fill)->ref++;
	ds->drawscreen.image = f->image;
	D2H(f->image)->ref++;
	ds->drawscreen.display = f->image->display;
	D2H(f->image->display)->ref++;
	ds->drawscreen.id = s->id;
	ds->dref = image->display->limbo;
	ds->dref->ref++;

	*f->ret = &ds->drawscreen;
    Return:
	if(locked)
		unlockdisplay(image->display);
	return;
}

void
Display_publicscreen(void *fp)
{
	F_Display_publicscreen *f;
	Heap *h;
	Screen *s;
	DScreen *ds;
	Display *disp;
	int locked;

	f = fp;
	disp = checkdisplay(f->d);
	locked = lockdisplay(disp, 0);
	destroy(*f->ret);
	*f->ret = H;
	s = publicscreen(disp, f->id, disp->image->ldepth);
	if(locked)
		unlockdisplay(disp);
	if(s == nil)
		return;
	h = heap(TScreen);
	if(h == H)
		return;
	ds = H2D(DScreen*, h);
	ds->screen = s;
	ds->drawscreen.fill = H;
	ds->drawscreen.image =H;
	ds->drawscreen.id = s->id;
	ds->drawscreen.display = f->d;
	D2H(f->d)->ref++;
	ds->dref = disp->limbo;
	ds->dref->ref++;
	*f->ret = &ds->drawscreen;
}

void
freedrawscreen(Heap *h, int swept)
{
	DScreen *ds;
	Screen *s;
	Display *disp;
	int locked;

	ds = H2D(DScreen*, h);
	if(!swept) {
		destroy(ds->drawscreen.image);
		destroy(ds->drawscreen.fill);
		destroy(ds->drawscreen.display);
	}
	s = lookupscreen(&ds->drawscreen);
	if(s == nil){
		if(!swept && TScreen->np)
			freeptrs(ds, TScreen);
		return;
	}
	disp = s->display;
	locked = lockdisplay(disp, 1);
	freescreen(s);
	if(locked)
		unlockdisplay(disp);
	display_dec(ds->dref);
	/* screen header will be freed by caller */
}

void
Font_build(void *fp)
{
	F_Font_build *f;
	Font *font;
	DFont *dfont;
	Heap *h;
	char buf[128];
	char *name, *data;
	Subfont *df;
	Display *disp;
	int locked;

	f = fp;
	disp = checkdisplay(f->d);
	destroy(*f->ret);
	*f->ret = H;

	name = string2c(f->name);
	font = font_open(disp, name);
	if(font == nil) {
		if(strcmp(name, deffontname) == 0) {
			df = disp->defaultsubfont;
			sprint(buf, "%d %d\n0 %d\t%s\n",
				df->height, df->ascent, df->n-1, name);
			data = buf;
		}
		else
		if(f->desc == H)
			return;
		else
			data = string2c(f->desc);

		locked = lockdisplay(disp, 0);
		/* BUG: 0 might not be right sometimes */
		font = buildfont(disp, data, name, 0);
		if(locked)
			unlockdisplay(disp);
		if(font == nil)
			return;
	}

	h = heap(TFont);
	if(h == H)
		return;

	dfont = H2D(DFont*, h);
	dfont->font = font;
	dfont->drawfont.name = f->name;
	D2H(f->name)->ref++;
	dfont->drawfont.height = font->height;
	dfont->drawfont.ascent = font->ascent;
	dfont->drawfont.display = f->d;
	D2H(f->d)->ref++;
	dfont->dref = disp->limbo;
	dfont->dref->ref++;

	*f->ret = &dfont->drawfont;
}

Font*
font_open(Display *display, char *name)
{
	Cache *c;
	Font *font;
	int locked;

	c = cachelookup(fcache, display, name);
	if(c)
		font = c->u.f;
	else {
		locked = lockdisplay(display, 0);
		/* BUG: sometimes ldepth 0 is wrong */
		font = openfont(display, name, 0);
		if(locked)
			unlockdisplay(display);
		if(font == nil)
			return nil;
		c = cacheinstall(fcache, display, name, font, "font");
	}
	if(c)
		c->ref++;

	return font;
}

void
font_close(Font *f)
{
	Cache *c;
	Display *disp;
	int locked;

	disp = f->display;
	if(f->name == nil)
		return;

	c = cachelookup(fcache, disp, f->name);
	if(c==nil || c->ref<=0)
 		return;

 	c->ref--;
	if(c->ref == 0) {
		cacheuninstall(fcache, disp, f->name, "font");
		locked = lockdisplay(disp, 1);
		freefont(f);
		if(locked)
			unlockdisplay(disp);
	}
}

void
freecachedsubfont(Subfont *sf)
{
	Cache *c;
	Display *disp;

	disp = sf->bits->display;
	c = cachelookup(sfcache, disp, sf->name);
	if(c == nil){
		fprint(2, "subfont %s not cached\n", sf->name);
		return;
	}
	if(c->ref > 0)
		c->ref--;
	/* if ref is zero, we leave it around for later harvesting by freeallsubfonts */
}

void
freeallsubfonts(Display *d)
{
	int i;
	Cache *c, *prev, *o;
	Subfont *sf;
	int locked;

	if(cacheqlock == nil)	/* may not have allocated anything yet */
		return;
	libqlock(cacheqlock);
	for(i=0; i<BIHASH; i++){
		c = sfcache[i];
		prev = 0;
		while(c != nil){
			if(c->ref==0 && (d==nil || c->display==d)){
				if(prev == 0)
					sfcache[i] = c->next;
				else
					prev->next = c->next;
				free(c->name);
				sf = c->u.sf;
				free(sf->info);
				locked = lockdisplay(c->display, 1);
				freeimage(sf->bits);
				if(locked)
					unlockdisplay(c->display);
				free(sf);
				o = c;
				c = c->next;
				free(o);
			}else{
				prev = c;
				c = c->next;
			}
		}
	}
	libqunlock(cacheqlock);
}

void
subfont_close(Subfont *sf)
{
	freecachedsubfont(sf);
}

void
freesubfont(Subfont *sf)
{
	freecachedsubfont(sf);
}

void
Font_open(void *fp)
{
	Heap *h;
	Font *font;
	Display *disp;
	DFont *df;
	F_Font_open *f;

	f = fp;

	disp = checkdisplay(f->d);
	destroy(*f->ret);
	*f->ret = H;

	font = font_open(disp, string2c(f->name));
	if(font == 0)
		return;

	h = heap(TFont);
	if(h == H)
		return;

	df = H2D(DFont*, h);
	df->font = font;
	df->drawfont.name = f->name;
	D2H(f->name)->ref++;
	df->drawfont.height = font->height;
	df->drawfont.ascent = font->ascent;
	df->drawfont.display = f->d;
	D2H(f->d)->ref++;
	df->dref = disp->limbo;
	df->dref->ref++;
	*f->ret = &df->drawfont;
}

void
Font_width(void *fp)
{
	F_Font_width *f;
	Font *font;
	char *s;
	int locked;

	f = fp;
	s = string2c(f->str);
	if(f->f == H || s[0]=='\0')
		*f->ret = 0;
	else{
		font = checkfont(f->f);
		locked = lockdisplay(font->display, 0);
		*f->ret = stringwidth(font, s);
		if(locked)
			unlockdisplay(font->display);
	}
}

/*
 * BUG: would be nice if this cached the whole font.
 * Instead only the subfonts are cached and the fonts are
 * freed when released.
 */
void
freedrawfont(Heap*h, int swept)
{
	Draw_Font *d;
	Font *f;

	d = H2D(Draw_Font*, h);
	f = lookupfont(d);
	if(!swept) {
		destroy(d->name);
		destroy(d->display);
	}
	font_close(f);
	display_dec(((DFont*)d)->dref);
}

void
Display_rgb(void *fp)
{
	int c;
	Display *disp;
	F_Display_rgb *f;
	int locked;

	f = fp;
	disp = checkdisplay(f->d);

	c = rgb2cmap(f->r, f->g, f->b);

	locked = lockdisplay(disp, 0);
	destroy(*f->ret);
	*f->ret = H;
	*f->ret = color((DDisplay*)f->d, c);
	if(locked)
		unlockdisplay(disp);
}

void
Display_rgb2cmap(void *fp)
{
	F_Display_rgb2cmap *f;

	f = fp;
	/* f->display is unused, but someday may have color map */
	*f->ret = rgb2cmap(f->r, f->g, f->b);
}

void
Display_cmap2rgb(void *fp)
{
	F_Display_cmap2rgb *f;
	ulong c;

	f = fp;
	/* f->display is unused, but someday may have color map */
	c = cmap2rgb(f->c);
	f->ret->t0 = (c>>16)&0xFF;
	f->ret->t1 = (c>>8)&0xFF;
	f->ret->t2 = (c>>0)&0xFF;
}

Draw_Image*
color(DDisplay *dd, int color)
{
	Draw_Rect r;

	r.min.x = 0;
	r.min.y = 0;
	r.max.x = 1;
	r.max.y = 1;
	return allocdrawimage(dd, r, 3, nil, 1, color);
}

Draw_Image*
mkdrawimage(Image *i, Draw_Screen *screen, Draw_Display *display, void *ref)
{
	Heap *h;
	DImage *di;

	h = heap(TImage);
	if(h == H)
		return H;

	di = H2D(DImage*, h);
	di->image = i;
	di->drawimage.screen = screen;
	if(screen != H)
		D2H(screen)->ref++;
	di->drawimage.display = display;
	if(display != H)
		D2H(display)->ref++;
	di->refreshptr = ref;

	R2R(di->drawimage.r, i->r);
	R2R(di->drawimage.clipr, i->clipr);
	di->drawimage.ldepth = i->ldepth;
	di->drawimage.repl = i->repl;
	di->flush = 1;
	di->dref = i->display->limbo;
	di->dref->ref++;
	return &di->drawimage;
}

void
Screen_newwindow(void *fp)
{
	F_Screen_newwindow *f;
	Image *i;
	Screen *s;
	Rectangle r;
	int locked;

	f = fp;
	s = checkscreen(f->screen);
	R2R(r, f->r);

	locked = lockdisplay(s->display, 0);
	destroy(*f->ret);
	*f->ret = H;

	i = allocwindow(s, r, 0, 0, f->color);
	if(locked)
		unlockdisplay(s->display);
	if(i == nil)
		return;

	*f->ret = mkdrawimage(i, f->screen, f->screen->display, 0);
}

void
Screen_top(void *fp)
{
	F_Screen_top *f;
	Screen *s;
	Array *array;
	Draw_Image **di;
	Image **ip;
	int i, n, locked;

	f = fp;
	s = checkscreen(f->screen);
	array = f->wins;
	di = (Draw_Image**)array->data;
	ip = malloc(array->len * sizeof(Image*));
	if(ip == nil)
		return;
	n = 0;
	for(i=0; i<array->len; i++)
		if(di[i] != H){
			ip[n] = lookupimage(di[i]);
			if(ip[n]==nil || ip[n]->screen != s){
				free(ip);
				return;
			}
			n++;
		}
	if(n == 0){
		free(ip);
		return;
	}
	locked = lockdisplay(s->display, 0);
	topnwindows(ip, n);
	free(ip);
	flushimage(s->display, 1);
	if(locked)
		unlockdisplay(s->display);
}

void
freedrawimage(Heap *h, int swept)
{
	Image *i;
	int locked;
	Display *disp;
	Draw_Image *d;

	d = H2D(Draw_Image*, h);
	i = lookupimage(d);
	if(i == nil) {
		if(!swept && TImage->np)
			freeptrs(d, TImage);
		return;
	}
	disp = i->display;
	locked = lockdisplay(disp, 1);
	freeimage(i);
	if(locked)
		unlockdisplay(disp);
	display_dec(((DImage*)d)->dref);
	/* image/layer header will be freed by caller */
}

void
Image_top(void *fp)
{
	F_Image_top *f;
	Image *i;
	int locked;

	f = fp;
	i = checkimage(f->win);
	locked = lockdisplay(i->display, 0);
	topwindow(i);
	flushimage(i->display, 1);
	if(locked)
		unlockdisplay(i->display);
}

void
Image_origin(void *fp)
{
	F_Image_origin *f;
	Image *i;
	int locked;

	f = fp;
	i = checkimage(f->win);
	if(i->screen == nil)
		*f->ret = -1;
	else{
		locked = lockdisplay(i->display, 0);
		if(originwindow(i, IPOINT(f->log), IPOINT(f->scr)) < 0)
			*f->ret = -1;
		else{
			f->win->r = DRECT(i->r);
			f->win->clipr = DRECT(i->clipr);
			*f->ret = 1;
		}
		if(locked)
			unlockdisplay(i->display);
	}
}

void
Image_bottom(void *fp)
{
	F_Image_top *f;
	Image *i;
	int locked;

	f = fp;
	i = checkimage(f->win);
	locked = lockdisplay(i->display, 0);
	bottomwindow(i);
	flushimage(i->display, 1);
	if(locked)
		unlockdisplay(i->display);
}

Draw_Image*
allocdrawimage(DDisplay *ddisplay, Draw_Rect r, int ld, Image *iimage, int repl, int color)
{
	Heap *h;
	DImage *di;
	Rectangle rr;
	Image *image;

	if(ld < 0 || ld > 3)
		return H;

	image = iimage;
	if(iimage == nil){
		R2R(rr, r);
		image = allocimage(ddisplay->display, rr, ld, repl, color);
		if(image == 0)
			return H;
	}

	h = heap(TImage);
	if(h == H){
		if(iimage == nil)
			freeimage(image);
		return H;
	}

	di = H2D(DImage*, h);
	di->drawimage.r = r;
	R2R(di->drawimage.clipr, image->clipr);
	di->drawimage.ldepth = ld;
	di->drawimage.repl = repl;
	di->drawimage.display = (Draw_Display*)ddisplay;
	D2H(di->drawimage.display)->ref++;
	di->drawimage.screen = H;
	di->dref = ddisplay->display->limbo;
	di->dref->ref++;
	di->image = image;
	di->refreshptr = 0;
	di->flush = 1;

	return &di->drawimage;
}

/*
 * Entry points called from the draw library
 */
Subfont*
lookupsubfont(Display *d, char *name)
{
	Cache *c;

	c = cachelookup(sfcache, d, name);
	if(c == nil)
		return nil;
	return c->u.sf;
}

void
installsubfont(char *name, Subfont *subfont)
{
	Cache *c;

	c = cacheinstall(sfcache, subfont->bits->display, name, subfont, "subfont");
	if(c)
		c->ref++;
}

/*
 * BUG version
 */
char*
subfontname(char *cfname, char *fname, int ldepth)
{
	char *t, *u, tmp1[256], tmp2[256];

	USED(ldepth);
	if(strcmp(cfname, deffontname) == 0)
		return strdup(cfname);
	t = cfname;
	if(t[0] != '/'){
		strcpy(tmp2, fname);
		u = utfrrune(tmp2, '/');
		if(u)
			u[0] = 0;
		else
			strcpy(tmp2, ".");
		sprint(tmp1, "%s/%s", tmp2, t);
		t = tmp1;
	}
	return strdup(t);
}

void
refreshslave(Display *d)
{
	int i, n, id;
	uchar buf[5*(5*4)], *p;
	Rectangle r;
	Image *im;
	int locked;

	for(;;){
		release();
		n = kchanio(d->refchan, buf, sizeof buf, OREAD);
		acquire();
		if(n < 0)	/* probably caused by closedisplay() closing refchan */
			return;	/* will fall off end of thread and close down */
		locked = lockdisplay(d, 0);
		p = buf;
		for(i=0; i<n; i+=5*4,p+=5*4){
			id = BGLONG(p+0*4);
			r.min.x = BGLONG(p+1*4);
			r.min.y = BGLONG(p+2*4);
			r.max.x = BGLONG(p+3*4);
			r.max.y = BGLONG(p+4*4);
			for(im=d->windows; im; im=im->next)
				if(im->id == id)
					break;
			if(im && im->screen && im->reffn)
				(*im->reffn)(im, r, im->refptr);
		}
		flushimage(d, 1);
		if(locked)
			unlockdisplay(d);
	}
}

void
startrefresh(Display *disp)
{
	USED(disp);
}

static
int
doflush(Display *d)
{
	int m, n;
	char err[ERRLEN];

	n = d->bufp-d->buf;
	if(n <= 0)
		return 1;

	if(d->local == 0)
		release();
	if((m = kchanio(d->datachan, d->buf, n, OWRITE)) != n){
		if(d->local == 0)
			acquire();
		err[0] = 0;
		kgerrstr(err);
		fprint(2, "flushimage fail: (%d not %d) d=%lux: %s\n", m, n, d, err);
		d->bufp = d->buf;	/* might as well; chance of continuing */
		return -1;
	}
	d->bufp = d->buf;
	if(d->local == 0)
		acquire();
	return 1;
}

int
flushimage(Display *d, int visible)
{
	int ret;
	Refreshq *r;

	for(;;){
		if(visible)
			*d->bufp++ = 'v';	/* one byte always reserved for this */
		ret = doflush(d);
		if(d->refhead == nil)
			break;
		while(r = d->refhead){	/* assign = */
			d->refhead = r->next;
			if(d->refhead == nil)
				d->reftail = nil;
			r->reffn(nil, r->r, r->refptr);
			free(r);
		}
	}

	return ret;
}

/*
 * Turn off refresh for this window and remove any pending refresh events for it.
 */
void
delrefresh(Image *i)
{
	Refreshq *r, *prev, *next;
	int locked;
	Display *d;
	void *refptr;

	d = i->display;
	/*
	 * Any refresh function will do, because the data pointer is nil.
	 * Can't use nil, though, because that turns backing store back on.
	 */
	if(d->local)
		drawlsetrefresh(d->dataqid, i->id, memlnorefresh, nil);
	refptr = i->refptr;
	i->refptr = nil;
	if(d->refhead==nil || refptr==nil)
		return;
	locked = lockdisplay(d, 1);
	prev = nil;
	for(r=d->refhead; r; r=next){
		next = r->next;
		if(r->refptr == refptr){
			if(prev)
				prev->next = next;
			else
				d->refhead = next;
			if(d->reftail == r)
				d->reftail = prev;
			free(r);
		}else
			prev = r;
	}
	if(locked)
		unlockdisplay(d);
}

void
queuerefresh(Image *i, Rectangle r, Reffn reffn, void *refptr)
{
	Display *d;
	Refreshq *rq;

	d = i->display;
	rq = malloc(sizeof(Refreshq));
	if(rq == nil)
		return;
	if(d->reftail)
		d->reftail->next = rq;
	else
		d->refhead = rq;
	d->reftail = rq;
	rq->reffn = reffn;
	rq->refptr = refptr;
	rq->r = r;
}

uchar*
bufimage(Display *d, int n)
{
	uchar *p;

	if(n<0 || n>Displaybufsize){
		kwerrstr("bad count in bufimage");
		return 0;
	}
	if(d->bufp+n > d->buf+Displaybufsize){
		if(d->local==0 && currun()!=libqlowner(d->qlock)) {
			print("bufimage: %lux %lux\n", libqlowner(d->qlock), currun());
			abort();
		}
		if(doflush(d) < 0)
			return 0;
	}
	p = d->bufp;
	d->bufp += n;
	/* return with buffer locked */
	return p;
}
