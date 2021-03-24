#include <lib9.h>
#include <kernel.h>
#include "image.h"
#include "tk.h"

#define	O(t, e)		((long)(&((t*)0)->e))

typedef struct TkScale TkScale;
struct TkScale
{
	int	value;
	int	bigi;
	int	digits;
	int	digwidth;
	int	from;		/* Base of range */
	int	to;		/* Limit of range */
	int	len;		/* Length of groove */
	int	res;		/* Resolution */
	int	sv;		/* Show value */
	int	sl;		/* Slider length */
	int	sw;		/* Slider width div 2 */
	int	relief;
	int	tick;
	int	orient;
	char*	command;
	char*	label;
	int	pixmin;
	int	pixmax;
	int	pixpos;
	int	center;
	int	pix;
	int	base;
};

static
TkOption opts[] =
{
	"bigincrement",		OPTdist,	O(TkScale, bigi),	nil,
	"digits",		OPTdist,	O(TkScale, digits),	nil,
	"from",			OPTfrac,	O(TkScale, from),	nil,
	"to",			OPTfrac,	O(TkScale, to),		nil,
	"length",		OPTdist,	O(TkScale, len),	nil,
	"resolution",		OPTfrac,	O(TkScale, res),	nil,
	"showvalue",		OPTstab,	O(TkScale, sv),		tkbool,
	"sliderlength",		OPTdist,	O(TkScale, sl),		nil,
	"sliderrelief",		OPTstab,	O(TkScale, relief),	tkrelief,
	"tickinterval",		OPTfrac,	O(TkScale, tick),	nil,
	"label",		OPTtext,	O(TkScale, label),	nil,
	"command",		OPTtext,	O(TkScale, command),	nil,
	"orient",		OPTstab,	O(TkScale, orient),	tkorient,
	nil
};

static char trough1[] = "trough1";
static char trough2[] = "trough2";
static char slider[]  = "slider";

static
TkEbind b[] = 
{
	{TkMotion,		"%W tkScaleMotion %x %y"},
	{TkButton1P|TkMotion,	"%W tkScaleDrag %x %y"},
	{TkButton1P,		"%W tkScaleBut1P %x %y"},
	{TkButton1P|TkDouble,	"%W tkScaleBut1P %x %y"},
	{TkButton1R,		"%W tkScaleBut1R; %W tkScaleMotion %x %y"},
};

enum
{
	Scalewidth	= 22,
	ScalePad	= 2,
	ScaleBW		= 2,
	ScaleSlider	= 18,
	ScaleLen	= 80
};

void
tksizescale(Tk *tk)
{
	Point p;
	char buf[32];
	TkScale *tks;
	int fh, w, h, digits;

	tks = TKobj(TkScale, tk);

	digits = tks->digits;
	if(digits <= 0)
		digits = tkfprint(buf, tks->to) - buf;

	digits *= tk->env->wzero;
	if(tks->sv != BoolT)
		digits = 0;

	tks->digwidth = digits;

	p = tkstringsize(tk, tks->label);
	if(tks->orient == Tkvertical) {
		w = p.x + digits + Scalewidth + ScalePad*4 + 2*ScaleBW;
		h = tks->len + 2*ScaleBW + ScalePad*2;
	}
	else {
		w = tks->len + ScaleBW + 2*ScalePad;
		if(w < p.x)
			w = p.x;
		h = 4*ScalePad + Scalewidth;
		fh = tk->env->font->height;
		if(tks->label != nil)
			h += fh;
		if(tks->sv == BoolT)
			h += fh;
	}
	if(!(tk->flag & Tksetwidth))
		tk->req.width = w;
	if(!(tk->flag & Tksetheight))
		tk->req.height = h;
}

char*
tkscale(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	char *e;
	TkName *names;
	TkScale *tks;
	TkOptab tko[3];

	tk = tknewobj(t, TKscale, sizeof(Tk)+sizeof(TkScale));
	if(tk == nil)
		return TkNomem;

	tks = TKobj(TkScale, tk);
	tks->res = TKI2F(1);
	tks->to = TKI2F(100);
	tks->len = ScaleLen;
	tks->orient = Tkvertical;
	tks->relief = TKraised;
	tks->sl = ScaleSlider;
	tks->sv = BoolT;

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tks;
	tko[1].optab = opts;
	tko[2].ptr = nil;

	names = nil;
	e = tkparse(t, arg, tko, &names);
	if(e != nil) {
		tkfreeobj(tk);
		return e;
	}

	tksizescale(tk);

	e = tkbindings(t, tk, b, nelem(b));

	if(e != nil) {
		tkfreeobj(tk);
		return e;
	}

	e = tkaddchild(t, tk, &names);
	tkfreename(names);
	if(e != nil) {
		tkfreeobj(tk);
		return e;
	}
	tk->name->link = nil;

	return tkvalue(ret, "%s", tk->name->name);
}

char*
tkscalecget(Tk *tk, char *arg, char **val)
{
	TkOptab tko[3];
	TkScale *tks = TKobj(TkScale, tk);

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tks;
	tko[1].optab = opts;
	tko[2].ptr = nil;

	return tkgencget(tko, arg, val);
}

void
tkfreescale(Tk *tk)
{
	TkScale *tks = TKobj(TkScale, tk);

	if(tks->command != nil)
		free(tks->command);
	if(tks->label != nil)
		free(tks->label);
}

static void
tkscalehoriz(Tk *tk, Image *i)
{
	TkEnv *e;
	char sv[32];
	TkScale *tks;
	Image *d, *l;
	Rectangle r;
	Point q, o, p;
	int fh, sh, v, sb, w, h, bw2, len;

	e = tk->env;
	tks = TKobj(TkScale, tk);

	fh = e->font->height;
	p.x = tk->borderwidth + ScalePad;
	p.y = p.x;
	if(tks->label != nil) {
		string(i, p, tkgc(e, TkCforegnd), p, e->font, tks->label);
		p.y += e->font->height;
	}
	p.y += 2*ScalePad;

	sh = 0;
	if(tks->label)
		sh += fh;
	if(tks->sv == BoolT) {
		sh += fh;
		p.y += fh;
	}

	bw2 = 2*tk->borderwidth;
	w = tk->act.width - 2*ScalePad - 2*ScaleBW - bw2;
	h = tk->act.height - 4*ScalePad - sh - 2*ScaleBW - bw2;

	l = tkgc(e, TkCbackgndlght);
	d = tkgc(e, TkCbackgnddark);
	tkbevel(i, p, w, h, ScaleBW, d, l);

	tks->pixmin = p.x;
	tks->pixmax = p.x + w;

	sh = h - 2*ScaleBW;
	tks->sw = sh/2;

	w -= (tks->sl + 2*ScaleBW);
	o = p;
	o.y += ScaleBW;
	v = TKF2I(tks->value-tks->from);
	len = TKF2I(tks->to-tks->from);
	o.x += ScaleBW;
	if (len!=0)
		o.x += (w*v)/len;

	q = o;
	q.x += tks->sl/2 + 1;
	q.y++; 
	sb = sh - 2;
	if(tk->flag & Tkactivated) {
		r.min = o;
		r.max.x = o.x+tks->sl;
		r.max.y = o.y+sh;
		draw(i, r, tkgc(e, TkCactivebgnd), i->display->ones, tkzp);
	}
	switch(tks->relief) {
	case TKsunken:
		tkbevel(i, o, tks->sl, sh, ScaleBW, d, l);
		tkbevel(i, q, 0, sb, 1, l, d);
		break;
	case TKraised:
		tkbevel(i, o, tks->sl, sh, ScaleBW, l, d);
		tkbevel(i, q, 0, sb, 1, d, l);
		break;
	}
	tks->pixpos = o.x;
	tks->center = o.y + sh/2;

	if(tks->sv != BoolT)
		return;

	o.x += tks->sl/2;
	o.y -= fh + ScalePad;
	tkfprint(sv, tks->value);
	if(tks->digits > 0 && tks->digits < strlen(sv))
		sv[tks->digits] = '\0';

	w = stringwidth(e->font, sv);
	o.x -= w/2;
	if(o.x < tks->pixmin)
		o.x = tks->pixmin;
	if(o.x+w > tks->pixmax)
		o.x = tks->pixmax - w;
	
	string(i, o, tkgc(e, TkCforegnd), o, e->font, sv);
}

static void
tkscalevert(Tk *tk, Image *i)
{
	TkEnv *e;
	char sv[32];
	TkScale *tks;
	Image *d, *l;
	Rectangle r;
	Point q, o, p, ls;
	int v, sb, sw, w, h, bw2, len;

	e = tk->env;
	tks = TKobj(TkScale, tk);

	ls = tkzp;
	if(tks->label != nil)
		ls = stringsize(e->font, tks->label);

	bw2 = 2*tk->borderwidth;
	h = tk->act.height - 2*ScalePad - 2*ScaleBW - bw2;
	w = tk->act.width - 4*ScalePad - tks->digwidth - ls.x - 2*ScaleBW - bw2;

	p.x = tks->digwidth + 2*ScalePad + tk->borderwidth;
	p.y = ScaleBW + ScalePad + tk->borderwidth;

	l = tkgc(e, TkCbackgndlght);
	d = tkgc(e, TkCbackgnddark);
	tkbevel(i, p, w, h, ScaleBW, d, l);

	tks->pixmin = p.y;
	tks->pixmax = p.y + h;

	sw = w - 2*ScaleBW;
	tks->sw = sw/2;

	h -= (tks->sl + 2*ScaleBW);
	o = p;
	o.x += ScaleBW;
	v = TKF2I(tks->value-tks->from);
	len = TKF2I(tks->to-tks->from);
	o.y += ScaleBW;
	if (len!=0)
		o.y += (h*v)/len;

	q = o;
	q.x++;
	q.y += tks->sl/2 + 1;
	sb = sw - 2;
	if(tk->flag & Tkactivated) {
		r.min = o;
		r.max.x = o.x+sw;
		r.max.y = o.y+tks->sl;
		draw(i, r, tkgc(e, TkCactivebgnd), i->display->ones, tkzp);
	}
	switch(tks->relief) {
	case TKsunken:
		tkbevel(i, o, sw, tks->sl, ScaleBW, d, l);
		tkbevel(i, q, sb, 0, 1, l, d);
		break;
	case TKraised:
		tkbevel(i, o, sw, tks->sl, ScaleBW, l, d);
		tkbevel(i, q, sb, 0, 1, d, l);
		break;
	}
	tks->pixpos = o.y;
	tks->center = o.x + sw/2;

	if(tks->sv == BoolT) {
		o.y += tks->sl/2;
		o.y -= e->font->height/2;
		tkfprint(sv, tks->value);
		if(tks->digits > 0 && tks->digits < strlen(sv))
			sv[tks->digits] = '\0';
		o.x -= stringwidth(e->font, sv) + ScaleBW + ScalePad;
		
		string(i, o, tkgc(e, TkCforegnd), o, e->font, sv);
	}

	if(tks->label != nil) {
		p.x += w + ScalePad + 2*ScaleBW;
		p.y += ScalePad;
		string(i, p, tkgc(e, TkCforegnd), p, e->font, tks->label);
	}
}

char*
tkdrawscale(Tk *tk, Point orig)
{
	Point p;
	TkTop *t;
	TkEnv *env;
	TkScale *tks;
	Rectangle r;
	Image *i, *ones;

	tks = TKobj(TkScale, tk);

	if(tks->value < tks->from)
		tks->value = tks->from;
	if(tks->value > tks->to)
		tks->value = tks->to;

	env = tk->env;
	t = env->top;

	r.min = tkzp;
	r.max.x = tk->act.width + 2*tk->borderwidth;
	r.max.y = tk->act.height + 2*tk->borderwidth;
	i = tkitmp(t, r.max);
	if(i == nil)
		return nil;

	ones = t->screen->display->ones;
	draw(i, r, tkgc(env, TkCbackgnd), ones, tkzp);

	if(tks->orient == Tkvertical)
		tkscalevert(tk, i);
	else
		tkscalehoriz(tk, i);

	tkdrawrelief(i, tk, &tkzp, 0);

	p.x = tk->act.x + orig.x;
	p.y = tk->act.y + orig.y;
	r = rectaddpt(r, p);
	draw(tkimageof(tk), r, i, ones, tkzp);

	return nil;
}

/* Widget Commands (+ means implemented)
	+cget
	+configure
	+coords
	+get
	+identify
	+set
*/

char*
tkscaleconf(Tk *tk, char *arg, char **val)
{
	char *e;
	TkGeom g;
	TkOptab tko[3];
	TkScale *tks = TKobj(TkScale, tk);

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tks;
	tko[1].optab = opts;
	tko[2].ptr = nil;

	if(*arg == '\0')
		return tkconflist(tko, val);

	g = tk->req;
	e = tkparse(tk->env->top, arg, tko, nil);
	tksizescale(tk);
	tkgeomchg(tk, &g);

	tk->flag |= Tkdirty;
	return e;
}

char*
tkscaleposn(TkEnv *env, Tk *tk, char *arg, int *z)
{
	int x, y;
	TkScale *tks = TKobj(TkScale, tk);

	arg = tkfrac(env->top, arg, &x, env);
	if(arg == nil)
		return nil;
	arg = tkfrac(env->top, arg, &y, env);
	if(arg == nil)
		return nil;

	x = TKF2I(x);
	y = TKF2I(y);

	if(tks->orient == Tkvertical) {
		if(z != nil) {
			z[0] = x;
			z[1] = y;
		}
		x = y;
	}
	else {
		if(z != nil) {
			z[0] = y;
			z[1] = x;
		}
	}
	if(x > tks->pixmin && x < tks->pixpos)
		return trough1;
	else
	if(x >= tks->pixpos && x < tks->pixpos+tks->sl)
		return slider;
	else
	if(x >= tks->pixpos+tks->sl && x < tks->pixmax)
		return trough2;

	return "";
}

char*
tkscaleident(Tk *tk, char *arg, char **val)
{
	char *v;

	v = tkscaleposn(tk->env, tk, arg, nil);
	if(v == nil)
		return TkBadvl;
	return tkvalue(val, "%s", v);
}

char*
tkscalecoords(Tk *tk, char *arg, char **val)
{
	int p, x, y, l, value;
	TkScale *tks = TKobj(TkScale, tk);

	value = tks->value;
	if(arg != nil) {
		if(tkfrac(tk->env->top, arg, &value, tk->env) == nil)
			return TkBadvl;
	}

	value -= tks->from;
	p = tks->pixmax - tks->pixmin;
	l = TKF2I(tks->to-tks->from);
	if (l==0)
		p /= 2;
	else
		p = value*p/l;
	p += tks->pixmin;
	if(tks->orient == Tkvertical) {
		x = tks->center;
		y = p;
	}
	else {
		x = p;
		y = tks->center;
	}
	return tkvalue(val, "%d %d", x, y);
}

char*
tkscaleget(Tk *tk, char *arg, char **val)
{
	int x, y, value, v, l;
	char buf[Tkmaxitem];
	TkScale *tks = TKobj(TkScale, tk);

	value = tks->value;
	if(arg[0] != '\0') {
		arg = tkfrac(tk->env->top, arg, &x, tk->env);
		if(arg == nil)
			return TkBadvl;
		arg = tkfrac(tk->env->top, arg, &y, tk->env);
		if(arg == nil)
			return TkBadvl;

		if(tks->orient == Tkvertical)
			v = TKF2I(y);
		else
			v = TKF2I(x);

		if(v < tks->pixmin)
			value = tks->from;
		else
		if(v > tks->pixmax)
			value = tks->to;
		else {
			l = tks->pixmax-tks->pixmin;
			value = 0;
			if (l!=0)
				value = v * ((tks->to-tks->from)/l);
			value += tks->from;
		}
		if(tks->res > 0)
			value = (value/tks->res)*tks->res;
	}
	tkfprint(buf, value);
	return tkvalue(val, "%s", buf);
}

char*
tkscaleset(Tk *tk, char *arg, char **val)
{
	int value;
	TkScale *tks = TKobj(TkScale, tk);

	USED(val);

	if(tkfrac(tk->env->top, arg, &value, tk->env) == nil)
		return TkBadvl;
	if(value < tks->from)
		value = tks->from;
	if(value > tks->to)
		value = tks->to;
	if(tks->res > 0)
		value = (value/tks->res)*tks->res;

	tks->value = value;
	tk->flag |= Tkdirty;
	return nil;		
}

/* tkScaleMotion %x %y */
char*
tkscalemotion(Tk *tk, char *arg, char **val)
{
	int o, z[2];
	char *v;
	TkScale *tks = TKobj(TkScale, tk);

	USED(val);

	v = tkscaleposn(tk->env, tk, arg, z);
	if(v == nil)
		return TkBadvl;

	o = tk->flag;
	if(v != slider || z[0] < tks->center-tks->sw || z[0] > tks->center+tks->sw)
		tk->flag &= ~Tkactivated;
	else
		tk->flag |= Tkactivated;

	if((o & Tkactivated) != (tk->flag & Tkactivated))
		tk->flag |= Tkdirty;

	return nil;
}

char*
tkscaledrag(Tk *tk, char *arg, char **val)
{
	int x, y, v;
	char *e, buf[Tkmaxitem], f[32];
	TkScale *tks = TKobj(TkScale, tk);

	if((tk->flag & Tkactivated) == 0)
		return nil;

	arg = tkfrac(tk->env->top, arg, &x, tk->env);
	if(arg == nil)
		return TkBadvl;
	arg = tkfrac(tk->env->top, arg, &y, tk->env);
	if(arg == nil)
		return TkBadvl;

	if(tks->orient == Tkvertical)
		v = TKF2I(y);
	else
		v = TKF2I(x);

	v -= tks->pix;
	x = tks->pixmax-tks->pixmin;
	if (x!=tks->sl)
		v = tks->base + (v * (tks->to-tks->from))/(x-tks->sl);
	else
		v = tks->base;
	if(tks->res > 0)
		v = (v/tks->res)*tks->res;

	tkfprint(buf, v);
	e = tkscaleset(tk, buf, val);
	if(e == nil && tks->command != nil) {
		tkfprint(f, tks->value);
		snprint(buf, sizeof(buf), "%s %s", tks->command, f);
		e = tkexec(tk->env->top, buf, nil);
	}
	return e;
}

char*
tkscalebut1p(Tk *tk, char *arg, char **val)
{
	int z[2];
	char *v, *e, buf[Tkmaxitem], f[32];
	TkScale *tks = TKobj(TkScale, tk);

	USED(val);
	v = tkscaleposn(tk->env, tk, arg, z);
	if(v == nil)
		return TkBadvl;

	if(v[0] == '\0' || z[0] < tks->center-tks->sw || z[0] > tks->center+tks->sw)
		return nil;

	if(v == trough1) {
		tks->value -= tks->res;
		if(tks->value < tks->from)
			tks->value = tks->from;
	}
	else
	if(v == trough2) {
		tks->value += tks->res;
		if(tks->value > tks->to)
			tks->value = tks->to;
	}
	else {
		tks->relief = TKsunken;
		tks->pix = z[1];
		tks->base = tks->value;
		tk->env->top->ctxt->tkMgrab = tk;
	}
	e = nil;
	if(tks->command != nil) {
		tkfprint(f, tks->value);
		snprint(buf, sizeof(buf), "%s %s", tks->command, f);
		e = tkexec(tk->env->top, buf, nil);
	}

	tk->flag |= Tkdirty;
	return e;
}

char*
tkscalebut1r(Tk *tk, char *arg, char **val)
{
	TkScale *tks = TKobj(TkScale, tk);
	USED(val);
	USED(arg);
	if(tk->flag & Tkactivated) {
		tks->relief = TKraised;
		tk->flag &= ~Tkactivated;
		tk->flag |= Tkdirty;
	}
	tk->env->top->ctxt->tkMgrab = nil;
	return nil;
}
