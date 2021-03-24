# minimum wmsh - obc
implement MwmSh;

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

include "tkwhset.m";

MwmSh: module
{
	init:	fn(ctxt: ref Draw->Context, args: list of string);
};

Command: module
{
	init:	fn(ctxt: ref Draw->Context, args: list of string);
};

BS:		con 8;		# ^h backspace character
BSW:		con 23;		# ^w bacspace word
BSL:		con 21;		# ^u backspace line
EOT:		con 4;		# ^d end of file
ESC:		con 27;		# hold mode

HIWAT:	con 2000;	# maximum number of lines in transcript
LOWAT:	con 1500;	# amount to reduce to after high water

Name:	con "Shell";

Rdreq: adt
{
	off:	int;
	nbytes:	int;
	fid:	int;
	rc:	chan of (array of byte, string);
};

shwin_cfg := array[] of {
	"frame .ft",
	"scrollbar .ft.scroll -command {.ft.t yview}",
	"text .ft.t -width 10c -height 7c -yscrollcommand {.ft.scroll set}",
	"pack .ft.scroll -side left -fill y",
	"pack .ft.t -fill both -expand 1",
	"pack .Wm_t -fill x",
	"pack .ft -fill both -expand 1",
	"pack propagate . 0",
	"focus .ft.t",
	"bind .ft.t <Key> {send keys {%A}}",
	"bind .ft.t <Control-d> {send keys {%A}}",
	"bind .ft.t <Control-h> {send keys {%A}}",
	"bind .ft.t <Button-1> +{grab set .ft.t}",
	"bind .ft.t <Double-Button-1> +{grab set .ft.t}",
	"bind .ft.t <ButtonRelease-1> +{grab release .ft.t}",
	"bind .ft.t <Motion-Button-2-Button-1> {}",
	"bind .ft.t <Motion-ButtonPress-2> {}",
	"bind .ft.t <Motion-Button-3> {}",
	"bind .ft.t <Motion-Button-3-Button-1> {}",
	"bind .ft.t <Double-Button-3> {}",
	"bind .ft.t <Double-ButtonRelease-3> {}",
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

	sys->pctl(Sys->FORKNS | Sys->NEWPGRP, nil);

	wmlib->init();

	tkargs := "";
	if (argv == nil)
  	  sys->print("MwmSh: missing argument to init\n");
	else
	  argv = tl argv;
	if(argv != nil) {
		tkargs = hd argv;
		argv = tl argv;
	}
        t := tk->toplevel(ctxt.screen,
             tkargs+" -font /fonts/lucidasans/latin1.7.font -bd 2 -relief raised");
	titlectl := chan of string;
	edit := chan of string;
	tk->namechan(t, edit, "edit");

	if ((wh := load Tkwh Tkwh->PATH) != nil)
	  shwin_cfg = wh->tkwhset(shwin_cfg, "text .ft.t -width", tkargs);

	wmlib->tkcmds(t, shwin_cfg);

	ioc := chan of (int, ref FileIO);
	spawn newsh(ctxt, ioc);

	(pid, file) := <-ioc;
	if(file == nil) {
		sys->print("newsh: %r\n");
		return;
	}

	keys := chan of string;
	tk->namechan(t, keys, "keys");

	rdrpc: Rdreq;

	# outpoint is place in text to insert characters printed by programs
	tk->cmd(t, ".ft.t mark set outpoint end; .ft.t mark gravity outpoint left");

	for(;;) alt {
	menu := <-titlectl =>
		if(menu[0] == 'e') {
			tk->cmd(t, "destroy .");
			kill(pid);
			return;
		}
		wmlib->titlectl(t, menu);
		tk->cmd(t, "focus .ft.t");

	c := <-keys =>
		char := c[1];
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
			spawn(e(titlectl));
		}

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

e(titlectl : chan of string)
{
  titlectl <-= "e";
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

newsh(ctxt: ref Context, ioc: chan of (int, ref FileIO))
{
	pid := sys->pctl(sys->NEWFD, nil);
	sh := load Command "/dis/sh.dis";
	if(sh == nil) {
		ioc <-= (0, nil);
		return;
	}

	tty := "cons."+string pid;

	sys->bind("#s","/chan",sys->MBEFORE);
	fio := sys->file2chan("/chan", tty);
	ioc <-= (pid, fio);
	if(fio == nil)
		return;

	sys->bind("/chan/"+tty, "/dev/cons", sys->MREPL);

	fd0 := sys->open("/dev/cons", sys->OREAD|sys->ORCLOSE);
	fd1 := sys->open("/dev/cons", sys->OWRITE);
	fd2 := sys->open("/dev/cons", sys->OWRITE);

	sh->init(ctxt, "sh" :: "-n" :: nil);
}

kill(pid: int)
{
	fd := sys->open("#p/"+string pid+"/ctl", sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "killgrp");
}
