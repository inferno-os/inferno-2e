#include "lib9.h"
#include "image.h"
#include "tk.h"

#define	O(t, e)		((long)(&((t*)0)->e))

/* Polygon Options (+ means implemented)
	+fill
	+smooth
	+splinesteps
	+stipple
	+tags
	+width
	+outline
*/
static
TkOption polyopts[] =
{
	"width",	OPTfrac,	O(TkCpoly, width),	nil,
	"stipple",	OPTbmap,	O(TkCpoly, stipple),	nil,
	"smooth",	OPTstab,	O(TkCpoly, smooth),	tkbool,
	"splinesteps",	OPTdist,	O(TkCpoly, steps),	nil,
	nil
};

static
TkOption itemopts[] =
{
	"tags",		OPTctag,	O(TkCitem, tags),	nil,
	"fill",		OPTcolr,	O(TkCitem, env),	IAUX(TkCforegnd),
	"outline",	OPTcolr,	O(TkCitem, env),	IAUX(TkCfill),
	nil
};

void
tkcvspolysize(TkCitem *i)
{
	int w;
	TkCpoly *p;

	p = TKobj(TkCpoly, i);
	w = TKF2I(p->width);

	i->p.bb = bbnil;
	tkpolybound(i->p.drawpt, i->p.npoint, &i->p.bb);
	i->p.bb = insetrect(i->p.bb, -w);
}

char*
tkcvspolycreat(Tk* tk, char *arg, char **val)
{
	char *e;
	TkCpoly *p;
	TkCitem *i;
	TkCanvas *c;
	TkOptab tko[3];

	c = TKobj(TkCanvas, tk);

	i = tkcnewitem(tk, TkCVpoly, sizeof(TkCitem)+sizeof(TkCpoly));
	if(i == nil)
		return TkNomem;

	p = TKobj(TkCpoly, i);
	p->width = TKI2F(1);

	e = tkparsepts(tk->env->top, &i->p, &arg);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}

	tko[0].ptr = p;
	tko[0].optab = polyopts;
	tko[1].ptr = i;
	tko[1].optab = itemopts;
	tko[2].ptr = nil;
	e = tkparse(tk->env->top, arg, tko, nil);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}
	tkmkpen(&p->pen, i->env, p->stipple);

	e = tkcaddtag(tk, i, 1);
	if(e != nil) {
		tkcvsfreeitem(i);
		return e;
	}

	tkcvspolysize(i);

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
tkcvspolycget(TkCitem *i, char *arg, char **val)
{
	TkOptab tko[3];
	TkCpoly *p = TKobj(TkCpoly, i);

	tko[0].ptr = p;
	tko[0].optab = polyopts;
	tko[1].ptr = i;
	tko[1].optab = itemopts;
	tko[2].ptr = nil;

	return tkgencget(tko, arg, val);
}

char*
tkcvspolyconf(Tk *tk, TkCitem *i, char *arg)
{
	char *e;
	TkOptab tko[3];
	TkCpoly *p = TKobj(TkCpoly, i);

	tko[0].ptr = p;
	tko[0].optab = polyopts;
	tko[1].ptr = i;
	tko[1].optab = itemopts;
	tko[2].ptr = nil;

	e = tkparse(tk->env->top, arg, tko, nil);

	tkmkpen(&p->pen, i->env, p->stipple);
	tkcvspolysize(i);

	return e;
}

void
tkcvspolyfree(TkCitem *i)
{
	TkCpoly *p;

	p = TKobj(TkCpoly, i);
	if(p->stipple)
		freeimage(p->stipple);
	if(p->pen)
		freeimage(p->pen);
}

void
tkcvspolydraw(Image *img, TkCitem *i)
{
	int w, j;
	Point *v;
	TkEnv *e;
	TkCpoly *p;
	Image *pen;

	p = TKobj(TkCpoly, i);

	e = i->env;

	pen = p->pen;
	if(pen == nil)
		pen = tkgc(e, TkCforegnd);

	if(i->p.npoint > 0)
		fillpoly(img, i->p.drawpt, i->p.npoint, ~0, pen, i->p.drawpt[0]);

	w = TKF2I(p->width) - 1;
	if(w >= 0 && (e->set & (1<<TkCfill))) {
		pen = tkgc(i->env, TkCfill);
		v = i->p.drawpt;
		for(j = 0; j < i->p.npoint-1; j++) {
			line(img, v[0], v[1], 0, 0, w, pen, v[0]);
			v++;
		}
		line(img, v[0], i->p.drawpt[0], 0, 0, w, pen, v[0]);
	}
}

char*
tkcvspolycoord(TkCitem *i, char *arg, int x, int y)
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
		if(p.npoint < 2) {
			tkfreepoint(&p);
			return TkFewpt;
		}
		tkfreepoint(&i->p);
		i->p = p;
	}
	tkcvspolysize(i);
	return nil;
}
