#include <lib9.h>
#include <kernel.h>
#include "image.h"
#include "tk.h"

char*
tkframe(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	char *e;
	TkOptab tko[2];
	TkName *names;

	tk = tknewobj(t, TKframe, sizeof(Tk)+sizeof(TkFrame));
	if(tk == nil)
		return TkNomem;

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = nil;
	names = nil;

	e = tkparse(t, arg, tko, &names);
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

/*
 * Also used for windows, menus, separators
 */
void
tkfreeframe(Tk *tk)
{
	TkWin *tkw;

	if((tk->flag & Tkwindow) == 0)
		return;

	if(tk->type == TKmenu) {
		tkw = TKobj(TkWin, tk);
		free(tkw->postcmd);
		free(tkw->cascade);
	}

	tkunmap(tk);
}

char*
tkdrawframe(Tk *tk, Point orig)
{	
	int bw2;
	Point p;
	Image *i;
	TkTop *t;
	Tk *f;
	Display *d;
	Rectangle r;

	i = tkimageof(tk);
	if(i == nil)
		return nil;
	
	t = tk->env->top;
	d = t->screen->display;

	p.x = orig.x + tk->act.x;
	p.y = orig.y + tk->act.y;

	bw2 = 2*tk->borderwidth;
	r.min.x = p.x;
	r.min.y = p.y;
	r.max.x = r.min.x + tk->act.width + bw2;
	r.max.y = r.min.y + tk->act.height + bw2;

	draw(i, r, tkgc(tk->env, TkCbackgnd), d->ones, tkzp);

	if(tk->type == TKmenu) {
		/* Make sure slaves are redrawn */
		for(f = tk->slave; f; f = f->next) {
			f->flag |= Tkdirty;
		}
	}
	if(tk->type == TKseparator) {
		r.min.y += (Dy(r) - 4)/2;
		r.max.y = r.min.y+2;
		draw(i, r, tkgc(tk->env, TkCbackgnddark), d->ones, tkzp);
		r.min.y += 2;
		r.max.y += 2;
		draw(i, r, tkgc(tk->env, TkCbackgndlght), d->ones, tkzp);
	}

	tkdrawrelief(i, tk, &p, 0);
	return nil;
}

char*
tkframecget(Tk *tk, char *arg, char **val)
{
	TkOptab tko[3];

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tk;
	tko[1].optab = tktop;
	tko[2].ptr = nil;

	return tkgencget(tko, arg, val);
}

char*
tkframeconf(Tk *tk, char *arg, char **val)
{
	char *e;
	TkGeom g;
	TkOptab tko[3];

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = nil;
	if(tk->flag & Tkwindow) {
		tko[1].ptr = tk;
		tko[1].optab = tktop;
		tko[2].ptr = nil;
	}

	if(*arg == '\0')
		return tkconflist(tko, val);

	g = tk->req;
	e = tkparse(tk->env->top, arg, tko, nil);
	tk->req.x = tk->act.x;
	tk->req.y = tk->act.y;
	tkgeomchg(tk, &g);

	tksetbits(tk, Tkdirty|Tkrefresh);

	return e;
}

char*
tkframemap(Tk *tk, char *arg, char **val)
{
	USED(arg);
	USED(val);
	if(tk->flag & Tkwindow)
		return tkmap(tk);
	return TkNotwm;
}

char*
tkframeunmap(Tk *tk, char *arg, char **val)
{
	USED(arg);
	USED(val);
	if(tk->flag & Tkwindow) {
		tkunmap(tk);
		return nil;
	}
	return TkNotwm;
}

char*
tkframepost(Tk *tk, char *arg, char **val)
{
	int x, y;
	TkTop *t;
	TkWin *tkw;
	char buf[Tkmaxitem];
	char *e;

	USED(val);

	t = tk->env->top;
	if(tk->master != nil || (tk->flag & Tkwindow))
		return TkWpack;

	tk->flag |= Tkwindow;
	tk->geom = tkmoveresize;

	tkw = TKobj(TkWin, tk);
	tkw->next = t->windows;
	t->windows = tk;

	arg = tkword(t, arg, buf, buf+sizeof(buf));
	if(buf[0] == '\0')
		return TkBadvl;
	x = atoi(buf);
	tkword(t, arg, buf, buf+sizeof(buf));
	if(buf[0] == '\0')
		return TkBadvl;
	y = atoi(buf);

	tkmoveresize(tk, x, y, tk->req.width, tk->req.height);
	e = tkmap(tk);
	if(e != nil)
		return e;
	tkpackqit(tk);
	tkrunpack();
	return tkupdate(t);
}

char*
tkframeunpost(Tk *tk, char *arg, char **val)
{
	TkTop *t;
	TkWin *tkw;
	Tk **l, *f;

	USED(arg);
	USED(val);
	if(tk->master != nil || (tk->flag & Tkwindow) == 0)
		return TkNotwm;

	tkunmap(tk);
	t = tk->env->top;
	if(t->root == tk)
		return nil;

	tk->geom = nil;
	tk->flag &= ~Tkwindow;

	tkw = TKobj(TkWin, tk);
	l = &t->windows;
	for(f = *l; f != nil; f = f->next) {
		if(f == tk) {
			*l = tkw->next;
			break;
		}
		l = &TKobj(TkWin, tk)->next;
	}

	return nil;
}
