#include "lib9.h"
#include "image.h"
#include "tk.h"

#define	O(t, e)		((long)(&((t*)0)->e))

/* Image Options (+ means implemented)
	+anchor
	+image
*/

typedef struct TkCimag TkCimag;
struct TkCimag
{
	int	anchor;
	Point	anchorp;
	TkImg*	tki;
};

static
TkOption imgopts[] =
{
	"anchor",	OPTstab,	O(TkCimag, anchor),	tkanchor,
	"image",	OPTimag,	O(TkCimag, tki),	nil,
	nil
};

static
TkOption itemopts[] =
{
	"tags",		OPTctag,	O(TkCitem, tags),	nil,
	"foreground",	OPTcolr,	O(Tk, env),		IAUX(TkCforegnd),
	"background",	OPTcolr,	O(Tk, env),		IAUX(TkCbackgnd),
	"fg",		OPTcolr,	O(Tk, env),		IAUX(TkCforegnd),
	"bg",		OPTcolr,	O(Tk, env),		IAUX(TkCbackgnd),
	nil
};

void
tkcvsimgsize(TkCitem *i)
{
	Point o;
	int dx, dy;
	TkCimag *t;

	t = TKobj(TkCimag, i);
	i->p.bb = bbnil;
	if(t->tki == nil)
		return;

	dx = t->tki->w;
	dy = t->tki->h;

	o = tkcvsanchor(i->p.drawpt[0], dx, dy, t->anchor);

	t->anchorp = o;
	i->p.bb.min.x = o.x;
	i->p.bb.min.y = o.y;
	i->p.bb.max.x = o.x + dx;
	i->p.bb.max.y = o.y + dy;
}

char*
tkcvsimgcreat(Tk* tk, char *arg, char **val)
{
	char *e;
	TkCimag *t;
	TkCitem *i;
	TkCanvas *c;
	TkOptab tko[3];

	c = TKobj(TkCanvas, tk);

	i = tkcnewitem(tk, TkCVimage, sizeof(TkCitem)+sizeof(TkCimag));
	if(i == nil)
		return TkNomem;

	t = TKobj(TkCimag, i);

	e = tkparsepts(tk->env->top, &i->p, &arg);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}
	if(i->p.npoint != 1) {
		tkcvsfreeitem(i);
		return TkFewpt;
	}

	tko[0].ptr = t;
	tko[0].optab = imgopts;
	tko[1].ptr = i;
	tko[1].optab = itemopts;
	tko[2].ptr = nil;
	e = tkparse(tk->env->top, arg, tko, nil);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}
	
	e = tkcaddtag(tk, i, 1);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}

	tkcvsimgsize(i);

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
tkcvsimgcget(TkCitem *i, char *arg, char **val)
{
	TkOptab tko[3];
	TkCimag *t = TKobj(TkCimag, i);

	tko[0].ptr = t;
	tko[0].optab = imgopts;
	tko[1].ptr = i;
	tko[1].optab = itemopts;
	tko[2].ptr = nil;

	return tkgencget(tko, arg, val);
}

char*
tkcvsimgconf(Tk *tk, TkCitem *i, char *arg)
{
	char *e;
	TkOptab tko[3];
	TkCimag *t = TKobj(TkCimag, i);

	tko[0].ptr = t;
	tko[0].optab = imgopts;
	tko[1].ptr = i;
	tko[1].optab = itemopts;
	tko[2].ptr = nil;

	e = tkparse(tk->env->top, arg, tko, nil);
	tkcvsimgsize(i);
	return e;
}

void
tkcvsimgfree(TkCitem *i)
{
	TkCimag *t;

	t = TKobj(TkCimag, i);
	if(t->tki)
		tkimgput(t->tki);
}

void
tkcvsimgdraw(Image *img, TkCitem *i)
{
	TkEnv *e;
	TkCimag *t;
	TkImg *tki;
	Rectangle r;
	Image *mask, *fg;

	e = i->env;
	t = TKobj(TkCimag, i);
	tki = t->tki;
	if(tki == nil)
		return;
	fg = tki->fgimg;
	if(fg == nil)
		return;

	mask = img->display->ones;
	if(tki->maskimg != nil)
		mask = tki->maskimg;

	r.min = t->anchorp;
	r.max = r.min;
	r.max.x += tki->w;
	r.max.y += tki->h;

	gendraw(img, r, tkgc(e, TkCbackgnd), r.min, img->display->ones, tkzp);
	draw(img, r, tki->fgimg, mask, tkzp);
}

char*
tkcvsimgcoord(TkCitem *i, char *arg, int x, int y)
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
	tkcvsimgsize(i);
	return nil;
}
