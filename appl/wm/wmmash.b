implement WmMash;

include "sys.m";
	sys: Sys;
	FileIO: import sys;

include "draw.m";
	draw: Draw;
	Context: import draw;

include "tk.m";
	tk: Tk;

include "wmlib.m";
	wmlib: Wmlib;

include	"plumbmsg.m";
	plumbmsg: Plumbmsg;
	Msg: import plumbmsg;

include "workdir.m";
	workdir: Workdir;

include "tkwhset.m";

WmMash: module
{
	init:	fn(ctxt: ref Draw->Context, args: list of string);
};

Command: module
{
	tkinit:	fn(ctxt: ref Draw->Context, t: ref Tk->Toplevel, args: list of string);
};

BS:		con 8;		# ^h backspace character
BSW:		con 23;		# ^w bacspace word
BSL:		con 21;		# ^u backspace line
EOT:		con 4;		# ^d end of file
ESC:		con 27;		# hold mode

HIWAT:	con 2000;	# maximum number of lines in transcript
LOWAT:	con 1500;	# amount to reduce to after high water

Name:	con "Mash";

Rdreq: adt
{
	off:	int;
	nbytes:	int;
	fid:	int;
	rc:	chan of (array of byte, string);
};

shwin_cfg := array[] of {
	"menu .m",
	".m add command -text Cut -command {send edit cut}",
	".m add command -text Paste -command {send edit paste}",
	".m add command -text Snarf -command {send edit snarf}",
	".m add command -text Send -command {send edit send}",
	"frame .b",
	"frame .ft",
	"scrollbar .ft.scroll -command {.ft.t yview}",
	"text .ft.t -width 10c -height 7c -yscrollcommand {.ft.scroll set}",
	"pack .ft.scroll -side left -fill y",
	"pack .ft.t -fill both -expand 1",
	"pack .Wm_t -fill x",
	"pack .b -anchor w",
	"pack .ft -fill both -expand 1",
	"pack propagate . 0",
	"focus .ft.t",
	"bind .ft.t <Key> {send keys {%A}}",
	"bind .ft.t <Control-d> {send keys {%A}}",
	"bind .ft.t <Control-h> {send keys {%A}}",
	"bind .ft.t <Button-1> +{grab set .ft.t; send but1 pressed}",
	"bind .ft.t <Double-Button-1> +{grab set .ft.t; send but1 pressed}",
	"bind .ft.t <ButtonRelease-1> +{grab release .ft.t; send but1 released}",
	"bind .ft.t <ButtonPress-2> {send but2 %X %Y}",
	"bind .ft.t <Motion-Button-2-Button-1> {}",
	"bind .ft.t <Motion-ButtonPress-2> {}",
	"bind .ft.t <ButtonPress-3> {send but3 pressed}",
	"bind .ft.t <ButtonRelease-3> {send but3 released %x %y}",
	"bind .ft.t <Motion-Button-3> {}",
	"bind .ft.t <Motion-Button-3-Button-1> {}",
	"bind .ft.t <Double-Button-3> {}",
	"bind .ft.t <Double-ButtonRelease-3> {}",
	"bind .m <ButtonRelease> {.m tkMenuButtonUp %x %y}",
	"update"
};

rdreq: list of Rdreq;
menuindex := "0";
holding := 0;
plumbed := 0;

init(ctxt: ref Context, argv: list of string)
{
	s: string;

	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;
	plumbmsg = load Plumbmsg Plumbmsg->PATH;

	sys->pctl(Sys->FORKNS | Sys->NEWPGRP, nil);

	wmlib->init();

	if(plumbmsg->init(1, nil, 0) >= 0){
		plumbed = 1;
		workdir = load Workdir Workdir->PATH;
	}

	tkargs := "";
	argv = tl argv;
	if(argv != nil) {
		tkargs = hd argv;
		argv = tl argv;
	}
	(t, titlectl) := wmlib->titlebar(ctxt.screen, tkargs, Name, Wmlib->Appl);

	# skip over the '/' that gets passed to us
	if(argv != nil)
		argv = tl argv;

	edit := chan of string;
	tk->namechan(t, edit, "edit");
#	mash := chan of string;
#	tk->namechan(t, mash, "mash");

	if ((wh := load Tkwh Tkwh->PATH) != nil)
	  shwin_cfg = wh->tkwhset(shwin_cfg, "text .ft.t -width", tkargs);

	wmlib->tkcmds(t, shwin_cfg);

	ioc := chan of (int, ref FileIO);
	spawn newsh(ctxt, t, ioc, argv);

	(pid, file) := <-ioc;
	if(file == nil) {
		sys->print("newsh: %r\n");
		return;
	}

	keys := chan of string;
	tk->namechan(t, keys, "keys");

	but1 := chan of string;
	tk->namechan(t, but1, "but1");
	but2 := chan of string;
	tk->namechan(t, but2, "but2");
	but3 := chan of string;
	tk->namechan(t, but3, "but3");
	button1 := 0;
	button3 := 0;

	rdrpc: Rdreq;

	# outpoint is place in text to insert characters printed by programs
	tk->cmd(t, ".ft.t mark set outpoint end; .ft.t mark gravity outpoint left");

	for(;;) alt {
	menu := <-titlectl =>
		if(menu[0] == 'e') {
			kill(pid);
			return;
		}
		wmlib->titlectl(t, menu);
		tk->cmd(t, "focus .ft.t");

	ecmd := <-edit =>
		editor(t, ecmd);
		sendinput(t);
		tk->cmd(t, "focus .ft.t");

	c := <-keys =>
		cut(t, 1);
		char := c[1];
		if(char == '\\')
			char = c[2];
		update := ";.ft.t see insert;update";
		case char {
		* =>
			tk->cmd(t, ".ft.t insert insert "+c+update);
		'\n' or EOT =>
			tk->cmd(t, ".ft.t insert insert "+c+update);
			sendinput(t);
		BS =>
			tk->cmd(t, ".ft.t tkTextDelIns -c"+update);
		BSL =>
			tk->cmd(t, ".ft.t tkTextDelIns -l"+update);
		BSW =>
			tk->cmd(t, ".ft.t tkTextDelIns -w"+update);
		ESC =>
			holding ^= 1;
			color := "blue";
			if(!holding){
				color = "black";
				wmlib->taskbar(t, Name);
				sendinput(t);
			}else
				wmlib->taskbar(t, Name+" (holding)");
			tk->cmd(t, ".ft.t configure -foreground "+color+update);
		}

	c := <-but1 =>
		button1 = (c == "pressed");
		button3 = 0;	# abort any pending button 3 action

	c := <-but2 =>
		if(button1){
			cut(t, 1);
			tk->cmd(t, "update");
			break;
		}
		(nil, l) := sys->tokenize(c, " ");
		x := int hd l - 50;
		y := int hd tl l - int tk->cmd(t, ".m yposition "+menuindex) - 10;
		tk->cmd(t, ".m activate "+menuindex+"; .m post "+string x+" "+string y+
			"; grab set .m; update");
		button3 = 0;	# abort any pending button 3 action

	c := <-but3 =>
		if(c == "pressed"){
			button3 = 1;
			if(button1){
				paste(t);
				tk->cmd(t, "update");
			}
			break;
		}
		if(plumbed == 0 || button3 == 0 || button1 != 0)
			break;
		button3 = 0;
		# Plumb message triggered by release of button 3
		(nil, l) := sys->tokenize(c, " ");
		x := int hd tl l;
		y := int hd tl tl l;
		index := tk->cmd(t, ".ft.t index @"+string x+","+string y);
		selindex := tk->cmd(t, ".ft.t tag ranges sel");
		if(selindex != "")
			insel := tk->cmd(t, ".ft.t compare sel.first <= "+index)=="1" &&
				tk->cmd(t, ".ft.t compare sel.last >= "+index)=="1";
		else
			insel = 0;
		attr := "";
		if(insel)
			text := tk->cmd(t, ".ft.t get sel.first sel.last");
		else{
			# have line with text in it
			# now extract whitespace-bounded string around click
			(nil, w) := sys->tokenize(index, ".");
			charno := int hd tl w;
			left := tk->cmd(t, ".ft.t index {"+index+" linestart}");
			right := tk->cmd(t, ".ft.t index {"+index+" lineend}");
			line := tk->cmd(t, ".ft.t get "+left+" "+right);
			for(i:=charno; i>0; --i)
				if(line[i-1]==' ' || line[i-1]=='\t')
					break;
			for(j:=charno; j<len line; j++)
				if(line[j]==' ' || line[j]=='\t')
					break;
			text = line[i:j];
			attr = "click="+string (charno-i);
		}
		msg := ref Msg(
			"WmSh",
			"",
			workdir->init(),
			"text",
			attr,
			array of byte text);
		if(msg.send() < 0)
			sys->fprint(sys->fildes(2), "sh: plumbing write error: %r\n");

	rdrpc = <-file.read =>
		if(rdrpc.rc == nil)
			return;
		append(rdrpc);
		sendinput(t);

	(off, data, fid, wc) := <-file.write =>
		if(wc == nil)
			return;
		cdata := string data;
		ncdata := string len cdata + "chars;";
		moveins := insat(t, "outpoint");
		tk->cmd(t, ".ft.t insert outpoint '"+ cdata);
		wc <-= (len data, nil);
		data = nil;
		s = ".ft.t mark set outpoint outpoint+" + ncdata;
		s += ".ft.t see outpoint;";
		if(moveins)
			s += ".ft.t mark set insert insert+" + ncdata;
		s += "update";
		tk->cmd(t, s);
		nlines := int tk->cmd(t, ".ft.t index end");
		if(nlines > HIWAT){
			s = ".ft.t delete 1.0 "+ string (nlines-LOWAT) +".0;update";
			tk->cmd(t, s);
		}
	}
}

RPCread: type (int, int, int, chan of (array of byte, string));

append(r: RPCread)
{
	t := r :: nil;
	while(rdreq != nil) {
		t = hd rdreq :: t;
		rdreq = tl rdreq;
	}
	rdreq = t;
}

insat(t: ref Tk->Toplevel, mark: string): int
{
	return tk->cmd(t, ".ft.t compare insert == "+mark) == "1";
}

insininput(t: ref Tk->Toplevel): int
{
	if(tk->cmd(t, ".ft.t compare insert >= outpoint") != "1")
		return 0;
	return tk->cmd(t, ".ft.t compare {insert linestart} == {outpoint linestart}") == "1";
}

isalnum(s: string): int
{
	if(s == "")
		return 0;
	c := s[0];
	if('a' <= c && c <= 'z')
		return 1;
	if('A' <= c && c <= 'Z')
		return 1;
	if('0' <= c && c <= '9')
		return 1;
	if(c == '_')
		return 1;
	if(c > 16rA0)
		return 1;
	return 0;
}

editor(t: ref Tk->Toplevel, ecmd: string)
{
	s, snarf: string;

	case ecmd {
	"cut" =>
		menuindex = "0";
		cut(t, 1);
	
	"paste" =>
		menuindex = "1";
		paste(t);

	"snarf" =>
		menuindex = "2";
		if(tk->cmd(t, ".ft.t tag ranges sel") == "")
			break;
		snarf = tk->cmd(t, ".ft.t get sel.first sel.last");
		wmlib->snarfput(snarf);

	"send" =>
		menuindex = "3";
		if(tk->cmd(t, ".ft.t tag ranges sel") != ""){
			snarf = tk->cmd(t, ".ft.t get sel.first sel.last");
			wmlib->snarfput(snarf);
		}else
			snarf = wmlib->snarfget();
		if(snarf != "")
			s = snarf;
		else
			return;
		if(s[len s-1] != '\n' && s[len s-1] != EOT)
			s[len s] = '\n';
		tk->cmd(t, ".ft.t see end; .ft.t insert end '"+s);
		tk->cmd(t, ".ft.t mark set insert end");
		tk->cmd(t, ".ft.t tag remove sel sel.first sel.last");
	}
	tk->cmd(t, "update");
}

cut(t: ref Tk->Toplevel, snarfit: int)
{
	if(tk->cmd(t, ".ft.t tag ranges sel") == "")
		return;
	if(snarfit)
		wmlib->snarfput(tk->cmd(t, ".ft.t get sel.first sel.last"));
	tk->cmd(t, ".ft.t delete sel.first sel.last");
}

paste(t: ref Tk->Toplevel)
{
	snarf := wmlib->snarfget();
	if(snarf == "")
		return;
	cut(t, 0);
	tk->cmd(t, ".ft.t insert insert '"+snarf);
	tk->cmd(t, ".ft.t tag add sel insert-"+string len snarf+"chars insert");
	sendinput(t);
}

sendinput(t: ref Tk->Toplevel)
{
	if(holding)
		return;
	input := tk->cmd(t, ".ft.t get outpoint end");
	slen := len input;
	if(slen == 0 || rdreq == nil)
		return;

	r := hd rdreq;
	for(i := 0; i < slen; i++)
		if(input[i] == '\n' || input[i] == EOT)
			break;

	if(i >= slen && slen < r.nbytes)
		return;

	if(i > r.nbytes)
		i = r.nbytes;
	advance := string (i+1);
	if(input[i] == EOT)
		input = input[0:i];
	else
		input = input[0:i+1];

	rdreq = tl rdreq;

	alt {
	r.rc <-= (array of byte input, "") =>
		tk->cmd(t, ".ft.t mark set outpoint outpoint+" + advance + "chars");
	* =>
		# requester has disappeared; ignore his request and try again
		sendinput(t);
	}
}

newsh(ctxt: ref Context, t: ref Tk->Toplevel, ioc: chan of (int, ref FileIO),
		args: list of string)
{
	pid := sys->pctl(sys->NEWFD, nil);

	sh := load Command "/dis/mash.dis";
	if(sh == nil) {
		ioc <-= (0, nil);
		return;
	}

	tty := "cons."+string pid;

	fio := sys->file2chan("/chan", tty);

	ioc <-= (pid, fio);
	if(fio == nil)
		return;

	sys->bind("/chan/"+tty, "/dev/cons", sys->MREPL);

	fd0 := sys->open("/dev/cons", sys->OREAD|sys->ORCLOSE);
	fd1 := sys->open("/dev/cons", sys->OWRITE);
	fd2 := sys->open("/dev/cons", sys->OWRITE);

	sh->tkinit(ctxt, t, "mash" :: args);
}

kill(pid: int)
{
	fd := sys->open("#p/"+string pid+"/ctl", sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "killgrp");
}
