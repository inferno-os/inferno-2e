implement WmPlay;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Context: import draw;
	gctxt: ref Context;

include "tk.m";
	tk: Tk;

include	"wmlib.m";
	wmlib: Wmlib;

tpid:	int;
ppid:	int;
Magic:	con "rate";
data:	con "/dev/audio";
ctl:	con "/dev/audioctl";
buffz:	con Sys->ATOMICIO;

WmPlay: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

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
	t := tk->toplevel(gctxt.screen, "-x 70 -y 70 -borderwidth 2 -relief raised");
	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");
	tk->cmd(t, "label .f.m -text {" + message + "}");
	for (i := 0; i < len notecmd; i++)
		tk->cmd(t, notecmd[i]);
	<- cmd;
}

play(f: string)
{
	ppid = sys->pctl(0, nil);
	buff := array[buffz] of byte;
	inf := sys->open(f, Sys->OREAD);
	if (inf == nil) {
		notice(sys->sprint("could not open %s: %r", f));
		return;
	}
	n := sys->read(inf, buff, buffz);
	if (n < 0) {
		notice(sys->sprint("could not read %s: %r", f));
		return;
	}
	if (n < 10 || string buff[0:4] != Magic) {
		notice(sys->sprint("%s: not an audio file", f));
		return;
	}
	i := 0;
	for (;;) {
		if (i == n) {
			notice(sys->sprint("%s: bad header", f));
			return;
		}
		if (buff[i] == byte '\n') {
			i++;
			if (i == n) {
				notice(sys->sprint("%s: bad header", f));
				return;
			}
			if (buff[i] == byte '\n') {
				i++;
				if ((i % 4) != 0) {
					notice(sys->sprint("%s: unpadded header", f));
					return;
				}
				break;
			}
		}
		else
			i++;
	}
	df := sys->open(data, Sys->OWRITE);
	if (df == nil) {
		notice(sys->sprint("could not open %s: %r", data));
		return;
	}
	cf := sys->open(ctl, Sys->OWRITE);
	if (cf == nil) {
		notice(sys->sprint("could not open %s: %r", ctl));
		return;
	}
	if (sys->write(cf, buff, i - 1) < 0) {
		notice(sys->sprint("could not write %s: %r", ctl));
		return;
	}
	if (n > i && sys->write(df, buff[i:n], n - i) < 0) {
		notice(sys->sprint("could not write %s: %r", data));
		return;
	}
	if (sys->stream(inf, df, Sys->ATOMICIO) < 0) {
		notice(sys->sprint("could not stream %s: %r", data));
		return;
	}
}

doplay(f: string)
{
	play(f);
	kill(tpid);
}

init(ctxt: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;

	gctxt = ctxt;
	wmlib->init();

	tkargs := "";
	argv = tl argv;
	if (argv != nil) {
		tkargs = hd argv;
		argv = tl argv;
	}

	file: string;
	if (argv != nil)
		file = hd argv;
	else {
		pat := list of {
			"*.iaf (Inferno Audio Files)",
			"* (All files)"
		};
		t := tk->toplevel(ctxt.screen, tkargs);
		file = wmlib->filename(ctxt.screen, t, "Locate Audio File", pat, "");
		if (file == "")
			exit;
	}

	(t, menubut) := wmlib->titlebar(ctxt.screen, tkargs + " -borderwidth 2 -relief raised", "Play", 0);
	tk->cmd(t, "label .d -label {" + file + "}");
	tk->cmd(t, "pack .Wm_t -fill x; pack .d; pack propagate . 0");
	tk->cmd(t, "update");

	tpid = sys->pctl(0, nil);
	spawn doplay(file);

	for(;;) {
		menu := <- menubut;
		if(menu[0] == 'e') {
			kill(ppid);
			return;
		}
		wmlib->titlectl(t, menu);
	}
}

kill(pid: int)
{
	fd := sys->open("#p/" + string pid + "/ctl", sys->OWRITE);
	if (fd != nil)
		sys->fprint(fd, "kill");
}
