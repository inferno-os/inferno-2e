implement Scribe;
 
include "sys.m";
	sys: Sys;
include "draw.m";
	draw: Draw;
	Display, Font, Black, Rect, Image, Point, Endsquare, Enddisc: import draw;
include "tk.m";
	tk: Tk;
	Toplevel: import tk;
include "papyrus.m";
	papyrus: Papyrus;
include "wmlib.m";
	wmlib: Wmlib;

CON_Maxnpts:	con 1000;
Maxnhits:		con 5;

d			: int = 0;	# Debug flag: 0 - off, 1 - on

 
Scribe: module
{
		init:   fn(ctxt: ref Draw->Context, argv: list of string);
};


raiseit(tk: Tk, t: ref Toplevel, c: chan of int)
{
	for(;;) alt {
	<- c =>
		return;
	* =>
		tk->cmd(t, "raise .; update");
		sys->sleep(3000);
	}
}

display: ref Display;
ones: ref Image;
t: ref Toplevel;
canvas: ref Image;
canvrect: Rect;
org: Point;
font: ref Font;
fg: ref Image;
bg: ref Image;
stderr: ref Sys->FD;
 
init(ctxt: ref Draw->Context, argv: list of string)
{
	pts:= array [CON_Maxnpts] of Papyrus->Point;

	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	papyrus = load Papyrus Papyrus->PATH;
	wmlib = load Wmlib Wmlib->PATH;

	stderr = sys->fildes(2);

	x, y, lastx, lasty: int;
 
	menubut: chan of string;
	wmlib->init();
	(t, menubut) = wmlib->titlebar(ctxt.screen, nil, "Scribe", 0);
	
	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");
	tk->cmd(t, "canvas .c -height 100 -width 100 -background red");
	tk->cmd(t, "frame .f");
	tk->cmd(t, "pack .c .f -side top -fill x");
	# tk->cmd(t, "bind .c <Key> { send cmd key %K}");
	tk->cmd(t, "bind .c <ButtonRelease-1> {send cmd b1up %x %y}");
	tk->cmd(t, "bind .c <Double-ButtonRelease-1> {send cmd b1up %x %y}");
	tk->cmd(t, "bind .c <ButtonPress-1> {send cmd b1down %x %y}");
	tk->cmd(t, "bind .c <Double-ButtonPress-1> {send cmd b1down %x %y}");
	tk->cmd(t, "bind .c <Button-1-Motion> {send cmd b1motion %x %y}");
	tk->cmd(t, "bind .c <Double-Button-1-Motion> {send cmd b1motion %x %y}");
	tk->cmd(t, "bind . <Configure> {send cmd resize}");
	tk->cmd(t, "update");
 
	# Do all Papyrus stuff here
	if (papyrus->Init(Papyrus->English, Papyrus->AsIfKeyboard) != Papyrus->StatusSuccess)
	{
		sys->fprint(stderr, "Papyrus Init failed\n");
	}
	display = ctxt.display;
	canvas = t.image;
	canvrect = canvposn(t);
	org = canvrect.min;
	ones = display.ones;
	
	font = Font.open(display, "*default*");

	npts := 0;
	WasUp := 1;

	killchan := chan of int;
	spawn raiseit(tk, t, killchan);

	bg = display.newimage(((0,0),(1,1)), t.image.ldepth, 1, 207);
	fg = display.newimage(((0,0),(1,1)), t.image.ldepth, 1, Draw->Black);

	redraw();

	for (;;) alt {
	menu := <-menubut =>
		if(len menu > 0 && menu[0] == 'e') {
			killchan <-= 1;
			return;
		}
		wmlib->titlectl(t, menu);
	
	s := <-cmd =>
		(n, cmdstr) := sys->tokenize(s, " \t\n");
		case hd cmdstr {
		"quit" =>
			exit;
		"b1up" =>
			AllowedRanges := (Papyrus->CharRangeAny | Papyrus->SyntaxWord | 
								Papyrus->SyntaxNumber | Papyrus->SyntaxInitialCap);
			pts[npts++] = (0, 0);
			if (d) sys->fprint(stderr, "npts = %d\n", npts);
			(nhits, Results) := papyrus->Recognize(pts[:npts], nil, 
											AllowedRanges, Maxnhits); 
			if (nhits > 0)
			{
				if(d) sys->fprint(stderr, "%s", string Results[0].Word[0]);
			}
			npts = 0;
			redraw();
			WasUp = 1;
		"b1down" =>
			tk->cmd(t, "raise .");
			tk->cmd(t, "update");
			redraw();
			lastx = int hd tl cmdstr;
			lasty = int hd tl tl cmdstr;
			if(d) sys->fprint(stderr, "Point [%d %d]\n", lastx, lasty);
			canvas.line((lastx+org.x, lasty+org.y),
				(lastx+org.x, lasty+org.y),
				Draw->Enddisc, Draw->Endsquare,
				1, fg, (0,0));
			WasUp = 0;
		"b1motion" =>
			if (WasUp == 1)
			{
			   # tk->cmd(t, ".c delete all; update");
			   WasUp = 0;
			}
			x = int hd tl cmdstr;
			y = int hd tl tl cmdstr;
			if(d) sys->fprint(stderr, "Point [%d %d]\n", x, y);
			if (npts >= CON_Maxnpts)
			{
				npts = 0;	# Silently clear
			}
			pts[npts++] = (x*4, y*4);
			canvas.line((lastx+org.x, lasty+org.y),
					(x+org.x, y+org.y),
					Draw->Endsquare, Draw->Endsquare,
					1, fg, (0,0));
			lastx = x; lasty = y;
		# "key" =>
		#	k := hd tl cmdstr;
		#	sys->print("k=%s\n", k);
		#	canvas.text(org, fg, (0,0), font, k);

		"resize" =>
			canvas = t.image;
			canvrect = canvposn(t);
			org = canvrect.min;
			redraw();
		}
	}
}

canvposn(t: ref Toplevel): Rect
{
	r: Rect;

	r.min.x = int tk->cmd(t, ".c cget -actx") + int tk->cmd(t, ".dx get");
	r.min.y = int tk->cmd(t, ".c cget -acty") + int tk->cmd(t, ".dy get");
	r.max.x = r.min.x + int tk->cmd(t, ".c cget -width") + int tk->cmd(t, ".dw get");
	r.max.y = r.min.y + int tk->cmd(t, ".c cget -height") + int tk->cmd(t, ".dh get");

	return r;
}


redraw()
{
	canvas.draw(canvrect, bg, ones, (0, 0));

	astate := ref Papyrus->AllegroState;

	s := papyrus->GetAllegroState(astate);
	# sys->fprint(stderr, "as: %ux %ux %ux %ux\n", s, astate.Shift,
	#			astate.Range, astate.ComposeLetter);
	str := string array[11] of {* => byte ' '};
	if(astate.Shift & Papyrus->ASShift) str[0] = '^';
	if(astate.Shift & Papyrus->ASCapsLock) str[2] = 'C';
	if(astate.Shift & Papyrus->ASSymbolShift) str[4] = 's';
	if(astate.Shift & Papyrus->ASMacro) str[6] = 'm';
	if(astate.Shift & Papyrus->ASCompose) str[8] = 'c';
	if(astate.Shift & Papyrus->ASSpecial) str[10] = 'S';
	canvas.text(org, fg, (0,0), font, str);
	str = string array[12] of {* => byte ' '};
	if(astate.Range & Papyrus->CharRangeLowerCase) str[0] = 'l';
	if(astate.Range & Papyrus->CharRangeUpperCase) str[2] = 'u';
	if(astate.Range & Papyrus->CharRangeDigit) str[4] = 'd';
	if(astate.Range & Papyrus->CharRangePunc) str[6] = 'p';
	if(astate.Range & Papyrus->CharRangeAposHyphen) str[8] = 'a';
	if(astate.Range & Papyrus->CharRangeMath) str[9] = 'm';
	if(astate.Range & Papyrus->CharRangeCurrency) str[10] = 'c';
	if(astate.Range & Papyrus->CharRangeSpecial) str[11] = 's';
	canvas.text((canvrect.min.x, canvrect.max.y-font.height), fg, (0,0), font, str);
}

