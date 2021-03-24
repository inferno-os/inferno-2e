#pragma src "/usr/inferno/image"

typedef struct	Cachefont Cachefont;
typedef struct	Cacheinfo Cacheinfo;
typedef struct	Cachesubf Cachesubf;
typedef struct	Display Display;
typedef struct	Font Font;
typedef struct	Fontchar Fontchar;
typedef struct	Image Image;
typedef struct	Point Point;
typedef struct	Rectangle Rectangle;
typedef struct	Refreshq Refreshq;
typedef struct	Screen Screen;
typedef struct	Subfont Subfont;

enum
{
	MaxImageSize	= 4194304,	/* max memory in bytes for image */
	MaxImageWidth	= 16384,	/* max image width in cols */
	MaxImageHeight	= 16384		/* max image height in rows */
};

enum
{
	Displaybufsize	= 8000
};

enum
{
	/* refresh methods */
	Refbackup	= 0,
	Reflocal	= 1,
	Refremote	= 2
};

enum
{
	/* line ends */
	Endsquare	= 0,
	Enddisc		= 1,
	Endarrow	= 2,
	Endmask		= 0x1F
};

#define	ARROW(a, b, c)	(Endarrow|((a)<<5)|((b)<<14)|((c)<<23))

struct	Point
{
	int	x;
	int	y;
};

struct Rectangle
{
	Point	min;
	Point	max;
};

typedef void	(*Reffn)(Image*, Rectangle, void*);

struct Screen
{
	Display	*display;	/* display holding data */
	int	id;		/* id of system-held Screen */
	Image	*image;		/* unused; for reference only */
	Image	*fill;		/* unused; for reference only */
};

struct Refreshq
{
	Reffn		reffn;
	void		*refptr;
	Rectangle	r;
	Refreshq	*next;
};

struct Display
{
	int		dirno;
	void		*datachan;
	void		*refchan;
	int		imageid;
	int		local;
	ulong		dataqid;
	Image		*ones;
	Image		*zeros;
	Image		*image;
	uchar		buf[Displaybufsize+1];	/* +1 for flush message */
	uchar		*bufp;
	Font		*defaultfont;
	Subfont		*defaultsubfont;
	Image		*windows;
	void		*qlock;
	Refreshq	*refhead;
	Refreshq	*reftail;
	void		*limbo;		/* used by limbo interpreter */
};

struct Image
{
	Display		*display;	/* display holding data */
	int		id;		/* id of system-held Image */
	Rectangle	r;		/* rectangle in data area, local coords */
	Rectangle 	clipr;		/* clipping region */
	int		ldepth;		/* log base 2 of number of bits per pixel */
	int		repl;		/* whether data area replicates to tile the plane */
	Screen		*screen;	/* 0 if not a window */
	Image		*next;
	Reffn		reffn;
	void		*refptr;
};

/*
 * Subfonts
 *
 * given char c, Subfont *f, Fontchar *i, and Point p, one says
 *	i = f->info+c;
 *	draw(b, Rect(p.x+i->left, p.y+i->top,
 *		p.x+i->left+((i+1)->x-i->x), p.y+i->bottom),
 *		color, f->bits, Pt(i->x, i->top));
 *	p.x += i->width;
 * to draw characters in the specified color (itself an Image) in Image b.
 */

struct	Fontchar
{
	int		x;		/* left edge of bits */
	uchar		top;		/* first non-zero scan-line */
	uchar		bottom;		/* last non-zero scan-line + 1 */
	char		left;		/* offset of baseline */
	uchar		width;		/* width of baseline */
};

struct	Subfont
{
	char		*name;
	short		n;		/* number of chars in font */
	uchar		height;		/* height of bitmap */
	char		ascent;		/* top of bitmap to baseline */
	Fontchar 	*info;		/* n+1 character descriptors */
	Image		*bits;		/* of font */
};

enum
{
	/* starting values */
	LOG2NFCACHE =	6,
	NFCACHE =	(1<<LOG2NFCACHE),	/* #chars cached */
	NFLOOK =	5,			/* #chars to scan in cache */
	NFSUBF =	2,			/* #subfonts to cache */
	/* max value */
	MAXFCACHE =	2048+NFLOOK,		/* generous upper limit */
	MAXSUBF =	50,			/* generous upper limit */
	/* deltas */
	DSUBF = 	4,
	/* expiry ages */
	SUBFAGE	=	10000,
	CACHEAGE =	10000
};

struct Cachefont
{
	Rune		min;	/* lowest rune value to be taken from subfont */
	Rune		max;	/* highest rune value+1 to be taken from subfont */
	int		offset;	/* position in subfont of character at min */
	char		*name;			/* stored in font */
	char		*subfontname;		/* to access subfont */
};

struct Cacheinfo
{
	ushort		x;		/* left edge of bits */
	uchar		width;		/* width of baseline */

	/*
	 * additions
	 */
	Rune		value;	/* value of character at this slot in cache */
	ushort		age;
};

struct Cachesubf
{
	ulong		age;	/* for replacement */
	Cachefont	*cf;	/* font info that owns us */
	Subfont		*f;	/* attached subfont */
};

struct Font
{
	char		*name;
	Display		*display;
	short		height;	/* max height of bitmap, interline spacing */
	short		ascent;	/* top of bitmap to baseline */
	int		maxldepth;	/* over all loaded subfonts */
	short		width;	/* widest so far; used in caching only */	
	short		ldepth;	/* of images */
	short		nsub;	/* number of subfonts */
	ulong		age;	/* increasing counter; used for LRU */
	int		ncache;	/* size of cache */
	int		nsubf;	/* size of subfont list */
	Cacheinfo	*cache;
	Cachesubf	*subf;
	Cachefont	**sub;	/* as read from file */
	Image		*cacheimage;
};

#define	Dx(r)	((r).max.x-(r).min.x)
#define	Dy(r)	((r).max.y-(r).min.y)

/*
 * Image management
 */
extern Image*	_allocimage(Display*, Rectangle, int, int, int, int, int);
extern Image*	allocimage(Display*, Rectangle, int, int, int);
extern ulong*	attachscreen(Rectangle*, int*, int*, int*);
extern uchar*	bufimage(Display*, int);
extern int	bytesperline(Rectangle, int);
extern void	closedisplay(Display*);
extern Image*	display_open(Display*, char*);
extern int	flushimage(Display*, int);
extern int	freeimage(Image*);
extern Display*	initdisplay(char*);
extern int	loadimage(Image*, Rectangle, uchar*, int);
extern Image*	readimage(Display*, int, int);
extern Image *	creadimage(Display*, int, int);
extern int	unloadimage(Image*, Rectangle, uchar*, int);
extern int	wordsperline(Rectangle, int);
extern int	writeimage(int, Image*);

/*
 * Windows
 */
extern Screen*	allocscreen(Image*, Image*, int);
extern Image*	allocwindow(Screen*, Rectangle, void (*)(Image*, Rectangle, void*), void*, int);
extern void	bottomnwindows(Image**, int);
extern void	bottomwindow(Image*);
extern void	cursor(Point, Image*);
extern int	freescreen(Screen*);
extern Screen*	publicscreen(Display*, int, int);
extern void	refreshslave(Display*);
extern void	topnwindows(Image**, int);
extern void	topwindow(Image*);
extern void	delrefresh(Image*);
extern void	queuerefresh(Image*, Rectangle, Reffn, void*);
extern int	originwindow(Image*, Point, Point);

/*
 * Geometry
 */
extern Point		Pt(int, int);
extern Rectangle	Rect(int, int, int, int);
extern Rectangle	Rpt(Point, Point);
extern Point		addpt(Point, Point);
extern Point		subpt(Point, Point);
extern Point		divpt(Point, int);
extern Point		mulpt(Point, int);
extern int		eqpt(Point, Point);
extern int		eqrect(Rectangle, Rectangle);
extern Rectangle	insetrect(Rectangle, int);
extern Rectangle	rectaddpt(Rectangle, Point);
extern Rectangle	rectsubpt(Rectangle, Point);
extern Rectangle	canonrect(Rectangle);
extern int		rectXrect(Rectangle, Rectangle);
extern int		rectinrect(Rectangle, Rectangle);
extern void	combinerect(Rectangle*, Rectangle);
extern int		rectclip(Rectangle*, Rectangle);
extern int		ptinrect(Point, Rectangle);
extern void		replclipr(Image*, int, Rectangle);
extern int		drawsetxy(int, int, int);
extern int		rgb2cmap(int, int, int);
extern int		cmap2rgb(int);

/*
 * Graphics
 */
extern void	draw(Image*, Rectangle, Image*, Image*, Point);
extern void	gendraw(Image*, Rectangle, Image*, Point, Image*, Point);
extern void	line(Image*, Point, Point, int, int, int, Image*, Point);
extern void	poly(Image*, Point*, int, int, int, int, Image*, Point);
extern void	fillpoly(Image*, Point*, int, int, Image*, Point);
extern Point	string(Image*, Point, Image*, Point, Font*, char*);
extern Point	stringn(Image*, Point, Image*, Point, Font*, char*, int);
extern Point	runestring(Image*, Point, Image*, Point, Font*, Rune*);
extern Point	runestringn(Image*, Point, Image*, Point, Font*, Rune*, int);
extern Point	_string(Image*, Point, Image*, Point, Font*, char*, Rune*, int, Rectangle);
extern Point	stringsubfont(Image*, Point, Image*, Subfont*, char*);
extern int		bezier(Image*, Point, Point, Point, Point, int, int, int, Image*, Point);
extern int		bezspline(Image*, Point*, int, int, int, int, Image*, Point);
extern int		fillbezier(Image*, Point, Point, Point, Point, int, Image*, Point);
extern int		fillbezspline(Image*, Point*, int, int, Image*, Point);
extern void	ellipse(Image*, Point, int, int, int, Image*, Point);
extern void	fillellipse(Image*, Point, int, int, Image*, Point);
extern void	arc(Image*, Point, int, int, int, Image*, Point, int, int);
extern void	fillarc(Image*, Point, int, int, Image*, Point, int, int);

/*
 * Font management
 */
extern Font*	openfont(Display*, char*, int);
extern Font*	buildfont(Display*, char*, char*, int);
extern void	freefont(Font*);
extern Font*	mkfont(Subfont*, Rune, int);
extern int	cachechars(Font*, char**, Rune**, ushort*, int, int*, char**);
extern void	agefont(Font*);
extern Subfont*	allocsubfont(char*, int, int, int, Fontchar*, Image*);
extern Subfont*	lookupsubfont(Display*, char*);
extern void	installsubfont(char*, Subfont*);
extern void	uninstallsubfont(Subfont*);
extern void	freesubfont(Subfont*);
extern Subfont*	readsubfont(Display*, char*, int, int);
extern void	_unpackinfo(Fontchar*, uchar*, int);
extern Point	stringsize(Font*, char*);
extern int	stringwidth(Font*, char*);
extern int	stringnwidth(Font*, char*, int);
extern Point	runestringsize(Font*, Rune*);
extern int	runestringwidth(Font*, Rune*);
extern int	runestringnwidth(Font*, Rune*, int);
extern Point	strsubfontwidth(Subfont*, char*);
extern int	loadchar(Font*, Rune, Cacheinfo*, int, int, char**);
extern char*	subfontname(char*, char*, int);
extern Subfont*	getsubfont(Display*, char*);
extern Subfont*	getdefont(Display*);
extern Font*	font_open(Display*, char*);
extern void	font_close(Font*);
extern void	display_inc(void*);
extern void	display_dec(void*);
extern int		lockdisplay(Display*, int);
extern void	unlockdisplay(Display*);
extern int		drawlsetrefresh(ulong, int, void*, void*);

/*
 * Predefined 
 */
extern	uchar	defontdata[];
extern	int		sizeofdefont;

#define	BGSHORT(p)		(((p)[0]<<0) | ((p)[1]<<8))
#define	BGLONG(p)		((BGSHORT(p)<<0) | (BGSHORT(p+2)<<16))
#define	BPSHORT(p, v)		((p)[0]=(v), (p)[1]=((v)>>8))
#define	BPLONG(p, v)		(BPSHORT(p, (v)), BPSHORT(p+2, (v)>>16))

#define P2P(p1, p2)	(p1).x = (p2).x, (p1).y = (p2).y
#define R2R(r1, r2)	(r1).min.x = (r2).min.x, (r1).min.y = (r2).min.y,\
			(r1).max.x = (r2).max.x, (r1).max.y = (r2).max.y
/*
 * Compressed bitmap parameters
 */
#define	NMATCH	3		/* shortest match possible */
#define	NRUN	(NMATCH+31)	/* longest match possible */
#define	NMEM	1024		/* window size */
#define	NDUMP	128		/* maximum length of dump */
#define	NCBLOCK	6000		/* size of compressed blocks */

/*
 * Macros to convert between C and Limbo types
 */
#define	IRECT(r)	(*(Rectangle*)&(r))
#define	DRECT(r)	(*(Draw_Rect*)&(r))
#define	IPOINT(p)	(*(Point*)&(p))
#define	DPOINT(p)	(*(Draw_Point*)&(p))
