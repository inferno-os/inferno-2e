#include "lib9.h"
#include "image.h"
#include "tk.h"

#define	O(t, e)		((long)(&((t*)0)->e))

/*
 * Here are some conventions about  our Tk widgets.
 *
 * When a widget is packed, its act geom record is
 * set so that act.{x,y} is the vector from the containing
 * widget's origin to the position of this widget.  The position
 * is the place just outside the top-left border.  The origin
 * is the place just inside the top-left border.
 * act.{width,height} gives the allocated dimensions inside
 * the border --- it will be the requested width or height
 * plus ipad{x,y} plus any filling done by the packer.
 *
 * The tkposn function returns the origin of its argument
 * widget, expressed in absolute screen coordinates.
 *
 * The actual drawing contents of the widget should be
 * drawn at an internal origin that is the widget's origin
 * plus the ipad vector.
 */

TkStab tkorient[] =
{
	"vertical",	Tkvertical,
	"horizontal",	Tkhorizontal,
	nil
};

#define RGB(r,g,b) ((r<<16)|(g<<8)|(b))

TkStab tkcolortab[] =
{
	"black",	RGB(0,0,0),
	"blue",		RGB(0,0,204),
	"darkblue",	RGB(93,0,187),
	"red",		RGB(255,0,0),
	"yellow",	RGB(255,255,0),
	"green",	RGB(0,128,0),
	"white",	RGB(255,255,255),
	"orange",	RGB(255,170,0),
	"aqua",		RGB(0,255,255),
	"fuchsia",	RGB(255,0,255),
	"gray",		RGB(128,128,128),
	"lime",		RGB(0,255,0),
	"maroon",	RGB(128,0,0),
	"navy",		RGB(0,0,128),
	"olive",	RGB(128,128,0),
	"purple",	RGB(128,0,128),
	"silver",	RGB(192,192,192),
	"teal",		RGB(0,128,128),
	nil
};

TkStab tklines[] =
{
	"none",		0,
	"first",	TkCarrowf,
	"last",		TkCarrowl,
	"both",		TkCarrowf|TkCarrowl,
	nil
};

TkStab tkrelief[] =
{
	"raised",	TKraised,
	"sunken",	TKsunken,
	"flat",		TKflat,
	"groove",	TKgroove,
	"ridge",	TKridge,
	nil
};

TkStab tkbool[] =
{
	"0",		BoolF,
	"no",		BoolF,
	"off",		BoolF,
	"false",	BoolF,
	"1",		BoolT,
	"yes",		BoolT,
	"on",		BoolT,
	"true",		BoolT,
	nil
};

TkStab tkanchor[] =
{
	"center",	Tkcenter,
	"n",		Tknorth,
	"ne",		Tknortheast,
	"e",		Tkeast,
	"se",		Tksoutheast,
	"s",		Tksouth,
	"sw",		Tksouthwest,
	"w",		Tkwest,
	"nw",		Tknorthwest,
	nil
};

TkStab tkside[] =
{
	"top",		Tktop,
	"bottom",	Tkbottom,
	"left",		Tkleft,
	"right",	Tkright,
	nil
};

TkStab tkfill[] =
{
	"none",		0,
	"x",		Tkfillx,
	"y",		Tkfilly,
	"both",		Tkfillx|Tkfilly,
	nil
};

TkStab tkstate[] =
{
	"normal",	0,
	"active",	Tkfocus,
	"disabled",	Tkdisabled,
	nil
};

TkStab tktabjust[] =
{
	"left",		Tkleft,
	"right",	Tkright,
	"center",	Tkcenter,
	"numeric",	Tknumeric,	
	nil
};

TkStab tkwrap[] =
{
	"none",		Tkwrapnone,
	"word",		Tkwrapword,
	"char",		Tkwrapchar,
	nil
};

TkStab tkjustify[] =
{
	"left",		Tkleft,
	"right",	Tkright,
	"center",	Tkcenter,
	nil
};

TkStab tkcompare[] =
{
	"<",		TkLt,
	"<=",		TkLte,
	"==",		TkEq,
	">=",		TkGte,
	">",		TkGt,
	"!=",		TkNeq,
	nil
};

TkStab tkalign[] =
{
	"top",		Tktop,
	"bottom",	Tkbottom,
	"center",	Tkcenter,
	"baseline",	Tkbaseline,
	nil
};

TkOption tkgeneric[] =
{
 "actwidth",		OPTdist, O(Tk, act.width),	IAUX(O(Tk, env)),
 "actheight",		OPTdist, O(Tk, act.height),	IAUX(O(Tk, env)),
 "bd",			OPTdist, O(Tk, borderwidth),	nil,
 "borderwidth",		OPTdist, O(Tk, borderwidth),	nil,
 "selectborderwidth",	OPTdist, O(Tk, sborderwidth),	nil,
 "height",		OPTsize, 0,			IAUX(O(Tk, env)),
 "width",		OPTsize, 0,			IAUX(O(Tk, env)),
 "relief",		OPTstab, O(Tk, relief),		tkrelief,
 "state",		OPTflag, O(Tk, flag),		tkstate,
 "font",		OPTfont, O(Tk, env),		nil,
 "foreground",		OPTcolr, O(Tk, env),		IAUX(TkCforegnd),
 "background",		OPTcolr, O(Tk, env),		IAUX(TkCbackgnd),
 "fg",			OPTcolr, O(Tk, env),		IAUX(TkCforegnd),
 "bg",			OPTcolr, O(Tk, env),		IAUX(TkCbackgnd),
 "bgimage",		OPTevim, O(Tk, env),		IAUX(TkCbackgnd),
 "fgimage",		OPTevim, O(Tk, env),		IAUX(TkCforegnd),
 "selectcolor",		OPTcolr, O(Tk, env),		IAUX(TkCselect),
 "selectforeground",	OPTcolr, O(Tk, env),		IAUX(TkCselectfgnd),
 "selectbackground",	OPTcolr, O(Tk, env),		IAUX(TkCselectbgnd),
 "activeforeground",	OPTcolr, O(Tk, env),		IAUX(TkCactivefgnd),
 "activebackground",	OPTcolr, O(Tk, env),		IAUX(TkCactivebgnd),
 "padx",		OPTdist, O(Tk, pad.x),		nil,
 "pady",		OPTdist, O(Tk, pad.y),		nil,
 nil
};

TkOption tktop[] =
{
	"x",		OPTdist,	O(Tk, act.x),		nil,
	"y",		OPTdist,	O(Tk, act.y),		nil,
	nil
};

TkOption tktopdbg[] =
{
	"debug",	OPTbool,	O(TkTop, debug),	nil,
	nil
};

Point		tkzp;
Rectangle	bbnil = { 1000000, 1000000, -1000000, -1000000 };
Rectangle	huger = { -1000000, -1000000, 1000000, 1000000 };
int		cvslshape[] = { TKI2F(8), TKI2F(10), TKI2F(3) };
Tk*		tkdepth;
TkTop*		tkwindows;

TkCmdtab tklabelcmd[] =
{
	"cget",			tklabelcget,
	"configure",		tklabelconf,
	nil
};

TkCmdtab tkframecmd[] =
{
	"cget",			tkframecget,
	"configure",		tkframeconf,
	"map",			tkframemap,
	"unmap",		tkframeunmap,
	"post",			tkframepost,
	"unpost",		tkframeunpost,
	nil
};

TkCmdtab tkbuttoncmd[] =
{
	"cget",			tkbuttoncget,
	"configure",		tkbuttonconf,
	"invoke",		tkbuttoninvoke,
	nil
};

TkCmdtab tkmenubutcmd[] =
{
	"cget",			tkmenubutcget,
	"configure",		tkmenubutconf,
	"menupost",		tkmenubutpost,
	"tkMBpress",		tkMBpress,
	"tkMBrelease",		tkMBrelease,
	"tkMBleave",		tkMBleave,
	nil
};

TkCmdtab tkchkbuttoncmd[] =
{
	"cget",			tkbuttoncget,
	"configure",		tkbuttonconf,
	"invoke",		tkbuttoninvoke,
	"select",		tkbuttonselect,
	"deselect",		tkbuttondeselect,
	"toggle",		tkbuttontoggle,
	nil
};

TkCmdtab tkradbuttoncmd[] =
{
	"cget",			tkbuttoncget,
	"configure",		tkbuttonconf,
	"invoke",		tkradioinvoke,
	"select",		tkbuttonselect,
	"deselect",		tkbuttondeselect,
	nil
};

TkCmdtab tkmenucmd[] =
{
	"activate",		tkmenuactivate,
	"add",			tkmenuadd,
	"cget",			tkmenucget,
	"configure",		tkmenuconf,
	"delete",		tkmenudelete,
	"entryconfigure",	tkmenuentryconfig,
	"entrycget",		tkmenuentrycget,
	"index",		tkmenuindex,
	"insert",		tkmenuinsert,
	"invoke",		tkmenuinvoke,
	"post",			tkmenupost,
	"postcascade",		tkmenupostcascade,
	"type",			tkmenutype,
	"unpost",		tkmenuunpost,
	"yposition",		tkmenuyposn,
	"tkMenuMotion",		tkMenuMotion,
	"tkMenuButtonDn",	tkMenuButtonDn,
	"tkMenuButtonUp",	tkMenuButtonUp,
	"tkMenuButtonLostfocus",tkMenuButtonLostfocus,
	"tkMenuAccel",		tkMenuAccel,
	nil
};

TkCmdtab tklistcmd[] =
{
	"activate",		tklistbactivate,
	"cget",			tklistbcget,
	"configure",		tklistbconf,
	"curselection",		tklistbcursel,
	"delete",		tklistbdelete,
	"get",			tklistbget,
	"index",		tklistbindex,
	"insert",		tklistbinsert,
	"nearest",		tklistbnearest,
	"selection",		tklistbselection,
	"see",			tklistbsee,
	"size",			tklistbsize,
	"xview",		tklistbxview,
	"yview",		tklistbyview,
	"tkListbButton1P",	tklistbbutton1,
	"tkListbButton1MP",	tklistbbutton1m,
	nil
};

TkCmdtab tkscrlbcmd[] =
{
	"activate",		tkscrollactivate,
	"cget",			tkscrollcget,
	"configure",		tkscrollconf,
	"delta",		tkscrolldelta,
	"fraction",		tkscrollfraction,
	"get",			tkscrollget,
	"identify",		tkscrollidentify,
	"set",			tkscrollset,
	"tkScrollDrag",		tkScrollDrag,
	"tkScrolBut1P",		tkScrolBut1P,
	"tkScrolBut1R",		tkScrolBut1R,
	"tkScrolBut2P",		tkScrolBut2P,
	nil
};

TkCmdtab tktextcmd[] =
{
	"bbox",			tktextbbox,
	"cget",			tktextcget,
	"compare",		tktextcompare,
	"configure",		tktextconfigure,
	"debug",		tktextdebug,
	"delete",		tktextdelete,
	"dlineinfo",		tktextdlineinfo,
	"dump",			tktextdump,
	"get",			tktextget,
	"index",		tktextindex,
	"insert",		tktextinsert,
	"mark",			tktextmark,
	"scan",			tktextscan,
	"search",		tktextsearch,
	"see",			tktextsee,
	"selection",		tktextselection,
	"tag",			tktexttag,
	"window",		tktextwindow,
	"xview",		tktextxview,
	"yview",		tktextyview,
	"tkTextButton1",	tktextbutton1,
	"tkTextDelIns",		tktextdelins,
	"tkTextInsert",		tktextinserti,
	"tkTextSelectTo",	tktextselectto,
	"tkTextSetCursor",	tktextsetcursor,
	"tkTextScrollPages",	tktextscrollpages,
	"tkTextCursor",		tktextcursor,
	nil
};

TkCmdtab tkcanvcmd[] =
{
	"addtag",		tkcvsaddtag,
	"bbox",			tkcvsbbox,
	"bind",			tkcvsbind,
	"cget",			tkcvscget,
	"configure",		tkcvsconf,
	"create",		tkcvscreate,
	"canvasx",		tkcvscanvx,
	"canvasy",		tkcvscanvy,
	"coords",		tkcvscoords,
	"dchars",		tkcvsdchars,
	"delete",		tkcvsdelete,
	"dtag",			tkcvsdtag,
	"find",			tkcvsfind,
	"focus",		tkcvsfocus,
	"gettags",		tkcvsgettags,
	"icursor",		tkcvsicursor,
	"insert",		tkcvsinsert,
	"index",		tkcvsindex,
	"itemcget",		tkcvsitemcget,
	"itemconfigure",	tkcvsitemconf,
	"lower",		tkcvslower,
	"move",			tkcvsmove,
	"raise",		tkcvsraise,
	"select",		tkcvsselect,
	"scale",		tkcvsscale,
	"type",			tkcvstype,
	"yview",		tkcvsyview,
	"xview",		tkcvsxview,
	nil
};

TkCmdtab tkentrycmd[] =
{
	"cget",			tkentrycget,
	"configure",		tkentryconf,
	"delete",		tkentrydelete,
	"get",			tkentryget,
	"icursor",		tkentryicursor,
	"index",		tkentryindex,
	"insert",		tkentryinsert,
	"selection",		tkentryselect,
	"xview",		tkentryxview,
	"tkEntryBS",		tkentrybs,
	"tkEntryBW",		tkentrybw,
	nil
};

TkCmdtab tkscalecmd[] =
{
	"cget",			tkscalecget,
	"configure",		tkscaleconf,
	"set",			tkscaleset,
	"identify",		tkscaleident,
	"get",			tkscaleget,
	"coords",		tkscalecoords,
	"tkScaleMotion",	tkscalemotion,
	"tkScaleDrag",		tkscaledrag,
	"tkScaleBut1P",		tkscalebut1p,
	"tkScaleBut1R",		tkscalebut1r,
	nil
};

TkMethod tkmethod[] =
{
	{
				"frame",
	/* TKframe */		tkfreeframe,
				tkdrawframe,
				nil,
				tkframecmd
	},

	{
				"label",
	/* TKlabel */		tkfreelabel,
				tkdrawlabel,
				nil,
				tklabelcmd,
	},

	{
				"checkbutton",
	/* TKcheckbutton */	tkfreelabel,
				tkdrawlabel,
				nil,
				tkchkbuttoncmd
	},

	{
				"button",
	/* TKbutton */		tkfreelabel,
				tkdrawlabel,
				nil,
				tkbuttoncmd
	},

	{
				"menubutton",
	/* TKmenubutton */	tkfreemenub,
				tkdrawlabel,
				nil,
				tkmenubutcmd
	},

	{
				"menu",
	/* TKmenu */		tkfreemenu,
				tkdrawframe,
				nil,
				tkmenucmd
	},

	{
				"separator",
	/* TKseparator */	tkfreeframe,
				tkdrawframe,
				nil,
				nil
	},

	{
				"cascade",
	/* TKcascade */		tkfreemenub,
				tkdrawlabel,
				nil,
				nil
	},

	{
				"listbox",
	/* TKlistbox */		tkfreelistb,
				tkdrawlistb,
				tklistbgeom,
				tklistcmd
	},

	{
				"scrollbar",
	/* TKscrollbar */	tkfreescrlb,
				tkdrawscrlb,
				nil,
				tkscrlbcmd
	},

	{
				"text",
	/* TKtext */		tkfreetext,
				tkdrawtext,
				tktextgeom,
				tktextcmd
	},

	{
				"canvas",
	/* TKcanvas */		tkfreecanv,
				tkdrawcanv,
				tkcvsgeom,
				tkcanvcmd
	},

	{
				"entry",
	/* TKentry */		tkfreeentry,
				tkdrawentry,
				tkentrygeom,
				tkentrycmd
	},

	{
				"radiobutton",
	/* TKradiobutton */	tkfreelabel,
				tkdrawlabel,
				nil,
				tkradbuttoncmd
	},

	{
				"scale",
	/* TKscale */		tkfreescale,
				tkdrawscale,
				nil,
				tkscalecmd
	},
};

TkCimeth tkcimethod[] =
{
	"line",		tkcvslinecreat,
			tkcvslinedraw,
			tkcvslinefree,
			tkcvslinecoord,
			tkcvslinecget,
			tkcvslineconf,

	"text",		tkcvstextcreat,	
			tkcvstextdraw,
			tkcvstextfree,
			tkcvstextcoord,
			tkcvstextcget,
			tkcvstextconf,

	"rectangle",	tkcvsrectcreat,	
			tkcvsrectdraw,
			tkcvsrectfree,
			tkcvsrectcoord,
			tkcvsrectcget,
			tkcvsrectconf,

	"oval",		tkcvsovalcreat,	
			tkcvsovaldraw,
			tkcvsovalfree,
			tkcvsovalcoord,
			tkcvsovalcget,
			tkcvsovalconf,

	"bitmap",	tkcvsbitcreat,	
			tkcvsbitdraw,
			tkcvsbitfree,
			tkcvsbitcoord,
			tkcvsbitcget,
			tkcvsbitconf,

	"polygon",	tkcvspolycreat,	
			tkcvspolydraw,
			tkcvspolyfree,
			tkcvspolycoord,
			tkcvspolycget,
			tkcvspolyconf,

	"window",	tkcvswindcreat,	
			tkcvswinddraw,
			tkcvswindfree,
			tkcvswindcoord,
			tkcvswindcget,
			tkcvswindconf,

	"image",	tkcvsimgcreat,	
			tkcvsimgdraw,
			tkcvsimgfree,
			tkcvsimgcoord,
			tkcvsimgcget,
			tkcvsimgconf,

	"arc",		tkcvsarccreat,	
			tkcvsarcdraw,
			tkcvsarcfree,
			tkcvsarccoord,
			tkcvsarccget,
			tkcvsarcconf,
	nil
};

char TkNomem[]	= "!out of memory";
char TkBadop[]	= "!bad option";
char TkOparg[]	= "!arg requires option";
char TkBadvl[]	= "!bad value";
char TkBadwp[]	= "!bad window path";
char TkWpack[]	= "!window is already packed";
char TkNotop[]	= "!no toplevel";
char TkDupli[]  = "!window path already exists";
char TkNotpk[]	= "!window not packed";
char TkBadcm[]	= "!bad command";
char TkIstop[]	= "!can't pack top level";
char TkBadbm[]	= "!failed to load bitmap";
char TkBadft[]	= "!failed to open font";
char TkBadit[]	= "!bad item type";
char TkBadtg[]	= "!bad/no matching tag";
char TkFewpt[]	= "!wrong number of points";
char TkBadsq[]	= "!bad event sequence";
char TkBadix[]	= "!bad index";
char TkNotwm[]	= "!not a window";
char TkBadvr[]	= "!variable does not exist";
char TkNotvt[]	= "!variable is wrong type";
char TkMovfw[]	= "!too many events buffered";
char TkBadsl[]	= "!selection already exists";
char TkSyntx[]	= "!bad [] or {} syntax";

Tk*	tkMgrab;	/* Widget holding onto mouse focus */
Tk*	tkKgrab;	/* Widget holding onto keyboard focus */
