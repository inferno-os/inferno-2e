#include <lib9.h>
#include <kernel.h>
#include "image.h"
#include "tk.h"

static TkCtxt*	tkctxt;

TkCtxt*
tkscrn2ctxt(Screen *s)
{
	Display *d;
	TkCtxt	*c;

	d = s->display;
	for(c = tkctxt; c != nil; c = c->link)
		if(c->disp == d)
			return c;

	return nil;
}

TkCtxt*
tkattachctxt(Display *d)
{
	TkCtxt	*c;

	for(c = tkctxt; c != nil; c = c->link) {
		if(c->disp == d) {
			c->ref++;
			return c;
		}
	}

	c = malloc(sizeof(TkCtxt));
	if(c == nil)
		return nil;

	c->ref = 1;
	c->disp = d;
	c->link = tkctxt;
	tkctxt = c;
	return c;
}

void
tkdetachctxt(TkTop *t)
{
	int i, dl;
	Display *d;
	TkTop *f, **l;
	TkCtxt *c, *x, **xc;
	
	c = t->ctxt;
	l = &c->tkwindows;
	for(f = *l; f != nil; f = f->link) {
		if(f == t) {
			*l = t->link;
			break;
		}
		l = &f->link;
	}

	if(c->ref-- > 1)
		return;

	xc = &tkctxt;
	for(x = *xc; x != nil; x = x->link) {
		if(x == c) {
			*xc = x->link;
			break;
		}
		xc = &x->link;
	}

	dl = 0;
	d = x->disp;
	if(d != nil)
		dl = lockdisplay(d, 0);
	freeimage(x->i);
	for(i = 0; i < nelem(c->colors); i++) {
		if(c->colors[i] != nil)
			freeimage(c->colors[i]);
	}
	if(dl)
		unlockdisplay(d);
}

Image*
tkitmp(TkTop *t, Point p)
{
	Image *i;
	TkCtxt *ti;
	Display *d;
	Rectangle r;

	ti = t->ctxt;
	d = t->screen->display;
	if(ti->i != nil) {
		i = ti->i;
		if(p.x <= i->r.max.x && p.y <= i->r.max.y)
			return i;
		r = i->r;
		freeimage(ti->i);
		if(p.x < r.max.x)
			p.x = r.max.x;
		if(p.y < r.max.y)
			p.y = r.max.y;
	}

	r.min = tkzp;
	r.max = p;
	ti->i = allocimage(d, r, d->image->ldepth, 0, 0);

	return ti->i;
}

void
tkgeomchg(Tk *tk, TkGeom *g)
{
	int w, h;
	void (*geomfn)(Tk*);

	if(memcmp(&tk->req, g, sizeof(TkGeom)) == 0)
		return;

	geomfn = tkmethod[tk->type].geom;
	if(geomfn != nil)
		geomfn(tk);

	if(tk->master != nil) {
		tkpackqit(tk->master);		/* XXX - Tad: can fail */
		tkrunpack();			/* XXX - Tad: can fail */
	}
	else
	if(tk->geom != nil) {
		w = tk->req.width;
		h = tk->req.height;
		tk->req.width = 0;
		tk->req.height = 0;
		tk->geom(tk, tk->act.x, tk->act.y, w, h);
		tkpackqit(tk);			/* XXX - Tad: can fail */
		tkrunpack();			/* XXX - Tad: can fail */
	}

	tkdeliver(tk, TkConfigure, g);
}

int
tkinwidget(Tk *t, Point p)
{

	if(p.x < t->act.x ||
	   p.x > t->act.x+t->act.width+2*t->borderwidth)
		return 0;
	if(p.y < t->act.y ||
	   p.y > t->act.y+t->act.height+2*t->borderwidth)
		return 0;
	return 1;
}

Tk*
tkinwindow(Tk *tk, Point p)
{
	Tk *f, *r;

	if(tkinwidget(tk, p) == 0)
		return nil;

	if(tk->flag & Tkstopevent)
		return tk;

	p.x -= (tk->act.x+tk->borderwidth);
	p.y -= (tk->act.y+tk->borderwidth);
	for(f = tk->slave; f; f = f->next) {
		r = tkinwindow(f, p);
		if(r != nil)
			return r;
	}
	return tk;
}

Tk*
tkfindfocus(TkCtxt *c, int x, int y)
{
	Point p;
	Tk *tk, *f;

	p.x = x;
	p.y = y;
	for(f = c->tkdepth; f != nil; f = f->depth) {
		if(f->flag & Tkmapped) {
			tk = tkinwindow(f, p);
			if(tk != nil)
				return tk;
		}
	}
	return nil;
}

char*
tkdrawslaves(Tk *tk, Point orig)
{
	Tk *f;
	char *e = nil;

	if(tk->flag & Tkdirty) {
		e = tkmethod[tk->type].draw(tk, orig);
		tk->flag &= ~Tkdirty;
	}
	if(e != nil)
		return e;

	orig.x += tk->act.x + tk->borderwidth;
	orig.y += tk->act.y + tk->borderwidth;
	for(f = tk->slave; e == nil && f; f = f->next)
		e = tkdrawslaves(f, orig);
	return e;
}

char*
tkupdate(TkTop *t)
{
	Tk* tk;
	int locked;
	TkWin *tkw;
	Display *d;
	char *e;

	d = t->screen->display;
	locked = lockdisplay(d, 0);

	tk = t->windows;
	while(tk) {
		tkw = TKobj(TkWin, tk);
		if(tk->flag & Tkmapped) {
			e = tkdrawslaves(tk, tkzp);
			if(e != nil)
				return e;
		}

		tk = tkw->next;
	}
	flushimage(d, 1);
	if(locked)
		unlockdisplay(d);
	return nil;
}

int
tkischild(Tk *tk, Tk *child)
{
	if(tk == child)
		return 1;
	for(tk = tk->slave; tk; tk = tk->next)
		if(tkischild(tk, child))
			return 1;
	return 0;
}

void
tksetbits(Tk *tk, int mask)
{
	tk->flag |= mask;
	for(tk = tk->slave; tk; tk = tk->next)
		tksetbits(tk, mask);
}

char*
tkmap(Tk *tk)
{
	TkCtxt *c;
	TkWin *tkw;
	int locked;
	Display *d;

	c = tkdeldepth(tk);
	tk->depth = c->tkdepth;
	c->tkdepth = tk;

	tkw = TKobj(TkWin, tk);
	if(tkw->image != nil){
		d = tk->env->top->screen->display;
		locked = lockdisplay(d, 0);
		topwindow(tkw->image);
		if(locked)
			unlockdisplay(d);
	}

	if(tk->flag & Tkmapped)
		return nil;

	tk->flag |= Tkmapped;
	tkmoveresize(tk, tk->act.x, tk->act.y, tk->act.width, tk->act.height);
	tkdeliver(tk, TkMap, nil);
	tkenterleave(c, c->tkmstate.x, c->tkmstate.y);
	return tkupdate(tk->env->top);
}

void
tkclrfocus(Tk *top, Tk *tk)
{
	TkCtxt *c;

	if(tk == nil)
		return;

	c = tk->env->top->ctxt;

	if(tk == c->tkKgrab) {
		c->tkKgrab = nil;
		tkdeliver(tk, TkFocusout, nil);
		if(top != tk)	/* DBK */
			tkdeliver(top, TkFocusout, nil);
	}
	if(tk == c->tkMgrab)
		c->tkMgrab = nil;
	if(tk == c->tkMfocus) {
		tkdeliver(c->tkMfocus, TkLeave, nil);
		c->tkMfocus = nil;
	}

	for(tk = tk->slave; tk; tk = tk->next)
		tkclrfocus(top, tk);
}

void
tkunmap(Tk *tk)
{
	TkTop *t;
	TkWin *w;
	TkCtxt *c;
	int locked;
	Display *d;

	while(tk->master)
		tk = tk->master;

	if((tk->flag & Tkmapped) == 0)
		return;

	tkdeldepth(tk);
	tkclrfocus(tk, tk);

	tk->flag &= ~Tkmapped;
	d = tk->env->top->screen->display;
	c = tk->env->top->ctxt;

	w = TKobj(TkWin, tk);
	t = tk->env->top;
	if(TKobj(TkWin, t->root) == w) {
		tktopimageptr(t, nil);
		w->image = nil;
	}
	else
	if(w->image != nil) {
		locked = lockdisplay(d, 0);
		freeimage(w->image);
		w->image = nil;
		if(locked)
			unlockdisplay(d);
	}
	tkdeliver(tk, TkUnmap, nil);
	tkenterleave(c, c->tkmstate.x, c->tkmstate.y);
}

void
tkmoveresize(Tk *tk, int x, int y, int w, int h)
{
	TkTop *t;
	Image *i;
	TkWin *tkw;
	Display *d;
	Rectangle r;
	int bgp, bw2, locked;

	tk->req.x = x;
	tk->req.y = y;
	tk->req.width = w;
	tk->req.height = h;
	tk->act = tk->req;
	if((tk->flag & Tkmapped) == 0)
		return;

	tkw = TKobj(TkWin, tk);

	bw2 = 2*tk->borderwidth;
	r.min.x = x;
	r.min.y = y;
	r.max.x = x + w + bw2;
	r.max.y = y + h + bw2;

	i = tkw->image;
	if(i != nil && memcmp(&r, &i->r, sizeof(Rectangle)) == 0)
		return;

	d = tk->env->top->screen->display;
	locked = lockdisplay(d, 0);
	/*
	 * if the tkw is a top level we let the limbo ref count
	 * in di free the image
	 */
	t = tk->env->top;
	if(TKobj(TkWin, t->root) != tkw)
		freeimage(tkw->image);
	bgp = tk->env->colors[TkCbackgnd];
	tkw->image = allocwindow(tk->env->top->screen, r, nil, 0, bgp);
	if(locked)
		unlockdisplay(d);

	if(tkw->image == nil) {
		tk->flag &= ~Tkmapped;
		return;
	}

	tksetbits(tk, Tkdirty|Tkrefresh);
	/*
	 * Only install a new limbo image pointer in the top level
	 * window. menu etc. are not accessible by their bits in TkTop Image
	 */
	if(TKobj(TkWin, t->root) == tkw)
		tktopimageptr(t, tkw->image);
}

Image*
tkimageof(Tk *tk)
{
	while(tk) {
		if(tk->flag & Tkwindow)
			return TKobj(TkWin, tk)->image;
		if(tk->parent != nil) {
			tk = tk->parent;
			switch(tk->type) {
			case TKcanvas:
				return TKobj(TkCanvas, tk)->image;
			case TKtext:
				return TKobj(TkText, tk)->image;
			}
			abort();
		}
		tk = tk->master;
	}
	return nil;
}

void
tktopopt(Tk *tk, char *opt)
{
	TkTop *t;
	TkOptab tko[4];

	t = tk->env->top;

	tko[0].ptr = tk;
	tko[0].optab = tktop;
	tko[1].ptr = tk;
	tko[1].optab = tkgeneric;
	tko[2].ptr = t;
	tko[2].optab = tktopdbg;
	tko[3].ptr = nil;

	tkparse(t, opt, tko, nil);
}
