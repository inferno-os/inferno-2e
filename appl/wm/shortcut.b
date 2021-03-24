implement WmShortcut;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;

include "tk.m";
	tk: Tk;

WmShortcut: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

Wm: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

Maxsetup:	con 4096;
stderr: ref Sys->FD;

Shortcut: adt
{
	icon:	string;
	name:	string;
	x, y:	int;
	cmd:	string;
	t:	ref Tk->Toplevel;
};

Font:	con "-font /fonts/misc/latin1.8x13.font";

tops := array[] of {
	"bind .i <Button-1> { grab set .i }",
	"bind .i <Motion-Button-1> {. configure -x %X -y %Y}",
	"bind .i <ButtonRelease> { grab release .i}",
	"bind .l <Button-1> { grab set .l }",
	"bind .l <Motion-Button-1> {. configure -x %X -y %Y}",
	"bind .l <ButtonRelease> { grab release .l}",
	"pack .i .l",
	"lower .",
	"update"
};

init(ctxt: ref Draw->Context, nil: list of string)
{
	sys  = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk   = load Tk Tk->PATH;

	stderr = sys->fildes(2);

	com := chan of string;

	cuts := readsetup();
	drag := array[len cuts] of chan of string;

	for(i := 0; i < len cuts; i++) {
		s := cuts[i];
		tkarg := sys->sprint("-x %d -y %d -bg #a1c3d1 %s", s.x, s.y, Font);
		s.t = tk->toplevel(ctxt.screen, tkarg);
		drag[i] = chan of string;
		tk->namechan(s.t, drag[i], "Wm_drag");
		tk->namechan(s.t, com, "com");
		tk->cmd(s.t, "label .i -bitmap "+s.icon);
		tk->cmd(s.t, "label .l -text "+s.name);
		action := "<Double-Button-2> { send com "+string i+"}";
		tk->cmd(s.t, "bind .i "+action);
		tk->cmd(s.t, "bind .l "+action);
		for(b := 0; b < len tops; b++)
			tk->cmd(s.t, tops[b]);
	}

	cmd: string;
	args: list of string;

	for(;;) {
		alt {
		s := cuts[int <-com] =>
			(cmd, args) = cmdline(s, "");
		(x, s) := <-drag =>
			(cmd, args) = cmdline(cuts[x], s);
		}
		wm := load Wm cmd+".dis";
		if(wm != nil)
			spawn applinit(wm, ctxt, args);
		else
			sys->fprint(stderr, "shortcut: load %s: %r\n", cmd);
		wm = nil;
	}
}

applinit(mod: Wm, ctxt: ref Draw->Context, args: list of string)
{
	sys->pctl(sys->NEWPGRP, nil);
	spawn mod->init(ctxt, args);
}

cmdline(s: ref Shortcut, drag: string): (string, list of string)
{
	cmd := s.cmd;
	line := "";
	p := 0;

	# Substitute the % formats
	#
	for(i := 0; i < len cmd; i++) {
		if(cmd[i] != '%') {
			line[p++] = cmd[i];
			continue;
		}
		i++;
		if(i > len cmd)
			break;
		case cmd[i] {
		'%' =>
			line[p++] = '%';
		'x' =>
			line += tk->cmd(s.t, ". cget -x");
			p = len line;
		'y' =>
			line += tk->cmd(s.t, ". cget -y");
			p = len line;
		'n' =>
			line += s.name;
			p = len line;
		'f' or
		'd' =>
			if(drag == nil)
				break;
			if(cmd[i] == 'f') {
				for(j := 0; j < len drag; j++) {
					if(drag[j] == '=') {
						drag = drag[j+1:];
						break;
					}
				}
			}
			line += drag;
			p = len line;
		}
	}

	# Turn it into a command line arg list
	#
	word := "";
	args: list of string;
	while(len line != 0) {
		line = skipbl(line);
		if(len line == 0)
			break;
		(line, word) = getword(line);
		args = word :: args;		
	}
	args = rev(args);
	cmd = "";
	if(args != nil)
		cmd = hd args;

	return (cmd, args);
}

getword(s: string): (string, string)
{
	l := len s;
	if(s[0] == '\'') {
		i := 1;
		while(i < l && s[i] != '\'')
			i++;
		return (s[i+1:], s[1:i]);
	}
	i := 0;
	while(i < l && s[i] != ' ' && s[i] != '\t')
		i++;
	return (s[i:], s[0:i]);
}

skipbl(s: string): string
{
	i := 0;
	l := len s;
	while(i < l && (s[i] == ' ' || s[i] == '\t'))
		i++;
	return s[i:];
}

rev(l: list of string): list of string
{
	t: list of string;
	while(l != nil) {
		t = hd l :: t;
		l = tl l;
	}
	return t;
}

readsetup(): array of ref Shortcut
{
	cuts: list of ref Shortcut;

	fd := sys->open("shortcut", sys->OREAD);
	if(fd == nil)
		return nil;

	buf := array[Maxsetup] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) {
		sys->fprint(stderr, "error reading shortcut\n");
		return nil;
	}
	if(n >= len buf) {
		sys->fprint(stderr, "shortcut file is too big\n");
		return nil;
	}

	(nline, line) := sys->tokenize(string buf[0:n], "\r\n");
	while(line != nil) {
		s := hd line;
		line = tl line;
		if(s[0] == '#')
			continue;
		(nfield, field) := sys->tokenize(s, ":");
		if(nfield != 4) {
			sys->fprint(stderr, "error parsing shortcut file\n");
			continue;
		}

		sc := ref Shortcut;
		sc.icon = hd field;
		field = tl field;

		sc.name = hd field;
		field = tl field;

		(nposn, posn) := sys->tokenize(hd field, ",");
		if(nposn != 2) {
			sys->fprint(stderr, "error parsing shortcut file");
			continue;
		}
		field = tl field;

		sc.x = int hd posn;
		posn = tl posn;
		sc.y = int hd posn;

		sc.cmd = hd field;

		cuts = sc :: cuts;
	}

	if(cuts == nil)
		return nil;

	acuts := array[len cuts] of ref Shortcut;
	i := 0;
	while(cuts != nil) {
		acuts[i++] = hd cuts;
		cuts = tl cuts;
	}
	return acuts;
}
