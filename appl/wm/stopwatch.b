implement WmStopWatch;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;

include "tk.m";
	tk: Tk;

include "tklib.m";
	tklib: Tklib;

include	"wmlib.m";
	wmlib: Wmlib;

include "daytime.m";
	daytime: Daytime;


WmStopWatch: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

t: ref Tk->Toplevel;
cmd: chan of string;

tpid: int;

hr,
min,
sec: int;

sw_cfg := array[] of {
	"frame .f",
	"button .f.b1 -text Start -command {send cmd start}",
	"button .f.b2 -text Stop -command {send cmd stop}",
	"button .f.b3 -text Reset -command {send cmd reset}",
	"pack .f.b1 .f.b2 .f.b3 -side left -fill x -expand 1",

	"frame .ft",
	"label .ft.d -label {0:00:00}",
	"pack .ft.d -expand 1",

	"frame .fs1",
	"button .fs1.s -text Time1 -command {send cmd s1}",
	"label .fs1.l -label {0:00:00}",
	"pack .fs1.s .fs1.l -side left -expand 1",

	"frame .fs2",
	"button .fs2.s -text Time2 -command {send cmd s2}",
	"label .fs2.l -label {0:00:00}",
	"pack .fs2.s .fs2.l -side left -expand 1",

	"frame .fs3",
	"button .fs3.s -text Time3 -command {send cmd s3}",
	"label .fs3.l -label {0:00:00}",
	"pack .fs3.s .fs3.l -side left -expand 1",

	"pack .Wm_t -fill x",
	"pack .f .ft .fs1 .fs2 .fs3",
	"pack propagate . 0",
	"update",
};

init(ctxt: ref Draw->Context, argv: list of string)
{
	sys  = load Sys  Sys->PATH;
	draw = load Draw Draw->PATH;
	tk   = load Tk  Tk->PATH;
	wmlib= load Wmlib Wmlib->PATH;
	daytime = load Daytime Daytime->PATH;

	if(draw==nil || tk==nil || wmlib==nil || daytime==nil){
		sys->fprint(sys->fildes(2), "stopwatch: couldn't load modules\n");
		return;
	}

	wmlib->init();

	tkargs := "";
	argv = tl argv;
	if(argv != nil) {
		tkargs = hd argv;
		argv = tl argv;
	}
	menubut := chan of string;
	(t, menubut) = wmlib->titlebar(ctxt.screen, tkargs+" -borderwidth 2 -relief raised", "StopWatch", Wmlib->Appl);

	hr = 0;
	min = 0;
	sec = 0;
	s := sys->sprint("%d:%2.2d:%2.2d", hr, min, sec);

	cmd = chan of string;
	tk->namechan(t, cmd, "cmd");
	wmlib->tkcmds(t, sw_cfg);

	tpid = 0;

	# keep the timerloop in a separate thread,
	# so that wm events don't hold up the ticker
	# i.e., titlebar click&hold would otherwise 
	# 'pause' the timer since the tick would not
	# be processed.

	pid := chan of int;
	spawn timerloop(pid);
	looppid := <- pid;

	for(;;) alt {
	menu := <-menubut =>
		if(menu[0] == 'e') {
			if(tpid)
				kill(tpid);
			kill(looppid);
			return;
		}
		wmlib->titlectl(t, menu);
	}
}

timerloop(pid: chan of int)
{
	pid <- = sys->pctl(0, nil);

	tick := chan of int;
	s: string;

	for(;;) alt {
	c := <-cmd =>
		if(c == "stop"){
			if(tpid != 0){
				kill(tpid);
				tpid = 0;
			}
		} else if(c == "reset"){
			hr = min = sec = 0;
			s = sys->sprint("%d:%2.2d:%2.2d", hr, min, sec);
			tk->cmd(t, ".ft.d configure -label {"+s+"};update");
		} else if(c == "start"){
			if(tpid == 0){
				spawn timer(tick);
				tpid = <- tick;
			}
		} else if(c == "s1" || c == "s2" || c == "s3"){
			s = sys->sprint("%d:%2.2d:%2.2d", hr, min, sec);
			tk->cmd(t, ".f"+c+".l configure -label {"+s+"};update");
		}
	<-tick =>
		sec++;
		if(sec>=60){
			sec = 0;
			min++;
			if(min>=60){
				min = 0;
				hr++;
			}
		}
		s = sys->sprint("%d:%2.2d:%2.2d", hr, min, sec);
		tk->cmd(t, ".ft.d configure -label {"+s+"};update");
	}
}

timer(c: chan of int)
{
	tpid := sys->pctl(0, nil);
	for(;;) {
		c <-= tpid;
		sys->sleep(1000);
	}
}

kill(pid: int)
{
	fd := sys->open("#p/"+string pid+"/ctl", Sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "kill");
}
