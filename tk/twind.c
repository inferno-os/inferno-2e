#include "lib9.h"
#include "image.h"
#include "tk.h"

#define istring u.string
#define iwin u.win
#define imark u.mark
#define iline u.line

#define	O(t, e)		((long)(&((t*)0)->e))

static char* tktwincget(Tk*, char*, char**);
static char* tktwinconfigure(Tk*, char*, char**);
static char* tktwincreate(Tk*, char*, char**);
static char* tktwinnames(Tk*, char*, char**);

static
TkOption twinopts[] =
{
	"align",	OPTstab,	O(TkTwind, align),	tkalign,
	"create",	OPTtext,	O(TkTwind, create),	nil,
	"padx",		OPTdist,	O(TkTwind, padx),	nil,
	"pady",		OPTdist,	O(TkTwind, pady),	nil,
	"stretch",	OPTstab,	O(TkTwind, stretch),	tkbool,
	"window",	OPTwinp,	O(TkTwind, sub),	nil,
	nil
};

TkCmdtab
tktwincmd[] =
{
	"cget",		tktwincget,
	"configure",	tktwinconfigure,
	"create",	tktwincreate,
	"names",	tktwinnames,
	nil
};

int
tktfindsubitem(Tk *sub, TkTindex *ix)
{
	Tk *tk, *isub;
	TkText *tkt;

	tk = sub->parent;
	if(tk != nil) {
		tkt = TKobj(TkText, tk);
		tktstartind(tkt, ix);
		while(tktadjustind(tkt, TkTbyitem, ix)) {
			if(ix->item->kind == TkTwin) {
				isub = ix->item->iwin->sub;
				if(isub != nil && 
				   (isub->name != nil) && 
				   strcmp(isub->name->name, sub->name->name) == 0)
				return 1;
			}
		}
	}
	return 0;
}

static void
tktwindsize(Tk *tk, TkTindex *ix)
{
	Tk *s;
	TkTitem *i;
	TkTwind *w;

	i = ix->item;

	w = i->iwin;
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

	tktfixgeom(tk, ix->line->prev, ix->line);
}

static int
tktchkwfocus(TkTwind *w, Tk *tk)
{
	if(w->focus == tk)
		return 1;
	for(tk = tk->slave; tk; tk = tk->next)
		if(tktchkwfocus(w, tk))
			return 1;
	return 0;
}

static void
tktwingeom(Tk *sub, int x, int y, int w, int h)
{
	TkTindex ix;
	Tk *tk;
	TkTwind *win;

	USED(x);
	USED(y);

	tk = sub->parent;
	if(!tktfindsubitem(sub, &ix))
		return;

	win = ix.item->iwin;

	if(win->focus != nil) {
		if(tktchkwfocus(win, sub) == 0)
			win->focus = nil;
	}

	win->width = w;
	win->height = h;

	sub->req.width = w;
	sub->req.height = h;
	tktwindsize(tk, &ix);
}

static void
tktdestroyed(Tk *sub)
{
	TkTindex ix;

	if(tktfindsubitem(sub, &ix)) {
		ix.item->iwin->sub = nil;
		ix.item->iwin->focus = nil;
		sub->parent = nil;
	}
}

void
tktdirty(Tk *sub)
{
	Tk *tk, *parent, *isub;
	TkText *tkt;
	TkTindex ix;

	parent = nil;
	for(tk = sub; tk && parent == nil; tk = tk->master)
		parent = tk->parent;

	tkt = TKobj(TkText, parent);
	tktstartind(tkt, &ix);
	while(tktadjustind(tkt, TkTbyitem, &ix)) {
		if(ix.item->kind == TkTwin) {
			isub = ix.item->iwin->sub;
			if(tkischild(isub, sub)) {
				tktfixgeom(parent, ix.line->prev, ix.line);
				return;
			}
		}
	}
}

static char*
tktwinchk(Tk *tk, TkTwind *w)
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
	tksetbits(sub, Tksubsub);
	sub->geom = tktwingeom;
	sub->destroyed = tktdestroyed;

	if(w->width == 0)
		w->width = sub->req.width;
	if(w->height == 0)
		w->height = sub->req.height;

	return nil;
}

/* Text Window Command (+ means implemented)
	+cget
	+configure
	+create
	+names
*/

static char*
tktwincget(Tk *tk, char *arg, char **val)
{
	char *e;
	TkTindex ix;
	TkOptab tko[2];

	e = tktindparse(tk, &arg, &ix);
	if(e != nil)
		return e;
	if(ix.item->kind != TkTwin)
		return TkBadwp;

	tko[1].ptr = ix.item->iwin;
	tko[1].optab = twinopts;
	tko[2].ptr = nil;

	return tkgencget(tko, arg, val);
}

static char*
tktwinconfigure(Tk *tk, char *arg, char **val)
{
	char *e;
	TkTindex ix;
	TkOptab tko[2];

	USED(val);

	e = tktindparse(tk, &arg, &ix);
	if(e != nil)
		return e;
	if(ix.item->kind != TkTwin)
		return TkBadwp;

	tko[1].ptr = ix.item->iwin;
	tko[1].optab = twinopts;
	tko[2].ptr = nil;

	e = tkparse(tk->env->top, arg, tko, nil);
	if(e != nil)
		return e;

	e = tktwinchk(tk, ix.item->iwin);
	if(e != nil)
		return e;

	tktfixgeom(tk, ix.line->prev, ix.line);
	return nil;
}

static char*
tktwincreate(Tk *tk, char *arg, char **val)
{
	char *e;
	TkTindex ix;
	TkTitem *i;
	TkText *tkt;
	TkOptab tko[2];

	USED(val);

	tkt = TKobj(TkText, tk);

	e = tktindparse(tk, &arg, &ix);
	if(e != nil)
		return e;

	e = tktnewitem(TkTwin, 0, &i);
	if(e != nil)
		return e;

	i->iwin = malloc(sizeof(TkTwind));
	if(i->iwin == nil) {
		tktfreeitems(tkt, i);
		return TkNomem;
	}

	i->iwin->align = Tkcenter;

	tko[0].ptr = i->iwin;
	tko[0].optab = twinopts;
	tko[1].ptr = nil;

	e = tkparse(tk->env->top, arg, tko, nil);
	if(e != nil) {
    err1:
		tktfreeitems(tkt, i);
		return e;
	}

	e = tktwinchk(tk, i->iwin);
	if(e != nil)
		goto err1;

	tktiteminsert(tkt, &ix, i);

	tktadjustind(tkt, TkTbyitemback, &ix);
	tktwindsize(tk, &ix);

	return nil;
}

static char*
tktwinnames(Tk *tk, char *arg, char **val)
{
	char *e, *fmt;
	TkTindex ix;
	TkText *tkt = TKobj(TkText, tk);

	USED(arg);

	tktstartind(tkt, &ix);
	fmt = "%s";
	do {
		if(ix.item->kind == TkTwin 
		   && ix.item->iwin->sub != nil
                   && (ix.item->iwin->sub->name != nil)) {
			e = tkvalue(val, fmt, ix.item->iwin->sub->name->name);
			if(e != nil)
				return e;
			fmt = " %s";
		}
	} while(tktadjustind(tkt, TkTbyitem, &ix));
	return nil;
}
