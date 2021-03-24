#
# winctl.b
#
implement WmWinctl;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Display, Image: import draw;

include "tk.m";
	tk: Tk;

include "wmlib.m";
	wmlib: Wmlib;

include "version.m";

WmWinctl: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

win_cfg := array[] of {
	"frame .mouse",
	"bind .mouse <Button> { send mouse %X,%Y }",
	"button .b.raise -text Raise -command {send cmd raise}",
	"button .b.lower -text Lower -command {send cmd lower}",
	"button .b.del   -text Delete -command {send cmd del}",
	"button .b.move	 -text Move -command {send cmd move}",
	"button .b.hide	 -text Hide -command {send cmd hide}",
	"frame .b",
	"pack .b.raise .b.lower .b.del .b.move .b.hide -fill x",
	"pack .b -fill x",
	"pack propagate . 0",
	"update",
};

init(ctxt: ref Draw->Context, argv: list of string)
{
	sys  = load Sys  Sys->PATH;
	draw = load Draw Draw->PATH;
	tk   = load Tk   Tk->PATH;
	wmlib= load Wmlib Wmlib->PATH;

	wmlib->init();

	tkargs := "";
	argv = tl argv;
	if(argv != nil) {
		tkargs = hd argv;
		argv = tl argv;
	}
	(t, menubut) := wmlib->titlebar(ctxt.screen, tkargs, "Wm ", Wmlib->Hide);

	cmd := chan of string;
	mouse := chan of string;

	tk->namechan(t, cmd, "cmd");
	tk->namechan(t, mouse, "mouse");
	wmlib->tkcmds(t, win_cfg);

	spawn ico(menubut);

	for(;;) alt {
	menu := <-menubut =>
		if(menu[0] == 'e')
			return;
		wmlib->titlectl(t, menu);
	s := <-cmd =>
		tk->cmd(t, "cursor -bitmap cursor.win; grab set .mouse");
		posn := <-mouse;
		tk->cmd(t, "cursor -default; grab release .mouse");

		p := which(ctxt.screen, posn);
		if(p == nil || p == t)
			break;

		case s {
		"raise" =>
			tk->cmd(p, "raise .");
		"lower" =>
			tk->cmd(p, "lower .");
		"del"   =>
			tk->cmd(p, "destroy .");
		"move"	=>
			wmlib->titlectl(p, "move");
		"hide"	=>
			spawn wmlib->titlectl(p, "task");
		}
		p = nil;
	}
}

ico(m: chan of string)
{
	m <-= "task";
}

which(screen: ref Draw->Screen, posn: string): ref Tk->Toplevel
{
	x := int posn;
	for(i := 0; posn[i] != ','; i++)
		;
	y := int posn[i+1:];

	return tk->intop(screen, x, y);
}
