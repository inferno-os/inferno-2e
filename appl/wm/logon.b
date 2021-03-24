implement WmLogon;
#
# Logon program for Wm environment
#
include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Screen, Display, Image, Context: import draw;
	ctxt: ref Context;

include "tk.m";
	tk: Tk;

include "sh.m";

include "newns.m";

include "keyring.m";
include "security.m";

WmLogon: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

Wm: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

kfd, waitfd:	ref Sys->FD;
waitdone:	chan of int;

cfg := array[] of {
	"label .p -bitmap @/icons/logon.bit -borderwidth 2 -relief raised",
	"frame .l",
	"label .l.u -text {User Name:} -anchor w",
	"pack .l.u -fill x",
	"frame .e",
	"entry .e.u",
	"pack .e.u -fill x",
	"frame .f -borderwidth 2 -relief raised",
	"pack .l .e -side left -in .f",
	"pack .p .f -fill x",
	"bind .e.u <Key-\n> {send cmd ok}",
	"focus .e.u"
};

init(nil: ref Draw->Context, args: list of string)
{
	usr: string;
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	sys->pctl(sys->NEWPGRP, nil);

	mfd := sys->open("/dev/pointer", sys->OREAD);
	if(mfd == nil) {
		sys->print("open: /dev/pointer: %r\n");
		return;
	}
	keyfd := sys->open("/dev/keyboard", sys->OREAD);
	if(keyfd == nil) {
		sys->print("can't open /dev/keyboard: %r");
		return;
	}

	ctxt = ref Context;
	ctxt.display = Display.allocate(nil);
	if(ctxt.display == nil) {
		sys->print("logon: can't initialize display: %r\n");
		return;
	}

	disp := ctxt.display.image;
	ctxt.screen = Screen.allocate(disp, ctxt.display.rgb(161, 195, 209), 1);
	disp.draw(disp.r, ctxt.screen.fill, ctxt.display.ones, disp.r.min);

	spid := string sys->pctl(0, nil);
	kfd = sys->open("#p/"+spid+"/ctl", sys->OWRITE);
	if(kfd == nil) {
		notice("error opening pid "+
			spid+"\n"+
			sys->sprint("%r"));
	}
	waitfd = sys->open("#p/"+spid+"/wait", sys->OREAD);
	if(waitfd == nil) {
		notice("error opening pid "+
			spid+" wait\n"+
			sys->sprint("%r"));
	}
	waitdone = chan of int;
	spawn waiter(2);
	<- waitdone;
	spawn mouse(ctxt.screen, mfd);
	spawn keyboard(ctxt.screen, keyfd);
	t := tk->toplevel(ctxt.screen, "-x 50 -y 50");

	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");

	for(i := 0; i < len cfg; i++)
		tk->cmd(t, cfg[i]);

	err := tk->cmd(t, "variable lasterr");
	if(err != nil)
		sys->print("logon: tk error: %s\n", err);

	if(args != nil) {
		args = tl args;
		if(args != nil && hd args == "-u") {
			args = tl args;
			if(args != nil) {
				usr = hd args;
				args = tl args;
			}
		}
	}

	for(;;) {
		tk->cmd(t, "focus .e.u; update");
		if(usr != nil)
			tk->cmd(t, ".e.u insert 0 "+usr+"; update");
		else {
			<-cmd;
			usr = tk->cmd(t, ".e.u get");
			if(usr == "") {
				notice("You must supply a user name to login");
				continue;
			}
		}
		if(logon(usr))
				break;
		tk->cmd(t, ".e.u delete 0 end");
		usr = nil;
	}
	tk->cmd(t, "cursor -bitmap cursor.wait");
	(ok, nil) := sys->stat("namespace");
	if(ok >= 0) {
		ns := load Newns Newns->PATH;
		if(ns == nil)
			notice("Failed to load namespace builder");
		else {
			nserr := ns->newns(nil, nil);
			if(nserr != nil)
				notice("Error in user namespace file:\n"+nserr);
		}
	}
	wm := load Wm "/dis/wm/wm.dis";
	if(wm == nil)
		notice("Failed to load window manager");
	sys->fprint(kfd, "killgrp");
	mfd = nil;
	keyfd = nil;
	sys->pctl(sys->NEWFD, 0 :: 1 :: 2 :: nil);
	profile(ctxt);
	tk->cmd(t, "cursor -default");
	<- waitdone;
	spawn wm->init(nil, "wm" :: args);
}

profile(ctxt: ref Context)
{
	(ok, nil) := sys->stat("profile");
	if(ok < 0)
		return;

	sh := load Command "/dis/sh.dis";
	if(sh == nil) {
		notice("Failed to execute profile\nshell not found");
		return;
	}

	sh->init(ctxt, "/dis/sh.dis" :: "profile" :: nil);
}

logon(user: string): int
{
	userdir := "/usr/"+user;
	if(sys->chdir(userdir) < 0) {
		notice("There is no home directory for \""+
			user+"\"\nmounted on this machine");
		return 0;
	}

	#
	# Set the user id
	#
	fd := sys->open("/dev/user", sys->OWRITE);
	if(fd == nil) {
		notice(sys->sprint("failed to open /dev/user: %r"));
		return 0;
	}
	b := array of byte user;
	if(sys->write(fd, b, len b) < 0) {
		notice("failed to write /dev/user\nwith error "+sys->sprint("%r"));
		return 0;
	}

	license(user);

	return 1;
}

notecmd := array[] of {
	"frame .f",
	"label .f.l -bitmap error -foreground red",
	"button .b -text Continue -command {send cmd done}",
	"focus .f",
	"bind .f <Key-\n> {send cmd done}",
	"pack .f.l .f.m -side left -expand 1 -padx 10 -pady 10",
	"pack .f .b -padx 10 -pady 10",
	"update; cursor -default"
};

notice(message: string)
{
	t := tk->toplevel(ctxt.screen, "-x 70 -y 70 -borderwidth 2 -relief raised");
	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");
	tk->cmd(t, "label .f.m -text {"+message+"}");
	for(i := 0; i < len notecmd; i++)
		tk->cmd(t, notecmd[i]);
	<-cmd;
}

mouse(s: ref Draw->Screen, fd: ref Sys->FD)
{
	n := 0;
	buf := array[100] of byte;
	for(;;) {
		n = sys->read(fd, buf, len buf);
		if(n <= 0)
			break;

		if(int buf[0] == 'm' && n == 37) {
			x := int(string buf[ 1:13]);
			y := int(string buf[12:25]);
			b := int(string buf[24:37]);
			tk->mouse(s, x, y, b);
		}
	}
}

keyboard(s: ref Draw->Screen, dfd: ref Sys->FD)
{
	b:= array[1] of byte;
	buf := array[10] of byte;
	i := 0;
	for(;;) {
		n := sys->read(dfd, buf[i:], len buf - i);
		if(n < 1){
			notice(sys->sprint("keyboard read error: %r"));
			break;
		}
		i += n;
		while(i >0 && (nutf := sys->utfbytes(buf, i)) > 0){
			str := string buf[0:nutf];
			for (j := 0; j < len str; j++)
				tk->keyboard(s, int str[j]);
			buf[0:] = buf[nutf:i];
			i -= nutf;
		}
	}
}

license(user: string)
{
	host := rf("/dev/sysname");

	uh := 0;
	for(i := 0; i < len user; i++)
		uh = uh*3 + user[i];
	hh := 0;
	for(i = 0; i < len host; i++)
		hh = hh*3 + host[i];

	path := sys->sprint("/licensedb/%.16bx", (big uh<<32)+big hh);
	(ok, nil) := sys->stat(path);
	if(ok >= 0)
		return;

	wm := load Wm "/dis/wm/license.dis";
	if(wm == nil)
		return;

	wm->init(ctxt, "license.dis" :: nil);
}

rf(path: string) : string
{
	fd := sys->open(path, sys->OREAD);
	if(fd == nil)
		return "Anon";

	buf := array[512] of byte;
	n := sys->read(fd, buf, len buf);
	if(n <= 0)
		return "Anon";

	return string buf[0:n];
}

waiter(n: int)
{
	sys->pctl(sys->NEWPGRP, nil);
	waitdone <-= 0;
	if (waitfd != nil) {
		b := array[sys->WAITLEN] of byte;
		while(n > 0) {
			if(sys->read(waitfd, b, sys->WAITLEN) <= 0)
				break;
			n--;
		}
	}
	waitdone <-= 0;
}
