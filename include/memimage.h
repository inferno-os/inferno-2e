#pragma src "/usr/inferno/memimage"

typedef struct	Memimage Memimage;
typedef struct	Memdata Memdata;
typedef struct	Memsubfont Memsubfont;
typedef struct	Memlayer Memlayer;

/*
 * Memdata is allocated from main pool, but .data from the image pool.
 * Memdata is allocated separately to permit patching its pointer after
 * compaction when windows share the image data.
 * The first word of data is a back pointer to the Memdata, to find
 * The word to patch.
 */

struct Memdata
{
	ulong	*base;	/* allocated object */
	ulong	*data;	/* pointer to first word of actual data */
};

struct Memimage
{
	Rectangle	r;	/* rectangle in data area, local coords */
	Rectangle	clipr;	/* clipping region */
	int		ldepth;	/* log base 2 of number of bits per pixel */
	int		repl;	/* whether data area replicates to tile the plane */
	Memdata	*data;	/* pointer to data; shared by windows in this image */
	int		zero;	/* *basep+zero=&word containing (0,0) */
	ulong	width;	/* width in words of total data area */
	Memlayer	*layer;	/* nil if not a layer*/
	void		*X;		/* private storage used only in X implementation */
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
 * to draw characters in the specified color (itself a Memimage) in Memimage b.
 */

struct	Memsubfont
{
	char		*name;
	short	n;		/* number of chars in font */
	uchar	height;		/* height of bitmap */
	char	ascent;		/* top of bitmap to baseline */
	Fontchar *info;		/* n+1 character descriptors */
	Memimage	*bits;		/* of font */
};

/*
 * Memimage management
 */
extern Memimage*	allocmemimage(Rectangle, int);
extern void	freememimage(Memimage*);
extern int		loadmemimage(Memimage*, Rectangle, uchar*, int);
extern int		cloadmemimage(Memimage*, Rectangle, uchar*, int);
extern int		unloadmemimage(Memimage*, Rectangle, uchar*, int);
extern ulong*	wordaddr(Memimage*, Point);
extern uchar*	byteaddr(Memimage*, Point);
extern int		membyteval(Memimage*);
extern int		drawclip(Memimage*, Rectangle*, Memimage*, Point*, Memimage*, Point*, Rectangle*);
extern void		memfillcolor(Memimage*, int);

/*
 * Graphics
 */
extern void	memdraw(Memimage*, Rectangle, Memimage*, Point, Memimage*, Point);
extern void	memline(Memimage*, Point, Point, int, int, int, Memimage*, Point);
extern void	mempoly(Memimage*, Point*, int, int, int, int, Memimage*, Point);
extern void	memfillpoly(Memimage*, Point*, int, int, Memimage*, Point);
extern void	memfillpolysc(Memimage*, Point*, int, int, Memimage*, Point, int, int, int);
extern void	memimagedraw(Memimage*, Rectangle, Memimage*, Point, Memimage*, Point);
extern void	memimageline(Memimage*, Point, Point, int, int, int, Memimage*, Point);
extern void	_memimageline(Memimage*, Point, Point, int, int, int, Memimage*, Point, Rectangle);
extern Point	memimagestring(Memimage*, Point, Memimage*, Memsubfont*, char*);
extern void	memellipse(Memimage*, Point, int, int, int, Memimage*, Point);
extern void	memarc(Memimage*, Point, int, int, int, Memimage*, Point, int, int);
extern Memimage*	membrush(int);
extern int	memlineendsize(int);
extern Rectangle	memlinebbox(Point, Point, int, int, int);

/*
 * Subfont management
 */
extern Memsubfont*	allocmemsubfont(char*, int, int, int, Fontchar*, Memimage*);
extern void	freememsubfont(Memsubfont*);
extern Point	memsubfontwidth(Memsubfont*, char*);
extern Memsubfont*	getmemdefont(void);

/*
 * Predefined 
 */
extern	Memimage	*memones;
extern	Memimage	*memzeros;
extern	int			isX;
