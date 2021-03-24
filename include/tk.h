enum
{
	TKframe,		/* Widget type */
	TKlabel,
	TKcheckbutton,
	TKbutton,
	TKmenubutton,
	TKmenu,
	TKseparator,
	TKcascade,
	TKlistbox,
	TKscrollbar,
	TKtext,
	TKcanvas,
	TKentry,
	TKradiobutton,
	TKscale,
	TKwidgets,

	TKsingle	= 0,	/* Select mode */
	TKbrowse,
	TKmultiple,

	TKraised,		/* Relief */
	TKsunken,
	TKflat,
	TKgroove,
	TKridge,

	TkArepl		= 0,	/* Bind options */
	TkAadd,

	Tkvertical	= 0,	/* Scroll bar orientation */
	Tkhorizontal,

	Tkwrapnone	= 0,	/* Wrap mode */
	Tkwrapword,
	Tkwrapchar,

	TkDdelivered	= 0,	/* Event Delivery results */
	TkDbreak,
	TkDnone,

	TkLt = 0,		/* Comparison operators */
	TkLte,
	TkEq,
	TkGte,
	TkGt,
	TkNeq,

	
	OPTdist		= 0,	/* Distance */
	OPTstab,		/* String->Constant table */
	OPTtext,		/* Text string */
	OPTwinp,		/* Window Path to Tk ptr */
	OPTflag,		/* Option sets bitmask */
	OPTbmap,		/* Option specifies bitmap file */
	OPTbool,		/* Set to one if option present */
	OPTfont,		/* Env font */
	OPTfrac,		/* list of fixed point distances (count in aux) */
	OPTctag,		/* Tag list for canvas item */
	OPTtabs,		/* Tab stops */
	OPTcolr,		/* Colors */
	OPTimag,		/* Image */
	OPTsize,		/* width/height */
	OPTevim,		/* Image as color */

	BoolX		= 0,
	BoolT,
	BoolF,

	Frameinset	= 2,	/* Frame inset left at box edges */
	Triangle	= 14,	/* Height of scroll bar triangle */
	Entrypady	= 3,
	Entrypadx	= 4,
	Listpadx	= 6,	/* X padding of text in listboxes */
	Listpady	= 2,	/* Y padding of text in listboxes */
	Textpadx	= 6,
	Textpady	= 3,
	Cvsicursor	= 1,	/* Extra height of insertion cursor in canvas */
	Bitpadx		= 2,	/* Bitmap padding in labels */
	Bitpady		= 2,
	Sepheight	= 12,	/* Height of menu separator */
	CheckButton	= 10,
	CheckButtonBW	= 2,
	ButtonBorder	= 4,
	CheckSpace	= CheckButton + 2*CheckButtonBW + 2*ButtonBorder,

	Tkmaxitem	= 128,
	Tkminitem	= 32,
	Tkcvstextins	= 1024,
	Tkmaxdump	= 1024,
	Tkentryins	= 128,
	TkMaxmsgs	= 100,
	Tkshdelta	= 0x40,	/* color intensity delta for shading */

	Tkfpscalar	= 1000,	/* Fixed point scale factor */

	Tkdpi		= 100,	/* pixels per inch on an average display */

	Tksweep		= 64,	/* binds before a sweep */

	TkStatic	= 0,
	TkDynamic	= 1
};

#define TKSTRUCTALIGN	4
#define TKI2F(i)	((i)*Tkfpscalar)
#define TKF2I(f)	((f)/Tkfpscalar)
#define IAUX(i)		((void*)i)
#define AUXI(i)		((int)i)
#define TKKEY(i)	((i)&0xFFFF)

typedef struct Tk Tk;
typedef struct TkCtxt TkCtxt;
typedef struct TkEnv TkEnv;
typedef struct TkFrame TkFrame;
typedef struct TkLabel TkLabel;
typedef struct TkName TkName;
typedef struct TkOptab TkOptab;
typedef struct TkOption TkOption;
typedef struct TkStab TkStab;
typedef struct TkTop TkTop;
typedef struct TkGeom TkGeom;
typedef struct TkMethod TkMethod;
typedef struct TkAction TkAction;
typedef struct TkMenubut TkMenubut;
typedef struct TkWin TkWin;
typedef struct TkCmdtab TkCmdtab;
typedef struct TkLentry TkLentry;
typedef struct TkMouse TkMouse;
typedef struct TkScroll TkScroll;
typedef struct TkText TkText;
typedef struct TkTitem TkTitem;
typedef struct TkTline TkTline;
typedef struct TkTindex TkTindex;
typedef struct TkTmarkinfo TkTmarkinfo;
typedef struct TkTtabstop TkTtabstop;
typedef struct TkTtaginfo TkTtaginfo;
typedef struct TkTwind TkTwind;
typedef struct TkCimeth TkCimeth;
typedef struct TkCitem TkCitem;
typedef struct TkCanvas TkCanvas;
typedef struct TkCline TkCline;
typedef struct TkCpoly TkCpoly;
typedef struct TkCtag TkCtag;
typedef struct TkCpoints TkCpoints;
typedef struct TkListbox TkListbox;
typedef struct TkCwind TkCwind;
typedef struct TkVar TkVar;
typedef struct TkMsg TkMsg;
typedef struct TkEbind TkEbind;
typedef struct TkImg TkImg;
typedef struct TkCursor TkCursor;
typedef void (*TkNewgeom)(Tk*, int, int, int, int);
typedef void (*TkDestroyed)(Tk*);
typedef void (*TkIamdirty)(Tk*);
typedef Point (*TkRelpos)(Tk*);

struct TkImg
{
	TkTop*	top;
	int	ref;
	int	type;
	int	w;
	int	h;
	TkEnv*	env;
	Image*	fgimg;
	Image*	maskimg;
	TkImg*	link;
	TkName*	name;
};

struct TkCursor
{
	int	def;
	Point	hot;
	Image*	bit;
	TkImg*	img;
};

struct TkEbind
{
	int	event;
	char*	cmd;
};

struct TkMsg
{
	TkVar*	var;
	TkMsg*	link;
	char	msg[TKSTRUCTALIGN];
};

enum
{
	TkVchan		= 1,
	TkVstring
};

struct TkVar
{
	int	type;
	TkVar*	link;
	void*	value;
	char	name[TKSTRUCTALIGN];
};

struct TkCline
{
	int		arrow;
	int		shape[3];
	int		width;
	Image*		stipple;
	Image*		pen;
	int		arrowf;
	int		arrowl;
	int		capstyle;
	int		smooth;
	int		steps;
};

struct TkCpoly
{
	int		width;
	Image*		stipple;
	Image*		pen;
	int		smooth;
	int		steps;
};

struct TkCwind
{
	Tk*		sub;		/* Subwindow of canvas */
	Tk*		focus;		/* Current Mouse focus */
	int		width;		/* Requested width */
	int		height;		/* Requested height */
	int		anchor;		/* Draw anchor */
};

struct TkCpoints
{
	int		npoint;		/* Number of points */
	Point*		parampt;	/* Parameters in fixed point */
	Point*		drawpt;		/* Draw coord in pixels */
	Rectangle	bb;		/* Bounding box in pixels */
};

struct TkCitem
{
	int		id;		/* Unique id */
	int		type;		/* Object type */
	TkCpoints	p;		/* Points plus bounding box */
	TkEnv*		env;		/* Colors & fonts */
	TkCitem*	next;		/* Z order */
	TkName*		tags;		/* Temporary tag spot */
	TkCtag*		stag;		/* Real tag structure */
	char		obj[TKSTRUCTALIGN];
};

struct TkCtag
{
	TkCitem*	item;		/* Link to item */
	TkName*		name;		/* Text name or id */
	TkCtag*		taglist;	/* Down from tag hash */
	TkCtag*		itemlist;	/* Down from item */
};

enum
{
	/* Item types */
	TkCVline,
	TkCVtext,
	TkCVrect,
	TkCVoval,
	TkCVbitmap,
	TkCVpoly,
	TkCVwindow,
	TkCVimage,
	TkCVarc,

	TkCselto	= 0,
	TkCselfrom,
	TkCseladjust,

	TkCadd		= 0,
	TkCfind,
	
	TkChash		= 32,

	TkCarrowf	= (1<<0),
	TkCarrowl	= (1<<1),
	Tknarrow	= 6		/* Number of points in arrow */
};

struct TkCanvas
{
	int		close;
	int		confine;
	int		cleanup;
	int		scrollr[4];
	Rectangle	region;
	Rectangle	update;		/* Area to paint next draw */
	Point		view;
	TkCitem*	selection;
	int		width;
	int		height;
	int		xscrolli;	/* Scroll increment */
	int		yscrolli;
	char*		xscroll;	/* Scroll commands */
	char*		yscroll;
	int		id;		/* Unique id */
	TkCitem*	head;		/* Items in Z order */
	TkCitem*	tail;		/* Head is lowest, tail is highest */
	TkCitem*	focus;		/* Keyboard focus */
	TkCitem*	mouse;		/* Mouse focus */
	TkName*		current;	/* Fake for current tag */
	TkCtag		curtag;
	Image*		image;		/* Drawing space */
	TkName*		thash[TkChash];	/* Tag hash */
	int		actions;
	int		actlim;
};

struct TkCimeth
{
	char*	name;
	char*	(*create)(Tk*, char *arg, char **val);
	void	(*draw)(Image*, TkCitem*);
	void	(*free)(TkCitem*);
	char*	(*coord)(TkCitem*, char*, int, int);
	char*	(*cget)(TkCitem*, char*, char**);
	char*	(*conf)(Tk*, TkCitem*, char*);
};

struct TkScroll
{
	int		activer;
	int		elembw;		/* Element border widths */
	int		orient;		/* Horitontal or Vertical */
	int		dragpix;	/* Scroll delta in button drag */
	int		dragtop;
	int		dragbot;
	int		jump;		/* Jump scroll enable */
	int		flag;		/* Display flags */
	int		top;		/* Top fraction */
	int		bot;		/* Bottom fraction */
	int		a1;		/* Pixel top/left arrow1 */
	int		t1;		/* Pixel top/left trough */
	int		t2;		/* Pixel top/left lower trough */
	int		a2;		/* Pixel top/left arrow2 */
	char*		cmd;
};

struct TkListbox
{
	TkLentry*	head;
	TkLentry*	anchor;
	TkLentry*	active;
	int		yelem;		/* Y element at top of box */
	int		xelem;		/* Left most character position */
	int		nitem;
	int		nwidth;
	int		selmode;
	char*		xscroll;
	char*		yscroll;
};

struct TkMouse
{
	int		x;
	int		y;
	int		b;
};

struct TkLentry
{
	TkLentry*	link;
	int		flag;
	int		len;
	char		text[TKSTRUCTALIGN];
};

struct TkCmdtab
{
	char*		name;
	char*		(*fn)(Tk*, char*, char**);
};

struct TkAction
{
	int		event;
	int		type;
	char*		arg;
	TkAction*	link;
};

struct TkStab
{
	char*		val;
	int		con;
};

struct TkOption
{
	char*		o;
	int		type;
	int		offset;
	void*		aux;
};

struct TkOptab
{
	void*		ptr;
	TkOption*	optab;	
};

enum
{
	/* Widget Control Bits */
	Tktop		= (1<<0),
	Tkbottom	= (1<<1),
	Tkleft		= (1<<2),
	Tkright		= (1<<3),
	Tkside		= Tktop|Tkbottom|Tkleft|Tkright,
	Tkfillx		= (1<<4),
	Tkfilly		= (1<<5),
	Tkfill		= Tkfillx|Tkfilly,
	Tkexpand	= (1<<6),
	Tkrefresh	= (1<<7),
	Tknoprop	= (1<<8),
	Tkcenter	= (1<<9),
	Tkbaseline 	= (1<<10),
	Tknumeric	= (1<<11),
	Tknorth		= (1<<10),
	Tknortheast	= (1<<11),
	Tkeast		= (1<<12),
	Tksoutheast	= (1<<13),
	Tksouth		= (1<<14),
	Tksouthwest	= (1<<15),
	Tkwest		= (1<<16),
	Tknorthwest	= (1<<17),
	Tkanchor	= Tkcenter|Tknorth|Tknortheast|Tkeast|Tksoutheast|
			  Tksouth|Tksouthwest|Tkwest|Tknorthwest,
	Tkdirty		= (1<<20),	/* Modified but not displayed */
	Tkwindow	= (1<<21),	/* Top level window */
	Tkfocus		= (1<<22),	/* Pointer is over active object */
	Tkactivated	= (1<<23),	/* Button pressed etc.. */
	Tkdisabled	= (1<<24),	/* Button is not accepting events */
	Tkmapped	= (1<<25),	/* Mapped onto display */
	Tkstopevent	= (1<<26),	/* Stop searching down for events */
	Tkdestroy	= (1<<27),	/* Marked for death */
	Tksetwidth	= (1<<28),
	Tksetheight	= (1<<29),
	Tksubsub	= (1<<30),
	Tkswept		= (1<<31),

	/* Supported Event Types 		*/
	/* Bits 0-15 are keyboard characters 	*/
	TkEnter		= (1<<16),
	TkLeave		= (1<<17),
	TkButton1P	= (1<<18),
	TkButton1R	= (1<<19),
	TkButton2P	= (1<<20),
	TkButton2R	= (1<<21),
	TkButton3P	= (1<<22),
	TkButton3R	= (1<<23),
	TkMotion	= (1<<24),
	TkMap		= (1<<25),
	TkUnmap		= (1<<26),
	TkKey		= (1<<27),
	TkFocusin	= (1<<28),
	TkFocusout	= (1<<29),
	TkConfigure	= (1<<30),
	TkDouble	= (1<<31),
	TkEmouse	= TkButton1P|TkButton1R|TkButton2P|TkButton2R|
			  TkButton3P|TkButton3R|TkMotion,
	TkEpress	= TkButton1P|TkButton2P|TkButton3P,
	TkErelease	= TkButton1R|TkButton2R|TkButton3R,
	TkAllEvents	= ~0,

	/* Motion event bits */
	TkMouse1	= (1<<0),
	TkMouse2	= (1<<1),
	TkMouse3	= (1<<2),

	TkactivA1	= (1<<0),	/* Scrollbar control */
	TkactivA2	= (1<<1),
	TkactivB1	= (1<<2),
	TkbuttonA1	= (1<<3),
	TkbuttonA2	= (1<<4),
	TkbuttonB1	= (1<<5),

	TkCforegnd	= 0,
	TkCbackgnd,			/* group of 3 */
	TkCbackgndlght,
	TkCbackgnddark,
	TkCselect,
	TkCselectbgnd,			/* group of 3 */
	TkCselectbgndlght,
	TkCselectbgnddark,
	TkCselectfgnd,
	TkCactivebgnd,
	TkCactivefgnd,
	TkCdisablefgnd,
	TkChlightbgnd,
	TkCfill,

	TkNcolor,

	TkTborderwidth	= 0,
	TkTjustify,
	TkTlmargin1,
	TkTlmargin2,
	TkTlmargin3,
	TkTrmargin,
	TkTspacing1,
	TkTspacing2,
	TkTspacing3,
	TkToffset,
	TkTunderline,
	TkToverstrike,
	TkTrelief,
	TkTwrap,

	TkTnumopts
};

struct TkEnv
{
	int		ref;
	TkTop*		top;			/* Owner */
	Image*		evim[TkCbackgnd+1];
	ulong		set;
	uchar		colors[TkNcolor];	/* OPTcolr */
	Font*		font;			/* Font description */
	int		wzero;			/* Width of "0" in pixel */
};

struct TkGeom
{
	int		x;
	int		y;
	int		width;
	int		height;
};

struct Tk
{
	int		type;		/* Widget type */
	TkName*		name;		/* Hierarchy name */
	Tk*		siblings;	/* Link to descendents */
	Tk*		master;		/* Pack owner */
	Tk*		slave;		/* Packer slaves */
	Tk*		next;		/* Link for packer slaves */
	Tk*		parent;		/* Window is sub of canvas or text */
	Tk*		depth;		/* Window depth when mapped */
	TkNewgeom	geom;		/* Geometry change notify function */
	TkDestroyed	destroyed;	/* Destroy notify function */
	TkIamdirty	dirty;		/* Child notifies parent to redraw */
	TkRelpos	relpos;		/* To get pos rel to parent */
	int		flag;		/* Misc flags. */
	TkEnv*		env;		/* Colors & fonts */
	TkAction*	binds;		/* Binding of current events */
	void		(*deliverfn)(Tk*, int, void*);

	TkGeom		req;		/* Requested size and position */
	TkGeom		act;		/* Actual size and position */
	int		relief;		/* 3D border type */
	int		borderwidth;	/* 3D border size */
	int		sborderwidth;	/* select 3D border size */
	Point		pad;		/* outside frame padding */
	Point		ipad;		/* inside frame padding */

	char		obj[TKSTRUCTALIGN];
};

enum
{
  TkNoUnmapOnFocusOut,		
  TkUnmapOnFocusOut		/* Menu unmaps on loss of focus */
};

struct TkWin
{
	Image*		image;
	Tk*		next;
	char*		postcmd;
	char*		cascade;
	Tk*		lastfocus;  /* Who held the focus before a menu pop-up */
	int		itemcount;  /* Number of entries in menu */
	int		unmapFocusCtl;
};

struct TkMethod
{
	char*		name;
	void		(*free)(Tk*);
	char*		(*draw)(Tk*, Point);
	void		(*geom)(Tk*);
	TkCmdtab*	cmd;
	int		ncmd;
};

struct TkFrame
{
	char		placeholder[TKSTRUCTALIGN];
};

struct TkLabel
{
	char*		text;		/* Label value */
	char*		command;	/* Command to execute at invoke */
	char*		value;		/* Variable value in radio button */
	char*		variable;	/* Variable name in radio button */
	Image*		bitmap;		/* Bitmap to display */
	int		check;
	int		anchor;
	int		ul;
	int		w;
	int		h;
	TkImg*		img;
};

struct TkMenubut
{
	TkLabel		lab;		/* Must be first */
	char*		menu;		/* Window name of menu to post */
};

struct TkName
{
	TkName*		link;
	void*		obj;		/* Name for ... */
	union {
		TkAction*	binds;
	}prop;				/* Properties for ... */
	int		ref;
	char		name[TKSTRUCTALIGN];
};

enum
{
	/* text item types */
	TkTascii,	/* contiguous string of ascii chars, all with same tags */
	TkTrune,	/* printable utf (one printing position) */
	TkTtab,
	TkTnewline,	/* line field contains pointer to containing line */
	TkTcontline,	/* end of non-newline line; line field as with TkTnewline */
	TkTwin,
	TkTmark,

	TkTbyitem = 0,	/* adjustment units */
	TkTbyitemback,
	TkTbytline,
	TkTbytlineback,
	TkTbychar,
	TkTbycharback,
	TkTbycharstart,
	TkTbyline,
	TkTbylineback,
	TkTbylinestart,
	TkTbylineend,
	TkTbywordstart,
	TkTbywordend,

	TkTselid	= 0,		/* id of sel tag */
	TkTmaxtag	= 32,
	Textwidth	= 40,		/* default width, in chars */
	Textheight	= 10,		/* default height, in chars */

	TkTfirst	= (1<<0),	/* first line in buffer, or after a TkTlast */
	TkTlast		= (1<<1),	/* TkTnewline at end of line */
	TkTdrawn	= (1<<2),	/* screen cache copy is ok */
	TkTdlocked	= (1<<3),	/* display already locked */
	TkTjustfoc	= (1<<4),	/* got focus on last B1 press */
	TkTunset 	= (1<<31)	/* marks int tag options "unspecified" */
};

struct TkTline
{
	Point		orig;		/* where to put first item of line */
	int		width;
	int		height;
	int		ascent;
	int		flags;
	TkTitem*	items;
	TkTline*	next;
	TkTline*	prev;
};

struct TkText
{
	TkTline		start;		/* fake before-the-first line */
	TkTline		end;		/* fake after-the-last line */
	Tk*			tagshare;
	TkTtabstop*	tabs;
	TkTtaginfo*	tags;
	TkTmarkinfo*	marks;
	char*		xscroll;
	char*		yscroll;
	uchar		selunit;	/* select adjustment unit */
	uchar		tflag;		/* various text-specific flags */
	int			nlines;	/* number of nl items in widget */
	TkTitem*	selfirst;	/* first item marked with sel tag */
	TkTitem*	sellast;	/* item after last marked with sel tag */
	Point		deltatv;	/* vector from text-space to view-space */
	Point		deltasv;	/* vector from screen-space to view-space */
	Point		deltaiv;	/* vector from image-space to view-space */
	Point		current;	/* last known mouse pos */
	Point		track;	/* for use when B1 or B2 is down */
	int		nexttag;	/* next usable tag index */
	TkTitem*	focus;		/* keyboard focus */
	TkTitem*	mouse;		/* mouse focus */
	int		inswidth;	/* width of insertion cursor */
	int		opts[TkTnumopts];
	int		scrolltop[2];
	int		scrollbot[2];
	Image*		image;
	uchar		cur_flag;	/* text cursor to be shown up? */
	Rectangle	cur_rec;	/* last text cursor rectangle */
};

struct TkTtabstop
{
	int		pos;
	int		justify;
	TkTtabstop*	next;
};

struct TkTwind
{
	Tk*		sub;		/* Subwindow of canvas */
	int		align;		/* how to align within line */
	char*		create;		/* creation script */
	int		padx;		/* extra space on each side */
	int		pady;		/* extra space on top and bot */
	int		width;		/* current internal width */
	int		height;		/* current internal height */
	int		stretch;	/* true if need to stretch height */
	Tk*		focus;		/* Current Mouse focus */
};

struct TkTitem
{
	uchar		kind;
	uchar		tagextra;
	short		width;
	TkTitem		*next;
	union	{
		char*		string;
		TkTwind*	win;
		TkTmarkinfo*	mark;
		TkTline*	line;
	} u;
	ulong		tags[1];
	/* TkTitem length extends tagextra ulongs beyond */
};

struct TkTmarkinfo
{
	char*		name;
	int		gravity;
	TkTitem*	cur;
	TkTmarkinfo*	next;
};

struct TkTtaginfo
{
	int		id;
	char*		name;
	TkEnv*		env;
	TkTtabstop*	tabs;
	TkTtaginfo*	next;
	TkAction*	binds;		/* Binding of current events */
	int		opts[TkTnumopts];
};

struct TkTindex
{
	TkTitem*	item;
	TkTline*	line;
	int		pos;		/* index within multichar tiem */
};

struct TkCtxt
{
	int		ref;
	Display*	disp;
	Image*		colors[256];
	Image*		i;
	Tk*		tkdepth;
	TkTop*		tkwindows;
	Tk*		tkMgrab;
	Tk*		tkMfocus;
	Tk*		tkKgrab;
	TkMouse		tkmstate;
	TkCtxt*		link;
};

struct TkTop
{
	int		id;		/* Must agree with tk.m/Toplevel */
	void*		di;		/* really Draw_Image* */

	/* Private from here on */
	TkCtxt*		ctxt;
	Screen*		screen;
	void*		dscreen;
	Tk*		root;
	Tk*		windows;
	Tk*		select;
	TkEnv*		env;
	TkTop*		link;
	TkVar*		vars;
	TkMsg*		msgs;
	TkImg*		imgs;
	TkAction*	binds[TKwidgets];
	int		nmsg;
	int		debug;
	char*		err;
	char		errcmd[32];
};

#define TKobj(t, p)	((t*)(p->obj))
#define OPTION(p, t, o)	(*(t*)((char*)p + o))

extern	Point		tkzp;
extern	Rectangle	bbnil;
extern	Rectangle	huger;
extern	TkMethod	tkmethod[];
extern	TkCimeth	tkcimethod[];
extern	TkCmdtab	tkttagcmd[];
extern	TkCmdtab	tktmarkcmd[];
extern	TkCmdtab	tktwincmd[];
extern	int		cvslshape[];
extern	TkCursor	tkcursor;
extern	char*		tkfont;

/* Errors */
extern	char	TkNomem[];
extern	char 	TkBadop[];
extern	char 	TkOparg[];
extern	char 	TkBadvl[];
extern	char 	TkBadwp[];
extern	char 	TkNotop[];
extern	char	TkDupli[];
extern	char	TkNotpk[];
extern	char	TkBadcm[];
extern	char	TkIstop[];
extern	char	TkBadbm[];
extern	char	TkBadft[];
extern	char	TkBadit[];
extern	char	TkBadtg[];
extern	char	TkFewpt[];
extern	char	TkBadsq[];
extern	char	TkBadix[];
extern	char	TkNotwm[];
extern	char	TkWpack[];
extern	char	TkBadvr[];
extern	char	TkNotvt[];
extern	char	TkMovfw[];
extern	char	TkBadsl[];
extern	char	TkSyntx[];

/* Widget Commands */
extern	TkCmdtab	tkmenucmd[];

/* Option tables */
extern	TkStab		tkrelief[];
extern	TkStab		tkorient[];
extern	TkStab		tklines[];
extern	TkStab		tkanchor[];
extern	TkStab		tkside[];
extern	TkStab		tkfill[];
extern	TkStab		tkbool[];
extern	TkStab		tktabjust[];
extern	TkStab		tkcolortab[];
extern	TkStab		tkwrap[];
extern	TkStab		tkstate[];
extern	TkStab		tkjustify[];
extern	TkStab		tkcompare[];
extern	TkStab		tkalign[];
extern	TkOption	tkgeneric[];
extern	TkOption	tktop[];
extern	TkOption	tkbutopts[];
extern	TkOption	tklabelopts[];
extern	TkOption	tktopdbg[];
extern	TkOption	tkradopts[];

/* Limbo entry points */
extern	char*	tkframe(TkTop*, char*, char**);
extern	char*	tklabel(TkTop*, char*, char**);
extern	char*	tkpack(TkTop*, char*, char**);
extern	char*	tkcheckbutton(TkTop*, char*, char**);
extern	char*	tkradiobutton(TkTop*, char*, char**);
extern	char*	tkbutton(TkTop*, char*, char**);
extern	char*	tkmenubutton(TkTop*, char*, char**);
extern	char*	tkmenu(TkTop*, char*, char**);
extern	char*	tklistbox(TkTop*, char*, char**);
extern	char*	tkscrollbar(TkTop*, char*, char**);
extern	char*	tktext(TkTop*, char*, char**);
extern	char*	tkcanvas(TkTop*, char*, char**);
extern	char*	tkbind(TkTop*, char*, char**);
extern	char*	tkfocus(TkTop*, char*, char**);
extern	char*	tkraise(TkTop*, char*, char**);
extern	char*	tklower(TkTop*, char*, char**);
extern	char*	tkentry(TkTop*, char*, char**);
extern	char*	tksend(TkTop*, char*, char**);
extern	char*	tkputs(TkTop*, char*, char**);
extern	char*	tkgrab(TkTop*, char*, char**);
extern	char*	tkdestroy(TkTop*, char*, char**);
extern	char*	tkimage(TkTop*, char*, char**);
extern	char*	tkupdatecmd(TkTop*, char*, char**);
extern	char*	tkvariable(TkTop*, char*, char**);
extern	char*	tkcursorcmd(TkTop*, char*, char**);
extern	char*	tkscale(TkTop*, char*, char**);
extern	char*	tkwinfo(TkTop*, char*, char**);

/* General */
extern	char*		tkaction(TkAction**, int, int, char*, int);
extern	char*		tkaddchild(TkTop*, Tk*, TkName**);
extern	void		tkappendpack(Tk*, Tk*, int);
extern	void		tkbbmax(Rectangle*, Rectangle*);
extern	void		tkbevel(Image*, Point, int, int, int, Image*, Image*);
extern	char*		tkbindings(TkTop*, Tk*, TkEbind*, int);
extern	char*		tkcaddtag(Tk*, TkCitem*, int);
extern	TkCtag*		tkcfirsttag(TkCitem*, TkCtag*);
extern	void		tkchkpress(Tk*, int, void*, void*);
extern	void		tkcmdbind(Tk*, int, void*, void*);
extern	TkName*		tkctaglook(Tk*, TkName*, char*);
extern	void		tkcvsappend(TkCanvas*, TkCitem*);
extern	void		tkcvsfreeitem(TkCitem*);
extern	void		tkcvsgeom(Tk*);
extern	Point		tkcvsrelpos(Tk*);
extern	char*		tkcvsdtag(Tk*, char*, char**);
extern	char*		tkcvstextdchar(Tk*, TkCitem*, char*);
extern	char*		tkcvstextindex(Tk*, TkCitem*, char*, char **val);
extern	char*		tkcvstextinsert(Tk*, TkCitem*, char*);
extern	char*		tkcvstexticursor(Tk*, TkCitem*, char*);
extern	TkEnv*		tkdefaultenv(TkTop*);
extern	void		tkdeliver(Tk*, int, void*);
extern	int		tkdist(Point, Point);
extern	char*		tkdrawcanv(Tk*, Point);
extern	char*		tkdrawframe(Tk*, Point);
extern	char*		tkdrawlabel(Tk*, Point);
extern	char*		tkdrawscale(Tk*, Point);
extern	char*		tkdrawlistb(Tk*, Point);
extern	void		tkdrawrelief(Image*, Tk*, Point*, int);
extern	char*		tkdrawscrlb(Tk*, Point);
extern	char*		tkdrawstring(Tk*, Image*, Point, char*, int, int);
extern	char*		tkdrawtext(Tk*, Point);
extern	Image*		tkdupcolor(Image*);
extern	TkEnv*		tkdupenv(TkEnv**);
extern	int		tkfillpoly(Point*, int, int, Image*, Image*);
extern  void            tkclrfocus(Tk*, Tk*);
extern	Tk*		tkfindfocus(TkCtxt*, int, int);
extern	void		tkfliprelief(Tk*);
extern	char*		tkfprint(char*, int);
extern	char*		tkfrac(TkTop*, char*, int*, TkEnv*);
extern	void		tkfreecanv(Tk*);
extern	void		tkfreeframe(Tk*);
extern	void		tkfreelabel(Tk*);
extern	void		tkfreescale(Tk*);
extern	void		tkfreelistb(Tk*);
extern	void		tkfreemenub(Tk*);
extern	void		tkfreemenu(Tk*);
extern	void		tkfreename(TkName*);
extern	void		tkfreeobj(Tk*);
extern	void		tkfreepoint(TkCpoints*);
extern	void		tkfreescrlb(Tk*);
extern	void		tkfreetext(Tk*);
extern	char*		tkgencget(TkOptab*, char*, char**);
extern	Image*		tkimageof(Tk*);
extern	char*		tkitem(char*, char*);
extern	void		tklistbgeom(Tk*);
extern	void		tklistbmotion(Tk*, int, void*, void*);
extern	Tk*		tklook(TkTop*, char*, int);
extern	char*		tkmap(Tk*);
extern	TkName*		tkmkname(char*);
extern	void		tkmoveresize(Tk*, int, int, int, int);
extern	char*		tkmpost(Tk*, int, int);
extern	TkEnv*		tknewenv(TkTop*);
extern	Tk*		tknewobj(TkTop*, int, int);
extern	void		tkpacker(Tk*);
extern	void		tkpackqit(Tk*);
extern	char*		tkparse(TkTop*, char*, TkOptab*, TkName**);
extern	char*		tkparsepts(TkTop*, TkCpoints*, char**);
extern	void		tkpolybound(Point*, int, Rectangle*);
extern	Point		tkposn(Tk*);
extern	void		tkpostmenu(Tk*, int, void*, void*);
extern	void		tkputenv(TkEnv*);
extern	char*		tkradioinvoke(Tk *, char *, char **);
extern	void		tkrunpack(void);
extern	void		tkscrollleave(Tk*, int, void*, void*);
extern	void		tkscrollmotion(Tk*, int, void*, void*);
extern	void		tksetselect(Tk*);
extern	void		tksizelabel(Tk*);
extern	Point		tkstringsize(Tk*, char*);
extern	int		tksubdeliver(Tk*, TkAction*, int, void*);
extern	void		tktextgeom(Tk*);
extern	char*		tktextdelete(Tk*, char*, char **);
extern	void		tktextevent(Tk*, int, void*);
extern	char*		tktextinsert(Tk*, char*, char **);
extern	char*		tktextmark(Tk*, char*, char **);
extern	char*		tktextsee(Tk*, char*, char**);
extern	char*		tktextselection(Tk*, char*, char**);
extern	char*		tktexttag(Tk*, char*, char**);
extern	char*		tktaddmarkinfo(TkText*, char*, TkTmarkinfo**);
extern	char*		tktaddtaginfo(Tk*, char*, TkTtaginfo**);
extern	int		tktadjustind(TkText*, int, TkTindex*);
extern	int		tktanytags(TkTitem*);
extern	Rectangle	tktbbox(Tk*, TkTindex*);
extern	void		tktdirty(Tk*);
extern	int		tktdispwidth(Tk*, TkTitem*, Font*, int, int, int);
extern	void		tktendind(TkText*, TkTindex*);
extern	TkTmarkinfo*	tktfindmark(TkTmarkinfo*, char*);
extern	int		tktfindsubitem(Tk*, TkTindex*);
extern	TkTtaginfo*	tktfindtag(TkTtaginfo*, char*);
extern	char*		tktfixgeom(Tk*, TkTline*, TkTline*);
extern	void		tktfreeitems(TkText*, TkTitem*);
extern	void		tktfreelines(TkText*, TkTline*);
extern	void		tktfreemarks(TkTmarkinfo*);
extern	void		tktfreetabs(TkTtabstop*);
extern	void		tktfreetags(TkTtaginfo*);
extern	int		tktindcompare(TkText*, TkTindex*, int, TkTindex*);
extern	int		tktindbefore(TkTindex*, TkTindex*);
extern	int		tktindrune(TkTindex*);
extern	char*		tktinsert(Tk*, TkTindex*, char*, TkTitem*);
extern	int		tktiswordchar(int);
extern	void		tktitemind(TkTitem*, TkTindex*);
extern	char*		tktiteminsert(TkText*, TkTindex*, TkTitem*);
extern	TkTline*	tktitemline(TkTitem*);
extern	char*		tktindparse(Tk*, char**, TkTindex*);
extern	TkTitem*	tktlastitem(TkTitem*);
extern	int		tktlinenum(TkText*, TkTindex*);
extern	int		tktlinepos(TkText*, TkTindex*);
extern	int		tktmarkind(Tk*, char*, TkTindex*);
extern	char*		tktmarkmove(Tk*, TkTmarkinfo*, TkTindex*);
extern	char*		tktmarkparse(Tk*, char**, TkTmarkinfo**);
extern	int		tktmaxwid(TkTline*);
extern	char*		tktnewitem(int, int, TkTitem**);
extern	char*		tktnewline(int, TkTitem*, TkTline*, TkTline*, TkTline**);
extern	int		tktposcount(TkTitem*);
extern	void		tktremitem(TkText*, TkTindex*);
extern	int		tktsametags(TkTitem*, TkTitem*);
extern	char*		tktsplititem(TkTindex*);
extern	void		tktstartind(TkText*, TkTindex*);
extern	char*		tkttagchange(Tk*, int, TkTindex*, TkTindex*, int);
extern	int		tkttagbit(TkTitem*, int, int);
extern	void		tkttagcomb(TkTitem*, TkTitem*, int);
extern	int		tkttagind(Tk*, char*, int, TkTindex*);
extern	char*		tkttagname(TkText*, int);
extern	int		tkttagnrange(TkText*, int, TkTindex*, TkTindex*, TkTindex*, TkTindex*);
extern	void		tkttagopts(Tk*, TkTitem*, int*, TkEnv*, int);
extern	char*		tkttagparse(Tk*, char**, TkTtaginfo**);
extern	int		tkttagset(TkTitem*, int);
extern	char*		tkunits(char, int*, TkEnv*);
extern	int		tktxyind(Tk*, int, int, TkTindex*);
extern	char*		tktxyparse(Tk*, char**, Point*);
extern	void		tkunmap(Tk*);
extern	char*		tkupdate(TkTop*);
extern	char*		tkvalue(char**, char*, ...);
extern	void		tkxlatepts(Point*, int, int, int);
extern	TkCitem*	 tkcnewitem(Tk*, int, int);
extern	char*		tkword(TkTop*, char*, char*, char*);
extern	void		tkmkpen(Image**, TkEnv*, Image*);
extern	void		tkcvstextfocus(Tk*, TkCitem*, int);
extern	char*		tkcvstextselect(Tk*, TkCitem*, char*, int);
extern	void		tkcvstextclr(Tk*);
extern	void		tkfreeentry(Tk*);
extern	char*		tkdrawentry(Tk*, Point);
extern	void		tkentrygeom(Tk*);
extern	char*		tkexec(TkTop*, char*, char**);
extern	void		tktextsdraw(Image*, Rectangle, TkEnv*, Image*, int);
extern	void		tksetbits(Tk*, int);
extern	char*		tkdrawslaves(Tk*, Point);
extern	void		tkcvsevent(Tk*, int, void*);
extern	Point		tkcvsanchor(Point, int, int, int);
extern	void		tkfreebind(TkAction*);
extern	void		tkgeomchg(Tk*, TkGeom*);
extern	int		tkseqparse(char*);
extern	void		tkbezspline(Image*, Point*, int, int, Image*);
extern	void		tktopopt(Tk*, char*);
extern	void		tktopopt(Tk*, char*);
extern	void		tksendmsg(void*, char*);
extern	int		tktolimbo(TkVar*, char*);
extern	char*		tkskip(char*, char*);
extern	int		col2dec(TkEnv *,Image *);
extern	Tk*		tkinwindow(Tk*, Point);
extern	char*		tkbuttonconf(Tk*, char*, char**);
extern	char*		tklabelconf(Tk*, char*, char**);
extern	char*		tkbuttoninvoke(Tk*, char*, char**);
extern	void		tkdelpack(Tk*);
extern	void		tkimgput(TkImg*);
extern	int		tkischild(Tk*, Tk*);
extern	TkImg*		tkname2img(TkTop*, char*);
extern	void		tkcancel(TkAction**, int);
extern	TkVar*		tkmkvar(TkTop*, char*, int);
extern	void		tkfreevar(TkTop*, char*, int);
extern	int		tkismapped(Tk*);
extern	Image*		tkitmp(TkTop*, Point);
extern	void		tkfreeitmp(Display*);
extern	void		tktextoptfix(void);
extern	void		tkcvsupdate(Tk*);
extern	void		tktopimageptr(TkTop*, Image*);
extern	TkCtxt*		tkdeldepth(Tk*);
extern	char*		tkconflist(TkOptab*, char**);
extern	void		tkenterleave(TkCtxt*, int, int);
extern	void		tksizeimage(Tk*, TkImg*);
extern	TkCtxt*		tkattachctxt(Display*);
extern	TkCtxt*		tkscrn2ctxt(Screen*);
extern	void		tkdetachctxt(TkTop*);
extern	Image*		tkgc(TkEnv*, int);
extern	void		tkfreectag(TkCtag*);
extern	char*		tkinitscroll(Tk*);

/* Widget commands */
extern	char*	tklabelcget(Tk*, char*, char**);
extern	char*	tklabelconf(Tk*, char*, char**);
extern	char*	tkframecget(Tk*, char*, char**);
extern	char*	tkframeconf(Tk*, char*, char**);
extern	char*	tkframemap(Tk*, char*, char**);
extern	char*	tkframepost(Tk*, char*, char**);
extern	char*	tkframeunpost(Tk*, char*, char**);
extern	char*	tkframeunmap(Tk*, char*, char**);
extern	char*	tkbuttoncget(Tk*, char*, char**);
extern	char*	tkbuttoninvoke(Tk*, char*, char**);
extern	char*	tkbuttonconf(Tk*, char*, char**);
extern	char*	tkmenubutcget(Tk*, char*, char**);
extern	char*	tkmenubutconf(Tk*, char*, char**);
extern	char*	tkmenubutpost(Tk*, char*, char**);
extern	char*	tkMBpress(Tk*, char*, char**);
extern	char*	tkMBleave(Tk*, char*, char**);
extern	char*	tkMBrelease(Tk*, char*, char**);
extern	char*	tkbuttonselect(Tk*, char*, char**);
extern	char*	tkbuttondeselect(Tk*, char*, char**);
extern	char*	tkbuttontoggle(Tk*, char*, char**);
extern	char*	tkradioinvoke(Tk*, char*, char**);
extern	char*	tkmenuadd(Tk*, char*, char**);
extern	char*	tkmenucget(Tk*, char*, char**);
extern	char*	tkmenuconf(Tk*, char*, char**);
extern	char*	tkmenuinsert(Tk*, char*, char**);
extern	char*	tkmenuactivate(Tk*, char*, char**);
extern	char*	tkmenuinvoke(Tk*tk, char*, char**);
extern	char*	tkmenupost(Tk*, char*, char**);
extern	char*	tkmenuunpost(Tk*, char*, char**);
extern	char*	tkmenuindex(Tk*, char*, char**);
extern	char*	tkmenuyposn(Tk*, char*, char**);
extern	char*	tkmenudelete(Tk*, char*, char**);
extern	char*	tkmenupostcascade(Tk*, char*, char**);
extern	char*	tkmenutype(Tk*, char*, char**);
extern	char*	tkmenuentryconfig(Tk*, char*, char**);
extern	char*	tkmenuentrycget(Tk*, char*, char**);
extern	char*	tkMenuMotion(Tk*, char*, char**);
extern	char*	tkMenuButtonDn(Tk*, char*, char**);
extern	char*	tkMenuButtonUp(Tk*, char*, char**);
extern	char*	tkMenuButtonLostfocus(Tk*, char*, char**);
extern	char*	tkMenuAccel(Tk*, char*, char**);
extern	char*	tklistbactivate(Tk*, char*, char**);
extern	char*	tklistbcget(Tk*, char*, char**);
extern	char*	tklistbdelete(Tk*, char*, char**);
extern	char*	tklistbindex(Tk*, char*, char**);
extern	char*	tklistbinsert(Tk*, char*, char**);
extern	char*	tklistbnearest(Tk*, char*, char**);
extern	char*	tklistbselection(Tk*, char*, char**);
extern	char*	tklistbsize(Tk*, char*, char**);
extern	char*	tklistbxview(Tk*, char*, char**);
extern	char*	tklistbyview(Tk*, char*, char**);
extern	char*	tklistbcursel(Tk*, char*, char**);
extern	char*	tklistbconf(Tk*, char*, char**);
extern	char*	tklistbget(Tk*, char*, char**);
extern	char*	tklistbsee(Tk*, char*, char**);
extern	char*	tklistbbutton1(Tk*, char*, char**);
extern	char*	tklistbbutton1m(Tk*, char*, char**);
extern	char*	tkscrollconf(Tk*, char*, char**);
extern	char*	tkscrollactivate(Tk*, char*, char**);
extern	char*	tkscrollcget(Tk*, char*, char**);
extern	char*	tkscrolldelta(Tk*, char*, char**);
extern	char*	tkscrollfraction(Tk*, char*, char**);
extern	char*	tkscrollget(Tk*, char*, char**);
extern	char*	tkscrollidentify(Tk*, char*, char**);
extern	char*	tkscrollset(Tk*, char*, char**);
extern	char*	tkScrollDrag(Tk*, char*, char**);
extern	char*	tkScrolBut1P(Tk*, char*, char**);
extern	char*	tkScrolBut1R(Tk*, char*, char**);
extern	char*	tkScrolBut2P(Tk*, char*, char**);
extern	char* 	tktextbbox(Tk*, char*, char **);
extern	char* 	tktextbutton1(Tk*, char*, char **);
extern	char* 	tktextcget(Tk*, char*, char **);
extern	char* 	tktextcompare(Tk*, char*, char **);
extern	char* 	tktextconfigure(Tk*, char*, char **);
extern	char* 	tktextdebug(Tk*, char*, char **);
extern	char* 	tktextdelete(Tk*, char*, char **);
extern	char* 	tktextdelins(Tk*, char*, char **);
extern	char* 	tktextdlineinfo(Tk*, char*, char **);
extern	char* 	tktextdump(Tk*, char*, char **);
extern	char* 	tktextget(Tk*, char*, char **);
extern	char* 	tktextindex(Tk*, char*, char **);
extern	char* 	tktextinsert(Tk*, char*, char **);
extern	char* 	tktextinserti(Tk*, char*, char **);
extern	char* 	tktextmark(Tk*, char*, char **);
extern	char*	tktextscan(Tk*, char*, char**);
extern	char*	tktextscrollpages(Tk*, char*, char**);
extern	char*	tktextsearch(Tk*, char*, char**);
extern	char*	tktextsee(Tk*, char*, char**);
extern	char* 	tktextselectto(Tk*, char*, char **);
extern	char*	tktextsetcursor(Tk*, char*, char**);
extern	char*	tktexttag(Tk*, char*, char**);
extern	char*	tktextwindow(Tk*, char*, char**);
extern	char*	tktextxview(Tk*, char*, char**);
extern	char*	tktextyview(Tk*, char*, char**);
extern	char*	tktextcursor(Tk*, char*, char **);
extern	char* 	tkcvsbbox(Tk*, char*, char **);
extern	char* 	tkcvsbind(Tk*, char*, char **);
extern	char* 	tkcvscreate(Tk*, char*, char **);
extern	char* 	tkcvscanvx(Tk*, char*, char **);
extern	char* 	tkcvscanvy(Tk*, char*, char **);
extern	char* 	tkcvscoords(Tk*, char*, char **);
extern	char* 	tkcvsdelete(Tk*, char*, char **);
extern	char* 	tkcvsgettags(Tk*, char*, char **);
extern	char* 	tkcvsmove(Tk*, char*, char **);
extern	char* 	tkcvstype(Tk*, char*, char **);
extern	char* 	tkcvsyview(Tk*, char*, char **);
extern	char* 	tkcvsxview(Tk*, char*, char **);
extern	char* 	tkcvsraise(Tk*, char*, char **);
extern	char* 	tkcvslower(Tk*, char*, char **);
extern	char*	tkcvsdchars(Tk*, char*, char**);
extern	char*	tkcvsinsert(Tk*, char*, char**);
extern	char*	tkcvsindex(Tk*, char*, char**);
extern	char*	tkcvsfocus(Tk*, char*, char**);
extern	char*	tkcvsconf(Tk*, char*, char**);
extern	char*	tkcvscget(Tk*, char*, char**);
extern	char*	tkcvsscale(Tk*, char*, char**);
extern	char*	tkcvsitemcget(Tk*, char*, char**);
extern	char*	tkcvsitemconf(Tk*, char*, char**);
extern	char*	tkcvsicursor(Tk*, char*, char**);
extern	char*	tkcvsselect(Tk*, char*, char**);
extern	char*	tkcvsaddtag(Tk*, char*, char**);
extern	char*	tkcvsfind(Tk*, char*, char**);
extern	char*	tkentrycget(Tk*, char*, char**);
extern	char*	tkentryconf(Tk*, char*, char**);
extern	char*	tkentryget(Tk*, char*, char**);
extern	char*	tkentryinsert(Tk*, char*, char**);
extern	char*	tkentryindex(Tk*, char*, char**);
extern	char*	tkentrydelete(Tk*, char*, char**);
extern	char*	tkentryicursor(Tk*, char*, char**);
extern	char*	tkentryxview(Tk*, char*, char**);
extern	char*	tkentryselect(Tk*, char*, char**);
extern	char*	tkentrybs(Tk*, char*, char**);
extern	char*	tkentrybw(Tk*, char*, char**);
extern	char*	tkscalecget(Tk*, char*, char**);
extern	char*	tkscaleconf(Tk*, char*, char**);
extern	char*	tkscaleset(Tk*, char*, char**);
extern	char*	tkscaleget(Tk*, char*, char**);
extern	char*	tkscaleident(Tk*, char*, char**);
extern	char*	tkscalecoords(Tk*, char*, char**);
extern	char*	tkscalemotion(Tk*, char*, char**);
extern	char*	tkscaledrag(Tk*, char*, char**);
extern	char*	tkscalebut1p(Tk*, char*, char**);
extern	char*	tkscalebut1r(Tk*, char*, char**);
extern	char*	tkcvslinecreat(Tk*, char *arg, char **val);
extern	void	tkcvslinedraw(Image*, TkCitem*);
extern	void	tkcvslinefree(TkCitem*);
extern	char*	tkcvslinecoord(TkCitem*, char*, int, int);
extern	char*	tkcvslinecget(TkCitem*, char*, char**);
extern	char*	tkcvslineconf(Tk*, TkCitem*, char*);
extern	char*	tkcvstextcreat(Tk*, char *arg, char **val);
extern	void	tkcvstextdraw(Image*, TkCitem*);
extern	void	tkcvstextfree(TkCitem*);
extern	char*	tkcvstextcoord(TkCitem*, char*, int, int);
extern	char*	tkcvstextcget(TkCitem*, char*, char**);
extern	char*	tkcvstextconf(Tk*, TkCitem*, char*);
extern	char*	tkcvsrectcreat(Tk*, char *arg, char **val);
extern	void	tkcvsrectdraw(Image*, TkCitem*);
extern	void	tkcvsrectfree(TkCitem*);
extern	char*	tkcvsrectcoord(TkCitem*, char*, int, int);
extern	char*	tkcvsrectcget(TkCitem*, char*, char**);
extern	char*	tkcvsrectconf(Tk*, TkCitem*, char*);
extern	char*	tkcvsovalcreat(Tk*, char *arg, char **val);
extern	void	tkcvsovaldraw(Image*, TkCitem*);
extern	void	tkcvsovalfree(TkCitem*);
extern	char*	tkcvsovalcoord(TkCitem*, char*, int, int);
extern	char*	tkcvsovalcget(TkCitem*, char*, char**);
extern	char*	tkcvsovalconf(Tk*, TkCitem*, char*);
extern	char*	tkcvsarccreat(Tk*, char *arg, char **val);
extern	void	tkcvsarcdraw(Image*, TkCitem*);
extern	void	tkcvsarcfree(TkCitem*);
extern	char*	tkcvsarccoord(TkCitem*, char*, int, int);
extern	char*	tkcvsarccget(TkCitem*, char*, char**);
extern	char*	tkcvsarcconf(Tk*, TkCitem*, char*);
extern	char*	tkcvsbitcreat(Tk*, char *arg, char **val);
extern	void	tkcvsbitdraw(Image*, TkCitem*);
extern	void	tkcvsbitfree(TkCitem*);
extern	char*	tkcvsbitcoord(TkCitem*, char*, int, int);
extern	char*	tkcvsbitcget(TkCitem*, char*, char**);
extern	char*	tkcvsbitconf(Tk*, TkCitem*, char*);
extern	char*	tkcvswindcreat(Tk*, char *arg, char **val);
extern	void	tkcvswinddraw(Image*, TkCitem*);
extern	void	tkcvswindfree(TkCitem*);
extern	char*	tkcvswindcoord(TkCitem*, char*, int, int);
extern	char*	tkcvswindcget(TkCitem*, char*, char**);
extern	char*	tkcvswindconf(Tk*, TkCitem*, char*);
extern	char*	tkcvspolycreat(Tk*, char *arg, char **val);
extern	void	tkcvspolydraw(Image*, TkCitem*);
extern	void	tkcvspolyfree(TkCitem*);
extern	char*	tkcvspolycoord(TkCitem*, char*, int, int);
extern	char*	tkcvspolycget(TkCitem*, char*, char**);
extern	char*	tkcvspolyconf(Tk*, TkCitem*, char*);
extern	char*	tkcvsimgcreat(Tk*, char *arg, char **val);
extern	void	tkcvsimgdraw(Image*, TkCitem*);
extern	void	tkcvsimgfree(TkCitem*);
extern	char*	tkcvsimgcoord(TkCitem*, char*, int, int);
extern	char*	tkcvsimgcget(TkCitem*, char*, char**);
extern	char*	tkcvsimgconf(Tk*, TkCitem*, char*);
extern	void	tksorttable(void);
