#include "lib9.h"
#include "image.h"
#include "tk.h"

#define	O(t, e)		((long)(&((t*)0)->e))

TkStab tkselmode[] =
{
	"single",	TKsingle,
	"browse",	TKbrowse,
	"multiple",	TKmultiple,
	"extended",	TKbrowse+TKmultiple,	/* questionable definition */
	nil
};

static
TkOption opts[] =
{
	"xscrollcommand",	OPTtext,	O(TkListbox, xscroll),	nil,
	"yscrollcommand",	OPTtext,	O(TkListbox, yscroll),	nil,
	"selectmode",		OPTstab,	O(TkListbox, selmode),	tkselmode,
	nil
};

static
TkEbind b[] = 
{
	{TkButton1P,		"focus %W;%W tkListbButton1P %y;grab set %W"},
	{TkButton1P|TkMotion,	"%W tkListbButton1MP %y"},
	{TkButton1R,		"grab release %W"},
	{TkMotion,		""},
};

char*
tklistbox(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	char *e;
	TkName *names;
	TkListbox *tkl;
	TkOptab tko[3];

	tk = tknewobj(t, TKlistbox, sizeof(Tk)+sizeof(TkListbox));
	if(tk == nil)
		return TkNomem;

	tk->relief = TKsunken;
	tk->req.width = 170;
	tk->req.height = (tk->env->font->height+2*Listpady)*10 + 2*Frameinset;

	tkl = TKobj(TkListbox, tk);

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = opts;
	tko[2].ptr = nil;

	names = nil;
	e = tkparse(t, arg, tko, &names);
	if(e != nil) {
		tkfreeobj(tk);
		return e;
	}

	if(tk->borderwidth == 0)
		tk->borderwidth = 2;
	if(tk->pad.x == 0)
		tk->pad.x = 8;
	if(tk->pad.y == 0)
		tk->pad.y = 8;

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
tklistbcget(Tk *tk, char *arg, char **val)
{
	TkOptab tko[3];
	TkListbox *tkl = TKobj(TkListbox, tk);

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = opts;
	tko[2].ptr = nil;

	return tkgencget(tko, arg, val);
}

void
tkfreelistb(Tk *tk)
{
	TkLentry *e, *next;
	TkListbox *l = TKobj(TkListbox, tk);

	for(e = l->head; e; e = next) {
		next = e->link;
		free(e);
	}
	if(l->xscroll != nil)
		free(l->xscroll);
	if(l->yscroll != nil)
		free(l->yscroll);
}

char*
tkdrawlistb(Tk *tk, Point orig)
{
	TkTop *t;
	Point o, p;
	TkEnv *env;
	TkLentry *e;
	int fh, w, n;
	Rectangle r, a;
	Image *i, *ones, *fg;
	TkListbox *l = TKobj(TkListbox, tk);

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

	replclipr(i, 0, insetrect(r, Frameinset));

	fh = env->font->height;
	o.x = Listpadx;
	o.y = Frameinset + Listpady + tk->borderwidth;
	n = 0;
	for(e = l->head; e && o.y < r.max.y; e = e->link) {
		if(e->flag & Tkactivated)
			fg = tkgc(env, TkCselectfgnd);
		else
			fg = tkgc(env, TkCforegnd);

		if(n++ < l->yelem)
			continue;

		if(e->flag & Tkactivated) {
			p.x = Frameinset + tk->borderwidth;
			p.y = o.y - Listpady + 1;
			w = tk->act.width-2*Frameinset-2*tk->borderwidth;
			a.min = p;
			a.max = p;
			a.max.x += w+4;
			a.max.y += fh+4;
			draw(i, a, tkgc(env, TkCselectbgnd), ones, tkzp);
			tkbevel(i, p, w, fh+Listpady - 1, 2, 
				tkgc(env, TkCselectbgndlght),
				tkgc(env, TkCselectbgnddark));
		}

		if(l->xelem < e->len) {
			a.max = string(i, o, fg, o, env->font, e->text+l->xelem);
			if(e->flag & Tkfocus) {
				a.max.y += env->font->height;
				a.min = a.max;
				a.min.x = o.x;
				a.max.y += 2;
				draw(i, a, fg, ones, tkzp);
			}
		}
		o.y += fh + 2*Listpady;
	}

	replclipr(i, 0, i->r);

	tkdrawrelief(i, tk, &tkzp, Frameinset);

	p.x = tk->act.x + orig.x;
	p.y = tk->act.y + orig.y;
	r = rectaddpt(r, p);
	draw(tkimageof(tk), r, i, ones, tkzp);

	return nil;
}

int
tklindex(Tk *tk, char *buf)
{
	int index;
	TkListbox *l;
	TkLentry *e, *s;

	l = TKobj(TkListbox, tk);

	if(*buf == '@') {
		while(*buf && *buf != ',')
			buf++;
		return atoi(buf+1)/tk->env->font->height;
	}
	if(*buf >= '0' && *buf <= '9')
		return atoi(buf);

	if(strcmp(buf, "end") == 0) {
		if(l->nitem == 0)
			return 0;
		return l->nitem-1;
	}

	index = 0;
	if(strcmp(buf, "active") == 0)
		s = l->active;
	else
	if(strcmp(buf, "anchor") == 0)
		s = l->anchor;
	else
		return -1;

	for(e = l->head; e; e = e->link) {
		if(e == s)
			return index;
		index++;
	}
	return -1;
}

void
tklistsv(Tk *tk)
{
	TkListbox *l;
	int nl, lh, top, bot;
	char val[Tkminitem], cmd[Tkmaxitem], *v, *e;

	l = TKobj(TkListbox, tk);
	if(l->yscroll == nil)
		return;

	top = 0;
	bot = Tkfpscalar;

	if(l->nitem != 0) {
		lh = tk->env->font->height+2*Listpady;	/* Line height */
		nl = tk->act.height/lh;			/* Lines in the box */
		top = l->yelem*Tkfpscalar/l->nitem;
		bot = (l->yelem+nl)*Tkfpscalar/l->nitem;
	}

	v = tkfprint(val, top);
	*v++ = ' ';
	tkfprint(v, bot);
	snprint(cmd, sizeof(cmd), "%s %s", l->yscroll, val);
	e = tkexec(tk->env->top, cmd, nil);
	if ((e != nil) && (tk->name != nil))
		print("tk: yscrollcommand \"%s\": %s\n", tk->name->name, e);
}

void
tklistsh(Tk *tk)
{
	int nl, top, bot;
	char val[Tkminitem], cmd[Tkmaxitem], *v, *e;
	TkListbox *l = TKobj(TkListbox, tk);

	if(l->xscroll == nil)
		return;

	top = 0;
	bot = Tkfpscalar;

	if(l->nwidth != 0) {
		nl = tk->act.width/tk->env->wzero;
		top = l->xelem*Tkfpscalar/l->nwidth;
		bot = (l->xelem+nl)*Tkfpscalar/l->nwidth;
	}

	v = tkfprint(val, top);
	*v++ = ' ';
	tkfprint(v, bot);
	snprint(cmd, sizeof(cmd), "%s %s", l->xscroll, val);
	e = tkexec(tk->env->top, cmd, nil);
	if ((e != nil) && (tk->name != nil))
		print("tk: xscrollcommand \"%s\": %s\n", tk->name->name, e);
}

void
tklistbgeom(Tk *tk)
{
	tklistsv(tk);
	tklistsh(tk);
}

/* Widget Commands (+ means implemented)
	+activate
	 bbox
	+cget
	+configure
	+curselection
	+delete
	+get
	+index
	+insert
	+nearest
	 scan
	+see
	+selection
	+size
	+xview
	+yview
*/

char*
tklistbconf(Tk *tk, char *arg, char **val)
{
	char *e;
	TkGeom g;
	TkOptab tko[3];
	TkListbox *tkl = TKobj(TkListbox, tk);

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tkl;
	tko[1].optab = opts;
	tko[2].ptr = nil;

	if(*arg == '\0')
		return tkconflist(tko, val);

	g = tk->req;
	e = tkparse(tk->env->top, arg, tko, nil);
	tkgeomchg(tk, &g);

	tk->flag |= Tkdirty;
	return e;
}

char*
tklistbactivate(Tk *tk, char *arg, char **val)
{
	int index;
	TkLentry *e;
	TkListbox *l = TKobj(TkListbox, tk);

	USED(val);
	index = tklindex(tk, arg);
	if(index == -1)
		return TkBadix;

	for(e = l->head; e; e = e->link) {
		if(index-- == 0)
			e->flag |= Tkfocus;
		else
			e->flag &= ~Tkfocus;
	}
	tk->flag |= Tkdirty;
	return nil;
}

char*
tklistbnearest(Tk *tk, char *arg, char **val)
{
	int lh, y, index;
	TkListbox *l = TKobj(TkListbox, tk);

	lh = tk->env->font->height+2*Listpady;	/* Line height */
	y = atoi(arg);
	index = l->yelem + y/lh;
	if(index > l->nitem)
		index = l->nitem;
	return tkvalue(val, "%d", index);
}

char*
tklistbindex(Tk *tk, char *arg, char **val)
{
	int index;
	index = tklindex(tk, arg);
	if(index == -1)
		return TkBadix;
	return tkvalue(val, "%d", index);
}

char*
tklistbsize(Tk *tk, char *arg, char **val)
{
	TkListbox *l = TKobj(TkListbox, tk);

	USED(arg);
	return tkvalue(val, "%d", l->nitem);
}

char*
tklistbinsert(Tk *tk, char *arg, char **val)
{
	int n, index;
	TkListbox *l;
	TkLentry *e, **el;
	char *tbuf, buf[Tkmaxitem];

	USED(val);
	l = TKobj(TkListbox, tk);

	arg = tkword(tk->env->top, arg, buf, buf+sizeof(buf));
	if(strcmp(buf, "end") == 0) {
		el = &l->head;
		if(*el != nil) {
			for(e = *el; e->link; e = e->link)
				;
			el = &e->link;
		}
	}
	else {
		index = tklindex(tk, buf);
		if(index == -1)
			return TkBadix;
		el = &l->head;
		for(e = *el; e && index-- > 0; e = e->link)
			el = &e->link;
	}

	n = strlen(arg);
	if(n > Tkmaxitem) {
		n = (n*3)/2;
		tbuf = malloc(n);
	}
	else {
		tbuf = buf;
		n = sizeof(buf);
	}

	while(*arg) {
		arg = tkword(tk->env->top, arg, tbuf, &tbuf[n]);
		e = malloc(sizeof(TkLentry)+strlen(tbuf)+1);
		if(e == nil)
			return TkNomem;

		e->flag = 0;
		strcpy(e->text, tbuf);
		e->link = *el;
		*el = e;
		el = &e->link;
		e->len = utflen(tbuf);
		if(e->len > l->nwidth)
			l->nwidth = e->len;
		l->nitem++;
	}

	if(tbuf != buf)
		free(tbuf);

	tklistbgeom(tk);
	tk->flag |= Tkdirty;
	return nil;
}

int
tklistbrange(Tk *tk, char *arg, int *s, int *e)
{
	char buf[Tkmaxitem];

	arg = tkword(tk->env->top, arg, buf, buf+sizeof(buf));
	*s = tklindex(tk, buf);
	if(*s == -1)
		return -1;
	*e = *s;
	if(*arg == '\0')
		return 0;

	tkword(tk->env->top, arg, buf, buf+sizeof(buf));
	*e = tklindex(tk, buf);
	if(*e == -1)
		return -1;
	return 0;
}

char*
tklistbselection(Tk *tk, char *arg, char **val)
{
	TkTop *t;
	TkLentry *f;
	TkListbox *l;
	int s, e, indx, selected;
	char buf[Tkmaxitem];

	l = TKobj(TkListbox, tk);

	t = tk->env->top;
	arg = tkword(t, arg, buf, buf+sizeof(buf));
	if(strcmp(buf, "includes") == 0) {
		tkword(t, arg, buf, buf+sizeof(buf));
		indx = tklindex(tk, buf);
		if(indx == -1)
			return TkBadix;
		for(f = l->head; f && indx > 0; f = f->link)
			indx--;
		s = 0;
		if(f && (f->flag&Tkactivated))
			s = 1;
		return tkvalue(val, "%d", s);
	}

	if(strcmp(buf, "anchor") == 0) {
		tkword(t, arg, buf, buf+sizeof(buf));
		indx = tklindex(tk, buf);
		if(indx == -1)
			return TkBadix;
		for(f = l->head; f && indx > 0; f = f->link)
			indx--;
		if(f != nil)
			l->anchor = f;
		return nil;
	}
	indx = 0;
	selected = 0;
	if(strcmp(buf, "clear") == 0) {
		if(tklistbrange(tk, arg, &s, &e) != 0)
			return TkBadix;
		for(f = l->head; f; f = f->link) {
			if(indx <= e && indx++ >= s)
				f->flag &= ~Tkactivated;
			if(f->flag & Tkactivated)
				selected = 1;
		}
		tk->flag |= Tkdirty;
		if (!selected && t->select == tk)
			t->select = nil;
		return nil;
	}
	if(strcmp(buf, "set") == 0) {
		if(tklistbrange(tk, arg, &s, &e) != 0)
			return TkBadix;
		for(f = l->head; f; f = f->link) {
			if(indx <= e && indx++ >= s)
				f->flag |= Tkactivated;
			if(f->flag & Tkactivated)
				selected = 1;
		}
		tk->flag |= Tkdirty;
		if (selected)
			tksetselect(tk);
		return nil;
	}
	return TkBadcm;
}

char*
tklistbdelete(Tk *tk, char *arg, char **val)
{
	TkLentry *e, **el;
	int start, end, indx, bh;
	TkListbox *l = TKobj(TkListbox, tk);

	USED(val);
	if(tklistbrange(tk, arg, &start, &end) != 0)
		return TkBadix;

	indx = 0;
	el = &l->head;
	for(e = l->head; e && indx < start; e = e->link) {
		indx++;
		el = &e->link;
	}
	while(e != nil && indx <= end) {
		*el = e->link;
		if(e->len == l->nwidth)
			l->nwidth = 0;
		free(e);
		e = *el;
		indx++;
		l->nitem--;
	}
	if(l->nwidth == 0) {
		for(e = l->head; e; e = e->link)
			if(e->len > l->nwidth)
				l->nwidth = e->len;
	}
	bh = tk->act.height/(tk->env->font->height+2*Listpady);	/* Box height */
	if(l->yelem + bh > l->nitem)
		l->yelem = l->nitem - bh;
	if(l->yelem < 0)
		l->yelem = 0;

	tklistbgeom(tk);
	tk->flag |= Tkdirty;
	return nil;
}

char*
tklistbget(Tk *tk, char *arg, char **val)
{
	TkLentry *e;
	char *r, *fmt;
	int start, end, indx;
	TkListbox *l = TKobj(TkListbox, tk);

	if(tklistbrange(tk, arg, &start, &end) != 0)
		return TkBadix;

	indx = 0;
	for(e = l->head; e && indx < start; e = e->link)
		indx++;
	fmt = "%s";
	while(e != nil && indx <= end) {
		r = tkvalue(val, fmt, e->text);
		if(r != nil)
			return r;
		indx++;
		fmt = " %s";
		e = e->link;
	}
	return nil;		
}

char*
tklistbcursel(Tk *tk, char *arg, char **val)
{
	int indx;
	TkLentry *e;
	char *r, *fmt;
	TkListbox *l = TKobj(TkListbox, tk);

	USED(arg);
	indx = 0;
	fmt = "%d";
	for(e = l->head; e; e = e->link) {
		if(e->flag & Tkactivated) {
			r = tkvalue(val, fmt, indx);
			if(r != nil)
				return r;
			fmt = " %d";
		}
		indx++;
	}
	return nil;		
}

static char*
tklistbview(Tk *tk, char *arg, char **val, int nl, int *posn, int max)
{
	int top, bot, amount;
	char buf[Tkmaxitem], *v;

	top = 0;
	if(*arg == '\0') {
		if ( max <= nl || max == 0 ) {	/* Double test redundant at
						 * this time, but want to
						 * protect against future
						 * calls. -- DBK */
			top = 0;
			bot = Tkfpscalar;
		}
		else {
			top = *posn*Tkfpscalar/max;
			bot = (*posn+nl)*Tkfpscalar/max;
		}
		v = tkfprint(buf, top);
		*v++ = ' ';
		tkfprint(v, bot);
		return tkvalue(val, "%s", buf);
	}

	arg = tkword(tk->env->top, arg, buf, buf+sizeof(buf));
	if(strcmp(buf, "moveto") == 0) {
		tkfrac(tk->env->top, arg, &top, nil);
		*posn = (top+1)*max/Tkfpscalar;
	}
	else
	if(strcmp(buf, "scroll") == 0) {
		arg = tkword(tk->env->top, arg, buf, buf+sizeof(buf));
		amount = atoi(buf);
		tkword(tk->env->top, arg, buf, buf+sizeof(buf));
		if(buf[0] == 'p')		/* Pages */
			amount *= nl;
		*posn += amount;
	}
	else {
		top = tklindex(tk, buf);
		if(top == -1)
			return TkBadix;
		*posn = top;
	}

	bot = max - nl;
	if(*posn > bot)
		*posn = bot;
	if(*posn < 0)
		*posn = 0;

	tk->flag |= Tkdirty;
	return nil;
}

char*
tklistbsee(Tk *tk, char *arg, char **val)
{
	int bh, index;
	char buf[Tkmaxitem];
	TkListbox *l = TKobj(TkListbox, tk);

	USED(val);
	tkword(tk->env->top, arg, buf, buf+sizeof(buf));
	index = tklindex(tk, buf);
	if(index == -1)
		return TkBadix;

	/* Box height in lines */
	bh = tk->act.height/(tk->env->font->height+2*Listpady);

	/* If the item is already visible, do nothing */
	if (index >= l->yelem && index < l->yelem+bh)
		return nil;

	l->yelem = index - bh/2;
	if(l->yelem+bh > l->nitem)
		l->yelem = l->nitem - bh;
	if(l->yelem < 0)
		l->yelem = 0;

	tklistsv(tk);
	tk->flag |= Tkdirty;
	return nil;
}

char*
tklistbyview(Tk *tk, char *arg, char **val)
{
	int bh;
	char *e;
	TkListbox *l = TKobj(TkListbox, tk);

	bh = tk->act.height/(tk->env->font->height+2*Listpady);	/* Box height */
	e = tklistbview(tk, arg, val, bh, &l->yelem, l->nitem);
	tklistsv(tk);
	return e;
}

char*
tklistbxview(Tk *tk, char *arg, char **val)
{
	int bw;
	char *e;
	TkListbox *l = TKobj(TkListbox, tk);

	bw = tk->act.width/tk->env->wzero;
	e = tklistbview(tk, arg, val, bw, &l->xelem, l->nwidth);
	tklistsh(tk);
	return e;
}

char*
tklistbbutton1(Tk *tk, char *arg, char **val)
{
	int y, indx, flags;
	TkLentry *e;
	TkListbox *l = TKobj(TkListbox, tk);

	USED(val);

	y = atoi(arg);

	flags = Tkdisabled;
	if(l->selmode != TKmultiple)
		flags |= Tkactivated;
	flags = ~flags;
	for(e = l->head; e; e = e->link)
		e->flag &= flags;
	
	indx = y/(tk->env->font->height+2*Listpady);
	indx += l->yelem;
	for(e = l->head; e && indx > 0; e = e->link)
		indx--;
	if(e != nil) {
		if(l->selmode != TKmultiple)
			e->flag |= Tkactivated;
		else
			e->flag ^= Tkactivated;
		e->flag |= Tkdisabled;	/* a temporary usage */
		l->anchor = e;
		tksetselect(tk);
	}
	tk->flag |= Tkdirty;
	return nil;
}

char*
tklistbbutton1m(Tk *tk, char *arg, char **val)
{
	int y, indx, inrange, bh;
	TkLentry *e;
	TkListbox *l = TKobj(TkListbox, tk);
	char buf[Tkminitem];

	USED(val);

	y = atoi(arg);
	/* If outside the box, may need to scroll first */
	if(y < 0) {
		if(l->yelem > 0) {
			l->yelem--;
			tk->flag |= Tkdirty;
		}
		y = 0;
	}
	if(y > tk->act.height) {
		bh = tk->act.height/(tk->env->font->height+2*Listpady);
		if(l->yelem+bh < l->nitem) {
			l->yelem++;
			tk->flag |= Tkdirty;
		}
		y = tk->act.height - tk->env->font->height;
	}

	if(l->selmode == TKsingle)
		return nil;
	if(l->selmode == TKbrowse) {
		snprint(buf, sizeof buf, "%d", y);
		return tklistbbutton1(tk, buf, val);
	}

	indx = y/(tk->env->font->height+2*Listpady);
	indx += l->yelem;
	inrange = 0;
	for(e = l->head; e; e = e->link) {
		if(indx == 0)
			inrange = !inrange;
		if(e == l->anchor)
			inrange = !inrange;
		if(inrange || e == l->anchor || indx == 0) {
			if(l->selmode != TKmultiple)
				e->flag |= Tkactivated;
			else {
				/* don't toggle more than once per drag */
				if((e->flag & Tkdisabled) == 0)
					e->flag ^= Tkactivated;
				e->flag |= Tkdisabled;
			}
		}
		else {
			if(l->selmode != TKmultiple)
				e->flag &= ~Tkactivated;
			else {
				if((e->flag & Tkdisabled) != 0)
					e->flag ^= Tkactivated;
				e->flag &= ~Tkdisabled;
			}
		}
		indx--;
	}
	tk->flag |= Tkdirty;
	return nil;
}
