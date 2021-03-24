# Gui implementation for running under wm (tk window manager)
implement Gui;

include "common.m";
include "tk.m";
include "wmlib.m";
include "charon_gui.m";

sys: Sys;
D: Draw;
	Font,Point, Rect, Image, Context, Screen, Display: import D;
	charon_gui:Charon_gui;
CU: CharonUtils;
E: Events;
	Event: import E;
tk: Tk;
wmlib: Wmlib;

screen: ref Screen;
tktop : ref Tk->Toplevel;
mousegrabbed := 0;

cfg := array[] of {
	"label .Wm_t.title -anchor w -bg #aaaaaa -fg white -text Charon",
	"pack .Wm_t.title -side left",
	"pack .Wm_t -fill x",
	"bind . <ButtonPress-1> {send gctl b1p %X %Y}",
	"bind . <ButtonRelease-1> {send gctl b1r %X %Y}",
	"bind . <Motion-Button-1> {send gctl b1d %X %Y}",
	"bind . <ButtonPress-2> {send gctl b2p %X %Y}",
	"bind . <ButtonRelease-2> {send gctl b2r %X %Y}",
	"bind . <Motion-Button-2> {send gctl b2d %X %Y}",
	"bind . <ButtonPress-3> {send gctl b3p %X %Y}",
	"bind . <ButtonRelease-3> {send gctl b3r %X %Y}",
	"bind . <Motion-Button-3> {send gctl b3d %X %Y}",
	"bind . <Motion> {send gctl m %X %Y}",
	"bind .Wm_t <ButtonPress-1> {send gctl b1p %X %Y}",
	"bind .Wm_t <ButtonRelease-1> {}",
	"bind .Wm_t <Motion-Button-1> {send gctl b1d %X %Y}",
	"bind .Wm_t <Motion> {}",
	"bind .Wm_t.title <ButtonPress-1> {send gctl b1p %X %Y}",
	"bind .Wm_t.title <ButtonRelease-1> {}",
	"bind .Wm_t.title <Motion-Button-1> {send gctl b1d %X %Y}",
	"bind .Wm_t.title <Motion> {}",
	"bind . <FocusIn> {send gctl fi}",
	"bind . <FocusOut> {send gctl fo}",
	"bind . <Key> {send gctl k %s}"
};

gctl : chan of string;
puch : chan of int;		# popup synchronization reply chan

WMargin : con 0;	# want this much spare screen width
HMargin : con 70;	# want this much spare screen height (allow for titlebar, toolbar)

titler: Rect;		# titlebar screen coords (including border)
totalr: Rect;		# toplevel (".") screen coords (includes titlebar)
mainr: Rect;		# browser's main window coords
ctlr: Rect;			# browser's control window coords
progr: Rect;		# browser's progress window coords
popupr: Rect;		# popup window screen coords
offset: Point;		# ctlr.min-ctlwin.r.min (accounts for origin change, due to move)
ctlh: int;			# height that ctlwin is supposed to be
progh: int;		# height that progwin is supposed to be

allwins : array of ref Image;

tbinit(ctxt: ref Context, nil, nil: chan of string, rmain, rctl, rprog: Rect, cu: CharonUtils) : (Rect, Rect, Rect)
{
	return init(ctxt, rmain, rctl, rprog, cu);
}

init(ctxt: ref Context, rmain, rctl, rprog: Rect, cu: CharonUtils) : (Rect, Rect, Rect)
{
	sys = load Sys Sys->PATH;
	D = load Draw Draw->PATH;
	CU = cu;
	E = cu->E;
	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;
	if(wmlib == nil)
		CU->raise(sys->sprint("EXInternal: can't load module Wmlib: %r"));
	wmlib->init();
	charon_gui=load Charon_gui Charon_gui->PATH;
	if(charon_gui==nil)
		CU->raise(sys->sprint("EXinternal:couldn't load Charon_gui:%r"));
	charon_gui->init();

	mainw := rmain.dx();
	mainh := rmain.dy();
	ctlh = rctl.dy();
	progh = rprog.dy();
	if(mainw < 250 || mainh< 40)
		CU->raise(sys->sprint("EXInternal: window too small (%d x %d)",
			mainw, mainh));
	if(ctxt == nil)
		CU->raise("EXInternal: need context to run under wm");

	display = ctxt.display;
	screen = ctxt.screen;
	screenw := screen.image.r.dx();
	screenh := screen.image.r.dy();
	x := rctl.min.x;
	y := rctl.min.y;
	if(x + mainw > screenw-WMargin) {
		x = 0;
		if(mainw > screenw - WMargin)
			mainw = screenw - WMargin;
	}
	if(y + mainh + ctlh + progh > screenh-HMargin) {
		y = 0;
		if(mainh + ctlh + progh > screenh - HMargin)
			mainh = screenh - ctlh - progh - HMargin;
	}

	# stuff similar to Wmlib->titlebar
	gctl = chan of string;
	puch = chan of int;
	tktop = tk->toplevel(screen, "-borderwidth 2 -relief raised -x " + string x
		+ " -y " + string y + " -width " + string mainw);
	tk->namechan(tktop, gctl, "gctl");
	tk->cmd(tktop, "frame .Wm_t -bg #aaaaaa -borderwidth 2 -width " + string mainw);
	# customized buttons
	buts := (CU->config).buttons;
	if(buts != "") {
		packlist := "";
		(nil, butl) := sys->tokenize(buts, ", \t");
		for(; butl != nil; butl = tl butl) {
			cmd := "";
			packitem := "";
			case hd butl {
			"help" =>
				cmd = "button .Wm_t.h -bitmap help.bit -command {send gctl h}";
				packitem = " .Wm_t.h";
			"resize" =>
				cmd = "button .Wm_t.m -bitmap maxf.bit -command {send gctl s}";
				packitem = " .Wm_t.m";
			"hide" =>
				cmd = "button .Wm_t.t -bitmap task.bit -command {send gctl t}";
				packitem = " .Wm_t.t";
			"exit" =>
				cmd = "button .Wm_t.e -bitmap exit.bit -command {send gctl e}";
				packitem = " .Wm_t.e";
			* =>
				continue;
			}
			tk->cmd(tktop, cmd);
			packlist = packitem + packlist;
		}
		# pack buttons
		if(packlist != nil)
			tk->cmd(tktop, "pack" + packlist + " -side right");
	}
	for(i := 0; i < len cfg; i++)
		tk->cmd(tktop, cfg[i]);
	tbarh := actr(tktop, ".Wm_t").dy();
	totalr = Rect(Point(x,y),Point(x+mainw,y+tbarh+ctlh+mainh+progh));
	offset = Point(0,0);
	if(progh > 0)
		allwins = array[3] of ref Image;
	else
		allwins = array[2] of ref Image;
	makewins(tktop);

	spawn evhandle(tktop, gctl, E->evchan);
	return (mainr, ctlr, progr);
}

# act(x,y) gives top-left, outside the border
# act(width,height) give dimensions inside the border
actr(t: ref Tk->Toplevel, wname: string) : Rect
{
	x := int tk->cmd(t, wname + " cget -actx");
	y := int tk->cmd(t, wname + " cget -acty");
	w := int tk->cmd(t, wname + " cget -actwidth");
	h := int tk->cmd(t, wname + " cget -actheight");
	bd := int tk->cmd(t, wname + " cget -borderwidth");
	return Rect((x,y),(x+w+2*bd,y+h+2*bd));
}

evhandle(t: ref Tk->Toplevel, gctl: chan of string, evchan: chan of ref Event)
{
	wdrag := 0;		# dragging main window title bar
	for(;;) {
		s := <- gctl;
		(nil, l) := sys->tokenize(s, " ");
		case hd l {
		"b1p" or "b1r" or "b1d" or 
		"b2p" or "b2r" or "b2d" or 
		"b3p" or "b3r" or "b3d" or 
		"m" =>
			l = tl l;
			x := int hd l;
			y := int hd tl l;
			mtype : int;
			if(s[0] == 'm')
				mtype = E->Mmove;
			else {
				case s[1] {
				'1' =>
					case s[2] {
					'p' => mtype = E->Mlbuttondown;
					'r' => mtype = E->Mlbuttonup;
					'd' => mtype = E->Mldrag;
					}
				'2' =>
					case s[2] {
					'p' => mtype = E->Mmbuttondown;
					'r' => mtype = E->Mmbuttonup;
					'd' => mtype = E->Mmdrag;
					}
				'3' =>
					case s[2] {
					'p' => mtype = E->Mrbuttondown;
					'r' => mtype = E->Mrbuttonup;
					'd' => mtype = E->Mrdrag;
					}
				}
			}
			p := Point(x,y);
			if(mtype == E->Mlbuttondown) {
				wdrag = 0;
				tk->cmd(t, "focus .");
			}
			if(p.in(titler) && !mousegrabbed) {
				if(mtype == E->Mlbuttondown) {
					tk->cmd(t, "raise .");
					screen.top(allwins);
					if(popupwin != nil)
						screen.top(array [] of {popupwin});
					wdrag = 1;
				}
				else if(mtype == E->Mldrag && wdrag)
					moveresize(t, 'm');
			} else {
				evchan <-= ref Event.Emouse( p.sub(offset), mtype);
			}
		"fi" =>
			tk->cmd(t, ".Wm_t configure -bg blue");
			tk->cmd(t, ".Wm_t.title configure -bg blue");
			tk->cmd(t, "update");
		"fo" =>
			tk->cmd(t, ".Wm_t configure -bg #aaaaaa");
			tk->cmd(t, ".Wm_t.title configure -bg #aaaaaa");
			tk->cmd(t, "update");
		"k" =>
			k := int hd tl l;
			if(k != 0)
				evchan <-= ref Event.Ekey(k);
		"e" =>
			evchan <-= ref Event.Equit(0);
		"h" =>
			evchan <-= ref Event.Ehelp(1); 
		"s" =>
			# do not allow resize if popup active - only iconize, move and exit permitted
			if (popupwin == nil) {
				ev := moveresize(t, 's');
				if(ev != nil) {
					screen.top(allwins);
					evchan <-= ev;
				}
			}
		"t" =>
			# move the browser windows off the screen to hide them
			ctlwin.origin(ctlwin.r.min, (-3000, -3000));
			mainwin.origin(mainwin.r.min, (-3000, -3000));
			if (progwin != nil)
				progwin.origin(progwin.r.min, (-3000,-3000));
			if (popupwin != nil)
				popupwin.origin(popupwin.r.min, (-3000,-3000));

			# minimise the Tk Window
			fd := sys->open("/chan/wm", sys->ORDWR);
			if(fd == nil)
				return;
			tk->cmd(t, ". unmap; update");
			sys->fprint(fd, "t%s", tk->cmd(t, ".Wm_t.title cget -text"));

			# restore the Tk window
			tk->cmd(t, ". map; update");
	
			# restore position of the offscreen windows
			ctlwin.origin(ctlwin.r.min, ctlr.min);
			mainwin.origin(mainwin.r.min, mainr.min);
			if(progwin != nil)
				progwin.origin(progwin.r.min, progr.min);
			screen.top(allwins);
			if(popupwin != nil) {
				popupwin.origin(popupwin.r.min, popupr.min);
				screen.top(array [] of {popupwin});
			}
		"popup" =>
			puch <- = 1;
		}
	}
}

# BUG
# mouse events for the popup window are obtained
# through TK system for the main window.
# re-sizing the main window can result in popup controls lying outside
# of the main window rect, and so mouse events can never be recieved for
# those co-ords.
makepopup(width, height: int) : ref Draw->Image
{
	# synchronize with evhandler to eliminate position change race
	gctl <- = "popup";
	rmain := mainwin.r;
	x := rmain.min.x + (rmain.dx() - width) / 2;
	if(x < 0)
		x = 0;
	y := rmain.min.y + (rmain.dy() - height) / 2;
	if(y < 0)
		y = 0;
	r := Rect(Point(x,y),Point(x+width,y+height));

	popupwin = screen.newwindow(r, display.rgb2cmap(16rDD, 16rDD, 16rDD));
	# popupwin co-ords must match those of the main window as all mouse event
	# co-ords come from the main window.  mainwin.r.min != screen position if
	# the window has been moved (e.g. by dragging)
	#
	# calculate the correct screen position of the popup
	popupr = r.addpt(mainr.subpt(mainwin.r.min).min);
	popupwin.origin(r.min, popupr.min);
	popupwin.flush(D->Flushoff);
	<- puch;
	return popupwin;
}

geticon(icon: int) : (ref Draw->Image, ref Draw->Image)
{
	fname := "/icons/charon/";
	case icon {
        IClogo =>
                fname += charon_gui->iLogoBit;
        ICback =>
                fname += charon_gui->iBackBit;
        ICfwd =>
                fname += charon_gui->iFwdBit;
        ICreload =>
                fname += charon_gui->iReloadBit;
        ICstop =>
                fname += charon_gui->iStopBit;
        IChist =>
                fname += charon_gui->iHistoryBit;
        ICbmark =>
                fname += charon_gui->iBookmarkBit;
        ICedit =>
                fname += charon_gui->iEditBit;
        ICconf =>
                fname += charon_gui->iConfBit;
        IChelp =>
                fname += charon_gui->iHelpBit;
        IChome =>
                fname += charon_gui->iHomeBit;
        ICsslon =>
                fname += charon_gui->iSslonBit;
        ICssloff =>
                fname += charon_gui->iSsloffBit;
        ICup =>
                fname += charon_gui->iUpBit;
        ICdown =>
                fname += charon_gui->iDownBit;
        ICplus =>
                fname += charon_gui->iPlusBit;
        ICminus =>
                fname += charon_gui->iMinusBit;
        ICexit =>
                fname += charon_gui->iExitBit;
	ICcopy  =>
		fname += charon_gui->iCopyBit;
	ICkeybd =>
		fname += charon_gui->iKbdBit;
	* =>
		CU->raise(sys->sprint("EXInternal: unknown icon index %d", icon));
	}
	fd := sys->open(fname, Sys->OREAD);
	if(fd == nil)
		CU->raise(sys->sprint("EXInternal: can't open icon file %s: %r", fname));
	im := display.readimage(fd);
	if(im == nil)
		CU->raise(sys->sprint("EXInternal: can't convert icon %s to image: %r", fname));
	mask : ref Image = nil;
	if(icon != IClogo)
		mask = im;
	return (im, mask);
}

# Dialog with wm to rubberband the window and return a new position
# or size
moveresize(t: ref Tk->Toplevel, mode: int) : ref Event
{
	fd := sys->open("/chan/wm", sys->ORDWR);
	if(fd == nil)
		return nil;
	sys->fprint(fd, "%c%5d %5d %5d %5d", mode,
			totalr.min.x, totalr.min.y, totalr.max.x, totalr.max.y);

	reply := array[128] of byte;
	n := sys->read(fd, reply, len reply);
	if(n < 0)
		return nil;

	s := string reply[0:n];
	x := int s;
	y := int s[6:];
	if(mode == 'm') {
		if(totalr.min.x != x || totalr.min.y != y) {
			p := Point(x,y);
			tk->cmd(t, ". configure -x "+string x+" -y "+string y+"; update");
			diff := p.sub(totalr.min);
			newctlr := ctlr.addpt(diff);
			newmainr := mainr.addpt(diff);
			newprogr := progr.addpt(diff);
			ctlwin.origin(ctlwin.r.min, newctlr.min);
			mainwin.origin(mainwin.r.min, newmainr.min);
			if(progwin != nil)
				progwin.origin(progwin.r.min, newprogr.min);
			screen.top(allwins);
			if(popupwin != nil) {
				popupr = popupr.addpt(diff);
				popupwin.origin(popupwin.r.min, popupr.min);
				screen.top(array [] of {popupwin});
			}
			totalr = totalr.addpt(diff);
			titler = titler.addpt(diff);
			ctlr = newctlr;
			mainr = newmainr;
			progr = newprogr;
			offset = ctlr.min.sub(ctlwin.r.min);
		}
		return nil;
	}
	w := int s[12:] - x;
	h := int s[18:] - y;

	totalr = Rect((x,y),(x+w,y+h));
	makewins(t);
	return ref Event.Ereshape(totalr);
}

# Use tbarh, ctlh, totalr to calculate titler, mainr and ctlr,
# reconfigure "." to cover totalr,
# and make (or remake) mainwin and ctlwin.
makewins(t: ref Tk->Toplevel)
{
	w := totalr.dx() - 4;
	tk->cmd(t, "pack propagate . 1");
	newlabelw := w - (actr(t, ".Wm_t").dx() - actr(t, ".Wm_t.title").dx());
	tk->cmd(t, ".Wm_t.title configure -width " + string newlabelw);
	tk->cmd(t, "pack propagate . 0");

#	titler = actr(t, ".");		# Unreliable, can get original full window size
#	clientr := Rect(Point(totalr.min.x, titler.max.y), Point(titler.max.x,totalr.max.y));
	titler = actr(t, ".Wm_t");
	titler = Rect((0,0), (titler.dx(), titler.dy()+4));
	# titler relative to (0,0) - make relative to totalr
	titler = titler.addpt(totalr.min);
	clientr := Rect(Point(totalr.min.x, titler.max.y), Point(totalr.max.x,totalr.max.y));
	tk->cmd(t, ". configure -x " + string totalr.min.x +
			" -y " + string totalr.min.y +
			" -width " + string (totalr.dx()-4) +
			" -height " + string (totalr.dy()-4));
	tk->cmd(t, "update");

	offset = Point(0,0);
	ctlr = clientr;
	mainr = clientr;
	progr = clientr;
	mainr.min.y = ctlr.max.y = ctlr.min.y+ctlh;
	progr.min.y = mainr.max.y = mainr.max.y - progh;
	mainwin = screen.newwindow(mainr, D->White);
	ctlwin = screen.newwindow(ctlr, display.rgb2cmap(16rDD, 16rDD, 16rDD));
	if(progh != 0)
		progwin = screen.newwindow(progr, display.rgb2cmap(16rDD, 16rDD, 16rDD));
	if(mainwin == nil || ctlwin == nil || (progh != 0 && progwin == nil))
		CU->raise(sys->sprint("EXFatal: can't initialize windows: %r"));
	mainwin.flush(D->Flushoff);
	ctlwin.flush(D->Flushoff);
	if(progwin != nil)
		progwin.flush(D->Flushoff);

	allwins[0] = mainwin;
	allwins[1] = ctlwin;
	if(progwin != nil)
		allwins[2] = progwin;
	screen.top(allwins);
}

# for debugging
SR(r: Rect) : string
{
	return sys->sprint("(%d,%d)(%d,%d)", r.min.x, r.min.y, r.max.x, r.max.y);
}

snarfput(s: string)
{
	wmlib->snarfput(s);
}

skeyboard(nil: ref Draw->Context)  
{
}

grabmouse(grab : int)
{
	c : string;
	if (grab)
		c = "grab set .";
	else
		c = "grab release .";
	mousegrabbed = grab;
	tk->cmd(tktop, c);
}
