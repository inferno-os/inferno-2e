implement Layout;

include "common.m";
#####################international
include "charon_code.m";
        charonCode: Charon_code;
include "charon_gui.m";
	charon_gui: Charon_gui;

sys: Sys;
CU: CharonUtils;
	ByteSource, MaskedImage, CImage, ImageCache,
	White, Black, Grey, DarkGrey, LightGrey, Blue, Navy, Red, DarkRed: import CU;

D: Draw;
	Point, Rect, Font, Image, Display: import D;
S: String;
T: StringIntTab;
U: Url;
	ParsedUrl: import U;
I: Img;
	ImageSource: import I;
J: Script;
E: Events;
	Event: import E;
G: Gui;
B: Build;

# B : Build, declared in layout.m so main program can use it
	Item, ItemSource,
	IFbrk, IFbrksp, IFnobrk, IFcleft, IFcright, IFwrap, IFhang,
	IFrjust, IFcjust, IFsmap, IFindentshift, IFindentmask,
	IFhangmask,
	Voffbias,
	ISPnull, ISPvline, ISPhspace, ISPgeneral,
	Align, Dimen, Formfield, Option, Form,
	Table, Tablecol, Tablerow, Tablecell,
	Anchor, DestAnchor, Map, Area, Kidinfo, Docinfo,
	Anone, Aleft, Acenter, Aright, Ajustify, Achar, Atop, Amiddle,
	Abottom, Abaseline,
	Dnone, Dpixels, Dpercent, Drelative,
	Ftext, Fpassword, Fcheckbox, Fradio, Fsubmit, Fhidden, Fimage,
	Freset, Ffile, Fbutton, Fselect, Ftextarea,
	Background,
	FntR, FntI, FntB, FntT, NumStyle,
	Tiny, Small, Normal, Large, Verylarge, NumSize, NumFnt, DefFnt,
	ULnone, ULunder, ULmid,
	FRnoresize, FRnoscroll, FRhscroll, FRvscroll,
	FRhscrollauto, FRvscrollauto
    : import B;


# font stuff
#Fontinfo : adt {
#	name:	string;
#	f:	ref Font;
#	spw:	int;			# width of a space in this font
#};

#fonts := array[NumFnt] of { Fontinfo
#	("/fonts/lucidasans/unicode.6.font", nil, 0),
#	("/fonts/lucidasans/unicode.7.font", nil, 0),
#	("/fonts/lucidasans/unicode.8.font", nil, 0),
#	("/fonts/lucidasans/unicode.10.font", nil, 0),
#	("/fonts/lucidasans/unicode.13.font", nil, 0),
#	("/fonts/lucidasans/italicunicode.6.font", nil, 0),
#	("/fonts/lucidasans/italicunicode.7.font", nil, 0),
#	("/fonts/lucidasans/italicunicode.8.font", nil, 0),
#	("/fonts/lucidasans/italicunicode.10.font", nil, 0),
#	("/fonts/lucidasans/italicunicode.13.font", nil, 0),
#	("/fonts/lucidasans/boldunicode.6.font", nil, 0),
#	("/fonts/lucidasans/boldunicode.7.font", nil, 0),
#	("/fonts/lucidasans/boldunicode.8.font", nil, 0),
#	("/fonts/lucidasans/boldunicode.10.font", nil, 0),
#	("/fonts/lucidasans/boldunicode.13.font", nil, 0),
#	("/fonts/lucidasans/typeunicode.6.font", nil, 0),
#	("/fonts/lucidasans/typeunicode.7.font", nil, 0),
#	("/fonts/lucidasans/typeunicode.9.font", nil, 0),
#	("/fonts/lucidasans/typeunicode.12.font", nil, 0),
#	("/fonts/lucidasans/typeunicode.16.font", nil, 0)
#};

# Seems better to use a slightly smaller font in Controls, to match other browsers
#CtlFnt: con (FntR*NumSize+Small);

# color stuff.  have hash table mapping RGB values to D->Image for that color
Colornode : adt {
	rgb:	int;
	im:	ref Image;
	next:	ref Colornode;
};

# Source of info for page (html, image, etc.)
Source: adt {
	bs:	ref ByteSource;
	redirects:	int;
	pick {
		Shtml =>
			itsrc: ref ItemSource;
		Simage =>
			ci: ref CImage;
			itl: list of ref Item;
			imsrc: ref ImageSource;
	}
};

Sources: adt {
	srcs: list of ref Source;
	tot: int;
	done: int;

	add: fn(srcs: self ref Sources, s: ref Source);
	findbs: fn(srcs: self ref Sources, bs: ref ByteSource) : ref Source;
};

NCOLHASH : con 19;	# 19 checked for standard colors: only 1 collision
colorhashtab := array[NCOLHASH] of ref Colornode;

# No line break should happen between adjacent characters if
# they are 'wordchars' : set in this array, or outside the array range.
# We include certain punctuation characters that are not traditionally
# regarded as 'word' characters.
wordchar := array[16rA0] of {
	'0'=>byte 1, '1'=>byte 1, '2'=>byte 1, '3'=>byte 1, '4'=>byte 1,
	'5'=>byte 1, '6'=>byte 1, '7'=>byte 1, '8'=>byte 1, '9'=>byte 1,
	'A'=>byte 1, 'B'=>byte 1, 'C'=>byte 1, 'D'=>byte 1, 'E'=>byte 1, 'F'=>byte 1,
	'G'=>byte 1, 'H'=>byte 1, 'I'=>byte 1, 'J'=>byte 1, 'K'=>byte 1, 'L'=>byte 1,
	'M'=>byte 1, 'N'=>byte 1, 'O'=>byte 1, 'P'=>byte 1, 'Q'=>byte 1, 'R'=>byte 1,
	'S'=>byte 1, 'T'=>byte 1, 'U'=>byte 1, 'V'=>byte 1, 'W'=>byte 1, 'X'=>byte 1,
	'Y'=>byte 1, 'Z'=>byte 1,
	'a'=>byte 1, 'b'=>byte 1, 'c'=>byte 1, 'd'=>byte 1, 'e'=>byte 1, 'f'=>byte 1,
	'g'=>byte 1, 'h'=>byte 1, 'i'=>byte 1, 'j'=>byte 1, 'k'=>byte 1, 'l'=>byte 1,
	'm'=>byte 1, 'n'=>byte 1, 'o'=>byte 1, 'p'=>byte 1, 'q'=>byte 1, 'r'=>byte 1,
	's'=>byte 1, 't'=>byte 1, 'u'=>byte 1, 'v'=>byte 1, 'w'=>byte 1, 'x'=>byte 1,
	'y'=>byte 1, 'z'=>byte 1,
	'_'=>byte 1,
	'\''=>byte 1, '"'=>byte 1, '.'=>byte 1, ','=>byte 1, '('=>byte 1, ')'=>byte 1,
	* => byte 0
};

TABPIX: con 30;		# number of pixels in a tab
CAPSEP: con 5;			# number of pixels separating tab from caption
SCRBREADTH: con 14;	# scrollbar breadth (normal)
SCRFBREADTH: con 14;	# scrollbar breadth (inside child frame or select control)
FRMARGIN: con 10;		# default margin around frames
RULESP: con 7;			# extra space before and after rules

# all of the following include room for relief
CBOXWID: con 14;		# check box width
CBOXHT: con 12;		# check box height
ENTVMARGIN : con 4;	# vertical margin inside entry box
ENTHMARGIN : con 6;	# horizontal margin inside entry box
SELMARGIN : con 4;		# margin inside select control
BUTMARGIN: con 4;		# margin inside button control
PBOXWID: con 16;		# progress box width
PBOXHT: con 16;		# progress box height

TABLEMAXTARGET: con 2000;	# targetwidth to get max width of table cell
TABLEFLOATTARGET: con 600;	# targetwidth for floating tables

zp := Point(0,0);

display: ref D->Display;

dbg := 0;
dbgtab := 0;
dbgev := 0;
linespace := 0;
lineascent := 0;
charspace := 0;
spspace := 0;
ctllinespace := 0;
ctllineascent := 0;
ctlcharspace := 0;
ctlspspace := 0;
frameid := 0;

SELBG: con 16r00FFFF;	# aqua

init(cu: CharonUtils)
{
	CU = cu;
	sys = load Sys Sys->PATH;
	D = load Draw Draw->PATH;
	S = load String String->PATH;
	T = load StringIntTab StringIntTab->PATH;
	U = load Url Url->PATH;
	E = cu->E;
	G = cu->G;
	I = cu->I;
	J = cu->J;
	B = cu->B;
	display = G->display;

	charonCode=load Charon_code Charon_code->PATH;
	if(charonCode==nil)
		cu->raise(sys->sprint("EXinternal:couldn't load Charon_code:%r"));
	charonCode->init();
	charon_gui=load Charon_gui Charon_gui->PATH;
	if(charon_gui==nil)
		cu->raise(sys->sprint("EXinternal:couldn't load Charon_gui:%r"));
	charon_gui->init();
	# make sure default and control fonts are loaded
	getfont(DefFnt);
	fnt := charon_gui->fonts[DefFnt].f;
	linespace = fnt.height;
	lineascent = fnt.ascent;
	charspace = fnt.width("a");	# a kind of average char width
	spspace = charon_gui->fonts[DefFnt].spw;
	getfont(charon_gui->CtlFnt);
	fnt = charon_gui->fonts[charon_gui->CtlFnt].f;
	ctllinespace = fnt.height;
	ctllineascent = fnt.ascent;
	ctlcharspace = fnt.width("a");
	ctlspspace = charon_gui->fonts[charon_gui->CtlFnt].spw;
	initrepeater();
}

stringwidth(s: string): int
{
	return charon_gui->fonts[DefFnt].f.width(s)/charspace;
}

# Use bsmain to fill frame f.
# Return buffer containing source when done.
layout(f: ref Frame, bsmain: ref ByteSource, linkclick: int) : array of byte
{
	dbg = int (CU->config).dbg['l'];
	dbgtab = int (CU->config).dbg['t'];
	dbgev = int (CU->config).dbg['e'];
	if(dbgev)
		CU->event("LAYOUT", 0);
	sources := ref Sources(nil, 0, 0);
	hdr := bsmain.hdr;
	auth := bsmain.req.auth;
	ans : array of byte = nil;
	di := Docinfo.new();
	if(linkclick && f.doc != nil)
		di.referrer = f.doc.src;
	f.reset();
	f.doc = di;
	di.frameid = f.id;
	di.src = hdr.actual;
	di.base = hdr.base;
	di.refresh = hdr.refresh;
	di.chset = hdr.chset;
	di.lastModified = hdr.lastModified;
	if(J != nil)
		J->havenewdoc(f);
	if(f.framebd != 0) {
		f.cr = f.r.inset(2);
		drawborder(f.cim, f.cr, 2, DarkGrey);
	}
	fillbg(f, f.cr);
	flushc(f);
	if(f.flags&FRvscroll)
		createvscroll(f);
	if(f.flags&FRhscroll)
		createhscroll(f);
	l := Lay.new(f.cr.dx(), Aleft, f.marginw, di.background);
	f.layout = l;
	anyanim := 0;
	if(hdr.mtype == CU->TextHtml || hdr.mtype == CU->TextPlain) {
		itsrc := ItemSource.new(bsmain, f, hdr.mtype);
		sources.add(ref Source.Shtml(bsmain, 0, itsrc));
	}
	else {
		# for now, must be supported image type
		if(!I->supported(hdr.mtype)) {
			sys->print("Need to implemented something: source isn't supported image type\n");
			return nil;
		}
		imsrc := I->ImageSource.new(bsmain, 0, 0);
		ci := CImage.new(bsmain.req.url, nil, 0, 0);
		simage := ref Source.Simage(bsmain, 0, ci, nil, imsrc);
		sources.add(simage);
		it := ref Item.Iimage(nil, 0, 0, 0, 0, 0, nil, len di.images, ci, 0, 0, "", nil, nil, -1, Abottom, byte 0, byte 0, byte 0);
		di.images = it :: nil;
		appenditems(f, l, it);
		simage.itl = it :: nil;
	}
	while(sources.done < sources.tot) {
		if(dbgev)
			CU->event("LAYOUT GETSOMETHING", 0);
		bs := CU->waitreq();
		if(bs.refgo == 0)
			# we freed this bs after this response to waitreq was queued
			continue;
		src := sources.findbs(bs);
		if(src == nil)
			continue;	# maybe previous freebs hasn't happened yet?
		freeit := 0;
		if(bs.err != "") {
			if(dbg)
				sys->print("error getting something: %s\n", bs.err);
			freeit = 1;
		}
		else {
			if(bs.hdr != nil && !bs.seenhdr) {
				(use, error, challenge, newurl) := CU->hdraction(bs, 0, src.redirects);
				if(challenge != nil) {
					sys->print("Need to implement authorization credential dialog\n");
					error = "Need authorization";
					use = 0;
				}
				if(error != "" && dbg)
					sys->print("subordinate error: %s\n", error);
				if(newurl != nil) {
					s := ref *src;
					freeit = 1;
					pick ps := src {
					Shtml =>
						sys->print("unexpected redirect of subord\n");
					Simage =>
						newci := CImage.new(newurl, nil, ps.ci.width, ps.ci.height);
						for(itl := ps.itl; itl != nil ; itl = tl itl) {
							pick imi := hd itl {
							Iimage =>
								imi.ci = newci;
							}
						}
						news := ref Source.Simage(nil, 0, newci, ps.itl, nil);
						sources.add(news);
						startimreq(news, auth);
					}
				}
				if(!use)
					freeit = 1;
			}
			if(!freeit && bs.edata > bs.lim) {
				pick s := src {
				Shtml =>
					itl := s.itsrc.getitems();
					if (bs == bsmain) {
						if(di.kidinfo != nil) {
							if(s.itsrc.kidstk == nil) {
								layframeset(f, di.kidinfo);
								f.cim.flush(D->Flushnow);
								freeit = 1;
							}
						}
						else {
							l.background = di.background;
							anyanim |= addsubords(sources, di, auth);
							if(itl != nil) {
								appenditems(f, l, itl);
								fixframegeom(f);
								if(dbgev)
									CU->event("LAYOUT_DRAWALL", 0);
								drawall(f);
							}
						}
					}
					else {
						sys->print("Need to implement embedded html objects\n");
						freeit = 1;
					}
				Simage =>
					(ret, mim) := s.imsrc.getmim();
					# mark it done even if error
					s.ci.complete = ret;
					if(ret == I->Mimerror) {
						bs.err = s.imsrc.err;
						freeit = 1;
					}
					else if(ret != I->Mimnone) {
						if(s.ci.mims == nil) {
							s.ci.mims = array[1] of { mim };
							s.ci.width = s.imsrc.width;
							s.ci.height = s.imsrc.height;
							if(ret == I->Mimdone && (CU->config).imagelvl <= CU->ImgNoAnim)
								freeit = 1;
						}
						else {
							n := len s.ci.mims;
							if(mim != s.ci.mims[n-1]) {
								newmims := array[n + 1] of ref MaskedImage;
								newmims[0:] = s.ci.mims;
								newmims[n] = mim;
								s.ci.mims = newmims;
								anyanim = 1;
							}
						}
						if(s.ci.mims[0] == mim)
							haveimage(f, s.ci, s.itl);
						if(bs.lim == bs.hdr.length)
							(CU->imcache).add(s.ci);
					}
				}
			}
			if(!freeit && bs.hdr != nil && bs.lim == bs.hdr.length)
				freeit = 1;
		}
		if(freeit) {
			if(bs == bsmain)
				ans = bs.data[0:bs.edata];
			CU->freebs(bs);
			src.bs = nil;
			sources.done++;
		}
	}
	if(anyanim && (CU->config).imagelvl > CU->ImgNoAnim)
		spawn animproc(f);
	if(dbgev)
		CU->event("LAYOUT_END", 0);
	return ans;
}

# return value is 1 if found any existing images needed animation
addsubords(sources: ref Sources, di: ref Docinfo, auth: string) : int
{
	anyanim := 0;
	if((CU->config).imagelvl == CU->ImgNone)
		return anyanim;
	newsims: list of ref Source.Simage = nil;
	for(il := di.images; il != nil; il = tl il) {
		it := hd il;
		pick i := it {
		Iimage =>
			if(i.ci.mims == nil) {
				cachedci := (CU->imcache).look(i.ci);
				if(cachedci != nil) {
					i.ci = cachedci;
					if(i.imwidth == 0)
						i.imwidth = i.ci.width;
					if(i.imheight == 0)
						i.imheight = i.ci.height;
					anyanim |= (len cachedci.mims > 1);
					if(it == di.backgrounditem)
						di.background.image = i.ci;
				}
				else {
				    sloop:
					for(sl := sources.srcs; sl != nil; sl = tl sl) {
						pick s := hd sl {
						Simage =>
							if(s.ci.match(i.ci)) {
								s.itl = it :: s.itl;
								# want all items on list to share same ci;
								# want most-specific dimension specs
								iciw := i.ci.width;
								icih := i.ci.height;
								i.ci = s.ci;
								if(s.ci.width == 0 && s.ci.height == 0) {
									s.ci.width = iciw;
									s.ci.height = icih;
								}
								break sloop;
							}
						}
					}
					if(sl == nil) {
						# didn't find existing Source for this image
						s := ref Source.Simage(nil, 0, i.ci, it:: nil, nil);
						newsims = s :: newsims;
						sources.add(s);
					}
				}
			}
		}
	}
	# Start requests for new newsources.
	# di.images are in last-in-document-first order,
	# so newsources is in first-in-document-first order (good order to load in).
	for(sl := newsims; sl != nil; sl = tl sl)
		startimreq(hd sl, auth);
	return anyanim;
}

startimreq(s: ref Source.Simage, auth: string)
{
	if(dbgev)
		CU->event(sys->sprint("LAYOUT STARTREQ %s", s.ci.src.tostring()), 0);
	bs := CU->startreq(ref CU->ReqInfo(s.ci.src, CU->HGet, nil, auth, ""));
	s.bs = bs;
	s.imsrc = I->ImageSource.new(bs, s.ci.width, s.ci.height);
}

createvscroll(f: ref Frame)
{
	breadth := SCRBREADTH;
	if(f.parent != nil)
		breadth = SCRFBREADTH;
	length := f.cr.dy();
	if(f.flags&FRhscroll)
		length -= breadth;
	f.vscr = Control.newscroll(f, 1, length, breadth);
	f.vscr.r = f.vscr.r.addpt(Point(f.cr.max.x-breadth, f.cr.min.y));
	f.cr.max.x -= breadth;
	if(f.cr.dx() <= 2*f.marginw)
		CU->raise("EXInternal: frame too small for layout");
	f.vscr.draw(0);
}

createhscroll(f: ref Frame)
{
	breadth := SCRBREADTH;
	if(f.parent != nil)
		breadth = SCRFBREADTH;
	length := f.cr.dx();
	x := f.cr.min.x;
	f.hscr = Control.newscroll(f, 0, length, breadth);
	f.hscr.r = f.hscr.r.addpt(Point(x,f.cr.max.y-breadth));
	f.cr.max.y -= breadth;
	if(f.cr.dy() <= 2*f.marginh)
		CU->raise("EXInternal: frame too small for layout");
	f.hscr.draw(0);
}

# Call after a change to f.layout or f.viewr.min to fix totalr and viewr
# (We are to leave viewr.min unchanged, if possible, as
# user might be scrolling).
fixframegeom(f: ref Frame)
{
	l := f.layout;
	if(dbg)
		sys->print("fixframegeom, layout width=%d, height=%d\n", l.width, l.height);
	f.totalr.max = Point(l.width, l.height);
	crwidth := f.cr.dx();
	crheight := f.cr.dy();
	crchanged := 0;
	n := l.height+l.margin-crheight;
	if(n > 0 && f.vscr == nil && (f.flags&FRvscrollauto)) {
		createvscroll(f);
		crchanged = 1;
		crwidth = f.cr.dx();
	}
	if(f.viewr.min.y > n)
		f.viewr.min.y = max(0, n);
	n = l.width+l.margin-crwidth;
	if(n > 0 && f.hscr == nil && (f.flags&FRhscrollauto)) {
		createhscroll(f);
		crchanged = 1;
		crheight = f.cr.dy();
	}
	if(crchanged) {
		relayout(f, l, crwidth, l.just);
		fixframegeom(f);
		return;
	}
	if(f.viewr.min.x > n)
		f.viewr.min.x = max(0, n);
	f.viewr.max.x = min(f.viewr.min.x+crwidth, l.width);
	f.viewr.max.y = min(f.viewr.min.y+crheight, l.height);
	if(f.vscr != nil)
		f.vscr.scrollset(f.viewr.min.y, f.viewr.max.y, f.totalr.max.y, 5);
	if(f.hscr != nil)
		f.hscr.scrollset(f.viewr.min.x, f.viewr.max.x, f.totalr.max.x, 5);
}

# The items its within f are Iimage items,
# and its image, ci, now has at least a ci.mims[0], which may be partially
# or fully filled.
haveimage(f: ref Frame, ci: ref CImage, itl: list of ref Item)
{
	if(dbgev)
		CU->event("HAVEIMAGE", 0);
	if(dbg)
		sys->print("\nHAVEIMAGE src=%s w=%d h=%d\n", ci.src.tostring(), ci.width, ci.height);
	dorelayout := 0;
	for( ; itl != nil; itl = tl itl) {
		it := hd itl;
		pick i := it {
		Iimage =>
			if(it == f.doc.backgrounditem && f.doc.background.image == nil) {
				im := ci.mims[0].im;
				im.repl = 1;
				im.clipr = Rect((-16rFFFFFFF, -16r3FFFFFFF), (16r3FFFFFFF, 16r3FFFFFFF));
				f.doc.background.image = i.ci;
			}
			else {
				# If i.imwidth and i.imheight are not both 0, the HTML specified the dimens.
				# If one of them is 0, the other is to be scaled by the same factor;
				# we have to relay the line in that case too.
				if(i.imwidth == 0 || i.imheight == 0) {
					i.imwidth = ci.width;
					i.imheight = ci.height;
					setimagedims(i);
					loc := f.find(zp, it);
					# sometimes the image was added to doc image list, but
					# never made it to layout (e.g., because html bug prevented
					# a table from being added).
					# also, script-created images won't have items
					if(loc != nil) {
						markchanges(loc);
						dorelayout = 1;
						# Floats are assumed to be premeasured, so if there
						# are any floats in the loc list, remeasure them
						for(k := loc.n-1; k > 0; k--) {
							if(loc.le[k].kind == LEitem) {
								locit := loc.le[k].item;
								pick fit := locit {
								Ifloat =>
									pick xi := fit.item {
									Iimage =>
										fit.height = fit.item.height;
									Itable =>
										checktabsize(f, xi, TABLEFLOATTARGET);
									}
								}
							}
						}
					}
				}
			}
			if(dbg > 1) {
				sys->print("\nhaveimage item: ");
				it.print();
			}
		}
	}
	if(dorelayout) {
		relayout(f, f.layout, f.layout.targetwidth, f.layout.just);
		fixframegeom(f);
	}
	drawall(f);
	if(dbgev)
		CU->event("HAVEIMAGE_END", 0);
}

# For first layout of subelements, such as table cells.
# After this, content items will be dispersed throughout resulting lay.
# Return index into f.sublays.
# (This roundabout way of storing sublayouts avoids pointers to Lay
# in Build, so that all of the layout-related stuff can be in Layout
# where it belongs.)
sublayout(f: ref Frame, targetwidth: int, just: byte, bg: Background, content: ref Item) : int
{
	if(dbg)
		sys->print("sublayout, targetwidth=%d\n", targetwidth);
	l := Lay.new(targetwidth, just, 0, bg);
	if(f.sublayid >= len f.sublays) {
		newsublays := array[len f.sublays + 30] of ref Lay;
		newsublays[0:] = f.sublays;
		f.sublays = newsublays;
	}
	id := f.sublayid;
	f.sublays[id] = l;
	f.sublayid++;
	appenditems(f, l, content);
	l.flags &= ~Lchanged;
	if(dbg)
		sys->print("after sublayout, width=%d\n", l.width);
	return id;
}

# Relayout of lay, given a new target width or if something changed inside
# or if the global justification for the layout changed.
# Floats are hard: for now, just relay everything with floats temporarily
# moved way down, if there are any floats.
relayout(f: ref Frame, lay: ref Lay, targetwidth: int, just: byte)
{
	if(dbg)
		sys->print("relayout, targetwidth=%d, old target=%d, changed=%d\n",
			targetwidth, lay.targetwidth, (lay.flags&Lchanged) != byte 0);
	changeall := (lay.targetwidth != targetwidth || lay.just != just);
	if(!changeall && !int(lay.flags&Lchanged))
		return;
	if(lay.floats != nil) {
		# move the current y positions of floats to a big value,
		# so they don't contribute to floatw until after they've
		# been encountered in current fixgeom
		for(flist := lay.floats; flist != nil; flist = tl flist) {
			ff := hd flist;
			ff.y = 10000;
		}
		changeall = 1;
	}
	lay.targetwidth = targetwidth;
	lay.just = just;
	lay.width = 0;
	if(changeall)
		changelines(lay.start.next, lay.end);
	fixgeom(f, lay, lay.start.next);
	lay.flags &= ~Lchanged;
	if(dbg)
		sys->print("after relayout, width=%d\n", lay.width);
}

# Measure and append the items to the end of layout lay,
# and fix the geometry.
appenditems(f: ref Frame, lay: ref Lay, items: ref Item)
{
	measure(f, items);
	if(dbg)
		items.printlist("appenditems, after measturetexts");
	it := items;
	if(it == nil)
		return;
	lprev := lay.end.prev;
	l : ref Line;
	lit := lastitem(lprev.items);
	if(lit == nil || (it.state&IFbrk)) {
		# start a new line after existing last line
		l = Line.new();
		appendline(lprev, l);
		l.items = it;
	}
	else {
		# start appending items to existing last line
		l = lprev;
		lit.next = it;
	}
	l.flags |= Lchanged;
	while(it != nil) {
		nexti := it.next;
		if(nexti == nil || (nexti.state&IFbrk)) {
			it.next = nil;
			fixgeom(f, lay, l);
			if(nexti == nil)
				break;
			# now there may be multiple lines containing the
			# items from l, but the one after the last is lay.end
			l = Line.new();
			appendline(lay.end.prev, l);
			l.flags |= Lchanged;
			it = nexti;
			l.items = it;
		}
		else
			it = nexti;
	}
}

# Fix up the geometry of line l and successors.
# Assume geometry of previous line is correct.
fixgeom(f: ref Frame, lay: ref Lay, l: ref Line)
{
	while(l != nil) {
		fixlinegeom(f, lay, l);
		l = l.next;
	}
	lay.height = lay.end.pos.y;
}

# Fix geom for one line.
# This may change the overall lay.width, if there is no way
# to fit the line into the target width. 
fixlinegeom(f: ref Frame, lay: ref Lay, l: ref Line)
{
	lprev := l.prev;
	y := lprev.pos.y + lprev.height;
	it := l.items;
	state := it.state;
	if(dbg > 1) {
		sys->print("\nfixlinegeom start, y=prev.y+prev.height=%d+%d=%d, changed=%d\n",
				l.prev.pos.y, lprev.height, y, int (l.flags&Lchanged));
		if(dbg > 2)
			it.printlist("items");
		else {
			sys->print("first item: ");
			it.print();
		}
	}
	if(state&IFbrk) {
		y = pastbrk(lay, y, state);
		if(dbg > 1 && y != lprev.pos.y + lprev.height)
			sys->print("after pastbrk, line y is now %d\n", y);
	}
	l.pos.y = y;
	lineh := max(l.height, linespace);
	lfloatw := floatw(y, y+lineh, lay.floats, Aleft);
	rfloatw := floatw(y, y+lineh, lay.floats, Aright);
	if((l.flags&Lchanged) == byte 0) {
		# possibly adjust lay.width
		n := (lay.width-rfloatw)-(l.pos.x-lay.margin+l.width);
		if(n < 0)
			lay.width += -n;
		return;
	}
	wrapping := int (state&IFwrap);
	hang := (state&IFhangmask)*TABPIX/10;
	linehang := hang;
	hangtogo := hang;
	indent := ((state&IFindentmask)>>IFindentshift)*TABPIX;
	just := (state&(IFcjust|IFrjust));
	if(just == 0 && lay.just != Aleft) {
		if(lay.just == byte Acenter)
			just = IFcjust;
		else if(lay.just == Aright)
			just = IFrjust;
	}
	right := max(lay.targetwidth, lay.width);
	lwid :=right - (lfloatw+rfloatw+indent);
	if(lwid < 0) {
		lay.width = lfloatw+rfloatw+indent;
		right = lay.width;
		lwid = 0;
	}
	lwid += hang;
	if(dbg > 1) {
		sys->print("fixlinegeom, now y=%d, lfloatw=%d, rfloatw=%d, indent=%d, hang=%d, lwid=%d\n",
				y, lfloatw, rfloatw, indent, hang, lwid);
	}
	l.pos.y = y;
	w := 0;
	lineh = 0;
	linea := 0;
	lastit: ref Item = nil;
	nextfloats: list of ref Item.Ifloat = nil;
	anystuff := 0;
	while(it != nil) {
		if(dbg > 2) {
			sys->print("fixlinegeom loop head, w=%d, loop item:\n", w);
			it.print();
		}
		state = it.state;
		if(anystuff && (state&IFbrk))
			break;
		checkw := 1;
		if(hang && !(state&IFhangmask)) {
			lwid -= hang;
			hang = 0;
			if(hangtogo > 0) {
				# insert a null spacer item
				spaceit := Item.newspacer(ISPgeneral);
				spaceit.width = hangtogo;
				if(lastit != nil) {
					spaceit.state = lastit.state & ~(IFbrk|IFbrksp|IFnobrk|IFcleft|IFcright);
					lastit.next = spaceit;
				}
				else
					lastit = spaceit;
				spaceit.next = it;
			}
		}
		pick i := it {
		Ifloat =>
			if(anystuff) {
				# float will go after this line
				nextfloats = i :: nextfloats;
			}
			else {
				# add float beside current line, adjust widths
				fixfloatxy(lay, y, i);
				# TODO: only do following if y and/or height changed
				changelines(l.next, lay.end);
				newlfloatw := floatw(y, y+1, lay.floats, Aleft);
				newrfloatw := floatw(y, y+1, lay.floats, Aright);
				lwid -= (newlfloatw-lfloatw) + (newrfloatw-rfloatw);
				if(lwid < 0) {
					lay.width += -lwid;
					right = lay.width;
					lwid = 0;
				}
				lfloatw = newlfloatw;
				rfloatw = newrfloatw;
				if(dbg > 1)
					sys->print("float added to current line, i.y=%d, side=%d, h=%d, lfloatw=%d, rfloatw=%d, lwid=%d, lay.width=%d, right=%d\n",
						i.y, int i.side, i.item.height, lfloatw, rfloatw, lwid, lay.width, right);
			}
			checkw = 0;
		Itable =>
			# When just doing layout for cell dimensions, don't
			# want a "100%" spec to make the table really wide
			kindspec := 0;
			if(lay.targetwidth == TABLEMAXTARGET && i.table.width.kind() == Dpercent) {
				kindspec = i.table.width.kindspec;
				i.table.width = Dimen.make(Dnone, 0);
			}
			checktabsize(f, i, lwid-w);
			if(kindspec != 0)
				i.table.width.kindspec = kindspec;
		Irule =>
			avail := lwid-w;
			# When just doing layout for cell dimensions, don't
			# want a "100%" spec to make the rule really wide
			if(lay.targetwidth == TABLEMAXTARGET)
				avail = min(10, avail);
			i.width = widthfromspec(i.wspec, avail);
		Iformfield =>
			checkffsize(f, i, i.formfield);
		}
		if(checkw) {
			iw := it.width;
			if(wrapping && w + iw > lwid) {
				# it doesn't fit; see if it can be broken
				takeit: int;
#				noneok := (anystuff || lfloatw != 0 || rfloatw != 0) && !(state&IFnobrk);
				noneok := anystuff && !(state&IFnobrk);
				(takeit, iw) = trybreak(it, lwid-w, iw, noneok);
				if(!takeit) {
					if(lastit == nil) {
						# Nothing added because one of the float widths
						# is nonzero, and not enough room for anything else.
						# Move y down until there's more room and try again.
						CU->assert(lfloatw != 0 || rfloatw != 0);
						oldy := y;
						y = pastbrk(lay, y, IFcleft|IFcright);
						if(dbg > 1)
							sys->print("moved y past %d, now y=%d\n", oldy, y);
						CU->assert(y > oldy);	# else infinite recurse
						# Do the move down by artificially increasing the
						# height of the previous line
						lprev.height += y-oldy;
						fixlinegeom(f, lay, l);
						return;
					}
					break;
				}
			}
			w += iw;
			if(hang)
				hangtogo -= w;
			(lineh, linea) = lgeom(lineh, linea, it);
			if(!anystuff) {
				anystuff = 1;
				# don't count an ordinary space as 'stuff' if wrapping
				pick t := it {
				Itext =>
					if(wrapping && t.s[0] == ' ')
						anystuff = 0;
				}
			}
		}
		lastit = it;
		it = it.next;
		if(it == nil) {
			# perhaps next lines items can now fit on this line
			nextl := l.next;
			nit := nextl.items;
			if(nextl != lay.end && !(nit.state&IFbrk)) {
				lastit.next = nit;
				# remove nextl
				l.next = nextl.next;
				l.next.prev = l;
				it = nit;
			}
		}
	}
	# line is complete, next line will start with it (or it is nil)
	rest := it;
	if(lastit == nil)
		CU->raise("EXInternal: no items on line");
	lastit.next = nil;

	l.width = w;
	x := lfloatw + indent - linehang;
	# shift line if it begins with a space or a rule
	pick pi := l.items {
		Itext =>
			if(pi.s[0] == ' ')
				x -= charon_gui->fonts[pi.fnt].spw;
		Irule =>
			# note: build ensures that rules appear on lines
			# by themselves
			if(pi.align == Acenter)
				just = IFcjust;
			else if(pi.align == Aright)
				just = IFrjust;
		Ifloat =>
			if(pi.next != nil) {
				pick qi := pi.next {
				Itext =>
					if(qi.s[0] == ' ')
						x -= charon_gui->fonts[qi.fnt].spw;
				}
			}
	}
	xright := x+w;
	n := (lay.width - rfloatw) - xright;
	if(n < 0) {
		lay.width += -n;
		n = 0;
	}
	# except when doing the maximum needed cell width, justification
	# is supposed to be within max(lay.width, targetwidth),
	# but also, if this is lay for frame itself, justify within f.targetwidth to match other browsers.
	justw : int;
	if(lay == f.layout)
		justw = lay.targetwidth;
	else if(lay.targetwidth == TABLEMAXTARGET)
		justw = lay.width;
	else
		justw = max(lay.width, lay.targetwidth);
	n = (justw - rfloatw) - xright;
	if(n > 0 && just) {
		if(just&IFcjust)
			x += n/2;
		else
			x += n;
		if(x+w+rfloatw > lay.width)
			lay.width = x+w+rfloatw;
	}
	if(dbg > 1) {
		sys->print("line geometry fixed, (x,y)=(%d,%d), w=%d, h=%d, a=%d, lfloatw=%d, rfloatw=%d, lay.width=%d\n",
			x, l.pos.y, w, lineh, linea, lfloatw, rfloatw, lay.width);
		if(dbg > 2)
			l.items.printlist("final line items");
	}
	l.pos.x = x+lay.margin;
	l.height = lineh;
	l.ascent = linea;
	l.flags &= ~Lchanged;

	if(nextfloats != nil)
		fixfloatsafter(lay, l, nextfloats);

	if(rest != nil) {
		nextl := l.next;
		if(nextl == lay.end || (nextl.items.state&IFbrk)) {
			nextl = Line.new();
			appendline(l, nextl);
		}
		li := lastitem(rest);
		li.next = nextl.items;
		nextl.items = rest;
		nextl.flags |= Lchanged;
	}
}

# Return y coord after y due to a break.
pastbrk(lay: ref Lay, y, state: int) : int
{
	nextralines := 0;
	if(state&IFbrksp)
		nextralines = 1;
	ynext := y;
	if(state&IFcleft)
		ynext = floatclry(lay.floats, Aleft, ynext);
	if(state&IFcright)
		ynext = max(ynext, floatclry(lay.floats, Aright, ynext));
	ynext += nextralines*linespace;
	return ynext;
}

# Add line l after lprev (and before lprev's current successor)
appendline(lprev, l: ref Line)
{
	l.next = lprev.next;
	l.prev = lprev;
	l.next.prev = l;
	lprev.next = l;
}

# Mark lines l up to but not including lend as changed
changelines(l, lend: ref Line)
{
	for( ; l != lend; l = l.next)
		l.flags |= Lchanged;
}

# Return a ref Font for font number num = (style*NumSize + size)
getfont(num: int) : ref Font
{
	f := charon_gui->fonts[num].f;
	if(f == nil) {
		f = Font.open(display, charon_gui->fonts[num].name);
		if(f == nil) {
			if(num == DefFnt)
				CU->raise(sys->sprint("exLayout: can't open default font %s: %r", charon_gui->fonts[num].name));
			else {
				if(int (CU->config).dbg['w'])
					sys->print("warning: substituting default for font %s\n",
						charon_gui->fonts[num].name);
				f = charon_gui->fonts[DefFnt].f;
			}
		}
		charon_gui->fonts[num].f = f;
		charon_gui->fonts[num].spw = f.width(" ");
	}
	return f;
}

# Set the width, height and ascent fields of all items, getting any necessary fonts.
# Some widths and heights depend on the available width on the line, and may be
# wrong until checked during fixlinegeom.
# Don't do tables here at all (except floating tables).
# Configure Controls for form fields.
measure(fr: ref Frame, items: ref Item)
{
	for(it := items; it != nil; it = it.next) {
		pick t := it {
		Itext =>
			t.fnt=charonCode->remeasure_font(t.s,t.fnt);
			f := getfont(t.fnt);
			it.width = f.width(t.s);
			a := f.ascent;
			h := f.height;
			if(t.voff != byte Voffbias) {
				a -= (int t.voff) - Voffbias;
				if(a > h)
					h = a;
			}
			it.height = h;
			it.ascent = a;
		Irule =>
			it.height =  t.size + 2*RULESP;
			it.ascent = t.size + RULESP;
		Iimage =>
			setimagedims(t);
		Iformfield =>
			c := Control.newff(fr, t.formfield);
			if(c != nil) {
				t.formfield.ctlid = fr.addcontrol(c);
				it.width = c.r.dx();
				it.height = c.r.dy();
				it.ascent = it.height;
				pick pc := c {
				Centry =>
					it.ascent = lineascent + ENTVMARGIN;
				Cselect =>
					it.ascent = lineascent + SELMARGIN;
				Cbutton =>
					if(pc.dorelief)
						it.ascent -= BUTMARGIN;
				}
			}
		Ifloat =>
			# Leave w at zero, so it doesn't contribute to line width in normal way
			# (Can find its width in t.item.width).
			pick i := t.item {
			Iimage =>
				setimagedims(i);
				it.height = t.item.height;
			Itable =>
				checktabsize(fr, i, TABLEFLOATTARGET);
			* =>
				CU->assert(0);
			}
			it.ascent = it.height;
		Ispacer =>
			case t.spkind {
			ISPvline =>
				it.height = linespace;
				it.ascent = lineascent;
			ISPhspace =>
				it.width = spspace;
			}
		}
	}
}

# Set the dimensions of an image item
setimagedims(i: ref Item.Iimage)
{
	i.width = i.imwidth + 2*(int i.hspace + int i.border);
	i.height = i.imheight + 2*(int i.vspace + int i.border);
	i.ascent = i.height - (int i.vspace + int i.border);
	if((CU->config).imagelvl == CU->ImgNone && i.altrep != "") {
		f := charon_gui->fonts[DefFnt].f;
		i.width = max(i.width, f.width(i.altrep));
		i.height = max(i.height, f.height);
		i.ascent = f.ascent;
	}
}

# Line geometry function:
# Given current line height (H) and ascent (distance from top to baseline) (A),
# and an item, see if that item changes height and ascent.
# Return (H', A'), the updated line height and ascent.
lgeom(H, A: int, it: ref Item) : (int, int)
{
	h := it.height;
	a := it.ascent;
	atype := Abaseline;
	pick i := it {
	Iimage =>
		atype = i.align;
	Itable =>
		atype = Atop;
	Ifloat =>
		return (H, A);
	}
	d := h-a;
	Hnew := H;
	Anew := A;
	case int atype {
	int Abaseline or int Abottom =>
		if(a > A) {
			Anew = a;
			Hnew += (Anew - A);
		}
		if(d > Hnew - Anew)
			Hnew = Anew + d;
	int Atop =>
		# OK to ignore what comes after in the line
		if(h > H)
			Hnew = h;
	int Amiddle or int Acenter =>
		# supposed to align middle with baseline
		hhalf := h/2;
		if(hhalf > A)
			Anew = hhalf;
		if(hhalf > H-Anew)
			Hnew = Anew + hhalf;
	}
	return (Hnew, Anew);
}

# Try breaking item bit to make it fit in availw.
# If that is possible, change bit to be the part that fits
# and insert the rest between bit and bit.next.
# iw is the current width of bit.
# If noneok is 0, break off the minimum size word
# even if it exceeds availw.
# Return (1 if supposed to take bit, iw' = new width of bit)
trybreak(bit: ref Item, availw, iw, noneok: int) : (int, int)
{
	if(iw <= 0)
		return (1, iw);
	if(availw < 0) {
		if(noneok)
			return (0, iw);
		else
			availw = 0;
	}
	pick t := bit {
	Itext =>
		if(len t.s < 2)
			return (!noneok, iw);
		(s1, w1, s2, w2) := breakstring(t.s, iw, charon_gui->fonts[t.fnt].f, availw, noneok);
		if(w1 == 0)
			return (0, iw);
		itn := Item.newtext(s2, t.fnt, t.fg, int t.voff, t.ul);
		itn.width = w2;
		itn.height = t.height;
		itn.ascent = t.ascent;
		itn.anchorid = t.anchorid;
		itn.state = t.state & ~(IFbrk|IFbrksp|IFnobrk|IFcleft|IFcright);
		itn.next = t.next;
		t.next = itn;
		t.s = s1;
		t.width = w1;
		return (1, w1);
	}
	return (!noneok, iw);
}

# s has width sw when drawn in fnt.
# Break s into s1 and s2 so that s1 fits in availw.
# If noneok is true, it is ok for s1 to be nil, otherwise might
# have to return an s1 that overflows availw somewhat.
# Return (s1, w1, s2, w2) where w1 and w2 are widths of s1 and s2.
# Assume caller has already checked that sw > availw.
breakstring(s: string, sw: int, fnt: ref Font, availw, noneok: int) : (string, int, string, int)
{
	slen := len s;
	if(slen < 2) {
		if(noneok)
			return (nil, 0, s, sw);
		else
			return (s, sw, nil, 0);
	}

	# Use linear interpolation to guess break point.
	# We know avail < iw by conditions of trybreak call.
	i := slen*availw / sw - 1;
	if(i < 0)
		i = 0;
	i = breakpoint(s, i, -1);
	(ss, ww) := tryw(fnt, s, i);
	if(ww > availw) {
		while(ww > availw) {
			i = breakpoint(s, i-1, -1);
			if(i <= 0)
				break;
			(ss, ww) = tryw(fnt, s, i);
		}
	}
	else {
		oldi := i;
		oldss := ss;
		oldww := ww;
		while(ww < availw) {
			oldi = i;
			oldss = ss;
			oldww = ww;
			i = breakpoint(s, i+1, 1);
			if(i >= slen)
				break;
			(ss, ww) = tryw(fnt, s, i);
		}
		i = oldi;
		ss = oldss;
		ww = oldww;
	}
	if(i <= 0 || i >= slen) {
		if(noneok)
			return (nil, 0, s, sw);
		i = breakpoint(s, 1, 1);
		(ss,ww) = tryw(fnt, s, i);
	}
	return (ss, ww, s[i:slen], sw-ww);
}

# If can break between s[i-1] and s[i], return i.
# Else move i in direction incr until this is true.
# (Might end up returning 0 or len s).
breakpoint(s: string, i, incr: int) : int
{
	slen := len s;
	ans := 0;
	while(i > 0 && i < slen) {
		ci := s[i];
		di := s[i-1];
		if((ci >= 16rA0 || int wordchar[ci]) && (di >= 16rA0 || int wordchar[di]))
			i += incr;
		else {
			ans = i;
			break;
		}
	}
	if(i == slen)
		ans = slen;
	return ans;
}

# Return (s[0:i], width of that slice in font fnt)
tryw(fnt: ref Font, s: string, i: int) : (string, int)
{
	if(i == 0)
		return ("", 0);
	ss := s[0:i];
	return (ss, fnt.width(ss));
}

# Return max width of a float that overlaps [ymin, ymax) on given side.
# Floats are in reverse order of addition, so each float's y is <= that of
# preceding floats in list.  Floats from both sides are intermixed.
floatw(ymin, ymax: int, flist: list of ref Item.Ifloat, side: byte) : int
{
	ans := 0;
	for( ; flist != nil; flist = tl flist) {
		fl := hd flist;
		if(fl.side != side)
			continue;
		fymin := fl.y;
		fymax := fymin + fl.item.height;
		if((fymin <= ymin && ymin < fymax) ||
		   (ymin <= fymin && fymin < ymax)) {
			w := fl.x;
			if(side == Aleft)
				w += fl.item.width;
			if(ans < w)
				ans = w;
		}
	}
	return ans;
}

# Float f is to be at vertical position >= y.
# Fix its (x,y) pos and add it to lay.floats, if not already there.
fixfloatxy(lay: ref Lay, y: int, f: ref Item.Ifloat)
{
	f.y = y;
	flist := lay.floats;
	if(f.infloats != byte 0) {
		# only take previous floats into account for width
		while(flist != nil) {
			x := hd flist;
			flist = tl flist;
			if(x == f)
				break;
		}
	}
	f.x = floatw(y, y+f.item.height, flist, f.side);
	# for right-side floats, x is distance from right edge,
	# which includes f's width
	if(f.side == Aright)
		f.x += f.item.width;
	if(dbg > 1) {
		sys->print("fixfloatxy, now float is:\n");
		it: ref Item = f;
		it.print();
	}
	if(f.infloats == byte 0) {
		lay.floats = f :: lay.floats;
		f.infloats = byte 1;
	}
}

# Floats in flist are to go after line l.
fixfloatsafter(lay: ref Lay, l: ref Line, flist: list of ref Item.Ifloat)
{
	change := 0;
	y := l.pos.y + l.height;
	for(itl := Item.revlist(flist); flist != nil; flist = tl flist) {
		pick fl := hd itl {
		Ifloat =>
			oldy := fl.y;
			fixfloatxy(lay, y, fl);
			if(fl.y != oldy)
				change = 1;
		}
	}
#	if(change)
# TODO only change if y and/or height changed
		changelines(l.next, lay.end);
}

# Return bottom edge of lowest float on given side,
# assuming flist is bottom-to-top
floatmaxy(flist: list of ref Item.Ifloat, side: byte) : int
{
	for( ; flist != nil; flist = tl flist) {
		fl := hd flist;
		if(fl.side == side)
			return fl.y + fl.item.height;
	}
	return 0;
}

# If there's a float on given side that starts on or before y and
# ends after y, return ending y of that float, else return original y.
# Assume float list is bottom up.
floatclry(flist: list of ref Item.Ifloat, side: byte, y: int) : int
{
	for( ; flist != nil; flist = tl flist) {
		fl := hd flist;
		if(fl.side == side) {
			if(fl.y <= y) {
				flymax := fl.y + fl.item.height;
				if(flymax <= y)
					return y;
				else
					return flymax;
			}
		}
	}
	return y;
}

# Do preliminaries to laying out table tab in target width linewidth,
# setting total height and width.
sizetable(f: ref Frame, tab: ref Table, availwidth: int)
{
	if(dbgtab)
		sys->print("sizetable %d, availwidth=%d, nrow=%d, ncol=%d, changed=%x, tab.availw=%d\n",
			tab.tableid, availwidth, tab.nrow, tab.ncol, int (tab.flags&Lchanged), tab.availw);
	if(tab.ncol == 0 || tab.nrow == 0)
		return;
	if(tab.availw == availwidth && (tab.flags&Lchanged) == byte 0)
		return;
	(hsp, vsp, pad, bd, cbd, hsep, vsep) := tableparams(tab);
	totw := widthfromspec(tab.width, availwidth);
	# reduce totw by spacing, padding, and rule widths
	# to leave amount left for contents
	totw -= (tab.ncol-1)*hsep+ 2*(hsp+bd+pad+cbd);

	if(totw <= 0)
		totw = 1;
	if(dbgtab)
		sys->print("\nsizetable %d, totw=%d, hsp=%d, vsp=%d, pad=%d, bd=%d, cbd=%d, hsep=%d, vsep=%d\n",
			tab.tableid, totw, hsp, vsp, pad, bd, cbd, hsep, vsep);
	for(cl := tab.cells; cl != nil; cl = tl cl) {
		c := hd cl;
		clay : ref Lay = nil;
		if(c.layid >= 0)
			clay = f.sublays[c.layid];
		if(clay == nil || (clay.flags&Lchanged) != byte 0) {
			c.minw = -1;
			tw := TABLEMAXTARGET;
			if(c.wspec.kind() != Dnone)
				tw = widthfromspec(c.wspec, totw);

			# When finding max widths, want to lay out using Aleft alignment,
			# because we don't yet know final width for proper justification.
			# If the max widths are accepted, we'll redo those needing other justification.
			if(clay == nil) {
				if(dbg)
					sys->print("Initial layout for cell %d.%d\n", tab.tableid, c.cellid);
				c.layid = sublayout(f, tw, Aleft, c.background, c.content);
				clay = f.sublays[c.layid];
				c.content = nil;
			}
			else {
				if(dbg)
					sys->print("Relayout (for max) for cell %d.%d\n", tab.tableid, c.cellid);
				relayout(f, clay, tw, Aleft);
			}
			clay.flags |= Lchanged;	# for min test, below
			c.maxw = clay.width;
			if(dbgtab)
				sys->print("sizetable %d for cell %d max layout done, targw=%d, c.maxw=%d\n",
						tab.tableid, c.cellid, tw, c.maxw);
			if(c.wspec.kind() == Dpixels) {
				# Other browsers don't make the following adjustment for
				# percentage and relative widths
				if(c.maxw <= tw)
					c.maxw = tw;
				if(dbgtab)
					sys->print("after spec adjustment, c.maxw=%d\n", c.maxw);
			}
		}
	}

	# calc max column widths
	colmaxw := array[tab.ncol] of { * => 0};
	maxw := widthcalc(tab, colmaxw, hsep, 1);

	if(dbgtab)
		sys->print("sizetable %d maxw=%d, totw=%d\n", tab.tableid, maxw, totw);
	ci: int;
	if(maxw <= totw) {
		# trial layouts are fine,
		# but if table width was specified, add more space
		d := 0;
		if(totw > maxw && tab.width.kind() != Dnone)
			d = (totw-maxw) / tab.ncol;
		for(ci = 0; ci < tab.ncol; ci++) {
			tab.cols[ci].width = colmaxw[ci] + d;
			if(dbgtab)
				sys->print("sizetable %d tab.cols[%d].width = %d\n", tab.tableid, ci, colmaxw[ci]);
		}
	}
	else {
		# calc min column widths and  apportion out
		# differences
		if(dbgtab)
			sys->print("sizetable %d, availwidth %d, need min widths too\n", tab.tableid, availwidth);
		for(cl = tab.cells; cl != nil; cl = tl cl) {
			c := hd cl;
			clay := f.sublays[c.layid];
			if(c.minw == -1 || (clay.flags&Lchanged) != byte 0) {
				if(dbg)
					sys->print("Relayout (for min) for cell %d.%d\n", tab.tableid, c.cellid);
				relayout(f, clay, 1, Aleft);
				c.minw = clay.width;
				if(dbgtab)
					sys->print("sizetable %d for cell %d min layout done, c.min=%d\n",
						tab.tableid, c.cellid, clay.width);
			}
		}
		colminw := array[tab.ncol] of { * => 0};
		minw := widthcalc(tab, colminw, hsep, 0);
		w := totw - minw;
		d := maxw - minw;
		if(dbgtab)
			sys->print("sizetable %d minw=%d, w=%d, d=%d\n", tab.tableid, minw, w, d);
		for(ci = 0; ci < tab.ncol; ci++) {
			wd : int;
			if(w < 0 || d < 0)
				wd = colminw[ci];
			else
				wd = colminw[ci] + (colmaxw[ci] - colminw[ci])*w/d;
			if(dbgtab)
				sys->print("sizetable %d col[%d].width = %d\n", tab.tableid, ci, wd);
			tab.cols[ci].width = wd;
		}

		if(dbgtab)
			sys->print("sizetable %d, availwidth %d, doing final layouts\n", tab.tableid, availwidth);
	}

	# now have col widths; set actual cell dimensions
	# and relayout (note: relayout will do no work if the target width
	# and just haven't changed from last layout)
	for(cl = tab.cells; cl != nil; cl = tl cl) {
		c := hd cl;
		clay := f.sublays[c.layid];
		wd := cellwidth(tab, c, hsep);
		if(dbgtab)
			sys->print("sizetable %d for cell %d, clay.width=%d, cellwidth=%d\n",
					tab.tableid, c.cellid, clay.width, wd);
		if(dbg)
			sys->print("Relayout (final) for cell %d.%d\n", tab.tableid, c.cellid);
		relayout(f, clay, wd, c.align.halign);
		if(dbgtab)
			sys->print("sizetable %d for cell %d, final width %d, got width %d, height %d\n",
					tab.tableid, c.cellid, wd, clay.width, clay.height);
	}

	# set row heights and ascents
	# first pass: ignore cells with rowspan > 1
	for(ri := 0; ri < tab.nrow; ri++) {
		row := tab.rows[ri];
		h := 0;
		a := 0;
		n : int;
		for(rcl := row.cells; rcl != nil; rcl = tl rcl) {
			c := hd rcl;
			if(c.rowspan > 1 || c.layid < 0)
				continue;
			al := c.align.valign;
			if(al == Anone)
				al = tab.rows[c.row].align.valign;
			clay := f.sublays[c.layid];
			if(al == Abaseline) {
				n = c.ascent;
				if(n > a) {
					h += (n - a);
					a = n;
				}
				n = clay.height - c.ascent;
				if(n > h-a)
					h = a + n;
			}
			else {
				n = clay.height;
				if(n > h)
					h = n;
			}
		}
		row.height = h;
		row.ascent = a;
	}
	# second pass: take care of rowspan > 1
	# (this algorithm isn't quite right -- it might add more space
	# than is needed in the presence of multiple overlapping rowspans)
	for(cl = tab.cells; cl != nil; cl = tl cl) {
		c := hd cl;
		if(c.rowspan > 1) {
			spanht := 0;
			for(i := 0; i < c.rowspan && c.row+i < tab.nrow; i++)
				spanht += tab.rows[c.row+i].height;
			if(c.layid < 0)
				continue;
			clay := f.sublays[c.layid];
			ht := clay.height - (c.rowspan-1)*vsep;
			if(ht > spanht) {
				# add extra space to last spanned row
				i = c.row+c.rowspan-1;
				if(i >= tab.nrow)
					i = tab.nrow - 1;
				tab.rows[i].height += ht - spanht;
				if(dbgtab)
					sys->print("sizetable %d, row %d height %d\n", tab.tableid, i, tab.rows[i].height);
			}
		}
	}
	# get total width, heights, and col x / row y positions
	totw = bd + hsp + cbd + pad;
	for(ci = 0; ci < tab.ncol; ci++) {
		tab.cols[ci].pos.x = totw;
		if(dbgtab)
			sys->print("sizetable %d, col %d at x=%d\n", tab.tableid, ci, totw);
		totw += tab.cols[ci].width + hsep;
	}
	totw = totw - (cbd+pad) + bd;
	toth := bd + vsp + cbd + pad;
	# first time: move tab.caption items into layout
	if(tab.caption != nil) {
		# lay caption with Aleft; drawing will center it over the table width
		tab.caption_lay = sublayout(f, availwidth, Aleft, f.layout.background, tab.caption);
		caplay := f.sublays[tab.caption_lay];
		tab.caph = caplay.height + CAPSEP;
		tab.caption = nil;
	}
	else if(tab.caption_lay >= 0) {
		caplay := f.sublays[tab.caption_lay];
		if(tab.availw != availwidth || (caplay.flags&Lchanged) != byte 0) {
			relayout(f, caplay, availwidth, Aleft);
			tab.caph = caplay.height + CAPSEP;
		}
	}
	if(tab.caption_place == Atop)
		toth += tab.caph;
	for(ri = 0; ri < tab.nrow; ri++) {
		tab.rows[ri].pos.y = toth;
		if(dbgtab)
			sys->print("sizetable %d, row %d at y=%d\n", tab.tableid, ri, toth);
		toth += tab.rows[ri].height + vsep;
	}
	toth = toth - (cbd+pad) + bd;
	if(tab.caption_place == Abottom)
		toth += tab.caph;
	tab.totw = totw;
	tab.toth = toth;
	tab.availw = availwidth;
	tab.flags &= ~Lchanged;
	if(dbgtab)
		sys->print("\ndone sizetable %d, availwidth %d, totw=%d, toth=%d\n\n",
			tab.tableid, availwidth, totw, toth);
}

# Calculate various table spacing parameters
tableparams(tab: ref Table) : (int, int, int, int, int, int, int)
{
	bd := tab.border;
	hsp := tab.cellspacing;
	vsp := hsp;
	pad := tab.cellpadding;
	if(bd != 0)
		cbd := 1;
	else
		cbd = 0;
	hsep := 2*(cbd+pad)+hsp;
	vsep := 2*(cbd+pad)+vsp;
	return (hsp, vsp, pad, bd, cbd, hsep, vsep);
}

# return cell width, taking multicol spanning into account
cellwidth(tab: ref Table, c: ref Tablecell, hsep: int) : int
{
	if(c.colspan == 1)
		return tab.cols[c.col].width;
	wd := (c.colspan-1)*hsep;
	for(i := 0; i < c.colspan && c.col + i < tab.ncol; i++)
		wd += tab.cols[c.col + i].width;
	return wd;
}

# return cell height, taking multirow spanning into account
cellheight(tab: ref Table, c: ref Tablecell, vsep: int) : int
{
	if(c.rowspan == 1)
		return tab.rows[c.row].height;
	ht := (c.rowspan-1)*vsep;
	for(i := 0; i < c.rowspan && c.row + i < tab.nrow; i++)
		ht += tab.rows[c.row + i].height;
	return ht;
}

# Calculate the column widths w as the max of the cells
# maxw or minw (as domax is 1 or 0).
# Return the total of all w.
# (hseps were accounted for by the adjustment that got
# totw from availwidth).
# hsep is amount of free space available between columns
# where there is multicolumn spanning.
# This is a two-pass algorithm.  The first pass ignores
# cells that span multiple columns.  The second pass
# sees if those multispanners need still more space, and
# if so, apportions the space out.
widthcalc(tab: ref Table, w: array of int, hsep, domax: int) : int
{
	anyspan := 0;
	totw := 0;
	for(pass := 1; pass <= 2; pass++) {
		if(pass==2 && !anyspan)
			break;
		totw = 0;
		for(ci := 0; ci < tab.ncol; ci++) {
			for(ri := 0; ri < tab.nrow; ri++) {
				c := tab.grid[ri][ci];
				if(c == nil)
					continue;
				if(domax)
					cwd := c.maxw;
				else
					cwd = c.minw;
				if(pass == 1) {
					if(c.colspan > 1) {
						anyspan = 1;
						continue;
					}
					if(cwd > w[ci])
						w[ci] = cwd;
				}
				else {
					if(c.colspan == 1 || !(ci==c.col && ri==c.row))
						continue;
					curw := 0;
					iend := ci+c.colspan;
					if(iend > tab.ncol)
						iend = tab.ncol;
					for(i:=ci; i < iend; i++)
						curw += w[i];
				
					# padding between spanned cols is free
					cwd -= hsep*(c.colspan-1);
					diff := cwd-curw;
					if(diff <= 0)
						continue;
					# doesn't fit: apportion diff among cols
					# in proportion to their current w
					for(i = ci; i < iend; i++) {
						if(curw == 0)
							w[i] = diff/c.colspan;
						else
							w[i] += diff*w[i]/curw;
					}
				}
			}
			totw += w[ci];
		}
	}
	return totw;
}

layframeset(f: ref Frame, ki: ref Kidinfo)
{
	fwid := f.cr.dx();
	fht := f.cr.dy();
	if(dbg)
		sys->print("layframeset, configuring frame %d wide by %d high\n", fwid, fht);
	(nrow, rowh) := frdimens(ki.rows, fht);
	(ncol, colw) := frdimens(ki.cols, fwid);
	l := ki.kidinfos;
	y := f.cr.min.y;
	for(i := 0; i < nrow; i++) {
		x := f.cr.min.x;
		for(j := 0; j < ncol; j++) {
			if(l == nil)
				return;
			r := Rect(Point(x,y), Point(x+colw[j],y+rowh[i]));
			if(dbg)
				sys->print("kid gets rect (%d,%d)(%d,%d)\n", r.min.x, r.min.y, r.max.x, r.max.y);
			kidki := hd l;
			l = tl l;
			kidf := Frame.newkid(f, kidki, r);
			if(!kidki.isframeset)
				f.kids = kidf :: f.kids;
			if(kidf.framebd != 0) {
				kidf.cr = kidf.r.inset(2);
				drawborder(kidf.cim, kidf.cr, 2, DarkGrey);
			}
			if(kidki.isframeset) {
				layframeset(kidf, kidki);
				for(al := kidf.kids; al != nil; al = tl al)
					f.kids = (hd al) :: f.kids;
			}
			x += colw[j];
		}
		y += rowh[i];
	}
}

# Use the dimension specs in dims to allocate total space t.
# Return (number of dimens, array of allocated space)
frdimens(dims: array of B->Dimen, t: int): (int, array of int)
{
	n := len dims;
	if(n == 1)
		return (1, array[] of {t});
	totpix := 0;
	totpcnt := 0;
	totrel := 0;
	for(i := 0; i < n; i++) {
		v := dims[i].spec();
		kind := dims[i].kind();
		if(v < 0) {
			v = 0;
			dims[i] = Dimen.make(kind, v);
		}
		case kind {
			B->Dpixels => totpix += v;
			B->Dpercent => totpcnt += v;
			B->Drelative => totrel += v;
			B->Dnone => totrel++;
		}
	}
	spix := 1.0;
	spcnt := 1.0;
	min_relu := 0;
	if(totrel > 0)
		min_relu = 30;	# allow for scrollbar (14) and a bit
	relu := real min_relu;
	tt := totpix + (t*totpcnt/100) + totrel*min_relu;
	# want
	#  t ==  totpix*spix + (totpcnt/100)*spcnt*t + totrel*relu
	if(tt < t) {
		# need to expand one of spix, spcnt, relu
		if(totrel == 0) {
			if(totpcnt != 0)
				# spix==1.0, relu==0, solve for spcnt
				spcnt = real ((t-totpix) * 100)/ real (t*totpcnt);
			else
				# relu==0, totpcnt==0, solve for spix
				spix = real t/ real totpix;
		}
		else
			# spix=1.0, spcnt=1.0, solve for relu
			relu += real (t-tt)/ real totrel;
	}
	else {
		# need to contract one or more of spix, spcnt, and have relu==min_relu
		totpixrel := totpix+totrel*min_relu;
		if(totpixrel < t) {
			# spix==1.0, solve for spcnt
			spcnt = real ((t-totpixrel) * 100)/ real (t*totpcnt);
		}
		else {
			# let spix==spcnt, solve
			trest := t - totrel*min_relu;
			if(trest > 0) {
				spcnt = real trest/real (totpix+(t*totpcnt/100));
			}
			else {
				spcnt = real t / real tt;
				relu = 0.0;
			}
			spix = spcnt;
		}
	}
	x := array[n] of int;
	tt = 0;
	for(i = 0; i < n-1; i++) {
		vr := real dims[i].spec();
		case dims[i].kind() {
			B->Dpixels => vr = vr * spix;
			B->Dpercent => vr = vr * real t * spcnt / 100.0;
			B->Drelative => vr = vr * relu;
			B->Dnone => vr = relu;
		}
		x[i] = int vr;
		tt += x[i];
	}
	x[n-1] = t - tt;
	return (n, x);
}

# Return last item of list of items, or nil if no items
lastitem(it: ref Item) : ref Item
{
	ans : ref Item = it;
	for( ; it != nil; it = it.next)
		ans = it;
	return ans;
}

# Lay out table if availw changed or tab changed
checktabsize(f: ref Frame, t: ref Item.Itable, availw: int)
{
	tab := t.table;
	if(availw != tab.availw || int (tab.flags&Lchanged)) {
		sizetable(f, tab, availw);
		t.width = tab.totw + 2*tab.border;
		t.height = tab.toth + 2*tab.border;
		t.ascent = t.height;
	}
}

widthfromspec(wspec: Dimen, availw: int) : int
{
	w := availw;
	spec := wspec.spec();
	case wspec.kind() {
		Dpixels => w = spec;
		Dpercent => w = spec*w/100;
	}
	return w;
}

# An image may have arrived for an image input field
checkffsize(f: ref Frame, i: ref Item, ff: ref Formfield)
{
	if(ff.ftype == Fimage) {
		pick imi := ff.image {
		Iimage =>
			if(imi.ci.mims != nil && ff.ctlid >= 0) {
				pick b := f.controls[ff.ctlid] {
				Cbutton =>
					if(b.pic == nil) {
						b.pic = imi.ci.mims[0].im;
						b.picmask = imi.ci.mims[0].mask;
						w := b.pic.r.dx();
						h := b.pic.r.dy();
						b.r.max.x = b.r.min.x + w;
						b.r.max.y = b.r.min.y + h;
						i.width = w;
						i.height = h;
						i.ascent = h;
					}
				}
			}
		}
	}
	else if(ff.ftype == Fselect) {
		opts := ff.options;
		if(ff.ctlid >=0) {
			pick c := f.controls[ff.ctlid] {
			Cselect =>
				if(len opts != len c.options) {
					nc := Control.newff(f, ff);
					f.controls[ff.ctlid] = nc;
					i.width = nc.r.dx();
					i.height = nc.r.dy();
					i.ascent = lineascent + SELMARGIN;
				}
			}
		}
	}
}

drawall(f: ref Frame)
{
	oclipr := f.cim.clipr;
	f.cim.clipr = f.cr;
	fillbg(f, f.cr);
	origin := f.lptosp(Point(0,0));
	if(dbg > 1)
		sys->print("drawall, cr=(%d,%d,%d,%d), viewr=(%d,%d,%d,%d), origin=(%d,%d)\n",
			f.cr.min.x, f.cr.min.y, f.cr.max.x, f.cr.max.y,
			f.viewr.min.x, f.viewr.min.y, f.viewr.max.x, f.viewr.max.y,
			origin.x, origin.y);
	if(f.layout != nil)
		drawlay(f, f.layout, origin);
	flushc(f);
	f.cim.clipr = oclipr;
}

drawlay(f: ref Frame, lay: ref Lay, origin: Point)
{
	for(l := lay.start.next; l != lay.end; l = l.next)
		drawline(f, origin, l, lay);
}

# Draw line l in frame f, assuming that content's (0,0)
# aligns with layorigin in f.cim.
drawline(f : ref Frame, layorigin : Point, l: ref Line, lay: ref Lay)
{
	im := f.cim;
	o := layorigin.add(l.pos);
	x := o.x;
	y := o.y;
	for(it := l.items; it != nil; it = it.next) {
		pick i := it {
		Itext =>
			fnt := charon_gui->fonts[i.fnt].f;
			yy := y+l.ascent - fnt.ascent + (int i.voff) - Voffbias;
			fgi := colorimage(i.fg);
			im.text(Point(x, yy), fgi, zp, fnt, i.s);
                        #charonCode->drawtext(charon_gui->fonts[DefFnt].f,im,Point(x, yy), fgi, zp, fnt, i.s);
			if(i.ul != ULnone) {
				if(i.ul == ULmid)
					yy += 2*i.ascent/3;
				else
					yy += i.height - 1;
				im.draw(Rect(Point(x,yy),Point(x+i.width,yy+1)), fgi, nil, zp);
			}
		Irule =>
			yy := y + RULESP;
			im.draw(Rect(Point(x,yy),Point(x+i.width,yy+i.size)),
					display.ones, nil, zp);
		Iimage =>
			yy := y;
			if(i.align == Abottom)
				# bottom aligns with baseline
				yy += l.ascent - i.imheight;
			else if(i.align == Amiddle)
				yy += l.ascent - (i.imheight/2);
			drawimg(f, Point(x,yy), i);
		Iformfield =>
			ff := i.formfield;
			if(ff.ctlid >= 0) {
				ctl := f.controls[ff.ctlid];
				dims := ctl.r.max.sub(ctl.r.min);
				# align as text
				yy := y + l.ascent - i.ascent;
				p := Point(x,yy);
				ctl.r = Rect(p, p.add(dims));
				ctl.draw(0);
			}
		Itable =>
			drawtable(f, lay, Point(x,y), i.table);
			t := i.table;
		Ifloat =>
			xx := layorigin.x + lay.margin;
			if(i.side == Aright) {
				xx -= i.x;
				# for main layout of frame, floats hug
				# right edge of frame, not layout
				# (other browsers do that)
				if(f.layout == lay)
					xx += lay.targetwidth;
				else
					xx += lay.width;
			}
			else
				xx += i.x;
			pick fi := i.item {
			Iimage =>
				drawimg(f, Point(xx, layorigin.y + i.y + (int fi.border + int fi.vspace)), fi);
			Itable =>
				drawtable(f, lay, Point(xx, layorigin.y + i.y), fi.table);
			}
		}
		x += it.width;
	}
}

drawimg(f: ref Frame, iorigin: Point, i: ref Item.Iimage)
{
	ci := i.ci;
	im := f.cim;
	iorigin.x += int i.hspace + int i.border;
	# y coord is already adjusted for border and vspace
	if(ci.mims != nil) {
		r := Rect(iorigin, iorigin.add(Point(i.imwidth,i.imheight)));
		if(i.ctlid >= 0) {
			# animated
			c := f.controls[i.ctlid];
			dims := c.r.max.sub(c.r.min);
			c.r = Rect(iorigin, iorigin.add(dims));
			pick ac := c {
			Canimimage =>
				ac.redraw = 1;
				ac.bg = f.layout.background;
			}
			c.draw(0);
		}
		else {
			mim := ci.mims[0];
			iorigin = iorigin.add(mim.origin);
			im.draw(r, mim.im, mim.mask, zp);
		}
		if(i.border != byte 0) {
			if(i.anchorid != 0)
				bdcol := f.doc.link;
			else
				bdcol = Black;
			drawborder(im, r, int i.border, bdcol);
		}
	}
	else if((CU->config).imagelvl == CU->ImgNone && i.altrep != "") {
		fnt := charon_gui->fonts[DefFnt].f;
		yy := iorigin.y+(i.imheight-fnt.height)/2;
		xx := iorigin.x + (i.width-fnt.width(i.altrep))/2;
		if(i.anchorid != 0)
			col := f.doc.link;
		else
			col = DarkGrey;
		fgi := colorimage(col);
		im.text(Point(xx, yy), fgi, zp, fnt, i.altrep);
	}
}

drawtable(f : ref Frame, parentlay: ref Lay, torigin: Point, tab: ref Table)
{
	if(tab.ncol == 0 || tab.nrow == 0)
		return;
	im := f.cim;
	(hsp, vsp, pad, bd, cbd, hsep, vsep) := tableparams(tab);
	x := torigin.x;
	y := torigin.y;
	capy := y;
	boxy := y;
	if(tab.caption_place == Abottom)
		capy = y+tab.toth-tab.caph+vsp;
	else
		boxy = y+tab.caph;
	if(tab.background.image != parentlay.background.image ||
	   tab.background.color != parentlay.background.color) {
		bgi := colorimage(tab.background.color);
		im.draw(((x,boxy),(x+tab.totw,boxy+tab.toth-tab.caph)),
			bgi, nil, zp);
	}
	if(bd != 0)
		drawborder(im, ((x+bd,boxy+bd),(x+tab.totw-bd,boxy+tab.toth-tab.caph-bd)),
			1, Black);
	for(cl := tab.cells; cl != nil; cl = tl cl) {
		c := hd cl;
		CU->assert(c.layid >= 0 && c.layid < len f.sublays);
		clay := f.sublays[c.layid];
		if(clay == nil)
			continue;
		cx := x + tab.cols[c.col].pos.x;
		cy := y + tab.rows[c.row].pos.y;
		wd := cellwidth(tab, c, hsep);
		ht := cellheight(tab, c, vsep);
		if(c.background.color != tab.background.color) {
			bgi := colorimage(c.background.color);
			im.draw(((cx-pad,cy-pad),(cx+wd+pad,cy+ht+pad)),
				bgi, nil, zp);
		}
		if(bd != 0)
			drawborder(im, ((cx-pad+1,cy-pad+1),(cx+wd+pad-1,cy+ht+pad-1)),
				1, Black);
		if(c.align.valign != Atop && c.align.valign != Abaseline) {
			n := ht - clay.height;
			if(c.align.valign == Amiddle)
				cy += n/2;
			else if(c.align.valign == Abottom)
				cy += n;
		}
		if(dbgtab)
			sys->print("drawtable %d cell %d at (%d,%d)\n",
				tab.tableid, c.cellid, cx, cy);
		drawlay(f, clay, Point(cx,cy));
	}
	if(tab.caption_lay >= 0) {
		caplay := f.sublays[tab.caption_lay];
		capx := x;
		if(caplay.width < tab.totw)
			capx += (tab.totw-caplay.width) / 2;
		drawlay(f, caplay, Point(capx,capy));
	}
}

# Draw border of width n just outside r, using src color
drawborder(im: ref Image, r: Rect, n, color: int)
{
	x := r.min.x-n;
	y := r.min.y - n;
	xr := r.max.x+n;
	ybi := r.max.y;
	src := colorimage(color);
	im.draw((Point(x,y),Point(xr,y+n)), src, nil, zp);				# top
	im.draw((Point(x,ybi),Point(xr,ybi+n)), src, nil, zp);			# bottom
	im.draw((Point(x,y+n),Point(x+n,ybi)), src, nil, zp);			# left
	im.draw((Point(xr-n,y+n),Point(xr,ybi)), src, nil, zp);			# right
}

# Draw relief border just outside r, width 2 border,
# colors white/lightgrey/darkgrey/black
# to give raised relief (if raised != 0) or sunken.
drawrelief(im: ref Image, r: Rect, raised: int)
{
	# ((x,y),(xr,yb)) == r
	x := r.min.x;
	x1 := x-1;
	x2 := x-2;
	xr := r.max.x;
	xr1 := xr+1;
	xr2 := xr+2;
	y := r.min.y;
	y1 := y-1;
	y2 := y-2;
	yb := r.max.y;
	yb1 := yb+1;
	yb2 := yb+2;

	# colors for top/left outside, top/left inside, bottom/right outside, bottom/right inside
	tlo, tli, bro, bri: ref Image;
	if(raised) {
		tlo = colorimage(Grey);
		tli = colorimage(White);
		bro = colorimage(Black);
		bri = colorimage(DarkGrey);
	}
	else {
		tlo = colorimage(DarkGrey);
		tli = colorimage(Black);
		bro = colorimage(White);
		bri = colorimage(Grey);
	}

	im.draw((Point(x2,y2), Point(xr1,y1)), tlo, nil, zp);		# top outside
	im.draw((Point(x1,y1), Point(xr,y)), tli, nil, zp);			# top inside
	im.draw((Point(x2,y1), Point(x1,yb1)), tlo, nil, zp);		# left outside
	im.draw((Point(x1,y), Point(x,yb)), tli, nil, zp);			# left inside
	im.draw((Point(xr,y1),Point(xr1,yb)), bri, nil, zp);		# right inside
	im.draw((Point(xr1,y),Point(xr2,yb1)), bro, nil, zp);		# right outside
	im.draw((Point(x1,yb),Point(xr1,yb1)), bri, nil, zp);		# bottom inside
	im.draw((Point(x,yb1),Point(xr2,yb2)), bro, nil, zp);		# bottom outside
}

# Fill r with color
drawfill(im: ref Image, r: Rect, color: int)
{
	im.draw(r, colorimage(color), nil, zp);
}

# Draw string in default font at p
drawstring(im: ref Image, p: Point, s: string)
{
	im.text(p, colorimage(Black), zp, charon_gui->fonts[DefFnt].f, s);
        #charonCode->drawtext(charon_gui->fonts[DefFnt].f,im,p, colorimage(Black), zp, charon_gui->fonts[DefFnt].f,s);
}

# Return (width, height) of string in default font
measurestring(s: string) : Point
{
	f := charon_gui->fonts[DefFnt].f;
	return (f.width(s), f.height);
}

# Mark as "changed" everything with change flags on the loc path
markchanges(loc: ref Loc)
{
	lastf : ref Frame = nil;
	for(i := 0; i < loc.n; i++) {
		case loc.le[i].kind {
		LEframe =>
			lastf = loc.le[i].frame;
			lastf.layout.flags |= Lchanged;
		LEline =>
			loc.le[i].line.flags |= Lchanged;
		LEitem =>
			pick it := loc.le[i].item {
			Itable =>
				it.table.flags |= Lchanged;
			Ifloat =>
				# whole layout will be redone if layout changes
				# and there are any floats
				;
			}
		LEtablecell =>
			if(lastf == nil)
				CU->raise("EXInternal: markchanges no lastf");
			c := loc.le[i].tcell;
			clay := lastf.sublays[c.layid];
			if(clay != nil)
				clay.flags |= Lchanged;
		}
	}
}

# one-item cache for colorimage
prevrgb := -1;
prevrgbimage : ref Image = nil;

colorimage(rgb: int) : ref Image
{
	if(rgb == prevrgb)
		return prevrgbimage;
	prevrgb = rgb;
	if(rgb == Black)
		prevrgbimage = display.ones;
	else if(rgb == White)
		prevrgbimage = display.zeros;
	else {
		hv := rgb % NCOLHASH;
		xhd := colorhashtab[hv];
		x := xhd;
		while(x != nil && x.rgb  != rgb)
			x = x.next;
		if(x == nil) {
			pix := I->closest_rgbpix((rgb>>16)&255, (rgb>>8)&255, rgb&255);
			im := display.color(pix);
			if(im == nil)
				CU->raise(sys->sprint("exLayout: can't allocate color %d: %r", pix));
			x = ref Colornode(rgb, im, xhd);
			colorhashtab[hv] = x;
		}
		prevrgbimage = x.im;
	}
	return prevrgbimage;
}

# Use f.background.image (if not nil) or f.background.color to fill r (in cim coord system)
# with background color.
fillbg(f: ref Frame, r: Rect)
{
	bgi: ref Image;
	bgci := f.doc.background.image;
	if(bgci != nil && bgci.mims != nil)
		bgi = bgci.mims[0].im;
	if(bgi == nil)
		bgi = colorimage(f.doc.background.color);
	f.cim.draw(r, bgi, nil, f.viewr.min);
}

TRIup, TRIdown, TRIleft, TRIright: con iota;
# Assume r is a square
drawtriangle(im: ref Image, r: Rect, kind, style: int)
{
	drawfill(im, r, Grey);
	b := r.max.x - r.min.x;
	if(b < 4)
		return;
	b2 := b/2;
	bm2 := b-ReliefBd;
	p := array[3] of Point;
	col012, col20 : ref Image;
	d := colorimage(DarkGrey);
	l := colorimage(White);
	case kind {
	TRIup =>
		p[0] = Point(b2, ReliefBd);
		p[1] = Point(bm2,bm2);
		p[2] = Point(ReliefBd,bm2);
		col012 = d;
		col20 = l;
	TRIdown =>
		p[0] = Point(b2,bm2);
		p[1] = Point(ReliefBd,ReliefBd);
		p[2] = Point(bm2,ReliefBd);
		col012 = l;
		col20 = d;
	TRIleft =>
		p[0] = Point(bm2, ReliefBd);
		p[1] = Point(bm2, bm2);
		p[2] = Point(ReliefBd,b2);
		col012 = d;
		col20 = l;
	TRIright =>
		p[0] = Point(ReliefBd,bm2);
		p[1] = Point(ReliefBd,ReliefBd);
		p[2] = Point(bm2,b2);
		col012 = l;
		col20 = d;
	}
	if(style == ReliefSunk) {
		t := col012;
		col012 = col20;
		col20 = t;
	}
	for(i := 0; i < 3; i++)
		p[i] = p[i].add(r.min);
	im.fillpoly(p, ~0, colorimage(Grey), zp);
	im.line(p[0], p[1], 0, 0, ReliefBd/2, col012, zp);
	im.line(p[1], p[2], 0, 0, ReliefBd/2, col012, zp);
	im.line(p[2], p[0], 0, 0, ReliefBd/2, col20, zp);
}

flushc(f: ref Frame)
{
	f.cim.flush(D->Flushnow);
}

max(a,b: int) : int
{
	if(a > b)
		return a;
	return b;
}

min(a,b: int) : int
{
	if(a < b)
		return a;
	return b;
}

abs(a: int) : int
{
	if(a < 0)
		return -a;
	return a;
}

Frame.new() : ref Frame
{
	f := ref Frame;
	f.parent = nil;
	f.cim = nil;
	f.r = Rect(Point(0, 0), Point(0, 0));
	f.animpid = 0;
	f.reset();
	return f;
}

Frame.newkid(parent: ref Frame, ki: ref Kidinfo, r: Rect) : ref Frame
{
	f := ref Frame;
	f.parent = parent;
	f.cim = parent.cim;
	f.r = r;
	f.animpid = 0;
	f.reset();
	f.src = ki.src;
	f.name = ki.name;
	f.marginw = ki.marginw;
	f.marginh = ki.marginh;
	f.framebd = ki.framebd;
	f.flags = ki.flags;
	return f;
}

# Note: f.parent, f.cim and f.r should not be reset
# And if f.parent is true, don't reset params set in frameset.
Frame.reset(f: self ref Frame)
{
	f.id = ++frameid;
	f.doc = nil;
	if(f.parent == nil) {
		f.src = nil;
		f.name = "";
		f.marginw = FRMARGIN;
		f.marginh = FRMARGIN;
		f.framebd = 0;
		f.flags = FRvscroll | FRhscrollauto;
	}
	f.layout = nil;
	f.sublays = nil;
	f.sublayid = 0;
	f.controls = nil;
	f.controlid = 0;
	f.cr = f.r;
	f.viewr = Rect(Point(0, 0), Point(0, 0));
	f.totalr = f.viewr;
	f.vscr = nil;
	f.hscr = nil;
	hadkids := (f.kids != nil);
	f.kids = nil;
	if(f.animpid != 0)
		CU->kill(f.animpid, 0);
	if(J != nil && hadkids)
		J->frametreechanged(f);
	f.animpid = 0;
}

Frame.addcontrol(f: self ref Frame, c: ref Control) : int
{
	if(len f.controls <= f.controlid) {
		newcontrols := array[len f.controls + 30] of ref Control;
		newcontrols[0:] = f.controls;
		f.controls = newcontrols;
	}
	f.controls[f.controlid] = c;
	ans := f.controlid++;
	return ans;
}

Frame.xscroll(f: self ref Frame, kind, val: int)
{
	newx := f.viewr.min.x;
	case kind {
	CAscrollpage =>
		newx += val*(f.cr.dx()*8/10);
	CAscrollline =>
		newx += val*f.cr.dx()/10;
	CAscrolldelta =>
		newx += val*f.totalr.max.x/f.cr.dx();
	CAscrollabs =>
		newx = val;
	}
	newx = max(0, min(newx, f.totalr.max.x));
	if(newx != f.viewr.min.x) {
		f.viewr.min.x = newx;
		fixframegeom(f);
		drawall(f);
	}
}

# Don't actually scroll by "page" and "line",
# But rather, 80% and 10%, which give more
# context in the first case, and more motion
# in the second.
Frame.yscroll(f: self ref Frame, kind, val: int)
{
	newy := f.viewr.min.y;
	case kind {
	CAscrollpage =>
		newy += val*(f.cr.dy()*8/10);
	CAscrollline =>
		newy += val*f.cr.dy()/20;
	CAscrolldelta =>
		newy += val*f.totalr.max.y/f.cr.dy();
	CAscrollabs =>
		newy = val;
	}
	newy = max(0, min(newy, f.totalr.max.y));
	if(newy != f.viewr.min.y) {
		f.viewr.min.y = newy;
		fixframegeom(f);
		drawall(f);
	}
}

# Convert layout coords (where (0,0) is top left of layout)
# to screen coords (i.e., coord system of mouse, f.cr, etc.)
Frame.sptolp(f: self ref Frame, sp: Point) : Point
{
	return f.viewr.min.add(sp.sub(f.cr.min));
}

# Reverse translation of sptolp
Frame.lptosp(f: self ref Frame, lp: Point) : Point
{
	return lp.add(f.cr.min.sub(f.viewr.min));
}

# Return Loc of Item or Scrollbar containing p (p in screen coords)
# or item it, if that is not nil.
Frame.find(f: self ref Frame, p: Point, it: ref Item) : ref Loc
{
	return framefind(Loc.new(), f, p, it);
}

# Find it (if non-nil) or place where p is (known to be inside f's layout).
framefind(loc: ref Loc, f: ref Frame, p: Point, it: ref Item) : ref Loc
{
	loc.add(LEframe, f.r.min);
	loc.le[loc.n-1].frame = f;
	if(it == nil) {
		if(f.vscr != nil && p.in(f.vscr.r)) {
			loc.add(LEcontrol, f.vscr.r.min);
			loc.le[loc.n-1].control = f.vscr;
			loc.pos = p.sub(f.vscr.r.min);
			return loc;
		}
		if(f.hscr != nil && p.in(f.hscr.r)) {
			loc.add(LEcontrol, f.hscr.r.min);
			loc.le[loc.n-1].control = f.hscr;
			loc.pos = p.sub(f.hscr.r.min);
			return loc;
		}
	}
	if(it != nil || p.in(f.cr)) {
		lay := f.layout;
		if(f.kids != nil) {
			for(fl := f.kids; fl != nil; fl = tl fl) {
				kf := hd fl;
				try := framefind(loc, kf, p, it);
				if(try != nil)
					return try;
			}
		}
		else if(lay != nil)
			return layfind(loc, f, lay, f.lptosp(Point(0,0)), p, it);
	}
	return nil;
}

# Find it (if non-nil) or place where p is (known to be inside f's layout).
# p (in screen coords), lay offset by origin also in screen coords
layfind(loc: ref Loc, f: ref Frame, lay: ref Lay, origin, p: Point, it: ref Item) : ref Loc
{
	for(flist := lay.floats; flist != nil; flist = tl flist) {
		fl := hd flist;
		fymin := fl.y+origin.y;
		fymax := fymin + fl.item.height;
		inside := 0;
		xx : int;
		if(it != nil || (fymin <= p.y && p.y < fymax)) {
			xx = origin.x + lay.margin;
			if(fl.side == Aright) {
				if(lay == f.layout)
					xx += f.cr.dx() - fl.x;
				else
					xx += lay.width - fl.x;
			}
			else
				xx += fl.x;
			if(p.x >= xx && p.x < xx+fl.item.width)
					inside = 1;
		}
		fp := Point(xx,fymin);
		match := 0;
		if(it != nil) {
			pick fi := fl.item {
			Itable =>
				loc.add(LEitem, fp);
				loc.le[loc.n-1].item = fl;
				loc.pos = p.sub(fp);
				lloc := tablefind(loc, f, fi, fp, p, it);
				if(lloc != nil)
					return lloc;
			Iimage =>
				match = (it == fl || it == fl.item);
			}
		}
		if(match || inside) {
			loc.add(LEitem, fp);
			loc.le[loc.n-1].item = fl;
			loc.pos = p.sub(fp);
			if(it == fl.item) {
				loc.add(LEitem, fp);
				loc.le[loc.n-1].item = fl.item;
			}
			if(inside) {
				pick fi := fl.item {
				Itable =>
					loc = tablefind(loc, f, fi, fp, p, it);
				}
			}
			return loc;
		}
	}
	for(l :=lay.start; l != nil; l = l.next) {
		o := origin.add(l.pos);
		if(it != nil || (o.y <= p.y && p.y < o.y+l.height)) {
			lloc := linefind(loc, f, l, o, p, it);
			if(lloc != nil)
				return lloc;
			if(it == nil && o.y + l.height >= p.y)
				break;
		}
	}
	return nil;
}

# p (in screen coords), line at o, also in screen coords
linefind(loc: ref Loc, f: ref Frame, l: ref Line, o, p: Point, it: ref Item) : ref Loc
{
	loc.add(LEline, o);
	loc.le[loc.n-1].line = l;
	x := o.x;
	y := o.y;
	inside := 0;
	for(i := l.items; i != nil; i = i.next) {
		if(it != nil || (x <= p.x && p.x < x+i.width)) {
			yy := y;
			h := 0;
			pick pi := i {
			Itext =>
				fnt := charon_gui->fonts[pi.fnt].f;
				yy += l.ascent - fnt.ascent + (int pi.voff) - Voffbias;
				h = fnt.height;
			Irule =>
				h = pi.size;
			Iimage =>
				yy = y;
				if(pi.align == Abottom)
					yy += l.ascent - pi.imheight;
				else if(pi.align == Amiddle)
					yy += l.ascent - (pi.imheight/2);
				h = pi.imheight;
			Iformfield =>
				h = pi.height;
				yy += l.ascent - pi.ascent;
				if(it != nil) {
					if(it == pi.formfield.image) {
						loc.add(LEitem, Point(x,yy));
						loc.le[loc.n-1].item = i;
						loc.add(LEitem, Point(x,yy));
						loc.le[loc.n-1].item = it;
						loc.pos = (Point(0,0));	# doesn't matter, its an 'it' test
						return loc;
					}
				}
				else if(yy < p.y && p.y < yy+h && pi.formfield.ctlid >= 0) {
					loc.add(LEcontrol, Point(x,yy));
					loc.le[loc.n-1].control = f.controls[pi.formfield.ctlid];
					loc.pos = p.sub(Point(x,yy));
					return loc;
				}
			Itable =>
				lloc := tablefind(loc, f, pi, Point(x,y), p, it);
				if(lloc != nil)
					return lloc;
				# else leave h==0 so p test will fail

			# floats were handled separately. nulls can be picked by 'it' test
			# leave h==0, so p test will fail
			}
			if(it == i || (it == nil && yy <= p.y && p.y < yy+h)) {
				loc.add(LEitem, Point(x,yy));
				loc.le[loc.n-1].item = i;
				loc.pos = p.sub(Point(x,yy));
				return loc;
			}
			if(it == nil)
				return nil;
		}
		x += i.width;
		if(it == nil && x >= p.x)
			break;
	}
	loc.n--;
	return nil;
}

tablefind(loc: ref Loc, f: ref Frame, ti: ref Item.Itable, torigin: Point, p: Point, it: ref Item) : ref Loc
{
	loc.add(LEitem, torigin);
	loc.le[loc.n-1].item = ti;
	t := ti.table;
	(hsp, vsp, pad, bd, cbd, hsep, vsep) := tableparams(t);
	if(t.caption_lay >= 0) {
		caplay := f.sublays[t.caption_lay];
		capy := torigin.y;
		if(t.caption_place == Abottom)
			capy += t.toth-t.caph+vsp;
		lloc := layfind(loc, f, caplay, Point(torigin.x,capy), p, it);
		if(lloc != nil)
			return lloc;
	}
	for(cl := t.cells; cl != nil; cl = tl cl) {
		c := hd cl;
		clay := f.sublays[c.layid];
		if(clay == nil)
			continue;
		cx := torigin.x + t.cols[c.col].pos.x;
		cy := torigin.y + t.rows[c.row].pos.y;
		wd := cellwidth(t, c, hsep);
		ht := cellheight(t, c, vsep);
		if(it == nil && !p.in(Rect(Point(cx,cy),Point(cx+wd,cy+ht))))
			continue;
		if(c.align.valign != Atop && c.align.valign != Abaseline) {
			n := ht - clay.height;
			if(c.align.valign == Amiddle)
				cy += n/2;
			else if(c.align.valign == Abottom)
				cy += n;
		}
		loc.add(LEtablecell, Point(cx,cy));
		loc.le[loc.n-1].tcell = c;
		lloc := layfind(loc, f, clay, Point(cx,cy), p, it);
		if(lloc != nil)
			return lloc;
		loc.n--;
		if(it == nil)
			return nil;
	}
	loc.n--;
	return nil;
}

# (called from jscript)
# 'it' is an Iimage item in frame f whose image is to be switched
# to come from the src URL.
#
# For now, assume this is called only after the entire build process
# has finished.  Also, only handle the case where the image has
# been preloaded and is in the cache now.  This isn't right (BUG), but will
# cover most of the cases of extant image swapping, and besides,
# image swapping is mostly cosmetic anyway.
# 
# For now, pay no attention to scaling issues or animation issues.
Frame.swapimage(f: self ref Frame, im: ref Item.Iimage, src: string)
{
	u := U->makeurl(src);
	if(u.scheme == U->UNKNOWN)
		return;
	u.makeabsolute(f.doc.base);
	# width=height=0 finds u if in cache
	newci := CImage.new(u, nil, 0, 0);
	cachedci := (CU->imcache).look(newci);
	if(cachedci == nil || cachedci.mims == nil)
		return;
	im.ci = cachedci;

	# we're assuming image will have same dimensions
	# as one that is replaced, so no relayout is needed;
	# otherwise need to call haveimage() instead of drawall()
	# Netscape scales replacement image to size of replaced image

	drawall(f);
}

Control.newff(f: ref Frame, ff: ref B->Formfield) : ref Control
{
	ans : ref Control = nil;
	case ff.ftype {
	Ftext or Fpassword or Ftextarea =>
		nh := ff.size;
		nv := 1;
		linewrap := 0;
		if(ff.ftype == Ftextarea) {
			nh = ff.cols;
			nv = ff.rows;
			linewrap = 1;
		}
		ans = Control.newentry(f, nh, nv, linewrap);
		if(ff.ftype == Fpassword)
			ans.flags |= CFsecure;
		ans.entryset(ff.value);
	Fcheckbox or Fradio =>
		ans = Control.newcheckbox(f, ff.ftype==Fradio);
		if((ff.flags&B->FFchecked) != byte 0)
			ans.flags |= CFactive;
	Fsubmit or Fimage or Freset or Fbutton =>
		if(ff.image == nil)
			ans = Control.newbutton(f, nil, nil, ff.value, nil, 0, 1);
		else {
			pick i := ff.image {
			Iimage =>
				pic, picmask : ref Image;
				if(i.ci.mims != nil) {
					pic = i.ci.mims[0].im;
					picmask = i.ci.mims[0].mask;
				}
				lab := "";
				if((CU->config).imagelvl == CU->ImgNone) {
					lab = i.altrep;
					i = nil;
				}
				ans = Control.newbutton(f, pic, picmask, lab, i, 0, 0);
			}
		}
	Fselect =>
		n := len ff.options;
		if(n > 0) {
			ao := array[n] of Option;
			l := ff.options;
			for(i := 0; i < n; i++) {
				o := hd l;
				# these are copied, so selected can be used for current state
				ao[i].selected = o.selected;
				ao[i].value = o.value;
				ao[i].display = o.display;
				l = tl l;
			}
			nvis := ff.size;
			if(nvis == 0)
				nvis = min(n, 4);
			ans = Control.newselect(f, nvis, ao);
		}
	Ffile =>
		if(dbg)
			sys->print("warning: unimplemented file form field\n");
	}
	if(ans != nil)
		ans.ff = ff;
	return ans;
}

Control.newscroll(f: ref Frame, isvert, length, breadth: int) : ref Control
{
	# need room for at least two squares and 2 borders of size 2
	if(length < 12) {
		breadth = 0;
		length = 0;
	}
	else if(breadth*2 + 4 > length)
		breadth = (length - 4) / 2;
	maxpt : Point;
	flags := CFenabled;
	if(isvert) {
		maxpt = Point(breadth, length);
		flags |= CFscrvert;
	}
	else
		maxpt = Point(length, breadth);
	return ref Control.Cscrollbar(f, nil, Rect(zp,maxpt), flags, 0, 0, 0, 0, nil);
}

Control.newentry(f: ref Frame, nh, nv, linewrap: int) : ref Control
{
	w := ctlcharspace*nh + 2*ENTHMARGIN;
	h := ctllinespace*nv + 2*ENTVMARGIN;
	return ref Control.Centry(f, nil, Rect(zp,Point(w,h)), CFenabled, "", (0, 0), 0, linewrap, 0);
}

Control.newbutton(f: ref Frame, pic, picmask: ref Image, lab: string, it: ref Item.Iimage, candisable, dorelief: int) : ref Control
{
	dpic, dpicmask: ref Image;
	w := 0;
	h := 0;
	if(pic != nil) {
		w = pic.r.dx();
		h = pic.r.dy();
	}
	else if(it != nil) {
		w = it.imwidth;
		h = it.imheight;
	}
	else {
		w = charon_gui->fonts[charon_gui->CtlFnt].f.width(lab);
		h = ctllinespace;
	}
	if(dorelief) {
		# form image buttons are shown without margins in other browsers
		w += 2*BUTMARGIN;
		h += 2*BUTMARGIN;
	}
	r := Rect(zp, Point(w,h));
	if(candisable && pic != nil) {
		# make "greyed out" image:
		#	- convert pic to monochrome (ones where pic is non-white)
		#	- draw pic in White, then DarkGrey shifted (-1,-1) and use
		#	    union of those two areas as mask
		dpicmask = display.newimage(pic.r, 0, 0, D->White);
		dpic = display.newimage(pic.r, pic.ldepth, 0, D->White);
		dpic.draw(dpic.r, colorimage(White), pic, zp);
		dpicmask.draw(dpicmask.r, display.ones, pic, zp);
		dpic.draw(dpic.r.addpt(Point(-1,-1)), colorimage(DarkGrey), pic, zp);
		dpicmask.draw(dpicmask.r.addpt(Point(-1,-1)), display.ones, pic, zp);
	}
	b := ref Control.Cbutton(f, nil, r, CFenabled, pic, picmask, dpic, dpicmask, lab, dorelief);
	return b;
}

Control.newcheckbox(f: ref Frame, isradio: int) : ref Control
{
	return ref Control.Ccheckbox(f, nil, Rect((0,0),(CBOXWID,CBOXHT)), CFenabled, isradio);
}

Control.newselect(f: ref Frame, nvis: int, options: array of B->Option) : ref Control
{
	fnt := charon_gui->fonts[charon_gui->CtlFnt].f;
	w := 0;
	for(i := 0; i < len options; i++)
		w = max(w, fnt.width(options[i].display));
	w += 2*SELMARGIN;
	h := ctllinespace*nvis + 2*SELMARGIN;
	scr: ref Control;
	if(nvis < len options) {
		scr = Control.newscroll(f, 1, h, SCRFBREADTH);
		scr.r.addpt(Point(w,0));
		w += SCRFBREADTH;
	}
	ans := ref Control.Cselect(f, nil, Rect(zp, Point(w,h)), CFenabled, scr, nvis, 0, options);
	if(scr != nil) {
		pick pscr := scr {
		Cscrollbar =>
			pscr.ctl = ans;
		}
		scr.scrollset(0, nvis, len options,1);
	}
	return ans;
}

Control.newlistbox(f: ref Frame, nrow, ncol: int, options: array of B->Option) : ref Control
{
	fnt := charon_gui->fonts[charon_gui->CtlFnt].f;
	w := charspace*ncol + 2*SELMARGIN;
	h := fnt.height*nrow + 2*SELMARGIN;

	vscr: ref Control = nil;
	#if(nrow < len options) {
		vscr = Control.newscroll(f, 1, h+SCRFBREADTH, SCRFBREADTH);
		vscr.r.addpt(Point(w-SCRFBREADTH,0));
		w += SCRFBREADTH;
	#}

	maxw := 0;
	for(i := 0; i < len options; i++)
		maxw = max(maxw, fnt.width(options[i].display));

	hscr: ref Control = nil;
	#if(w < maxw) {
		hscr = Control.newscroll(f, 0, w-SCRFBREADTH, SCRFBREADTH);
		hscr.r.addpt(Point(0, h-SCRBREADTH));
		h += SCRFBREADTH;
	#}

	ans := ref Control.Clistbox(f, nil, Rect(zp, Point(w,h)), CFenabled, hscr, vscr, nrow, 0, 0, maxw/charspace, options, nil);
	if(vscr != nil) {
		pick pscr := vscr {
		Cscrollbar =>
			pscr.ctl = ans;
		}
		vscr.scrollset(0, nrow, len options, 1);
	}
	if(hscr != nil) {
		pick pscr := hscr {
		Cscrollbar =>
			pscr.ctl = ans;
		}
		hscr.scrollset(0, w-SCRFBREADTH, maxw, 1);
	}
	return ans;	
}

Control.newanimimage(f: ref Frame, cim: ref CU->CImage, bg: Background) : ref Control
{
	return ref Control.Canimimage(f, nil, Rect((0,0),(cim.width,cim.height)), 0, cim, 0, 0, bg);
}

Control.newprogbox(f: ref Frame) : ref Control
{
	return ref Control.Cprogbox(f, nil, Rect((0,0),(PBOXWID,PBOXHT)), CFenabled, 
		CU->Punused, 0, -1, nil, nil);
}

Control.newlabel(f: ref Frame, s: string) : ref Control
{
	w := charon_gui->fonts[DefFnt].f.width(s);
	h := ctllinespace + 2*ENTVMARGIN;	# give it same height as an entry box
	return ref Control.Clabel(f, nil, Rect(zp,Point(w,h)), 0, s);
}

Control.disable(c: self ref Control)
{
	if(c.flags & CFenabled) {
		win := c.f.cim;
		c.flags &= ~CFenabled;
		if(c.f.cim != nil)
			c.draw(1);
	}
}

Control.enable(c: self ref Control)
{
	if(!(c.flags & CFenabled)) {
		c.flags |= CFenabled;
		if(c.f.cim != nil)
			c.draw(1);
	}
}

changeevent(c: ref Control)
{
	onchange := 0;
	pick pc := c {
	Centry =>
		onchange = pc.onchange;
		pc.onchange = 0;
# this code reproduced Navigator 2 bug
# changes to Select Formfield selection only resulted in onchange event upon
# loss of focus.  Now handled by domouse() code so event can be raised
# immediately
#	Cselect =>
#		onchange = pc.onchange;
#		pc.onchange = 0;
	}
	if(onchange) {
		se := ref E->ScriptEvent(E->SEonchange, c.f.id, c.ff.form.formid, c.ff.fieldid, -1, -1, -1, -1, 1, nil);
		J->jevchan <-= se;
	}
}

blurfocusevent(c: ref Control, kind: int)
{
	if((CU->config).doscripts && c.ff != nil && c.ff.events != nil) {
		if(kind == E->SEonblur)
			changeevent(c);
		se := ref E->ScriptEvent(kind, c.f.id, c.ff.form.formid, c.ff.fieldid, -1, -1, -1, -1, 1, nil);
		J->jevchan <-= se;
	}
}

Control.losefocus(c: self ref Control, popupactive: int)
{
	if(c.flags & CFhasfocus) {
		c.flags &= ~CFhasfocus;
		if(c.f.cim != nil) {
			if(!popupactive)
				blurfocusevent(c, E->SEonblur);
			c.draw(1);
		}
	}
}

Control.gainfocus(c: self ref Control, popupactive: int)
{
	if(!(c.flags & CFhasfocus)) {
		c.flags |= CFhasfocus;
		if(c.f.cim != nil) {
			if(!popupactive)
				blurfocusevent(c, E->SEonfocus);
			c.draw(1);
		}
	}
}

Control.scrollset(c: self ref Control, v1, v2, vmax, mindelta: int)
{
	pick sc := c {
	Cscrollbar =>
		sc.mindelta = mindelta;
		if(vmax <= 0) {
			sc.top = 0;
			sc.bot = 0;
		}
		else {
			if(v1 < 0)
				v1 = 0;
			if(v2 > vmax)
				v2 = vmax;
			if(v1 > v2)
				v1 = v2;
			length, breadth: int;
			if(sc.flags&CFscrvert) {
				length = sc.r.max.y - sc.r.min.y;
				breadth = sc.r.max.x - sc.r.min.x;
			}
			else {
				length = sc.r.max.x - sc.r.min.x;
				breadth = sc.r.max.y - sc.r.min.y;
			}
			l := length - 2*breadth;
			if(l < 0)
				CU->raise("EXInternal: negative scrollbar trough");
			sc.top = l*v1/vmax;
			sc.bot = l*(vmax-v2)/vmax;
		}
		if(sc.f.cim != nil)
			sc.draw(1);
	}
}

CTRLMASK : con 16r1f;
DEL : con 16r7f;
TAB : con '\t';
CR: con '\n';
HOME, END, UPP, DOW, LEF, RIG, PDOWN, PUP : con (iota + 57360);
SCREENUP : con 57443;
SCREENDM : con 57444;
PINTS : con 57449;
NUMLOC : con 57471;
CALLSE : con 57446;
KF1, KF2, KF3, KF4, KF5, KF6, KF7, KF8, KF9, KF10, KF11, KF12, KF13 : con (iota + 57409);
Control.dokey(ctl: self ref Control, keychar: int) : int
{
	if(!(ctl.flags&CFenabled))
		return CAnone;
	ans := CAnone;
	pick c := ctl {
	Centry =>
		olds := c.s;
		slen := len c.s;
		(sels, sele) := normalsel(c.sel);
		(osels, osele) := (sels, sele);
		case keychar {
			('a' & CTRLMASK) or HOME =>
				(sels, sele) = (0, 0);
			('e' & CTRLMASK) or END =>
				(sels, sele) = (slen, slen);
			'f' & CTRLMASK or RIG =>
				if(sele < slen)
					(sels, sele) = (sele+1, sele+1);
			'b' & CTRLMASK or LEF =>
				if(sels > 0)
					(sels, sele) = (sels-1, sels-1);
			'u' & CTRLMASK =>
				entrydelrange(c, 0, slen);
				(sels, sele) = c.sel;
			'v' & CTRLMASK =>
				entrysetfromsnarf(c);
				(sels, sele) = c.sel;
			'h' & CTRLMASK =>
				if (sels != sele)
					entrydelrange(c, sels, sele);
				else if(sels > 0)
					entrydelrange(c, sels-1, sels);
				(sels, sele) = c.sel;
			DEL =>
				# actually, won't get here now because main
				# window picks off del as key to exit charon
				if (sels != sele)
					entrydelrange(c, sels, sele);
				else if(sels < len c.s)
					entrydelrange(c, sels, sels+1);
				(sels, sele) = c.sel;
			TAB =>
				ans = CAtabkey;
			KF1 or KF2 or KF3 or KF4 or KF5 or KF6 or KF7 or KF8 
			or KF9 or KF10 or KF11 or KF12 or KF13 or	
			PINTS or NUMLOC or CALLSE or SCREENUP or
			PUP or PDOWN or
			SCREENDM =>
				;
			* =>
				if(keychar == CR) {
					if(c.linewrap)
						keychar = '\n';
					else
						ans = CAreturnkey;
				}
				if(keychar > CTRLMASK || (keychar == '\n' && c.linewrap)) {
					if (sels != sele) {
						entrydelrange(c, sels, sele);
						(sels, sele) = c.sel;
					}
					slen = len c.s;
					c.s[slen] = 0;	# expand string by 1 char
					for(k := slen; k > sels; k--)
						c.s[k] = c.s[k-1];
					c.s[sels] = keychar;
					(sels, sele) = (sels+1, sels+1);
				}
		}
		c.sel = (sels, sele);
		if(osels != sels || osele != sele) {
			entryscroll(c);
			c.draw(1);
		}
		if (c.s != olds)
			c.onchange = 1;
	}
	return ans;
}
autoevent : ref E->Event;
autorepeat : chan of int;

initrepeater()
{
	autorepeat = chan of int;
	spawn autorepeater(autorepeat);
}

timeout(c : chan of int, ms : int)
{
	sys->sleep(ms);
	c <- = 1;
}

autorepeater(ctl : chan of int)
{
	on := 0;
	active := 0;
	timer := chan of int;
	evchan := chan of ref Event;
	dummychan := evchan;
	ev : ref Event;

	for (;;) alt {
	evchan <- = ev =>
		evchan = dummychan;
		if (on)
			spawn timeout(timer, 200);
		else
			active = 0;

	<- timer =>
		if (on) {
			ev = autoevent;
			evchan = E->evchan;
		} else
			active = 0;

	n := <- ctl =>
		if (n && !on && !active) {
			active = 1;
			spawn timeout(timer, 200);
		}
		on = n;
	}
}

Control.domouse(ctl: self ref Control, p: Point, mtype: int, oldgrab : ref Control) : (int, ref Control)
{
	autorepeat <- = 0;
	if(!(ctl.flags&CFenabled))
		return (CAnone, nil);
	ans := CAnone;
	changed := 0;
	newgrab : ref Control;
	grabbed := oldgrab != nil;
	pick c := ctl {
	Cbutton =>
		if(mtype == E->Mlbuttondown) {
			c.flags |= CFactive;
			newgrab = c;
			changed = 1;
		}
		else if(mtype == E->Mmove && c.ff == nil) {
			ans = CAflyover;
		}
		else if (mtype == E->Mldrag && grabbed) {
			newgrab = c;
			active := 0;
			if (p.in(c.r))
				active = CFactive;
			if ((c.flags & CFactive) != active)
				changed = 1;
			c.flags = (c.flags & ~CFactive) | active;
		}
		else if(mtype == E->Mlbuttonup || mtype == E->Mldrop) {
			if (c.flags & CFactive)
				ans = CAbuttonpush;
			c.flags &= ~CFactive;
			changed = 1;
		}
	Centry =>
		(sels, sele) := c.sel;
		if(mtype == E->Mlbuttonup && grabbed) {
			if (sels != sele)
				ans = CAselected;
		}
		if(mtype == E->Mlbuttondown || (mtype == E->Mldrag && grabbed)) {
			newgrab = c;
			x := c.r.min.x+ENTHMARGIN;
			fnt := charon_gui->fonts[charon_gui->CtlFnt].f;
			s := c.s;
			if(c.flags&CFsecure) {
				for(i := 0; i < len s; i++)
					s[i] = '*';
			}
			(osels, osele) := c.sel;
			s1 := " ";
			i := 0;
			iend := len s - 1;
			if(c.linewrap) {
				(lines, linestarts, topline, cursline) := entrywrapcalc(c);
				if(len lines > 1) {
					lineno := topline + (p.y - (c.r.min.y+ENTVMARGIN)) / ctllinespace;
					if(lineno >= len lines)
						lineno = len lines - 1;
					i = linestarts[lineno];
					iend = linestarts[lineno+1];
				}
			} else
				x -= fnt.width(s[:c.left]);
			for(; i <= iend; i++) {
				s1[0] = s[i];
				cx := fnt.width(s1);
				if(p.x < x + cx)
					break;
				x += cx;
			}
			sele = i;

			if (mtype == E->Mlbuttondown)
				sels = sele;
			c.sel = (sels, sele);

			if (sels != osels || sele != osele) {
				changed = 1;
				entryscroll(c);
				if (p.x < c.r.min.x + ENTHMARGIN || p.x > c.r.max.x - ENTHMARGIN) {
					autoevent = ref (Event.Emouse)(p, mtype);
					autorepeat <- = 1;
				}
			}

			if(!(c.flags&CFhasfocus))
				ans = CAkeyfocus;
		}
	Ccheckbox=>
		if(mtype == E->Mlbuttonup || mtype == E->Mldrop) {
			if(c.isradio) {
				if(!(c.flags&CFactive)) {
					c.flags |= CFactive;
					changed = 1;
					# turn off other radio button
					frm := c.ff.form;
					for(lf := frm.fields; lf != nil; lf = tl lf) {
						ff := hd lf;
						if(ff == c.ff)
							continue;
						if(ff.ftype == Fradio && ff.name==c.ff.name && ff.ctlid >= 0) {
							d := c.f.controls[ff.ctlid];
							if(d.flags&CFactive) {
								d.flags &= ~CFactive;
								d.draw(0);
								break;		# at most one other should be on
							}
						}
					}
				}
			}
			else {
				c.flags ^= CFactive;
				changed = 1;
			}
		}
	Cselect =>
		if(c.scr != nil && (grabbed || p.x >= c.r.max.x-SCRFBREADTH)) {
			pick scr := c.scr {
			Cscrollbar =>
				(a, grab) := scr.domouse(p, mtype, oldgrab);
				if (grab != nil)
					grab = c;
				return (a, grab);
			}
		}
		else if(mtype == E->Mlbuttonup || mtype == E->Mldrop) {
			n := (p.y - (c.r.min.y+SELMARGIN))/ctllinespace + c.first;
			if(n >= 0 && n < len c.options) {
				c.options[n].selected ^= 1;
				if(c.ff != nil && (c.ff.flags&B->FFmultiple) == byte 0 && c.options[n].selected) {
					# turn off other selections
					for(i := 0; i < len c.options; i++) {
						if(i != n)
							c.options[i].selected = 0;
					}
				}
				changed = 1;
				ans = CAchanged;
#				c.onchange = 1;
			}
		}
	Clistbox =>
		if(c.vscr != nil && (c.grab == c.vscr || (!grabbed && p.x >= c.r.max.x-SCRFBREADTH))) {
			c.grab = nil;
			pick vscr := c.vscr {
			Cscrollbar =>
				(a, grab) := vscr.domouse(p, mtype, oldgrab);
				if (grab != nil) {
					c.grab = c.vscr;
					grab = c;
				}
				return (a, grab);
			}
		}
		else if(c.hscr != nil && (c.grab == c.hscr || (!grabbed && p.y >= c.r.max.y-SCRFBREADTH))) {
			c.grab = nil;
			pick hscr := c.hscr {
			Cscrollbar =>
				(a, grab) := hscr.domouse(p, mtype, oldgrab);
				if (grab != nil) {
					c.grab = c.hscr;
					grab = c;
				}
				return (a, grab);
			}
		}
		else if(mtype == E->Mlbuttonup || mtype == E->Mldrop) {
			fnt := charon_gui->fonts[DefFnt].f;
			n := (p.y - (c.r.min.y+SELMARGIN))/fnt.height + c.first;
			if(n >= 0 && n < len c.options) {
				c.options[n].selected ^= 1;
				# turn off other selections
				for(i := 0; i < len c.options; i++) {
					if(i != n)
						c.options[i].selected = 0;
				}
				ans = CAchanged;
				changed = 1;
			}
		}
	Cscrollbar =>
		val := 0;
		v, vmin, vmax, b: int;
		if(c.flags&CFscrvert) {
			v = p.y;
			vmin = c.r.min.y;
			vmax = c.r.max.y;
			b = c.r.dx();
		}
		else {
			v = p.x;
			vmin = c.r.min.x;
			vmax = c.r.max.x;
			b = c.r.dy();
		}
		vsltop := vmin+b+c.top;
		vslbot := vmax-b-c.bot;
		actflags := 0;
		oldactflags := c.flags&CFscrallact;
		down := (mtype == E->Mlbuttondown);
		if((v >= vsltop && v < vslbot) || (c.flags&CFactive)) {
			if(mtype == E->Mldrag && (c.flags&CFactive)) {
				actflags = CFactive;
				val = v - c.dragv;
				if(abs(val) > c.mindelta) {
					ans = CAscrolldelta;
					c.dragv = v;
				}
				newgrab = c;
			}
			else if(down) {
				actflags = CFactive; 
				c.dragv = v;
				newgrab = c;
			}
		}
		else if(down || mtype == E->Mlbuttonup || mtype == E->Mldrop) {
			if(v < vmin+b) {
				if(down)
					actflags = CFscracta1;
				else {
					ans = CAscrollline;
					val = -1;
				}
			}
			else if(v < vsltop) {
				if(down)
					actflags = CFscracttr1;
				else {
					ans = CAscrollpage;
					val = -1;
				}
			}
			else if(v >= vmax-b) {
				if(down)
					actflags = CFscracta2;
				else {
					ans = CAscrollline;
					val = 1;
				}
			}
			else if(v >= vslbot) {
				if(down)
					actflags = CFscracttr2;
				else {
					ans = CAscrollpage;
					val = 1;
				}
			}
		}
		c.flags = (c.flags & ~CFscrallact) | actflags;
		if(ans != CAnone) {
			ftoscroll := c.f;
			if(c.ctl != nil) {
				pick cff := c.ctl {
				Cselect =>
					newfirst := cff.first;
					case ans {
					CAscrollpage =>
						newfirst += val*cff.nvis;
					CAscrollline =>
						newfirst += val;
					CAscrolldelta =>
						if(val > 0)
							newfirst++;
						else
							newfirst--;
					}
					newfirst = max(0, min(newfirst, len cff.options - cff.nvis));
					cff.first = newfirst;
					c.scrollset(newfirst, newfirst+cff.nvis, len cff.options, 1);
					cff.draw(1);
					return (ans, newgrab);
				Clistbox =>
					if(c.flags&CFscrvert) {
						newfirst := cff.first;
						case ans {
						CAscrollpage =>
							newfirst += val*cff.nvis;
						CAscrollline =>
							newfirst += val;
						CAscrolldelta =>
							if(val > 0)
								newfirst++;
							else
								newfirst--;
						}
						newfirst = max(0, min(newfirst, len cff.options - cff.nvis));
						cff.first = newfirst;
						c.scrollset(newfirst, newfirst+cff.nvis, len cff.options, 1);
						# TODO: need redraw only vscr and content
					}
					else {
						hw := cff.maxcol;
						w := (c.r.max.x - c.r.min.x - SCRFBREADTH)/charspace;
						newstart := cff.start;
						case ans {
						CAscrollpage =>
								newstart += val*hw;
						CAscrollline =>
								newstart += val;
						CAscrolldelta =>
							if(val > 0)
								newstart++;
							else
								newstart--;
						}
						if(hw < w)
							newstart = 0;
						else
							newstart = max(0, min(newstart, hw - w));
						cff.start = newstart;
						c.scrollset(newstart, w+newstart, hw, 1);
						# TODO: need redraw only hscr and content
					}
					cff.draw(1);
					return (ans, newgrab);
				}
			}
			else {
				if(c.flags&CFscrvert)
					c.f.yscroll(ans, val);
				else
					c.f.xscroll(ans, val);
			}
			changed = 1;
		}
		else if(actflags != oldactflags) {
			changed = 1;
		}
	Cprogbox =>
		if(mtype == E->Mlbuttondown)
			ans = CAbuttonpush;
	}
	if(changed)
		ctl.draw(1);
	return (ans, newgrab);
}

Control.reset(ctl: self ref Control)
{
	pick c := ctl {
	Cbutton =>
		c.flags &= ~CFactive;
	Centry =>
		c.s = "";
		c.sel = (0, 0);
		c.left = 0;
		if(c.ff != nil && c.ff.value != "")
			c.s = c.ff.value;
	Ccheckbox=>
		c.flags &= ~CFactive;
		if(c.ff != nil && (c.ff.flags&B->FFchecked) != byte 0)
			c.flags |= CFactive;
	Cselect =>
		if(c.ff != nil) {
			l := c.ff.options;
			for(i := 0; i < len c.options; i++) {
				o := hd l;
				c.options[i].selected = o.selected;
				l = tl l;
			}
		}
		c.first = 0;
		if(c.scr != nil) {
			c.scr.scrollset(0, c.nvis, len c.options,1);
		}
	Clistbox =>
		c.first = 0;
		if(c.vscr != nil) {
			c.vscr.scrollset(0, c.nvis, len c.options, 1);
		}
		hw := 0;
		for(i := 0; i < len c.options; i++)
			hw = max(hw, charon_gui->fonts[DefFnt].f.width(c.options[i].display)); 
		if(c.hscr != nil) {
			c.hscr.scrollset(0, c.r.max.x, hw, 1); 
		}
	Canimimage =>
		c.cur = 0;
	}
	ctl.draw(0);
}

Control.draw(ctl: self ref Control, flush: int)
{
	win := ctl.f.cim;
	oclipr := win.clipr;
	(clipr, any) := ctl.r.clip(oclipr);
	if(!any && ctl != ctl.f.vscr && ctl != ctl.f.hscr)
		return;
	win.clipr = clipr;
	pick c := ctl {
	Cbutton =>
		if(c.ff != nil && c.ff.image != nil && c.pic == nil) {
			# check to see if image arrived
			# (dimensions will have been set by checkffsize, if needed;
			# this code is only for when the HTML specified the dimensions)
			pick imi := c.ff.image {
			Iimage =>
				if(imi.ci.mims != nil) {
					c.pic = imi.ci.mims[0].im;
					c.picmask = imi.ci.mims[0].mask;
				}
			}
		}
		if(c.dorelief || c.pic == nil)
			win.draw(c.r, colorimage(Grey), nil, zp);
		if(c.pic != nil) {
			p, m: ref Image;
			if(c.flags & CFenabled) {
				p = c.pic;
				m = c.picmask;
			}
			else {
				p = c.dpic;
				m = c.dpicmask;
			}
			w := p.r.dx();
			h := p.r.dy();
			x := c.r.min.x + (c.r.dx() - w) / 2;
			y := c.r.min.y + (c.r.dy() - h) / 2;
			if((c.flags & CFactive) && c.dorelief) {
				x++;
				y++;
			}
			win.draw(Rect((x,y),(x+w,y+h)), p, m, zp);
		}
		else if(c.label != "") {
			p := c.r.min.add(Point(BUTMARGIN, BUTMARGIN));
			if(c.flags & CFactive)
				p = p.add(Point(1,1));
			win.text(p, colorimage(Black), zp, charon_gui->fonts[charon_gui->CtlFnt].f, c.label);
                #charonCode->drawtext(charon_gui->fonts[DefFnt].f,win,p, colorimage(Black), zp, charon_gui->fonts[CtlFnt].f,c.label);
		}
		if(c.dorelief) {
			relief := ReliefRaised;
			if(c.flags & CFactive)
				relief = ReliefSunk;
			drawrelief(win, c.r.inset(2), relief);
		}
	Centry =>
		win.draw(c.r, colorimage(White), nil, zp);
		drawrelief(win, c.r.inset(2), ReliefSunk);
		p := c.r.min.add(Point(ENTHMARGIN,ENTVMARGIN));
		s := c.s;
		fnt := charon_gui->fonts[charon_gui->CtlFnt].f;
		if(c.left > 0)
			s = s[c.left:];
		if(c.flags&CFsecure) {
			for(i := 0; i < len s; i++)
				s[i] = '*';
		}

		(sels, sele) := normalsel(c.sel);
		(sels, sele) = (sels-c.left, sele-c.left);

		lines : array of string;
		linestarts : array of int;
		if (c.linewrap)
			(lines, linestarts) = wrapstring(fnt, s, c.r.dx()-2*ENTHMARGIN);
		else
			(lines, linestarts) = (array [] of {s}, array [] of {0});

		# TODO: make some indication as to whether we have focus (c.flags&CFhasfocus)

		q := p;
		black := colorimage(Black);
		white := colorimage(White);
		navy := colorimage(Navy);
		for (n := 0; n < len lines; n++) {
			segs : list of (int, int, int);
			# only show cursor or selection if we have focus
			if (c.flags & CFhasfocus)
				segs = selsegs(len lines[n], sels-linestarts[n], sele-linestarts[n]);
			else
				segs = (0, len lines[n], 0) :: nil;
			for (; segs != nil; segs = tl segs) {
				(ss, se, sel) := hd segs;
				txt := lines[n][ss:se];
				w := fnt.width(txt);
				txtcol : ref Image;
				if (!sel)
					txtcol = black;
				else {
					txtcol = white;
					bgcol := navy;
					selr := Rect((q.x, q.y-1), (q.x+w, q.y+ctllinespace+1));
					if (selr.dx() == 0) {
						# empty selection - assume cursor
						bgcol = black;
						selr.max.x = selr.min.x + 2;
					}
					win.draw(selr, bgcol, nil, zp);
				}
				if (se > ss)
					win.text(q, txtcol, zp, fnt, txt);
				q.x += w;
			}
			q = (p.x, q.y + ctllinespace);
		}
	Ccheckbox=>
		win.draw(c.r, colorimage(White), nil, zp);
		if(c.isradio) {
			a := CBOXHT/2;
			a1 := a-1;
			cen := Point(c.r.min.x+a,c.r.min.y+a);
			win.ellipse(cen, a1, a1, 1, colorimage(DarkGrey), zp);
			win.arc(cen, a, a, 0, colorimage(Black), zp, 45, 180);
			win.arc(cen, a, a, 0, colorimage(Grey), zp, 225, 180);
			if(c.flags&CFactive)
				win.fillellipse(cen, 2, 2, colorimage(Black), zp);
		}
		else {
			ir := c.r.inset(2);
			ir.min.x += CBOXWID-CBOXHT;
			ir.max.x -= CBOXWID-CBOXHT;
			drawrelief(win, ir, ReliefSunk);
			if(c.flags&CFactive) {
				p1 := Point(ir.min.x, ir.min.y);
				p2 := Point(ir.max.x, ir.max.y);
				p3 := Point(ir.max.x, ir.min.y);
				p4 := Point(ir.min.x, ir.max.y);
				win.line(p1, p2, D->Endsquare, D->Endsquare, 0, colorimage(Black), zp);
				win.line(p3, p4, D->Endsquare, D->Endsquare, 0, colorimage(Black), zp);
			}
		}
	Cselect =>
		black := colorimage(Black);
		white := colorimage(White);
		navy := colorimage(Navy);
		win.draw(c.r, white, nil, zp);
		drawrelief(win, c.r.inset(2), ReliefSunk);
		ir := c.r.inset(SELMARGIN);
		p := ir.min;
		fnt := charon_gui->fonts[charon_gui->CtlFnt].f;
		for(i := c.first; i < len c.options && i < c.first+c.nvis; i++) {
			if(c.options[i].selected) {
				r := Rect((p.x-SELMARGIN,p.y),(c.r.max.x-SCRFBREADTH,p.y+ctllinespace));
				win.draw(r, navy, nil, zp);
                #charonCode->drawtext(charon_gui->fonts[DefFnt].f,win,p, white, zp, fnt,c.options[i].display);
				win.text(p, white, zp, fnt, c.options[i].display);
			}
			else {
                #charonCode->drawtext(charon_gui->fonts[DefFnt].f,win,p, black, zp, fnt,c.options[i].display);
				win.text(p, black, zp, fnt, c.options[i].display);
			}
			p.y += ctllinespace;
		}
		if(c.scr != nil) {
			c.scr.r = c.scr.r.subpt(c.scr.r.min);
			c.scr.r = c.scr.r.addpt(Point(c.r.max.x-SCRFBREADTH,c.r.min.y));
			c.scr.draw(0);
		}
	Clistbox =>
		black := colorimage(Black);
		white := colorimage(White);
		navy := colorimage(Navy);
		win.draw(c.r, white, nil, zp);
		#drawrelief(win, c.r.inset(2), ReliefSunk);
		ir := c.r.inset(SELMARGIN);
		p := ir.min;
		fnt := charon_gui->fonts[charon_gui->CtlFnt].f;
		for(i := c.first; i < len c.options && i < c.first+c.nvis; i++) {
			if(c.options[i].selected) {
				r := Rect((p.x-SELMARGIN,p.y),(c.r.max.x-SCRFBREADTH,p.y+fnt.height));
				win.draw(r, navy, nil, zp);
                #charonCode->drawtext(charon_gui->fonts[DefFnt].f,win,p, white, zp, fnt,c.options[i].display[c.start:]);
				win.text(p, white, zp, fnt, c.options[i].display[c.start:]);
			}
			else {
                #charonCode->drawtext(charon_gui->fonts[DefFnt].f,win,p, black, zp, fnt,c.options[i].display[c.start:]);
				win.text(p, black, zp, fnt, c.options[i].display[c.start:]);
			}
			p.y +=fnt.height;
		}
		if(c.vscr != nil) {
			c.vscr.r = c.vscr.r.subpt(c.vscr.r.min);
			c.vscr.r = c.vscr.r.addpt(Point(c.r.max.x-SCRFBREADTH-2, c.r.min.y)); 	 
c.vscr.draw(0); 		}	 		if(c.hscr != nil) {
			c.hscr.r = c.hscr.r.subpt(c.hscr.r.min);
			c.hscr.r = c.hscr.r.addpt(Point(c.r.min.x, c.r.max.y-SCRFBREADTH-2)); 			c.hscr.draw(0);
		}

		drawrelief(win, c.r.inset(2), ReliefSunk);

	Cscrollbar =>
		# Scrollbar components: arrow 1 (a1), trough 1 (t1), slider (s), trough 2 (t2), arrow 2 (a2)
		x := c.r.min.x;
		y := c.r.min.y;
		ra1, rt1, rs, rt2, ra2: Rect;
		b, l, a1kind, a2kind: int;
		if(c.flags&CFscrvert) {
			l = c.r.max.y - c.r.min.y;
			b = c.r.max.x - c.r.min.x;
			xr := x+b;
			yt1 := y+b;
			ys := yt1+c.top;
			yb := y+l;
			ya2 := yb-b;
			yt2 := ya2-c.bot;
			ra1 = Rect(Point(x,y),Point(xr,yt1));
			rt1 = Rect(Point(x,yt1),Point(xr,ys));
			rs = Rect(Point(x,ys),Point(xr,yt2));
			rt2 = Rect(Point(x,yt2),Point(xr,ya2));
			ra2 = Rect(Point(x,ya2),Point(xr,yb));
			a1kind = TRIup;
			a2kind = TRIdown;
		}
		else {
			l = c.r.max.x - c.r.min.x;
			b = c.r.max.y - c.r.min.y;
			yb := y+b;
			xt1 := x+b;
			xs := xt1+c.top;
			xr := x+l;
			xa2 := xr-b;
			xt2 := xa2-c.bot;
			ra1 = Rect(Point(x,y),Point(xt1,yb));
			rt1 = Rect(Point(xt1,y),Point(xs,yb));
			rs = Rect(Point(xs,y),Point(xt2,yb));
			rt2 = Rect(Point(xt2,y),Point(xa2,yb));
			ra2 = Rect(Point(xa2,y),Point(xr,yb));
			a1kind = TRIleft;
			a2kind = TRIright;
		}
		a1relief := ReliefRaised;
		if(c.flags&CFscracta1)
			a1relief = ReliefSunk;
		a2relief := ReliefRaised;
		if(c.flags&CFscracta2)
			a2relief = ReliefSunk;
		drawtriangle(win, ra1, a1kind, a1relief);
		drawtriangle(win, ra2, a2kind, a2relief);
		drawfill(win, rt1, Grey);
		rs = rs.inset(2);
		drawfill(win, rs, Grey);
		rsrelief := ReliefRaised;
		if(c.flags&CFactive)
			rsrelief = ReliefSunk;
		drawrelief(win, rs, rsrelief);
		drawfill(win, rt2, Grey);
	Canimimage =>
		i := c.cur;
		if(c.redraw)
			i = 0;
		else if(i > 0) {
			iprev := i-1;
			if(c.cim.mims[iprev].bgcolor != -1) {
				i = iprev;
				# get i back to before all "reset to previous"
				# images (which will be skipped in following
				# image drawing loop)
				while(i > 0 && c.cim.mims[i].bgcolor == -2)
					i--;
			}
		}
		bgi := colorimage(c.bg.color);
		if(c.bg.image != nil && len c.bg.image.mims > 0)
			bgi = c.bg.image.mims[0].im;
		for( ; i <= c.cur; i++) {
			mim := c.cim.mims[i];
			if(i > 0 && i < c.cur && mim.bgcolor == -2)
				continue;
			p := c.r.min.add(mim.origin);
			r := mim.im.r;
			r = Rect(p, p.add(Point(r.dx(), r.dy())));

			# IE takes "clear-to-background" disposal method to mean
			# clear to background of HTML page, ignoring any background
			# color specified in the GIF.
			# IE clears to background before frame 0
			if(i == 0)
				win.draw(c.r, bgi, nil, zp);

			if(i != c.cur && mim.bgcolor >= 0)
				win.draw(r, bgi, nil, zp);
			else
				win.draw(r, mim.im, mim.mask, zp);
		}
	Cprogbox =>
		if(c.state != CU->Punused) {
			bcol := Black;
			rcol := Grey;
			xw := (c.pcnt * PBOXWID + 50) / 100;
			if(xw > PBOXWID)
				xw = PBOXWID;
			case c.state {
			CU->Pstart =>
				drawfill(win, c.r, Grey);	# reset interior, if wrapped around
				bcol = White;
			CU->Pconnected =>
				bcol = Black;
			CU->Phavehdr =>
				bcol = DarkGrey;
			CU->Phavedata =>
				bcol = rcol = DarkGrey;
			CU->Pdone =>
				xw = PBOXWID;
				bcol = rcol = 16r666666;	# darker than DarkGrey
			CU->Perr =>
				bcol = rcol = Red;
				xw = PBOXWID;
			CU->Paborted =>
				bcol = rcol = Navy;
				xw = PBOXWID;
			}
			drawborder(win, c.r.inset(3), 3, bcol);
			if(rcol != Grey) {
				r := c.r;
				r.max.x -= (PBOXWID - xw);
				drawfill(win, r, rcol);
			}
		}
	Clabel =>
		p := c.r.min.add(Point(0,ENTVMARGIN));
                #charonCode->drawtext(charon_gui->fonts[DefFnt].f,win,p, colorimage(Black), zp, charon_gui->fonts[DefFnt].f,c.s);
		win.text(p, colorimage(Black), zp, charon_gui->fonts[DefFnt].f, c.s);
	}
	if(flush)
		win.flush(D->Flushnow);
	win.clipr = oclipr;
}

# Break s up into substrings that fit in width availw
# when printing with font fnt.
# The second returned array contains the indexes into the original
# string where the corresponding line starts (which might not be simply
# the sum of the preceding lines because of cr/lf's in the original string
# which are omitted from the lines array.
# Empty lines (ending in cr) get put into the array as empty strings.
# The start indices array has an entry for the phantom next line, to avoid
# the need for special cases in the rest of the code.
wrapstring(fnt: ref Font, s: string, availw: int) : (array of string, array of int)
{
	sl : list of (string, int) = nil;
	sw := fnt.width(s);
	n := 0;
	k := 0;	# index into original s where current s starts
	origlen := len s;
	done := 0;
	while(!done) {
		kincr := 0;
		s1, s2: string;
		if(s == "") {
			s1 = s;
			done = 1;
		}
		else {
			# if any newlines in s1, it's a forced break
			# (and newlines aren't to appear in result)
			(s1, s2) = S->splitl(s, "\n");
			if(s2 != nil && fnt.width(s1) <= availw) {
				s = s2[1:];
				sw = fnt.width(s);
				kincr = (len s1) + 1;
			}
			else if(sw <= availw) {
				s1 = s;
				done = 1;
			}
			else {
				(s1, nil, s, sw) = breakstring(s, sw, fnt, availw, 0);
				kincr = len s1;
				if(s == "")
					done = 1;
			}
		}
		sl = (s1, k) :: sl;
		k += kincr;
		n++;
	}
	# reverse sl back to original order
	lines := array[n] of string;
	linestarts := array[n+1] of int;
	linestarts[n] = origlen;
	while(sl != nil) {
		(ss, nn) := hd sl;
		lines[--n] = ss;
		linestarts[n] = nn;
		sl = tl sl;
	}
	return (lines, linestarts);
}

normalsel(sel : (int, int)) : (int, int)
{
	(s, e) := sel;
	if (s > e)
		(e, s) = sel;
	return (s, e);
}

selsegs(n, s, e : int) : list of (int, int, int)
{
	if (e < 0 || s > n)
		# selection is not in 0..n
		return (0, n, 0) :: nil;

	if (e > n) {
		# second half of string is selected
		if (s <= 0)
			return (0, n, 1) :: nil;
		return (0, s, 0) :: (s, n, 1) :: nil;
	}

	if (s < 0) {
		# first half of string is selected
		if (e >= n)
			return (0, n, 1) :: nil;
		return (0, e, 1) :: (e, n, 0) :: nil;
	}
	# middle section of string is selected
	return (0, s, 0) :: (s, e, 1) :: (e, n, 0) :: nil;
}

# Figure out in which area of scrollbar, if any, p lies.
# Then use p and mtype from mouse event to return desired action.
Control.entryset(c: self ref Control, s: string)
{
	pick e := c {
	Centry =>
		e.s = s;
		e.sel = (0, 0);
		e.left = 0;
		e.draw(1);
	}
}

# delete given range of characters, and redraw
entrydelrange(e: ref Control.Centry, istart, iend: int)
{
	n := iend - istart;
	(sels, sele) := normalsel(e.sel);
	if(n > 0) {
		e.s = e.s[0:istart] + e.s[iend:];

		if(sels > istart) {
			if(sels < iend)
				sels = istart;
			else
				sels -= n;
		}
		if (sele > istart) {
			if (sele < iend)
				sele = istart;
			else
				sele -= n;
		}

		if(e.left > istart)
			e.left = max(istart-1, 0);
		e.sel = (sels, sele);
		entryscroll(e);
	}
}

# hack to set entry from outside-of-charon snarf buffer.
# Use /services/webget/ossnarf, and hope that someone bound
# the external /dev/snarf there before starting emu.
# (This is only useful when Charon runs under Brazil)
entrysetfromsnarf(e: ref Control.Centry)
{
	f := sys->open("/services/webget/ossnarf", sys->OREAD);
	if(f != nil) {
		buf := array[sys->ATOMICIO] of byte;
		n := sys->read(f, buf, len buf);
		if(n > 0) {
			# trim a trailing newline, as a service...
			if(buf[n-1] == byte '\n')
				n--;
			s := string buf[0:n];
			e.entryset(s);
		}
	}
}

# make sure can see cursor and following char or two
entryscroll(e: ref Control.Centry)
{
	s := e.s;
	slen := len s;
	if(e.flags&CFsecure) {
		for(i := 0; i < slen; i++)
			s[i] = '*';
	}
	if(e.linewrap) {
		# For multiple line entries, c.left is the char
		# at the beginning of the topmost visible line,
		# and we just want to scroll to make sure that
		# the line with the cursor is visible
		(lines, linestarts, topline, cursline) := entrywrapcalc(e);
		if(cursline < topline)
			topline = cursline;
		else {
			vislines := (e.r.dy()-2*ENTVMARGIN) / ctllinespace;
			if(cursline >= topline+vislines)
				topline = cursline-vislines+1;
		}
		e.left = linestarts[topline];
	}
	else {
		(nil, sele) := e.sel;
		# sele is always the drag point
		if(sele < e.left)
			e.left = sele;
		else if(sele > e.left) {
			fnt := charon_gui->fonts[charon_gui->CtlFnt].f;
			wantw := e.r.dx() -2*ENTHMARGIN; # - 2*ctlspspace;
			while(e.left < sele-1) {
				w := fnt.width(e.s[e.left:sele]);
				if(w < wantw)
					break;
				e.left++;
			}
		}
	}
}

# Given e, a Centry with line wrapping,
# return (wrapped lines, line start indices, line# of top displayed line, line# containing cursor).
entrywrapcalc(e: ref Control.Centry) : (array of string, array of int, int, int)
{
	s := e.s;
	if(e.flags&CFsecure) {
		for(i := 0; i < len s; i++)
			s[i] = '*';
	}
	(nil, sele) := e.sel;
	(lines, linestarts) := wrapstring(charon_gui->fonts[charon_gui->CtlFnt].f, s, e.r.dx()-2*ENTHMARGIN);
	topline := 0;
	cursline := 0;
	for(i := 0; i < len lines; i++) {
		s = lines[i];
		i1 := linestarts[i];
		i2 := linestarts[i+1];
		if(sele >= i1 && e.left < i2)
			topline = i;
		if(sele >= i1 && sele < i2)
			cursline = i;
	}
	if(sele == linestarts[len lines])
		cursline = len lines - 1;
	return (lines, linestarts, topline, cursline);
}

Lay.new(targwidth: int, just: byte, margin: int, bg: Background) : ref Lay
{
	ans := ref Lay(Line.new(), Line.new(),
			targwidth, 0, 0, margin, nil, bg, just, byte 0);
	ans.targetwidth -= 2*margin;
	if(ans.targetwidth < 0)
		ans.targetwidth = 0;
	ans.start.pos = Point(margin, margin);
	ans.start.next = ans.end;
	ans.end.prev = ans.start;
	# dummy item at end so ans.end will have correct y coord
	it := Item.newspacer(ISPnull);
	it.state = IFbrk|IFcleft|IFcright;
	ans.end.items = it;
	return ans;
}

Line.new() : ref Line
{
	return ref Line(
			nil, nil, nil,		# items, next, prev
			Point(0,0),		# pos
			0, 0, 0,		# width, height, ascent
			byte 0);		# flags
}

Loc.new() : ref Loc
{
	return ref Loc(array[10] of Locelem, 0, Point(0,0));	# le, n, pos
}

Loc.add(loc: self ref Loc, kind: int, pos: Point)
{
	if(loc.n == len loc.le) {
		newa := array[len loc.le + 10] of Locelem;
		newa[0:] = loc.le;
		loc.le = newa;
	}
	loc.le[loc.n].kind = kind;
	loc.le[loc.n].pos = pos;
	loc.n++;
}

# return last frame in loc's path
Loc.lastframe(loc: self ref Loc) : ref Frame
{
	for(i := loc.n-1; i >=0; i--)
		if(loc.le[i].kind == LEframe)
			return loc.le[i].frame;
	return nil;
}

Loc.print(loc: self ref Loc, msg: string)
{
	sys->print("%s: Loc with %d components, pos=(%d,%d)\n", msg, loc.n, loc.pos.x, loc.pos.y);
	for(i := 0; i < loc.n; i++) {
		case loc.le[i].kind {
		LEframe =>
			sys->print("frame %x\n",  loc.le[i].frame);
		LEline =>
			sys->print("line %x\n", loc.le[i].line);
		LEitem =>
			sys->print("item: %x", loc.le[i].item);
			loc.le[i].item.print();
		LEtablecell =>
			sys->print("tablecell: %x, cellid=%d\n", loc.le[i].tcell, loc.le[i].tcell.cellid);
		LEcontrol =>
			sys->print("control %x\n", loc.le[i].control);
		}
	}
}

Sources.add(srcs: self ref Sources, s: ref Source)
{
	srcs.srcs = s :: srcs.srcs;
	srcs.tot++;
}

Sources.findbs(srcs: self ref Sources, bs: ref ByteSource) : ref Source
{
	for(sl := srcs.srcs; sl != nil; sl = tl sl) {
		s := hd sl;
		if(s.bs == bs)
			return s;
	}
	return nil;
}

# spawned to animate images in frame f
animproc(f: ref Frame)
{
	f.animpid = sys->pctl(0, nil);
	aits : list of ref Item = nil;
	# let del be millisecs to sleep before next frame change
	del := 10000000;
	d : int;
	for(il := f.doc.images; il != nil; il = tl il) {
		it := hd il;
		pick i := it {
		Iimage =>
			ms := i.ci.mims;
			if(len ms > 1) {
				loc := f.find(zp, it);
				if(loc == nil) {
					# could be background, I suppose -- don't animate it
					if(dbg)
						sys->print("couldn't find item for animated image\n");
					continue;
				}
				p := loc.le[loc.n-1].pos;
				p.x += int i.hspace + int i.border;
				# BUG: should get background from least enclosing layout
				ctl := Control.newanimimage(f, i.ci, f.layout.background);
				ctl.r = ctl.r.addpt(p);
				i.ctlid = f.addcontrol(ctl);
				d = ms[0].delay;
				if(dbg)
					sys->print("added anim ctl %d for image %s, initial delay %d\n",
						i.ctlid, i.ci.src.tostring(), d);
				aits = it :: aits;
				if(d < del)
					del = d;
			}
		}
	}
	if(aits == nil)
		return;
	tot := big 0;
	for(;;) {
		sys->sleep(del);
		tot = tot + big del;
		newdel := 10000000;
		for(al := aits; al != nil; al = tl al) {
			it := hd al;
			pick i := hd al {
			Iimage =>
				ms := i.ci.mims;
				pick c := f.controls[i.ctlid] {
				Canimimage =>
					m := ms[c.cur];
					d = m.delay;
					if(d > 0) {
						d = int (tot % (big d));
						if(d > 0)
							d = m.delay - d;
					}
					if(d == 0) {
						# advance to next frame and show it
						c.cur++;
						if(c.cur == len ms)
							c.cur = 0;
						d = ms[c.cur].delay;
						c.draw(1);
					}
					if(d < newdel)
						newdel = d;
				}
			}
		}
		del = newdel;
	}
}
