#include <lib9.h>
#include <kernel.h>
#include "image.h"
#include "tk.h"

#define	O(t, e)		((long)(&((t*)0)->e))

TkOption tklabelopts[] =
{
	"text",		OPTtext,	O(TkLabel, text),	nil,
	"label",	OPTtext,	O(TkLabel, text),	nil,
	"underline",	OPTdist,	O(TkLabel, ul),		nil,
	"anchor",	OPTflag,	O(TkLabel, anchor),	tkanchor,
	"bitmap",	OPTbmap,	O(TkLabel, bitmap),	nil,
	"image",	OPTimag,	O(TkLabel, img),	nil,
	nil
};

char*
tklabel(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	char *e;
	TkLabel *tkl;
	TkName *names;
	TkOptab tko[3];

	tk = tknewobj(t, TKlabel, sizeof(Tk)+sizeof(TkLabel));
	if(tk == nil)
		return TkNomem;

	tkl = TKobj(TkLabel, tk);
	tkl->ul = -1;

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = tklabelopts;
	tko[2].ptr = nil;

	names = nil;
	e = tkparse(t, arg, tko, &names);
	if(e != nil) {
		tkfreeobj(tk);
		return e;
	}

	tksizelabel(tk);

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
tklabelcget(Tk *tk, char *arg, char **val)
{
	TkOptab tko[3];
	TkLabel *tkl = TKobj(TkLabel, tk);

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = tklabelopts;
	tko[2].ptr = nil;

	return tkgencget(tko, arg, val);
}

char*
tklabelconf(Tk *tk, char *arg, char **val)
{
	char *e;
	TkGeom g;
	TkOptab tko[3];
	TkLabel *tkl = TKobj(TkLabel, tk);

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = tklabelopts;
	tko[2].ptr = nil;

	if(*arg == '\0')
		return tkconflist(tko, val);

	g = tk->req;
	e = tkparse(tk->env->top, arg, tko, nil);
	tksizelabel(tk);
	tkgeomchg(tk, &g);

	tk->flag |= Tkdirty;
	return e;
}

void
tksizelabel(Tk *tk)
{
	Point p;
	int w, h;
	TkLabel *tkl;
	
	tkl = TKobj(TkLabel, tk);
	if(tkl->anchor == 0)	
		tkl->anchor = Tkcenter;

	w = 0;
	h = 0;
	if(tkl->img != nil) {
		w = tkl->img->w + 2*Bitpadx;
		h = tkl->img->h + 2*Bitpady;
	}
	else
	if(tkl->bitmap != nil) {
		w = Dx(tkl->bitmap->r) + 2*Bitpadx;
		h = Dy(tkl->bitmap->r) + 2*Bitpady;
	}
	else 
	if(tkl->text != nil) {
		p = tkstringsize(tk, tkl->text);
		w = p.x + 2*Textpadx;
		h = p.y + 2*Textpady;
		if(tkl->ul != -1 && tkl->ul > strlen(tkl->text))
			tkl->ul = -1;
	}

	if(tk->type == TKcheckbutton || tk->type == TKradiobutton) {
		w += CheckSpace;
		if(h < CheckSpace)
			h = CheckSpace;
	}
	tkl->w = w;
	tkl->h = h;
	if((tk->flag & Tksetwidth) == 0)
		tk->req.width = w;
	if((tk->flag & Tksetheight) == 0)
		tk->req.height = h;
}

void
tkfreelabel(Tk *tk)
{
	Image *i;
	int locked;
	Display *d;
	TkLabel *tkl;

	tkl = TKobj(TkLabel, tk);

	if(tkl->text != nil)
		free(tkl->text);
	if(tkl->command != nil)
		free(tkl->command);
	if(tkl->value != nil)
		free(tkl->value);
	if(tkl->variable != nil) {
		tkfreevar(tk->env->top, tkl->variable, tk->flag & Tkswept);
		free(tkl->variable);
	}
	if(tkl->img != nil)
		tkimgput(tkl->img);
	i = tkl->bitmap;
	if(i != nil) {
		d = i->display;
		locked = lockdisplay(d, 0);
		freeimage(i);
		if(locked)
			unlockdisplay(d);
	}
}

void
tktriangle(Point u, Image *i, TkEnv *e)
{	
	int j;
	Point p[3];

	u.y++;
	p[0].x = u.x + CheckButton;
	p[0].y = u.y + CheckButton/2;
	p[1].x = u.x;
	p[1].y = u.y + CheckButton;
	p[2].x = u.x;
	p[2].y = u.y;
	fillpoly(i, p, 3, ~0, tkgc(e, TkCbackgnddark), p[0]);
	for(j = 0; j < 3; j++)
		p[j].y -= 2;
	
	fillpoly(i, p, 3, ~0, tkgc(e, TkCbackgndlght), p[0]);
}

/*
 * draw TKlabel, TKcheckbutton, TKradiobutton
 */
char*
tkdrawlabel(Tk *tk, Point orig)
{
	TkTop *t;
 	TkEnv *e;
	Display *d;
	TkLabel *tkl;
	Rectangle r, s;
	int dx, dy, col;
	Point p, u, v, *pp;
	Image *i, *dst, *cd, *cl, *ct;
	char *o;

	e = tk->env;
	t = e->top;

	dst = tkimageof(tk);
	if(dst == nil)
		return nil;

	v.x = tk->act.width + 2*tk->borderwidth;
	v.y = tk->act.height + 2*tk->borderwidth;

	r.min = tkzp;
	r.max.x = v.x;
	r.max.y = v.y;

	i = tkitmp(t, r.max);
	if(i == nil)
		return nil;

	d = t->screen->display;
	draw(i, r, tkgc(e, TkCbackgnd), d->ones, tkzp);

	p = tkzp;

	tkl = TKobj(TkLabel, tk);

	if(tk->flag & Tkfocus) {
		s.min = p;
		s.max.x = p.x + v.x;
		s.max.y = p.y + v.y;
		draw(i, s, tkgc(e, TkCactivebgnd), d->ones, tkzp);
	}

	p.x += tk->borderwidth;
	p.y += tk->borderwidth;

	dx = tk->act.width - tkl->w - tk->ipad.x;
	dy = tk->act.height - tkl->h - tk->ipad.y;
	if(tkl->anchor & Tkcenter)
		p.y += dy/2;
	else
	if(tkl->anchor & (Tksouth|Tksoutheast|Tksouthwest))
		p.y += dy;

	if(tkl->anchor & Tkcenter)
		p.x += dx/2;
	else
	if(tkl->anchor & (Tkeast|Tknortheast|Tksoutheast))
		p.x += dx;

	switch(tk->type) {
	case TKcheckbutton:
		u.x = p.x + ButtonBorder;
		u.y = p.y + ButtonBorder + (tk->act.height-CheckSpace)/2;

		cd = tkgc(e, TkCbackgnddark);
		cl = tkgc(e, TkCbackgndlght);
		if(tkl->check) {
			tkbevel(i, u, CheckButton, CheckButton, CheckButtonBW, cd, cl);
			u.x += CheckButtonBW;
			u.y += CheckButtonBW;
			s.min = u;
			s.max.x = u.x + CheckButton;
			s.max.y = u.y + CheckButton;
			draw(i, s, tkgc(e, TkCselect), d->ones, tkzp);
		}
		else
			tkbevel(i, u, CheckButton, CheckButton, CheckButtonBW, cl, cd);
		break;
	case TKradiobutton:
		u.x = p.x + ButtonBorder;
		u.y = p.y + ButtonBorder + (tk->act.height-CheckSpace)/2;
		pp = mallocz(4*sizeof(Point), 0);
		if(pp == nil)
			return TkNomem;
		pp[0].x = u.x + CheckButton/2;
		pp[0].y = u.y;
		pp[1].x = u.x + CheckButton;
		pp[1].y = u.y + CheckButton/2;
		pp[2].x = pp[0].x;
		pp[2].y = u.y + CheckButton;
		pp[3].x = u.x;
		pp[3].y = pp[1].y;
		cd = tkgc(e, TkCbackgnddark);
		cl = tkgc(e, TkCbackgndlght);
		if(tkl->check)
			fillpoly(i, pp, 4, ~0, tkgc(e, TkCselect), pp[0]);
		else {
			ct = cl;
			cl = cd;
			cd = ct;
		}
		line(i, pp[0], pp[1], 0, Enddisc, CheckButtonBW/2, cd, pp[0]);
		line(i, pp[1], pp[2], 0, Enddisc, CheckButtonBW/2, cl, pp[1]);
		line(i, pp[2], pp[3], 0, Enddisc, CheckButtonBW/2, cl, pp[2]);
		line(i, pp[3], pp[0], 0, Enddisc, CheckButtonBW/2, cd, pp[3]);
		free(pp);
		break;
	case TKcascade:
		u.x = p.x + v.x - CheckSpace;
		u.y = p.y + ButtonBorder + (tk->act.height-CheckSpace)/2;
		tktriangle(u, i, e);
		break;
	}

	p.x += tk->ipad.x/2;
	p.y += tk->ipad.y/2;
	if(tkl->img && tkl->img->fgimg != nil) {
		p.x += Bitpadx;
		p.y += Bitpady;
		s = rectaddpt(r, p);
		if(tk->type == TKbutton && tk->relief == TKsunken) {
			u.x = 1;
			u.y = 1;
			s = rectaddpt(s, u);
		}
		cd = tkl->img->maskimg;
		if(cd == nil)
			cd = d->ones;
		draw(i, s, tkl->img->fgimg, cd, tkzp);
	}
	else
	if(tkl->bitmap != nil) {
		s.min.x = p.x + Bitpadx;
		s.min.y = p.y + Bitpady;
		s.max.x = s.min.x + Dx(tkl->bitmap->r);
		s.max.y = s.min.y + Dy(tkl->bitmap->r);
		if(tk->type == TKbutton && tk->relief == TKsunken) {
			u.x = 1;
			u.y = 1;
			s = rectaddpt(s, u);
		}
		if(tkl->bitmap->ldepth == 0)
			draw(i, s, tkgc(e, TkCforegnd), tkl->bitmap, tkzp);
		else
			draw(i, s, tkl->bitmap, d->ones, tkzp);
	}
	else
	if(tkl->text != nil) {
		u.x = Textpadx;
		u.y = Textpady;
		if(tk->type == TKbutton && tk->relief == TKsunken) {
			u.x++;
			u.y++;
		}
		if(tk->type == TKcheckbutton || tk->type == TKradiobutton)
			u.x += CheckSpace;
		col = TkCforegnd;
		if(tk->flag & Tkdisabled)
			col = TkCbackgnddark;
		else
		if(tk->flag & Tkfocus)
			col = TkCactivefgnd;
		
		o = tkdrawstring(tk, i, addpt(u, p), tkl->text, tkl->ul, col);
		if(o != nil)
			return o;
	}

	tkdrawrelief(i, tk, &tkzp, 0);

	p.x = tk->act.x + orig.x;
	p.y = tk->act.y + orig.y;
	r = rectaddpt(r, p);
	draw(dst, r, i, d->ones, tkzp);

	return nil;
}
