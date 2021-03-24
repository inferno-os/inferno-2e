#include	"u.h"
#include	"../port/lib.h"
#include	"mem.h"
#include	"dat.h"
#include	"fns.h"
#include	"../port/error.h"

#include	"image.h"
#include	<memimage.h>
#include	<memlayer.h>
#include	<cursor.h>

enum
{
	Qtopdir		= 0,
	Q2nd,
	Qnew,
	Q3rd,
	Qctl,
	Qdata,
	Qrefresh,
};

/*
 * Qid path is:
 *	 3 bits of file type (qids above)
 *	24 bits of mux slot number +1; 0 means not attached to client
 */
#define	QSHIFT	3	/* location in qid of client # */

#define	QID(q)		(((q).path&0x00000007)>>0)
#define	CLIENTPATH(q)	((q&0x07FFFFFF8)>>QSHIFT)
#define	CLIENT(q)	CLIENTPATH((q).path)

#define	NHASH		(1<<5)
#define	HASHMASK	(NHASH-1)

typedef struct Client Client;
typedef struct Draw Draw;
typedef struct DImage DImage;
typedef struct DScreen DScreen;
typedef struct CScreen CScreen;
typedef struct FChar FChar;
typedef struct Refresh Refresh;
typedef struct Refx Refx;

struct Draw
{
	QLock;
	int		clientid;
	int		nclient;
	Client**	client;
	int		softscreen;
};

struct Client
{
	Ref		r;
	DImage*		dimage[NHASH];
	CScreen*	cscreen;
	Refresh*	refresh;
	Rendez		refrend;
	uchar*		readdata;
	int		nreaddata;
	int		busy;
	int		clientid;
	int		slot;
	int		refreshme;
};

struct Refresh
{
	DImage*		dimage;
	Rectangle	r;
	Refresh*	next;
};

struct Refx
{
	Client*		client;
	DImage*		dimage;
};

struct FChar
{
	int		minx;	/* left edge of bits */
	int		maxx;	/* right edge of bits */
	uchar		miny;	/* first non-zero scan-line */
	uchar		maxy;	/* last non-zero scan-line + 1 */
	schar		left;	/* offset of baseline */
	uchar		width;	/* width of baseline */
};

struct DImage
{
	int		id;
	Memimage*	image;
	int		ascent;
	int		nfchar;
	FChar*		fchar;
	DScreen*	dscreen;	/* 0 if not a window */
	DImage*		next;
};

struct CScreen
{
	DScreen*	dscreen;
	CScreen*	next;
};

struct DScreen
{
	int		id;
	int		public;
	int		ref;
	Memscreen*	screen;
	Client*		owner;
	DScreen*	next;
	int		imageid;	/* to be freed when screen is freed */
	int		fillid;	/* to be freed when screen is freed */
};

static	Draw		sdraw;
static	Memimage	screenimage;
static	Memdata	screendata;
static	Rectangle	flushrect;
static	int		waste;
static	Rectangle	cursornogo;
static	DScreen*	dscreen;
static  Drawcursor	curs;
extern	void		flushmemscreen(Rectangle);
extern	void		cursorupdate(Rectangle);
	void		drawmesg(Client*, void*, int);
	void		drawuninstall(Client*, int);

static	char Enodrawimage[] =	"unknown id for draw image";
static	char Enodrawscreen[] =	"unknown id for draw screen";
static	char Eshortdraw[] =	"short draw message";
static	char Eshortread[] =	"draw read too short";
static	char Eimageexists[] =	"image id in use";
static	char Escreenexists[] =	"screen id in use";
static	char Edrawmem[] =	"image memory allocation failed";
static	char Ereadoutside[] =	"readimage outside image";
static	char Ewriteoutside[] =	"writeimage outside image";
static	char Enotfont[] =	"image not a font";
static	char Eindex[] =		"character index out of range";
static	char Enoclient[] =	"no such draw client";
static	char Eldepth[] =	"image has bad ldepth";

static int
drawgen(Chan *c, Dirtab *tab, int x, int s, Dir *dp)
{
	int t;
	Qid q;
	ulong path;
	Client *cl;
	char buf[NAMELEN];

	USED(tab, x);
	q.vers = 0;

	/*
	 * Top level directory contains the name of the device.
	 */
	if(c->qid.path == CHDIR){
		switch(s){
		case 0:
			q = (Qid){CHDIR|Q2nd, 0};
			devdir(c, q, "draw", 0, eve, 0555, dp);
			break;
		default:
			return -1;
		}
		return 1;
	}

	/*
	 * Second level contains "new" plus all the clients.
	 */
	t = QID(c->qid);
	if(t == Q2nd || t == Qnew){
		if(s == 0){
			q = (Qid){Qnew, 0};
			devdir(c, q, "new", 0, eve, 0666, dp);
		}
		else if(s <= sdraw.nclient){
			cl = sdraw.client[s-1];
			if(cl == 0)
				return 0;
			sprint(buf, "%d", cl->clientid);
			q = (Qid){CHDIR|(s<<QSHIFT)|Q3rd, 0};
			devdir(c, q, buf, 0, eve, 0555, dp);
			return 1;
		}
		else
			return -1;
		return 1;
	}

	/*
	 * Third level.
	 */
	path = c->qid.path&~(CHDIR|((1<<QSHIFT)-1));	/* slot component */
	q.vers = c->qid.vers;
	switch(s){
	case 0:
		q = (Qid){path|Qctl, c->qid.vers};
		devdir(c, q, "ctl", 0, eve, 0600, dp);
		break;
	case 1:
		q = (Qid){path|Qdata, c->qid.vers};
		devdir(c, q, "data", 0, eve, 0600, dp);
		break;
	case 2:
		q = (Qid){path|Qrefresh, c->qid.vers};
		devdir(c, q, "refresh", 0, eve, 0400, dp);
		break;
	default:
		return -1;
	}
	return 1;
}

static
void
bbox(Rectangle *r1, Rectangle *r2)
{
	if(r1->min.x > r2->min.x)
		r1->min.x = r2->min.x;
	if(r1->min.y > r2->min.y)
		r1->min.y = r2->min.y;
	if(r1->max.x < r2->max.x)
		r1->max.x = r2->max.x;
	if(r1->max.y < r2->max.y)
		r1->max.y = r2->max.y;
}

static
int
drawrefactive(void *a)
{
	Client *c;

	c = a;
	return c->refreshme || c->refresh!=0;
}

static
void
drawrefresh(Memimage *l, Rectangle r, void *v)
{
	Refx *x;
	DImage *d;
	Client *c;
	Refresh *ref;

	USED(l);
	if(v == 0)
		return;
	x = v;
	c = x->client;
	d = x->dimage;
	for(ref=c->refresh; ref; ref=ref->next)
		if(ref->dimage == d){
			bbox(&ref->r, &r);
			return;
		}
	ref = malloc(sizeof(Refresh));
	if(ref){
		ref->dimage = d;
		ref->r = r;
		ref->next = c->refresh;
		c->refresh = ref;
	}
}

static
void
addflush(Rectangle r)
{
	int abb, ar, anbb;
	Rectangle nbb;

	if(sdraw.softscreen==0 || !rectclip(&r, screenimage.r))
		return;

	if(flushrect.min.x >= flushrect.max.x){
		flushrect = r;
		waste = 0;
		return;
	}
	nbb = flushrect;
	bbox(&nbb, &r);
	ar = Dx(r)*Dy(r);
	abb = Dx(flushrect)*Dy(flushrect);
	anbb = Dx(nbb)*Dy(nbb);
	/*
	 * Area of new waste is area of new bb minus area of old bb,
	 * less the area of the new segment, which we assume is not waste.
	 * This could be negative, but that's OK.
	 */
	waste += anbb-abb - ar;
	if(waste < 0)
		waste = 0;
	/*
	 * absorb if:
	 *	total area is small
	 *	waste is less than ½ total area
	 * 	rectangles touch
	 */
	if(anbb<=1024 || waste*2<anbb || rectXrect(flushrect, r)){
		flushrect = nbb;
		return;
	}
	/* emit current state */
	flushmemscreen(flushrect);
	flushrect = r;
	waste = 0;
}

static
void
dstflush(int dstid, Memimage *dst, Rectangle r)
{
	Memlayer *l;

	if(dstid == 0){
		bbox(&flushrect, &r);
		return;
	}
	l = dst->layer;
	if(l == nil)
		return;
	do{
		if(l->screen->image->data != screenimage.data)
			return;
		r = rectaddpt(r, l->delta);
		l = l->screen->image->layer;
	}while(l);
	addflush(r);
}

static
void
drawflush(void)
{
	flushmemscreen(flushrect);
	flushrect = Rect(10000, 10000, -10000, -10000);
	if (swcursor) {
		cursornogo = flushrect;
		cursorupdate(cursornogo);
	}
}

static
void
cursorshield(Rectangle r)
{
	if (swcursor) {
		bbox(&cursornogo, &r);
		cursorupdate(cursornogo);
	}
}

/* static */
void
cursorhide(void)
{
	cursorshield(Rect(-10000, -10000, 10000, 10000));
}

static
void
dstcursorhide(int dstid, Memimage *dst, Rectangle r)
{
	if (!swcursor)
		return;
	if(dstid == 0){
		cursorshield(r);
		return;
	}
	if(dst->layer == nil)
		return;
	if(dst->layer->screen->image->data != screenimage.data)
		return;
	r = rectaddpt(r, dst->layer->delta);
	cursorshield(r);
}

DImage*
drawlookup(Client *client, int id)
{
	DImage *d;

	d = client->dimage[id&HASHMASK];
	while(d){
		if(d->id == id)
			return d;
		d = d->next;
	}
	return 0;
}

DScreen*
drawlookupdscreen(int id)
{
	DScreen *s;

	s = dscreen;
	while(s){
		if(s->id == id)
			return s;
		s = s->next;
	}
	return 0;
}

DScreen*
drawlookupscreen(Client *client, int id, CScreen **cs)
{
	CScreen *s;

	s = client->cscreen;
	while(s){
		if(s->dscreen->id == id){
			*cs = s;
			return s->dscreen;
		}
		s = s->next;
	}
	error(Enodrawscreen);
	return 0;
}

Memimage*
drawinstall(Client *client, int id, Memimage *i, DScreen *dscreen)
{
	DImage *d;

	d = malloc(sizeof(DImage));
	if(d == 0)
		return 0;
	d->id = id;
	d->image = i;
	d->nfchar = 0;
	d->fchar = 0;
	d->dscreen = dscreen;
	d->next = client->dimage[id&HASHMASK];
	client->dimage[id&HASHMASK] = d;
	return i;
}

Memscreen*
drawinstallscreen(Client *client, DScreen *d, int id, Memimage *image, Memimage *fill, int public)
{
	Memscreen *s;
	CScreen *c;

	c = malloc(sizeof(CScreen));
	if(c == 0)
		return 0;
	if(d == 0){
		d = malloc(sizeof(DScreen));
		if(d == 0){
			free(c);
			return 0;
		}
		s = malloc(sizeof(Memscreen));
		if(s == 0){
			free(c);
			free(d);
			return 0;
		}
		s->frontmost = 0;
		s->rearmost = 0;
		s->image = image;
		s->fill = fill;
		d->ref = 0;
		d->id = id;
		d->screen = s;
		d->public = public;
		d->next = dscreen;
		d->owner = client;
		d->imageid = 0;
		d->fillid = 0;
		dscreen = d;
	}
	c->dscreen = d;
	d->ref++;
	c->next = client->cscreen;
	client->cscreen = c;
	return d->screen;
}

int
drawisscreen(DImage *d)
{
	DScreen *s;
	Memimage *i;

	i = d->image;
	for(s=dscreen; s; s=s->next){
		if(s->screen->image==i || s->screen->fill==i){
			/* be careful, in case (pathologically) it's both */
			if(s->screen->image==i)
				s->imageid = d->id;
			else
				s->fillid = d->id;
			return 1;
		}
	}
	return 0;
}

void
drawfreedscreen(DScreen *this)
{
	DScreen *ds, *next;

	this->ref--;
	if(this->ref < 0)
		print("negative ref in drawfreedscreen\n");
	if(this->ref > 0)
		return;
	ds = dscreen;
	if(ds == this){
		dscreen = this->next;
		goto Found;
	}
	while(next = ds->next){	/* assign = */
		if(next == this){
			ds->next = this->next;
			goto Found;
		}
		ds = next;
	}
	error(Enodrawimage);

    Found:
	if(this->imageid > 0)
		drawuninstall(this->owner, this->imageid);
	if(this->fillid > 0)
		drawuninstall(this->owner, this->fillid);
	free(this->screen);
	free(this);
}

void
drawuninstallscreen(Client *client, CScreen *this)
{
	CScreen *cs, *next;

	cs = client->cscreen;
	if(cs == this){
		client->cscreen = this->next;
		drawfreedscreen(this->dscreen);
		free(this);
		return;
	}
	while(next = cs->next){	/* assign = */
		if(next == this){
			cs->next = this->next;
			drawfreedscreen(this->dscreen);
			free(this);
			return;
		}
		cs = next;
	}
}

void
drawfreedimage(DImage *dimage)
{
	Memimage *l;
	DScreen *ds;

	ds = dimage->dscreen;
	if(ds){
		l = dimage->image;
		if(l->data == screenimage.data)
			addflush(l->layer->screenr);
		if(l->layer->refreshfn == drawrefresh)	/* else true owner will clean up */
			free(l->layer->refreshptr);
		l->layer->refreshptr = nil;
		cursorhide();
		memldelete(l);
		drawfreedscreen(ds);
	}else
		freememimage(dimage->image);
}

void
drawuninstall(Client *client, int id)
{
	DImage *d, *next;

	/*
	 * If this image is a screen fill or image, don't drop it yet; save it
	 * in the DScreen and drawfreedscreen will ask again later.
	 */
	d = client->dimage[id&HASHMASK];
	if(d == 0)
		error(Enodrawimage);
	if(d->id == id){
		if(drawisscreen(d))
			return;
		if(id != 0)
			drawfreedimage(d);
		free(d->fchar);
		client->dimage[id&HASHMASK] = d->next;
		free(d);
		return;
	}
	while(next = d->next){	/* assign = */
		if(next->id == id){
			if(drawisscreen(next))
				return;
			drawfreedimage(next);
			free(next->fchar);
			d->next = next->next;
			free(next);
			return;
		}
		d = next;
	}
	error(Enodrawimage);
}

Client*
drawnewclient(void)
{
	Client *cl, **cp;
	int i;

	cl = 0;
	qlock(&sdraw);
	for(i=0; i<sdraw.nclient; i++){
		cl = sdraw.client[i];
		if(cl == 0)
			break;
	}
	if(i == sdraw.nclient){
		cp = malloc((sdraw.nclient+1)*sizeof(Client*));
		if(cp == 0)
			goto Return;
		memmove(cp, sdraw.client, sdraw.nclient*sizeof(Client*));
		free(sdraw.client);
		sdraw.client = cp;
		sdraw.nclient++;
		cp[i] = 0;
	}
	cl = malloc(sizeof(Client));
	if(cl == 0)
		goto Return;
	cl->slot = i;
	cl->clientid = ++sdraw.clientid;
	sdraw.client[i] = cl;
Return:
	qunlock(&sdraw);
	return cl;
}

Client*
drawclientofpath(ulong path)
{
	Client *cl;
	int slot;

	slot = CLIENTPATH(path);
	if(slot == 0)
		return nil;
	cl = sdraw.client[slot-1];
	if(cl==0 || cl->clientid==0)
		return nil;
	return cl;
}


Client*
drawclient(Chan *c)
{
	Client *client;

	client = drawclientofpath(c->qid.path);
	if(client == nil)
		error(Enoclient);
	return client;
}

Memimage*
drawimage(Client *client, uchar *a)
{
	DImage *d;

	d = drawlookup(client, BGLONG(a));
	if(d == nil)
		error(Enodrawimage);
	return d->image;
}

void
drawrectangle(Rectangle *r, uchar *a)
{
	r->min.x = BGLONG(a+0*4);
	r->min.y = BGLONG(a+1*4);
	r->max.x = BGLONG(a+2*4);
	r->max.y = BGLONG(a+3*4);
}

void
drawpoint(Point *p, uchar *a)
{
	p->x = BGLONG(a+0*4);
	p->y = BGLONG(a+1*4);
}

Point
drawchar(Memimage *dst, Point p, Memimage *src, Point *sp, DImage *font, int index)
{
	FChar *fc;
	Rectangle r;
	Point sp1;

	fc = &font->fchar[index];
	r.min.x = p.x+fc->left;
	r.min.y = p.y-(font->ascent-fc->miny);
	r.max.x = r.min.x+(fc->maxx-fc->minx);
	r.max.y = r.min.y+(fc->maxy-fc->miny);
	sp1.x = sp->x+fc->left;
	sp1.y = sp->y+fc->miny;
	memdraw(dst, r, src, sp1, font->image, Pt(fc->minx, fc->miny));
	p.x += fc->width;
	sp->x += fc->width;
	return p;
}

static Chan*
drawattach(char *spec)
{
	int width;

	if(screendata.data == nil){
		screendata.base = nil;
		screendata.data = attachscreen(&screenimage.r, &screenimage.ldepth, &width, &sdraw.softscreen);
		screenimage.data = &screendata;
		screenimage.width = width;
		screenimage.clipr  = screenimage.r;
		if(screendata.data == nil)
			error("no frame buffer");
	}
	return devattach('d', spec);
}

static int
drawwalk(Chan *c, char *name)
{
	Path *op;

	if(screendata.data == nil)
		error("no frame buffer");
	if(strcmp(name, "..") == 0){
		switch(QID(c->qid)){
		case Qtopdir:
			return 1;
		case Q2nd:
			c->qid = (Qid){CHDIR|Qtopdir, 0};
			break;
		case Q3rd:
			c->qid = (Qid){CHDIR|Q2nd, 0};
			break;
		default:
			panic("drawwalk %lux", c->qid.path);
		}
		op = c->path;
		c->path = ptenter(&syspt, op, name);
		decref(op);
		return 1;
	}
	return devwalk(c, name, 0, 0, drawgen);
}

static void
drawstat(Chan *c, char *db)
{
	devstat(c, db, 0, 0, drawgen);
}

static Chan*
drawopen(Chan *c, int omode)
{
	Client *cl;

	if(c->qid.path & CHDIR)
		return devopen(c, omode, 0, 0, drawgen);

	if(QID(c->qid) == Qnew){
		cl = drawnewclient();
		if(cl == 0)
			error(Enodev);
		c->qid.path = Qctl|((cl->slot+1)<<QSHIFT);
	}

	qlock(&sdraw);
	if(waserror()){
		qunlock(&sdraw);
		nexterror();
	}

	switch(QID(c->qid)){
	case Qnew:
		break;

	case Qctl:
		cl = drawclient(c);
		if(cl->busy)
			error(Einuse);
		cl->busy = 1;
		flushrect = Rect(10000, 10000, -10000, -10000);
		drawinstall(cl, 0, &screenimage, 0);
		incref(&cl->r);
		break;
	case Qdata:
	case Qrefresh:
		cl = drawclient(c);
		incref(&cl->r);
		break;
	}
	qunlock(&sdraw);
	poperror();
	c->mode = openmode(omode);
	c->flag |= COPEN;
	c->offset = 0;
	return c;
}

static void
drawclose(Chan *c)
{
	int i;
	DImage *d, **dp;
	Client *cl;
	Refresh *r;

	if(c->qid.path & CHDIR)
		return;
	qlock(&sdraw);
	if(waserror()){
		qunlock(&sdraw);
		nexterror();
	}

	cl = drawclient(c);
	if(QID(c->qid) == Qctl)
		cl->busy = 0;
	if((c->flag&COPEN) && (decref(&cl->r)==0)){
		while(r = cl->refresh){	/* assign = */
			cl->refresh = r->next;
			free(r);
		}
		while(cl->cscreen)
			drawuninstallscreen(cl, cl->cscreen);
		/* all screens are freed, so now we can free images */
		dp = cl->dimage;
		for(i=0; i<NHASH; i++){
			while(d = *dp){	/* assign = */
/*BUG: what about shared screens, etc.? */
				*dp = d->next;
				if(d->id != 0)
					drawfreedimage(d);
				free(d->fchar);	
				free(d);
			}
			dp++;
		}
		sdraw.client[cl->slot] = 0;
		drawflush();	/* to erase visible, now dead windows */
		free(cl);
	}
	qunlock(&sdraw);
	poperror();
}

static long
drawread(Chan *c, void *a, long n, ulong offset)
{
	Client *cl;
	uchar *p;
	Refresh *r;

	USED(offset);
	if(c->qid.path & CHDIR)
		return devdirread(c, a, n, 0, 0, drawgen);
	cl = drawclient(c);
	switch(QID(c->qid)){
	case Qctl:
		if(offset > 0)
			return 0;
		if(n <= 7*12)
			error(Eshortread);
		n = sprint(a, "%11d %11d %11d %11d %11d %11d %11d ",
			cl->clientid, 0,
			screenimage.ldepth, screenimage.r.min.x,
			screenimage.r.min.y, screenimage.r.max.x,
			screenimage.r.max.y);
		return n+1;

	case Qdata:
		if(cl->readdata == nil)
			error("no draw data");
		if(n < cl->nreaddata)
			error(Eshortread);
		n = cl->nreaddata;
		memmove(a, cl->readdata, cl->nreaddata);
		free(cl->readdata);
		cl->readdata = nil;
		return n;

	case Qrefresh:
		if(n < 5*4)
			error(Ebadarg);
		for(;;){
			qlock(&sdraw);
			if(cl->refreshme || cl->refresh)
				break;
			qunlock(&sdraw);
			sleep(&cl->refrend, drawrefactive, cl);
		}
		p = a;
		while(cl->refresh && n>=5*4){
			r = cl->refresh;
			BPLONG(p+0*4, r->dimage->id);
			BPLONG(p+1*4, r->r.min.x);
			BPLONG(p+2*4, r->r.min.y);
			BPLONG(p+3*4, r->r.max.x);
			BPLONG(p+4*4, r->r.max.y);
			cl->refresh = r->next;
			free(r);
			p += 5*4;
			n -= 5*4;
		}
		cl->refreshme = 0;
		qunlock(&sdraw);
		return p-(uchar*)a;
	}
}

void
drawwakeall(void)
{
	Client *cl;
	int i;

	for(i=0; i<sdraw.nclient; i++){
		cl = sdraw.client[i];
		if(cl && (cl->refreshme || cl->refresh))
			wakeup(&cl->refrend);
	}
}

static long
drawwrite(Chan *c, void *a, long n, ulong offset)
{
	Client *cl;

	USED(offset);
	if(c->qid.path & CHDIR)
		error(Eisdir);
	cl = drawclient(c);
	qlock(&sdraw);
	if(waserror()){
		drawwakeall();
		qunlock(&sdraw);
		nexterror();
	}
	switch(QID(c->qid)){
	case Qctl:
		error("unknown draw control request");
		break;
	case Qdata:
		drawmesg(cl, a, n);
		drawwakeall();
		break;
	default:
		error(Ebadusefd);
	}
	qunlock(&sdraw);
	poperror();
	return n;
}

uchar*
drawcoord(uchar *p, uchar *maxp, int oldx, int *newx)
{
	int b, x;

	if(p >= maxp)
		error(Eshortdraw);
	b = *p++;
	x = b & 0x7F;
	if(b & 0x80){
		if(p+1 >= maxp)
			error(Eshortdraw);
		x |= *p++ << 7;
		x |= *p++ << 15;
		if(x & (1<<22))
			x |= ~0<<23;
	}else{
		if(b & 0x40)
			x |= ~0<<7;
		x += oldx;
	}
	*newx = x;
	return p;
}

void
drawmesg(Client *client, void *av, int n)
{
	int c, ldepth, repl, m, y, dstid, scrnid, ni, ci, j, nw, e0, e1, ox, oy, esize, doflush;
	uchar *u, *a;
	Rectangle r, clipr;
	Point p, q, *pp, sp;
	Memimage *i, *dst, *src, *mask;
	Memimage *l, **lp;
	Memscreen *scrn;
	DImage *font, *ll;
	DScreen *dscrn;
	FChar *fc;
	Refx *refx;
	CScreen *cs;
	Refreshfn reffn;

	a = av;
	m = 0;
	while((n-=m) > 0){
		a += m;
		switch(*a){
		default:
			print("bad draw command: %c=0x%ux\n", *a, *a);
			error("bad draw command");
			break;
		/* allocate: 'a' id[4] screenid[4] refresh[1] ldepth[2] repl[1] R[4*4] clipR[4*4] value[1] */
		case 'a':
			m = 1+4+4+1+2+1+4*4+4*4+1;
			if(n < m)
				error(Eshortdraw);
			dstid = BGLONG(a+1);
			if(drawlookup(client, dstid))
				error(Eimageexists);
			ldepth = BGSHORT(a+10);
			repl = a[12];
			drawrectangle(&r, a+13);
			scrnid = BGSHORT(a+5);
			if(scrnid){
				dscrn = drawlookupscreen(client, scrnid, &cs);
				scrn = dscrn->screen;
				if(repl || ldepth!=scrn->image->ldepth)
					error("image parameters incompatible with screen");
				reffn = nil;
				switch(a[9]){
				case Refbackup:
					break;
				case Reflocal:
					reffn = memlnorefresh;
					break;
				case Refremote:
					reffn = drawrefresh;
					break;
				default:
					error("unknown refresh method");
				}
				cursorhide();
				l = memlalloc(scrn, r, reffn, 0, a[45]);
				if(l == 0)
					error(Edrawmem);
				addflush(l->layer->screenr);
				drawrectangle(&l->clipr, a+29);
				rectclip(&l->clipr, r);
				if(drawinstall(client, dstid, l, dscrn) == 0){
					cursorhide();
					memldelete(l);
					error(Edrawmem);
				}
				dscrn->ref++;
				if(reffn){
					refx = nil;
					if(reffn == drawrefresh){
						refx = malloc(sizeof(Refx));
						if(refx == 0){
							drawuninstall(client, dstid);
							error(Edrawmem);
						}
						refx->client = client;
						refx->dimage = drawlookup(client, dstid);
					}
					cursorhide();
					memlsetrefresh(l, reffn, refx);
				}
				continue;
			}
			cursorhide();
			i = allocmemimage(r, ldepth);
			if(i == 0)
				error(Edrawmem);
			i->repl = repl;
			drawrectangle(&i->clipr, a+29);
			if(!repl)
				rectclip(&i->clipr, r);
			if(drawinstall(client, dstid, i, 0) == 0){
				freememimage(i);
				error(Edrawmem);
			}
			cursorhide();
			memfillcolor(i, a[45]);
			continue;

		/* allocate screen: 'A' id[4] imageid[4] fillid[4] public[1] */
		case 'A':
			m = 1+4+4+4+1;
			if(n < m)
				error(Eshortdraw);
			dstid = BGLONG(a+1);
			if(dstid == 0)
				error(Ebadarg);
			if(drawlookupdscreen(dstid))
				error(Escreenexists);
			dst = drawimage(client, a+5);
			src = drawimage(client, a+9);
			if(drawinstallscreen(client, 0, dstid, dst, src, a[13]) == 0)
				error(Edrawmem);
			continue;

		/* set repl and clip: 'c' dstid[4] repl[1] clipR[4*4] */
		case 'c':
			m = 1+4+1+4*4;
			if(n < m)
				error(Eshortdraw);
			dst = drawimage(client, a+1);
			dst->repl = a[5];
			drawrectangle(&r, a+6);
			drawrectangle(&dst->clipr, a+6);
			continue;

		/* cursor change: 'C' id[4] hotspot[2*4] */
		case 'C':
			m = 1+4+2*4;
			if(n < m)
				error(Eshortdraw);
			dstid = BGLONG(a+1);
			if(dstid == 0) {
				curs.data = nil;
				drawcursor(&curs);
				continue;
			}
			i = drawimage(client, a+1);
			if(i->ldepth != 0)
				error(Eldepth);

			ni = i->width*sizeof(ulong)*Dy(i->r);
			curs.data = malloc(ni);
			if(curs.data == nil)
				error(Enomem);
			drawpoint(&p, a+5);
			curs.hotx = p.x;
			curs.hoty = p.y;
			curs.minx = i->r.min.x;
			curs.miny = i->r.min.y;
			curs.maxx = i->r.max.x;
			curs.maxy = i->r.max.y;
			unloadmemimage(i, i->r, curs.data, ni);
			cursorhide();
			drawcursor(&curs);
			free(curs.data);
			continue;

		/* draw: 'd' dstid[4] srcid[4] maskid[4] R[4*4] P[2*4] P[2*4] */
		case 'd':
			m = 1+4+4+4+4*4+2*4+2*4;
			if(n < m)
				error(Eshortdraw);
			dst = drawimage(client, a+1);
			dstid = BGLONG(a+1);
			src = drawimage(client, a+5);
			mask = drawimage(client, a+9);
			drawrectangle(&r, a+13);
			drawpoint(&p, a+29);
			drawpoint(&q, a+37);
			dstcursorhide(dstid, dst, r);
			dstcursorhide(BGLONG(a+5), src, rectaddpt(r, subpt(p, r.min)));
			memdraw(dst, r, src, p, mask, q);
			dstflush(dstid, dst, r);
			continue;

		/* ellipse: 'e' dstid[4] srcid[4] center[2*4] a[4] b[4] thick[4] sp[2*4] alpha[4] phi[4]*/
		case 'e':
		case 'E':
			m = 1+4+4+2*4+4+4+4+2*4+2*4;
			if(n < m)
				error(Eshortdraw);
			dst = drawimage(client, a+1);
			dstid = BGLONG(a+1);
			src = drawimage(client, a+5);
			drawpoint(&p, a+9);
			e0 = BGLONG(a+17);
			e1 = BGLONG(a+21);
			if(e0<0 || e1<0)
				error("invalid ellipse semidiameter");
			j = BGLONG(a+25);
			if(j < 0)
				error("negative ellipse thickness");
			drawpoint(&sp, a+29);
			c = j;
			if(*a == 'E')
				c = -1;
			ox = BGLONG(a+37);
			oy = BGLONG(a+41);
			cursorhide();
			/* high bit indicates arc angles are present */
			if(ox & (1<<31)){
				if((ox & (1<<30)) == 0)
					ox &= ~(1<<31);
				memarc(dst, p, e0, e1, c, src, sp, ox, oy);
			}else
				memellipse(dst, p, e0, e1, c, src, sp);

			dstflush(dstid, dst, Rect(p.x-e0-j, p.y-e1-j, p.x+e0+j+1, p.y+e1+j+1));
			continue;

		/* free: 'f' id[4] */
		case 'f':
			m = 1+4;
			if(n < m)
				error(Eshortdraw);
			ll = drawlookup(client, BGLONG(a+1));
			if(ll && ll->dscreen && ll->dscreen->owner != client)
				ll->dscreen->owner->refreshme = 1;
			cursorhide();
			drawuninstall(client, BGLONG(a+1));
			drawflush();	/* BUG: otherwise cursor vanishes! */
			continue;

		/* free screen: 'F' id[4] */
		case 'F':
			m = 1+4;
			if(n < m)
				error(Eshortdraw);
			drawlookupscreen(client, BGLONG(a+1), &cs);
			cursorhide();
			drawuninstallscreen(client, cs);
			continue;

		/* initialize font: 'i' fontid[4] nchars[4] ascent[1] */
		case 'i':
			m = 1+4+4+1;
			if(n < m)
				error(Eshortdraw);
			dstid = BGLONG(a+1);
			if(dstid == 0)
				error("can't use display as font");
			font = drawlookup(client, dstid);
			if(font == 0)
				error(Enodrawimage);
			if(font->image->layer)
				error("can't use window as font");
			free(font->fchar);	/* should we complain if non-zero? */
			ni = BGLONG(a+5);
			font->fchar = malloc(ni*sizeof(FChar));
			if(font->fchar == 0)
				error("no memory for font");
			font->nfchar = ni;
			font->ascent = a[9];
			continue;

		/* load character: 'l' fontid[4] srcid[4] index[2] R[4*4] P[2*4] left[1] width[1] */
		case 'l':
			m = 1+4+4+2+4*4+2*4+1+1;
			if(n < m)
				error(Eshortdraw);
			font = drawlookup(client, BGLONG(a+1));
			if(font == 0)
				error(Enodrawimage);
			if(font->nfchar == 0)
				error(Enotfont);
			src = drawimage(client, a+5);
			ci = BGSHORT(a+9);
			if(ci >= font->nfchar)
				error(Eindex);
			drawrectangle(&r, a+11);
			drawpoint(&p, a+27);
			memdraw(font->image, r, src, p, memones, p);
			fc = &font->fchar[ci];
			fc->minx = r.min.x;
			fc->maxx = r.max.x;
			fc->miny = r.min.y;
			fc->maxy = r.max.y;
			fc->left = a[35];
			fc->width = a[36];
			continue;

		/* draw line: 'L' dstid[4] p0[2*4] p1[2*4] end0[4] end1[4] radius[4] srcid[4] sp[2*4] */
		case 'L':
			m = 1+4+2*4+2*4+4+4+4+4+2*4;
			if(n < m)
				error(Eshortdraw);
			dst = drawimage(client, a+1);
			dstid = BGLONG(a+1);
			drawpoint(&p, a+5);
			drawpoint(&q, a+13);
			e0 = BGLONG(a+21);
			e1 = BGLONG(a+25);
			j = BGLONG(a+29);
			if(j < 0)
				error("negative line width");
			src = drawimage(client, a+33);
			drawpoint(&sp, a+37);
			cursorhide();
			memline(dst, p, q, e0, e1, j, src, sp);
			/* avoid memlinebbox if possible */
			if(dstid==0 || dst->layer!=nil){
				/* BUG: this is terribly inefficient: update maximal containing rect*/
				r = memlinebbox(p, q, e0, e1, j);
				dstflush(dstid, dst, insetrect(r, -(1+1+j)));
			}
			continue;

		/* position window: 'o' id[4] r.min [2*4] screenr.min [2*4] */
		case 'o':
			m = 1+4+2*4+2*4;
			if(n < m)
				error(Eshortdraw);
			dst = drawimage(client, a+1);
			if(dst->layer){
				drawpoint(&p, a+5);
				drawpoint(&q, a+13);
				r = dst->layer->screenr;
				cursorhide();
				ni = memlorigin(dst, p, q);
				if(ni < 0)
					error("image origin failed");
				if(ni > 0){
					addflush(r);
					addflush(dst->layer->screenr);
					ll = drawlookup(client, BGLONG(a+1));
					if(ll->dscreen->owner != client)
						ll->dscreen->owner->refreshme = 1;
				}
			}
			continue;

		/* filled polygon: 'P' dstid[4] n[2] wind[4] ignore[2*4] srcid[4] sp[2*4] p0[2*4] dp[2*2*n] */
		/* polygon: 'p' dstid[4] n[2] end0[4] end1[4] radius[4] srcid[4] sp[2*4] p0[2*4] dp[2*2*n] */
		case 'p':
		case 'P':
			m = 1+4+2+4+4+4+4+2*4;
			if(n < m)
				error(Eshortdraw);
			dstid = BGLONG(a+1);
			dst = drawimage(client, a+1);
			ni = BGSHORT(a+5);
			if(ni < 0)
				error("negative count in polygon");
			e0 = BGLONG(a+7);
			e1 = BGLONG(a+11);
			j = 0;
			if(*a == 'p'){
				j = BGLONG(a+15);
				if(j < 0)
					error("negative polygon line width");
			}
			src = drawimage(client, a+19);
			drawpoint(&sp, a+23);
			drawpoint(&p, a+31);
			ni++;
			pp = malloc(ni*sizeof(Point));
			if(pp == nil)
				error(Enomem);
			doflush = 0;
			if(dstid==0 || (dst->layer && dst->layer->screen->image->data == screenimage.data))
				doflush = 1;	/* simplify test in loop */
			ox = oy = 0;
			u = a+m;
			for(y=0; y<ni; y++){
				u = drawcoord(u, a+n, ox, &p.x);
				u = drawcoord(u, a+n, oy, &p.y);
				ox = p.x;
				oy = p.y;
				if(doflush){
					esize = j;
					if(*a == 'p'){
						if(y == 0){
							c = memlineendsize(e0);
							if(c > esize)
								esize = c;
						}
						if(y == ni-1){
							c = memlineendsize(e1);
							if(c > esize)
								esize = c;
						}
					}
					if(*a=='P' && e0!=1 && e0 !=~0)
						r = dst->clipr;
					else
						r = Rect(p.x-esize, p.y-esize, p.x+esize+1, p.y+esize+1);
					dstcursorhide(dstid, dst, r);
					dstflush(dstid, dst, r);
				}
				pp[y] = p;
			}
//			chide = cursormaybehide(&flushrect, dst,r, 0, 0);
//			cursorhide();
			if(*a == 'p')
				mempoly(dst, pp, ni, e0, e1, j, src, sp);
			else
				memfillpoly(dst, pp, ni, e0, src, sp);
			free(pp);
			m = u-a;
			continue;

		/* read: 'r' id[4] R[4*4] */
		case 'r':
			m = 1+4+4*4;
			if(n < m)
				error(Eshortdraw);
			i = drawimage(client, a+1);
			if(i->layer)
				error("readimage from window unimplemented");
			drawrectangle(&r, a+5);
			if(!rectinrect(r, i->r))
				error(Ereadoutside);
			free(client->readdata);
			c = bytesperline(r, i->ldepth);
			c *= Dy(r);
			client->readdata = mallocz(c, 0);
			if(client->readdata == nil)
				error("readimage malloc failed");
			cursorhide();
			client->nreaddata = unloadmemimage(i, r, client->readdata, c);
			if(client->nreaddata < 0){
				free(client->readdata);
				client->readdata = nil;
				error("bad readimage call");
			}
			continue;

		/* string: 's' dstid[4] srcid[4] fontid[4] P[2*4] clipr[4*4] sp[2*4] ni[2] ni*(index[2]) */
		case 's':
			m = 1+4+4+4+2*4+4*4+2*4+2;
			if(n < m)
				error(Eshortdraw);
			dst = drawimage(client, a+1);
			dstid = BGLONG(a+1);
			src = drawimage(client, a+5);
			font = drawlookup(client, BGLONG(a+9));
			if(font == 0)
				error(Enodrawimage);
			if(font->nfchar == 0)
				error(Enotfont);
			drawpoint(&p, a+13);
			drawrectangle(&r, a+21);
			drawpoint(&sp, a+37);
			ni = BGSHORT(a+45);
			u = a+m;
			m += ni*2;
			if(n < m)
				error(Eshortdraw);
			clipr = dst->clipr;
			dst->clipr = r;
			q = p;
			cursorhide();
			while(--ni >= 0){
				ci = BGSHORT(u);
				if(ci<0 || ci>=font->nfchar){
					dst->clipr = clipr;
					error(Eindex);
				}
				q = drawchar(dst, q, src, &sp, font, ci);
				u += 2;
			}
			dst->clipr = clipr;
			p.y -= font->ascent;
			dstflush(dstid, dst, Rect(p.x, p.y, q.x, p.y+Dy(font->image->r)));
			continue;

		/* use public screen: 'S' id[4] ldepth[4] */
		case 'S':
			m = 1+4+4;
			if(n < m)
				error(Eshortdraw);
			dstid = BGLONG(a+1);
			if(dstid == 0)
				error(Ebadarg);
			dscrn = drawlookupdscreen(dstid);
			if(dscrn==0 || (dscrn->public==0 && dscrn->owner!=client))
				error(Enodrawscreen);
			if(dscrn->screen->image->ldepth != BGLONG(a+5))
				error("inconsistent ldepth");
			if(drawinstallscreen(client, dscrn, 0, 0, 0, 0) == 0)
				error(Edrawmem);
			continue;

		/* top or bottom windows: 't' top[1] nw[2] n*id[4] */
		case 't':
			m = 1+1+2;
			if(n < m)
				error(Eshortdraw);
			nw = BGSHORT(a+2);
			if(nw < 0)
				error(Ebadarg);
			if(nw == 0)
				continue;
			m += nw*4;
			if(n < m)
				error(Eshortdraw);
			lp = malloc(nw*sizeof(Memimage*));
			if(lp == 0)
				error(Enomem);
			if(waserror()){
				free(lp);
				nexterror();
			}
			for(j=0; j<nw; j++)
				lp[j] = drawimage(client, a+1+1+2+j*4);
			if(lp[0]->layer == 0)
				error("images are not windows");
			for(j=1; j<nw; j++)
				if(lp[j]->layer->screen != lp[0]->layer->screen)
					error("images not on same screen");
			cursorhide();
			if(a[1])
				memltofrontn(lp, nw);
			else
				memltorearn(lp, nw);
			if(lp[0]->layer->screen->image->data == screenimage.data)
				for(j=0; j<nw; j++)
					addflush(lp[j]->layer->screenr);
			ll = drawlookup(client, BGLONG(a+1+1+2));
			if(ll->dscreen->owner != client)
				ll->dscreen->owner->refreshme = 1;
			poperror();
			free(lp);
			continue;

		/* visible: 'v' */
		case 'v':
			m = 1;
			drawflush();
			continue;

		/* write: 'w' id[4] R[4*4] data[x*1] */
		/* write from compressed data: 'W' id[4] R[4*4] data[x*1] */
		case 'w':
		case 'W':
			m = 1+4+4*4;
			if(n < m)
				error(Eshortdraw);
			dstid = BGLONG(a+1);
			dst = drawimage(client, a+1);
			drawrectangle(&r, a+5);
			if(!rectinrect(r, dst->r))
				error(Ewriteoutside);
			dstcursorhide(dstid, dst, r);
			y = memload(dst, r, a+m, n-m, *a=='W');
			if(y < 0)
				error("bad writeimage call");
			dstflush(dstid, dst, r);
			m += y;
			continue;
		}
	}
}

int
drawlsetrefresh(ulong qidpath, int id, void *reffn, void *refx)
{
	DImage *d;
	Memimage *i;
	Client *client;
	int r;

	client = drawclientofpath(qidpath);
	if(client == 0)
		return 0;
	d = drawlookup(client, id);
	if(d == nil)
		return 0;
	i = d->image;
	if(i->layer == nil)
		return 0;
	cursorhide();
	r = memlsetrefresh(i, reffn, refx);
	return r;
}

Dev drawdevtab = {
	'd',
	"draw",

	devreset,
	devinit,
	drawattach,
	devdetach,
	devclone,
	drawwalk,
	drawstat,
	drawopen,
	devcreate,
	drawclose,
	drawread,
	devbread,
	drawwrite,
	devbwrite,
	devremove,
	devwstat,
};
