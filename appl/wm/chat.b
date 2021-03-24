implement WmChat;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;

include "tk.m";
	tk: Tk;

include "wmlib.m";
	wmlib: Wmlib;


WmChat: module
{
	init:	fn(ctxt: ref Draw->Context, argl: list of string);
};

t:		ref Tk->Toplevel;
screen:		ref Draw->Screen;
stderr:		ref Sys->FD;
chatio:		ref Sys->FD;
joined:		string;
dialog:		int;
chatreadpid:	int;

main_cmds := array[] of {
	"frame .m -relief raised -bd 2",
	"menubutton .m.session -text Session -menu .m.session.menu",
	"menu .m.session.menu",
	".m.session.menu add command -label Join... -command {send c join}",
	".m.session.menu add command -label Drop -command {send c drop}",
	".m.session.menu add separator",
	".m.session.menu add command -label Exit -command {send c exit}",
	"label .m.slabel -text {-}",
	"pack .m.session -side left",
	"pack .m.slabel -side right",

	"frame .b",
        "scrollbar .b.s -command {.b.t yview}",
        "text .b.t -yscrollcommand {.b.s set}",
	"bind .b.t <Key> {}",
        "pack .b.s -fill y -side right",
        "pack .b.t -fill both -expand 1",

	"frame .e",
	"label .e.alabel -text {:}",
	"entry .e.ent -relief sunken -bd 2",
	"pack .e.alabel  -side left -fill x ",
	"pack .e.ent -side left -fill x -expand 1",
	"bind .e.ent <Key-\n> {send c entry}",

	"pack .m -fill x",
        "pack .b -fill both -expand 1",
	"pack .e -fill x",
	"pack propagate . 0",

	"focus .e.ent",
	"update",
};

diag_cmds := array[] of {
	"frame .b1",
	"label .b1.snl -text Session",
	"entry .b1.sne",
	"pack .b1.snl -side left",
	"pack .b1.sne -side right -fill x -expand 1",

	"frame .b2",
	"label .b2.all -text {Alias  }",
	"entry .b2.ale",
	"pack .b2.all -side left",
	"pack .b2.ale -side right -fill x  -expand 1",

	"frame .fl",
	"scrollbar .fl.scrolly -orient vertical   -command {.fl.l yview}",
	"scrollbar .fl.scrollx -orient horizontal -command {.fl.l xview}",
	"listbox .fl.l -yscrollcommand {.fl.scrolly set} " +
		"-xscrollcommand {.fl.scrollx set}",
	"bind .fl.l <ButtonPress-1> +{send cmd select}",
	"bind .fl.l <Double-Button> +{send cmd pick}",
	"pack .fl.scrollx -side bottom -fill x",
	"pack .fl.scrolly -side right -fill y",
	"pack .fl.l -fill both -expand 1",

	"frame .f2",
	"button .f2.bj -text Join -command {send cmd join}",
	"button .f2.br -text Refresh -command {send cmd refresh}",
	"button .f2.bc -text Cancel -command {send cmd cancel}",
	"pack .f2.bj .f2.br .f2.bc -side left -fill x -expand 1 " +
		"-padx 2 -pady 2",

	"pack .b1 -fill x",
	"pack .b2 -fill x",
	"pack .fl -fill both -expand 1",
	"pack .f2 -fill x ",
	"pack propagate . 0",
};


init(ctxt: ref Draw->Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	stderr = sys->fildes(2);

	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;

	sys->pctl(Sys->NEWPGRP, nil);
	screen = ctxt.screen;

	chatio = sys->open("/chan/chat", Sys->ORDWR);
	if (chatio == nil) {
		sys->fprint(stderr, "chat: couldn't open /chan/chat: %r\n");
		return;
	}

	if(args != nil)
		args = tl args;
	tkargs := "";
	if(args != nil && hd args != "" && (hd args)[0] == '-'){
		tkargs = hd args;
		args = tl args;
	}

	wmlib->init();
	titlectl: chan of string;
	(t, titlectl) = wmlib->titlebar(screen, tkargs, "Chat", Wmlib->Appl);
	wmlib->tkcmds(t, main_cmds);

	c := chan of string;
	tk->namechan(t, c, "c");

	for(;;) alt {
	menu := <-titlectl =>
		if(menu[0] == 'e')
			shutdown();
		wmlib->titlectl(t, menu);
		tk->cmd(t, "focus .e.ent; update");

	cmd := <-c =>
		case cmd {
		"entry" =>
			if(joined != nil){
				msg := ":" + tk->cmd(t, ".e.ent get");
				d := array of byte msg;
				sys->write(chatio, d, len d);
			}
			else
				msgbox("error -fg red", "Send Message",
					"You must join a session first!",
					0, "Continue" :: nil);
			tk->cmd(t, ".e.ent delete 0 end");
			tk->cmd(t, "focus .e.ent; update");
		"exit" =>
			shutdown();
		"join" =>
			if(!dialog){
				dialog = 1;
				spawn join_dialog();
			}
		"drop" =>
			drop();
		}
	}
}

join_dialog()
{
	(dt, diagctl) := wmlib->titlebar( screen, nil, 
				"Join a Session", Wmlib->Appl);
	if(dt == nil){
		dialog = 0;
		return;
	}

	cmd := chan of string;
	tk->namechan(dt, cmd, "cmd");

	wmlib->tkcmds(dt, diag_cmds);
	readsessions(dt);
	tk->cmd(dt, ".b2.ale insert end '" + username());
	tk->cmd(dt, "update");

	session: string;
	alias: string;

	for(;;) alt {
	menu := <-diagctl =>
		if(menu[0] == 'e'){
			dialog = 0;
			return;
		}
		wmlib->titlectl(dt, menu);
		readsessions(dt);
		tk->cmd(dt, "update");
	dcmd := <-cmd =>
		case dcmd {
		"join" =>
			session = tk->cmd(dt, ".b1.sne get");
			alias = tk->cmd(dt, ".b2.ale get");
			if(session != "" && alias != ""){
				join(session, alias);
				dialog = 0;
				return;
			}	
			else
				msgbox("error -fg red", "Join Session",
					"Please make sure you have "+
					"entered/chosen\na session and "+
					"entered an alias",
					0, "Continue" :: nil);
		"cancel" =>
			dialog = 0;
			return;
		"select" =>
			updateselection(dt);
		"pick" =>
			updateselection(dt);
			session = tk->cmd(dt, ".b1.sne get");
			alias = tk->cmd(dt, ".b2.ale get");
			if(session != "" && alias != ""){
				ok := join(session, alias);
				if(!ok)
					break;
				dialog = 0;
				return;
			}
			else
				msgbox("error -fg red", "Join Session",
					"Please make sure you have "+
					"entered/chosen\na session and "+
					"entered an alias",
					0, "Continue" :: nil);
		"refresh" =>
			tk->cmd(dt, ".b1.sne delete 0 end");
			tk->cmd(dt, ".b2.ale delete 0 end");
			tk->cmd(dt, ".b2.ale insert end '" + username());
			readsessions(dt);
			tk->cmd(dt, "update");
		}
	}
}

updateselection(dt: ref Tk->Toplevel)
{
	sel := tk->cmd(dt, ".fl.l curselection");
	if(sel == "")
		return;
	entry := tk->cmd(dt, ".fl.l get "+sel);
	if(entry == "")
		return;
	(nil,sl) := sys->tokenize(entry, ":");
	session := hd sl;
	tk->cmd(dt, ".b1.sne delete 0 end");
	tk->cmd(dt, ".b1.sne insert end '" + session);
	tk->cmd(dt, "update");
}

join(session, alias: string): int
{
	drop();
	joined = session;

	d := array of byte ("join " + session + " " + alias);
	if(sys->write(chatio, d, len d) != len d){
		msgbox("error -fg red", "ChatServer Connection",
			sys->sprint("Error when sending to chatserver:\n%r"),
			0, "Continue" :: nil);
		return 0;
	}
	tk->cmd(t, ".m.slabel configure -text {" + session +
		"}; .e.alabel configure -text {" + alias + ":" +
		"}; update");

	if(chatreadpid)
		kill(chatreadpid);
	c := chan of int;
	spawn chatread(c, chatio);
	chatreadpid = <- c;
	return 1;
}

drop()
{
	d := array of byte "drop";
	sys->write(chatio, d, len d);
	if(chatreadpid)
		kill(chatreadpid);
	chatreadpid = 0;
	joined = nil;
	# tk->cmd(t, ".b.t delete 0 end");	# how to do this?
	tk->cmd(t, ".m.slabel configure -text {-}");
	tk->cmd(t, ".e.alabel configure -text {:}");
	tk->cmd(t, "update");
}

chatread(c: chan of int, i: ref Sys->FD)
{
	n: int;

	c <-= sys->pctl(0, nil);

	buf := array[512] of byte;
	for(;;){
		n = sys->read(i, buf, len buf);
		if(n <= 0)
			break;
		tk->cmd(t, ".b.t see end; .b.t insert end '"+ string buf[:n]);
		tk->cmd(t, "update");
	}
	if(n < 0)
		msgbox("error -fg red", "ChatServer Connection",
			sys->sprint("Error when receiving from chatserver:\n%r"),
			0, "Continue" :: nil);
}

readsessions(dt: ref Tk->Toplevel)
{
	tk->cmd(dt, ".fl.l delete 0 end");

	fd := sys->open("/chan/chatctl", sys->OREAD);
	if(fd == nil)
		return;

	d := array[512] of byte;
	n := sys->read(fd, d, len d);
	(nil, sl) := sys->tokenize(string d[:n], "\n");
	for(; sl != nil; sl = tl sl)
		tk->cmd(dt, ".fl.l insert end '" + hd sl);
}

msgbox(ico, title, msg: string, dflt: int, labs: list of string)
{
	dt := tk->toplevel(screen, "-x 0 -y 0");
	wmlib->dialog(dt, ico, title, msg, dflt, labs);
}

username(): string
{
	fd := sys->open("/dev/user", Sys->OREAD);
	if(fd == nil)
		return "";
	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) 
		return "";
	return string buf[0:n];
}

shutdown()
{
	drop();
	pid := sys->pctl(0, nil);
	fd := sys->open("/prog/"+string pid+"/ctl", Sys->OWRITE);
	if (fd != nil)
		sys->fprint(fd, "killgrp");
	exit;
}

kill(pid: int)
{
	fd := sys->open("/prog/"+string pid+"/ctl", Sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "kill");
}

