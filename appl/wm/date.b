implement WmDate;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;

include "tk.m";
	tk: Tk;

include	"wmlib.m";
	wmlib: Wmlib;

include "daytime.m";
	daytime: Daytime;


WmDate: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

tpid: int;

init(ctxt: ref Draw->Context, argv: list of string)
{
	sys  = load Sys  Sys->PATH;
	draw = load Draw Draw->PATH;
	tk   = load Tk   Tk->PATH;
	wmlib= load Wmlib Wmlib->PATH;
	daytime = load Daytime Daytime->PATH;

	wmlib->init();

	tkargs := "";
	argv = tl argv;
	if(argv != nil) {
		tkargs = hd argv;
		argv = tl argv;
	}
	(t, menubut) := wmlib->titlebar(ctxt.screen, tkargs, "Date", 0);

	cmd := chan of string;

	tk->namechan(t, cmd, "cmd");
	s := daytime->time()[0:19];
	tk->cmd(t, "label .d -label {"+s+"}");
	tk->cmd(t, "pack .d; pack propagate . 0");

	tick := chan of int;
	spawn timer(tick);

	for(;;) alt {
	menu := <-menubut =>
		if(menu[0] == 'e') {
			kill(tpid);
			return;
		}
		wmlib->titlectl(t, menu);
	<-tick =>
		tk->cmd(t, ".d configure -label {"+daytime->time()[0:19]+"};update");
	}
}

timer(c: chan of int)
{
	tpid = sys->pctl(0, nil);
	for(;;) {
		c <-= 1;
		sys->sleep(1000);
	}
}

kill(pid: int)
{
	fd := sys->open("#p/"+string pid+"/ctl", sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "kill");
}
