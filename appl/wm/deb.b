implement WmDebugger;

include "sys.m";
	sys: Sys;
	stderr: ref Sys->FD;

include "string.m";
	str: String;

include "readdir.m";
	readdir: Readdir;

include "draw.m";
	draw: Draw;

include "tk.m";
	tk: Tk;

include "wmlib.m";
	wmlib: Wmlib;

include "debug.m";
	debug: Debug;
	Prog, Exp, Module, Src, Sym: import debug;

include "wmdeb.m";
	debdata: DebData;
	Vars: import debdata;
	debsrc: DebSrc;
	opendir, Mod: import debsrc;

WmDebugger: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

icondir :	con "debug/";

tkconfig := array[] of {
	"frame .m -relief raised -bd 1",
	"frame .p -padx 2",
	"frame .ctls -padx 2",
	"frame .body",

	# menu bar
	"menubutton .m.file -text File -menu .m.file.menu",
	"menubutton .m.search -text Search -menu .m.search.menu",
	"pack .m.file .m.search -side left",

	# file menu
	"menu .m.file.menu",
	".m.file.menu add command -label Open... -command {send m open}",
	".m.file.menu add command -label Thread... -command {send m pickup}",
	".m.file.menu add command -label Options... -command {send m options}",
	".m.file.menu add separator",

	# search menu
	"menu .m.search.menu",
	".m.search.menu add command -state disabled"+
		" -label Look -command {send m look}",
	".m.search.menu add command -state disabled"+
		" -label {Search For} -command {send m search}",

	# program control
	"image create bitmap Detach -file "+icondir+
			"detach.bit -maskfile "+icondir+"detach.mask",
	"image create bitmap Kill -file "+icondir+
			"kill.bit -maskfile "+icondir+"kill.mask",
	"image create bitmap Run -file "+icondir+
			"run.bit -maskfile "+icondir+"run.mask",
	"image create bitmap Stop -file "+icondir+
			"stop.bit -maskfile "+icondir+"stop.mask",
	"image create bitmap Bpt -file "+icondir+
			"break.bit -maskfile "+icondir+"break.mask",
	"image create bitmap Stepop -file "+icondir+
			"stepop.bit -maskfile "+icondir+"stepop.mask",
	"image create bitmap Stepin -file "+icondir+
			"stepin.bit -maskfile "+icondir+"stepin.mask",
	"image create bitmap Stepout -file "+icondir+
			"stepout.bit -maskfile "+icondir+"stepout.mask",
	"image create bitmap Stepover -file "+icondir+
			"stepover.bit -maskfile "+icondir+"stepover.mask",
	"button .p.kill -image Kill -command {send m killall}"+
			" -state disabled -relief sunken",
	"bind .p.kill <Enter> +{.p.status configure -text {kill current process}}",
	"bind .p.kill <Leave> +{.p.status configure -text {}}",
	"button .p.detach -image Detach -command {send m detach}"+
			" -state disabled -relief sunken",
	"bind .p.detach <Enter> +{.p.status configure -text {stop debugging current process}}",
	"bind .p.detach <Leave> +{.p.status configure -text {}}",
	"button .p.run -image Run -command {send m run}"+
			" -state disabled -relief sunken",
	"bind .p.run <Enter> +{.p.status configure -text {run to breakpoint}}",
	"bind .p.run <Leave> +{.p.status configure -text {}}",
	"button .p.step -image Stepop -command {send m step}"+
			" -state disabled -relief sunken",
	"bind .p.step <Enter> +{.p.status configure -text {step one operation}}",
	"bind .p.step <Leave> +{.p.status configure -text {}}",
	"button .p.stmt -image Stepin -command {send m stmt}"+
			" -state disabled -relief sunken",
	"bind .p.stmt <Enter> +{.p.status configure -text {step one statement}}",
	"bind .p.stmt <Leave> +{.p.status configure -text {}}",
	"button .p.over -image Stepover -command {send m over}"+
			" -state disabled -relief sunken",
	"bind .p.over <Enter> +{.p.status configure -text {step over calls}}",
	"bind .p.over <Leave> +{.p.status configure -text {}}",
	"button .p.out -image Stepout -command {send m out}"+
			" -state disabled -relief sunken",
	"bind .p.out <Enter> +{.p.status configure -text {step out of fn}}",
	"bind .p.out <Leave> +{.p.status configure -text {}}",
	"button .p.bpt -image Bpt -command {send m setbpt}"+
			" -state disabled -relief sunken",
	"bind .p.bpt <Enter> +{.p.status configure -text {set/clear breakpoint}}",
	"bind .p.bpt <Leave> +{.p.status configure -text {}}",
	"frame .p.steps",
	"label .p.status -anchor w",
	"pack .p.step .p.stmt .p.over .p.out -in .p.steps -side left",
	"pack .p.kill .p.detach .p.run .p.steps .p.bpt -side left -padx 5",
	"pack .p.status -side left -expand 1 -fill x",

	# progs
	"frame .prog",
	"label .prog.l -text Threads",
	"canvas .prog.d -height 1 -width 1 -relief sunken -bd 2",
	"frame .prog.v",
	".prog.d create window 0 0 -window .prog.v -anchor nw",
	"pack .prog.l -side top -anchor w",
	"pack .prog.d -side left -fill both -expand 1",

	# breakpoints
	"frame .bpt",
	"label .bpt.l -text Break",
	"canvas .bpt.d -height 1 -width 1 -relief sunken -bd 2",
	"frame .bpt.v",
	".bpt.d create window 0 0 -window .bpt.v -anchor nw",
	"pack .bpt.l -side top -anchor w",
	"pack .bpt.d -side left -fill both -expand 1",

	"pack .prog .bpt -side top -fill both -expand 1 -in .ctls",

	# test body
	"frame .body.ft -bd 1 -relief sunken -width 60w -height 20h",
	"scrollbar .body.scy",
	"pack .body.scy -side right -fill y",

	"pack .body.ft -side top -expand 1 -fill both",
	"pack propagate .body.ft 0",

	"pack .m .p -side top -fill x",
	"pack .ctls -side left -fill y",
	"pack .body -expand 1 -fill both",

	"pack propagate . 0",

	"raise .; update; cursor -default"
};

# commands for disabling or enabling buttons
searchoff := array[] of {
	".m.search.menu entryconfigure 0 -state disabled",
	".m.search.menu entryconfigure 1 -state disabled",
	".m.search.menu entryconfigure 2 -state disabled",
};
searchon := array[] of {
	".m.search.menu entryconfigure 0 -state normal",
	".m.search.menu entryconfigure 1 -state normal",
	".m.search.menu entryconfigure 2 -state normal",
};
tkstopped := array[] of {
	".p.bpt configure -state normal -relief raised",
	".p.detach configure -state normal -relief raised",
	".p.kill configure -state normal -relief raised",
	".p.out configure -state normal -relief raised",
	".p.over configure -state normal -relief raised",
	".p.run configure -state normal -relief raised -image Run -command {send m run}",
	".p.step configure -state normal -relief raised",
	".p.stmt configure -state normal -relief raised",
};
tkrunning := array[] of {
	".p.bpt configure -state normal -relief raised",
	".p.detach configure -state normal -relief raised",
	".p.kill configure -state normal -relief raised",
	".p.out configure -state disabled -relief sunken",
	".p.over configure -state disabled -relief sunken",
	".p.run configure -state normal -relief raised -image Stop -command {send m stop}",
	".p.step configure -state disabled -relief sunken",
	".p.stmt configure -state disabled -relief sunken",
};
tkexited := array[] of {
	".p.bpt configure -state normal -relief raised",
	".p.detach configure -state normal -relief raised",
	".p.kill configure -state normal -relief raised",
	".p.out configure -state disabled -relief sunken",
	".p.over configure -state disabled -relief sunken",
	".p.run configure -state disabled -relief sunken -image Run -command {send m run}",
	".p.step configure -state disabled -relief sunken",
	".p.stmt configure -state disabled -relief sunken",
	".p.stop configure -state disabled -relief sunken",
};
tkloaded := array[] of {
	".p.bpt configure -state normal -relief raised",
	".p.detach configure -state disabled -relief sunken",
	".p.kill configure -state disabled -relief sunken",
	".p.out configure -state disabled -relief sunken",
	".p.over configure -state disabled -relief sunken",
	".p.run configure -state normal -relief raised -image Run -command {send m run}",
	".p.step configure -state disabled -relief sunken",
	".p.stmt configure -state disabled -relief sunken",
};
tknobody := array[] of {
	".p.bpt configure -state disabled -relief sunken",
	".p.detach configure -state disabled -relief sunken",
	".p.kill configure -state disabled -relief sunken",
	".p.out configure -state disabled -relief sunken",
	".p.over configure -state disabled -relief sunken",
	".p.run configure -state disabled -relief sunken -image Run -command {send m run}",
	".p.step configure -state disabled -relief sunken",
	".p.stmt configure -state disabled -relief sunken",
};

#tk option dialog
tkoptpack := array[] of {
	"frame .buts",

	"pack .opts -side left -padx 10 -pady 5",
};

tkoptions := array[] of {
	# general options
	"frame .gen",
	"frame .mod",
	"label .modlab -text 'Executable module",
	"entry .modent",
	"pack .modlab -in .mod -anchor w",
	"pack .modent -in .mod -fill x",

	"frame .wm",
	"checkbutton .wm.prog -command {send cmd wmprog} -text 'WM program",
	"pack .wm.prog -side left",

	"frame .arg",
	"label .arglab -text 'Program Arguments",
	"entry .argent -width 300",
	"pack .arglab -in .arg -anchor w",
	"pack .argent -in .arg -fill x",

	"frame .wd",
	"label .wdlab -text 'Working Directory",
	"entry .wdent",
	"pack .wdlab -in .wd -anchor w",
	"pack .wdent -in .wd -fill x",

	"pack .mod .wm .arg .wd -fill x -anchor w -pady 10 -in .gen",

	# thread control options
	"frame .prog",
	"frame .new",
	"radiobutton .new.run -variable new -value r -text 'Run new threads",
	"radiobutton .new.block -variable new -value b  -text 'Block new threads",
	"pack .new.block .new.run -anchor w",
	"frame .x",
	"radiobutton .x.kill -variable exit -value k -text 'Kill threads on exit",
	"radiobutton .x.detach -variable exit -value d -text 'Detach threads on exit",
	"pack .x.kill .x.detach -anchor w",
	"pack .new .x -expand 1 -anchor w -in .prog",
};

tkopttabs := array[] of {
	("General",	".gen"),
	("Thread",	".prog"),
};

# prog listing dialog box
tkpicktab := array[] of {
	"frame .progs",
	"scrollbar .progs.s -command '.progs.p yview",
	"listbox .progs.p -width 35w -yscrollcommand '.progs.s set",
	"bind .progs.p <Double-Button-1> 'send cmd prog",
	"pack .progs.s -side right -fill y",
	"pack .progs.p -fill both -expand 1",

	"frame .buts",
	"button .buts.prog -text {Add Thread} -command 'send cmd prog",
	"button .buts.grp -text {Add Group} -command 'send cmd group",
	"pack .buts.prog .buts.grp -expand 1 -side left -fill x -padx 4 -pady 4",

	"pack .progs -fill both -expand 1",
	"pack .buts -fill x",
	"pack propagate . 0",
};

Bpt: adt
{
	id:	int;
	m:	ref Mod;
	pc:	int;
};

Recv, Send, Alt, Running, Stopped, Exited, Broken, Killing, Killed: con iota;
status := array[] of
{
	Running =>	"Running",
	Recv =>		"Receive",
	Send =>		"Send",
	Alt =>		"Alt",
	Stopped =>	"Stopped",
	Exited =>	"Exited",
	Broken =>	"Broken",
	Killing =>	"Killed",
	Killed =>	"Killed",
};

tktools : array of array of string;
toolstate : array of string;

KidGrab, KidStep, KidStmt, KidOver, KidOut, KidKill, KidRun: con iota;
Kid: adt
{
	state:	int;
	prog:	ref Prog;
	watch:	int;		# pid of watching prog
	run:	int;		# pid of stepping prog
	pickup:	int;		# picking up this kid?
	cmd:	chan of int;
	stack:	ref Vars;
};

Options: adt
{
	start:	string;		# src of module to start
	mod:	ref Mod;	# module to start
	wm:	int;		# program is a wm program?
	path:	array of string;# search path for .src and .sbl
	args:	list of string;	# argument for starting a kid
	dir:	string;		# . for kid
	tabs:	int;		# options to show
	nrun:	int;		# run new kids?
	xkill:	int;		# kill kids on exit?
};

tktop:		ref Tk->Toplevel;
kids:		list of ref Kid;
kid:		ref Kid;
kidctxt:	ref Draw->Context;
kidack:		chan of (ref Kid, string);
kidevent:	chan of (ref Kid, string);
bpts:		list of ref Bpt;
bptid:=		1;
title:		string;
runok :=	0;
screen:		ref Draw->Screen;
opts:		ref Options;
dbpid:		int;
searchfor:	string;
initsrc:	string;

init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;
	str = load String String->PATH;
	readdir = load Readdir Readdir->PATH;
	debug = load Debug Debug->PATH;
	debdata = load DebData DebData->PATH;
	debsrc = load DebSrc DebSrc->PATH;

	if(tk == nil || wmlib == nil || str == nil || readdir == nil
	|| debug == nil || debdata == nil || debsrc == nil){
		sys->print("Can't load modules\n");
		return;
	}

	dbpid = sys->pctl(Sys->NEWPGRP, nil);
	opts = ref Options;
	opts.wm = 0;
	opts.tabs = 0;
	opts.nrun = 0;
	opts.xkill = 1;
	sysnam := sysname();
	screen = ctxt.screen;

	grabpid := -1;
	geom := "";
	argv = tl argv;
	if(argv != nil){
		g := hd argv;
		if (g[0:1] == "-x") {
			geom = hd argv;
			argv = tl argv;
		}
	}
loop:	for(; argv != nil; argv = tl argv){
		arg := hd argv;
		if(len arg < 2 || arg[0] != '-')
			break;
		case arg[1]{
		'-' =>
			break loop;
		'x' =>
			geom = arg;
		'f' =>
			s := arg[2:];
			if(s == ""){
				argv = tl argv;
				if(argv == nil)
					break loop;
				s = hd argv;
			}
			initsrc = s;
		'p' =>
			s := arg[2:];
			if(s == ""){
				argv = tl argv;
				if(argv == nil)
					break loop;
				s = hd argv;
			}
			grabpid = int s;
		* =>
			sys->print("usage: wmdeb [geom] [-p pid]\n");
			return;
		}
	}

	pickdummy := chan of int;
	pickchan := pickdummy;
	optdummy := chan of ref Options;
	optchan := optdummy;

	tktools = array[] of {
		Running =>	tkrunning,
		Recv =>		tkrunning,
		Send =>		tkrunning,
		Alt =>		tkrunning,
		Stopped =>	tkstopped,
		Exited =>	tkexited,
		Broken =>	tkexited,
		Killing =>	tkexited,
		Killed =>	tkexited,
	};

	mygeom := geom;
	argc: int;
	(argc, argv) = sys->tokenize(geom, " \t");
	if(argc == 4 && hd argv == "-x" && hd tl tl argv == "-y")
		mygeom = "-x "+string(int hd tl argv + 20)+
			" -y "+string(int hd tl tl tl argv + 20);

	wmlib->init();
	title = sysnam+":Wmdeb";
	titlebut := chan of string;
	(tktop, titlebut) = wmlib->titlebar(screen, mygeom, title, Wmlib->Appl);
	tkcmd("cursor -bitmap cursor.wait");

	debug->init();
	kidctxt = ctxt;

	stderr = sys->fildes(2);

	debsrc->init(screen, tktop, sys, tk, wmlib, str, debug);
	(datactl, datatitle) := debdata->init(screen, geom, debsrc, sys, tk, str, debug);

	m := chan of string;
	tk->namechan(tktop, m, "m");
	toolstate = tknobody;
	wmlib->tkcmds(tktop, tkconfig);

	kids = nil;
	kid = nil;
	kidack = chan of (ref Kid, string);
	kidevent = chan of (ref Kid, string);

	# pick up a src file, a kid?
	if(initsrc != nil)
		open1(initsrc);
	else if(grabpid > 0)
		pickup(grabpid);

	for(exiting := 0; !exiting || kids != nil; ){
		tkcmd("update");
		alt {
		s := <-titlebut =>
			case s{
			"exit" =>
				if(!exiting){
					if(opts.xkill)
						killkids();
					else
						detachkids();
					tkcmd("destroy .");
				}
				exiting = 1;
				break;
			"task" =>
				spawn wmlib->titlectl(tktop, s);
			* =>
				wmlib->titlectl(tktop, s);
			}
		s := <-datactl =>
			debdata->ctl(s);
		s := <-datatitle =>
			case s{
			"task" =>
				spawn debdata->titlectl(s);
			* =>
				debdata->titlectl(s);
			}
		o := <-optchan =>
			if(o != nil && checkopts(o))
				opts = o;
			optchan = optdummy;
		p := <-pickchan =>
			if(p < 0){
				pickchan = pickdummy;
				break;
			}
			k := pickup(p);
			if(k != nil && k != kid){
				kid = k;
				refresh(k);
			}
		s := <-m =>
			case s {
			"open" =>
				open();
			"pickup" =>
				if(pickchan == pickdummy){
					pickchan = chan of int;
					spawn pickprog(pickchan);
				}
			"options" =>
				if(optchan == optdummy){
					optchan = chan of ref Options;
					spawn options(opts, optchan);
				}
			"step" =>
				step(kid, KidStep);
			"over" =>
				step(kid, KidOver);
			"out" =>
				step(kid, KidOut);
			"stmt" =>
				step(kid, KidStmt);
			"run" =>
				step(kid, KidRun);
			"stop" =>
				if(kid != nil)
					kid.prog.stop();
			"killall" =>
				killkids();
			"kill" =>
				killkid(kid);
			"detach" =>
				detachkid(kid);
			"setbpt" =>
				setbpt();
			"look" =>
				debsrc->search(debsrc->snarf());
			"search" =>
				s = wmlib->getstring(tktop, "Search For");
				if(s == ""){
					tkcmd(".m.search.menu delete 2");
				}else{
					if(searchfor == "")
						tkcmd(".m.search.menu add command -command {send m research}");
					tkcmd(".m.search.menu entryconfigure 2 -label '/"+s);
					debsrc->search(s);
				}
				searchfor = s;
			"research" =>
				debsrc->search(searchfor);
			* =>
				if(str->prefix("open ", s))
					debsrc->showstrsrc(s[len "open ":]);
				else if(str->prefix("seeprog ", s))
					seekid(int s[len "seeprog ":]);
				else if(str->prefix("seebpt ", s))
					seebpt(int s[len "seebpt ":]);
			}
		(k, s) := <-kidevent =>
			case s{
			"recv" =>
				if(k.state == Running)
					k.state = Recv;
			"send" =>
				if(k.state == Running)
					k.state = Send;
			"alt" =>
				if(k.state == Running)
					k.state = Alt;
			"run" =>
				if(k.state == Recv || k.state == Send || k.state == Alt)
					k.state = Running;
			"exited" =>
				k.state = Exited;
			"interrupted" or
			"killed" =>
				alert("Thread "+string k.prog.id+" "+s);
				k.state = Exited;
			* =>
				if(str->prefix("new ", s)){
					nk := newkid(int s[len "new ":]);
					if(opts.nrun)
						step(nk, KidRun);
					break;
				}

				if(str->prefix("child: ", s))
					s = s[len "child: ":];

				if(str->prefix("broken: ", s))
					k.state = Broken;
				alert("Thread "+string k.prog.id+" "+s);
			}
			if(k == kid && k.state != Running)
				refresh(k);
			k = nil;
		(k, s) := <-kidack =>
			if(k.state == Killing){
				k.state = Killed;
				k.cmd <-= KidKill;
				k = nil;
				break;
			}
			if(k.state == Killed){
				delkid(k);
				k = nil;
				break;
			}
			case s{
			"" or "child: breakpoint" or "child: stopped" =>
				k.state = Stopped;
				k.prog.unstop();
			"prog broken" =>
				k.state = Broken;
			* =>
				if(!str->prefix("child: ", s))
					alert("Debugger error "+status[k.state]+" "+string k.prog.id+" '"+s+"'");
			}
			if(k == kid)
				refresh(k);
			if(k.pickup && opts.nrun){
				k.pickup = 0;
				if(k.state == Stopped)
					step(k, KidRun);
			}
			k = nil;
		}
	}
	exitdb();
}

open()
{
	pattern := list of {
		"*.b (Limbo source files)",
		"* (All files)"
	};

	file := wmlib->filename(screen, tktop, "Open source file", pattern, opendir);
	if(file != nil)
		open1(file);
}

open1(file: string)
{
	(opendir, nil) = str->splitr(file, "/");
	if(opendir == "")
		opendir = ".";
	m := debsrc->loadsrc(file, 1);
	if(m == nil){
		alert("Can't open "+file);
		return;
	}
	debsrc->showmodsrc(m, ref Src((file, 1, 0), (file, 1, 0)));
	kidstate();
	if(opts.start == nil){
		opts.start = file;
		opts.mod = m;
	}
	if(opts.dir == "")
		opts.dir = opendir;
}

options(oo: ref Options, r: chan of ref Options)
{
	(t, titlebut) := wmlib->titlebar(screen, wmlib->geom(tktop), "Wmdeb Options", wmlib->OK);
	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");

	wmlib->tkcmds(t, tkoptions);
	tabs := wmlib->mktabs(t, ".opts", tkopttabs, oo.tabs);
	wmlib->tkcmds(t, tkoptpack);

	o := ref *oo;
	if(o.start != nil)
		tk->cmd(t, ".modent insert end '"+o.start);
	args := "";
	for(oa := o.args; oa != nil; oa = tl oa){
		if(args == "")
			args = hd oa;
		else
			args += " " + hd oa;
	}
	tk->cmd(t, ".argent insert end '"+args);
	tk->cmd(t, ".wdent insert end '"+o.dir);
	if(o.xkill)
		tk->cmd(t, ".x.kill invoke");
	else
		tk->cmd(t, ".x.detach invoke");
	if(o.nrun)
		tk->cmd(t, ".new.run invoke");
	else
		tk->cmd(t, ".new.block invoke");
	if(o.wm)
		tk->cmd(t, ".wm.prog select");

	tk->cmd(t, ".killkids configure -command 'send cmd kill");
	tk->cmd(t, ".runkids configure -command 'send cmd run");

out:	for(;;){
		tk->cmd(t, "update");
		alt{
		s := <-tabs =>
			o.tabs = wmlib->tabsctl(t, ".opts", tkopttabs, o.tabs, s);
		s := <-titlebut =>
			case s{
			"exit" =>
				r <-= nil;
				exit;
			"ok" =>
				break out;
			}
			wmlib->titlectl(t, s);
		s := <-cmd =>
			case s{
			"wmprog" =>
				o.wm ^= 1;
			}
		}
	}
	o.start = tk->cmd(t, ".modent get");
	(nil, o.args) = sys->tokenize(tk->cmd(t, ".argent get"), " \t\n");
	o.dir = tk->cmd(t, ".wdent get");
	case tk->cmd(t, "variable new"){
	"r" => o.nrun = 1;
	"b" => o.nrun = 0;
	}
	case tk->cmd(t, "variable exit"){
	"k" => o.xkill = 1;
	"d" => o.xkill = 0;
	}
	r <-= o;
}

checkopts(o: ref Options): int
{
	if(o.start != ""){
		o.mod = debsrc->loadsrc(o.start, 1);
		if(o.mod == nil)
			o.start = "";
	}
	return 1;
}

pickprog(c: chan of int)
{
	(t, titlebut) := wmlib->titlebar(screen, wmlib->geom(tktop), "Wmdeb Thread List", 0);
	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");

	wmlib->tkcmds(t, tkpicktab);
	tk->cmd(t, "update");
	ids := addpickprogs(t);

	for(;;){
		tk->cmd(t, "update");
		alt{
		s := <-titlebut =>
			if(s[0] == 'e'){
				c <-= -1;
				exit;
			}
			wmlib->titlectl(t, s);
		s := <-cmd =>
			case s{
			"ok" =>
				c <-= -1;
				exit;
			"prog" =>
				sel := tk->cmd(t, ".progs.p curselection");
				if(sel == "")
					break;
				pid := int tk->cmd(t, ".progs.p get "+sel);
				c <-= pid;
			"group" =>
				sel := tk->cmd(t, ".progs.p curselection");
				if(sel == "")
					break;
				nid := int sel;
				if(nid > len ids || nid < 0)
					break;
				(pid, gid) := ids[nid];
				nid = len ids;
				for(i := 0; i < nid; i++){
					(p, g) := ids[i];
					if(g == gid)
						c <-= p;
				}
			}
		}
	}
}

addpickprogs(t: ref Tk->Toplevel): array of (int, int)
{
	(d, n) := readdir->init("/prog", Readdir->NONE);
	if(n <= 0)
		return nil;
	a := array[n] of { * => (-1, -1) };
	for(i := 0; i < n; i++){
		(p, nil) := debug->prog(int d[i].name);
		if(p == nil)
			continue;
		(grp, user, st, code) := debug->p.status();
		if(grp < 0)
			continue;
		a[i] = (p.id, grp);
		tk->cmd(t, ".progs.p insert end '"+
				sys->sprint("%4d %4d %8s %s", p.id, grp, st, code));
	}
	return a;
}

step(k: ref Kid, cmd: int)
{
	if(k == nil){
		if(kids != nil){
			alert("No current thread");
			return;
		}
		k = spawnkid(opts);
		kid = k;
		if(k != nil)
			refresh(k);
		return;
	}
	case k.state{
	Stopped =>
		k.cmd <-= cmd;
		k.state = Running;
		if(k == kid)
			kidstate();
	Running or Send or Recv or Alt or Exited or Broken =>
		;
	* =>
		sys->print("bad debug step state %d\n", k.state);
	}
}

setbpt()
{
	(m, pc) := debsrc->getsel();
	if(m == nil)
		return;
	s := m.sym.pctosrc(pc);
	if(s == nil){
		alert("No pc is appropriate");
		return;
	}

	# if the breakpoint is already there, delete it
	for(bl := bpts; bl != nil; bl = tl bl){
		b := hd bl;
		if(b.m == m && b.pc == pc){
			bpts = delbpt(b, bpts);
			return;
		}
	}

	b := ref Bpt(bptid++, m, pc);
	bpts = b :: bpts;
	debsrc->attachdis(m);
	for(kl := kids; kl != nil; kl = tl kl){
		k := hd kl;
		k.prog.setbpt(m.dis, pc);
	}

	# mark the breakpoint text
	tkcmd(m.tk+" tag add bpt "+string s.start.line+"."+string s.start.pos+" "+string s.stop.line+"."+string s.stop.pos);

	# add the kid to the breakpoint window
	me := ".bpt.v."+string b.id;
	tkcmd("label "+me+" -text "+string b.id);
	tkcmd("pack "+me+" -side top -fill x");
	tkcmd("bind "+me+" <ButtonRelease-1> {send m seebpt "+string b.id+"}");
	updatebpts();
}

seebpt(bpt: int)
{
	for(bl := bpts; bl != nil; bl = tl bl){
		b := hd bl;
		if(b.id == bpt){
			s := b.m.sym.pctosrc(b.pc);
			debsrc->showmodsrc(b.m, s);
			return;
		}
	}
}

delbpt(b: ref Bpt, bpts: list of ref Bpt): list of ref Bpt
{
	if(bpts == nil)
		return nil;
	hb := hd bpts;
	tb := tl bpts;
	if(b == hb){
		# remove mark from breakpoint text
		s := b.m.sym.pctosrc(b.pc);
		tkcmd(b.m.tk+" tag remove bpt "+string s.start.line+"."+string s.start.pos+" "+string s.stop.line+"."+string s.stop.pos);
	
		# remove the breakpoint window
		tkcmd("destroy .bpt.v."+string b.id);

		# remove from kids
		disablebpt(b);
		return tb;
	}
	return hb :: delbpt(b, tb);

}

disablebpt(b: ref Bpt)
{
	for(kl := kids; kl != nil; kl = tl kl){
		k := hd kl;
		k.prog.delbpt(b.m.dis, b.pc);
	}
}

updatebpts()
{
tkcmd("update");
	tkcmd(".bpt.d configure -scrollregion {0 0 [.bpt.v cget -width] [.bpt.v cget -height]}");
}

seekid(pid: int)
{
	for(kl := kids; kl != nil; kl = tl kl){
		k := hd kl;
		if(k.prog.id == pid){
			kid = k;
			kid.stack.show();
			refresh(kid);
			return;
		}
	}
}

delkid(k: ref Kid)
{
	kids = rdelkid(k, kids);
	if(kid == k){
		if(kids == nil){
			kid = nil;
			kidstate();
		}else{
			kid = hd kids;
			refresh(kid);
		}
	}
}

rdelkid(k: ref Kid, kids: list of ref Kid): list of ref Kid
{
	if(kids == nil)
		return nil;
	hk := hd kids;
	t := tl kids;
	if(k == hk){
		# remove kid from display
		k.stack.delete();
		tkcmd("destroy .prog.v."+string k.prog.id);
		updatekids();
		return t;
	}
	return hk :: rdelkid(k, t);
}

updatekids()
{
tkcmd("update");
	tkcmd(".prog.d configure -scrollregion {0 0 [.prog.v cget -width] [.prog.v cget -height]}");
}

killkids()
{
	for(kl := kids; kl != nil; kl = tl kl)
		killkid(hd kl);
}

killkid(k: ref Kid)
{
	if(k.watch >= 0){
		killpid(k.watch);
		k.watch = -1;
	}
	case k.state{
	Exited or Broken or Stopped =>
		k.cmd <-= KidKill;
		k.state = Killed;
	Running or Send or Recv or Alt or Killing =>
		err := k.prog.kill();
		k.state = Killing;
	* =>
		sys->print("unknown state %d in killkid\n", k.state);
	}
}

detachkids()
{
	for(kl := kids; kl != nil; kl = tl kl)
		detachkid(hd kl);
}

detachkid(k: ref Kid)
{
	if(k == nil){
		alert("No current thread");
		return;
	}
	if(k.state == Exited){
		killkid(k);
		return;
	}

	# kill off the debugger progs
	killpid(k.watch);
	killpid(k.run);
	err := k.prog.start();
	if(err != "")
		alert("Detaching thread: "+err);

	delkid(k);
}

kidstate()
{
	ts : array of string;
	if(kid == nil){
		tkcmd(".Wm_t.title configure -text '"+title);
		if(debsrc->packed == nil){
			wmlib->tkcmds(tktop, searchoff);
			ts = tknobody;
		}else{
			ts = tkloaded;
			wmlib->tkcmds(tktop, searchon);
		}
	}else{
		tkcmd(".Wm_t.title configure -text '"+title+" "+string kid.prog.id+" "+status[kid.state]);
		ts = tktools[kid.state];
		wmlib->tkcmds(tktop, searchon);
	}
	if(ts != toolstate){
		toolstate = ts;
		wmlib->tkcmds(tktop, ts);
	}
}

#
# update the stack an src displays
# to reflect the current state of k
#
refresh(k: ref Kid)
{
	if(k.state == Killing || k.state == Killed){
		kidstate();
		return;
	}
	(s, err) := k.prog.stack();
	if(s == nil && err == "")
		err = "No stack";
	if(err != ""){
		kidstate();
		return;
	}
	for(i := 0; i < len s; i++){
		debsrc->findmod(s[i].m);
		s[i].findsym();
	}
	err = s[0].findsym();
	src := s[0].src();
	kidstate();
	m := s[0].m;
	if(src == nil && len s > 1){
		dis := s[0].m.dis();
		if(len dis > 0 && dis[0] == '$'){
			m = s[1].m;
			s[1].findsym();
			src = s[1].src();
		}
	}
	debsrc->showmodsrc(debsrc->findmod(m), src);
	k.stack.refresh(s);
	k.stack.show();
}

pickup(pid: int): ref Kid
{
	for(kl := kids; kl != nil; kl = tl kl)
		if((hd kl).prog.id == pid)
			return hd kl;
	k := newkid(pid);
	if(k == nil)
		return nil;
	k.cmd <-= KidGrab;
	k.state = Running;
	k.pickup = 1;
	if(kid == nil){
		kid = k;
		refresh(kid);
	}
	return k;
}

newkid(pid: int): ref Kid
{
	(p, err) := debug->prog(pid);
	if(err != ""){
		alert("Can't pick up thread "+err);
		return nil;
	}
	return mkkid(p);
}

mkkid(p: ref Prog): ref Kid
{
	for(bl := bpts; bl != nil; bl = tl bl){
		b := hd bl;
		debsrc->attachdis(b.m);
		p.setbpt(b.m.dis, b.pc);
	}
	k := ref Kid(Stopped, p, -1, -1, 0, chan of int, Vars.create());
	kids = k :: kids;
	c := chan of int;
	spawn kidslave(k, c);
	k.run = <- c;
	spawn kidwatch(k, c);
	k.watch = <-c;
	me := ".prog.v."+string p.id;
	tkcmd("label "+me+" -text "+string p.id);
	tkcmd("pack "+me+" -side top -fill x");
	tkcmd("bind "+me+" <ButtonRelease-1> {send m seeprog "+string p.id+"}");
	tkcmd(".prog.d configure -scrollregion {0 0 [.prog.v cget -width] [.prog.v cget -height]}");
	return k;
}

spawnkid(o: ref Options): ref Kid
{
	m := o.mod;
	if(m == nil){
		alert("No module to run");
		return nil;
	}

	if(!debsrc->attachdis(m)){
		alert("Can't load Dis file "+m.dis);
		return nil;
	}

	argv := m.dis :: o.args;
	if(o.wm)
		argv = m.dis :: kidgeom() :: o.args;
	(p, err) := debug->startprog(m.dis, o.dir, kidctxt, argv);
	if(err != nil){
		alert(m.dis+" is not a debuggable Dis command module");
		return nil;
	}

	return mkkid(p);
}

xlate := array[] of {
	KidStep => Debug->StepExp,
	KidStmt => Debug->StepStmt,
	KidOver => Debug->StepOver,
	KidOut => Debug->StepOut,
};

kidslave(k: ref Kid, me: chan of int)
{
	me <-= sys->pctl(0, nil);
	me = nil;
	for(;;){
		c := <-k.cmd;
		case c{
		KidGrab =>
			err := k.prog.grab();
			kidack <-= (k, err);
		KidStep or KidStmt or KidOver or KidOut =>
			err := k.prog.step(xlate[c]);
			kidack <-= (k, err);
		KidKill =>
			err := "kill "+k.prog.kill();
			k.prog.kill();			# kill again to slay blocked progs
			kidack <-= (k, err);
			exit;
		KidRun =>
			err := k.prog.cont();
			kidack <-= (k, err);
		* =>
			sys->print("kidslave: bad command %d\n", c);
			exit;
		}
	}
}

kidwatch(k: ref Kid, me: chan of int)
{
	me <-= sys->pctl(0, nil);
	me = nil;
	for(;;)
		kidevent <-= (k, k.prog.event());
}

alert(m: string)
{
	wmlib->dialog(tktop, "warning -fg yellow",
		"Debugger Alert", m, 0, "Dismiss"::nil);
}

tkcmd(cmd: string): string
{
	s := tk->cmd(tktop, cmd);
	if(len s != 0 && s[0] == '!')
		sys->print("%s '%s'\n", s, cmd);
	return s;
}

sysname(): string
{
	fd := sys->open("#c/sysname", sys->OREAD);
	if(fd == nil)
		return "Anon";
	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) 
		return "Anon";
	return string buf[:n];
}

gx := 20;
gy := 20;
kidgeom(): string
{
	if(gx > 130) {
		gx = 0;
		gy = 0;
	}
	gx += 20;
	gy += 20;
	return " -x "+string gx+" -y "+string gy;
}

exitdb()
{
	fd := sys->open("#p/"+string dbpid+"/ctl", sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "killgrp");
	exit;
}

killpid(pid: int)
{
	fd := sys->open("#p/"+string pid+"/ctl", sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "kill");
}
