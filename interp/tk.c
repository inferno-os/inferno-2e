#include "lib9.h"
#include "interp.h"
#include "isa.h"
#include "runt.h"
#include "image.h"
#include "tk.h"
#include "tkmod.h"
#include "pool.h"
#include "drawif.h"
#include "keyboard.h"

extern	void	tkfreetop(Heap*, int);
static	Type*	fakeTkTop;
static	uchar	TktypeMap[] = Tk_Toplevel_map;

static void
tkmarktop(Type *t, void *vw)
{
	Heap *h;
	TkVar *v;
	TkTop *top;

	markheap(t, vw);
	top = vw;
	for(v = top->vars; v; v = v->link) {
		if(v->type == TkVchan) {
			h = D2H(v->value);
			Setmark(h);
		}
	}
}

void
tkmodinit(void)
{
	builtinmod("$Tk", Tkmodtab);

	fakeTkTop = dtype(tkfreetop, sizeof(TkTop), TktypeMap, sizeof(TktypeMap));
	fakeTkTop->mark = tkmarktop;

	/*
	 * Adjust option table because compilers can't fold
	 * constants
	 */
	tktextoptfix();

	tksorttable();
}

void
Tk_toplevel(void *a)
{
	Tk *tk;
	Heap *h;
	TkTop *t;
	TkWin *tkw;
	TkCtxt *ctxt;
	static int id;
	Screen *screen;
	F_Tk_toplevel *f = a;
	void *r;

	r = *f->ret;
	*f->ret = H;
	destroy(r);
	screen = checkscreen(f->screen);

	h = heapz(fakeTkTop);
	t = H2D(TkTop*, h);
	t->id = id++;
	t->di = H;
	poolimmutable(h);

	t->screen = screen;
	t->dscreen = f->screen;
	D2H(f->screen)->ref++;

	tk = tknewobj(t, TKframe, sizeof(Tk)+sizeof(TkWin));
	if(tk == nil) {
		destroy(t);
		return;
	}

	tk->act.x = 0;
	tk->act.y = 0;
	tk->act.width = 1;
	tk->act.height = 1;
	tk->flag |= Tkwindow;

	tktopopt(tk, string2c(f->arg));
	
	tk->geom = tkmoveresize;
	tk->name = tkmkname(".");
	if(tk->name == nil) {
		tkfreeobj(tk);
		destroy(t);
		return;
	}

	ctxt = tkattachctxt(screen->display);
	if(ctxt == nil) {
		tkfreeobj(tk);
		destroy(t);
		return;
	}
	t->ctxt = ctxt;
	t->link = ctxt->tkwindows;
	ctxt->tkwindows = t;

	tkw = TKobj(TkWin, tk);
	tkw->next = t->windows;
	t->windows = tk;
	t->root = tk;
	Setmark(h);
	poolmutable(h);
	*f->ret = (Tk_Toplevel*)t;

	tkmap(tk);
}

void
Tk_windows(void *a)
{
	TkTop *t;
	TkCtxt *c;
	Screen *s;
	List **h, *nl;
	F_Tk_windows *f = a;

	destroy(*f->ret);
	*f->ret = H;

	s = checkscreen(f->screen);
	c = tkscrn2ctxt(s);
	if(c == nil)
		return;

	h = f->ret;
	for(t = c->tkwindows; t != nil; t = t->link) {
		nl = cons(IBY2WD, h);
		nl->tail = H;
		nl->t = &Tptr;
		Tptr.ref++;
		*(TkTop**)nl->data = t;
		D2H(t)->ref++;
		h = &nl->tail;
	}
}

void
Tk_intop(void *a)
{
	Tk *tk;
	TkTop *t;
	TkCtxt *c;
	Screen *s;
	F_Tk_intop *f = a;
	void *r;

	r = *f->ret;
	*f->ret = H;
	destroy(r);

	s = checkscreen(f->screen);
	c = tkscrn2ctxt(s);
	if(c == nil)
		return;

	tk = tkfindfocus(c, f->x, f->y);
	if(tk == nil)
		return;

	t = tk->env->top;
	D2H(t)->ref++;
	*f->ret = (Tk_Toplevel*)t;
}

void
Tk_cmd(void *a)
{
	TkTop *t;
	char *val, *e;
	F_Tk_cmd *f = a;

	t = (TkTop*)f->t;
	if(t == H || D2H(t)->t != fakeTkTop) {
		retstr(TkNotop, f->ret);
		return;
	}
	val = nil;
	e = tkexec(t, string2c(f->arg), &val);
	retstr(e == nil ? val : e, f->ret);
	free(val);
}

int
tkdescendant(Tk *p, Tk *c)
{
	int n;

	if(c == nil || p->env->top != c->env->top)
		return 0;

	if (p->name != nil) {
 	n = strlen(p->name->name);
	if((c->name != nil) && (strncmp(p->name->name, c->name->name, n) == 0))
		return 1;
	}

	return 0;
}

void
tkenterleave(TkCtxt *c, int x, int y)
{
	Tk *tk;
	TkTop *t;
	TkMouse m;

	tk = tkfindfocus(c, x, y);
	if(tk == c->tkMfocus)
		return;

	m.x = x;
	m.y = y;
	if(c->tkMfocus != nil) {
		t = c->tkMfocus->env->top;
		tkdeliver(c->tkMfocus, TkLeave, &m);
		tkupdate(t);
		c->tkMfocus = nil;
	}
	if(c->tkMgrab != nil && tkdescendant(c->tkMgrab, tk) == 0)
		return;

	c->tkMfocus = tk;
	tkdeliver(tk, TkEnter, &m);
}

void
Tk_mouse(void *a)
{
	Tk *tk;
	TkMouse m;
	TkCtxt *c;
	Screen *s;
	int d, dtype, etype;
	F_Tk_mouse *f = a;

	s = checkscreen(f->screen);
	c = tkscrn2ctxt(s);
	if(c == nil)
		return;

	if(c->tkmstate.x != f->x || c->tkmstate.y != f->y) {
		tkenterleave(c, f->x, f->y);
		c->tkmstate.x = f->x;
		c->tkmstate.y = f->y;
	}

	tk = c->tkMfocus;
	if(c->tkMgrab != nil && tkdescendant(c->tkMgrab, tk) == 0)
		tk = c->tkMgrab;
	if(tk == nil)
		return;

	m.x = f->x;
	m.y = f->y;
	m.b = f->button & 7;		/* Just the buttons */
	etype = 0;
	dtype = 0;
	if(f->button & (1<<4))		/* Double */
		dtype = TkDouble;

	d = c->tkmstate.b ^ m.b;
	if(1 & d) {
		etype = TkButton1R;
		if(m.b & 1)
			etype = TkButton1P|dtype;
		tkdeliver(tk, etype, &m);
	}
	if(2 & d) {
		etype = TkButton2R;
		if(m.b & 2)
			etype = TkButton2P|dtype;
		tkdeliver(tk, etype, &m);
	}
	if(4 & d) {
		etype = TkButton3R;
		if(m.b & 4)
			etype = TkButton3P|dtype;
		tkdeliver(tk, etype, &m);
	}
	if(etype == 0) {
		etype = TkMotion;
		if(m.b & 1)
			etype |= TkButton1P|dtype;
		if(m.b & 2)
			etype |= TkButton2P|dtype;
		if(m.b & 4)
			etype |= TkButton3P|dtype;
		tkdeliver(tk, etype, &m);
	}
	c->tkmstate.b = m.b;
	tkupdate(tk->env->top);
}


void
Tk_keyboard(void *a)
{
	TkTop *t;
	Tk *tk;
	TkMenubut *tkb;
	Screen *s;
	TkCtxt *c;
	F_Tk_keyboard *f = a;
	int k,l;
	char *p;

	s = checkscreen(f->screen);
	c = tkscrn2ctxt(s);
	if(c == nil || c->tkKgrab == nil)
		return;

	t = c->tkKgrab->env->top;
	
	/* The following code is a hackish way of allowing menu speedkeys
		from anywhere within a toplevel Tk frame. */

	/* See if the key is META+key */
	if (f->key >= APP+32 && f->key < APP+256) {
		k = f->key - APP;
		if (k >= 'a' && k <= 'z')
			k += 'A'-'a';		/* Force uppercase comparison */

		/* Look through all the menu buttons under the toplevel
			and see if the key matches the underline for any */
		for (tk = t->root; tk; tk = tk->siblings) {
			if(tk->type != TKmenubutton || (tk->flag & Tkdisabled))
				continue;
			tkb = TKobj(TkMenubut, tk);
			p = tkb->lab.text;
			if(tkb->lab.ul >= 0 && p != nil &&
						tkb->lab.ul < strlen(p)) {
				l = (int) p[tkb->lab.ul];
				if (l >= 'a' && l <= 'z')
					l += 'A'-'a';	/* Force uppercase
								comparison */
				if (k == l) {
					tkMBpress(tk, nil, nil);
					tkupdate(t);
					break;
				}
			}
		}
	}
	else {
		tkdeliver(c->tkKgrab, TkKey|TKKEY(f->key), nil);
		tkupdate(t);
	}
}

TkVar*
tkmkvar(TkTop *t, char *name, int type)
{
	TkVar *v;

	for(v = t->vars; v; v = v->link)
		if(strcmp(v->name, name) == 0)
			return v;

	if(type == 0)
		return nil;

	v = malloc(sizeof(TkVar)+strlen(name)+1);
	if(v == nil)
		return nil;
	strcpy(v->name, name);
	v->link = t->vars;
	t->vars = v;
	v->type = type;
	v->value = nil;
	if(type == TkVchan)
		v->value = H;
	return v;
}

void
tkfreevar(TkTop *t, char *name, int swept)
{
	TkVar **l, *p;

	if(name == nil)
		return;
	l = &t->vars;
	for(p = *l; p != nil; p = p->link) {
		if(strcmp(p->name, name) == 0) {
			*l = p->link;
			switch(p->type) {
			default:
				free(p->value);
				break;
			case TkVchan:
				if(!swept)
					destroy(p->value);
				break;
			}
			free(p);
			return;
		}
		l = &p->link;
	}
}

void
Tk_namechan(void *a)
{
	Heap *h;
	TkVar *v;
	TkTop *t;
	char *name;
	F_Tk_namechan *f = a;

	t = (TkTop*)f->t;
	if(t == H || D2H(t)->t != fakeTkTop) {
		retstr(TkNotop, f->ret);
		return;
	}
	if(f->c == H) {
		retstr("nil channel", f->ret);
		return;
	}
	name = string2c(f->n);
	if(name[0] == '\0') {
		retstr(TkBadvl, f->ret);
		return;
	}

	v = tkmkvar(t, name, TkVchan);
	if(v == nil) {
		retstr(TkNomem, f->ret);
		return;
	}
	if(v->type != TkVchan) {
		retstr(TkNotvt, f->ret);
		return;
	}
	destroy(v->value);
	v->value = f->c;
	h = D2H(v->value);
	h->ref++;
	Setmark(h);
	// poolimmutable((void *)h);
	retstr("", f->ret);
}

void
tkreplimg(TkTop *t, Draw_Image *f, Image **ximg)
{
	int dx, dy;
	Display *d;
	Image *cimg, *new;

	cimg = lookupimage(f);
	d = t->screen->display;
	if(cimg == nil || cimg->screen != nil || cimg->display != d)
		return;

	dx = cimg->r.max.x-cimg->r.min.x;
	dy = cimg->r.max.y-cimg->r.min.y;
	new = allocimage(d, Rect(0, 0, dx, dy), cimg->ldepth, 1, 0);
	if(new == nil)
		return;
	draw(new, new->r, cimg, d->ones, cimg->r.min);
	if(*ximg != nil)
		freeimage(*ximg);
	*ximg = new;
}

void
Tk_imageput(void *a)
{
	TkTop *t;
	TkImg *i;
	int locked;
	Display *d;
	F_Tk_imageput *f;
	void *r;

	f = a;

	r = *f->ret;
	*f->ret = H;
	destroy(r);

	t = (TkTop*)f->t;
	if(t == H || D2H(t)->t != fakeTkTop) {
		retstr(TkNotop, f->ret);
		return;
	}

	i = tkname2img(t, string2c(f->name));
	if(i == nil) {
		retstr(TkBadvl, f->ret);
		return;
	}

	d = t->screen->display;
	locked = lockdisplay(d, 0);
	if(f->i != H)
		tkreplimg(t, f->i, &i->fgimg);
	if(f->m != H)
		tkreplimg(t, f->m, &i->maskimg);
	if(locked)
		unlockdisplay(d);

	tksizeimage(t->root, i);
}

Draw_Image*
tkimgcopy(TkTop *t, Image *cimg)
{
	Image *new;
	Display *dp;
	Draw_Image *i;
	Draw_Screen *d;

	if(cimg == nil)
		return H;

	dp = t->screen->display;
	new = allocimage(dp, cimg->r, cimg->ldepth, 1, 0);
	if(new == nil)
		return H;

	draw(new, new->r, cimg, dp->ones, cimg->r.min);

	d = (Draw_Screen*)t->dscreen;
	i = mkdrawimage(new, H, d->display, nil);
	if(i == H)
		freeimage(new);

	return i;
}

void
Tk_imageget(void *a)
{
	Tk *tk;
	char *n;
	TkImg *i;
	TkTop *t;
	int locked;
	Display *d;
	TkLabel *tkl;
	F_Tk_imageget *f;
	void *r;

	f = a;

	r = f->ret->t0;
	f->ret->t0 = H;
	destroy(r);
	r = f->ret->t1;
	f->ret->t1 = H;
	destroy(r);
	r = f->ret->t2;
	f->ret->t2 = H;
	destroy(r);

	t = (TkTop*)f->t;
	if(t == H || D2H(t)->t != fakeTkTop) {
		retstr(TkNotop, &f->ret->t2);
		return;
	}
	d = t->screen->display;
	n = string2c(f->name);
	i = tkname2img(t, n);
	if(i == nil) {
		tk = tklook(t, n, 0);
		if(tk == nil)
			goto badval;
		switch(tk->type) {
		badval:
		default:
			retstr(TkBadvl, &f->ret->t2);
			return;
		case TKlabel:
		case TKbutton:
		case TKmenubutton:
		case TKcheckbutton:
			tkl = TKobj(TkLabel, tk);
			if(tkl->img != nil) {
				i = tkl->img;
				goto isimage;
			}
			locked = lockdisplay(d, 0);
			f->ret->t0 = tkimgcopy(t, tkl->bitmap);
			if(locked)
				unlockdisplay(d);
			return;
		}
	}
isimage:
	i->ref++;
	locked = lockdisplay(d, 0);
	f->ret->t0 = tkimgcopy(t, i->fgimg);
	f->ret->t1 = tkimgcopy(t, i->maskimg);
	if(locked)
		unlockdisplay(d);
	tkimgput(i);
}

void
tkfreetop(Heap *h, int swept)
{
	TkTop *t;
	Tk *f, *link;
	TkImg *i, *nexti;
	TkVar *v, *nextv;
	int wgtype;
	void *r;

	t = H2D(TkTop*, h);
	if(swept)
		t->di = H;	/* gc gets it, anyway */

	t->windows = nil;

	for(f = t->root; f; f = f->siblings) {
		f->flag |= Tkdestroy;
		if(f->destroyed != nil)
			f->destroyed(f);
	}

	if (t->ctxt)
		tkdetachctxt(t);

	for(f = t->root; f; f = link) {
		link = f->siblings;
		if(swept)
			f->flag |= Tkswept;
		tkfreeobj(f);
	}

	for(v = t->vars; v; v = nextv) {
		nextv = v->link;
		switch(v->type) {
		default:
			free(v->value);
			break;
		case TkVchan:
			if(!swept)
				destroy(v->value);
			break;
		}
		free(v);
	}

	for(i = t->imgs; i; i = nexti) {
		if(i->ref != 1)
			abort();
		nexti = i->link;
		tkimgput(i);
	}

	for(wgtype = 0; wgtype < TKwidgets; wgtype++)
		if(t->binds[wgtype])
			tkfreebind(t->binds[wgtype]);

	if(!swept) {
		r = t->di;
		t->di = H;
		destroy(r);
		r = t->dscreen;
		t->dscreen = H;
		destroy(r);
	}
}

void
tktopimageptr(TkTop *t, Image *i)
{
	Draw_Screen *d;
	Draw_Image *di;

	di = t->di;
	t->di = H;
	destroy(di);
	if(i == nil)
		return;
	d = (Draw_Screen*)t->dscreen;
	t->di = mkdrawimage(i, d, d->display, nil);
}

int
tktolimbo(TkVar *var, char *msg)
{
	void *ptrs[1];
	int r;

	ptrs[0] = H;
	retstr(msg, (String**) &ptrs[0]);
	r = csendq((Channel *)var, ptrs, &Tptr, TkMaxmsgs);
	return r;
}
