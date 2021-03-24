#include "lib9.h"
#include "image.h"
#include "tk.h"
#include <kernel.h>
#include <interp.h>

enum
{
	Cmask,
	Cctl,
	Ckey,
	Cbp,
	Cbr
};

struct 
{
	char*	event;
	int	mask;
	int	action;
} etab[] =
{
	"Motion",		TkMotion,	Cmask,
	"Double",		TkDouble,	Cmask,	
	"Map",			TkMap,		Cmask,
	"Unmap",		TkUnmap,	Cmask,
	"Enter",		TkEnter,	Cmask,
	"Leave",		TkLeave,	Cmask,
	"FocusIn",		TkFocusin,	Cmask,
	"FocusOut",		TkFocusout,	Cmask,
	"Configure",		TkConfigure,	Cmask,
	"Control",		0,		Cctl,
	"Key",			0,		Ckey,
	"KeyPress",		0,		Ckey,
	"Button",		0,		Cbp,
	"ButtonPress",		0,		Cbp,
	"ButtonRelease",	0,		Cbr,
};

static
TkOption tkcurop[] =
{
	"x",		OPTdist,	O(TkCursor, hot.x),	nil,
	"y",		OPTdist,	O(TkCursor, hot.y),	nil,
	"bitmap",	OPTbmap,	O(TkCursor, bit),	nil,
	"image",	OPTimag,	O(TkCursor, img),	nil,
	"default",	OPTbool,	O(TkCursor, def),	nil,
	nil
};

TkCursor	tkcursor;

static char*
tkseqitem(char *buf, char *arg)
{
	while(*arg && (*arg == ' ' || *arg == '-'))
		arg++;
	while(*arg && *arg != ' ' && *arg != '-' && *arg != '>')
		*buf++ = *arg++;
	*buf = '\0';
	return arg;
}

int
tkseqparse(char *seq)
{
	Rune r;
	int i, event;
	char *buf;

	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil)
		return -1;

	event = 0;

	while(*seq && *seq != '>') {
		seq = tkseqitem(buf, seq);
	
		for(i = 0; i < nelem(etab); i++)	
			if(strcmp(buf, etab[i].event) == 0)
				break;
	
		if(i >= nelem(etab)) {
			free(buf);
			return -1;
		}
	
	
		switch(etab[i].action) {
		case Cmask:
			event |= etab[i].mask;
			break;
		case Cctl:
			tkseqitem(buf, seq);
			if(buf[0] == '\0') {
				free(buf);
				return -1;
			}
			chartorune(&r, buf);
			if(r <= '~')
				r &= 0x1f;
			free(buf);
			return TkKey|TKKEY(r);	
		case Ckey:
			seq = tkseqitem(buf, seq);
			if(buf[0] != '\0') {
				chartorune(&r, buf);
				event |= TKKEY(r);
			}
			event |= TkKey;
			break;
		case Cbp:
			seq = tkseqitem(buf, seq);
			switch(buf[0]) {
			default:
				free(buf);
				return -1;
			case '\0':
				event |= TkEpress;
				break;
			case '1':
				event |= TkButton1P;
				break;
			case '2':
				event |= TkButton2P;
				break;
			case '3':
				event |= TkButton3P;
				break;
			}
			break;
		case Cbr:
			seq = tkseqitem(buf, seq);
			switch(buf[0]) {
			default:
				free(buf);
				return -1;
			case '\0':
				event |= TkErelease;
				break;
			case '1':
				event |= TkButton1R;
				break;
			case '2':
				event |= TkButton2R;
				break;
			case '3':
				event |= TkButton3R;
				break;
			}
			break;
		}
	}
	free(buf);
	return event;
}

void
tkcmdbind(Tk *tk, int event, void *arg, void *data)
{
	Point p;
	TkMouse *m;
	TkGeom *g;
	int v, len;
	char *e, *s, *c, *ec, *cmd;

	if(arg == nil)
		return;

	s = arg;
	cmd = malloc(2*Tkmaxitem);
	if (cmd == nil) {
		print("tk: bind command \"%s\": %s\n",
			tk->name ? tk->name->name : "(noname)", TkNomem);
		return;
	}

	m = (TkMouse*)data;
	c = cmd;
	ec = cmd+2*Tkmaxitem-1;
	while(*s && c < ec) {
		if(*s != '%') {
			*c++ = *s++;
			continue;
		}
		s++;
		len = ec-c;
		switch(*s++) {
		def:
		default:
			*c++ = s[-1];
			break;
		case '%':
			*c++ = '%';
			break;
		case 'b':
			v = 0;
			if(event & (TkButton1P|TkButton1R))
				v = 1;
			else
			if(event & (TkButton2P|TkButton2R))
				v = 2;
			else
			if(event & (TkButton3P|TkButton3R))
				v = 3;
			c += snprint(c, len, "%d", v);
			break;
		case 'h':
			if((event & TkConfigure) == 0)
				goto def;
			g = (TkGeom*)data;
			c += snprint(c, len, "%d", g->height);
			break;
		case 's':
			if((event & TkEmouse))
				c += snprint(c, len, "%d", m->b);
			else
			if((event & TkKey))
				c += snprint(c, len, "%d", TKKEY(event));
			else
				goto def;
			break;
		case 'w':
			if((event & TkConfigure) == 0)
				goto def;
			g = (TkGeom*)data;
			c += snprint(c, len, "%d", g->width);
			break;
		case 'x':		/* Relative mouse coords */
		case 'y':
			if((event & (TkEmouse|TkEnter|TkLeave)) == 0)
				goto def;
			p = tkposn(tk);
			if(s[-1] == 'x')
				v = m->x - p.x;
			else
				v = m->y - p.y;
			c += snprint(c, len, "%d", v - tk->borderwidth);
			break;
		case 'X':		/* Absolute mouse coords */
		case 'Y':
			if((event & TkEmouse) == 0)
				goto def;
			c += snprint(c, len, "%d", s[-1] == 'X' ? m->x : m->y);
			break;
		case 'A':
			if((event & TkKey) == 0)
				goto def;
			v = TKKEY(event);
			if(v == '{' || v == '}' || v == '\\')
				c += snprint(c, len, "\\%C", v);
			else
			if(v != '\0')
				c += snprint(c, len, "%C", v);
			break;
		case 'K':
			if((event & TkKey) == 0)
				goto def;
			c += snprint(c, len, "%.4X", TKKEY(event));
			break;
		case 'W':
		        if (tk->name != nil) 
			  c += snprint(c, len, "%s", tk->name->name);
			break;
		}
	}
	*c = '\0';

	e = nil;
	if(cmd[0] == '|')
		tkexec(tk->env->top, cmd+1, nil);
	else
	if(cmd[0] != '\0')
		e = tkexec(tk->env->top, cmd, nil);

	if(e == nil) {
		free(cmd);
		return;
	}

	if (tk->name != nil)
	  print("tk: bind command \"%s\": %s: %s\n", tk->name->name, cmd, e);
	free(cmd);
}

char*
tkbind(TkTop *t, char *arg, char **ret)
{
	Rune r;
	Tk *tk;
	TkAction **ap;
	int i, mode, event;
	char *cmd, *tag, *seq;
	char *e;

	USED(ret);

	tag = mallocz(Tkmaxitem, 0);
	if(tag == nil)
		return TkNomem;
	seq = mallocz(Tkmaxitem, 0);
	if(seq == nil) {
		free(tag);
		return TkNomem;
	}

	arg = tkword(t, arg, tag, tag+Tkmaxitem);
	if(tag[0] == '\0') {
		e = TkBadtg;
		goto err;
	}

	arg = tkword(t, arg, seq, seq+Tkmaxitem);
	if(seq[0] == '<') {
		event = tkseqparse(seq+1);
		if(event == -1) {
			e = TkBadsq;
			goto err;
		}
	}
	else {
		chartorune(&r, seq);
		event = TkKey | r;
	}
	if(event == 0) {
		e = TkBadsq;
		goto err;
	}

	arg = tkskip(arg, " \t");

	mode = TkArepl;
	if(*arg == '+') {
		mode = TkAadd;
		arg++;
	}

	if(*arg == '{') {
		cmd = tkskip(arg+1, " \t");
		if(*cmd == '}') {
			tk = tklook(t, tag, 0);
			if(tk == nil) {
				for(i = 0; ; i++) {
					if(i >= TKwidgets) {
						e = TkBadwp;
						goto err;
					}
					if(strcmp(tag, tkmethod[i].name) == 0) {
						ap = &(t->binds[i]);
						break;
					}
				}
			}
			else
				ap = &tk->binds;
			tkcancel(ap, event);
		}
	}

	tkword(t, arg, seq, seq+Tkmaxitem);
	if(tag[0] == '.') {
		tk = tklook(t, tag, 0);
		if(tk == nil) {
			e = TkBadwp;
			goto err;
		}

		cmd = strdup(seq);
		if(cmd == nil) {
			e = TkNomem;
			goto err;
		}
		e = tkaction(&tk->binds, event, TkDynamic, cmd, mode);
		if(e != nil)
			goto err;	/* tkaction does free(cmd) */
		free(tag);
		free(seq);
		return nil;
	}
	if(strcmp(tag, "all") == 0) {
		for(tk = t->root; tk; tk = tk->next) {
			cmd = strdup(seq);
			if(cmd == nil) {
				e = TkNomem;
				goto err;
			}
			e = tkaction(&tk->binds, event, TkDynamic, cmd, mode);
			if(e != nil)
				goto err;
		}
		free(tag);
		free(seq);
		return nil;
	}
	for(i = 0; i < TKwidgets; i++) {
		if(strcmp(tag, tkmethod[i].name) == 0) {
			cmd = strdup(seq);
			if(cmd == nil) {
				e = TkNomem;
				goto err;
			}
			e = tkaction(t->binds + i,event, TkDynamic, cmd, mode);
			if(e != nil)
				goto err;
			free(tag);
			free(seq);
			return nil;
		}
	}

	e = TkBadtg;
err:
	free(tag);
	free(seq);

	return e;
}

char*
tksend(TkTop *t, char *arg, char **ret)
{

	TkVar *v;
	char *var;

	USED(ret);

	var = mallocz(Tkmaxitem, 0);
	if(var == nil)
		return TkNomem;

	arg = tkword(t, arg, var, var+Tkmaxitem);
	v = tkmkvar(t, var, 0);
	free(var);
	if(v == nil)
		return TkBadvr;
	if(v->type != TkVchan)
		return TkNotvt;

	arg = tkskip(arg, " \t");
	if(tktolimbo(v->value, arg) == 0)
		return TkMovfw;

	return nil;
}

char*
tkfocus(TkTop *t, char *arg, char **ret)
{
	TkCtxt *c;
	Tk *tk, *ok;
	char *wp;

	if(*arg == '\0') {
		tk = t->ctxt->tkKgrab;
		if ((tk != nil) && (tk->name != nil))
			return tkvalue(ret, "%s", tk->name->name);
		return nil;
	}

	wp = mallocz(Tkmaxitem, 0);
	if(wp == nil)
		return TkNomem;

	tkword(t, arg, wp, wp+Tkmaxitem);
	tk = tklook(t, wp, 0);
	free(wp);
	if(tk == nil)
		return TkBadwp;

	c = t->ctxt;
	ok = c->tkKgrab;
	if(ok == tk)		/* DBK - no focus events delivered*/
		return nil;		/* DBK - if no widget change */
	if(ok != tk->env->top->root)	/* DBK - don't focus out if was root */
		tkdeliver(c->tkKgrab, TkFocusout, nil);
	c->tkKgrab = tk;
	if(ok != nil && ok->env->top != tk->env->top &&
				ok != ok->env->top->root)	/* DBK */
		tkdeliver(ok->env->top->root, TkFocusout, nil);

	tkdeliver(c->tkKgrab, TkFocusin, nil);
	if(ok == nil || ok->env->top != tk->env->top)
		if(tk != tk->env->top->root) 		/* DBK */
			tkdeliver(tk->env->top->root, TkFocusin, nil);

	if(tk != tk->env->top->root)			/* DBK - ??? */
		c->tkKgrab->flag |= Tkdirty;

	return nil;
}

TkCtxt*
tkdeldepth(Tk *t)
{
	TkCtxt *c;
	Tk *f, **l;

	c = t->env->top->ctxt;
	l = &c->tkdepth;
	for(f = *l; f; f = f->depth) {
		if(f == t) {
			*l = t->depth;
			break;
		}
		l = &f->depth;
	}
	t->depth = nil;
	return c;
}

char*
tkraise(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	TkCtxt *c;
	int locked;
	TkWin *tkw;
	Display *d;
	char *wp;

	USED(ret);

	wp = mallocz(Tkmaxitem, 0);
	if(wp == nil)
		return TkNomem;
	tkword(t, arg, wp, wp+Tkmaxitem);
	tk = tklook(t, wp, 0);
	free(wp);
	if(tk == nil)
		return TkBadwp;

	if((tk->flag & Tkwindow) == 0)
		return TkNotwm;

	c = tkdeldepth(tk);
	tk->depth = c->tkdepth;
	c->tkdepth = tk;

	tkw = TKobj(TkWin, tk);
	if(tkw->image == nil)
		return nil;

	d = t->screen->display;
	locked = lockdisplay(d, 0);
	topwindow(tkw->image);
	if(locked)
		unlockdisplay(d);

	return nil;
}

char*
tklower(TkTop *t, char *arg, char **ret)
{
	TkCtxt *c;
	Tk *tk, *f;
	int locked;
	TkWin *tkw;
	Display *d;
	char *wp;

	USED(ret);
	wp = mallocz(Tkmaxitem, 0);
	if(wp == nil)
		return TkNomem;
	tkword(t, arg, wp, wp+Tkmaxitem);
	tk = tklook(t, wp, 0);
	free(wp);
	if(tk == nil)
		return TkBadwp;

	if((tk->flag & Tkwindow) == 0)
		return TkNotwm;

	c = tkdeldepth(tk);
	if(c->tkdepth == nil)
		c->tkdepth = tk;
	else {
		for(f = c->tkdepth; f->depth != nil; f = f->depth)
			;
		f->depth = tk;
	}

	tkw = TKobj(TkWin, tk);
	if(tkw->image == nil)
		return nil;

	d = t->screen->display;
	locked = lockdisplay(d, 0);
	bottomwindow(tkw->image);
	if(locked)
		unlockdisplay(d);

	return nil;
}

char*
tkgrab(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	TkCtxt *c;
	char *r, *buf, *wp;

	USED(ret);

	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil)
		return TkNomem;

	wp = mallocz(Tkmaxitem, 0);
	if(wp == nil) {
		free(buf);
		return TkNomem;
	}
	arg = tkword(t, arg, buf, buf+Tkmaxitem);

	tkword(t, arg, wp, wp+Tkmaxitem);
	tk = tklook(t, wp, 0);
	free(wp);
	if(tk == nil) {
		free(buf);
		return TkBadwp;
	}

	c = t->ctxt;
	if(strcmp(buf, "release") == 0) {
		free(buf);
		if(c->tkMgrab == tk)
			c->tkMgrab = nil;
		return nil;
	}
	if(strcmp(buf, "set") == 0) {
		free(buf);
		c->tkMgrab = tk;
		return nil;
	}
	if(strcmp(buf, "ifunset") == 0) {
		free(buf);
		if(c->tkMgrab == nil)
			c->tkMgrab = tk;
		return nil;
	}
	if(strcmp(buf, "status") == 0) {
		free(buf);
		r = "none";
		if ((c->tkMgrab != nil) && (c->tkMgrab->name != nil))
			r = c->tkMgrab->name->name;
		return tkvalue(ret, "%s", r);
	}
	free(buf);
	return TkBadcm;
}

char*
tkputs(TkTop *t, char *arg, char **ret)
{
	char *buf;

	USED(ret);

	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil)
		return TkNomem;
	tkword(t, arg, buf, buf+Tkmaxitem);
	print("%s\n", buf);
	free(buf);
	return nil;
}

char*
tkdestroy(TkTop *t, char *arg, char **ret)
{
	int found, len;
	Tk *tk, **l, *next;
	char *n, *e, *buf;

	USED(ret);
	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil)
		return TkNomem;

	e = nil;
	for(;;) {
		arg = tkword(t, arg, buf, buf+Tkmaxitem);
		if(buf[0] == '\0')
			break;

		len = strlen(buf);
		found = 0;
		for(tk = t->root; tk; tk = tk->siblings) {
		        if (tk->name != nil) 
			  n = tk->name->name;
			else
			  n = 0;
			if(strcmp(buf, n) == 0) {
				tk->flag |= Tkdestroy;
				found = 1;
			}
			if(strncmp(buf, n, len) == 0 && n[len] == '.')
				tk->flag |= Tkdestroy;
		}
		if(!found) {
			e = TkBadwp;
			break;
		}
	}
	free(buf);

	for(tk = t->root; tk; tk = tk->siblings) {
		if((tk->flag & Tkdestroy) == 0)
			continue;
		if(tk->destroyed != nil)
			tk->destroyed(tk);
		if(tk->flag & Tkwindow) {
			tkunmap(tk);
			if((tk->name != nil) 
			   && (strcmp(tk->name->name, ".") == 0))
				tk->flag &= ~Tkdestroy;
		}
		tkpackqit(tk->master);
		tkdelpack(tk);
		if(tk->parent != nil && tk->geom != nil)
			tk->geom(tk, 0, 0, 0, 0);
	}
	tkrunpack();

	l = &t->windows;
	for(tk = t->windows; tk; tk = next) {
		next = TKobj(TkWin, tk)->next;
		if(tk->flag & Tkdestroy) {
			*l = next;
			continue;
		}
		l = &TKobj(TkWin, tk)->next;		
	}
	l = &t->root;
	for(tk = t->root; tk; tk = next) {
		next = tk->siblings;
		if(tk->flag & Tkdestroy) {
			*l = next;
			tkfreeobj(tk);
			continue;
		}
		l = &tk->siblings;
	}

	return e;
}

char*
tkupdatecmd(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	int x, y;
	Rectangle *dr;
	char buf[Tkmaxitem];

	USED(ret);

	tkword(t, arg, buf, buf+sizeof(buf));
	if(strcmp(buf, "-onscreen") == 0) {
		tk = t->root;
		dr = &t->screen->display->image->r;
		x = tk->act.x;
		if(x+tk->act.width > dr->max.x)
			x = dr->max.x - tk->act.width;
		if(x < 0)
			x = 0;
		y = tk->act.y;
		if(y+tk->act.height > dr->max.y)
			y = dr->max.y - tk->act.height;
		if(y < 0)
			y = 0;
		tkmoveresize(tk, x, y, tk->act.width, tk->act.height);
	}
	return tkupdate(t);
}

char*
tkvariable(TkTop *t, char *arg, char **ret)
{
	TkVar *v;
	char *fmt, *e;
	char *buf;

	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil)
		return TkNomem;

	tkword(t, arg, buf, buf+Tkmaxitem);
	if(strcmp(buf, "lasterror") == 0) {
		if(t->err == nil)
			return nil;
		fmt = "%s: %s";
		if(strlen(t->errcmd) == sizeof(t->errcmd)-1)
			fmt = "%s...: %s";
		e = tkvalue(ret, fmt, t->errcmd, t->err);
		t->err = nil;
		return e;
	}
	v = tkmkvar(t, buf, 0);
	free(buf);
	if(v == nil || v->value == nil)
		return nil;
	if(v->type != TkVstring)
		return TkNotvt;
	return tkvalue(ret, "%s", v->value);
}

char*
tkwinfo(TkTop *t, char *arg, char **ret)
{
	Tk *tk;
	char *cmd, *arg1;

	cmd = mallocz(Tkmaxitem, 0);
	if(cmd == nil)
		return TkNomem;

	arg = tkword(t, arg, cmd, cmd+Tkmaxitem);
	if(strcmp(cmd, "class") == 0) {
		arg1 = mallocz(Tkmaxitem, 0);
		if(arg1 == nil) {
			free(cmd);
			return TkNomem;
		}
		tkword(t, arg, arg1, arg1+Tkmaxitem);
		tk = tklook(t, arg1, 0);
		free(arg1);
		if(tk == nil)
			return TkBadwp;
		return tkvalue(ret, "%s", tkmethod[tk->type].name);
	}
	free(cmd);
	return TkBadcm;
}

char*
tkcursorcmd(TkTop *t, char *arg, char **ret)
{
	char *e;
	int locked;
	Display *d;
	TkOptab tko[3];

	USED(ret);

	tkcursor.def = 0;
	tko[0].ptr = &tkcursor;
	tko[0].optab = tkcurop;
	tko[1].ptr = nil;
	e = tkparse(t, arg, tko, nil);
	if(e != nil)
		return e;

	d = t->screen->display;
	locked = lockdisplay(d, 0);

	if(tkcursor.def)
		cursor(tkcursor.hot, d->image);
	else
	if(tkcursor.img != nil && tkcursor.img->fgimg != nil) {
		if(tkcursor.img->fgimg != nil)
			cursor(tkcursor.hot, tkcursor.img->fgimg);
		tkimgput(tkcursor.img);
		tkcursor.img = nil;
	}
	else
	if(tkcursor.bit != nil) {
		cursor(tkcursor.hot, tkcursor.bit);
		freeimage(tkcursor.bit);
		tkcursor.bit = nil;
	}
	if(locked)
		unlockdisplay(d);

	return nil;	
}

char *
tkbindings(TkTop *t, Tk *tk, TkEbind *b, int blen)
{
	TkAction *a, **ap;
	char *cmd, *e;
	int i;

	e = nil;
	for(i = 0; e == nil && i < blen; i++)	/* default bindings */
		e = tkaction(&tk->binds, b[i].event, TkStatic, b[i].cmd,TkAadd);
	
	if(e != nil)
		return e;

	ap = &tk->binds;
	for(a = t->binds[tk->type]; a; a = a->link) {	/* user "defaults" */
		cmd = strdup(a->arg);
		if(cmd == nil)
			return TkNomem;

		e = tkaction(ap, a->event, TkDynamic, cmd,
						(a->type >> 8) & 0xff);
		if(e != nil)
			return e;
		ap = &(*ap)->link;
	}
	return nil;
}
