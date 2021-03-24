implement Gui;

include "common.m";
include "charon_gui.m";

sys: Sys;
D: Draw;
	Font,Point, Rect, Image, Context, Screen, Display: import D;
	charon_gui: Charon_gui;
CU: CharonUtils;
E: Events;
	Event: import E;

screen: ref Screen;

CTLHEIGHT : con 65;	# height of control (menu and toolbar) window
PROGHEIGHT : con 24;	# height of progress window


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
	charon_gui = load Charon_gui Charon_gui->PATH;
	if(charon_gui==nil)
		CU->raise(sys->sprint("EXinternal:couldn't load Charon_gui:%r"));
	charon_gui->init();

	if(rmain.dy() < 250 || rmain.dx() < 40)
		CU->raise(sys->sprint("EXInternal: window too small(%d x %d)",
				rmain.dy(), rmain.dx()));
	if(ctxt != nil)
		CU->raise("EXInternal: context not nil");

	display = Display.allocate(nil);
	if(display == nil)
		CU->raise(sys->sprint("EXFatal: can't initialize display: %r"));
	screen = Screen.allocate(display.image, display.rgb(16rA1, 16rC3, 16rD1), 1);
	if(screen == nil)
		CU->raise(sys->sprint("EXFatal: can't initialize screen: %r"));

	# resize the windows to fill display
	x := display.image.r.max.x;
	y := display.image.r.max.y;
	rctl = Rect((0,0), (x, CTLHEIGHT));
	rmain = Rect((0,CTLHEIGHT), (x, y));
	if(rprog.dy() != 0) {
		rprog = Rect((0,y-PROGHEIGHT),(x,y));
		rmain.max.y -= PROGHEIGHT;
	}

	mainwin = screen.newwindow(rmain, D->White);
	ctlwin = screen.newwindow(rctl, display.rgb2cmap(16rDD, 16rDD, 16rDD));
	if(rprog.dy() != 0)
		progwin = screen.newwindow(rprog, display.rgb2cmap(16rDD, 16rDD, 16rDD));
	if(mainwin == nil || ctlwin == nil || (rprog.dy() != 0 && progwin == nil))
		CU->raise(sys->sprint("EXFatal: can't initialize windows: %r"));

	mainwin.flush(D->Flushoff);
	ctlwin.flush(D->Flushoff);
	if(progwin != nil)
		progwin.flush(D->Flushoff);
	spawn rawmouse();
	spawn rawkeyboard();
	return (rmain, rctl, rprog);
}

rawkeyboard()
{
	ch := E->evchan;
	fd := sys->open("/dev/keyboard", sys->OREAD);
	if(fd == nil)
		CU->raise(sys->sprint("exFatal: can't open /dev/keyboard: %r"));

	b:= array[1] of byte;
	buf := array[10] of byte;
	i := 0;
	for(;;) {
		n := sys->read(fd, buf[i:], len buf - i);
		if(n < 1)
			break;
		i += n;
		while(i >0 && (nutf := sys->utfbytes(buf, i)) > 0) {
			s := string buf[0:nutf];
			k := int s[0];
			case k {
				# These are from Carrera/Lexmark; fix for others
				128 => k = E->Kdown;
				206 => k = E->Kup;
				205 => k = E->Khome;
			}
			ch <-= ref Event.Ekey(k);
			buf[0:] = buf[nutf:i];
			i -= nutf;
		}
	}
}

# Ensure following rules:
#   - button-down always followed by (button-drag* button-up)
#   - button-drag and button-up don't otherwise occur
#   - separate button-move or button-drag on every discernable move
rawmouse()
{
	ch := E->evchan;
	fd := sys->open("/dev/pointer", sys->OREAD);
	if(fd == nil)
		CU->raise(sys->sprint("exFatal: can't open /dev/pointer: %r"));

	n := 0;
	buf := array[100] of byte;
	xold := -1;
	yold := -1;
	bold := 0;
	for(;;) {
		n = sys->read(fd, buf, len buf);
		if(n <= 0)
			break;

		if(int buf[0] != 'm' || n != 37)
			continue;

		x := int(string buf[ 1:13]);
		y := int(string buf[12:25]);
		b := int(string buf[24:37]);
		domoved := (x != xold || y != yold);
		if(b == 0) {
			if(bold & 1)
				ch <-= ref Event.Emouse(Point(xold,yold), E->Mlbuttonup);
			if(bold & 2)
				ch <-= ref Event.Emouse(Point(xold,yold), E->Mmbuttonup);
			if(bold & 4) 
				ch <-= ref Event.Emouse(Point(xold,yold), E->Mrbuttonup);
		}
		else {
			if(b & 1) {
				if(bold & 1) {
					ch <-= ref Event.Emouse(Point(x,y), E->Mldrag);
					domoved = 0;
				}
				else {
					if(domoved) {
						ch <-= ref Event.Emouse(Point(x,y), E->Mmove);
						domoved = 0;
					}
					ch <-= ref Event.Emouse(Point(x,y), E->Mlbuttondown);
				}
			}
			if(b & 2) {
				if(bold & 2) {
					ch <-= ref Event.Emouse(Point(x,y), E->Mmdrag);
					domoved = 0;
				}
				else {
					if(domoved) {
						ch <-= ref Event.Emouse(Point(x,y), E->Mmove);
						domoved = 0;
					}
					ch <-= ref Event.Emouse(Point(x,y), E->Mmbuttondown);
				}
			}
			if(b & 4) {
				if(bold & 4) {
					ch <-= ref Event.Emouse(Point(x,y), E->Mrdrag);
					domoved = 0;
				}
				else {
					if(domoved) {
						ch <-= ref Event.Emouse(Point(x,y), E->Mmove);
						domoved = 0;
					}
					ch <-= ref Event.Emouse(Point(x,y), E->Mrbuttondown);
				}
			}
		}
		if(domoved)
			ch <-= ref Event.Emouse(Point(x,y), E->Mmove);
		xold = x;
		yold = y;
		bold = b;
	}
}

makepopup(width, height: int) : ref Draw->Image
{
	rmain := mainwin.r;
	x := rmain.min.x + (rmain.dx() - width) / 2;
	if(x < 0)
		x = 0;
	y := rmain.min.y + (rmain.dy() - height) / 2;
	if(y < 0)
		y = 0;
	r := Rect(Point(x,y),Point(x+width,y+height));
	popupwin = screen.newwindow(r, display.rgb2cmap(16rDD, 16rDD, 16rDD));
	popupwin.flush(D->Flushoff);
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

skeyboard(nil: ref Draw->Context)
{
}

snarfput(nil: string)
{
}

grabmouse(nil : int)
{
}
