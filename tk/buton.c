#include "lib9.h"
#include "image.h"
#include "tk.h"

#define	O(t, e)		((long)(&((t*)0)->e))

static char* tksetlabvar(TkTop*, TkLabel*, char*);

/* Widget Commands (+ means implemented)
	+cget
	+configure
	+invoke
	+select
	+deselect
	+toggle
 */

static
TkEbind bb[] = 
{
	{TkEnter,	"%W configure -state active"},
	{TkLeave,	"%W configure -state normal -relief raised"},
	{TkButton1P,	"%W configure -relief sunken"},
	{TkButton1R,	"%W configure -relief raised; %W invoke"},
};

static
TkEbind cb[] = 
{
	{TkEnter,		"%W configure -state active"},
	{TkLeave,		"%W configure -state normal"},
	{TkButton1P,		"%W invoke"},
	{TkMotion|TkButton1P, 	"" },
};

TkOption tkbutopts[] =
{
	"text",		OPTtext,	O(TkLabel, text),	nil,
	"label",	OPTtext,	O(TkLabel, text),	nil,
	"underline",	OPTdist,	O(TkLabel, ul),		nil,
	"anchor",	OPTflag,	O(TkLabel, anchor),	tkanchor,
	"command",	OPTtext,	O(TkLabel, command),	nil,
	"bitmap",	OPTbmap,	O(TkLabel, bitmap),	nil,
	"image",	OPTimag,	O(TkLabel, img),	nil,
	nil
};

TkOption tkradopts[] =
{
	"variable",	OPTtext,	O(TkLabel, variable),	nil,
	"value",	OPTtext,	O(TkLabel, value),	nil,
	nil,
};

static char
tkselbut[] = "selectedButton";

char*
tkbutton(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	char *e;
	TkLabel *tkl;
	TkName *names;
	TkOptab tko[3];

	tk = tknewobj(t, TKbutton, sizeof(Tk)+sizeof(TkLabel));
	if(tk == nil)
		return TkNomem;

	tk->relief = TKraised;
	tkl = TKobj(TkLabel, tk);
	tkl->ul = -1;

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = tkbutopts;
	tko[2].ptr = nil;

	names = nil;
	e = tkparse(t, arg, tko, &names);
	if(e != nil) {
		tkfreeobj(tk);
		return e;
	}

	e = tkbindings(t, tk, bb, nelem(bb));

	if(e != nil) {
		tkfreeobj(tk);
		return e;
	}

	if(tk->borderwidth == 0)
		tk->borderwidth = 2;

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
tkbuttoncget(Tk *tk, char *arg, char **val)
{
	TkOptab tko[4];
	TkLabel *tkl = TKobj(TkLabel, tk);

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = tkbutopts;
	tko[2].ptr = nil;
	if(tk->type == TKradiobutton || tk->type == TKcheckbutton) {
		tko[2].ptr = tkl;
		tko[2].optab = tkradopts;
		tko[3].ptr = nil;
	}
	return tkgencget(tko, arg, val);
}

char*
tkbuttonconf(Tk *tk, char *arg, char **val)
{
	char *e;
	TkGeom g;
	TkOptab tko[4];
	TkLabel *tkl = TKobj(TkLabel, tk);

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = tkbutopts;
	tko[2].ptr = nil;
	if(tk->type == TKradiobutton || tk->type == TKcheckbutton) {
		tko[2].ptr = tkl;
		tko[2].optab = tkradopts;
		tko[3].ptr = nil;
	}

	if(*arg == '\0')
		return tkconflist(tko, val);

	g = tk->req;
	e = tkparse(tk->env->top, arg, tko, nil);
	tksizelabel(tk);
	tkgeomchg(tk, &g);

	tk->flag |= Tkdirty;
	return e;
}

char*
tkbuttonselect(Tk *tk, char *arg, char **val)
{
	char *e;
	TkLabel *tkl = TKobj(TkLabel, tk);

	USED(arg);
	USED(val);
	tkl->check = 1;
	e = tksetlabvar(tk->env->top, tkl, "1");
	if(e != nil)
		return e;
	tk->flag |= Tkdirty;
	return nil;
}

char*
tkbuttondeselect(Tk *tk, char *arg, char **val)
{
	char *e;
	TkLabel *tkl = TKobj(TkLabel, tk);

	USED(arg);
	USED(val);
	tkl->check = 0;
	e = tksetlabvar(tk->env->top, tkl, tk->type == TKradiobutton ? nil : "0");
	if(e != nil)
		return e;
	tk->flag |= Tkdirty;
	return nil;
}

char*
tkcheckbutton(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	char *e;
	TkLabel *tkl;
	TkName *names;
	TkOptab tko[4];

	tk = tknewobj(t, TKcheckbutton, sizeof(Tk)+sizeof(TkLabel));
	if(tk == nil)
		return TkNomem;

	tkl = TKobj(TkLabel, tk);
	tkl->ul = -1;

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = tkbutopts;
	tko[2].ptr = tkl;
	tko[2].optab = tkradopts;
	tko[3].ptr = nil;

	names = nil;
	e = tkparse(t, arg, tko, &names);
	if(e != nil)
		goto err;

	e = tkbindings(t, tk, cb, nelem(cb));

	if(e != nil)
		goto err;

	tksizelabel(tk);

	e = tkaddchild(t, tk, &names);
	tkfreename(names);
	if(e != nil)
		goto err;
	tk->name->link = nil;

	e = tksetlabvar(tk->env->top, tkl, "0");
	if(e != nil)
		goto err;

	return tkvalue(ret, "%s", tk->name->name);

err:
	tkfreeobj(tk);
	return e;
}

char*
tkradiobutton(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	char *e;
	TkLabel *tkl;
	TkName *names;
	TkOptab tko[4];

	tk = tknewobj(t, TKradiobutton, sizeof(Tk)+sizeof(TkLabel));
	if(tk == nil)
		return TkNomem;

	tkl = TKobj(TkLabel, tk);
	tkl->ul = -1;

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = tkbutopts;
	tko[2].ptr = tkl;
	tko[2].optab = tkradopts;
	tko[3].ptr = nil;

	names = nil;
	e = tkparse(t, arg, tko, &names);
	if(e != nil)
		goto err;

	e = tkbindings(t, tk, cb, nelem(cb));

	if(e != nil)
		goto err;

	tksizelabel(tk);

	e = tkaddchild(t, tk, &names);
	tkfreename(names);
	if(e != nil)
		goto err;
	tk->name->link = nil;

	return tkvalue(ret, "%s", tk->name->name);
err:
	tkfreeobj(tk);
	return e;
}

static void
tkradiovar(char *var, Tk *f)
{
	char *s;
	TkLabel *tkl;

	tkl = TKobj(TkLabel, f);
	s = tkl->variable;
	if(s == nil)
		s = tkselbut;
	if(strcmp(s, var) == 0) {
		tkl->check = 0;
		f->flag |= Tkdirty;
	}
}

static char*
tksetlabvar(TkTop *top, TkLabel *tkl, char *newval)
{
	char *c;
	TkVar *v;

	c = tkl->variable;
	if(c == nil)
		c = tkselbut;

	v = tkmkvar(top, c, TkVstring);
	if(v == nil)
		return TkNomem;
	if(v->type != TkVstring)
		return TkNotvt;

	if(v->value != nil)
		free(v->value);

	if(newval == nil)
		newval = "";
	v->value = strdup(newval);
	if(v->value == nil)
		return TkNomem;
	return nil;
}

char*
tkbuttontoggle(Tk *tk, char *arg, char **val)
{
	char *e = nil;
	TkLabel *tkl = TKobj(TkLabel, tk);

	USED(arg);
	USED(val);
	if(tk->flag & Tkdisabled)
		return nil;
	if(tk->type == TKcheckbutton) {
		tkl->check = !tkl->check;
		tk->flag |= Tkdirty;
		e = tksetlabvar(tk->env->top, tkl, tkl->check? "1" : "0");
	}
	return e;
}

char*
tkbuttoninvoke(Tk *tk, char *arg, char **val)
{
	char *e;
	TkLabel *tkl = TKobj(TkLabel, tk);

	if(tk->flag & Tkdisabled)
		return nil;
	e = tkbuttontoggle(tk, arg, val);
	if(e != nil)
		return e;
	if(tkl->command != nil)
		return tkexec(tk->env->top, tkl->command, val);
	return nil;
}

char*
tkradioinvoke(Tk *tk, char *arg, char **val)
{
	char *c, *e;
	Tk *f, *m;
	TkTop *top;
	TkLabel *tkl = TKobj(TkLabel, tk);

	USED(arg);

	if(tk->flag & Tkdisabled)
		return nil;

	top = tk->env->top;
	tkl->check = 1;
	tk->flag |= Tkdirty;

	e = tksetlabvar(top, tkl, tkl->value);
	if(e != nil)
		return e;
	c = tkl->variable;
	if(c == nil)
		c = tkselbut;

	for(f = top->root; f; f = f->siblings) {
		if(f->type == TKmenu) {
			for(m = f->slave; m; m = m->next)
				if(m->type == TKradiobutton && m != tk)
					tkradiovar(c, m);
		}
		else
		if(f->type == TKradiobutton && f != tk)
			tkradiovar(c, f);
	}
	if(tkl->command != nil)
		return tkexec(tk->env->top, tkl->command, val);

	return nil;
}
