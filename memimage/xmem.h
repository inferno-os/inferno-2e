#define	Font	XXFont
#define	Screen	XXScreen
#define	Display	XXDisplay

#include <X11/Xlib.h>
/* #include <X11/Xlibint.h> */
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>

#undef	Font
#undef	Screen
#undef	Display

/*
 * Structure pointed to by X field of Memimage
 */
typedef struct Xmem Xmem;

enum
{
	PMundef	= ~0		/* undefined pixmap id */
};

enum
{
	XXonepixel = 1<<0,
	XXcloned = 1<<1
};

struct Xmem
{
	int	pmid;	/* pixmap id for screen ldepth instance */
	int	pmid0;	/* pixmap id for ldepth 0 instance */
	int	flag;
	ulong	word;	/* i->base points here */
	ulong	*wordp;	/* pointer to xmem->word of origin pixmap */
};

extern	int		xtblbit;
extern	int		x24bitswap;
extern	int		infernotox11[];
extern  int		x11toinferno[];
extern	int		xscreendepth;	/* NOT ldepth */
extern	XXDisplay	*xdisplay;
extern	Drawable	xscreenid;
extern	Visual		*xvis;
extern	GC		xgcfill, xgcfill0;
extern	int		xgcfillcolor, xgcfillcolor0;
extern	GC		xgccopy, xgccopy0;
extern	GC		xgczero, xgczero0;
extern	int		xgczeropm, xgczeropm0;
extern	GC		xgcsimplesrc, xgcsimplesrc0;
extern	int		xgcsimplecolor, xgcsimplecolor0, xgcsimplepm, xgcsimplepm0;
extern	GC		xgcreplsrc, xgcreplsrc0;
extern	int		xgcreplsrcpm, xgcreplsrcpm0, xgcreplsrctile, xgcreplsrctile0;
extern	void 		putXdata(Memimage*, XImage*, Rectangle);
extern	XImage*		getXdata(Memimage*, Rectangle);
extern	void		xdirtied(Memimage*);
extern	Memimage*	clonememimage(Memimage*);
