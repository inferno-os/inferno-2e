#include "lib9.h"
#include "image.h"
#include "tk.h"

#define	O(t, e)		((long)(&((t*)0)->e))

/* Window Options (+ means implemented)
	 +tags
	 +width
	 +height
	 +window
	 +anchor
*/

static
TkOption windopts[] =
{
	"width",	OPTfrac,	O(TkCwind, width),	nil,
	"height",	OPTfrac,	O(TkCwind, height),	nil,
	"anchor",	OPTstab,	O(TkCwind, anchor),	tkanchor,
	"window",	OPTwinp,	O(TkCwind, sub),	nil,
	nil
};

static
TkOption itemopts[] =
{
	"tags",		OPTctag,	O(TkCitem, tags),	nil,
	nil
};

static void
tkcvswindsize(TkCitem *i)
{
	Tk *s;
	int bw;
	Point p;
	TkCwind *w;

	w = TKobj(TkCwind, i);
	s = w->sub;
	if(s == nil)
		return;

	if(w->width != s->act.width || w->height != s->act.height) {
		s->act.width = w->width;
		s->act.height = w->height;
		if(s->slave) {
			tkpackqit(s);
			tkrunpack();
		}
	}

	p = tkcvsanchor(i->p.drawpt[0], s->act.width, s->act.height, w->anchor);
	s->act.x = p.x;
	s->act.y = p.y;

	bw = 2*s->borderwidth;
	i->p.bb.min = p;
	i->p.bb.max.x = p.x + s->act.width + bw;
	i->p.bb.max.y = p.y + s->act.height + bw;
}

static int
tkcvschkwfocus(TkCwind *w, Tk *tk)
{
	if(w->focus == tk)
		return 1;
	for(tk = tk->slave; tk; tk = tk->next)
		if(tkcvschkwfocus(w, tk))
			return 1;
	return 0;
}

static void
tkcvswindgeom(Tk *sub, int x, int y, int w, int h)
{
	TkCitem *i;
	Tk *parent;
	TkCanvas *c;
	TkCwind *win;

	USED(x);
	USED(y);
	parent = sub->parent;

	win = nil;
	c = TKobj(TkCanvas, parent);
	for(i = c->head; i; i = i->next) {
		if(i->type == TkCVwindow) {
			win = TKobj(TkCwind, i);
			if(win->sub == sub)
				break;
		}
	}

	if(win->focus != nil) {
		if(tkcvschkwfocus(win, sub) == 0)
			win->focus = nil;
	}

	tkbbmax(&c->update, &i->p.bb);

	if(win->width == sub->req.width)
		win->width = w;
	if(win->height == sub->req.height)
		win->height = h;

	sub->req.width = w;
	sub->req.height = h;
	tkcvswindsize(i);

	tkbbmax(&c->update, &i->p.bb);
	parent->flag |= Tkdirty;
}

static void
tkcvssubdestry(Tk *sub)
{
	Tk *tk;
	TkCitem *i;
	TkCanvas *c;
	TkCwind *win;

	tk = sub->parent;
	if(tk == nil)
		return;

	c = TKobj(TkCanvas, tk);
	for(i = c->head; i; i = i->next) {
		if(i->type == TkCVwindow) {
			win = TKobj(TkCwind, i);
			if(win->sub == sub) {
				tkbbmax(&c->update, &i->p.bb);
				tk->flag |= Tkdirty;

				win->focus = nil;
				win->sub = nil;
				sub->parent = nil;
				return;
			}
		}
	}
}

Point
tkcvsrelpos(Tk *sub)
{
	Tk *tk;
	TkCitem *i;
	TkCanvas *c;
	TkCwind *win;

	tk = sub->parent;
	if(tk == nil)
		return tkzp;

	c = TKobj(TkCanvas, tk);
	for(i = c->head; i; i = i->next) {
		if(i->type == TkCVwindow) {
			win = TKobj(TkCwind, i);
			if(win->sub == sub)
				return subpt(i->p.bb.min, c->view);
		}
	}
	return tkzp;
}

static char*
tkcvswindchk(Tk *tk, TkCwind *w)
{
	Tk *sub;

	sub = w->sub;
	if(sub == nil)
		return nil;

	if(sub->flag & Tkwindow)
		return TkIstop;

	if(sub->master != nil || sub->parent != nil)
		return TkWpack;

	sub->parent = tk;
	tksetbits(w->sub, Tksubsub);
	sub->geom = tkcvswindgeom;
	sub->destroyed = tkcvssubdestry;

	if(w->width == 0)
		w->width = sub->req.width;
	if(w->height == 0)
		w->height = sub->req.height;

	return nil;
}

char*
tkcvswindcreat(Tk* tk, char *arg, char **val)
{
	char *e;
	TkCwind *w;
	TkCitem *i;
	TkCanvas *c;
	TkOptab tko[3];

	c = TKobj(TkCanvas, tk);

	i = tkcnewitem(tk, TkCVwindow, sizeof(TkCitem)+sizeof(TkCwind));
	if(i == nil)
		return TkNomem;

	w = TKobj(TkCwind, i);
	w->anchor = Tkcenter;

	e = tkparsepts(tk->env->top, &i->p, &arg);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}
	if(i->p.npoint != 1) {
		tkcvsfreeitem(i);
		return TkFewpt;
	}

	tko[0].ptr = w;
	tko[0].optab = windopts;
	tko[1].ptr = i;
	tko[1].optab = itemopts;
	tko[2].ptr = nil;
	e = tkparse(tk->env->top, arg, tko, nil);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}

	e = tkcvswindchk(tk, w);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}

	e = tkcaddtag(tk, i, 1);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}

	tkcvswindsize(i);
	e = tkvalue(val, "%d", i->id);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}
	tkcvsappend(c, i);

	tkbbmax(&c->update, &i->p.bb);
	tk->flag |= Tkdirty;
	return nil;
}

char*
tkcvswindcget(TkCitem *i, char *arg, char **val)
{
	TkOptab tko[3];
	TkCwind *w = TKobj(TkCwind, i);

	tko[0].ptr = w;
	tko[0].optab = windopts;
	tko[1].ptr = i;
	tko[1].optab = itemopts;
	tko[2].ptr = nil;

	return tkgencget(tko, arg, val);
}

char*
tkcvswindconf(Tk *tk, TkCitem *i, char *arg)
{
	char *e;
	int dx, dy;
	TkOptab tko[3];
	TkCwind *w = TKobj(TkCwind, i);

	tko[0].ptr = w;
	tko[0].optab = windopts;
	tko[1].ptr = i;
	tko[1].optab = itemopts;
	tko[2].ptr = nil;

	dx = w->width;
	dy = w->height;
	w->width = 0;
	w->height = 0;

	e = tkparse(tk->env->top, arg, tko, nil);
	if(e == nil) {
		e = tkcvswindchk(tk, w);
		if(e != nil) {
			w->sub = nil;
			return e;
		}
		if(w->width == 0)
			w->width = dx;
		if(w->height == 0)
			w->height = dy;
		tkcvswindsize(i);
	}
	return e;
}

void
tkcvswindfree(TkCitem *i)
{
	Tk *sub;
	TkCwind *w;

	w = TKobj(TkCwind, i);
	sub = w->sub;
	if(w->focus == sub)
		w->focus = nil;
	if(sub != nil) {
		sub->parent = nil;
		sub->geom = nil;
	}
}

void
tkcvswinddraw(Image *img, TkCitem *i)
{
	TkCwind *w;

	USED(img);			/* See tkimageof */
	w = TKobj(TkCwind, i);
	if(w->sub != nil) {
		tksetbits(w->sub, Tkdirty|Tkrefresh);
		tkdrawslaves(w->sub, tkzp);	/* XXX - Tad: propagate err? */
	}
}

char*
tkcvswindcoord(TkCitem *i, char *arg, int x, int y)
{
	char *e;
	TkCpoints p;

	if(arg == nil) {
		tkxlatepts(i->p.parampt, i->p.npoint, x, y);
		tkxlatepts(i->p.drawpt, i->p.npoint, TKF2I(x), TKF2I(y));
	}
	else {
		p.parampt = nil;
		p.drawpt = nil;
		e = tkparsepts(i->env->top, &p, &arg);
		if(e != nil) {
			tkfreepoint(&p);
			return e;
		}
		if(p.npoint != 1) {
			tkfreepoint(&p);
			return TkFewpt;
		}
		tkfreepoint(&i->p);
		i->p = p;
	}
	tkcvswindsize(i);
	return nil;
}
