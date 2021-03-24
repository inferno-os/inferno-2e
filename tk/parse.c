#include "lib9.h"
#include "kernel.h"
#include "image.h"
#include "tk.h"

#define	O(t, e)		((long)(&((t*)0)->e))

static char* pdist(TkTop*, TkOption*, void*, char**, char*, char*);
static char* pstab(TkTop*, TkOption*, void*, char**, char*, char*);
static char* ptext(TkTop*, TkOption*, void*, char**, char*, char*);
static char* pwinp(TkTop*, TkOption*, void*, char**, char*, char*);
static char* pbmap(TkTop*, TkOption*, void*, char**, char*, char*);
static char* pbool(TkTop*, TkOption*, void*, char**, char*, char*);
static char* pfont(TkTop*, TkOption*, void*, char**, char*, char*);
static char* pfrac(TkTop*, TkOption*, void*, char**, char*, char*);
static char* pctag(TkTop*, TkOption*, void*, char**, char*, char*);
static char* ptabs(TkTop*, TkOption*, void*, char**, char*, char*);
static char* pcolr(TkTop*, TkOption*, void*, char**, char*, char*);
static char* pimag(TkTop*, TkOption*, void*, char**, char*, char*);
static char* psize(TkTop*, TkOption*, void*, char**, char*, char*);
static char* pevim(TkTop*, TkOption*, void*, char**, char*, char*);

static char* (*oparse[])(TkTop*, TkOption*, void*, char**, char*, char*) =
{
	/* OPTdist */	pdist,
	/* OPTstab */	pstab,
	/* OPTtext */	ptext,
	/* OPTwinp */	pwinp,
	/* OPTflag */	pstab,
	/* OPTbmap */	pbmap,
	/* OPTbool */	pbool,
	/* OPTfont */	pfont,
	/* OPTfrac */	pfrac,
	/* OPTctag */	pctag,
	/* OPTtabs */	ptabs,
	/* OPTcolr */	pcolr,
	/* OPTimag */	pimag,
	/* OPTsize */	psize,
	/* OPTevim */	pevim,
};

char*
tkskip(char *s, char *bl)
{
	char *p;

	while(*s) {
		for(p = bl; *p; p++)
			if(*p == *s)
				break;
		if(*p == '\0')
			return s;	
		s++;
	}
	return s;
}

/* XXX - Tad: error propagation? */
char*
tkword(TkTop *t, char *str, char *buf, char *ebuf)
{
	int c, lev;
	char *val, *e, *p, *cmd;

	/*
	 * ebuf is one beyond last byte in buf; leave room for nul byte in
	 * all cases.
	 */
	--ebuf;

	str = tkskip(str, " \t");
	lev = 1;
	switch(*str) {
	case '{':
		/* XXX - DBK: According to Ousterhout (p.37), while back=
		 * slashed braces don't count toward finding the matching
		 * closing braces, the backslashes should not be removed.
		 * Presumably this also applies to other backslashed
		 * characters: the backslash should not be removed.
		 */
		str++;
		while(*str && buf < ebuf) {
			c = *str++;
			if(c == '\\') {
				if(*str == '}' || *str == '{' || *str == '\\')
					c = *str++;
			}
			else
			if(c == '}') {
				lev--;
				if(lev == 0)
					break;
			}
			else
			if(c == '{')
				lev++;
			*buf++ = c;
		}
		break;
	case '[':
		/* XXX - DBK: According to Ousterhout (p. 33) command
		 * substitution may occur anywhere within a word, not
		 * only (as here) at the beginning.
		 */
		cmd = malloc(strlen(str));
		if ( cmd == nil ) {
			buf[0] = '\0';	/* DBK - Why not an error message? */
			return str;
		}
		p = cmd;
		str++;
		while(*str) {
			c = *str++;
			if(c == '\\') {
				if(*str == ']' || *str == '[' || *str == '\\')
					c = *str++;
			}
			else
			if(c == ']') {
				lev--;
				if(lev == 0)
					break;
			}
			else
			if(c == '[')
				lev++;
			*p++ = c;
		}
		*p = '\0';
		val = nil;
		e = tkexec(t, cmd, &val);
		free(cmd);
		 /* XXX - Tad: is this appropriate behavior?
		  *	      Am I sure that the error doesn't need to be
		  *	      propagated back to the caller?
		  */
		if(e == nil && val != nil) {
			strncpy(buf, val, ebuf-buf);
			buf = ebuf;
			free(val);
		}
		break;
	case '\'':
		str++;
		while(*str && buf < ebuf)
			*buf++ = *str++;
		break;
	default:
		/* XXX - DBK: See comment above about command substitution.
		 * Also, any backslashed character should be replaced by
		 * itself (e.g. to put a space, tab, or [ into a word.
		 * We assume that the C compiler has already done the
		 * standard ANSI C substitutions.  (But should we?)
		 */
		while(*str && *str != ' ' && *str != '\t' && buf < ebuf)
			*buf++ = *str++;
	}
	*buf = '\0';
	return str;
}

static TkOption*
Getopt(TkOption *o, char *buf)
{
	while(o->o != nil) {
		if(strcmp(buf, o->o) == 0)
			return o;
		o++;
	}
	return nil;
}

TkName*
tkmkname(char *name)
{
	TkName *n;

	n = malloc(sizeof(struct TkName)+strlen(name));
	if(n == nil)
		return nil;
	strcpy(n->name, name);
	n->link = nil;
	n->obj = nil;
	return n;
}

char*
tkparse(TkTop *t, char *str, TkOptab *ot, TkName **nl)
{
	int l;
	TkOptab *ft;
	TkOption *o;
	TkName *f, *n;
	char *e, *buf, *ebuf;

	l = strlen(str);
	buf = malloc(l);
	if(buf == 0)
		return TkNomem;
	ebuf = buf + l;

	e = nil;
	while(e == nil) {
		str = tkword(t, str, buf, ebuf);
		switch(*buf) {
		case '\0':
			goto done;
		case '-':
			for(ft = ot; ft->ptr; ft++) {
				o = Getopt(ft->optab, buf+1);
				if(o != nil) {
					e = oparse[o->type](t, o, ft->ptr, &str, buf, ebuf);
					break;
				}
			}
			if(ft->ptr == nil)
				e = TkBadop;
			break;
		default:
			if(nl == nil) {
				e = TkBadop;
				break;
			}
			n = tkmkname(buf);
			if(n == nil) {
				e = TkNomem;
				break;
			}
			if(*nl == nil)
				*nl = n;
			else {
				for(f = *nl; f->link; f = f->link)
					;
				f->link = n;
			}
		}		
	}

	if(e != nil && nl != nil)
		tkfreename(*nl);
done:
	free(buf);
	return e;
}

char*
tkconflist(TkOptab *ot, char **val)
{
	TkOption *o;
	char *f, *e;

	f = "-%s";
	while(ot->ptr != nil) {
		o = ot->optab;
		while(o->o != nil) {
			e = tkvalue(val, f, o->o);
			if(e != nil)
				return e;
			f = " -%s";
			o++;
		}
		ot++;
	}
	return nil;
}

char*
tkgencget(TkOptab *ft, char *arg, char **val)
{
	Tk *w;
	char *c;
	Point g;
	TkEnv *e;
	TkStab *s;
	TkOption *o;
	int wh, con, i, n, *v;
	char *r, *buf, *fmt;

	buf = mallocz(Tkmaxitem, 0);
	if(buf == nil)
		return TkNomem;

	tkitem(buf, arg);
	r = buf;
	if(*r == '-')
		r++;
	if(strcmp(r, "actx") == 0 || strcmp(r, "acty") == 0) {
		w = ft->ptr;
		g = tkposn(w);
		n = g.y;
		if(r[3] == 'x')
			n = g.x;
		free(buf);
		return tkvalue(val, "%d", n);
	}
	o = nil;
	while(ft->ptr) {
		o = Getopt(ft->optab, r);
		if(o != nil)
			break;
		ft++;
	}
	if(o == nil) {
		free(buf);
		return TkBadop;
	}

	switch(o->type) {
	default:
		free(buf);
		return TkBadop;
	case OPTdist:
		free(buf);
		return tkvalue(val, "%d", OPTION(ft->ptr, int, o->offset));
	case OPTsize:
		w = ft->ptr;
		if(strcmp(r, "width") == 0)
			wh = w->req.width;
		else
			wh = w->req.height;
		free(buf);
		return tkvalue(val, "%d", wh);
	case OPTtext:
		c = OPTION(ft->ptr, char*, o->offset);
		if(c == nil)
			c = "";
		free(buf);
		return tkvalue(val, "%s", c);
	case OPTwinp:
		w = OPTION(ft->ptr, Tk*, o->offset);
		if(w == nil || w->name == nil)
			c = "";
		else
			c = w->name->name;
		free(buf);
		return tkvalue(val, "%s", c);
	case OPTstab:
		s = o->aux;
		c = "";
		con = OPTION(ft->ptr, int, o->offset);
		while(s->val) {
			if(con == s->con) {
				c = s->val;
				break;
			}
			s++;
		}
		free(buf);
		return tkvalue(val, "%s", c);
	case OPTflag:
		s = o->aux;
		c = "";
		con = OPTION(ft->ptr, int, o->offset);
		while(s->val) {
			if(s->con == 0)
				c = s->val;
			if(con & s->con) {
				c = s->val;
				break;
			}
			s++;
		}
		free(buf);
		return tkvalue(val, "%s", c);
	case OPTfont:
		e = OPTION(ft->ptr, TkEnv*, o->offset);
		free(buf);
		return tkvalue(val, "%s", e->font->name);
	case OPTcolr:
		e = OPTION(ft->ptr, TkEnv*, o->offset);
		i = AUXI(o->aux);
		free(buf);
		return tkvalue(val, "#%.6x", cmap2rgb(e->colors[i]));
	case OPTfrac:
		v = &OPTION(ft->ptr, int, o->offset);
		n = (int)o->aux;
		if(n == 0)
			n = 1;
		fmt = "%s";
		for(i = 0; i < n; i++) {
			tkfprint(buf, *v++);
			r = tkvalue(val, fmt, buf);
			if(r != nil) {
				free(buf);
				return r;
			}
			fmt = " %s";
		}
		free(buf);
		return nil;
	}
}

static char*
pdist(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	int d;
	char *p;

	USED(buf);
	USED(ebuf);
	p = tkfrac(t, *str, &d, OPTION(place, TkEnv*, AUXI(o->aux)));
	if(p == nil)
		return TkBadvl;
	*str = p;
	OPTION(place, int, o->offset) = TKF2I(d);
	return nil;
}

static char*
psize(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	Tk *tk;
	char *p;
	int d, off;

	USED(ebuf);
	p = tkfrac(t, *str, &d, OPTION(place, TkEnv*, AUXI(o->aux)));
	if(p == nil)
		return TkBadvl;
	*str = p;

	tk = place;
	if(strcmp(buf+1, "width") == 0) {
		tk->flag |= Tksetwidth;
		off = O(Tk, req.width);
	}
	else {
		tk->flag |= Tksetheight;
		off = O(Tk, req.height);
	}
	OPTION(place, int, off) = TKF2I(d);
	return nil;
}

static char*
pstab(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	char *p;
	int mask;
	TkStab *s, *c;

	p = tkword(t, *str, buf, ebuf);
	if(*buf == '\0')
		return TkOparg;
	*str = p;

	for(s = o->aux; s->val; s++)
		if(strcmp(s->val, buf) == 0)
			break;
	if(s->val == nil)
		return TkBadvl;

	if(o->type == OPTstab) {
		OPTION(place, int, o->offset) = s->con;
		return nil;
	}

	mask = 0;
	for(c = o->aux; c->val; c++)
		mask |= c->con;

	OPTION(place, int, o->offset) &= ~mask;
	OPTION(place, int, o->offset) |= s->con;
	return nil;
}

static char*
ptext(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	char **p;

	*str = tkword(t, *str, buf, ebuf);

	p = &OPTION(place, char*, o->offset);
	if(*p != nil)
		free(*p);
	if(buf[0] == '\0')
		*p = nil;
	else {
		*p = strdup(buf);
		if(*p == nil)
			return TkNomem;
	}
	return nil;
}

static char*
pimag(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	int locked;
	Display *d;
	TkImg **p, *i;

	i = nil;
	p = &OPTION(place, TkImg*, o->offset);
	*str = tkword(t, *str, buf, ebuf);
	if(*buf != '\0') {
		i = tkname2img(t, buf);
		if(i == nil)
			return TkBadvl;
		i->ref++;
	}

	if(*p != nil) {
		d = t->screen->display;
		locked = lockdisplay(d, 0);
		tkimgput(*p);
		if(locked)
			unlockdisplay(d);
	}
	*p = i;
	return nil;
}

static char*
pevim(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	TkImg *i;
	TkEnv *e;
	char *err;
	Display *d;
	Image *cimg;
	int locked, col;

	col = AUXI(o->aux);
	d = t->screen->display;
	i = nil;

	*str = tkword(t, *str, buf, ebuf);
	if(buf[0] != '\0') {
		i = tkname2img(t, buf);
		if(i == nil || i->fgimg == nil)
			return TkBadvl;
	}

	e = tkdupenv(&OPTION(place, TkEnv*, o->offset));
	if(e == nil)
		return TkNomem;

	err = nil;
	locked = lockdisplay(d, 0);
	if(e->evim[col] != nil) {
		freeimage(e->evim[col]);
		e->evim[col] = nil;
	}
	if(i != nil) {
		cimg = allocimage(d, i->fgimg->r, i->fgimg->ldepth, 1, e->colors[TkCbackgnd]);
		if(cimg == nil) {
			err = TkNomem;
			goto out;
		}
		draw(cimg, cimg->r, i->fgimg, cimg->display->ones, tkzp);
		e->evim[col] = cimg;
	}
out:
	if(locked)
		unlockdisplay(d);
	return err;
}

static char*
pbmap(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	Display *d;
	Image *i, **p;
	int locked, fd;
	char *c;

	p = &OPTION(place, Image*, o->offset);

	d = t->screen->display;
	*str = tkword(t, *str, buf, ebuf);
	if(*buf == '\0' || *buf == '-') {
		if(*p != nil) {
			locked = lockdisplay(d, 0);
			freeimage(*p);
			if(locked)
				unlockdisplay(d);
			*p = nil;
		}
		return nil;
	}

	if(buf[0] == '@')
		i = display_open(d, buf+1);
	else
	if(buf[0] == '<') {
		buf++;
		fd = strtoul(buf, &c, 0);
		if(c == buf) {
			return TkBadvl;
		}
		i = readimage(d, fd, 1);
	}
	else {
		char *file;

		file = mallocz(Tkmaxitem, 0);
		if(file == nil)
			return TkNomem;

		snprint(file, Tkmaxitem, "/icons/tk/%s", buf);
		i = display_open(d, file);
		free(file);
	}
	if(i == nil)
		return TkBadbm;

	if(*p != nil) {
		locked = lockdisplay(d, 0);
		freeimage(*p);
		if(locked)
			unlockdisplay(d);
	}
	*p = i;
	return nil;
}

static char*
pfont(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	TkEnv *e;
	Display *d;
	int locked;
	Font *font;

	*str = tkword(t, *str, buf, ebuf);
	if(*buf == '\0')
		return TkOparg;

	d = t->screen->display;
	font = font_open(d, buf);
	if(font == nil)
		return TkBadft;

	e = tkdupenv(&OPTION(place, TkEnv*, o->offset));
	if(e == nil) {
		freefont(font);
		return TkNomem;
	}
	if(e->font)
		font_close(e->font);
	e->font = font;

	locked = lockdisplay(d, 0);
	e->wzero = stringwidth(font, "0");
	if ( e->wzero <= 0 )
		e->wzero = e->font->height / 2;
	if(locked)
		unlockdisplay(d);

	return nil;
}

static int
hex(int c)
{
	if(c >= 'a')
		c -= 'a'-'A';
	if(c >= 'A')
		c = 10 + (c - 'A');
	else
		c -= '0';
	return c;
}

static void
changecol(TkEnv *e, int col, int rgb, int delta)
{
	int val, R, G, B;

	R = (rgb>>16) + delta;
	G = ((rgb>>8) & 0xFF) + delta;
	B = (rgb & 0xFF) + delta;
	val = rgb2cmap(R, G, B);

	e->colors[col] = val;
	e->set |= (1<<col);
}

static char*
pcolr(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	TkEnv *e;
	char *err;
	TkOption op;
	uchar R, G, B;
	int rgb, color;

	*str = tkskip(*str, " \t");
	switch(**str) {
	case '=':
		*str = tkword(t, *str, buf, ebuf);
		if(*buf == '\0')
			return TkOparg;
		rgb = cmap2rgb(atoi(buf+1));
		break;
	case '#':
		*str = tkword(t, *str, buf, ebuf);
		if(*buf == '\0')
			return TkOparg;
		switch(strlen(buf)) {
		case 4:			/* #RGB */
			R = hex(buf[1]);
			G = hex(buf[2]);
			B = hex(buf[3]);
			rgb = (R<<20) | (G<<12) | (B<<4);
			break;
		case 7:			/* #RRGGBB */
			R = (hex(buf[1])<<4)|(hex(buf[2]));
			G = (hex(buf[3])<<4)|(hex(buf[4]));
			B = (hex(buf[5])<<4)|(hex(buf[6]));
			rgb = (R<<16) | (G<<8) | (B);
			break;
		default:
			return TkBadvl;
		}
		break;
	default:
		op.type = OPTstab;
		op.offset = 0;
		op.aux = tkcolortab;
		err = pstab(t, &op, &rgb, str, buf, ebuf);
		if(err != nil)
			return err;
	}	

	e = tkdupenv(&OPTION(place, TkEnv*, o->offset));
	if(e == nil)
		return TkNomem;

	color = AUXI(o->aux);
	changecol(e, color, rgb, 0);
	if(color == TkCbackgnd || color == TkCselectbgnd) {
		changecol(e, color+1, rgb, Tkshdelta);
		changecol(e, color+2, rgb, -Tkshdelta);
	}

	return nil;
}

static char*
pbool(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	USED(buf);
	USED(ebuf);
	USED(str);
	USED(t);
	OPTION(place, int, o->offset) = 1;
	return nil;
}

static char*
pwinp(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	Tk *f;
	char *p;

	p = tkword(t, *str, buf, ebuf);
	if(*buf == '\0')
		return TkOparg;
	*str = p;
	
	f = tklook(t, buf, 0);
	if(f == nil)
		return TkBadwp;

	OPTION(place, Tk*, o->offset) = f;
	return nil;
}

static char*
pctag(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	char *p;
	TkName *n, *l;

	*str = tkword(t, *str, buf, ebuf);

	l = nil;
	p = buf;
	while(*p) {
		p = tkskip(p, " \t");
		buf = p;
		while(*p && *p != ' ' && *p != '\t')
			p++;
		if(*p != '\0')
			*p++ = '\0';

		if(p == buf || buf[0] >= '0' && buf[0] <= '9') {
			tkfreename(l);
			return TkBadtg;
		}
		n = tkmkname(buf);
		if(n == nil) {
			tkfreename(l);
			return TkNomem;
		}
		n->link = l;
		l = n;
	}
	tkfreename(OPTION(place, TkName*, o->offset));
	OPTION(place, TkName*, o->offset) = l;
	return nil;
}

static char*
pfrac(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	char *p;
	int i, n, d, *v;
	char *item;

	*str = tkword(t, *str, buf, ebuf);

	v = &OPTION(place, int, o->offset);
	n = (int)o->aux;
	if(n == 0)
		n = 1;
	p = buf;
	item = mallocz(Tkmaxitem, 0);
	if(item == nil)
		return TkNomem;
	for(i = 0; i < n; i++) {
		p = tkword(t, p, item, item+Tkmaxitem);
		if(*item == '\0') {
			free(item);
			return TkOparg;
		}
		if(tkfrac(t, item, &d, nil) == nil) {
			free(item);
			return TkBadvl;
		}
		*v++ = d;
	}
	free(item);
	return nil;
}

typedef struct Tabspec {
	int	dist;
	int	just;
	TkEnv	*env;
} Tabspec;

static char*
ptabs(TkTop *t, TkOption *o, void *place, char **str, char *buf, char *ebuf)
{
	char *e, *p, *eibuf;
	TkOption opd, opj;
	Tabspec tspec;
	TkTtabstop *tabfirst, *tab, *tabprev;
	char *ibuf;

	ibuf = mallocz(Tkmaxitem, 0);
	if(ibuf == nil)
		return TkNomem;
	eibuf = ibuf + Tkmaxitem;
	tspec.env = OPTION(place, TkEnv*, AUXI(o->aux));
	opd.offset = O(Tabspec, dist);
	opd.aux = IAUX(O(Tabspec, env));
	opj.offset = O(Tabspec, dist);
	opj.aux = tktabjust;
	tabprev = nil;
	tabfirst = nil;

	p = tkword(t, *str, buf, ebuf);
	if(*buf == '\0') {
		free(ibuf);
		return TkOparg;
	}
	*str = p;

	p = buf;
	while(*p != '\0') {
		e = pdist(t, &opd, &tspec, &p, ibuf, eibuf);
		if(e != nil) {
			free(ibuf);
			return e;
		}

		e = pstab(t, &opj, &tspec, &p, ibuf, eibuf);
		if(e != nil)
			tspec.just = Tkleft;

		tab = malloc(sizeof(TkTtabstop));
		if(tab == nil) {
			free(ibuf);
			return TkNomem;
		}

		tab->pos = tspec.dist;
		tab->justify = tspec.just;
		tab->next = nil;
		if(tabfirst == nil)
			tabfirst = tab;
		else
			tabprev->next = tab;
		tabprev = tab;
	}
	free(ibuf);

	tab = OPTION(place, TkTtabstop*, o->offset);
	if(tab != nil)
		free(tab);
	OPTION(place, TkTtabstop*, o->offset) = tabfirst;
	return nil;
}
