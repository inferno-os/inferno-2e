implement Charon;

include "common.m";
include "cci.m";
include "debug.m";
#####################international
include "charon_gui.m";
include "charon_code.m";

sys: Sys;
CU: CharonUtils;
	ByteSource, MaskedImage, CImage, ImageCache, ReqInfo, Header, 
	ResourceState, config: import CU;

D: Draw;
	Point, Rect, Font, Image, Display, Screen: import D;
	charon_gui: Charon_gui;
	charon_code: Charon_code;
S: String;
U: Url;
	ParsedUrl: import U;
L: Layout;
	Frame, Loc, Control: import L;
I: Img;
	ImageSource: import I;

B: Build;
	Item, Dimen: import B;

E: Events;
	Event: import E;

J: Script;

cci: CCI;

G: Gui;
	popupwin, grabmouse : import G;

include "sh.m"; 
	mailtool: Command;

# package up info related to a navigation command
GoSpec: adt {
	kind: int;			# GoNormal, etc.
	url: ref ParsedUrl;		# destination (absolute)
	meth: int;			# HGet or HPost
	body: string;		# used if HPost
	target: string;		# name of target frame
	auth: string;		# optional auth info
	histnode: ref HistNode;	# if kind is GoHistnode

	newget: fn(kind: int, url: ref ParsedUrl, target: string) : ref GoSpec;
	newpost: fn(url: ref ParsedUrl, body, target: string) : ref GoSpec;
	newspecial: fn(kind: int, histnode: ref HistNode) : ref GoSpec;
	equal: fn(a: self ref GoSpec, b: ref GoSpec) : int;
};

GoNormal, GoReplace, GoLink, GoHistnode, GoBookmarks, GoHome, GoHelp, GoHistory: con iota;

# Information about a set of frames making up the screen
DocConfig: adt {
	framename: string;		# nonempty, except possibly for topconfig
	title: string;
	initconfig: int;			# true unless this is a frameset and some subframe changed
	gospec: cyclic ref GoSpec;
	# TODO: add current y pos and form field values

	equal: fn(a: self ref DocConfig, b: ref DocConfig) : int;
	equalarray: fn(a1: array of ref DocConfig, a2: array of ref DocConfig) : int;
};

# Information about a particular screen configuration
HistNode: adt {
	topconfig: cyclic ref DocConfig;			# config of top (whole doc, or frameset root)
	kidconfigs: cyclic array of ref DocConfig;	# configs for kid frames (if a frameset)
	preds: cyclic list of ref HistNode;	# edges in (via normal navigation)
	succs: cyclic list of ref HistNode;	# edges out (via normal navigation)

	addedge: fn(a: self ref HistNode, b: ref HistNode, atob: int);
	copy: fn(a: self ref HistNode) : ref HistNode;
};

History: adt {
	h: array of ref HistNode;	# all visited HistNodes, in LRU order
	n: int;				# h[0:n] is valid part of h

	add: fn(h: self ref History, f: ref Frame, g: ref GoSpec, navkind: int);
	update: fn(h: self ref History, f: ref Frame);
	find: fn(h: self ref History, k: int) : ref HistNode;
	print: fn(h: self ref History);
	histinfo: fn(h: self ref History) : (int, string, string, string);
	findurl: fn(h: self ref History, s: string) : ref HistNode;
};

# Authentication strings
AuthInfo: adt {
	realm: string;
	credentials: string;
};

auths: list of ref AuthInfo = nil;

CtlLayout: adt {
	logoicon: ref Image;
	logopos: Point;
	butspos: Point;
	entrypos: Point;
	statuspos: Point;
	controls: list of ref Control;
	backbut: ref Control;
	fwdbut: ref Control;
	reloadbut: ref Control;
	stopbut: ref Control;
	histbut: ref Control;
	bmarkbut: ref Control;
	editbut: ref Control;
	confbut: ref Control;
	homebut: ref Control;
	helpbut: ref Control;
	copybut: ref Control;
	keybdbut: ref Control;
	exitbut: ref Control;
	entry: ref Control;
	status: string;
};

ProgLayout: adt {
	box: array of ref Control;
	nused: int;
	first: Point;
	dx: int;
	sslpos: Point;
	sslstate: int;
	sslon: ref Image;
	ssloff: ref Image;
};

PopupLayout: adt {
	kind: int;			# PopupNone, PopupAuth, etc.
	controls: list of ref Control;
	okbut: ref Control;
	cancelbut: ref Control;
	refocus: ref Loc;		# return focus here when popup done
};

PopupNone, PopupAuth, PopupSaveAs, PopupEditBookmarks, PopupConfig, PopupAlert, PopupConfirm, PopupPrompt: con iota;
popupans: chan of (int, string);
popupactive := 0;
# list action for edit bookmarks
CLmoveup, CLmovedown, CLdelete, CLadd: con iota;

history : ref History;
ctllay: CtlLayout;
proglay: ProgLayout;
popuplay: PopupLayout;
keyfocus: ref Loc;
mouseover: ref B->Anchor;
mouseoverfr: ref Frame;

CTLHEIGHT : con 65;	# height of control (menu and toolbar) window
PROGHEIGHT : con 24;	# height of  progress window
ALERTLINELEN: con 80;	# max number of characters in an alert line
SP : con 8;				# a spacer for between controls
SP2 : con 4;			# half of SP
SP3 : con 2;
pgrp := 0;
gopgrp := 0;
dbg := 0;
warn := 0;
dbgres := 0;
doscripts := 0;

top, curframe, ctlframe, progframe, popupframe: ref Frame;
mainwin, ctlwin, progwin: ref Image;
zp := Point(0,0);

tbw, tbr: chan of string;
usetoolbar: int = 0;

toolbarinit(ctxt: ref Draw->Context, argv: list of string, wc, rc: chan of string)
{
	sys = load Sys Sys->PATH;
	if(ctxt != nil && wc != nil && rc != nil) {
		if ((sys->bind(Gui->GUITBPATH, Gui->GUIWMPATH, Sys->MREPL)) < 0)
			sys->print("failed to bind %s over %s\n", Gui->GUITBPATH, Gui->GUIWMPATH);
		usetoolbar = 1;
	}
	# else ignore toolbar

	tbw = wc;
	tbr = rc;
	
	# append exec name
	argv = Charon->PATH :: argv;

	init(ctxt, argv);
}

init(ctxt: ref Draw->Context, argl: list of string)
{
	sys = load Sys Sys->PATH;
	(retval, nil) := sys->stat("/net/tcp");
	if(retval < 0)
		sys->bind("#I", "/net", sys->MREPL);
	(retval, nil) = sys->stat("/net/cs");
	if(retval < 0)
		startcs();

	pgrp = sys->pctl(sys->NEWPGRP, nil);
	exc := ref sys->Exception;
	if(sys->rescue("*", exc) == sys->EXCEPTION) {
		if(exc.name == "")
			sys->rescued(sys->EXIT, nil);
		fatalerror(sys->sprint("exception %s, in module %s, %s",
			exc.name, exc.mod, pctoloc(exc.mod, exc.pc)));
	}
	CU = load CharonUtils CharonUtils->PATH;
	if(CU == nil){
		estr := sys->sprint("EXInternal: couldn't load CharonUtils: %r");
		if(len estr > Sys->ERRLEN)
			estr = estr[:Sys->ERRLEN];
		sys->raise(estr);
	}
	errpath := CU->init(load Charon SELF, CU, argl, ctxt!=nil);
	if(errpath != "")
		fatalerror(sys->sprint("Couldn't load %s\n", errpath));

#####################international
	charon_gui= load Charon_gui Charon_gui->PATH;
	if(charon_gui==nil)
		CU->raise(sys->sprint("EXinternal:couldn't load Charon_gui:%r"));
	charon_gui->init();

	charon_code= load Charon_code Charon_code->PATH;
        if(charon_code==nil)
                CU->raise(sys->sprint("EXinternal:couldn't load Charon_code:%r"));
        charon_code->init();

	sys = load Sys Sys->PATH;
	D = load Draw Draw->PATH;
	S = load String String->PATH;
	U = load Url Url->PATH;
	E = CU->E;
	L = CU->L;
	I = CU->I;
	B = CU->B;
	J = CU->J;
	G = CU->G;

	dbg = int (CU->config).dbg['d'];
	warn = dbg ||  int (CU->config).dbg['w'];
	dbgres = int (CU->config).dbg['r'];
	doscripts = (CU->config).doscripts;
	showprog := (CU->config).showprogress;
	if(dbg && (CU->config).dbgfile != "") {
		dfile := sys->create((CU->config).dbgfile, sys->OWRITE, 8r777);
		if(dfile != nil) {
			sys->dup(dfile.fd, 1);
		}
	}
	curres := ResourceState.cur();
	newres: ResourceState;
	if(dbgres) {
		(CU->startres).print("starting resources");
		curres = ResourceState.cur();
	}
	x := config.x;
	y := config.y;
	rctl := Rect((x,y), (x+config.defaultwidth,y+CTLHEIGHT));
	rmain := Rect((x,y+CTLHEIGHT), (x+config.defaultwidth, y+config.defaultheight));
	rprog := Rect((x,rmain.max.y),(x+config.defaultwidth,rmain.max.y));
	if(showprog) {
		rmain.max.y -= PROGHEIGHT;
		rprog.min.y -= PROGHEIGHT;
	}
	if(usetoolbar)
		(rmain, rctl, rprog) = G->tbinit(ctxt, tbw, tbr, rmain, rctl, rprog, CU);
	else
		(rmain, rctl, rprog) = G->init(ctxt, rmain, rctl, rprog, CU);
	
	if(dbgres) {
		newres = ResourceState.cur();
		newres.since(curres).print("difference after G->init (made screen windows)");
		curres = newres;
	}
	mainwin = G->mainwin;
	ctlwin = G->ctlwin;
	progwin = G->progwin;
	# L->init() was deferred until after G was inited
	L->init(CU);
	if(dbgres) {
		newres = ResourceState.cur();
		newres.since(curres).print("difference after L->init (loaded Build, Lex)");
		curres = newres;
	}
	if(dbgres) {
		newres = ResourceState.cur();
		newres.since(curres).print("difference after I->init");
		curres = newres;
	}
	(CU->imcache).init();
	if(ctxt == nil)
		start(1);
	else {
		if(usetoolbar) 
			tbstart(1);
		else
			start(ctxt==nil);
	}
	if(J != nil)
		J->frametreechanged(top);
	startpage := config.starturl;
	g := GoSpec.newget(GoNormal, startpage, "_top");
	ech := E->evchan;
	if(showprog)
		spawn progressmon();
	if(dbgres) {
		newres = ResourceState.cur();
		newres.since(curres).print("difference after initial configure");
		curres = newres;
	}
	if(config.usecci) {
		cci = load CCI CCI->PATH;
		if(cci != nil)
			cci->init(S, E, U);
	}
	spawn go(g);
Forloop:
	for(;;) {
		ev := <- ech;
		if(dbg > 1) {
			pick de := ev {
			Emouse =>
				if(dbg > 2 || de.mtype != E->Mmove)
					sys->print("%s\n", ev.tostring());
			* =>
				sys->print("%s\n", ev.tostring());
			}
		}
		pick  e := ev {
		Ekey =>
			g = nil;
			case e.keychar {
			E->Kdown =>
				curframe.yscroll(L->CAscrollpage, -1);
				g = nil;	
			E->Kup =>
				curframe.yscroll(L->CAscrollpage, 1);
				g = nil;	
			E->Khome =>
				curframe.yscroll(L->CAscrollpage, -10000);
				g = nil;	
			E->Kend => 
				curframe.yscroll(L->CAscrollpage, 10000);	
				g = nil;	
			E->Kaup =>
				curframe.yscroll(L->CAscrollline, -1);
				g = nil;	
			E->Kadown => 
				curframe.yscroll(L->CAscrollline, 1);	
				g = nil;	
			* =>
				g = handlekey(e);
			}
		Emouse =>
			g = handlemouse(ctxt, e);
		Ereshape =>
			mainwin = G->mainwin;
			ctlwin = G->ctlwin;
			progwin = G->progwin;	
			if(usetoolbar)
				tbredrawctl(1);
			else
				redrawctl(1);
			redrawmain(1);
			tbredrawprog(1);	
			curframe = top;
			g = GoSpec.newspecial(GoHistnode, history.find(0));
		Eexpose =>
			g = nil;
		Ehide =>
			g = nil;
		Ehelp =>
			g = GoSpec.newspecial(GoHelp, nil);		
		Elower =>
			if(popupactive)
				finishpopup(0);
			g = nil;
		Equit =>
			break Forloop;
		Estop =>
			if(gopgrp != 0) {
				ctllay.stopbut.disable();
				CU->abortgo(gopgrp);
				showstatus("Stopped");
			}
			g = nil;
		Ealert =>
			spawn alert(e.msg, e.sync);
			g = nil;
		Econfirm =>
			spawn confirm(e.msg, e.sync);
			g = nil;
		Eprompt =>
			spawn prompt(e.msg, e.inputdflt, e.sync);
			g = nil;
		Eform =>
			formaction(e.frameid, e.formid, e.ftype, 0);
			g = nil;
		Eformfield =>
			formfieldaction(e.frameid, e.formid, e.fieldid, e.fftype);
			g = nil;
		Ego =>
			case e.gtype {
			E->EGnormal =>
				g = GoSpec.newget(GoNormal, U->makeurl(e.url), e.target);
			E->EGreplace =>
				g = GoSpec.newget(GoReplace, U->makeurl(e.url), e.target);
			E->EGreload =>
				g = GoSpec.newspecial(GoHistnode, history.find(0));
			E->EGforward =>
				g = GoSpec.newspecial(GoHistnode, history.find(1));
			E->EGback =>
				g = GoSpec.newspecial(GoHistnode, history.find(-1));
			E->EGhome =>
				g = GoSpec.newspecial(GoHome, nil);
			E->EGbookmarks =>
				g = GoSpec.newspecial(GoBookmarks, nil);
			E->EGdelta =>
				g = GoSpec.newspecial(GoHistnode, history.find(e.delta));
			E->EGlocation =>
				g = GoSpec.newspecial(GoHistnode, history.findurl(e.url));
			}
		Esubmit =>
			if(e.subkind == CU->HGet)
				g = GoSpec.newget(GoNormal, e.action, e.target);
			else {
				query := e.action.query;
				e.action.query = "";
				g = GoSpec.newpost(e.action, query, e.target);
			}
		}
		if(g != nil) {
			if(gopgrp != 0) {
				ctllay.stopbut.disable();
				CU->abortgo(gopgrp);
				showstatus(charon_gui->iStopped);
			}
			if(g.url != nil && g.url.scheme == Url->MAILTO) {
				sdest := g.url.tostring();
				showstatus(charon_gui->iRedirEmail + sdest);
				mailto := "-m" + sdest[7:];
				if(usetoolbar)
					tbw <-= "redirect EMAIL ; " + mailto;
				else {
					mailtool = load Command "/dis/mailtool/mailtool.dis";
					if(mailtool != nil) {
						args := mailto :: nil;
						spawn mailtool->init(ctxt, args);
					}
					else
						showstatus(charon_gui->iFailMail);
				}
			}
			else
				spawn go(g);
		}
	}
	finish();
}

start(needexit: int)
{
	top = Frame.new();
	ctlframe = Frame.new();	# not really a frame, but need cim pointer
	progframe = Frame.new();	# ditto
	popupframe = Frame.new();	# ditto
	curframe = top;
	history = ref History(nil, 0);
	(ctllay.logoicon, nil) = G->geticon(G->IClogo);
	(iback, ibackm) := G->geticon(G->ICback);
	#####################international
        ctllay.backbut = Control.newbutton(ctlframe,iback,ibackm,charon_gui->iGoBack,nil,1,1);
	(ifwd, ifwdm) := G->geticon(G->ICfwd);
	ctllay.fwdbut = Control.newbutton(ctlframe, ifwd, ifwdm, charon_gui->iGoForward, nil, 1, 1);
	(ireload, ireloadm) := G->geticon(G->ICreload);
	ctllay.reloadbut = Control.newbutton(ctlframe, ireload, ireloadm, charon_gui->iReload, nil, 1, 1);
	(istop, istopm) := G->geticon(G->ICstop);
	ctllay.stopbut = Control.newbutton(ctlframe, istop, istopm, charon_gui->iStop, nil, 1, 1);
	(ihist, ihistm) := G->geticon(G->IChist);
	ctllay.histbut = Control.newbutton(ctlframe, ihist, ihistm, charon_gui->iHistory, nil, 1, 1);
	(ibmark, ibmarkm) := G->geticon(G->ICbmark);
	ctllay.bmarkbut = Control.newbutton(ctlframe, ibmark, ibmarkm, charon_gui->iShowBookmarks, nil, 1, 1);
	(iedit, ieditm) := G->geticon(G->ICedit);
	 ctllay.editbut = Control.newbutton(ctlframe, iedit, ieditm, charon_gui->iEditBookmarks, nil, 1, 1);
	(iconf, iconfm) := G->geticon(G->ICconf);
        ctllay.confbut = Control.newbutton(ctlframe, iconf, iconfm, charon_gui->
iConfig, nil, 1, 1);
	(ihome, ihomem) := G->geticon(G->IChome);
        ctllay.homebut = Control.newbutton(ctlframe, ihome, ihomem, charon_gui->iHomePage,nil, 1, 1);
	(ihelp, ihelpm) := G->geticon(G->IChelp);
        ctllay.helpbut = Control.newbutton(ctlframe, ihelp, ihelpm, charon_gui->iHelp, nil, 1, 1);
	
	ctllay.controls = nil;
	if(needexit) {
		(iexit, iexitm) := G->geticon(G->ICexit);
                ctllay.exitbut = Control.newbutton(ctlframe, iexit, iexitm, charon_gui->iExit, nil, 1, 1);
		ctllay.controls = ctllay.exitbut :: nil;
	}
	else
		ctllay.exitbut = nil;
	ctllay.entry = Control.newentry(ctlframe, 30, 1, 0);
	ctllay.controls = ctllay.backbut :: ctllay.fwdbut :: ctllay.reloadbut :: 
		ctllay.stopbut :: ctllay.homebut ::  
		ctllay.histbut :: ctllay.bmarkbut :: ctllay.editbut ::	
		ctllay.confbut :: ctllay.helpbut ::  ctllay.entry :: ctllay.controls;
	ctllay.backbut.disable();
	ctllay.fwdbut.disable();
	ctllay.stopbut.disable();

	ctllay.status = "";
	keyfocus = frameloc(ctllay.entry, ctlframe);
	mouseover = nil;
	ctllay.entry.gainfocus(popupactive);
	popuplay.kind = PopupNone;
	popupans = chan of (int, string);
	(proglay.sslon, nil) =  G->geticon(G->ICsslon);
	(proglay.ssloff, nil) = G->geticon(G->ICssloff);
	redrawctl(1);
	redrawmain(1);
	tbredrawprog(1);
}

redrawctl(resized: int)
{
	ctlwin.clipr = ctlwin.r;
	r := ctlwin.r.inset(L->ReliefBd);
	L->drawfill(ctlwin, r, CU->Grey);
	L->drawrelief(ctlwin, r, L->ReliefRaised);
	ctlwin.clipr = r;
	n := 0;
	p := ctllay.logopos;
	li := ctllay.logoicon;
	lw := li.r.dx();
	lh := li.r.dy();
	conter := 0;
	if(resized) {
		ctlframe.r = ctlwin.r.inset(2*L->ReliefBd);
		ctlframe.cim = ctlwin;
		p = r.min.add(Point(7,7));
		ctllay.logopos = p;
		x := p.x + lw;
		y := p.y;
		x += SP;
		x1 := x;
		y1 := y;
		for(bl := ctllay.controls; bl != nil; bl = tl bl) {
			b := hd bl;
			if(b == ctllay.entry || b == ctllay.exitbut)
				continue;
			b.r = b.r.subpt(b.r.min);
			b.r = b.r.addpt(Point(x,y));
			if(conter == 4) {
				x = x1;
				y += b.r.dx() + SP3;
			}
			else {
				x += b.r.dx() + SP2;
			}
			conter++;
		}
		x += SP2;
		ctllay.entrypos = Point(x,y1);
		ctllay.entry.r = Rect(ctllay.entrypos, Point(r.max.x-7,y1+22));
		ctllay.statuspos = Point(x,y+SP);
		if(ctllay.exitbut != nil) {
			ctllay.entry.r.max.x -= ctllay.exitbut.r.dx() + SP2;
			ctllay.exitbut.r = ctllay.exitbut.r.addpt(Point(ctllay.entry.r.max.x+SP2,y1));
		}
	}
	#ctllay.statuspos = Point(p.x + lw + SP, p.y + (hd ctllay.controls).r.dy() + SP);
	ctlwin.draw(Rect(p,p.add(Point(lw,lh))), li, nil, zp);
	for(bl := ctllay.controls; bl != nil; bl = tl bl)
		(hd bl).draw(0);
	showstatus(ctllay.status);
	ctlwin.flush(D->Flushnow);
}

tbstart(nil: int)		# parameter is: needexit
{
	top = Frame.new();
	ctlframe = Frame.new();	# not really a frame, but need cim pointer
	progframe = Frame.new();	# ditto
	popupframe = Frame.new();	# ditto
	curframe = top;
	history = ref History(nil, 0);
	(ctllay.logoicon, nil) = G->geticon(G->IClogo);
	(iback, ibackm) := G->geticon(G->ICback);
        ctllay.backbut = Control.newbutton(ctlframe, iback, ibackm, charon_gui->iGoBack, nil, 1, 1);
	(ifwd, ifwdm) := G->geticon(G->ICfwd);
        ctllay.fwdbut = Control.newbutton(ctlframe, ifwd, ifwdm, charon_gui->iGoForward, nil, 1, 1);
	(ireload, ireloadm) := G->geticon(G->ICreload);
        ctllay.reloadbut = Control.newbutton(ctlframe, ireload, ireloadm, charon_gui->iReload, nil, 1, 1);
	(istop, istopm) := G->geticon(G->ICstop);
        ctllay.stopbut = Control.newbutton(ctlframe, istop, istopm, charon_gui->iStop, nil, 1, 1);
	(ihist, ihistm) := G->geticon(G->IChist);
        ctllay.histbut = Control.newbutton(ctlframe, ihist, ihistm, charon_gui->iHistory, nil, 1, 1);
	(ibmark, ibmarkm) := G->geticon(G->ICbmark);
        ctllay.bmarkbut = Control.newbutton(ctlframe, ibmark, ibmarkm, charon_gui->iShowBookmarks, nil, 1, 1);
	(iedit, ieditm) := G->geticon(G->ICedit);
        ctllay.editbut = Control.newbutton(ctlframe, iedit, ieditm, charon_gui->iEditBookmarks, nil, 1, 1);
	(iconf, iconfm) := G->geticon(G->ICconf);
        ctllay.confbut = Control.newbutton(ctlframe, iconf, iconfm, charon_gui->iConfig, nil, 1, 1);
	(ihome, ihomem) := G->geticon(G->IChome);
        ctllay.homebut = Control.newbutton(ctlframe, ihome, ihomem, charon_gui->iHomePage,nil, 1, 1);
	(ihelp, ihelpm) := G->geticon(G->IChelp);
        ctllay.helpbut = Control.newbutton(ctlframe, ihelp, ihelpm, charon_gui->iHelp, nil, 1, 1);
	(icopy, icopym) := G->geticon(G->ICcopy);
        ctllay.copybut = Control.newbutton(ctlframe, icopy, icopym, charon_gui->iCopyLoc, nil, 1, 1);
	ctllay.controls = nil;
	
	(ikbd, ikbdm) := G->geticon(G->ICkeybd);
        ctllay.keybdbut = Control.newbutton(ctlframe, ikbd, ikbd, charon_gui->iSoftKeyboard, nil, 1, 1);
	
	ctllay.entry = Control.newentry(ctlframe, 30, 1, 0);
	ctllay.controls = ctllay.backbut :: ctllay.fwdbut :: ctllay.reloadbut :: 
		ctllay.stopbut :: ctllay.homebut ::  ctllay.keybdbut ::
		ctllay.histbut :: ctllay.bmarkbut :: ctllay.editbut ::	
		ctllay.confbut :: ctllay.helpbut :: ctllay.copybut :: ctllay.entry :: ctllay.controls;
	ctllay.backbut.disable();
	ctllay.fwdbut.disable();
	ctllay.stopbut.disable();

	ctllay.status = "";
	keyfocus = frameloc(ctllay.entry, ctlframe);
	ctllay.entry.gainfocus(popupactive);
	popuplay.kind = PopupNone;
	popupans = chan of (int, string);
	(proglay.sslon, nil) =  G->geticon(G->ICsslon);
	(proglay.ssloff, nil) = G->geticon(G->ICssloff);
	tbredrawctl(1);
	redrawmain(1);
	tbredrawprog(1);
}

tbredrawctl(resized: int)
{
	ctlwin.clipr = ctlwin.r;
	r := ctlwin.r.inset(L->ReliefBd);
	L->drawfill(ctlwin, r, CU->Grey);
	L->drawrelief(ctlwin, r, L->ReliefRaised);
	ctlwin.clipr = r;
	p := r.min;
	li := ctllay.logoicon;
	lw := 0;
	lh := 0;	
	stat := 0;
	cont := 0;	
	if(!usetoolbar) {
		p = ctllay.logopos;
		lw = li.r.dx();
		lh = li.r.dy();
	}
	if(resized) {
		ctlframe.r = ctlwin.r.inset(2*L->ReliefBd);
		ctlframe.cim = ctlwin;
		if(!usetoolbar)
			p = r.min.add(Point(7,7));
		ctllay.logopos = p;
		x := p.x + lw;
		y := p.y + SP2;
		x += SP;
		x2 := x;	
		for(bl := ctllay.controls; bl != nil; bl = tl bl) {
			b := hd bl;

			if(b == ctllay.entry || b == ctllay.exitbut)
	 			continue;

			b.r = b.r.subpt(b.r.min);
			b.r = b.r.addpt(Point(x,y));
			if (cont == 3) {	
		        	x += 20;
			#	stat = x + 30 + SP2;
			}	
			if (cont == 5) {
				stat = x;	
				x = x2 - 28;	
				y = p.y + b.r.dy() + SP2;
			}	
		        if (cont == 9) 
				x += 20;
			cont += 1;	
			x += b.r.dx() + SP2;
		}
		x += SP2;
		x = r.max.x - 7;
		
		ctllay.entrypos = Point(stat + 49, y-23);
		ctllay.entry.r = Rect(ctllay.entrypos, Point(r.max.x-25,y));
		ctllay.statuspos = Point(stat + 49 , y+SP);
	}
	if(!usetoolbar)
		ctlwin.draw(Rect(p,p.add(Point(lw,lh))), li, nil, zp);
	for(bl := ctllay.controls; bl != nil; bl = tl bl){
		
		(hd bl).draw(0);
	}	
	showstatus(ctllay.status);
	ctlwin.flush(D->Flushnow);
}

redrawmain(resized: int)
{
	im := mainwin;
	if(resized) {
		top.r = im.r.inset(2*L->ReliefBd);
		top.cim = mainwin;
		top.reset();
		(CU->imcache).resetlimits();
	}
	im.clipr = im.r;
	L->drawrelief(im, top.r.inset(-L->ReliefBd), L->ReliefRaised);
	L->drawrelief(im, top.r, L->ReliefSunk);
	im.clipr = top.r;
	L->drawfill(im, top.r, CU->White);
	im.flush(D->Flushnow);
}

tbredrawprog(resized: int)
{
	progwin.clipr = progwin.r;
	r := progwin.r.inset(L->ReliefBd);
	L->drawfill(progwin, r, CU->Grey);
	L->drawrelief(progwin, r, L->ReliefRaised);
	progwin.clipr = r;
	p := proglay.sslpos;
	si : ref Image;
	if(proglay.sslstate)
		si = proglay.sslon;
	else
		si = proglay.ssloff;
	sw := si.r.dx();
	sh := si.r.dy();
	if(resized) {
		#p := proglay.first;
		nbox := len proglay.box;
		progframe.r = progwin.r.inset(2*L->ReliefBd);
		progframe.cim = progwin;
		nboxold := nbox;
		c := Control.newprogbox(progframe);
		vo := (r.dy() - c.r.dy()) / 2;
		p = r.min.add(Point(7,vo));
		proglay.sslpos = p;
		proglay.first = Point(p.x + sw + SP, p.y);
		proglay.dx = c.r.dx() + SP2;
		nbox = (r.dx() - 2*7 - SP2 - sw - SP) / proglay.dx;
		if(nboxold != nbox) {
			newbox := array[nbox] of ref Control;
			if(nboxold > 0) {
				if(nboxold > nbox)
					newbox[0:] = proglay.box[0:nbox];
				else {
					newbox[0:] = proglay.box;
				}
			}
			proglay.box = newbox;
			proglay.nused = min(proglay.nused, len proglay.box);
			for(i := nboxold; i < nbox; i++)
				proglay.box[i] = Control.newprogbox(progframe);
		}
		for(i := 0; i < nbox; i++) {
			c = proglay.box[i];
			c.r = c.r.subpt(c.r.min);
			c.r = c.r.addpt(Point(p.x+i*proglay.dx + sw + SP, p.y));
		}
	}
	progwin.draw(Rect(p,p.add(Point(sw,sh))), si, nil, zp);
	for(i := 0; i < proglay.nused; i++)
		proglay.box[i].draw(0);
	progwin.flush(D->Flushnow);
}

# Display popup of given kind (s & s2 are more info for drawing, depending on kind),
# and return user's answer on popupans as (code, string), where code will
# be -1 for error, 0 when the user hit cancel, and 1 when the user hit OK.
dopopup(kind: int, s, s2: string)
{
        cok := Control.newbutton(popupframe, nil, nil, charon_gui->iOk, nil, 1,1);
        ccancel := Control.newbutton(popupframe, nil, nil, charon_gui->iCancel,nil, 1, 1);
	p := Point(0,0);
	case kind {
	PopupAuth =>
		chead := Control.newlabel(popupframe, "Type your user name and password");
		crealm := Control.newlabel(popupframe, "Resource: " + s);
		cunlab := Control.newlabel(popupframe, "User Name: ");
		cpwlab := Control.newlabel(popupframe, "Password: ");
		cuser := Control.newentry(popupframe, 30, 1, 0);
		cpass := Control.newentry(popupframe, 30, 1, 0);

		w := SP + max(chead.r.max.x, max(crealm.r.max.x, cunlab.r.max.x + cuser.r.max.x)) + SP;
		w = min(w, mainwin.r.dx()-2*SP);
		h := SP + chead.r.max.y + SP + crealm.r.max.y + SP + cuser.r.max.y + SP + cpass.r.max.y
				+ 2*SP + cok.r.max.y + SP;
		popupwin = G->makepopup(w, h);
		if(popupwin == nil) {
			popupans <-= (-1, "");
			return;
		}

		# put user, password entries at beginning of list for easy access
		popuplay.controls = cuser :: cpass :: chead :: crealm :: cunlab :: cpwlab :: cok :: ccancel :: nil;

		p = popupwin.r.min.add(Point(SP,SP));
		chead.r = chead.r.addpt(p);
		p.y += chead.r.dy() + SP;
		crealm.r = crealm.r.addpt(p);
		p.y += crealm.r.dy() + SP;
		cunlab.r = cunlab.r.addpt(p);
		cuser.r = cuser.r.addpt(p.add(Point(cunlab.r.dx(),0)));
		p.y += cuser.r.dy() + SP;
		cpwlab.r = cpwlab.r.addpt(p);
		cpass.r = cuser.r.addpt(Point(0,cuser.r.dy()+SP));
		p.y += cpass.r.dy() + 2*SP;
		pick cinit := cpass {
		Centry =>
			cinit.flags |= L->CFsecure;
		}

	PopupSaveAs =>
		clab := Control.newlabel(popupframe, "Save As: ");
		cfile := Control.newentry(popupframe, 40, 1, 0);
		pick ce := cfile {
		Centry =>
			ce.s = s;
		}

		w := SP + clab.r.max.x + cfile.r.max.x + SP;
		h := SP + cfile.r.max.y + 2*SP + cok.r.max.y + SP;
		popupwin = G->makepopup(w, h);
		if(popupwin == nil) {
			popupans <-= (-1, "");
			return;
		}
		popuplay.controls = cfile :: clab :: cok :: ccancel :: nil;

		p = popupwin.r.min.add(Point(SP,SP));
		clab.r = clab.r.addpt(p);
		cfile.r = cfile.r.addpt(p.add(Point(clab.r.dx()+SP,0)));
		p.y += clab.r.dy() + 2*SP;

	# PopupAlert/PopupConfirm/PopupPrompt similar
	# PopupConfirm adds Cancel button to PopupAlert
	# PopupPrompt adds Entry field to PopupConfirm
	PopupAlert or
	PopupConfirm or
	PopupPrompt =>
		# split s up into lines of at most ALERTLINELEN characters each
		(n, words) := sys->tokenize(s, " \t\n\r");
		lines : list of string = nil;
		curlen := 0;
		curline := "";
		for( ; words != nil; words = tl words) {
			w := hd words;
			nw := len w;
			if(curlen + nw >= ALERTLINELEN && curline != "") {
				lines = curline :: lines;
				curline = w;
				curlen = nw;
			}
			else {
				if(curline != "") {
					curline += " ";
					curlen++;
				}
				curline += w;
				curlen += nw;
			}
		}
		if(curline != "")
			lines = curline :: lines;

		# lines are in reverse order, so the following
		# puts the controls in the correct order
		popuplay.controls = nil;
		w := SP + cok.r.max.x + SP;
		if(kind == PopupAlert)
			ccancel = nil;
		else
			w += ccancel.r.max.x + SP;
		h := SP + 2*SP + cok.r.max.y + SP;
		for(; lines != nil; lines = tl lines) {
			l := hd lines;
			c := Control.newlabel(popupframe, l);
			popuplay.controls = c :: popuplay.controls;
			w = max(w, SP + c.r.dx() + SP);
			h += c.r.dy();
		}
		cinput : ref Control;
		if(kind == PopupPrompt) {
			cinput = Control.newentry(popupframe, 40, 1, 0);
			pick ce := cinput {
			Centry =>
				ce.s = s2;
				ce.sel = (len s2, len s2);
			}
			w = max(w, SP + cinput.r.dx() + SP);
			h += cinput.r.dy() + SP;
		}
		popupwin = G->makepopup(w, h);
		if(popupwin == nil) {
			popupans <-= (-1, "");
			return;
		}
		p = popupwin.r.min.add(Point(SP,SP));
		for(cl := popuplay.controls; cl != nil; cl = tl cl) {
			c := hd cl;
			c.r = c.r.addpt(p);
			p.y += c.r.dy();
		}
		if(kind == PopupPrompt) {
			p.y += SP;
			cinput.r = cinput.r.addpt(p);
			p.y += cinput.r.dy();
		}
		p.y += 2*SP;
		if(kind != PopupAlert)
			popuplay.controls = ccancel :: popuplay.controls;
		popuplay.controls = cok :: popuplay.controls;
		if(kind == PopupPrompt)
			popuplay.controls = cinput :: popuplay.controls;

	PopupEditBookmarks =>
                chead := Control.newlabel(popupframe, charon_gui->iEditBookmarks);
		(iup, iupm) := G->geticon(G->ICup);
                cmoveup := Control.newbutton(popupframe, iup, iupm, charon_gui->iMoveupButton, nil, 1, 1);
		(idown, idownm) := G->geticon(G->ICdown);
		cmovedown := Control.newbutton(popupframe, idown, idownm, charon_gui->iMovedownButton, nil, 1, 1);
		(idel, idelm) := G->geticon(G->ICminus);
		cdelete := Control.newbutton(popupframe, idel, idelm, charon_gui->iDeleteButton, nil, 1, 1);
		(iadd, iaddm) := G->geticon(G->ICplus);
		cadd := Control.newbutton(popupframe, iadd, iaddm, charon_gui->iAddButton, nil, 1, 1);

		options: array of B->Option;
		fname := config.userdir + "/bookmarks.html";
		fd := sys->open(fname, sys->OREAD);
		if(fd != nil) {
			buf := array [8192] of byte;
			nbyte := sys->read(fd, buf, len buf);
			if(nbyte <= 0)
				options = nil;
			else
				options = parsebookmarks(buf[0:nbyte]);
		}
		if(options != nil && len options != 0)
			options[0].selected = 1;
		clist := Control.newlistbox(popupframe, 8, 25, options);
                ctitlelab := Control.newlabel(popupframe, charon_gui->iTitleLabel);
                curllab := Control.newlabel(popupframe, charon_gui->iUrlLabel);
		ctitle := Control.newentry(popupframe, 21, 1, 0);
		curl := Control.newentry(popupframe, 21, 1, 0);

		xp := clist.r.max.x + SP;
		w := SP + xp + SP + cmovedown.r.max.x + SP;
		h := SP + chead.r.max.y + SP + clist.r.max.y + SP + 
			ctitle.r.max.y + SP + curl.r.max.y + 2*SP + 
			cok.r.max.y + SP;
		popupwin = G->makepopup(w, h);
		if(popupwin == nil)
			popupans <-= (-1, "");

		popuplay.controls = clist :: ctitle :: curl :: cmoveup :: 
			cmovedown :: cdelete :: cadd :: cok :: ccancel :: 
			chead :: ctitlelab :: curllab :: nil;

		p = popupwin.r.min.add(Point(SP, SP));
		chead.r = chead.r.addpt(p);
		p.y += chead.r.dy() + SP;
		clist.r = clist.r.addpt(p);
		xp += SP;
		cmoveup.r = cmoveup.r.addpt(p.add(Point(xp, 0)));
		yp := cmoveup.r.dy() + SP;
		cdelete.r = cdelete.r.addpt(p.add(Point(xp, yp)));
		yp += cdelete.r.dy() + SP;
		cmovedown.r = cmovedown.r.addpt(p.add(Point(xp, yp)));
		p.y += clist.r.dy() + SP;
		ctitlelab.r = ctitlelab.r.addpt(p);
		ctitle.r = ctitle.r.addpt(p.add(Point(ctitlelab.r.dx(), 0))); 
		p.y += ctitle.r.dy() + SP;
		curllab.r = curllab.r.addpt(p);
		curl.r = curl.r.addpt(p.add(Point(curllab.r.dx(), 0)));
		cadd.r = cadd.r.addpt(p.add(Point(xp, 0)));
		p.y += curl.r.dy() + 2*SP; 

		if(history != nil && history.h != nil) {
			root := history.h[history.n-1].topconfig;
			pick c := ctitle {
			Centry =>
				c.s = root.title; 
			}
			pick c := curl {
			Centry =>
				c.s = root.gospec.url.tostring();
			}
		}

	PopupConfig =>
                chead := Control.newlabel(popupframe, charon_gui->iUserPreferences);
                cinfo := Control.newlabel(popupframe, charon_gui->iProxyCaption1);
                cinfo1 := Control.newlabel(popupframe, charon_gui->iProxyCaption2);
                cproxylab := Control.newlabel(popupframe, charon_gui->iProxyLabel);
		cproxy := Control.newentry(popupframe, 25, 1, 0);
                cportlab := Control.newlabel(popupframe, charon_gui->iPortLabel);
		cport := Control.newentry(popupframe, 5, 1, 0);
                cnoproxylab := Control.newlabel(popupframe, charon_gui->iNoproxyLabel);
		cnoproxy := Control.newentry(popupframe, 31, 1, 0);
                chomelab := Control.newlabel(popupframe, charon_gui->iHomepageLabel);
		chome := Control.newentry(popupframe, 33, 1, 0);
                cimglab := Control.newlabel(popupframe, charon_gui->iLoadimageLabel);
		cimg := Control.newcheckbox(popupframe, 0);
 		cssllab := Control.newlabel(popupframe, charon_gui->iUseSSL);
 		cv2lab := Control.newlabel(popupframe, charon_gui->iSSLV2);
 		cv2 := Control.newcheckbox(popupframe, 0);
 		cv3lab := Control.newlabel(popupframe, charon_gui->iSSLV3);
 		cv3 := Control.newcheckbox(popupframe, 0);

		w := SP + max(cinfo.r.max.x, max(cnoproxylab.r.max.x+cnoproxy.r.max.x, 
			cproxylab.r.max.x+cproxy.r.max.x+cportlab.r.max.x+cport.r.max.x)) + SP;
		h := SP + chead.r.max.y + SP + cinfo.r.max.y + SP2 + cinfo1.r.max.y + SP + 
			cproxy.r.max.y + SP + cnoproxy.r.max.y + SP + chome.r.max.y + SP +
			cimg.r.max.y + SP + cv2.r.max.y + 2*SP + cok.r.max.y + SP;
 
		popupwin = G->makepopup(w, h);
		if(popupwin == nil){
			popupans <-= (-1, "");
			return;
		}

 		popuplay.controls = cproxy :: cport :: cnoproxy :: chome :: cimg :: cv2
 			:: cv3 :: cok :: ccancel :: chead :: cinfo :: cinfo1 :: cproxylab :: cportlab ::
 			cnoproxylab :: chomelab :: cimglab :: cssllab :: cv2lab :: 
 			cv3lab :: nil;

		p = popupwin.r.min.add(Point(SP, SP));
		chead.r = chead.r.addpt(p);
		p.y += chead.r.dy() + SP;
		cinfo.r = cinfo.r.addpt(p);
		p.y += cinfo.r.dy();
		cinfo1.r = cinfo1.r.addpt(p);
		p.y += cinfo1.r.dy() + SP;
		cproxylab.r = cproxylab.r.addpt(p);
		xp := cproxylab.r.dx();
		cproxy.r = cproxy.r.addpt(p.add(Point(xp, 0)));
		xp += cproxy.r.dx();
		cportlab.r = cportlab.r.addpt(p.add(Point(xp, 0)));
		xp += cportlab.r.dx();	
		cport.r = cport.r.addpt(p.add(Point(xp, 0)));
		p.y += cproxy.r.dy() + SP;
		cnoproxylab.r = cnoproxylab.r.addpt(p);
		cnoproxy.r = cnoproxy.r.addpt(p.add(Point(cnoproxylab.r.dx(), 0)));
		p.y += cnoproxy.r.dy() + SP;
		chomelab.r = chomelab.r.addpt(p);
		chome.r = chome.r.addpt(p.add(Point(chomelab.r.dx(), 0)));
		p.y += chome.r.dy() + SP;
		cimglab.r = cimglab.r.addpt(p);
 		cimg.r = cimg.r.addpt(p.add(Point(cimglab.r.dx(), SP2)));
 		p.y += cimg.r.dy() + SP;
 		cssllab.r = cssllab.r.addpt(p);
 		xp = cssllab.r.dx();
 		cv2lab.r = cv2lab.r.addpt(p.add(Point(xp, 0)));
 		xp += cv2lab.r.dx();
 		cv2.r = cv2.r.addpt(p.add(Point(xp, SP2)));
 		xp += cv2.r.dx() + SP;
 		cv3lab.r = cv3lab.r.addpt(p.add(Point(xp, 0)));
		xp += cv3.r.dx() + SP;
		cv3.r = cv3.r.addpt(p.add(Point(xp, SP2)));
		p.y += cv2.r.dy() + 2*SP;

		pick cinit := cproxy {
		Centry =>
			if(config.httpproxy != nil)
				cinit.s = config.httpproxy.host;
		}
		pick cinit := cport {
		Centry =>
			if(config.httpproxy != nil)
				cinit.s = config.httpproxy.port;
		}

		pick cinit := cnoproxy {
		Centry =>
			cinit.s = "";
			doml := config.noproxydoms;
			while(doml != nil) {
				cinit.s += hd doml + ", ";			
				doml = tl doml;
			}
		}
		pick cinit := chome {
		Centry =>
			cinit.s = config.homeurl.tostring();
		}
		if(!config.change_homeurl)
			chome.disable();	

		pick cinit :=  cimg {
		Ccheckbox =>
			if(config.imagelvl == 0)
				cimg.flags &= ~L->CFactive; 
			else
				cimg.flags |= L->CFactive;

		}

 		pick v2 := cv2 {
 		Ccheckbox =>
 			if(config.usessl & CU->SSLV2)
 				v2.flags |= L->CFactive;
 			else
 				v2.flags &= ~L->CFactive;
 		}
 
 		pick v3 := cv3 {
 		Ccheckbox =>
 			if(config.usessl & CU->SSLV3)
 				v3.flags |= L->CFactive;
 			else
 				v3.flags &= ~L->CFactive;
 		}
	* =>
		CU->assert(0);
	}
	# center the buttons on the bottom
	if(ccancel == nil) {
		p.x += (popupwin.r.dx() - cok.r.max.x) / 2 - SP;
		cok.r = cok.r.addpt(p);
	}
	else {
		p.x += (popupwin.r.dx() - (cok.r.max.x + SP + ccancel.r.max.x)) / 2 - SP;
		cok.r = cok.r.addpt(p);
		p.x += cok.r.dx() + SP;
		ccancel.r = ccancel.r.addpt(p);
	}

	popupframe.cim = popupwin;
	popuplay.kind = kind;
	popuplay.okbut = cok;
	popuplay.cancelbut = ccancel;
	popupactive = 1;
	oldc := keyfocus.le[keyfocus.n-1].control;
	if(oldc != nil) {
		popuplay.refocus = keyfocus;
		oldc.losefocus(popupactive);
	}
	else
		popuplay.refocus = nil;
	newc := hd popuplay.controls;
	keyfocus = frameloc(newc, popupframe);
	newc.gainfocus(popupactive);
	redrawpopup();
	# answer will be delivered from finishpopup
}

redrawpopup()
{
	if(popuplay.kind == PopupNone)
		return;
	CU->assert(popupwin != nil);
	popupwin.clipr = popupwin.r;
	r := popupwin.r.inset(L->ReliefBd);
	L->drawfill(popupwin, r, CU->Grey);
	L->drawrelief(popupwin, r, L->ReliefRaised);
	popupwin.clipr = r;
	for(cl := popuplay.controls; cl != nil; cl = tl cl)
		(hd cl).draw(0);
	popupwin.flush(D->Flushnow);
}

finishpopup(code: int)
{
	str := "";
	case popuplay.kind {
	PopupConfig =>
		if(code == 1) {
			c := popuplay.controls;
			host := ctlentrytext(hd c);
			c = tl c;
			port := ctlentrytext(hd c);
			if(host == nil)
				config.httpproxy = nil;
			else
				config.httpproxy = CU->makeabsurl("http://" + host + ":" + port);
			c = tl c;
			(nil, config.noproxydoms) = sys->tokenize(ctlentrytext(hd c), ";, \t");
			c = tl c;
			homeurl := ctlentrytext(hd c);
			if(homeurl != "")
				config.homeurl = CU->makeabsurl(homeurl);
			c = tl c;
			config.imagelvl = 0; # default
			pick cc := hd c {
			Ccheckbox =>
				if(cc.flags&L->CFactive)
					config.imagelvl = 3;
			* =>
				CU->assert(0);
			}
 			c = tl c;
 			pick v2 := hd c {
 			Ccheckbox =>
 				if(v2.flags&L->CFactive)
 					config.usessl |= CU->SSLV2;
 			* =>
 				CU->assert(0);
 			} 
 			c = tl c;
 			pick v3 := hd c {
 			Ccheckbox =>
 				if(v3.flags&L->CFactive)
 					config.usessl |= CU->SSLV3;
 			* =>
 				CU->assert(0);
 			}
			fname := config.userdir + "/config";
			if(CU->saveconfig(fname) < 0){
				sys->print("can't save user config\n");
				code = -1;
			}
		}
	PopupEditBookmarks =>
		if(code == 1) {
			pick c := hd popuplay.controls {
			Clistbox =>
				savebookmarks(c.options);
			}
		}
	PopupAuth =>
		str = ctlentrytext(hd popuplay.controls) + ":" +
			ctlentrytext(hd (tl popuplay.controls));
	PopupSaveAs or
	PopupPrompt =>
		str = ctlentrytext(hd popuplay.controls);
	}
	popuplay.kind = PopupNone;
	popuplay.controls = nil;
	popuplay.okbut = nil;
	popuplay.cancelbut = nil;
	popupframe.cim = nil;
	popupwin = nil;
	refocus := popuplay.refocus;
	if(refocus != nil) {
		keyfocus = refocus;
		popuplay.refocus = nil;
		oldc := keyfocus.le[keyfocus.n-1].control;
		oldc.gainfocus(popupactive);
	}
	popupactive = 0;
	popupans <-= (code, str);
}

# assuming c is an entry control, return its contents
ctlentrytext(c: ref Control) : string
{
	pick ec := c {
	Centry =>
		return ec.s;
	* =>
		CU->assert(0);
	}
	return "";
}

# Return a Loc representing a control in the frame f
frameloc(c: ref Control, f: ref Frame) : ref Loc
{
	loc := Loc.new();
	loc.add(L->LEframe, f.r.min);
	loc.le[loc.n-1].frame = f;
	loc.add(L->LEcontrol, c.r.min);
	loc.le[loc.n-1].control = c;
	return loc;
}

# Frame oldf is being reset, so change keyfocus back to ctllay.entry
resetkeyfocus(nil: ref Frame)
{
	keyfocus = frameloc(ctllay.entry, ctlframe);
}

popupmouse(nil: ref Draw->Context, e: ref Event.Emouse, grab: ref Control) : (ref GoSpec, ref Control)
{
	p := e.p;
	action : int;
	newgrab : ref Control;
	cindex := 0;

	for(cl := popuplay.controls; cl != nil; (cl, cindex) = (tl cl, cindex + 1)) {
		c := hd cl;
		if((c == grab) || (grab == nil && p.in(c.r))) {
			if(dbg > 1)
				sys->print("mouse in popup control\n");
			(action, newgrab) = c.domouse(p, e.mtype, grab);
			case (action) {
			L->CAbuttonpush =>
				if(c == popuplay.okbut)
					finishpopup(1);
				else if(c == popuplay.cancelbut)
					finishpopup(0);
				if(popuplay.kind == PopupEditBookmarks) {
					clist := hd popuplay.controls;
					# TODO: edit listbox
					case cindex {
					3 => # move up
						ctllist(clist, CLmoveup, "", "");
					4 => # move down 
						ctllist(clist,  CLmovedown, "", "");
					5 => # delete
						ctllist(clist, CLdelete, "", "");
					6 => # add
						display, value: string;
						pick ctitle := hd tl popuplay.controls {
						Centry =>
							display = ctlentrytext(ctitle);
						}
						pick curl := hd tl tl popuplay.controls {
						Centry =>
							value = " HREF=" + ctlentrytext(curl) + " TARGET=_top";
						}
						ctllist(clist, CLadd, value, display); 		
	 				}  	
					#clist.draw(1);
				}
			L->CAkeyfocus =>
				if(dbg > 1)
					keyfocus.print("old focus");
				oldc := keyfocus.le[keyfocus.n-1].control;
				if(oldc != nil)
					oldc.losefocus(popupactive);
				keyfocus = frameloc(c, popupframe);
				c.gainfocus(popupactive);
				if(dbg > 1)
					keyfocus.print("new focus");
			}
			break;
		}
	}
	return (nil, newgrab);
}

controlmouse(ctxt : ref Draw->Context, e: ref Event.Emouse, grab : ref Control) : (ref GoSpec, ref Control)
{
	p := e.p;
	g : ref GoSpec;
	action : int;
	newgrab : ref Control;

	for(cl := ctllay.controls; cl != nil; cl = tl cl) {
		c := hd cl;
		if((c == grab) || (grab == nil && p.in(c.r))) {
			if(dbg > 1)
				sys->print("mouse in controlwin control\n");
			(action, newgrab) = c.domouse(p, e.mtype, grab);
			case (action) {
			L->CAbuttonpush =>
				if(c == ctllay.backbut)
					g = GoSpec.newspecial(GoHistnode, history.find(-1));
				else if(c == ctllay.fwdbut)
					g = GoSpec.newspecial(GoHistnode, history.find(1));
				else if(c == ctllay.reloadbut)
					g = GoSpec.newspecial(GoHistnode, history.find(0));
				else if(c == ctllay.histbut)
					g = GoSpec.newspecial(GoHistory, nil);
				else if(c == ctllay.bmarkbut)
					g = GoSpec.newspecial(GoBookmarks, nil);
				else if(c == ctllay.stopbut) {
					if(gopgrp != 0) {
						ctllay.stopbut.disable();
						CU->abortgo(gopgrp);
						showstatus(charon_gui->iStopped);
					}		
				}
				else if(c == ctllay.editbut)
					spawn editbookmarks();

				else if(c == ctllay.confbut) 		
					spawn doconfig();

				else if(c == ctllay.homebut)
					g = GoSpec.newspecial(GoHome, nil);

				else if(c == ctllay.helpbut)
					g = GoSpec.newspecial(GoHelp, nil);

				else if(c == ctllay.copybut) {
					pick ce := ctllay.entry {
					Centry =>
						s := ce.s;
						if(s != "")
							G->snarfput(s);
					}
				}
 				else if(c == ctllay.keybdbut) {
					if(usetoolbar) {
						# handle software keyboard
						spawn G->skeyboard(ctxt);
					}
				}
				else if(c == ctllay.exitbut)
					finish();
			L->CAflyover =>
				pick pc := c {
				Cbutton =>
					showstatus(pc.label);
				}
			L->CAkeyfocus =>
				if(dbg > 1)
					keyfocus.print("old focus");
				oldc := keyfocus.le[keyfocus.n-1].control;
				if(oldc != nil)
					oldc.losefocus(popupactive);
				keyfocus = frameloc(c, ctlframe);
				c.gainfocus(popupactive);
				if(dbg > 1)
					keyfocus.print("new focus");
			}
			break;
		}
	}
	return (g, newgrab);
}

mainwinmouse(nil : ref Draw->Context, e: ref Event.Emouse, grab : ref Control) : (ref GoSpec, ref Control)
{
	p := e.p;
	g : ref GoSpec;
	ctl : ref Control;
	newgrab : ref Control;
	action : int;
	domouseout := 0;
	loc : ref Loc;
	if(mouseover != nil)
		domouseout = 1;
	if (grab != nil) {
		ctl = grab;
		loc = grabloc;
		# fix-up loc.pos as mouse point relative to control origin
		f := loc.lastframe();
		loc.pos = p.sub(loc.le[loc.n-1].pos);
	} else {
		loc = top.find(p, nil);
		if(loc != nil) {
			if(dbg > 1)
				loc.print("mouse loc");
			f := loc.lastframe();
			hasscripts := f.doc.hasscripts;
			if(e.mtype != E->Mmove)
				curframe = f;
			n1 := loc.n-1;
			case loc.le[n1].kind {
			L->LEitem =>
				it := loc.le[n1].item;
				if(it.anchorid >= 0) {
					for(al := f.doc.anchors; al != nil; al = tl al) {
						a := hd al;
						if(a.index == it.anchorid) {
							if(dbg > 1)
								sys->print("in anchor %d, href=%s\n", a.index, a.href.tostring());
							if(doscripts && a.events != nil) {
								if(a == mouseover) {
									domouseout = 0;	# still over same anchor
								}
								else if(e.mtype == E->Mmove) {
									if(domouseout) {
										se := ref E->ScriptEvent(E->SEonmouseout,
											mouseoverfr.id, -1, -1, mouseover.index, -1, 0, 0, 0, nil);
										J->jevchan <-= se;
										domouseout = 0;
									}
									mouseover = a;
									mouseoverfr = f;
									se := ref E->ScriptEvent(E->SEonmouseover,
										f.id, -1, -1, a.index, -1,
										e.p.x, e.p.y, 0, nil);
									J->jevchan <-= se;
								}
							}
							if(e.mtype == E->Mlbuttonup || e.mtype == E->Mldrop)
								g = anchorgospec(it, a, loc.pos);
							else if(e.mtype == E->Mmbuttonup)
								showstatus(a.href.tostring());
						}
					}
				}
			L->LEcontrol =>
				ctl = loc.le[n1].control;
			}
		}
	}

	if (ctl != nil) {
		ev := -1;
		(action, newgrab) = ctl.domouse(p, e.mtype, grab);
		case (action) {
		L->CAbuttonpush =>
			if(doscripts && ctl.ff != nil && ctl.ff.events != nil)
				ev = E->SEonclick;
			else {
				pushaction(ctl, loc);
				g = nil;
			}
		L->CAkeyfocus =>
			if(dbg > 1)
				keyfocus.print("old focus");
			oldc := keyfocus.le[keyfocus.n-1].control;
			if(oldc != nil)
				oldc.losefocus(popupactive);
			keyfocus = frameloc(ctl, ctlframe);
			ctl.gainfocus(popupactive);
			if(dbg > 1)
				keyfocus.print("new focus");
		L->CAchanged =>
			# Select Formfield - selection has changed
			ev = E->SEonchange;
		L->CAselected =>
			# text input Formfield - text selection has changed
			ev = E->SEonselect;
		}
		if (ev != -1 && doscripts && ctl.ff != nil && ctl.ff.events != nil) {
			se := ref E->ScriptEvent(ev, ctl.f.id, ctl.ff.form.formid, ctl.ff.fieldid,
					-1, -1, e.p.x, e.p.y, 1, nil);
			J->jevchan <-= se;
			return (nil, newgrab);
		}
	}
	if(newgrab == nil && domouseout && doscripts) {
		se := ref E->ScriptEvent(E->SEonmouseout,
			mouseoverfr.id, -1, -1, mouseover.index, -1, 0, 0, 0, nil);
		J->jevchan <-= se;
		mouseoverfr = nil;
		mouseover = nil;
	}
	if (newgrab != nil)
		grabloc = loc;
	return (g, newgrab);
}

progwinmouse(nil : ref Draw->Context, e: ref Event.Emouse, grab : ref Control) : (ref GoSpec, ref Control)
{
	action : int;
	p := e.p;
	newgrab : ref Control;

	for(i := 0; i < len proglay.box; i++) {
		c := proglay.box[i];
		if((c == grab) || (grab == nil && p.in(c.r))) {
			if(dbg > 1)
				sys->print("mouse in progbox control %d\n", i);
			(action, newgrab) = c.domouse(p, e.mtype, grab);
			case (action) {
			L->CAbuttonpush =>
				pick pc := c {
				Cprogbox =>
					msg := sys->sprint("%s, %d%% done", pc.src, pc.pcnt);
					if(pc.err != "")
						msg += ", " + pc.err;
					if(dbg)
						msg += sys->sprint(", bsid=%d", pc.bsid);
					showstatus(msg);
				}
			}
		}
	}
	return (nil, newgrab);
}


grabctl : ref Control;
grabloc : ref Loc;
grabrgn : int;
Rpopup, Rcontrol, Rmainwin, Rprogwin : con iota;

# If mouse event results in command to navigate somewhere else,
# return a GoSpec ref, else nil.
# TODO: deactivate activated controls if mouse leaves the area;
# perhaps do grabs?

handlemouse(ctxt : ref Draw->Context, e: ref Event.Emouse) : ref GoSpec
{
	newgrab : ref Control;
	p := e.p;
	g : ref GoSpec = nil;

	if (popupactive) {
		(g, newgrab) = popupmouse(ctxt, e, grabctl);
		grabrgn = Rpopup;
	} else if((grabctl == nil && p.in(ctlwin.r)) || (grabctl != nil && grabrgn == Rcontrol)) {
		(g, newgrab) = controlmouse(ctxt, e, grabctl);
		grabrgn = Rcontrol;
	} else if(grabctl == nil && (p.in(mainwin.r)) || (grabctl != nil && grabrgn == Rmainwin)) {
		(g, newgrab) = mainwinmouse(ctxt, e, grabctl);
		grabrgn = Rmainwin;
	} else if((grabctl == nil && p.in(progwin.r)) || (grabctl != nil && grabrgn == Rprogwin)) {
		(g, newgrab) = progwinmouse(ctxt, e, grabctl);
		grabrgn = Rprogwin;
	}
	if (grabctl != newgrab)
		grabmouse(newgrab != nil);
	grabctl = newgrab;
	return g;
}

editbookmarks()
{
	dopopup(PopupEditBookmarks, "", "");
	(code, ans) := <-popupans;
	if(code == -1)
		sys->print("couldn't create popup window\n");
	else {
		# modify bookmarks
	}
}

doconfig()
{
	dopopup(PopupConfig, "", "");
	(code, ans) := <-popupans;
	if(code == -1)
		sys->print("couldn't create popup window\n");
	else {
		# modify config data
	}
}

ctllist(sc: ref Control, action: int, value, display: string)
{
	changed := 0;
	pick c := sc {
	Clistbox =>
		case action {
		CLmoveup =>
			for(i := 0; i < len c.options; i++) {
				if(c.options[i].selected == 1) {
					if(i != 0) {
						changed = 1;
						swapopt(c.options, i-1, i);
					}
					break;
				}
			}

		CLmovedown =>
			for(i := 0; i < len c.options; i++) {
				if(c.options[i].selected == 1) {
					if(i != (len c.options - 1)) {
						changed = 1;
						swapopt(c.options, i, i+1);
					}
					break;
				}
			}

		CLdelete =>
			if(c.options == nil){
				c.first = 0;
				return;
			}
			n := len c.options;
			if(n <= 1) {
				c.options = nil;
			}
			else {
				nopt := array[n-1] of B->Option;
				i, found, sel : int = 0;
				for(; i < n; i++) {
					if(c.options[i].selected == 1){
						sel = i;
						found = 1;
						break;
					}
					else {
						nopt[i].selected = 0;
						nopt[i].value = c.options[i].value;
						nopt[i].display = c.options[i].display;
					}
				}
				for(; i < n-1; i++) {
					nopt[i].selected = 0;
					nopt[i].value = c.options[i+1].value;
					nopt[i].display = c.options[i+1].display;
				}
				if(found && n > 1) {
					if(sel < n-1)
						nopt[sel].selected = 1;
					else
						nopt[n-2].selected = 1;
				}
				c.options = nopt;
			}
			changed = 1;
		
		CLadd =>
			n := len c.options;
			if(n >= 100)
				return;
			nopt := array[n+1] of B->Option;
			for(i := 0; i < n; i++) {
				nopt[i].selected = 0;
				nopt[i].value = c.options[i].value;
				nopt[i].display = c.options[i].display;
			}
			nopt[n].selected = 1;
			nopt[n].value = value;
			nopt[n].display = display;
			c.options = nopt;
			if(n+1 <= c.nvis)
				c.first = 0;
			else
				c.first = n + 1 - c.nvis;
			c.maxcol = max(c.maxcol, L->stringwidth(display));
			changed = 1;
		}
	}
	if(changed)
		sc.draw(1);
}

swapopt(a: array of B->Option, i, j: int)
{
	selected := a[i].selected;
	value := a[i].value;
	display := a[i].display;
	
	a[i].selected = a[j].selected;
	a[i].value = a[j].value;
	a[i].display = a[j].display;

	a[j].selected = selected;
	a[j].value = value;
	a[j].display = display;
}

parsebookmarks(b: array of byte): array of B->Option
{
	opts := array [100] of B->Option; # allow maximum user defines

	LookForBeginA, 
	LookForEndA : con iota;

	nopt := 0;
	state := LookForBeginA;
	begin : int = 0;
	for(i := 0; i < len b; i++) {
		case state {
		LookForBeginA => # look for pattern <A
			if(b[i] == byte '<') {
				if(len b == i+1)
					break; 
				if(b[i+1] == byte 'A') {
					i++;
					begin = i + 1;
					state = LookForEndA;
				}
			}
		LookForEndA => # look for pattern </A>
			if(b[i] == byte '<') {
				if(len b <= i+3)
					break;
				if(b[i+1] == byte '/' && b[i+2] == byte 'A' && b[i+3] == byte '>') {
					# break into value and display
					(value, display) := S->splitl(string b[begin:i], ">");
					opts[nopt].selected = 0;
					opts[nopt].value = value;
					opts[nopt++].display = display[1:];

					state = LookForBeginA;
				}
			}
		}
	}

	return opts[0:nopt];
}

savebookmarks(options: array of B->Option)
{
	fname := config.userdir + "/bookmarks.html";
	fd := sys->create(fname, sys->OWRITE, 8r600);
	if(fd == nil) {
		#if(warn)
			sys->print("can't create user bookmarks file\n");
		return;
	}
	buf := array [Sys->ATOMICIO] of byte;
	line := "<HEAD> <TITLE>User Bookmarks</TITLE></HEAD>\n<BODY>\n";
	aline := array of byte line;
	buf[0:] = aline;
	bufpos := len aline;
	for(i := 0; i < len options; i++) {
		line = "<A" + options[i].value + ">" + options[i].display + "</A><BR>\n";
		aline = array of byte line;
		if(bufpos + len aline > Sys->ATOMICIO) {
			sys->write(fd, buf, bufpos);
			bufpos = 0;
		}
		buf[bufpos:] = aline;
		bufpos += len aline;
	} 
	if(bufpos > 0)
		sys->write(fd, buf, bufpos);
}

# If key event results in command to navigate somewhere else,
# return a GoSpec ref, else nil.
handlekey(e: ref Event.Ekey) : ref GoSpec
{
	loc := keyfocus;
	if(dbg > 1)
		loc.print("key focus loc");
	f := loc.lastframe();
	n1 := loc.n-1;
	case loc.le[n1].kind {
	L->LEcontrol =>
		c := loc.le[n1].control;
		pick ce := c {
		Centry =>
			case c.dokey(e.keychar) {
			L->CAreturnkey =>
				if(c == ctllay.entry) {
					s := ce.s;
					if(s != "") {
						u := CU->makeabsurl(s);
						return GoSpec.newget(GoNormal, u, "_top");
					}
				}
				else if(popupactive) {
					finishpopup(1 );
					return nil;
				}
				else if(c.ff != nil) {
					spawn form_submit(c.f, c.ff.form, zp, c, 1);
					return nil;
				}
			L->CAtabkey =>
				if(popupactive)
					popuprefocus();
			}
		}
	}
	return nil;
}

# focus only to entry boxes
popuprefocus()
{
	focus : list of ref Control;
	cs := popuplay.controls;

	case popuplay.kind {
	PopupNone =>
		;
	PopupAuth =>
		focus = hd cs :: hd tl cs :: nil;
	PopupSaveAs =>
		;
	PopupAlert =>
		;
	PopupConfirm =>
		;
	PopupPrompt =>
		focus = hd cs :: nil;
	PopupEditBookmarks =>
		cs = tl cs;
		focus =	hd cs :: hd tl cs :: nil; 
	PopupConfig =>
		focus = hd cs :: hd tl cs :: hd tl tl cs :: hd tl tl tl cs :: nil;
	}

	cs = focus;
	c : ref Control;
	while(cs != nil) { # remove all focus
		c = hd cs;
		cs = tl cs;
		if(c.flags & L->CFhasfocus) {
			c.losefocus(popupactive);
			break;
		}	
	}

	if(cs == nil)
		c = hd focus;
	else
		c = hd cs;
	c.gainfocus(popupactive);
	keyfocus = frameloc(c, popupframe);
}

fileexist(file: string) :int
{
		fd := sys->open(file, sys->OREAD);
		if (fd == nil)
			return 0;
		else
			return 1;
}

go(g: ref GoSpec)
{
	gopgrp = sys->pctl(sys->NEWPGRP, nil);
	spawn goproc(g);

	# got to make netget the thread with the gopgrp thread,
	# since it runs until killed, and killing a pgrp needs an active
	# thread
	CU->netget();
}

goproc(g: ref GoSpec)
{
	origkind := g.kind;
	hn : ref HistNode = nil;
	case origkind {
	GoNormal or
	GoReplace =>
		;
	GoHistnode =>
		hn = g.histnode;
		if(hn == nil)
			return;
		g = hn.topconfig.gospec;
	GoBookmarks =>
		url : ref Url->ParsedUrl;
		if ((config.userdir == "/usr/inferno/charon") && fileexist(config.userdir + "/bookmarks.html") && fileexist(config.custbkurl))
			url = U->makeurl("file:" + config.dualbkurl);
		else if ((config.userdir == "/usr/inferno/charon") && fileexist(config.custbkurl))
			url = U->makeurl("file:" + config.custbkurl);
		else
			url = U->makeurl("file:" + config.userdir + "/bookmarks.html");
		g = GoSpec.newget(GoNormal, url, "_top");
	GoHome =>
		g = GoSpec.newget(GoNormal, config.homeurl, "_top");
	GoHelp =>
		g = GoSpec.newget(GoNormal, config.helpurl, "_top");
	GoHistory =>
		url := U->makeurl("file:" + config.userdir + "/history.html");
		dumphistory();
		g = GoSpec.newget(GoNormal, url, "_top");
	}
	case g.target {
	"_top" =>
		curframe = top;
	"_self" =>
		; # curframe is already OK
	"_parent" =>
		if(curframe.parent != nil)
			curframe = curframe.parent;
	"_blank" =>
		curframe = top; # we don't create new browsers...
	* =>
		# this is recommended "current practice"
		curframe = findnamedframe(curframe, g.target);
		if(curframe == nil) {
			curframe = findnamedframe(top, g.target);
			if(curframe == nil)
				curframe = top;
		}
	}
	f := curframe;
	if(dbg) {
		sys->print("\n\nGO TO %s\n", g.url.tostring());
		if(g.target != "_top")
			sys->print("target frame name=%s\n", f.name);
	}
	if(g.url.frag != "" && (origkind == GoNormal || origkind == GoReplace || origkind == GoLink)
			&& f.doc != nil && f.doc.src != nil && CU->urlequal(g.url, f.doc.src)) {
		go_local(f, g.url.frag);
		return;
	}
	if((CU->config).showprogress)
		CU->progresschan <-= (-1, 0, 0, "");
	ctllay.stopbut.enable();
	get(g, f, origkind, hn);
	ctllay.stopbut.disable();

	if((err := getmainstatus()) != nil) {
		showstatus(err);
	} else {
		finalstatus := charon_gui->iDone;
		if(doscripts && J->defaultStatus != "")
			finalstatus = J->defaultStatus;
		showstatus(finalstatus);
	}
	checkrefresh(f);
}

getmainstatus(): string
{
	# return any error message about main url get
	pick pb := proglay.box[0] {
	Cprogbox =>
		if(pb.state == CU->Perr)
			return pb.err;
	}
	return nil;
}

get(g: ref GoSpec, f: ref Frame, origkind: int, hn: ref HistNode)
{
	curres, newres: ResourceState;
	if(dbgres) {
		(CU->imcache).clear();
		curres = ResourceState.cur();
	}
	sdest := g.url.tostring();
        showstatus(charon_gui->iFetchStatus + sdest);
	bsmain : ref ByteSource;
	hdr : ref Header;
	ri := ref ReqInfo(g.url, g.meth, array of byte g.body, g.auth, g.target);
	authtried := 0;
	realm := "";
	auth := "";
	for(nredirs := 0; ; nredirs++) {
		bsmain = CU->startreq(ri);
		if(bsmain.err != "") {
			showstatus(bsmain.err);
			CU->freebs(bsmain);
			return;
		}
		bs := CU->waitreq();
		if(bs.refgo == 0)
			continue;
		CU->assert(bs == bsmain);
		if(bsmain.err != "") {
			showstatus(bsmain.err);
			CU->freebs(bsmain);
			return;
		}
		hdr = bsmain.hdr;
		(use, error, challenge, newurl) := CU->hdraction(bsmain, 1, nredirs);
		if(challenge != nil) {
			if(authtried) {
				# we already tried once; give up
				error = "Need authorization";
				use = 1;
			}
			else {
				(realm, auth) = getauth(challenge);
				if(auth != "") {
					ri.auth = auth;
					authtried = 1;
					CU->freebs(bsmain);
					continue;
				}
				else {
					error = "Need authorization";
					use = 1;
				}
			}
		}
		if(error != "")
			showstatus(error);
		else {
			showstatus(CU->hcphrase(hdr.code));
			if(authtried) {
				# it succeeded; add to auths list so don't have to ask again
				auths = ref AuthInfo(realm, auth) :: auths;
			}
		}
		if(newurl != nil) {
			ri.url = newurl;
			# some sites (e.g., amazon.com) assume that POST turns into
			# GET on redirect (maybe this is just http 1.0?)
			ri.method = CU->HGet;
			CU->freebs(bsmain);
			continue;
		}
		if(use == 0) {
			CU->freebs(bsmain);
			return;
		}
		break;
	}
	if(dbgres > 1) {
		newres = ResourceState.cur();
		newres.since(curres).print("resources to get header");
		curres = newres;
	}
	if(hdr.length > 0 && (hdr.mtype == CU->TextHtml || hdr.mtype == CU->TextPlain ||
					I->supported(hdr.mtype))) {
		showurl(sdest);
		history.add(f, g, origkind);
		resetkeyfocus(f);
		srcdata := L->layout(f, bsmain, origkind == GoLink);
		if(cci != nil) {
			vurl := sdest;
			if(origkind == GoHistnode) {
				if(hn == history.find(0))
					vurl = "RELOAD";
				else if(hn == history.find(1))
					vurl = "FORWARD";
				else if(hn == history.find(-1))
					vurl = "BACK";
			}
			cci->view(vurl, CU->mnames[hdr.mtype], srcdata);
		}
		history.update(f);
		if(dbgres > 1) {
			newres = ResourceState.cur();
			newres.since(curres).print("resources to get page and do layout");
			curres = newres;
		}
		if(f.kids != nil) {
			if(J != nil)
				J->frametreechanged(top);
			i := 0;
			for(kl := f.kids; kl != nil; kl = tl kl) {
				k := hd kl;
				if(k.src != nil) {
					if(hn != nil)
						gs := hn.kidconfigs[i].gospec;
					else
						gs = GoSpec.newget(GoNormal, k.src, "_self");
					if(dbg)
						sys->print("get child frame %s\n", gs.url.tostring());
					if(origkind == GoLink)
						gokind := GoLink;
					else
						gokind = GoNormal;
					get(gs, k, gokind, nil);
					checkrefresh(k);
				}
				i++;
			}
		}

		# at this point all sub-frames and images have been loaded
		if (doscripts && f.doc.hasscripts) {
			J->jevchan <-= ref E->ScriptEvent(E->SEonload, f.id, -1, -1, -1, -1, -1, -1, -1, nil);
			for(itl := f.doc.images; itl != nil; itl = tl itl) {
				it := hd itl;
				if(it.genattr == nil)
					continue;
				ev := -1;
				pick im := it {
				Iimage =>
					case im.ci.complete {
					# correct to equate these two ?
					Img->Mimnone or
					Img->Mimerror =>
						ev = E->SEonerror;
					Img->Mimdone =>
						ev = E->SEonload;
					}
					if(ev != -1)
						J->jevchan <-= ref E->ScriptEvent(ev, f.id, -1, -1, -1, im.imageid, -1, -1, -1, nil);
				}
			}
		}

		if(g.url.frag != "")
			go_local(f, g.url.frag);
	}
	else {
		if(hdr.length == 0)
                        showstatus(charon_gui->iEmptyPage);
		else
                        showstatus(charon_gui->iUnsuppMedia + CU->mnames[hdr.mtype]);
		# Eventually put a save-as dialog up here.
		# For now, sleep a bit so use sees error message before overwritten by "Done"
		sys->sleep(3000);
		CU->freebs(bsmain);
	}
	if(dbgres == 1) {
		newres = ResourceState.cur();
		newres.since(curres).print("resources to do page");
		curres = newres;
	}
}

# Scroll frame f so that destination hyperlink loc is at top of view
go_local(f: ref Frame, loc: string)
{
	if(dbg)
		sys->print("go to local destination %s\n", loc);
	for(ld := f.doc.dests; ld != nil; ld = tl ld) {
		d := hd ld;
		if(d.name == loc) {
			dloc := f.find(zp, d.item);
			if(dloc == nil) {
				if(warn)
					sys->print("couldn't find item for destination anchor %s\n", loc);
				return;
			}
			p := f.sptolp(dloc.le[dloc.n-1].pos);
			f.yscroll(L->CAscrollabs, p.y);
			return;
		}
	}
	if(warn)
		sys->print("couldn't find destination anchor %s\n", loc);
}

# If refresh has been set in f (i.e., client pull),
# pause the appropriate amount of time and then go to new place
checkrefresh(f: ref Frame)
{
	if(f.doc != nil && f.doc.refresh != "") {
		seconds := 0;
		url : ref ParsedUrl = nil;
		(n, l) := sys->tokenize(f.doc.refresh, "; ");
		if(n > 0) {
			seconds = int hd l;
			if(n > 1) {
				s := hd tl l;
				if(len s > 4 && S->tolower(s[0:4]) == "url=") {
					url = U->makeurl(s[4:]);
					url.makeabsolute(f.doc.base);
				}
			}
		}
		spawn dorefresh(f, seconds, url);
	}
}

dorefresh(f: ref Frame, seconds: int, url: ref ParsedUrl)
{
	sys->sleep(seconds * 1000);
	e : ref Event;
	if(url == nil)
		e = ref Event.Ego(nil, f.name, 0, E->EGreload);
	else
		e = ref Event.Ego(url.tostring(), f.name, 0, E->EGnormal);
	E->evchan <-= e;
}

# Do depth first search from f, looking for frame with given name.
findnamedframe(f: ref Frame, name: string) : ref Frame
{
	if(f.name == name)
		return f;
	for(l := f.kids; l != nil; l = tl l) {
		k := hd l;
		a := findnamedframe(k, name);
		if(a != nil)
			return a;
	}
	return nil;
}

# Similar, but look for frame id, starting from f
findframe(f: ref Frame, id: int) : ref Frame
{
	if(f.id == id)
		return f;
	for(l := f.kids; l != nil; l = tl l) {
		k := hd l;
		a := findframe(k, id);
		if(a != nil)
			return a;
	}
	return nil;
}

# Return Gospec resulting from button up in anchor a, at offset pos inside item it.
anchorgospec(it: ref Item, a: ref B->Anchor, p: Point) : ref GoSpec
{
	g : ref GoSpec;
	u := a.href;
	target := a.target;
	pick i := it {
	Iimage =>
		ci := i.ci;
		if(ci.mims != nil) {
			if(i.map != nil) {
				(u, target) = findhit(i.map, p, ci.width, ci.height);
			}
			else if(u != nil && (it.state&B->IFsmap)) {
				# copy u, add ?x,y
				x := min(max(p.x-(int i.hspace + int i.border),0),ci.width-1);
				y := min(max(p.y-(int i.vspace + int i.border),0),ci.height-1);
				u = ref *a.href;
				u.query = string x + "," + string y;
			}
		}
	Ifloat =>
		return anchorgospec(i.item, a, p);
	}

	if(u != nil)
		g = GoSpec.newget(GoLink, u, target);
	return g;
}

# Control c has been pushed.
# Find the form it is in and perform required action (reset, or submit).
pushaction(c: ref Control, loc: ref Loc)
{
	pick b := c {
	Cbutton =>
		ff := b.ff;
		f := b.f;
		if(ff != nil) {
			case ff.ftype {
			B->Fsubmit or B->Fimage =>
				spawn form_submit(c.f, ff.form, loc.pos, c, 1);
			B->Freset =>
				spawn form_reset(f, ff.form);
			}
		}
	}
}

# does Form frm have event handler eh?
has_handler(frm: ref B->Form, eh: int) : int
{
	for(l := frm.events; l != nil; l = tl l) {
		a := hd l;
		if(a.attid == eh)
			return 1;
	}
	return 0;
}

# if onsubmit==1, then raise onsubmit event (if handler present)
form_submit(fr: ref Frame, frm: ref B->Form, p: Point, submitctl: ref Control, onsubmit: int)
{
	if(submitctl != nil && tagof(submitctl) == tagof(Control.Centry)) {
		# Via CR, so only submit if there is only one visible control in the form
		nnonhidden := 0;
		nsubmits := 0;
		for(l := frm.fields; l != nil; l = tl l) {
			f := hd l;
			case f.ftype {
			B->Fhidden or
			B->Freset =>
				;
			B->Fsubmit or
			B->Fimage =>
				nsubmits++;
			* =>
				nnonhidden++;
			}
		}
		if(nnonhidden > 1 || nsubmits > 1)
			return;
	}
	if(doscripts && fr.doc.hasscripts && onsubmit && has_handler(frm, Lex->Aonsubmit)) {
		c := chan of int;
		J->jevchan <-= ref E->ScriptEvent(E->SEonsubmit, fr.id, frm.formid, -1, -1, -1, -1, -1, -1, c);
		if(<-c == 0)
			return;
	}
	v := "";
	sep := "";
	radiodone : list of string = nil;
floop:
	for(l := frm.fields; l != nil; l = tl l) {
		f := hd l;
		if(f.name == "")
			continue;
		val := "";
		c: ref Control;
		if(f.ctlid >= 0)
			c = fr.controls[f.ctlid];
		case f.ftype {
			B->Ftext or B->Fpassword or B->Ftextarea =>
				if(c != nil)
					pick e := c {
					Centry =>
						val=charon_code->getRightcode(e.s);
						#val = e.s;
					}
				if(val != "" && f.name == "_ISINDEX_") {
					# just the index terms after the "?"
					if(sep != "") {
						v = v + sep;
						sep = "&";
					}
					v = v + ucvt(val);
					break floop;
				}
			B->Fcheckbox or B->Fradio =>
				if(f.ftype == B->Fradio) {
					# Need the following to catch case where there
					# is more than one radiobutton with the same name
					# and value.
					for(rl := radiodone; rl != nil; rl = tl rl)
						if(hd rl == f.name)
							continue floop;
				}
				checked := 0;
				if(c != nil)
					pick cb := c {
					Ccheckbox =>
						checked = cb.flags & L->CFactive;
					}
				if(checked) {
					val = f.value;
					if(f.ftype == B->Fradio)
						radiodone = f.name :: radiodone;
				}
				else
					continue;
			B->Fhidden =>
				val = f.value;
			B->Fsubmit =>
				if(submitctl != nil && f == submitctl.ff && f.name != "_no_name_submit_")
					val = f.value;
				else
					continue;
			B->Fselect =>
				if(c != nil)
					pick s := c {
					Cselect =>
						for(i := 0; i < len s.options; i++) {
							if(s.options[i].selected) {
								if(sep != "")
									v = v + sep;
								sep = "&";
								v = v + ucvt(f.name) + "=" + ucvt(s.options[i].value);
							}
						}
						continue;
					}
			B->Fimage =>
				if(submitctl != nil && f == submitctl.ff) {
					if(sep != "")
						v = v + sep;
					sep = "&";
					v = v + ucvt(f.name + ".x") + "=" + ucvt(string max(p.x,0))
						+ sep + ucvt(f.name + ".y") + "=" + ucvt(string max(p.y,0));
					continue;
				}
		}
		if(val != "") {
			if(sep != "")
				v = v + sep;
			sep = "&";
			v = v + ucvt(f.name) + "=" + ucvt(val);
		}
	}
	action := ref *frm.action;
	action.query = v;
	E->evchan <-= ref Event.Esubmit(frm.method, action, frm.target);
}

ucvt(s: string): string
{
	u := "";
	for(i := 0; i < len s; i++) {
		c := s[i];
		if(S->in(c, "-/$_@.!*'(),a-zA-Z0-9"))
			u[len u] = c;
		else if(c == ' ')
			u[len u] = '+';
		else {
			if((c &16rff00) >0)
				{
				d:=(c & 16rff00)>>8;
				u[len u]='%';
				u[len u]=hexdigit((d>>4)&15);
				u[len u]=hexdigit(d&15);
				c=c & 16rff;
				}
			u[len u] = '%';
			u[len u] = hexdigit((c>>4)&15);
			u[len u] = hexdigit(c&15);
		}
	}
	return u;
}

hexdigit(v: int): int
{
	if(0 <= v && v <= 9)
		return '0' + v;
	else
		return 'A' + v - 10;
}

form_reset(fr: ref Frame, frm: ref B->Form)
{
	if(doscripts && fr.doc.hasscripts && has_handler(frm, Lex->Aonreset)) {
		c := chan of int;
		J->jevchan <-= ref E->ScriptEvent(E->SEonreset, fr.id, frm.formid, -1, -1, -1, -1, -1, -1, c);
		if(<-c == 0)
			return;
	}
	for(fl := frm.fields; fl != nil; fl = tl fl) {
		a := hd fl;
		if(a.ctlid >= 0)
			fr.controls[a.ctlid].reset();
	}
	fr.cim.flush(D->Flushnow);
}

formaction(frameid, formid, ftype, onsubmit: int)
{
	if(dbg > 1)
		sys->print("formaction %d %d %d %d\n", frameid, formid, ftype, onsubmit);
	f := findframe(top, frameid);
	if(f != nil) {
		d := f.doc;
		if(d != nil) {
			for(fl := d.forms; fl != nil; fl = tl fl) {
				frm := hd fl;
				if(frm.formid == formid) {
					if(ftype == E->EFsubmit)
						spawn form_submit(f, frm, Point(0,0), nil, onsubmit);
					else
						spawn form_reset(f, frm);
				}
			}
		}
	}
}

formfield_blur(f: ref Frame, ff: ref B->Formfield)
{
	if(ff.ftype != B->Fhidden) {
		c := f.controls[ff.ctlid];
		if(!(c.flags & L->CFhasfocus))
			return;
		c.losefocus(popupactive);
		keyfocus = frameloc(ctllay.entry, ctlframe);
		ctllay.entry.gainfocus(popupactive);
	}
}

formfield_focus(f: ref Frame, ff: ref B->Formfield)
{
	if(ff.ftype != B->Fhidden) {
		c := f.controls[ff.ctlid];
		if(c.flags & L->CFhasfocus)
			return;
		oldc := keyfocus.le[keyfocus.n-1].control;
		if(oldc != nil)
			oldc.losefocus(popupactive);
		keyfocus = frameloc(c, f);
		c.gainfocus(popupactive);
	}
}

# simulate a mouse click, but don't trigger onclick event
formfield_click(f: ref Frame, frm: ref B->Form, ff: ref B->Formfield)
{
	c := f.controls[ff.ctlid];
	case ff.ftype {
	B->Fcheckbox or
	B->Fradio or
	B->Fbutton =>
		c.domouse(zp, E->Mlbuttonup, nil);
	B->Fsubmit =>
		spawn form_submit(f, frm, zp, nil, 1);
	B->Freset =>
		spawn form_reset(f, frm);
	}
}

formfield_select(f: ref Frame, ff: ref B->Formfield)
{
	case ff.ftype {
	B->Ftext or
	B->Fselect or
	B->Ftextarea =>
		ctl := f.controls[ff.ctlid];
		pick c := ctl {
		Centry =>
			c.sel = (0, len c.s);
			ctl.draw(1);
		}
	}
}

formfieldaction(frameid, formid, fieldid, fftype: int)
{
	if(dbg > 1)
		sys->print("formfieldaction %d %d %d %d\n", frameid, formid, fieldid, fftype);
	f := findframe(top, frameid);
	if(f == nil || f.doc == nil)
		return;

	# find form in frame
	frm : ref B->Form;
	for(fl := f.doc.forms; fl != nil; fl = tl fl) {
		if((hd fl).formid == formid) {
			frm = hd fl;
			break;
		}
	}
	if(frm == nil)
		return;

	# find formfield in form
	ff : ref B->Formfield;
	for(ffl := frm.fields; ffl != nil; ffl = tl ffl) {
		if((hd ffl).fieldid == fieldid) {
			ff = hd ffl;
			break;
		}
	}
	if(ff == nil || ff.ctlid < 0)
		return;

	# perform action
	case fftype {
	E->EFFblur =>
		formfield_blur(f, ff);
	E->EFFfocus =>
		formfield_focus(f, ff);
	E->EFFclick =>
		formfield_click(f, frm, ff);
	E->EFFselect =>
		formfield_select(f, ff);
	}
}

# Find hit in a local map
findhit(map: ref B->Map, p: Point, w, h: int) : (ref ParsedUrl, string)
{
	x := p.x;
	y := p.y;
	dflt : ref ParsedUrl = nil;
	dflttarg := "";
	for(al := map.areas; al != nil; al = tl al) {
		a := hd al;
		c := a.coords;
		nc := len c;
		x1 := 0;
		y1 := 0;
		x2 := 0;
		y2 := 0;
		if(nc >= 2) {
			x1 = d2pix(c[0], w);
			y1= d2pix(c[1], h);
			if(nc > 2) {
				x2 = d2pix(c[2], w);
				if(nc > 3)
					y2 = d2pix(c[3], h);
			}
		}
		hit := 0;
		case a.shape {
		"rect" or "rectangle" =>
			if(nc == 4)
				hit = x1 <= x && x <= x2 &&
					y1 <= y && y <= y2;
		"circ" or "circle" =>
			if(nc == 3) {
				xd := x - x1;
				yd := y - y1;
				hit = xd*xd + yd*yd <= x2*x2;
			}
		"poly" or "polygon" =>
			np := nc / 2;
			hit = 0;
			xr := real x;
			yr := real y;
			j := np - 1;
			for(i := 0; i < np; j = i++) {
				xi := real d2pix(c[2*i], w);
				yi := real d2pix(c[2*i+1], h);
				xj := real d2pix(c[2*j], w);
				yj := real d2pix(c[2*j+1], h);
				if ((((yi<=yr) && (yr<yj)) ||
				     ((yj<=yr) && (yr<yi))) &&
				    (xr < (xj - xi) * (yr - yi) / (yj - yi) + xi))
					hit = !hit;
			}
		"def" or "default" =>
			dflt = a.href;
			dflttarg = a.target;
		}
		if(hit)
			return (a.href, a.target);
	}
	return (dflt, dflttarg);
}

d2pix(d: B->Dimen, tot: int) : int
{
	ans := d.spec();
	if(d.kind() == B->Dpercent)
		ans = (ans * tot) / 100;
	return ans;
}
GoSpec.newget(kind: int, url: ref ParsedUrl, target: string) : ref GoSpec
{
	return ref GoSpec(kind, url, CU->HGet, "", target, "", nil);
}

GoSpec.newpost(url: ref ParsedUrl, body, target: string) : ref GoSpec
{
	return ref GoSpec(GoNormal, url, CU->HPost, body, target, "", nil);
}

GoSpec.newspecial(kind: int, hn: ref HistNode) : ref GoSpec
{
	return ref GoSpec(kind, nil, 0, "", "", "", hn);
}

GoSpec.equal(a: self ref GoSpec, b: ref GoSpec) : int
{
	if(a.url == nil || b.url == nil)
		return 0;
	return CU->urlequal(a.url, b.url) && a.meth == b.meth && a.body == b.body;
}

DocConfig.equal(a: self ref DocConfig, b: ref DocConfig) : int
{
	return a.framename == b.framename && a.gospec.equal(b.gospec);
}

DocConfig.equalarray(a1: array of ref DocConfig, a2: array of ref DocConfig) : int
{
	n := len a1;
	if(n != len a2)
		return 0;
	for(i := 0; i < n; i++) {
		if(a1[i] == nil || a2[i] == nil)
			continue;
		if(!(a1[i]).equal(a2[i]))
			return 0;
	}
	return 1;
}

# Put b in a.succs (if atob is true) or a.preds (if atob is false)
# at front of list.
# If it is already in the list, move it to the front.
HistNode.addedge(a: self ref HistNode, b: ref HistNode, atob: int)
{
	if(atob)
		oldl := a.succs;
	else
		oldl = a.preds;
	there := 0;
	for(l := oldl; l != nil; l = tl l)
		if(hd l == b) {
			there = 1;
			break;
		}
	if(there)
		newl := b :: remhnode(oldl, b);
	else
		newl = b :: oldl;
	if(atob)
		a.succs = newl;
	else
		a.preds = newl;
}

# return copy of l with hn removed (known that hn
# occurs at most once)
remhnode(l: list of ref HistNode, hn: ref HistNode) : list of ref HistNode
{
	if(l == nil)
		return nil;
	hdl := hd l;
	if(hdl == hn)
		return tl l;
	return hdl :: remhnode(tl l, hn);
}

# Copy of a, with new kidconfigs array (so that it can be changed independent
# of a), and clear the preds and succs.
HistNode.copy(a: self ref HistNode) : ref HistNode
{
	n := len a.kidconfigs;
	kc : array of ref DocConfig = nil;
	if(n > 0) {
		kc = array[n] of ref DocConfig;
		for(i := 0; i < n; i++)
			kc[i] = a.kidconfigs[i];
	}
	return ref HistNode(a.topconfig, kc, nil, nil);
}

# This is called just before layout of f with result of getting g.
# (we don't yet know doctitle and whether this is a frameset).
# If navkind is not GoHistnode, update the history graph; but if
# navkind is GoReplace, replace oldcur with the new HistNode.
# In any case reorder the history array to put latest last in array.
History.add(h: self ref History, f: ref Frame, g: ref GoSpec, navkind: int)
{
	if(len h.h <= h.n) {
		newh := array[len h.h + 20] of ref HistNode;
		newh[0:] = h.h;
		h.h = newh;
	}
	oldcur : ref HistNode;
	if(h.n > 0)
		oldcur = h.h[h.n-1];
	dc := ref DocConfig(f.name, g.url.tostring(), navkind != GoHistnode, g);
	hnode := ref HistNode(dc, nil, nil, nil);
	if(f == top) {
		g.target = "_top";
	}
	else if(oldcur != nil) {
		# oldcur should be a frameset and f should be a kid in it
		kidpos := -1;
		for(i := 0; i < len oldcur.kidconfigs; i++) {
			kc := oldcur.kidconfigs[i];
			if(kc != nil && kc.framename == f.name) {
				kidpos = i;
				break;
			}
		}
		if(kidpos == -1) {
			if(dbg)
				sys->print("history botch\n");
		}
		else {
			hnode = oldcur.copy();
			hnode.kidconfigs[kidpos] = dc;
		}
	}
	# see if equivalent node to hnode is already in history
	hnodepos := -1;
	for(i := 0; i < h.n; i++) {
		if(hnode.topconfig.equal(h.h[i].topconfig)) {
			if((hnode.kidconfigs==nil && h.h[i].topconfig.initconfig) ||
			   DocConfig.equalarray(hnode.kidconfigs, h.h[i].kidconfigs)) {
				hnodepos = i;
				hnode = h.h[i];
				break;
			}
		}
	}
	if(hnodepos == -1) {
		if(navkind == GoReplace && h.n > 0)
			h.n--;
		hnodepos = h.n;
		h.h[h.n++] = hnode;
	}
	if(oldcur != nil && hnode != oldcur && navkind != GoHistnode) {
		oldcur.addedge(hnode, 1);
		if(navkind != GoReplace)
			hnode.addedge(oldcur, 0);
		else if(oldcur.preds != nil)
			hnode.addedge(hd oldcur.preds, 0);
	}
	if(hnodepos != h.n-1) {
		# move hnode to h.n-1, and shift rest back
		for(k := hnodepos; k < h.n-1; k++)
			h.h[k] = h.h[k+1];
		h.h[h.n-1] = hnode;
	}
	if(hnode.preds != nil)
		ctllay.backbut.enable();
	else
		ctllay.backbut.disable();
	if(hnode.succs != nil)
		ctllay.fwdbut.enable();
	else
		ctllay.fwdbut.disable();
}

# This is called just after layout of f.
# Now we can put in correct doctitle, and make kids array if necessary.
History.update(h: self ref History, f: ref Frame)
{
	hnode := h.h[h.n-1];
	if(f == top) {
		hnode.topconfig.title = f.doc.doctitle;
		if(f.kids != nil && hnode.kidconfigs == nil) {
			kc := array[len f.kids] of ref DocConfig;
			i := 0;
			for(l := f.kids; l != nil; l = tl l) {
				kf := hd l;
				if(kf.src != nil)
					kc[i] = ref DocConfig(kf.name, kf.src.tostring(), 1,  GoSpec.newget(GoNormal, kf.src, "_self"));
				i++;
			}
			hnode.kidconfigs = kc;
		}
	}
	else {
		# hnode should be a frameset and f should be a kid in it
		for(i := 0; i < len hnode.kidconfigs; i++) {
			kc := hnode.kidconfigs[i];
			if(kc != nil && kc.framename == f.name) {
				hnode.kidconfigs[i].title = f.doc.doctitle;
				return;
			}
		}
		if(dbg)
			sys->print("history update botch\n");
	}
}

# Find the gokind node (-1==Back, 0==Same, +1==Forward)
# other gokind values come from JavaScript's History.go(delta)
History.find(h: self ref History, gokind: int) : ref HistNode
{
	if(h.n > 0) {
		cur := h.h[h.n-1];
		case gokind {
		1 =>
			if(cur.succs != nil)
				return hd cur.succs;
		-1 =>
			if(cur.preds != nil)
				return hd cur.preds;
		0 =>
			return cur;
		* =>
			hn : list of ref HistNode;
			if(gokind > 0)
				hn = cur.succs;
			else {
				hn = cur.preds;
				gokind = -gokind;
			}
			while(hn != nil && gokind > 0) {
				hn = tl hn;
				gokind--;
			}
			if(hn != nil)
				return hd hn;
		}
	}
	return nil;
}

# for debugging
History.print(h: self ref History)
{
	sys->print("History\n");
	for(i := 0; i < h.n; i++) {
		hn := history.h[i];
		sys->print("Node %d:\n", i);
		dc := hn.topconfig;
		sys->print("\tframe=%s, target=%s, url=%s\n", dc.framename, dc.gospec.target, dc.gospec.url.tostring());
		if(hn.kidconfigs != nil) {
			for(j := 0; j < len hn.kidconfigs; j++) {
				dc = hn.kidconfigs[j];
				if(dc != nil)
					sys->print("\t\t%d: frame=%s, target=%s, url=%s\n",
							j, dc.framename, dc.gospec.target, dc.gospec.url.tostring());
			}
		}
		if(hn.preds != nil)
			printhnodeindices(h, "Preds", hn.preds);
		if(hn.succs != nil)
			printhnodeindices(h, "Succs", hn.succs);
	}
	sys->print("\n");
}

# helpers for JavaScript's History object
History.histinfo(h: self ref History) : (int, string, string, string)
{
	length := 0;
	current, next, previous : string;

	if(h.n > 0) {
		hn := h.h[h.n-1];
		length = len hn.succs + len hn.preds + 1;
		current = hn.topconfig.gospec.url.tostring();
		if(hn.succs != nil) {
			fwd := hd hn.succs;
			next = fwd.topconfig.gospec.url.tostring();
		}
		if(hn.preds != nil) {
			back := hd hn.preds;
			previous = back.topconfig.gospec.url.tostring();
		}
	}
	return (length, current, next, previous);
}

histinfo() : (int, string, string, string)
{
	return history.histinfo();
}

# does URL in hn contain s as a substring?
isurlsubstring(hn: ref HistNode, s: string) : int
{
	url := hn.topconfig.gospec.url.tostring();
	(l, r) := S->splitstrl(url, s);
	if(r != nil)
		return 1;
	return 0;
}

# for JavaScript's History.go(location)
# find nearest history entry whose URL contains s as a substring
# (search forward and backward from current "in parallel"?)
History.findurl(h: self ref History, s: string) : ref HistNode
{
	if(h.n > 0) {
		hn := h.h[h.n-1];
		if(isurlsubstring(hn, s))
			return hn;
		fwd := hn.succs;
		back := hn.preds;
		while(fwd != nil && back != nil) {
			if(fwd != nil) {
				if(isurlsubstring(hd fwd, s))
					return hd fwd;
				fwd = tl fwd;
			}
			if(back != nil) {
				if(isurlsubstring(hd back, s))
					return hd back;
				back = tl back;
			}
		}
	}
	return nil;
}

printhnodeindices(h: ref History, label: string, l: list of ref HistNode)
{
	sys->print("\t%s:", label);
	for( ; l != nil; l = tl l) {
		hn := hd l;
		for(i := 0; i < h.n; i++) {
			if(hn == h.h[i]) {
				sys->print(" %d", i);
				break;
			}
		}
		if(i == h.n)
			sys->print(" ?");
	}
	sys->print("\n");
}

dumphistory()
{
	fname := config.userdir + "/history.html";
	fd := sys->create(fname, sys->OWRITE, 8r600);
	if(fd == nil) {
		if(warn)
			sys->print("can't create history file\n");
		return;
	}
	line := "<HEAD> <TITLE>History</TITLE></HEAD>\n<BODY>\n";
	buf := array[Sys->ATOMICIO] of byte;
	aline := array of byte line;
	buf[0:] = aline;
	bufpos := len aline;
	for(i := history.n-1; i >= 0; i--) {
		hn := history.h[i];
		dc := hn.topconfig;
		line = "<A HREF=" + dc.gospec.url.tostring() + " TARGET=\"_top\">" + dc.title + "</A><BR>\n";
		if(hn.kidconfigs != nil) {
			line += "<UL>";
			for(j := 0; j < len hn.kidconfigs; j++) {
				dc = hn.kidconfigs[j];
				if(dc != nil) {
					line += "<LI><A HREF=" + dc.gospec.url.tostring() +
						" TARGET=\"" + dc.framename + "\">" +
						dc.title + "</A>\n";
				}
			}
			line += "</UL>";
		}
		aline = array of byte line;
		if(bufpos + len aline > Sys->ATOMICIO) {
			sys->write(fd, buf, bufpos);
			bufpos = 0;
		}
		buf[bufpos:] = aline;
		bufpos += len aline;
	}
	if(bufpos > 0)
		sys->write(fd, buf, bufpos);
}

# getauth returns the (realm, credentials), with "" for the credentials
# if we fail in getting authorization for some reason
getauth(chal: string) : (string, string)
{
	if(len chal < 12 || S->tolower(chal[0:12]) != "basic realm=") {
		if(dbg || warn)
			sys->print("unrecognized authorization challenge: %s\n", chal);
		return ("", "");
	}
	realm := chal[12:];
	if(realm[0] == '"')
		realm = realm[1:len realm - 1];
	for(al := auths; al != nil; al = tl al) {
		a := hd al;
		if(realm == a.realm)
			return (realm, a.credentials);
	}
	dopopup(PopupAuth, realm, "");
	(code, ans) := <- popupans;
	if(code == -1)
		sys->print("couldn't create popup window\n");
	else if(code == 1)
		ans = tobase64(ans);
	return (realm, ans);
}

# Convert string to the base64 encoding
tobase64(a: string) : string
{
	n := len a;
	if(n == 0)
		return "";
	out := "";
	j := 0;
	i := 0;
	while(i < n) {
		x := a[i++] << 16;
		if(i < n)
			x |= (a[i++]&255) << 8;
		if(i < n)
			x |= (a[i++]&255);
		out[j++] = c64(x>>18);
		out[j++] = c64(x>>12);
		out[j++] = c64(x>> 6);
		out[j++] = c64(x);
	}
	nmod3 := n % 3;
	if(nmod3 != 0) {
		out[j-1] = '=';
		if(nmod3 == 1)
			out[j-2] = '=';
	}
	return out;
}

c64(c: int) : int
{
	v : con "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	return v[c&63];
}

dosaveas(bsmain: ref ByteSource)
{
	dopopup(PopupSaveAs, "", "");
	(code, ans) := <- popupans;
	if(code == -1)
		sys->print("couldn't create popup window\n");
	else if(code == 1 && ans != "") {
		if(ans[0] != '/')
			ans = config.userdir + "/" + ans;
		fd := sys->create(ans, sys->OWRITE, 8r644);
		if(fd == nil) {
			dopopup(PopupAlert, "Couldn't create " + ans, "");
			(nil, nil) = <- popupans;
		}
		else {
                        showstatus(charon_gui->iSaving + bsmain.hdr.actual.tostring());
			# TODO: should really use a different protocol that
			# doesn't require getting whole file before proceeding
			err := "";
			flen := bsmain.hdr.length;
			while(bsmain.edata < flen) {
				bs := CU->waitreq();
				if(bs.refgo == 0)
					continue;
				CU->assert(bs == bsmain);
				if(bs.err != "") {
					err = bs.err;
					break;
				}
			}
			if(err== "") {
				for(i := 0; i < flen; ) {
					n := sys->write(fd, bsmain.data[i:flen], flen-i);
					if(n <= 0)
						break;
					i += n;
				}
				if(i != flen)
					err = "whole file not written";
			}
			if(err != "")
				dopopup(PopupAlert, err, "");
			else
				dopopup(PopupAlert, "Created " + ans, "");
			(nil, nil) = <- popupans;
		}
	}
	CU->freebs(bsmain);
}

# Thread to update progress bar, based on events along CU->progresschan
progressmon()
{
	i : int;
	for(;;) {
		(bsid, state, pcnt, s) := <- CU->progresschan;
		if(bsid == -1) {
			# clear the progress bar
			for(i = 0; i < proglay.nused; i++) {
				pick pb := proglay.box[i] {
				Cprogbox =>
					pb.state = CU->Punused;
					pb.pcnt = 0;
					pb.bsid = -1;
					pb.src = nil;
					pb.err = nil;
				}
			}
			proglay.nused = 0;
			#if(usetoolbar)
				tbredrawprog(0);
			#else
				#redrawprog(0);
		}
		else {
			changed := 0;
			si: ref Image;
			if(state == CU->Pstart) {
				# assign a progbox to bsid
				if(proglay.nused < len proglay.box) {
					i = proglay.nused;
					proglay.nused++;
				}
				else
					i = 0;		# wrap around
				pick pb := proglay.box[i] {
				Cprogbox =>
					pb.state = state;
					pb.bsid = bsid;
					pb.src = s;
				}
			}
			else {
				if(state == CU->Psslconnected) {
					if(proglay.sslstate != 1) {
						proglay.sslstate = 1;
						changed = 1;
						si = proglay.sslon; 
					}
					# TODO: popup a confirmation window
				}
				else if(state == CU->Pconnected) {
					if(proglay.sslstate != 0) {
						proglay.sslstate = 0;
						changed = 1;
						si = proglay.ssloff;
					}
					# TODO: check if proglay.sslstate
					# if ssl is on, popup a choice window
				}

				# find progbox assigned to bsid
			    findloop:
				for(i = 0; i < proglay.nused; i++)
					pick pb := proglay.box[i] {
					Cprogbox =>
						if(pb.bsid == bsid)
							break findloop;
					}
				if(i == proglay.nused) {
					# wrapped around
					continue;
				}
				pick pb := proglay.box[i] {
				Cprogbox =>
					if(pb.state != CU->Perr) {
						pb.state = state;
						pb.pcnt = pcnt;
						pb.err = s;
					}
				}
			}
			if(changed) {
				sw := si.r.dx();
				sh := si.r.dy();
				p := proglay.sslpos;
				progwin.draw(Rect(p,p.add(Point(sw,sh))), si, nil, zp);
			}
			proglay.box[i].draw(1);
		}
	}
}

showstatus(msg: string)
{
	ostatus := ctllay.status;
	p := ctllay.statuspos;
	if(ostatus != "" && ostatus != msg) {
		# do background over old msg
		sp := L->measurestring(ostatus);
		L->drawfill(ctlwin, Rect(p,p.add(sp)), CU->Grey);
	}
	statlen := ctlwin.r.max.x - p.x;
	mp := L->measurestring(msg);
	while(mp.x > statlen){
		# slightly long; compress middle by 3 chars each time
		# 6 chars the first time
		if(len msg < 21)
			break;
		nmsg := msg[:15] + "..." + msg[21:];
		msg = nmsg;
		mp = L->measurestring(msg);
	}
	ctllay.status = msg;
	L->drawstring(ctlwin, p, msg);
	ctlwin.flush(D->Flushnow);
}

alert(msg: string, sync: chan of int)
{
	dopopup(PopupAlert, msg, "");
	(code, nil) := <- popupans;
	if(code == -1)
		sys->print("couldn't create popup window\n");
	sync <-= 1;
}

confirm(msg: string, sync: chan of int)
{
	dopopup(PopupConfirm, msg, "");
	(code, nil) := <- popupans;
	if(code == -1) {
		sys->print("couldn't create popup window\n");
		code = 0;
	}
	sync <-= code;
}

prompt(msg, inputdflt: string, sync: chan of (int, string))
{
	dopopup(PopupPrompt, msg, inputdflt);
	(code, input) := <- popupans;
	if(code == -1) {
		sys->print("couldn't create popup window\n");
		code = 0;
	}
	sync <-= (code, input);
}

showurl(u: string)
{
	ctllay.entry.entryset(u);
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

fatalerror(msg: string)
{
	if(sys == nil)
		sys = load Sys Sys->PATH;
	sys->print("Fatal error: %s\n", msg);
	finish();
}

pctoloc(mod: string, pc: int) : string
{
	ans := sys->sprint("pc=%d", pc);
	db := load Debug Debug->PATH;
	if(db == nil)
		return ans;
	Sym : import db;
	db->init();
	modname := mod;
	for(i := 0; i < len mod; i++)
		if(mod[i] == '[') {
			modname = mod[0:i];
			break;
		}
	sblname := "";
	case modname {
	"Build" =>
		sblname = "build.sbl";
	"CharonUtils" =>
		sblname = "chutils.sbl";
	"Gui" =>
		sblname = "gui.sbl";
	"Img" =>
		sblname = "img.sbl";
	"Layout" =>
		sblname = "layout.sbl";
	"Lex" =>
		sblname = "lex.sbl";
	"Test" =>
		sblname = "test.sbl";
	}
	if(sblname == "")
		return ans;
	(sym, nil) := db->sym(sblname);
	if(sym == nil)
		return ans;
	src := sym.pctosrc(pc);
	if(src == nil)
		return ans;
	return sys->sprint("%s:%d", src.start.file, src.start.line);
}

startcs()
{
	cs := load Command "/dis/lib/cs.dis";
	spawn cs->init(nil, nil);
	sys->sleep(1000);
}

# Kill all processes spawned by us, and exit
finish()
{
	CU->kill(pgrp, 1);
	if(gopgrp != 0)
		CU->kill(gopgrp, 1);
	if(usetoolbar)
		tbw <-= "destroy";
	exit;
}
