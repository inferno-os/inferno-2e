#include <lib9.h>
#include <kernel.h>
#include "image.h"
#include "keyboard.h"
#include "tk.h"

#define istring u.string
#define iwin u.win
#define imark u.mark
#define iline u.line

#define	O(t, e)		((int)(&((t*)0)->e))

typedef struct Interval {
	int	lo;
	int	hi;
} Interval;

typedef struct Mprint Mprint;
struct Mprint
{
	char*	buf;
	int	ptr;
	int	len;
};

typedef struct TkDump TkDump;
struct TkDump
{
	int	sgml;
	int	metrics;
};

static
TkOption dumpopts[] =
{
	"sgml",		OPTbool,	O(TkDump, sgml),	nil,
	"metrics",	OPTbool,	O(TkDump, metrics),	nil,
	nil
};

static
TkOption textopts[] =
{
	"wrap",			OPTstab, O(TkText, opts[TkTwrap]),	tkwrap,
	"spacing1",		OPTdist, O(TkText, opts[TkTspacing1]),	(void *)O(Tk, env),
	"spacing2",		OPTdist, O(TkText, opts[TkTspacing2]),	(void *)O(Tk, env),
	"spacing3",		OPTdist, O(TkText, opts[TkTspacing3]),	(void *)O(Tk, env),
	"tabs",			OPTtabs, O(TkText, tabs), 		(void *)O(Tk, env),
	"xscrollcommand",	OPTtext, O(TkText, xscroll),		nil,
	"yscrollcommand",	OPTtext, O(TkText, yscroll),		nil,
	"insertwidth",		OPTdist, O(TkText, inswidth),		nil,
	"tagshare",		OPTwinp, O(TkText, tagshare),		nil,
	nil
};

#define CNTL(c) ((c)&0x1f)
#define DEL 0x7f

static TkEbind tktbinds[] = {
	{TkButton1P,		"%W tkTextButton1 %x %y"},
	{TkButton1P|TkMotion,	"%W tkTextSelectTo %x %y"},
	{TkButton1P|TkDouble,	"%W tkTextSelectTo %x %y double"},
	{TkButton2P,		"%W scan mark %x %y"},
	{TkButton2P|TkMotion,	"%W scan dragto %x %y"},
	{TkKey,			"%W tkTextInsert {%A}"},
	{TkKey|CNTL('a'),	"%W tkTextSetCursor {insert linestart}"},
	{TkKey|Home,		"%W tkTextSetCursor {insert linestart}"},
	{TkKey|CNTL('b'),	"%W tkTextSetCursor insert-1c"},
	{TkKey|Left,	"%W tkTextSetCursor insert-1c"},
	{TkKey|CNTL('d'),	"%W delete insert"},
	{TkKey|CNTL('e'),	"%W tkTextSetCursor {insert lineend}"}, 
	{TkKey|End,		"%W tkTextSetCursor {insert lineend}"}, 
	{TkKey|CNTL('>'),	"%W tkTextSetCursor end"},
	{TkKey|CNTL('<'),	"%W tkTextSetCursor start"},
	{TkKey|CNTL('f'),	"%W tkTextSetCursor insert+1c"},
	{TkKey|Right,	"%W tkTextSetCursor insert+1c"},
	{TkKey|CNTL('h'),	"%W tkTextDelIns -c"},
	{TkKey|DEL,		"%W tkTextDelIns +c"},
	{TkKey|CNTL('k'),	"%W delete insert {insert lineend}"},
	{TkKey|CNTL('n'),	"%W tkTextSetCursor {insert+1l}"},
	{TkKey|Down,	"%W tkTextSetCursor {insert+1l}"},
        {TkKey|CNTL('o'),       "%W tkTextInsert {\n}; %W mark set insert insert-1c"},
	{TkKey|CNTL('p'),	"%W tkTextSetCursor {insert-1l}"},
	{TkKey|Up,	"%W tkTextSetCursor {insert-1l}"},
	{TkKey|CNTL('u'),	"%W tkTextDelIns -l"},
	{TkKey|CNTL('v'),	"%W yview scroll 1 page"},
	{TkKey|Pgdown,	"%W yview scroll 1 page"},
	{TkKey|CNTL('w'),	"%W tkTextDelIns -w"},
	{TkKey|Pgup,	"%W yview scroll -1 page"},
	{TkFocusout,            "%W tkTextCursor delete"},
	{TkKey|APP|'\t',	""},
	{TkKey|BackTab,		""},
};

static int	tktclickmatch(TkText *, int, int, int, TkTindex *);
static void	tktdoubleclick(TkText *, TkTindex *, TkTindex *);
static char* 	tktdrawline(Image*, Tk*, TkTline*, Point);
static void	tktextcursordraw(Tk *, int);
static char* 	tktsetscroll(Tk*, int);
static void	tktsetclip(Tk *);
static char* 	tktview(Tk*, char*, char**, int, int*, int, int);
static Interval tkttranslate(Tk*, Interval, int);
static void 	tktfixscroll(Tk*, Point, Point);
static void 	tktnotdrawn(Tk*, int, int, int);
static void	tktdrawbg(Tk*, int, int, int);
static int	tktwidbetween(Tk*, int, TkTindex*, TkTindex*);
static int	tktpostspace(Tk*, TkTline*);
static int	tktprespace(Tk*, TkTline*);
static void	tktsee(Tk*, TkTindex*, int);
static Point	tktrelpos(Tk*);

/* debugging */
extern int tktdbg;
extern void tktprinttext(TkText*);
extern void tktprintindex(TkTindex*);
extern void tktprintitem(TkTitem*);
extern void tktprintline(TkTline*);
extern void tktcheck(TkText*, char*);
extern int tktutfpos(char *, int);

void
tktextoptfix(void)
{
	TkOption *o;

	for(o = textopts; o->o != nil; o++)
		o->offset += O(Tk, obj[0]);
}

char*
tktext(TkTop *t, char* arg, char **ret)
{
	Tk *tk;
	char *e;
	TkEnv *ev;
	TkTline *l;
	TkTitem *it = nil;
	TkName *names = nil;
	TkTtaginfo *ti = nil;
	TkOptab tko[3];
	TkTmarkinfo *mi = nil;
	TkText *tkt, *tktshare;
	int freetabs = 0;

	tk = tknewobj(t, TKtext, sizeof(Tk)+sizeof(TkText));
	if(tk == nil)
		return TkNomem;

	tkt = TKobj(TkText, tk);

	tk->relief = TKsunken;
	tk->borderwidth = 2;
	tk->sborderwidth = 1;
	tk->ipad.x = Textpadx;
	tk->ipad.y = Textpady;
	tkt->inswidth = 2;
	tkt->cur_flag = 0;	/* text cursor doesn't show up initially */
	tkt->opts[TkTwrap] = Tkwrapchar;
	tkt->opts[TkTrelief] = TKflat;
	tkt->opts[TkTjustify] = Tkleft;

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tk;
	tko[1].optab = textopts;
	tko[2].ptr = nil;

	tk->req.width = tk->env->wzero*Textwidth;
	tk->req.height = tk->env->font->height*Textheight;

	names = nil;
	e = tkparse(t, arg, tko, &names);
	if(e != nil)
		goto err;
	if(names == nil) {
		e = TkBadwp;
		goto err;
	}

	if(tkt->tagshare != nil) {
		tkputenv(tk->env);
		tk->env = tkt->tagshare->env;
		tk->env->ref++;
	}

	if(tk->flag&Tkdisabled)
		tkt->inswidth = 0;

	if(tkt->tabs == nil) {
		freetabs=1;	/* XXX - Tad: hack hack hack */
		tkt->tabs = malloc(sizeof(TkTtabstop));
		if(tkt->tabs == nil) 
			goto err;
		tkt->tabs->pos = 8*tk->env->wzero;
		tkt->tabs->justify = Tkleft;
		tkt->tabs->next = nil;
	}

	if(tkt->tagshare != nil) {
		tktshare = TKobj(TkText, tkt->tagshare);
		tkt->tags = tktshare->tags;
		tkt->nexttag = tktshare->nexttag;
	}
	else {
		/* Note: sel should have id == TkTselid == 0 */
		e = tktaddtaginfo(tk, "sel", &ti);
		if(e != nil)
			goto err;

		tkputenv(ti->env);
		ti->env = tknewenv(t);
		if(ti->env == nil)
			goto err;

		ev = ti->env;
		ev->colors[TkCbackgnd] = tk->env->colors[TkCselectbgnd];
		ev->colors[TkCbackgndlght] = tk->env->colors[TkCselectbgndlght];
		ev->colors[TkCbackgnddark] = tk->env->colors[TkCselectbgnddark];
		ev->colors[TkCforegnd] = tk->env->colors[TkCselectfgnd];
		ev->set = (1<<TkCbackgnd)|(1<<TkCbackgndlght)|
			  (1<<TkCbackgnddark)|(1<<TkCforegnd);

		ti->opts[TkTborderwidth] = tk->sborderwidth;
		if(tk->sborderwidth > 0)
			ti->opts[TkTrelief] = TKraised;
	}

	e = tktaddmarkinfo(tkt, "current", &mi);
	if(e != nil)
		goto err;

	e = tktaddmarkinfo(tkt, "insert", &mi);
	if(e != nil)
		goto err;

	tkt->start.flags = TkTfirst|TkTlast;
	tkt->end.flags = TkTlast;

	e = tktnewitem(TkTnewline, 0, &it);

	if(e != nil)
		goto err;

	e = tktnewline(TkTfirst|TkTlast, it, &tkt->start, &tkt->end, &l);
	if(e != nil)
		goto err;

	e = tktnewitem(TkTmark, 0, &it);
	if(e != nil)
		goto err;

	it->next = l->items;
	l->items = it;
	it->imark = mi;
	mi->cur = it;
	tkt->nlines = 1;
	tkt->scrolltop[Tkvertical] = -1;
	tkt->scrolltop[Tkhorizontal] = -1;
	tkt->scrollbot[Tkvertical] = -1;
	tkt->scrollbot[Tkhorizontal] = -1;

	tk->deliverfn = tktextevent;
	tk->dirty = tktdirty;
	tk->relpos = tktrelpos;

	if(tkt->tagshare != nil)
		tk->binds = tkt->tagshare->binds;
	else {
		e = tkbindings(t, tk, tktbinds, nelem(tktbinds));

		if(e != nil)
			goto err;
	}

	e = tkaddchild(t, tk, &names);
	tkfreename(names);
	if(e != nil)
		goto err;
	tk->name->link = nil;

	return tkvalue(ret, "%s", tk->name->name);

err:
	if(freetabs && tkt->tabs != nil)
		tktfreetabs(tkt->tabs);
	if(ti != nil)
		tktfreetags(ti);
	if(mi != nil)
		tktfreemarks(mi);
	if(it != nil)
		tktfreeitems(tkt, it);
	tkfreeobj(tk);
	return e;
}

/*
 * There are four coordinate systems of interest:
 *	S - screen coordinate system (i.e. top left corner of
 *		inferno screen is (0,0) in S space.
 *	I - image coordinate system (i.e. top left corner of
 *		tkimageof(this widget) is (0,0) in I space.
 *	T - text coordinate system (i.e., top left of first line
 *		is at (0,0) in T space.
 *	V - view coordinate system (i.e., top left of visible
 *		portion of widget is at (0,0) in V space.
 *
 *	A point P in the four systems (Ps, Pi, Pt, Pv) satisfies:
 *		Pt = Ps - deltast
 *		Pv = Ps - deltasv
 *		Pv = Pi - deltaiv
 *	(where deltast is vector from S origin to T origin;
 *	     deltasv is vector from S origin to V origin;
 *	     deltaiv is vector from I origin to V origin)
 *
 *	We keep deltatv, deltasv, and deltaiv in tkt.
 *	Deltatv is updated by scrolling.
 *	Deltasv is updated by geom changes:
 *		tkposn(tk)+ipad/2
 *	Deltaiv is affected by geom changes and the call to the draw function:
 *		tk->act+orig+ipad/2+(bw,bw) (orig is the parameter to tkdrawtext),
 *
 *	We can derive
 *		Ps = Pt + deltast
 *		   = Pt +  deltasv - deltatv
 *
 *		Pv = Pt - deltatv
 *
 * Here are various coordinates in the text widget according
 * to which coordinate system they use:
 *
 *	S - Mouse coordinates (coming in to tktextevent);
 *		the deltasv parameter to tkdrawtext;
 *		coords in tkt->image, where drawing is done to
 *		(to get same bit-alignment as screen, for fast transfer)
 *	T - orig in TkTlines
 *	V - %x,%y delivered via binds to TkText or its tags
 *
 */
static void
tktsetdeltas(Tk *tk, Point orig)
{
	Point g;
	TkText *tkt = TKobj(TkText, tk);

	tkt->deltaiv.x = orig.x + tk->act.x + tk->ipad.x/2 + tk->borderwidth;
	tkt->deltaiv.y = orig.y + tk->act.y + tk->ipad.y/2 + tk->borderwidth;

	g = tkposn(tk);
	tkt->deltasv.x = g.x + tk->borderwidth + tk->ipad.x/2;
	tkt->deltasv.y = g.y + tk->borderwidth + tk->ipad.y/2;
}

static Point
tktrelpos(Tk *sub)
{
	Tk *tk;
	TkTindex ix;
	Rectangle r;
	Point ans;

	tk = sub->parent;
	if(tk == nil)
		return tkzp;

	if(tktfindsubitem(sub, &ix)) {
		r = tktbbox(tk, &ix);
		ans.x = r.min.x;
		ans.y = r.min.y;
		return r.min;
	}
	return tkzp;
}

static void
tktreplclipr(Image *dst, Rectangle r)
{
	int locked;

	locked = lockdisplay(dst->display, 1);
	replclipr(dst, 0, r);
	if(locked)
		unlockdisplay(dst->display);
}

char*
tkdrawtext(Tk *tk, Point orig)
{
	int vh;
	Image *dst;
	TkText *tkt;
	TkTline *l, *lend;
	Point p, deltait;
	Rectangle oclipr;
	int reldone = 1;
	char *e;

	tkt = TKobj(TkText, tk);
	dst = tkimageof(tk);
	tkt->image = dst;
	tktsetdeltas(tk, orig);
	tkt->tflag |= TkTdrawn|TkTdlocked;
	oclipr = dst->clipr;
	tktsetclip(tk);

	if(tk->flag&Tkrefresh) {
		reldone = 0;
		tktnotdrawn(tk, 0, tkt->end.orig.y, 1);
	}
	tk->flag &= ~Tkrefresh;

	deltait = subpt(tkt->deltaiv, tkt->deltatv);
	vh = tk->act.height - tk->ipad.y/2;
	lend = &tkt->end;
	for(l = tkt->start.next; l != lend; l = l->next) {
		if(l->orig.y+l->height < tkt->deltatv.y)
			continue;
		if(l->orig.y > tkt->deltatv.y + vh)
			break;
		if(!(l->flags&TkTdrawn)) {
			e = tktdrawline(dst, tk, l, deltait);
			if(e != nil)
				return e;
		}
	}

	tktreplclipr(dst, oclipr);
	if(!reldone) {
		p.x = orig.x + tk->act.x;
		p.y = orig.y + tk->act.y;
		tkdrawrelief(dst, tk, &p, 0);
	}
	tkt->tflag &= ~TkTdlocked;

	return nil;
}

/*
 * Set the clipping rectangle of the destination image to the
 * intersection of the current clipping rectangle and the area inside
 * the text widget.  The caller should save the old one and restore it later.
 */
static void
tktsetclip(Tk *tk)
{
	Rectangle r;
	Image *dst;
	TkText *tkt = TKobj(TkText, tk);

	dst = tkt->image;
	r.min = subpt(tkt->deltaiv, divpt(tk->ipad, 2));
	r.max.x = r.min.x + tk->act.width;
	r.max.y = r.min.y + tk->act.height;

	if(!rectclip(&r, dst->clipr))
		r.max = r.min;
	tktreplclipr(dst, r);
}

static char*
tktdrawline(Image *i, Tk *tk, TkTline *l, Point deltait)
{
	Tk *sub;
	Font *f;
	Image *bg;
	Point p, q;
	Rectangle r;
	TkText *tkt;
	TkTitem *it, *z;
	int bevtop, bevbot;
	TkEnv *e, *et, *env;
	int *opts;
	int o, bd, ul, ov, h, w, la, lh, cursorx, join;
	char *err;

	env = mallocz(sizeof(TkEnv), 0);
	if(env == nil)
		return TkNomem;
	opts = mallocz(TkTnumopts*sizeof(int), 0);
	if(opts == nil) {
		free(env);
		return TkNomem;
	}
	tkt = TKobj(TkText, tk);
	e = tk->env;
	et = env;
	et->top = e->top;
	f = e->font;

	/* l->orig is in T space, p is in I space */
	la = l->ascent;
	lh = l->height;
	p = addpt(l->orig, deltait);
	p.y += la;
/* if(tktdbg){print("drawline, p=(%d,%d), f->a=%d, f->h=%d\n", p.x, p.y, f->ascent, f->height); tktprintline(l);} */
	cursorx = -1000;
	join = 0;
	for(it = l->items; it != nil; it = it->next) {
		bg = tkgc(e, TkCbackgnd);
		if(tktanytags(it)) {
			tkttagopts(tk, it, opts, env, 1);
			if(e->colors[TkCbackgnd] != et->colors[TkCbackgnd]) {
				bg = tkgc(et, TkCbackgnd);
				r.min = p;
				r.min.y -= la;
				r.max.x = r.min.x + it->width;
				r.max.y = r.min.y + lh;
				draw(i, r, bg, i->display->ones, tkzp);
			}
			o = opts[TkTrelief];
			bd = opts[TkTborderwidth];
			if((o == TKsunken || o == TKraised) && bd > 0) {
				/* fit relief inside item bounding box */

				q.x = p.x;
				q.y = p.y - la;
				if(it->width < 2*bd)
					bd = it->width / 2;
				if(lh < 2*bd)
					bd = lh / 2;
				w = it->width - 2*bd;
				h = lh - 2*bd;
				if(o == TKraised) {
					bevtop = TkCbackgndlght;
					bevbot = TkCbackgnddark;
				}
				else {
					bevtop = TkCbackgnddark;
					bevbot = TkCbackgndlght;
				}

				tkbevel(i, q, w, h, bd,
					tkgc(et, bevtop), tkgc(et, bevbot));

				/* join relief between adjacent items if tags match */
				if(join) {
					r.min.x = q.x;
					r.max.x = q.x + bd;
					r.min.y = q.y + bd;
					r.max.y = r.min.y + h;
					draw(i, r, bg, i->display->ones, tkzp);
					r.min.y = r.max.y;
					r.max.y = r.min.y + bd;
					draw(i, r, tkgc(et, bevbot), i->display->ones, tkzp);
				}
				for(z = it->next; z != nil && z->kind == TkTmark; )
					z = z->next;
				if(z != nil && tktsametags(z, it)) {
					r.min.x = q.x + bd + w;
					r.max.x = r.min.x + bd;
					r.min.y = q.y;
					r.max.y = q.y + bd;
					draw(i, r, tkgc(et, bevtop), i->display->ones, tkzp);
					r.min.y = r.max.y;
					r.max.y = r.min.y + h;
					draw(i, r, bg, i->display->ones, tkzp);
					join = 1;
				}
				else
					join = 0;
			}
			o = opts[TkToffset];
			ul = opts[TkTunderline];
			ov = opts[TkToverstrike];
		}
		else {
			memset(et->evim, 0, sizeof(et->evim));
			et->font = f;
			et->colors[TkCforegnd] = e->colors[TkCforegnd];
			o = 0;
			ul = 0;
			ov = 0;
		}

		switch(it->kind) {
		case TkTascii:
		case TkTrune:
			q.x = p.x;
			q.y = p.y - env->font->ascent - o;
/*if(tktdbg)print("q=(%d,%d)\n", q.x, q.y);*/
			string(i, q, tkgc(et, TkCforegnd), q, env->font, it->istring);
			if(ov) {
				r.min.x = q.x;
				r.max.x = r.min.x + it->width;
				r.min.y = q.y + 2*env->font->ascent/3;
				r.max.y = r.min.y + 2;
				draw(i, r, tkgc(et, TkCforegnd), i->display->ones, tkzp);
			}
			if(ul) {
				r.min.x = q.x;
				r.max.x = r.min.x + it->width;
				r.max.y = p.y - la + lh;
				r.min.y = r.max.y - 2;
				draw(i, r, tkgc(et, TkCforegnd), i->display->ones, tkzp);
			}
			break;
		case TkTmark:
			if((it->imark != nil) 
                           && strcmp(it->imark->name, "insert") == 0) {
				cursorx = p.x - 1;
			}
			break;
		case TkTwin:
			sub = it->iwin->sub;
			if(sub != nil) {
				tksetbits(sub, Tkdirty|Tkrefresh);
				err = tkdrawslaves(sub, p);
				if(err != nil) {
					free(opts);
					free(env);
					return err;
				}
			}
			break;
		}
		p.x += it->width;
	}
	l->flags |= TkTdrawn;

	/* do cursor last, so not overwritten by later items */
	if(cursorx != -1000 && tkt->inswidth > 0) {
		r.min.x = cursorx;
		r.min.y = p.y - la;
		r.max.x = r.min.x + tkt->inswidth;
		r.max.y = r.min.y + lh;
		tkt->cur_rec = rectsubpt(r, deltait);
		if(tkt->cur_flag)
			tktextcursordraw(tk, TkCforegnd);
	}

	free(opts);
	free(env);
	return nil;
}

static void
tktextcursordraw(Tk *tk, int color)
{
	Rectangle r;
	TkText *tkt;
	Image *i;

	tkt = TKobj(TkText, tk);
	
	r = rectaddpt(tkt->cur_rec, subpt(tkt->deltaiv, tkt->deltatv));

	/* check the cursor with widget boundary */
	/* do nothing if entire cursor outside widget boundary */
	if( ! (	r.max.x < tkt->deltaiv.x ||
		r.min.x > tkt->deltaiv.x + tk->act.width ||
		r.max.y < tkt->deltaiv.y ||
		r.min.y > tkt->deltaiv.y + tk->act.height)) {

		/* clip rectangle if extends beyond widget boundary */
		if (r.min.x < tkt->deltaiv.x)
			r.min.x = tkt->deltaiv.x;
		if (r.max.x > tkt->deltaiv.x + tk->act.width)
			r.max.x = tkt->deltaiv.x + tk->act.width;
		if (r.min.y < tkt->deltaiv.y)
			r.min.y = tkt->deltaiv.y;
		if (r.max.y > tkt->deltaiv.y + tk->act.height)
			r.max.y = tkt->deltaiv.y + tk->act.height;
		i = tkimageof(tk);
		draw(i, r, tkgc(tk->env, color), i->display->ones, tkzp);
	}
}

/* 
 * This is a TK4.0 noncompatible widget operation, which has been
 * added for MR inf971854.
 * It's called with two events, TkFocusout and TkFocusin; in case of
 * TkFocusout, it removes the text cursor (it's confusing
 * to locate the input cursor when there are many text widgets 
 * on the display and each of them has its own text cursor). In 
 * case of TkFocusin, it inserts the text cursor back to the widget.
 */
char*
tktextcursor(Tk *tk, char* arg, char **ret)
{
	Image *dst;
	TkText *tkt;
	TkTline *l, *lend;
	TkTitem *it;
	Point orig;
	int drawn;

	USED(ret);
	tkt = TKobj(TkText, tk);
	dst = tkimageof(tk);
	if (dst == nil)
		return nil;
	/*
	 * Redraws the line which has the cursor, either with or without
	 * the cursor, depending on whether the arg is " insert" or not.
	 */
	if(strcmp(arg, " delete") == 0) {
		tktextcursordraw(tk, TkCbackgnd);
		tkt->cur_flag = 0;
	}
	else
	if(strcmp(arg, " insert") == 0)
		tkt->cur_flag = 1;
	lend = &tkt->end;

	drawn = 0;
	for(l = tkt->start.next; l != lend && ! drawn; l = l->next) 
		for (it = l->items; it != nil; it = it->next) 
			if (it->kind == TkTmark && it->imark != nil &&
				    strcmp(it->imark->name, "insert") == 0) {
				l->flags &= ~TkTdrawn;
				orig.x = tkt->deltaiv.x - tk->act.x - 
						tk->ipad.x/2 - tk->borderwidth;
				orig.y = tkt->deltaiv.y - tk->act.y - 
						tk->ipad.y/2 - tk->borderwidth;
				tkdrawtext(tk, orig);
				drawn = 1;
				break;
			}
	return nil;
}

/*
 * Insert string s just before ins, but don't worry about geometry values.
 * Don't worry about doing wrapping correctly, but break long strings
 * into pieces to avoid bad behavior in the wrapping code of tktfixgeom.
 * If tagit != 0, use its tags, else use the intersection of tags of 
 * non cont or mark elements just before and just after insertion point.
 * (At beginning and end of widget, just use the tags of one adjacent item).
 * Keep *ins up-to-date.
 */
char*
tktinsert(Tk *tk, TkTindex *ins, char *s, TkTitem *tagit)
{
	int c, n, m, nextra, nmax, atend, atbeg;
	char *e;
	uchar *us, *p;
	TkTindex iprev, inext;
	TkTitem *i, *utagit;
	TkText *tkt = TKobj(TkText, tk);

	e = tktsplititem(ins);
	if(e != nil)
		return e;

	/* if no tags give, use intersection of previous and next char tags */

	nextra = 0;
	n = tk->env->wzero;
	if(n <= 0)
		n = 8;
	nmax = tk->act.width - tk->ipad.x;
	if(nmax <= 0) {
		nmax = tk->req.width;
		if(nmax <= 0)
			nmax = 60*n;
	}
	nmax = (nmax + n - 1) / n;
	utagit = nil;
	if(tagit == nil) {
		inext = *ins;
		tktadjustind(tkt, TkTbycharstart, &inext);
		atend = (inext.item->next == nil && inext.line->next == &tkt->end);
		if(atend || tktanytags(inext.item)) {
			iprev = *ins;
			tktadjustind(tkt, TkTbycharback, &iprev);
			atbeg = (iprev.line->prev == &tkt->start && iprev.line->items == iprev.item);
			if(atbeg || tktanytags(iprev.item)) {
				nextra = 0;
				if(!atend)
					nextra = inext.item->tagextra;
				if(!atbeg && iprev.item->tagextra > nextra)
					nextra = iprev.item->tagextra;
				e = tktnewitem(0, nextra, &utagit);
				if(e != nil)
					return e;
				if(!atend) {
					tkttagcomb(utagit, inext.item, 1);
					if(!atbeg)
						tkttagcomb(utagit, iprev.item, 0);
				}
				else if(!atbeg)
					tkttagcomb(utagit, iprev.item, 1);
				tagit = utagit;
			}
		}
	}
	else
		nextra = tagit->tagextra;

	us = (uchar *)s;
	while(*us != '\0') {
		e = tktnewitem(TkTascii, nextra, &i);
		if(e != nil) {
			if(utagit != nil)
				free(utagit);
			return e;
		}

		if(tagit != nil)
			tkttagcomb(i, tagit, 1);

		c = *us;
		if(c == '\n') {
			i->kind = TkTnewline;
			tkt->nlines++;
			us++;
		}
		else
		if(c == '\t') {
			i->kind = TkTtab;
			us++;
		}
		else {
			p = us;
			n = 0;
			if(c < Runeself) {
				i->kind = TkTascii;
				while(c != '\0' && c < Runeself && c != '\n' && c != '\t' && n < nmax) {
					c = *++us;
					n++;
				}
			}
			else {
				i->kind = TkTrune;
				do {
					for(m = 1; m <= UTFmax; m++) {
						c = *++us;
						if(c == '\0' || fullrune((char*)p, m))
							break;
					}
					if ((n+m) >= nmax)
						break;
					n += m;
					p = us;
				} while(c >= Runeself);
				p -= n;
			}
			i->istring = malloc(n+1);
			if(i->istring == nil) {
				tktfreeitems(tkt, i);
				if(utagit != nil)
					free(utagit);
				return TkNomem;
			}
			memmove(i->istring, p, n);
			i->istring[n] = '\0';
		}
		e = tktiteminsert(tkt, ins, i);
		if(e != nil) {
			if(utagit != nil)
				free(utagit);
			tktfreeitems(tkt,i);
			return e;
		}
	}

	if(utagit != nil)
		free(utagit);
	return nil;
}

/*
 * For lines l1->next, ..., l2, fix up the geometry
 * elements of constituent TkTlines and TkTitems.
 * This involves doing proper line wrapping, and calculating item
 * widths and positions.
 * Also, merge any adjacent TkTascii/TkTrune items with the same tags.
 * Finally, bump the y component of lines l2->next, ... end.
 * l2 should not be tkt->end.
 */
char*
tktfixgeom(Tk *tk, TkTline *l1, TkTline *l2)
{
	int x, y, a, h, w, o, n, j, sp3, xleft, xright, winw, oa, oh;
	int wrapmode, just, needsplit;
	char *e;
	TkText *tkt;
	Tk *sub;
	TkTitem *i, *it, *ilast, *iprev;
	TkTindex ix, ixprev;
	TkTline *l, *lafter;
	Interval oldi, hole, rest, newrest;
	TkEnv *env;
	Font *f;
	int *opts;

	tkt = TKobj(TkText, tk);

	if(tktdbg)
		tktcheck(tkt, "tktfixgeom");

	winw = tk->act.width - tk->ipad.x;
	if(winw <= 0) {
		winw = tk->req.width;
		if(winw < 0)
			return nil;
	}

	/*
	 * Make lafter be the first line after l2 that comes after a newline
	 * (so that wrap correction cannot affect it)
	 */
	lafter = l2->next;
	if(tktdbg && lafter == nil) {
		print("tktfixgeom: botch 1\n");
		return nil;
	}
	while((lafter->flags & TkTfirst) == 0 && lafter != &tkt->end)
		lafter = lafter->next;


	y = l1->orig.y + l1->height + tktpostspace(tk, l1);

	l2 = lafter->prev;
	oldi.lo = y;
	oldi.hi = l2->orig.y + l2->height;
	rest.lo = lafter->orig.y;
	rest.hi = rest.lo + 1000; /* get background after end, too */

	opts = mallocz(TkTnumopts*sizeof(int), 0);
	if(opts == nil)
		return TkNomem;
	env = mallocz(sizeof(TkEnv), 0);
	if(env == nil) {
		free(opts);
		return TkNomem;
	}

	for(l = l1->next; l != lafter; l = l->next) {
		if(tktdbg && l == nil) {
			print("tktfixgeom: botch 2\n");
			free(opts);
			free(env);
			return nil;
		}

		l->flags &= ~TkTdrawn;

		/* some spacing depends on tags of first non-mark on display line */
		iprev = nil;
		for(i = l->items; i->kind == TkTmark; ) {
			iprev = i;
			i = i->next;
		}
		tkttagopts(tk, i, opts, env, 1);

		if(l->flags&TkTfirst) {
			xleft = opts[TkTlmargin1];
			y += opts[TkTspacing1];
		}
		else {
			xleft = opts[TkTlmargin2];
			y += opts[TkTspacing2];
		}
		sp3 = opts[TkTspacing3];
		just = opts[TkTjustify];

		wrapmode = opts[TkTwrap];
		f = env->font;
		h = f->height;
		a = f->ascent;
		x = xleft;
		xright = winw - opts[TkTrmargin];
		if(xright < xleft)
			xright = xleft;

		for(; i != nil; iprev = i, i = i->next) {
		    again:
			if(i->kind == TkTmark)
				continue;
			if(i->kind == TkTnewline)
				break;
			if(i->kind == TkTcontline) {
				/*
				 * See if some of following line fits on this one.
				 * First, ensure that following line isn't empty.
				 */
				it = l->next->items;
				while(it->kind == TkTmark)
					it = it->next;
				
				if(it->kind == TkTnewline || it->kind == TkTcontline) {
					/* next line is empty; join it to this one by removing i */
					ix.item = i;
					ix.line = l;
					ix.pos = 0;
					tktremitem(tkt, &ix);
					it = l->next->items;
					if(iprev == nil)
						i = l->items;
					else
						i = iprev->next;
					goto again;
				}

				n = xright - x;
				if(n <= 0)
					break;
				ixprev.line = l;
				ixprev.item = i;
				ixprev.pos = 0;
				ix = ixprev;
				tktadjustind(tkt, TkTbychar, &ix);
				if(wrapmode == Tkwrapword)
					tktadjustind(tkt, TkTbywordend, &ix);
				if(wrapmode != Tkwrapnone && tktwidbetween(tk, x, &ixprev, &ix) > n)
					break;
				/* move one item up from next line and try again */
				it = l->next->items;
				if(tktdbg && (it == nil || it->kind == TkTnewline || it->kind == TkTcontline)) {
					print("tktfixgeom: botch 3\n");
					free(opts);
					free(env);
					return nil;
				}
				if(iprev == nil)
					l->items = it;
				else
					iprev->next = it;
				l->next->items = it->next;
				it->next = i;
				i = it;
				goto again;
			}

			oa = a;
			oh = h;
			if(!tktanytags(i)) {
				env->font = tk->env->font;
				o = 0;
			}
			else {
				tkttagopts(tk, i, opts, env, 1);
				o = opts[TkToffset];
			}
			if((o != 0 || env->font != f) && i->kind != TkTwin) {
				/* check ascent of current item */
				n = o+env->font->ascent;
				if(n > a) {
					a = n;
					h += (a - oa);
				}
				/* check descent of current item */
				n = (env->font->height - env->font->ascent) - o;
				if(n > h-a)
					h = a + n;
			}
			if(i->kind == TkTwin && i->iwin->sub != nil) {
				sub = i->iwin->sub;
				n = 2 * i->iwin->pady + sub->act.height +
					2 * sub->borderwidth;
				switch(i->iwin->align) {
				case Tktop:
				case Tkbottom:
					if(n > h)
						h = n;
					break;
				case Tkcenter:
					if(n/2 > a)
						a = n/2;
					if(n/2 > h-a)
						h = a + n/2;
					break;
				case Tkbaseline:
					n -= i->iwin->pady;
					if(n > a)
						a = n;
					if(i->iwin->pady > h-a)
						h = a + i->iwin->pady;
					break;
				}
			}

			w = tktdispwidth(tk, i, env->font, x, 0, -1);

			n = x + w - xright;
			if(n > 0 && wrapmode != Tkwrapnone) {
				j = tktposcount(i) - 1;
				while(j > 0 && tktdispwidth(tk, i, env->font, x, j, -1) < n)
					j--;
				/* put at least one item on a line before splitting */
				if(j == 0 && x == xleft) {
					if(tktposcount(i) == 1)
						goto nosplit;
					j = 1;
				}
				ix.line = l;
				ix.item = i;
				ix.pos = j;
				if(wrapmode != Tkwrapchar) {
					tktadjustind(tkt, TkTbywordstart, &ix);
					if(ix.line != l || (ix.item == l->items && ix.pos == 0)) {
						ix.line = l;
						ix.item = l->items;
						ix.pos = 0;
						tktadjustind(tkt, TkTbywordend, &ix);
						/* if got to new item, don't split the current one */
						if(ix.item != l->items)
							goto nosplit;
					}
				}
				if(ix.pos > 0) {
					needsplit = 1;
					e = tktsplititem(&ix);
					if(e != nil) {
						free(opts);
						free(env);
						return e;
					}
				}
				else
					needsplit = 0;

				e = tktnewitem(TkTcontline, 0, &it);
				if(e != nil) {
					free(opts);
					free(env);
					return e;
				}
				e = tktiteminsert(tkt, &ix, it);
				if(e != nil) {
					free(opts);
					free(env);
					tktfreeitems(tkt, it);
					return e;
				}

				l = l->prev;	/* work on part of line up to split */

				if(needsplit) {
					/* have to calculate width of pre-split part */
					ixprev = ix;
					if(tktadjustind(tkt, TkTbyitemback, &ixprev) &&
					   tktadjustind(tkt, TkTbyitemback, &ixprev)) {
						w = tktdispwidth(tk, ixprev.item, nil, x, 0, -1);
						ixprev.item->width = w;
						x += w;
					}
				}
				else {
					h = oh;
					a = oa;
				}
				break;
			}
			else {
			    nosplit:
				i->width =w;
				x += w;
			}
		}

		/*
		 * Now line l is broken correctly and has correct item widths.
		 * Merge adjacent TkTascii/TkTrune items with same tags.
		 * Also, set act{x,y} of embedded widgets to offset from
		 * left of item box at baseline.
		 */
		for(i = l->items; i->next != nil; i = i->next) {
			it = i->next;
			if( (i->kind == TkTascii || i->kind == TkTrune)
			      &&
			     i->kind == it->kind
			      &&
			     tktsametags(i, it)) {
				n = strlen(i->istring);
				j = strlen(it->istring);
				i->istring = realloc(i->istring, n + j + 1);
				if(i->istring == nil) {
					free(opts);
					free(env);
					return TkNomem;
				}
				memmove(i->istring+n, it->istring, j+1);
				i->width += it->width;
				i->next = it->next;
				it->next = nil;
				tktfreeitems(tkt, it);
			}
			else if(i->kind == TkTwin && i->iwin->sub != nil) {
				sub = i->iwin->sub;
				n = sub->act.height + 2 * sub->borderwidth;
				o = i->iwin->pady;
				sub->act.x = i->iwin->padx;
				switch(i->iwin->align) {
				case Tktop:
					sub->act.y = o - a;
					break;
				case Tkbottom:
					sub->act.y = h - (o + n) - a;
					break;
				case Tkcenter:
					sub->act.y = (h - n) / 2 - a;
					break;
				case Tkbaseline:
					sub->act.y = -n;
					break;
				}
			}
		}

		l->width = x - xleft;

		/* justification bug: wrong if line has tabs */
		l->orig.x = xleft;
		n = xright - x;
		if(n > 0) {
			if(just == Tkright)
				l->orig.x += n;
			else
			if(just == Tkcenter)
				l->orig.x += n/2;
		}

		/* give newline or contline width up to right margin */
		ilast = tktlastitem(l->items);
		ilast->width = xright - l->width;
		if(ilast->width < 0)
			ilast->width = 0;

		l->orig.y = y;
		l->height = h;
		l->ascent = a;
		y += h;
		if(l->flags&TkTlast)
			y += sp3;
	}
	free(opts);
	free(env);

	tktdrawbg(tk, oldi.lo, oldi.hi, 0);

	y += tktprespace(tk, l);
	newrest.lo = y;
	newrest.hi = y + rest.hi - rest.lo;
	hole = tkttranslate(tk, newrest, rest.lo);
	tktdrawbg(tk, hole.lo, hole.hi, 0);

	if(l != &tkt->end) {
		while(l != &tkt->end) {
			oh = l->next->orig.y - l->orig.y;
			l->orig.y = y;
			if(y + oh > hole.lo && y < hole.hi) {
				l->flags &= ~TkTdrawn;
			}
			y += oh;
			l = l->next;
		}
	}
	tkt->end.orig.y = tkt->end.prev->orig.y + tkt->end.prev->height;

	if(tkt->deltatv.y > tkt->end.orig.y)
		tkt->deltatv.y = tkt->end.prev->orig.y;

	e = tktsetscroll(tk, Tkvertical);
	if(e != nil)
		return e;
	e = tktsetscroll(tk, Tkhorizontal);
	if(e != nil)
		return e;

	tk->flag |= Tkdirty;
	if(tktdbg)
		tktcheck(tkt, "tktfixgeom end");

	return nil;
}

static int
tktpostspace(Tk *tk, TkTline *l)
{
	int ans;
	TkTitem *i;
	TkEnv env;
	int *opts;

	opts = mallocz(TkTnumopts*sizeof(int), 0);
	if(opts == nil)
		return 0;
	ans = 0;
	if(l->items != nil && (l->flags&TkTlast)) {
		for(i = l->items; i->kind == TkTmark; )
			i = i->next;
		tkttagopts(tk, i, opts, &env, 1);
		ans = opts[TkTspacing3];
	}
	free(opts);
	return ans;
}

static int
tktprespace(Tk *tk, TkTline *l)
{
	int ans;
	TkTitem *i;
	TkEnv env;
	int *opts;
	
	opts = mallocz(TkTnumopts*sizeof(int), 0);
	if(opts == nil)
		return 0;

	ans = 0;
	if(l->items != nil) {
		for(i = l->items; i->kind == TkTmark; )
			i = i->next;
		tkttagopts(tk, i, opts, &env, 1);
		if(l->flags&TkTfirst)
			ans = opts[TkTspacing1];
		else
			ans = opts[TkTspacing2];
	}
	free(opts);
	return ans;
}

static int
tktwidbetween(Tk *tk, int x, TkTindex *i1, TkTindex *i2)
{
	int d, w, n;
	TkTindex ix;
	TkText *tkt = TKobj(TkText, tk);

	w = 0;
	ix = *i1;
	while(ix.item != i2->item) {
		d = tktdispwidth(tk, ix.item, nil, x, ix.pos, -1);
		w += d;
		x += d;
		if(!tktadjustind(tkt, TkTbyitem, &ix)) {
			if(tktdbg)
				print("tktwidbetween botch\n");
			break;
		}
	}
	n = i2->pos - ix.pos;
	if(n > 0)
		w += tktdispwidth(tk, ix.item, nil, x, ix.pos, i2->pos-ix.pos);
	return w;
}

static Interval
tktvclip(Interval i, int vh)
{
	if(i.lo < 0)
		i.lo = 0;
	if(i.hi > vh)
		i.hi = vh;
	return i;
}

/*
 * Do translation of any part of interval that appears on screen
 * starting at srcy to its new position, dsti.
 * Return y-range of the hole left in the image (either because
 * the src bits were out of the V window, or because the src bits
 * vacated an area of the V window).
 * The coordinates passed in and out are in T space.
 */
static Interval
tkttranslate(Tk *tk, Interval dsti, int srcy)
{
	int vh, vw, dvty, locked;
	TkText *tkt;
	Image *i;
	Interval hole, vdst, vsrc;
	Point src;
	Rectangle dst;
	Display *d;

	hole.hi = 0;
	hole.lo = 0;
	/*
	 * If we are embedded in a text widget, we need to come in through
	 * the tkdrawtext routine, to ensure our clipr is set properly, so we
	 * just punt in that case.
	 */
	if(tk->parent != nil && tk->parent->type == TKtext) {
		tk->flag |= Tkrefresh;
		return hole;
	}
	tkt = TKobj(TkText, tk);
	dvty = tkt->deltatv.y;
	i = tkt->image;

	vw = tk->act.width - tk->ipad.x;
	vh = tk->act.height - tk->ipad.y;

	/* convert to V space */
	vdst.lo = dsti.lo - dvty;
	vdst.hi = dsti.hi - dvty;
	vsrc.lo = srcy - dvty;
	vsrc.hi = vsrc.lo + dsti.hi - dsti.lo;

	if(vsrc.lo == vsrc.hi || vsrc.lo == vdst.lo)
		return hole;
	else if(vsrc.hi <= 0 || vsrc.lo >= vh)
		hole = tktvclip(vdst, vh);
	else if(vdst.hi <= 0 || vdst.lo >= vh)
		hole = tktvclip(vsrc, vh);
	else if(i != nil) {
		src.x = 0;
		src.y = vsrc.lo;
		if(vdst.lo > vsrc.lo) {  /* see earlier text lines */
			if(vsrc.lo < 0) {
				src.y = 0;
				vdst.lo -= vsrc.lo;
			}
			if(vdst.hi > vh)
				vdst.hi = vh;
			hole.lo = src.y;
			hole.hi = vdst.lo;
		}
		else {  /* see later text lines */
			if(vsrc.hi > vh)
				vdst.hi -= (vsrc.hi - vh);
			if(vdst.lo < 0){
				src.y -= vdst.lo;
				vdst.lo = 0;
			}
			hole.lo = vdst.hi;
			hole.hi = src.y + (vdst.hi - vdst.lo);
		}
		if(vdst.hi > vdst.lo && (tkt->tflag&TkTdrawn)) {
			src = addpt(src, tkt->deltaiv);
			dst = rectaddpt(Rect(0, vdst.lo, vw, vdst.hi), tkt->deltaiv);
			d = tk->env->top->screen->display;
			locked = 0;
			if(!(tkt->tflag&TkTdlocked))
				locked = lockdisplay(d, 0);
			i = tkimageof(tk);
			tkt->image = i;
			if(i != nil)
				draw(i, dst, i, i->display->ones, src);
			if(locked)
				unlockdisplay(d);
		}
	}
	hole.lo += dvty;
	hole.hi += dvty;
	return hole;
}


/* firsty and lasty are in T space */
static void
tktnotdrawn(Tk *tk, int firsty, int lasty, int all)
{
	int any;
	TkTline *lend, *l;
	TkText *tkt = TKobj(TkText, tk);

	if(firsty >= lasty && !all)
		return;
	any = 0;
	lend = &tkt->end;
	for(l = tkt->start.next; l != lend; l = l->next) {
		if(l->orig.y+l->height < firsty)
			continue;
		if(l->orig.y >= lasty)
			break;
		l->flags &= ~TkTdrawn;
		any = 1;
	}
	if(any || all) {
		tk->flag |= Tkdirty;
		tktdrawbg(tk, firsty, lasty, all);
	}
}

/*
 * firsty and lasty are in T space
 */
static void
tktdrawbg(Tk *tk, int firsty, int lasty, int all)
{
	int vw, vh, locked;
	Rectangle r;
	Image *i;
	Display *d;
	TkText *tkt = TKobj(TkText, tk);

	/*
	 * If we are embedded in a text widget, we need to come in through
	 * the tkdrawtext routine, to ensure our clipr is set properly, so we
	 * just punt in that case.
	 */
	if(tk->parent != nil && tk->parent->type == TKtext) {
		tk->flag |= Tkrefresh;
		return;
	}
	vw = tk->act.width - tk->ipad.x;
	vh = tk->act.height - tk->ipad.y;
	if(all) {
		/* whole background is to be drawn, not just until last line */
		firsty = 0;
		lasty = 100000;
	}
	if(firsty >= lasty)
		return;
	firsty -= tkt->deltatv.y;
	lasty -= tkt->deltatv.y;
	if(firsty < 0)
		firsty = 0;
	if(lasty > vh)
		lasty = vh;
	r = rectaddpt(Rect(0, firsty, vw, lasty), tkt->deltaiv);
	if(r.min.y < r.max.y && (tkt->tflag&TkTdrawn)) {
		d = tk->env->top->screen->display;
		locked = 0;
		if(!(tkt->tflag&TkTdlocked))
			locked = lockdisplay(d, 0);
		i = tkimageof(tk);
		tkt->image = i;
		if(i != nil)
			draw(i, r, tkgc(tk->env, TkCbackgnd), d->ones, tkzp);
		if(locked)
			unlockdisplay(d);
	}
}

static void
tktfixscroll(Tk *tk, Point odeltatv, Point ndeltatv)
{
	int lasty;
	Interval oi, hole;
	Rectangle oclipr;
	Image *dst;
	TkText *tkt = TKobj(TkText, tk);

	if(eqpt(odeltatv, ndeltatv))
		return;

	/* set clipr to avoid spilling outside (in case didn't come in through draw) */
	dst = tkimageof(tk);
	if(dst != nil) {
		tkt->image = dst;
		oclipr = dst->clipr;
		tktsetclip(tk);
	}

	lasty = tkt->end.orig.y;
	if(odeltatv.x != ndeltatv.x)
		tktnotdrawn(tk, ndeltatv.y, lasty, 0);
	else {
		oi.lo = odeltatv.y;
		oi.hi = lasty;
		hole = tkttranslate(tk, oi, ndeltatv.y);
		tktnotdrawn(tk, hole.lo, hole.hi, 0);
	}
	if(dst != nil)
		tktreplclipr(dst, oclipr);
}

void
tktextgeom(Tk *tk)
{
	TkTindex ix;
	Rectangle oclipr;
	Image *dst;
	TkText *tkt = TKobj(TkText, tk);

	tktsetdeltas(tk, tkzp);
	/* find index of current top-left, so can see it again */
	tktxyind(tk, 0, 0, &ix);
	/* make sure scroll bar is redrawn */
	tkt->scrolltop[Tkvertical] = -1;
	tkt->scrolltop[Tkhorizontal] = -1;
	tkt->scrollbot[Tkvertical] = -1;
	tkt->scrollbot[Tkhorizontal] = -1;

	/* set clipr to avoid spilling outside (didn't come in through draw) */
	dst = tkimageof(tk);
	if(dst != nil) {
		tkt->image = dst;
		oclipr = dst->clipr;
		tktsetclip(tk);
	}

	tktfixgeom(tk, &tkt->start, tkt->end.prev);
	tktsee(tk, &ix, 1);

	if(dst != nil)
		tktreplclipr(dst, oclipr);
}

static char*
tktsetscroll(Tk *tk, int orient)
{
	TkText *tkt;
	TkTline *l;
	int ntot, nmin, nmax, top, bot, vw, vh;
	char *val, *cmd, *v, *e, *s;

	tkt = TKobj(TkText, tk);

	s = (orient == Tkvertical)? tkt->yscroll : tkt->xscroll;
	if(s == nil)
		return nil;

	vw = tk->act.width - tk->ipad.x;
	vh = tk->act.height - tk->ipad.y;

	if(orient == Tkvertical) {
		l = tkt->end.prev;
		ntot = l->orig.y + l->height;
		nmin = tkt->deltatv.y;
		if(vh <= 0)
			nmax = nmin;
		else
			nmax = nmin + vh;
	}
	else {
		ntot = tktmaxwid(tkt->start.next);
		nmin = tkt->deltatv.x;
		if(vw <= 0)
			nmax = nmin;
		else
			nmax = nmin + vw;
	}

	if(ntot == 0) {
		top = 0;
		bot = Tkfpscalar;
	}
	else {
		if(ntot < nmax)
			ntot = nmax;
		top = nmin*Tkfpscalar/ntot;
		bot = nmax*Tkfpscalar/ntot;
	}

	if(tkt->scrolltop[orient] == top && tkt->scrollbot[orient] == bot)
		return nil;

	tkt->scrolltop[orient] = top;
	tkt->scrollbot[orient] = bot;

	val = mallocz(Tkminitem, 0);
	if(val == nil)
		return TkNomem;
	cmd = mallocz(Tkminitem, 0);
	if(cmd == nil) {
		free(val);
		return TkNomem;
	}

	v = tkfprint(val, top);
	*v++ = ' ';
	tkfprint(v, bot);
	snprint(cmd, Tkminitem, "%s %s", s, val);
	e = tkexec(tk->env->top, cmd, nil);
	free(cmd);
	free(val);
	return e;
}

static char*
tktview(Tk *tk, char *arg, char **val, int nl, int *posn, int max, int orient)
{
	int top, bot, amount, n;
	char buf[Tkmaxitem], *v;

	if(*arg == '\0') {
		if ( max == 0 ) {
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

	arg = tkitem(buf, arg);
	if(strcmp(buf, "moveto") == 0) {
		tkfrac(tk->env->top, arg, &top, nil);
		*posn = top*max/Tkfpscalar;
	}
	else
	if(strcmp(buf, "scroll") == 0) {
		arg = tkitem(buf, arg);
		amount = atoi(buf);
		if(*arg == 'p')		/* Pages */
			amount *= nl;
		else				/* Lines or Characters */
		if(orient == Tkvertical) {
			/* needs improvement */
			amount *= tk->env->font->height;
		}
		else
			amount *= tk->env->wzero;
		n = *posn + amount;
		if(n < 0)
			n = 0;
		if(n > max)
			n = max;
		*posn = n;
	}
	else	
		return TkBadcm;

	if(*posn < 0)
		*posn = 0;
	bot = max - nl;
	if(*posn > bot)
		*posn = bot;

	return nil;
}

static void
tktclearsel(Tk *tk)
{
	TkTindex ibeg, iend;
	TkText *tkt = TKobj(TkText, tk);
	TkTop *top;

	if(tkt->selfirst == nil)
		return;
	top = tk->env->top;
	tktitemind(tkt->selfirst, &ibeg);
	tktitemind(tkt->sellast, &iend);

	tkttagchange(tk, TkTselid, &ibeg, &iend, 0);
	if (top->select == tk)
		top->select = nil;
}

static int
tktgetsel(Tk *tk, TkTindex *i1, TkTindex *i2)
{
	TkText *tkt =TKobj(TkText, tk);

	if(tkt->selfirst == nil)
		return 0;
	tktitemind(tkt->selfirst, i1);
	tktitemind(tkt->sellast, i2);
	return 1;
}

/*
 * Adjust tkt->deltatv so that indexed character is visible.
 *	- if seetop is true, make indexed char be at top of window
 *	- if it is already visible, do nothing.
 *	- if it is > 1/2 screenful off edge of screen, center it
 *	   else put it at bottom or top (whichever is nearer)
 *	- if first line is visible, put it at top
 *	- if last line is visible, allow one blank line at bottom
 *
 * BUG: should handle x visibility too
 */
static void
tktsee(Tk *tk, TkTindex *ix, int seetop)
{
	int ycur, ynext, deltatvy, adjy, h;
	Point p, odeltatv;
	TkTline *l, *el;
	TkText *tkt = TKobj(TkText, tk);

	deltatvy = tkt->deltatv.y;
	odeltatv = tkt->deltatv;
	h = tk->act.height;

	/* find p (in T space): top left of indexed line */
	l = ix->line;
	p = l->orig;

	/* ycur, ynext in V space */
	ycur = p.y - deltatvy;
	ynext = ycur + l->height;
	adjy = 0;

	/* quantize h to line boundaries (works if single font) */
	if ( l->height )
		h -= h%l->height;

	if(seetop) {
		deltatvy = p.y;
		adjy = 1;
	}
	else
	if(ycur < 0 || ynext >= h) {
		adjy = 1;

		if(ycur < -h/2 || ycur > 3*h/2)
			deltatvy = p.y - h/2;
		else if(ycur < 0)
			deltatvy = p.y;
		else
			deltatvy = p.y - h + l->height;

		el = tkt->end.prev;
		if(el != nil && el->orig.y - deltatvy < h)
			deltatvy = tkt->end.orig.y - h + tk->env->font->height;

		if(p.y - deltatvy < 0)
			deltatvy = p.y;
		if(deltatvy < 0)
			deltatvy = 0;
	}
	if(adjy) {
		tkt->deltatv.y = deltatvy;
		tktsetscroll(tk, Tkvertical);	/* XXX - Tad: err ignored */
		tktfixscroll(tk, odeltatv, tkt->deltatv);
	}
}

static int
tktcmatch(int c1, int c2, int nocase)
{
	if(nocase) {
		if(c1 >= 'a' && c1 <= 'z')
			c1 -= 'a' - 'A';
		if(c2 >= 'a' && c2 <= 'z')
			c2 -= 'a' - 'A';
	}
	return (c1 == c2);
}

static int
mprint(Mprint *m, char *fmt, ...)
{
	char *new;
	va_list arg;

	if(m->len - m->ptr < Tkmaxdump) {
		m->len += 4*Tkmaxdump;
		new = realloc(m->buf, m->len);
		if(new == nil) {
			free(m->buf);
			return -1;
		}
		m->buf = new;
	}
	va_start(arg, fmt);
	new = doprint(m->buf+m->ptr, m->buf+m->len, fmt, arg);
	va_end(arg);
	m->ptr = new - m->buf;
	return m->ptr;
}

/*
 * Return 1 if tag with id m1 ends before tag with id m2,
 * starting at the item after that indexed in ix (but don't
 * modify ix).
 */
static int
tagendsbefore(TkText *tkt, TkTindex *ix, int m1, int m2)
{
	int s1, s2;
	TkTindex ix1;
	TkTitem *i;

	ix1 = *ix;
	while(tktadjustind(tkt, TkTbyitem, &ix1)) {
		i = ix1.item;
		if(i->kind == TkTwin || i->kind == TkTcontline || i->kind == TkTmark)
			continue;
		s1 = tkttagset(i, m1);
		s2 = tkttagset(i, m2);
		if(!s1)
			return s2;
		else if(!s2)
			return 0;
	}
	return 0;
}

static int
tktsgmltags(TkText *tkt, Mprint *mp, TkTitem *iprev, TkTitem *i, TkTindex *ix, int *stack, int *pnstack, int *tmpstack)
{
	int nprev, n, m, r, k, j, ii, onstack, nt;

	nprev = 0;
	if(iprev != nil && (iprev->tags[0] != 0 || iprev->tagextra > 0))
		nprev = 32*(iprev->tagextra + 1);
	n = 0;
	if(i != nil && (i->tags[0] != 0 || i->tagextra > 0))
		n = 32*(i->tagextra + 1);
	nt = 0;
	if(n > 0) {
		/* find tags which open here */
		for(m = 0; m < n; m++)
			if(tkttagset(i, m) && (iprev == nil || !tkttagset(iprev, m)))
				tmpstack[nt++] = m;
	}
	if(nprev > 0) {
		/*
		 * Find lowest tag in stack that ends before any tag beginning here.
		 * We have to emit end tags all the way down to there, then add
		 * back the ones that haven't actually ended here, together with ones
		 * that start here, and sort all of the added ones so that tags that
		 * end later are lower in the stack.
		 */
		ii = *pnstack;
		for(k = *pnstack - 1; k >=0; k--) {
			m = stack[k];
			if(i == nil || !tkttagset(i, m))
				ii = k;
			else
				for(j = 0; j < nt; j++)
					if(tagendsbefore(tkt, ix, m, tmpstack[j]))
						ii = k;
		}
		for(k = *pnstack - 1; k >= ii; k--) {
			m = stack[k];
			r = mprint(mp, "</%s>", tkttagname(tkt, m));
			if(r < 0)
				return r;
			/* add m back to starting tags if m didn't actually end here */
			if(i != nil && tkttagset(i, m))
				tmpstack[nt++] = m;
		}
		*pnstack = ii;
	}
	if(nt > 0) {
		/* add tags which open  or reopen here */
		onstack = *pnstack;
		k = onstack;
		for(j = 0; j < nt; j++)
			stack[k++] = tmpstack[j];
		*pnstack = k;
		if(k - onstack > 1) {
			/* sort new stack entries so tags that end later are lower in stack */
			for(ii = k-2; ii>= onstack; ii--) {
				m = stack[ii];
				for(j = ii+1; j < k && tagendsbefore(tkt, ix, m, stack[j]); j++) {
					stack[j-1] = stack[j];
				}
				stack[j-1] = m;
			}
		}
		for(j = onstack; j < k; j++) {
			r = mprint(mp, "<%s>", tkttagname(tkt, stack[j]));
			if(r < 0)
				return r;
		}
	}
	return 0;
}

/*
 * In 'sgml' format, just print text (no special treatment of
 * special characters, except that < turns into &lt;)
 * interspersed with things like <Bold> and </Bold>
 * (where Bold is a tag name).
 * Make sure that the tag pairs nest properly.
*/
static char*
tktget(TkText *tkt, TkTindex *ix1, TkTindex *ix2, int sgml, char **val)
{
	int n, m, i, bychar, nstack;
	int *stack, *tmpstack;
	char *s;
	TkTitem *iprev;
	Tk *sub;
	Mprint mbuf;
	char *buf;


	if(!tktindbefore(ix1, ix2))
		return nil;

	stack = nil;
	tmpstack = nil;

	iprev = nil;
	memset(&mbuf, 0, sizeof(mbuf));
	buf = mallocz(100, 0);
	if(buf == nil)
		return TkNomem;
	if(sgml) {
		stack = malloc((tkt->nexttag+1)*sizeof(int));
		tmpstack = malloc((tkt->nexttag+1)*sizeof(int));
		if(stack == nil || tmpstack == nil)
			goto nomemret;
		nstack = 0;
	}
	for(;;) {
		if(ix1->item == ix2->item && ix1->pos == ix2->pos)
			break;
		s = nil;
		bychar = 0;
		m = 1;
		switch(ix1->item->kind) {
		case TkTrune:
			s = ix1->item->istring;
			s += tktutfpos(s, ix1->pos);
			if(ix1->item == ix2->item) {
				m = ix2->pos - ix1->pos;
				bychar = 1;
			}
			break;
		case TkTascii:
			s = ix1->item->istring + ix1->pos;
			if(ix1->item == ix2->item) {
				m = ix2->pos - ix1->pos;
				bychar = 1;
			}
			else {
				m = strlen(s);
				if(sgml && memchr(s, '<', m) != nil)
					bychar = 1;
			}
			break;
		case TkTtab:
			s = "\t";
			break;
		case TkTnewline:
			s = "\n";
			break;
		case TkTwin:
			sub = ix1->item->iwin->sub;
			if(sgml &&  sub != nil && sub->name != nil) {
				snprint(buf, 100, "<Window %s>", sub->name->name);
				s = buf;
			}
		}
		if(s != nil) {
			if(sgml) {
				n = tktsgmltags(tkt, &mbuf, iprev, ix1->item, ix1, stack, &nstack, tmpstack);
				if(n < 0)
					goto nomemret;
			}
			if(bychar) {
				if (ix1->item->kind == TkTrune)
					n = mprint(&mbuf, "%.*s", m, s);
				else {
					n = 0;
					for(i = 0; i < m && n >= 0; i++) {
						if(s[i] == '<')
							n = mprint(&mbuf, "&lt;");
						else
							n = mprint(&mbuf, "%c", s[i]);
					}
				}
			}
			else
				n = mprint(&mbuf, "%s", s);
			if(n < 0)
				goto nomemret;
			iprev = ix1->item;
		}
		if(ix1->item == ix2->item)
			break;
		if(!tktadjustind(tkt, TkTbyitem, ix1)) {
			if(tktdbg)
				print("tktextget botch\n");
			break;
		}
	}
	if(sgml) {
		n = tktsgmltags(tkt, &mbuf, iprev, nil, nil, stack, &nstack, tmpstack);
		if(n < 0)
			goto nomemret;
	}

	*val = mbuf.buf;
	free(buf);
	return nil;

nomemret:
	free(buf);
	if(stack != nil)
		free(stack);
	if(tmpstack != nil)
		free(tmpstack);
	return TkNomem;
}

/* Widget Commands (+ means implemented)
	+bbox
	+cget
	+compare
	+configure
	+debug
	+delete
	+dlineinfo
	+dump
	+get
	+index
	+insert
	+mark
	+scan
	+search
	+see
	+tag
	+window
	+xview
	+yview
*/

char*
tktextbbox(Tk *tk, char *arg, char **val)
{
	char *e;
	int noclip, w, h;
	Rectangle r, rview;
	TkTindex ix;
	TkText *tkt;
	char buf[Tkmaxitem];

	e = tktindparse(tk, &arg, &ix);
	if(e != nil)
		return e;

	noclip = 0;
	if(*arg != '\0') {
		/* extension to tk4.0:
		 * "noclip" means don't clip to viewable area
		 * "all" means give unclipped bbox of entire contents
		 */
		arg = tkword(tk->env->top, arg, buf, buf+sizeof(buf));
		if(strcmp(buf, "noclip") == 0)
			noclip = 1;
		else
		if(strcmp(buf, "all") == 0) {
			tkt = TKobj(TkText, tk);
			w = tktmaxwid(tkt->start.next);
			h = tkt->end.orig.y;
			return tkvalue(val, "{0 0 %d %d}", w, h);
		}
	}

	r = tktbbox(tk, &ix);

	rview.min.x = 0;
	rview.min.y = 0;
	rview.max.x = tk->act.width - tk->ipad.x;
	rview.max.y = tk->act.height - tk->ipad.y;
	if(noclip || rectclip(&r, rview))
		return tkvalue(val, "{%d %d %d %d}", r.min.x, r.min.y,
			r.max.x-r.min.x, r.max.y-r.min.y);
	return nil;
}

char*
tktextbutton1(Tk *tk, char *arg, char **val)
{
	char *e;
	Point p;
	TkCtxt *c;
	TkTindex ix;
	TkTmarkinfo *mi;
	TkText *tkt = TKobj(TkText, tk);

	USED(val);

	e = tktxyparse(tk, &arg, &p);
	if(e != nil)
		return e;

	tktxyind(tk, p.x, p.y, &ix);
	tkt->track = p;
	tkt->tflag &= ~TkTjustfoc;
	c = tk->env->top->ctxt;
	if(!(tk->flag&Tkdisabled) && c->tkKgrab != tk 
                      && (tk->name != nil) && ix.item->kind != TkTwin) {
		tkfocus(tk->env->top, tk->name->name, nil);
		tkt->tflag |= TkTjustfoc;
		return nil;
	}
	if(ix.item->kind == TkTwin)
		tkt->focus = ix.item;
	else
		tkt->focus = nil;

	mi = tktfindmark(tkt->marks, "insert");
	if(tktdbg && !mi) {
		print("tktextbutton1: botch\n");
		return nil;
	}
	tktmarkmove(tk, mi, &ix);

	tktclearsel(tk);

	return nil;
}

char*
tktextcget(Tk *tk, char *arg, char **val)
{
	TkOptab tko[3];

	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tk;
	tko[1].optab = textopts;
	tko[2].ptr = nil;

	return tkgencget(tko, arg, val);
}

char*
tktextcompare(Tk *tk, char *arg, char **val)
{
	int op;
	char *e;
	TkTindex i1, i2;
	TkText *tkt;
	TkStab *s;
	char *buf;

	tkt = TKobj(TkText, tk);

	e = tktindparse(tk, &arg, &i1);
	if(e != nil)
		return e;

	if(*arg == '\0')
		return TkBadcm;

	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil)
		return TkNomem;

	arg = tkword(tk->env->top, arg, buf, buf+Tkmaxitem);

	op = -1;
	for(s = tkcompare; s->val; s++)
		if(strcmp(s->val, buf) == 0) {
			op = s->con;
			break;
		}
	if(op == -1) {
		free(buf);
		return TkBadcm;
	}

	e = tktindparse(tk, &arg, &i2);
	if(e != nil) {
		free(buf);
		return e;
	}

	e = tkvalue(val, tktindcompare(tkt, &i1, op, &i2)? "1" : "0");
	free(buf);
	return e;
}

char*
tktextconfigure(Tk *tk, char *arg, char **val)
{
	char *e;
	TkGeom g;
	TkText *tkt;
	TkOptab tko[3];

	tkt = TKobj(TkText, tk);
	tko[0].ptr = tk;
	tko[0].optab = tkgeneric;
	tko[1].ptr = tk;
	tko[1].optab = textopts;
	tko[2].ptr = nil;

	if(*arg == '\0')
		return tkconflist(tko, val);

	g = tk->req;
	e = tkparse(tk->env->top, arg, tko, nil);
	tkgeomchg(tk, &g);
	tktnotdrawn(tk, 0, tkt->end.orig.y, 1);
	return e;
}

char*
tktextdebug(Tk *tk, char *arg, char **val)
{
	char buf[Tkmaxitem];

	tkword(tk->env->top, arg, buf, buf+sizeof(buf));
	if(*buf == '\0')
		return tkvalue(val, "%s", tktdbg? "on" : "off");
	else {
		tktdbg = (strcmp(buf, "1") == 0 || strcmp(buf, "yes") == 0);
		if(tktdbg) {
			tktprinttext(TKobj(TkText, tk));
		}
		return nil;
	}
}

char*
tktextdelete(Tk *tk, char *arg, char **val)
{
	int sameit;
	char *e;
	TkTindex i1, i2, ip, isee;
	TkTline *lmin;
	TkText *tkt = TKobj(TkText, tk);

	USED(val);

	e = tktindparse(tk, &arg, &i1);
	if(e != nil)
		return e;
	tktadjustind(tkt, TkTbycharstart, &i1);

	e = tktsplititem(&i1);
	if(e != nil)
		return e;

	if(*arg != '\0') {
		e = tktindparse(tk, &arg, &i2);
		if(e != nil)
			return e;
	}
	else {
		i2 = i1;
		tktadjustind(tkt, TkTbychar, &i2);
	}
	if(tktindcompare(tkt, &i1, TkGte, &i2))
		return nil;

	sameit = (i1.item == i2.item);

	/* save possible fixup see place */
	isee.line = nil;
	if(i2.line->orig.y < tkt->deltatv.y) {
		/* delete completely precedes view */
		tktxyind(tk, 0, 0, &isee);
	}

	e = tktsplititem(&i2);
	if(e != nil)
		return e;

	if(sameit) {
		/* after split, i1 should be in previous item to i2 */
		ip = i2;
		tktadjustind(tkt, TkTbyitemback, &ip);
		i1.item = ip.item;
	}

	lmin = i1.line->prev;
	while(i1.item != i2.item) {
		if(i1.item->kind != TkTmark)
			tktremitem(tkt, &i1);
			/* tktremitem moves i1 to next item */
		else {
			if(!tktadjustind(tkt, TkTbyitem, &i1)) {
				if(tktdbg)
					print("tktextdelete botch\n");
				break;
			}
		}
	}

	tktfixgeom(tk, lmin, i1.line);
	if(isee.line != nil)
		tktsee(tk, &isee, 1);
	return nil;
}

char*
tktextdelins(Tk *tk, char *arg, char **val)
{
	int m, c, skipping, wordc, n;
	TkTindex ix, ix2;
	TkText *tkt = TKobj(TkText, tk);
	char buf[30];

	USED(val);

	if(tk->flag&Tkdisabled)
		return nil;

	if(tktgetsel(tk, &ix, &ix2))
		tktextdelete(tk, "sel.first sel.last", nil);
	else {
		while(*arg == ' ')
			arg++;
		if(*arg == '-') {
			m = arg[1];
			if(m == 'c')
				n = 1;
			else {
				/* delete prev word (m=='w') or prev line (m=='l') */
				if(!tktmarkind(tk, "insert", &ix))
					return nil;
				if(!tktadjustind(tkt, TkTbycharback, &ix))
					return nil;
				n = 1;
				/* ^W skips back over nonwordchars, then takes maximal seq of wordchars */
				skipping = 1;
				for(;;) {
					c = tktindrune(&ix);
					if(c == '\n') {
						/* special case: always delete at least one char */
						if(n > 1)
							n--;
						break;
					}
					if(m == 'w') {
						wordc = tktiswordchar(c);
						if(wordc && skipping)
							skipping = 0;
						else if(!wordc && !skipping) {
							n--;
							break;
						}
					}
					if(tktadjustind(tkt, TkTbycharback, &ix))
						n++;
					else
						break;
				}
			}
			sprint(buf, "insert-%dc insert", n);
			tktextdelete(tk, buf, nil);
		}
		else
			tktextdelete(tk, "insert", nil);
		tktextsee(tk, "insert", nil);
	}

	return nil;
}

char*
tktextdlineinfo(Tk *tk, char *arg, char **val)
{
	char *e;
	TkTindex ix;
	TkTline *l;
	Point p;
	int vh;
	TkText *tkt = TKobj(TkText, tk);

	e = tktindparse(tk, &arg, &ix);
	if(e != nil)
		return e;

	l = ix.line;
	vh = tk->act.height;

	/* get p in V space */
	p = subpt(l->orig, tkt->deltatv);
	if(p.y+l->height < 0 || p.y >= vh)
		return nil;

	return tkvalue(val, "%d %d %d %d %d",
		p.x, p.y, l->width, l->height, p.y+l->ascent);
}

char*
tktextdump(Tk *tk, char *arg, char **val)
{
	TkTline *l;
	TkTitem *i;
	Mprint mbuf;
	TkText *tkt;
	TkDump tkdump;
	TkOptab tko[2];
	TkTtaginfo *ti;
	TkName *names, *n;
	char *e, *win, *p;
	TkTindex ix1, ix2;
	int r, j, numitems, fg, bg;

	tkt = TKobj(TkText, tk);

	memset(&mbuf, 0, sizeof(mbuf));

	tkdump.sgml = 0;
	tkdump.metrics = 0;

	tko[0].ptr = &tkdump;
	tko[0].optab = dumpopts;
	tko[1].ptr = nil;
	names = nil;
	e = tkparse(tk->env->top, arg, tko, &names);
	if(e != nil)
		return e;

	if(names != nil) {			/* supplied indices */
		p = names->name;
		e = tktindparse(tk, &p, &ix1);
		if(e != nil) {
			tkfreename(names);
			return e;
		}
		n = names->link;
		if(n != nil) {
			p = n->name;
			e = tktindparse(tk, &p, &ix2);
			if(e != nil) {
				tkfreename(names);
				return e;
			}
		}
		else {		
			ix2 = ix1;
			tktadjustind(tkt, TkTbychar, &ix2);
		}
		tkfreename(names);
		if(!tktindbefore(&ix1, &ix2))
			return nil;
	}
	else
		return TkBadix;
	
	if(tkdump.metrics != 0) {
		if(mprint(&mbuf, "%%Fonts\n") < 0)
			return TkNomem;
		for(ti=tkt->tags; ti != nil; ti=ti->next) {
			if(ti->env == nil || ti->env->font == nil)
				continue;
			if(mprint(&mbuf, "%d::%s\n", ti->id,ti->env->font->name) < 0)
				return TkNomem;
		}
		if(mprint(&mbuf, "-1::%s\n%%Colors\n", tk->env->font->name) < 0)
			return TkNomem;
		for(ti=tkt->tags; ti != nil; ti=ti->next) {
			if(ti->env == nil)
				continue;
			bg = ti->env->colors[TkCbackgnd];
			fg = ti->env->colors[TkCforegnd];
			if(bg == tk->env->colors[TkCbackgnd] &&
			   fg == ti->env->colors[TkCforegnd])
				continue;
			r = mprint(&mbuf,"%d::#%.6x\n", ti->id, cmap2rgb(bg));
			if(r < 0)
				return TkNomem;
			r = mprint(&mbuf,"%d::#%.6x\n", ti->id, cmap2rgb(fg));
			if(r < 0)
				return TkNomem;
		}
		if(mprint(&mbuf, "%%Lines\n") < 0)
			return TkNomem;

		/*
		 * In 'metrics' format lines are recorded in the following way:
		 *    xorig yorig wd ht as [data]
		 * where data is of the form:
		 *    CodeWidth{tags} data
		 * For Example;
		 *    A200{200000} Hello World!
		 * denotes an A(scii) contiguous string of 200 pixels with
		 * bit 20 set in its tags which corresponds to some font.
		 *
	 	*/
		if(ix2.line->items != ix2.item)
			ix2.line = ix2.line->next;
		for(l = ix1.line; l != ix2.line; l = l->next) {
			numitems = 0;
			for(i = l->items; i != nil; i = i->next) {
				if(i->kind != TkTmark)
					numitems++;
			}
			r = mprint(&mbuf, "%d %d %d %d %d %d ",
				l->orig.x, l->orig.y, l->width, l->height, l->ascent,numitems);
			if(r < 0)
				return TkNomem;
			for(i = l->items; i != nil; i = i->next) {
				switch(i->kind) {
				case TkTascii:
				case TkTrune:
					r = i->kind == TkTascii ? 'A' : 'R';
					if(mprint(&mbuf,"[%c%d{", r, i->width) < 0)
						return TkNomem;
					if(i->tags !=0 || i->tagextra !=0) {
						if(mprint(&mbuf,"%ux", i->tags[0]) < 0)
							return TkNomem;
						for(j=0; j < i->tagextra; j++)
							if(mprint(&mbuf,"::%ux", i->tags[j+1]) < 0)
								return TkNomem;
					}
					if(mprint(&mbuf,"}%s]", i->istring) < 0)
						return TkNomem;
					break;
				case TkTnewline:
				case TkTcontline:
					r = i->kind == TkTnewline ? 'N' : 'C';
					if(mprint(&mbuf, "[%c]", r) < 0)
						return TkNomem;
					break;
				case TkTtab:
					if(mprint(&mbuf,"[T%d]",i->width) < 0)
						return TkNomem;
					break;
				case TkTwin:
					win = "<null>";
					if(i->iwin->sub != nil)
						win = i->iwin->sub->name->name;
					if(mprint(&mbuf,"[W%d %s]",i->width, win) < 0)
						return TkNomem;
					break;
				}
				if(mprint(&mbuf, " ") < 0)
					return TkNomem;
	
			}
			if(mprint(&mbuf, "\n") < 0)
				return TkNomem;
			*val = mbuf.buf;
		}
	}
	else
		return tktget(tkt, &ix1, &ix2, tkdump.sgml, val);

	return nil;
}


char*
tktextget(Tk *tk, char *arg, char **val)
{
	char *e;
	TkTindex ix1, ix2;
	TkText *tkt = TKobj(TkText, tk);

	e = tktindparse(tk, &arg, &ix1);
	if(e != nil)
		return e;

	if(*arg != '\0') {
		e = tktindparse(tk, &arg, &ix2);
		if(e != nil)
			return e;
	}
	else {
		ix2 = ix1;
		tktadjustind(tkt, TkTbychar, &ix2);
	}
	return tktget(tkt, &ix1, &ix2, 0, val);
}

char*
tktextindex(Tk *tk, char *arg, char **val)
{
	char *e;
	TkTindex ix;
	TkText *tkt = TKobj(TkText, tk);

	e = tktindparse(tk, &arg, &ix);
	if(e != nil)
		return e;
	return tkvalue(val, "%d.%d", tktlinenum(tkt, &ix), tktlinepos(tkt, &ix));
}

char*
tktextinsert(Tk *tk, char *arg, char **val)
{
	int n;
	char *e, *p, *pe;
	TkTindex ins, pins;
	TkTtaginfo *ti;
	TkText *tkt;
	TkTline *lmin;
	TkTop *top;
	TkTitem *tagit;
	char *tbuf, *buf;

	USED(val);

	tkt = TKobj(TkText, tk);
	top = tk->env->top;

	e = tktindparse(tk, &arg, &ins);
	if(e != nil)
		return e;

	if(ins.item->kind == TkTmark) {
		if(ins.item->imark->gravity == Tkleft) {
			while(ins.item->kind == TkTmark && ins.item->imark->gravity == Tkleft)
				if(!tktadjustind(tkt, TkTbyitem, &ins)) {
					if(tktdbg)
						print("tktextinsert botch\n");
					break;
				}
		}
		else {
			for(;;) {
				pins = ins;
				if(!tktadjustind(tkt, TkTbyitemback, &pins))
					break;
				if(pins.item->kind == TkTmark && pins.item->imark->gravity == Tkright)
					ins = pins;
				else
					break;
			}
		}
	}

	lmin = ins.line->prev;

	n = strlen(arg) + 1;
	if(n < Tkmaxitem)
		n = Tkmaxitem;
	tbuf = malloc(n);
	if(tbuf == nil)
		return TkNomem;
	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil) {
		free(tbuf);
		return TkNomem;
	}

	tagit = nil;

	while(*arg != '\0') {
		arg = tkword(top, arg, tbuf, tbuf+n);
		if(*arg != '\0') {
			/* tag list spec -- add some slop to tagextra for added tags */
			e = tktnewitem(0, (tkt->nexttag-1)/32 + 1, &tagit);
			if(e != nil) {
				free(tbuf);
				free(buf);
				return e;
			}
			arg = tkword(top, arg, buf, buf+Tkmaxitem);
			p = buf;
			while(*p) {
				while(*p == ' ') {
					p++;
				}
				if(*p == '\0')
					break;
				pe = strchr(p, ' ');
				if(pe != nil)
					*pe = '\0';
				ti = tktfindtag(tkt->tags, p);
				if(ti == nil) {
					e = tktaddtaginfo(tk, p, &ti);
					if(e != nil) {
						if(tagit != nil)
							free(tagit);
						free(tbuf);
						free(buf);
						return e;
					}
				}
				tkttagbit(tagit, ti->id, 1);
				if(pe == nil)
					break;
				else
					p = pe+1;
			}
		}
		e = tktinsert(tk, &ins, tbuf, tagit);
		if(tagit != nil) {
			free(tagit);
			tagit = nil;
		}
		if(e != nil) {
			free(tbuf);
			free(buf);
			return e;
		}
	}

	tktfixgeom(tk, lmin, ins.line);

	free(tbuf);
	free(buf);

	return nil;
}

char*
tktextinserti(Tk *tk, char *arg, char **val)
{
	int n;
	TkTline *lmin;
	TkTindex ix, is1, is2;
	TkText *tkt = TKobj(TkText, tk);
	char *tbuf, *buf;

	USED(val);

	if(tk->flag&Tkdisabled)
		return nil;

	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil)
		return TkNomem;

	tbuf = nil;
	n = strlen(arg) + 1;
	if(n < Tkmaxitem)
		tkword(tk->env->top, arg, buf, buf+sizeof(buf));
	else {
		tbuf = malloc(n);
		if(tbuf == nil) {
			free(buf);
			return TkNomem;
		}
		tkword(tk->env->top, arg, tbuf, buf+n);
	}
	if(*buf == '\0') {
    err1:
		if(tbuf != nil)
			free(tbuf);
		free(buf);
		return nil;
	}
	if(!tktmarkind(tk, "insert", &ix)) {
		print("tktextinserti: botch\n");
		goto err1;
	}
	if(tktgetsel(tk, &is1, &is2)) {
		if(tktindcompare(tkt, &is1, TkLte, &ix) &&
		   tktindcompare(tkt, &is2, TkGte, &ix)) {
			tktextdelete(tk, "sel.first sel.last", nil);
			/* delete might have changed ix item */
			tktmarkind(tk, "insert", &ix);
		}
	}

	lmin = ix.line->prev;
	tktinsert(tk, &ix, tbuf==nil ? buf : tbuf, 0);
	tktsee(tk, &ix, 0);
	tktfixgeom(tk, lmin, ix.line);
	if(tbuf != nil)
		free(tbuf);
	free(buf);
	return nil;
}

char*
tktextmark(Tk *tk, char *arg, char **val)
{
	char *buf;
	TkCmdtab *cmd;

	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil)
		return TkNomem;
	arg = tkword(tk->env->top, arg, buf, buf+Tkmaxitem);
	for(cmd = tktmarkcmd; cmd->name != nil; cmd++) {
		if(strcmp(cmd->name, buf) == 0) {
			free(buf);
			return cmd->fn(tk, arg, val);
		}
	}
	free(buf);
	return TkBadcm;
}

char*
tktextscan(Tk *tk, char *arg, char **val)
{
	char *e;
	int mark, x, y, xmax, ymax, vh, vw;
	Point p, odeltatv;
	char buf[Tkmaxitem];
	TkText *tkt = TKobj(TkText, tk);

	USED(val);

	arg = tkword(tk->env->top, arg, buf, buf+sizeof(buf));

	if(strcmp(buf, "mark") == 0)
		mark = 1;
	else
	if(strcmp(buf, "dragto") == 0)
		mark = 0;
	else
		return TkBadcm;

	e = tktxyparse(tk, &arg, &p);
	if(e != nil)
		return e;

	if(mark)
		tkt->track = p;
	else {
		odeltatv = tkt->deltatv;
		vw = tk->act.width - tk->ipad.x;
		vh = tk->act.height - tk->ipad.y;
		ymax = tkt->end.prev->orig.y + tkt->end.prev->height - vh;
		y = tkt->deltatv.y -10*(p.y - tkt->track.y);
		if(y < 0)
			y = 0;
		if(y > ymax)
			y = ymax;
		tkt->deltatv.y = y;
		e = tktsetscroll(tk, Tkvertical);
		if(e != nil)
			return e;
		if(tkt->opts[TkTwrap] == Tkwrapnone) {
			xmax = tktmaxwid(tkt->start.next) - vw;
			x = tkt->deltatv.x + 10*(p.x - tkt->track.x);
			if(x < 0)
				x = 0;
			if(x > xmax)
				x = xmax;
			tkt->deltatv.x = x;
			e = tktsetscroll(tk, Tkhorizontal);
			if(e != nil)
				return e;
		}
		tktfixscroll(tk, odeltatv, tkt->deltatv);
	}

	return nil;
}

char*
tktextscrollpages(Tk *tk, char *arg, char **val)
{
	TkText *tkt = TKobj(TkText, tk);

	USED(tkt);
	USED(arg);
	USED(val);
	return nil;
}

char*
tktextsearch(Tk *tk, char *arg, char **val)
{
	int i, n;
	Rune r;
	char *e, *s;
	int wrap, fwd, nocase;
	TkText *tkt;
	TkTindex ix1, ix2, ixstart, ixend, tx;
	char buf[Tkmaxitem];

	tkt = TKobj(TkText, tk);

	fwd = 1;
	nocase = 0;

	while(*arg != '\0') {
		arg = tkword(tk->env->top, arg, buf, buf+sizeof(buf));
		if(*buf != '-')
			break;
		if(strcmp(buf, "-backwards") == 0)
			fwd = 0;
		else if(strcmp(buf, "-nocase") == 0)
			nocase = 1;
		else if(strcmp(buf, "--") == 0) {
			arg = tkword(tk->env->top, arg, buf, buf+sizeof(buf));
			break;
		}
	}

	tktstartind(tkt, &ixstart);
	tktadjustind(tkt, TkTbycharstart, &ixstart);
	tktendind(tkt, &ixend);

	if(*arg == '\0')
		return TkOparg;

	e = tktindparse(tk, &arg, &ix1);
	if(e != nil)
 		return e;
	tktadjustind(tkt, fwd? TkTbycharstart : TkTbycharback, &ix1);

	if(*arg != '\0') {
		wrap = 0;
		e = tktindparse(tk, &arg, &ix2);
		if(e != nil)
			return e;
		if(!fwd)
			tktadjustind(tkt, TkTbycharback, &ix2);
	}
	else {
		wrap = 1;
		if(fwd) {
			if(tktindcompare(tkt, &ix1, TkEq, &ixstart))
				ix2 = ixend;
			else {
				ix2 = ix1;
				tktadjustind(tkt, TkTbycharback, &ix2);
			}
		}
		else {
			if(tktindcompare(tkt, &ix1, TkEq, &ixend))
				ix2 = ixstart;
			else {
				ix2 = ix1;
				tktadjustind(tkt, TkTbychar, &ix2);
			}
		}
	}
	tktadjustind(tkt, TkTbycharstart, &ix2);
	if(tktindcompare(tkt, &ix1, TkEq, &ix2))
		return nil;

	if(*buf == '\0')
		return tkvalue(val, "%d.%d", tktlinenum(tkt, &ix1), tktlinepos(tkt, &ix1));

	while(!(ix1.item == ix2.item && ix1.pos == ix2.pos)) {
		tx = ix1;
		for(i = 0; buf[i] != '\0'; i++) {
			switch(tx.item->kind) {
			case TkTascii:
				if(!tktcmatch(tx.item->istring[tx.pos], buf[i], nocase))
					goto nomatch;
				break;
			case TkTrune:
				s = tx.item->istring;
				s += tktutfpos(s, tx.pos);
				n = chartorune(&r, s);
				if(strncmp(s, buf+i, n) != 0)
					goto nomatch;
				i += n-1;
				break;
			case TkTtab:
				if(buf[i] != '\t')
					goto nomatch;
				break;
			case TkTnewline:
				if(buf[i] != '\n')
					goto nomatch;
				break;
			default:
				goto nomatch;
			}
			tktadjustind(tkt, TkTbychar, &tx);
		}
		return tkvalue(val, "%d.%d", tktlinenum(tkt, &ix1), tktlinepos(tkt, &ix1));
	nomatch:
		if(fwd) {
			if(!tktadjustind(tkt, TkTbychar, &ix1)) {
				if(!wrap)
					break;
				ix1 = ixstart;
			}
		}
		else {
			if(!tktadjustind(tkt, TkTbycharback, &ix1)) {
				if(!wrap)
					break;
				ix1 = ixend;
			}
		}
	}

	return nil;
}

char*
tktextsee(Tk *tk, char *arg, char **val)
{
	char *e;
	TkTindex ix;

	USED(val);

	e = tktindparse(tk, &arg, &ix);
	if(e != nil)
		return e;

	tktsee(tk, &ix, 0);
	return nil;
}

char*
tktextselection(Tk *tk, char *arg, char **val)
{
	USED(val);
	if (strcmp(arg, " clear") == 0) {
		tktclearsel(tk);
		return nil;
	}
	else
		return TkBadcm;
}

char*
tktextselectto(Tk *tk, char *arg, char **val)
{
	int dbl, dx, dy, halfway;
	char *e;
	Point p;
	TkTindex cur, insert, first, last;
	TkText *tkt = TKobj(TkText, tk);

	USED(val);

	if(tkt->tflag & TkTjustfoc)
		return nil;

	e = tktxyparse(tk, &arg, &p);
	if(e != nil)
		return e;

	arg = tkskip(arg, " ");
	if(*arg == 'd')
		dbl = 1;
	else {
		dbl = 0;
		dx = tkt->track.x - p.x;
		dy =  tkt->track.y - p.y;
		if(dx*dx +dy*dy <= 9)
			return nil;
		tkt->track = p;
	}

	tktclearsel(tk);

	halfway = tktxyind(tk, p.x, p.y, &cur);

	if(!dbl) {
		if(!tktmarkind(tk, "insert", &insert))
			insert = cur;

		if(tktindcompare(tkt, &cur, TkLt, &insert)) {
			first = cur;
			last = insert;
		}
		else {
			first = insert;
			last = cur;
			if(halfway)
				tktadjustind(tkt, TkTbychar, &last);
			if(last.line == &tkt->end)
				tktadjustind(tkt, TkTbycharback, &last);
			if(tktindcompare(tkt, &first, TkGte, &last))
				return nil;
		}
	}
	else {
		first = cur;
		last = cur;
		tktdoubleclick(tkt, &first, &last);
	}

	tkttagchange(tk, TkTselid, &first, &last, 1);
	return nil;
}

static char tktleft1[] = "{[(<";
static char tktright1[] = "}])>";
static char tktleft2[] = "\n";
static char tktleft3[] = "\'\"`";

static char *tktleft[] = {tktleft1, tktleft2, tktleft3, nil};
static char *tktright[] = {tktright1,  tktleft2, tktleft3, nil};

static void
tktdoubleclick(TkText *tkt, TkTindex *first, TkTindex *last)
{
	int c, i;
	TkTindex ix, ix2;
	char *r, *l, *p;

	for(i = 0; tktleft[i] != nil; i++) {
		ix = *first;
		l = tktleft[i];
		r = tktright[i];
		/* try matching character to left, looking right */
		ix2 = ix;
		if(!tktadjustind(tkt, TkTbycharback, &ix2))
			c = '\n';
		else
			c = tktindrune(&ix2);
		p = strchr(l, c);
		if(p != nil) {
			if(tktclickmatch(tkt, c, r[p-l], 1, &ix)) {
				*last = ix;
				if(c != '\n')
					tktadjustind(tkt, TkTbycharback, last);
			}
			return;
		}
		/* try matching character to right, looking left */
		c = tktindrune(&ix);
		p = strchr(r, c);
		if(p != nil) {
			if(tktclickmatch(tkt, c, l[p-r], -1, &ix)) {
				*last = *first;
				if(c == '\n')
					tktadjustind(tkt, TkTbychar, last);
				*first = ix;
				if(!(c=='\n' && ix.line == tkt->start.next && ix.item == ix.line->items))
					tktadjustind(tkt, TkTbychar, first);
			}
			return;
		}
	}
	/* try filling out word to right */
	while(tktiswordchar(tktindrune(last))) {
		if(!tktadjustind(tkt, TkTbychar, last))
			break;
	}
	/* try filling out word to left */
	for(;;) {
		ix = *first;
		if(!tktadjustind(tkt, TkTbycharback, &ix))
			break;
		if(!tktiswordchar(tktindrune(&ix)))
			break;
		*first = ix;
	}
}

static int
tktclickmatch(TkText *tkt, int cl, int cr, int dir, TkTindex *ix)
{
	int c, nest, atend;

	nest = 1;
	atend = 0;
	for(;;) {
		if(dir > 0) {
			if(atend)
				break;
			c = tktindrune(ix);
			atend = !tktadjustind(tkt, TkTbychar, ix);
		} else {
			if(!tktadjustind(tkt, TkTbycharback, ix))
				break;
			c = tktindrune(ix);
		}
		if(c == cr){
			if(--nest==0)
				return 1;
		}else if(c == cl)
			nest++;
	}
	return cl=='\n' && nest==1;
}

char*
tktextsetcursor(Tk *tk, char *arg, char **val)
{
	char *e;
	TkTindex ix;
	TkTmarkinfo *mi;
	TkText *tkt = TKobj(TkText, tk);

	USED(val);

	/* do clearsel here, because it can change indices */
	tktclearsel(tk);

	e = tktindparse(tk, &arg, &ix);
	if(e != nil)
		return e;

	mi = tktfindmark(tkt->marks, "insert");
	if(tktdbg && mi == nil) {
		print("tktextsetcursor: botch\n");
		return nil;
	}
	tktmarkmove(tk, mi, &ix);
	tktsee(tk, &ix, 0);
	return nil;
}

char*
tktexttag(Tk *tk, char *arg, char **val)
{
	char *buf;
	TkCmdtab *cmd;

	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil)
		return TkNomem;
	arg = tkword(tk->env->top, arg, buf, buf+Tkmaxitem);
	for(cmd = tkttagcmd; cmd->name != nil; cmd++) {
		if(strcmp(cmd->name, buf) == 0) {
			free(buf);
			return cmd->fn(tk, arg, val);
		}
	}
	free(buf);
	return TkBadcm;
}

char*
tktextwindow(Tk *tk, char *arg, char **val)
{
	char buf[Tkmaxitem];
	TkCmdtab *cmd;

	arg = tkword(tk->env->top, arg, buf, buf+sizeof(buf));
	for(cmd = tktwincmd; cmd->name != nil; cmd++) {
		if(strcmp(cmd->name, buf) == 0)
			return cmd->fn(tk, arg, val);
	}
	return TkBadcm;
}

char*
tktextxview(Tk *tk, char *arg, char **val)
{
	int ntot, vw;
	char *e;
	Point odeltatv;
	TkText *tkt = TKobj(TkText, tk);

	odeltatv = tkt->deltatv;
	vw = tk->act.width - tk->ipad.x;
	ntot = tktmaxwid(tkt->start.next);
	if(ntot < tkt->deltatv.x +vw)
		ntot = tkt->deltatv.x + vw;
	e = tktview(tk, arg, val, vw, &tkt->deltatv.x, ntot, Tkhorizontal);
	if(e == nil) {
		e = tktsetscroll(tk, Tkhorizontal);
		if(e == nil)
			tktfixscroll(tk, odeltatv, tkt->deltatv);
	}
	return e;
}

char*
tktextyview(Tk *tk, char *arg, char **val)
{
	int ntot, vh;
	char *e;
	TkTline *l;
	Point odeltatv;
	TkTindex ix;
	TkText *tkt = TKobj(TkText, tk);
	char buf[Tkmaxitem], *v;

	if(*arg != '\0') {
		v = tkitem(buf, arg);
		if(strcmp(buf, "-pickplace") == 0)
			return tktextsee(tk,v, val);
		if(strcmp(buf, "moveto") != 0 && strcmp(buf, "scroll") != 0) {
			e = tktindparse(tk, &arg, &ix);
			if(e != nil)
				return e;
			tktsee(tk, &ix, 1);
			return nil;
		}
	}
	odeltatv = tkt->deltatv;
	vh = tk->act.height;
	l =  tkt->end.prev;
	ntot = l->orig.y + l->height;
	if(ntot < tkt->deltatv.y + vh)
		ntot = tkt->deltatv.y + vh;
	e = tktview(tk, arg, val, vh, &tkt->deltatv.y, ntot, Tkvertical);
	if(e == nil) {
		e = tktsetscroll(tk, Tkvertical);
		if(e == nil)
			tktfixscroll(tk, odeltatv, tkt->deltatv);
	}
	return e;
}
