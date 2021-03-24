implement WmAbout;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Display, Image: import draw;

include "tk.m";
	tk: Tk;

include	"wmlib.m";
	wmlib: Wmlib;

include "version.m";

WmAbout: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

about_cfg := array[] of {
	"label .b -bitmap @/icons/lucent.bit",
	"label .t -text {"+
		"Inferno OS "+
		Version->VERSION+
		"\n"+
		"©1999 Lucent Technologies\n"+
	"}",
	"pack .b .t",
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
	(t, menubut) := wmlib->titlebar(ctxt.screen, tkargs, "About Inferno", 0);

	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");
	wmlib->tkcmds(t, about_cfg);

	for(;;) alt {
	menu := <-menubut =>
		if(menu[0] == 'e')
			return;
		wmlib->titlectl(t, menu);
	}
}
