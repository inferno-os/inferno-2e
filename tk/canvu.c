#include <lib9.h>
#include <kernel.h>
#include "image.h"
#include "tk.h"

char*
tkparsepts(TkTop *t, TkCpoints *i, char **arg)
{
	char *s;
	Point *p, *d;
	int n, npoint;

	s = *arg;
	npoint = 0;
	while(*s) {
		s = tkskip(s, " \t");
		if(*s == '-')
			break;
		while(*s && *s != ' ' && *s != '\t')
			s++;
		npoint++;
	}

	i->parampt = realloc(i->parampt, npoint*sizeof(Point));
	if(i->parampt == nil)
		return TkNomem;

	s = *arg;
	p = i->parampt;
	npoint = 0;
	while(*s) {
		s = tkfrac(t, s, &p->x, nil);
		if(s == nil)
			return TkBadvl;
		s = tkfrac(t, s, &p->y, nil);
		if(s == nil)
			return TkBadvl;
		npoint++;
		s = tkskip(s, " \t");
		if(*s == '-')
			break;
		p++;
	}
	*arg = s;

	i->drawpt = realloc(i->drawpt, npoint*sizeof(Point));
	if(i->drawpt == nil)
		return TkNomem;

	i->bb = bbnil;

	d = i->drawpt;
	p = i->parampt;
	for(n = 0; n < npoint; n++) {
		d->x = TKF2I(p->x);
		d->y = TKF2I(p->y);
		if(d->x < i->bb.min.x)
			i->bb.min.x = d->x;
		if(d->x > i->bb.max.x)
			i->bb.max.x = d->x;
		if(d->y < i->bb.min.y)
			i->bb.min.y = d->y;
		if(d->y > i->bb.max.y)
			i->bb.max.y = d->y;
		d++;
		p++;
	}

	i->npoint = npoint;
	return nil;
}

TkCitem*
tkcnewitem(Tk *tk, int t, int n)
{
	TkCitem *i;

	i = malloc(n);
	if(i == nil)
		return nil;

	i->type = t;
	i->env = tk->env;
	i->env->ref++;

	return i;
}

void
tkxlatepts(Point *p, int npoints, int x, int y)
{
	while(npoints--) {
		p->x += x;
		p->y += y;
		p++;
	}
}

void
tkbbmax(Rectangle *bb, Rectangle *r)
{
	if(r->min.x < bb->min.x)
		bb->min.x = r->min.x;
	if(r->min.y < bb->min.y)
		bb->min.y = r->min.y;
	if(r->max.x > bb->max.x)
		bb->max.x = r->max.x;
	if(r->max.y > bb->max.y)
		bb->max.y = r->max.y;
}

void
tkpolybound(Point *p, int n, Rectangle *r)
{
	while(n--) {
		if(p->x < r->min.x)
			r->min.x = p->x;
		if(p->y < r->min.y)
			r->min.y = p->y;
		if(p->x > r->max.x)
			r->max.x = p->x;
		if(p->y > r->max.y)
			r->max.y = p->y;
		p++;
	}
}

TkName*
tkctaglook(Tk* tk, TkName *n, char *name)
{
	ulong h;
	TkCanvas *c;
	char *p, *s;
	TkName *f, **l;

	c = TKobj(TkCanvas, tk);

	s = name;
	if(s == nil)
		s = n->name;

	if(strcmp(s, "current") == 0)
		return c->current;

	h = 0;
	for(p = s; *p; p++)
		h += 3*h + *p;

	l = &c->thash[h%TkChash];
	for(f = *l; f; f = f->link)
		if(strcmp(f->name, s) == 0)
			return f;

	if(n == nil)
		return nil;
	n->obj = nil;
	n->link = *l;
	*l = n;
	return n;
}

char*
tkcaddtag(Tk *tk, TkCitem *i, int new)
{
	TkCtag *t;
	TkCanvas *c;
	char buf[16];
	TkName *n, *f, *link;

	c = TKobj(TkCanvas, tk);
	if(new != 0) {
		i->id = ++c->id;
		snprint(buf, sizeof(buf), "%d", i->id);
		n = tkmkname(buf);
		if(n == nil)
			return TkNomem;
		n->link = i->tags;
		i->tags = n;
	}

	for(n = i->tags; n; n = link) {
		link = n->link;
		f = tkctaglook(tk, n, nil);
		if(n != f)
			free(n);

		for(t = i->stag; t; t = t->itemlist)
			if(t->name == f)
				break;
		if(t == nil) {
			t = malloc(sizeof(TkCtag));
			if(t == nil) {
				tkfreename(link);
				return TkNomem;
			}
			t->name = f;
			t->taglist = f->obj;	/* Link onto list names */
			f->obj = t;
			t->item = i;
			t->itemlist = i->stag;	/* Link onto list items */
			i->stag = t;
		}
	}

	if(new != 0) {
		i->tags = tkmkname("all");
		if(i->tags == nil)
			return TkNomem;		/* XXX - Tad: memory leak? */
		return tkcaddtag(tk, i, 0);
	}

	return nil;
}

void
tkfreepoint(TkCpoints *p)
{
	if(p->drawpt != nil)
		free(p->drawpt);
	if(p->parampt != nil)
		free(p->parampt);
}

TkCtag*
tkcfirsttag(TkCitem *ilist, TkCtag* tag)
{
	TkCtag *last, *t;

	last = nil;
	while(ilist) {
		for(t = tag; t; t = t->taglist) {
			if(t->item == ilist) {
				last = t;
				break;
			}
		}
		ilist = ilist->next;
	}
	return last;
}

void
tkmkpen(Image **pen, TkEnv *e, Image *stipple)
{
	int locked;
	Display *d;
	Image *new, *fill, *bg;

	bg = tkgc(e, TkCbackgnd);
	fill = tkgc(e, TkCforegnd);

	d = e->top->screen->display;
	locked = lockdisplay(d, 0);
	if(*pen != nil) {
		freeimage(*pen);
		*pen = nil;
	}
	if(stipple == nil) {
		if(locked)
			unlockdisplay(d);
		return;
	}

	if(fill == nil)
		fill = d->ones;
	new = allocimage(d, stipple->r, 3, 1, 0);
	if(new != nil) {
		draw(new, stipple->r, bg, d->ones, tkzp);
		draw(new, stipple->r, fill, stipple, tkzp);
	}
	else
		new = fill;
	if(locked)
		unlockdisplay(d);
	*pen = new;
}

Point
tkcvsanchor(Point dp, int w, int h, int anchor)
{
	Point o;

	if(anchor & (Tknorth|Tknortheast|Tknorthwest))
		o.y = dp.y;
	else
	if(anchor & (Tksouth|Tksoutheast|Tksouthwest))
		o.y = dp.y - h;
	else
		o.y = dp.y - h/2;

	if(anchor & (Tkwest|Tknorthwest|Tksouthwest))
		o.x = dp.x;
	else
	if(anchor & (Tkeast|Tknortheast|Tksoutheast))
		o.x = dp.x - w;
	else
		o.x = dp.x - w/2;

	return o;
}

static int
tkcvsinline(TkCitem *i, Point p)
{
	TkCline *l;
	Point *a, *b;
	int w, npoints, z, nx, ny, nrm;

	l = TKobj(TkCline, i);
	w =TKF2I(l->width) + 2;	/* 2 for slop */
	npoints = i->p.npoint;
	a = i->p.drawpt;
	while(npoints-- > 1) {
		b = a+1;
		nx = a->y - b->y;
		ny = b->x - a->x;
		nrm = (nx < 0.? -nx : nx) + (ny < 0.? -ny : ny);
		if(nrm)
			z = (p.x-b->x)*nx/nrm + (p.y-b->y)*ny/nrm;
		else
			z = (p.x-b->x) + (p.y-b->y);
		if(z < 0)
			z = -z;
		if(z < w)
			return 1;
		a++;
	}
	return 0;
}

static TkCitem*
tkcvsmousefocus(TkCanvas *c, Point p)
{
	TkCitem *i, *s;

	s = nil;
	for(i = c->head; i; i = i->next)
		if(ptinrect(p, i->p.bb)) {
			if(i->type == TkCVline && !tkcvsinline(i, p))
				continue;
			s = i;
		}

	return s;
}

static void
tkcvsdeliver(Tk *tk, TkCitem *i, int event, void *data)
{
	Tk *ftk;
	TkMouse m;
	TkCtag *t;
	TkCwind *w;
	Point mp, g;
	TkCanvas *c;
	TkAction *a;

	if(i->type == TkCVwindow) {
		w = TKobj(TkCwind, i);
		if(w->sub == nil)
			return;

		if(event & TkEmouse) {
			m = *(TkMouse*)data;
			g = tkposn(tk);
			c = TKobj(TkCanvas, tk);
			mp.x = m.x - (g.x + tk->borderwidth) + c->view.x;
			mp.y = m.y - (g.y + tk->borderwidth) + c->view.y;
			ftk = tkinwindow(w->sub, mp);
			if(ftk != w->focus) {
				tkdeliver(w->focus, TkLeave, nil);
				tkdeliver(ftk, TkEnter, nil);
				w->focus = ftk;
			}
			if(ftk != nil)
				tkdeliver(ftk, event, &m);
		}
		else
		if(w->sub != nil)
			tkdeliver(w->sub, event, data);
		return;
	}

	for(t = i->stag; t != nil; t = t->itemlist) {
		a = t->name->prop.binds;
		if(a != nil)
			tksubdeliver(tk, a, event, data);
	}
}

void
tkcvsevent(Tk *tk, int event, void *data)
{
	TkMouse m;
	TkCitem *f;
	Point mp, g;
	TkCanvas *c;

	c = TKobj(TkCanvas, tk);

	if(event == TkLeave && c->mouse != nil) {
		tkcvsdeliver(tk, c->mouse, TkLeave, nil);
		c->mouse = nil;
	}

	if(event & TkEmouse) {
		m = *(TkMouse*)data;
		g = tkposn(tk);
		mp.x = (m.x - g.x - tk->borderwidth) + c->view.x;
		mp.y = (m.y - g.y - tk->borderwidth) + c->view.y;
		f = tkcvsmousefocus(c, mp);
		if(c->mouse != f) {
			if(c->mouse != nil) {
				tkcvsdeliver(tk, c->mouse, TkLeave, nil);
				c->current->obj = nil;
			}
			if(f != nil) {
				c->current->obj = &c->curtag;
				c->curtag.item = f;
				tkcvsdeliver(tk, f, TkEnter, nil);
			}
			c->mouse = f;
		}
		f = c->mouse;
		if(f != nil)
			tkcvsdeliver(tk, f, event, &m);
		tksubdeliver(tk, tk->binds, event, data);
	}

	if(event & TkKey) {
		f = c->focus;
		if(f != nil)
			tkcvsdeliver(tk, f, event, data);
		tksubdeliver(tk, tk->binds, event, data);
	}
}
