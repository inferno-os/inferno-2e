implement WmMemory;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Display, Image: import draw;

include "tk.m";
	tk: Tk;
	t: ref Tk->Toplevel;

include	"wmlib.m";
	wmlib: Wmlib;

include "version.m";

WmMemory: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

Arena: adt
{
	name:	string;
	limit:	int;
	size:	int;
	hw:	int;
	y:	int;
	tag:	string;
	taghw:	string;
};
a := array[10] of Arena;

mem_cfg := array[] of {
	"canvas .c -width 280 -height 70",
	"pack .c",
	"update",
};

init(ctxt: ref Draw->Context, argv: list of string)
{
	spawn realinit(ctxt, argv);
}

realinit(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;

	wmlib->init();

	tkargs := "";
	argv = tl argv;
	if(argv != nil) {
		tkargs = hd argv;
		argv = tl argv;
	}

	menubut := chan of string;
	(t, menubut) = wmlib->titlebar(ctxt.screen, tkargs, "Memory", 0);

	cmd := chan of string;

	tk->namechan(t, cmd, "cmd");
	wmlib->tkcmds(t, mem_cfg);

	tick := chan of int;
	spawn ticker(tick);

	mfd := sys->open("/dev/memory", sys->OREAD);

	n := getmem(mfd);
	initdraw(n);

	pid: int;
	for(;;) alt {
	menu := <-menubut =>
		if(menu[0] == 'e'){
			kill(pid);
			return;
		}
		wmlib->titlectl(t, menu);
	pid = <-tick =>
		update(mfd);
		for(i := 0; i < n; i++) {
			x := int ((big a[i].size * big (270-80)) / big a[i].limit);
			s := sys->sprint(".c coords %s 80 %d %d %d",
				a[i].tag,
				a[i].y - 8,
				80+x,
				a[i].y + 8);
			tk->cmd(t, s);
			x = int ((big a[i].hw * big (270-80)) / big a[i].limit);
			s = sys->sprint(".c coords %s 80 %d %d %d",
				a[i].taghw,
				a[i].y - 8,
				80+x,
				a[i].y + 8);
			tk->cmd(t, s);
		}
		tk->cmd(t, "update");
	}
}

ticker(c: chan of int)
{
	pid := sys->pctl(0, nil);
	for(;;) {
		c <-= pid;
		sys->sleep(1000);
	}
}

initdraw(n: int)
{
	y := 20;
	for(i := 0; i < n; i++) {
		tk->cmd(t, ".c create text 5 "+string y+" -anchor w -text "+a[i].name);
		s := sys->sprint("%d", a[i].limit/(1024*1024));
		tk->cmd(t, ".c create text 75 "+string y+" -anchor e -text "+s);
		s = sys->sprint(".c create rectangle 80 %d 270 %d -fill white", y-8, y+8);
		tk->cmd(t, s);
		s = sys->sprint(".c create rectangle 80 %d 270 %d -fill white", y-8, y+8);
		a[i].taghw = tk->cmd(t, s);
		s = sys->sprint(".c create rectangle 80 %d 270 %d -fill red", y-8, y+8);
		a[i].tag = tk->cmd(t, s);
		a[i].y = y;
		y += 20;
	}
	tk->cmd(t, ".c configure -height "+string y);
	tk->cmd(t, "update");
}

buf := array[8192] of byte;

update(mfd: ref Sys->FD): int
{
	sys->seek(mfd, 0, Sys->SEEKSTART);
	n := sys->read(mfd, buf, len buf);
	if(n <= 0)
		exit;
	(nil, l) := sys->tokenize(string buf[0:n], "\n");
	i := 0;
	while(l != nil) {
		s := hd l;
		a[i].size = int s[0:];
		a[i++].hw = int s[24:];
		l = tl l;
	}
	return i;
}

getmem(mfd: ref Sys->FD): int
{
	n := sys->read(mfd, buf, len buf);
	if(n <= 0)
		exit;
	(nil, l) := sys->tokenize(string buf[0:n], "\n");
	i := 0;
	while(l != nil) {
		s := hd l;
		a[i].size = int s[0:];
		a[i].limit = int s[12:];
		a[i].hw = int s[24:];
		a[i].name = s[7*12:];
		i++;
		l = tl l;
	}
	return i;
}

kill(pid: int)
{
	fd := sys->open("#p/"+string pid+"/ctl", sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "kill");
}
