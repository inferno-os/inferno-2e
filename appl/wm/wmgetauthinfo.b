implement WmGetauthinfo;

include "sys.m";
	sys: Sys;

include "security.m";
	login: Login;
	ssl: SSL;

include "draw.m";
	draw: Draw;

include "tk.m";
	tk: Tk;

include "wmlib.m";
	wmlib: Wmlib;

include "keyring.m";
	kr: Keyring;

include "string.m";

include "sh.m";

#
# Tk version of getauthinfo command
#
WmGetauthinfo: module 
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

Wm: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

cfg := array[] of {
	"frame .all -borderwidth 2 -relief raised",

	"frame .u",
	"label .u.l -text {User    } -anchor w",
	"entry .u.e",
	"pack .u.l .u.e -side left -in .u -expand 1",
	"bind .u.e <Key-\n> {send cmd u}",
	"focus .u.e",

	"frame .p",
	"label .p.l -text {Password} -anchor w",
	"entry .p.e -show *",
	"pack .p.l .p.e -side left -in .p -expand 1",
	"bind .p.e <Key-\n> {send cmd p}",

	"frame .s",
	"label .s.l -text {Signer  } -anchor w",
	"entry .s.e",
	"pack .s.l .s.e -side left -in .s -expand 1",
	"bind .s.e <Key-\n> {send cmd s}",

	"frame .f",
	"label .f.l -text {Save key} -anchor w",
	"entry .f.e",
	"pack .f.l .f.e -side left -in .f -expand 1",
	"bind .f.e <Key-\n> {send cmd f}",

	"frame .b",
	"radiobutton .b.p -variable save -value p -anchor w -text '" + "Permanent",
	"radiobutton .b.t -variable save -value t -anchor w -text '" + "Temporary",
	"pack .b.p .b.t -side right -in .b -expand 1",
	".b.p invoke",
	"pack .u .p .s .f .b -in .all",
	"pack .Wm_t .all -fill x -expand 1",
	"update"
};

about : con "Generate keys and\n" + 
	    "request certificate for\n" +
	    "mounting remote server";


init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	kr = load Keyring Keyring->PATH;
	str := load String String->PATH;

	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;

	wmlib = load Wmlib Wmlib->PATH;
	wmlib->init();
	(top, titlectl) := wmlib->titlebar(ctxt.screen, wmlib->geom(nil),
		"Service Signing Up", Wmlib->Help);
	wmlib->tkcmds(top, cfg);
	cmd := chan of string;
	tk->namechan(top, cmd, "cmd");

	cs := load Command "/dis/lib/cs.dis";
	if(cs == nil){
		wmlib->dialog(top, "error -fg red", "Error", 
			"Can not load /dis/lib/cs.dis", 0, "Exit"::nil);
		exit;
	}

	login = load Login Login->PATH;
	if(login == nil || login->init() != nil){
		wmlib->dialog(top, "error -fg red", "Error", 
			"Can not load or init " + Login->PATH, 0, "Exit"::nil);
		exit;
	}

	ssl = load SSL SSL->PATH;
	if(ssl == nil){
		wmlib->dialog(top, "error -fg red", "Error", 
			"Can not load " + SSL->PATH, 0, "Exit"::nil);
		exit;
	}

	# push ssl, leave in clear mode for now
	if(sys->bind("#D", "/n/ssl", Sys->MREPL) < 0){
		wmlib->dialog(top, "error -fg red", "Error", 
			"Can not bind #D to /n/ssl", 0, "Exit"::nil);
		exit;
	}

	# start interactive
	usr := user();
	passwd := "";
	signer := login->defaultsigner();
	dir:= "";
	file := "net!";
	path := "";
	tk->cmd(top, ".u.e insert end '" + usr);
	tk->cmd(top, ".s.e insert end '" + signer);
	tk->cmd(top, "update");
	info : ref Keyring->Authinfo;
	for(;;){
		alt {
		menu := <-titlectl =>
			case (menu[0]) {
			'e' =>
				exit;
			'h' =>
				wmlib->dialog(top, "info -fg green", "About", 
				  about, 0, "OK"::nil);
			}
			wmlib->titlectl(top, menu);
		rdy := <-cmd =>
			case (rdy[0]) {
			'u' =>
				usr = tk->cmd(top, ".u.e get");
				if(usr == "")
					tk->cmd(top, "focus .u.e; update");
				else {
					dir = "/usr/" + usr + "/keyring/";
					path = dir + file;
					tk->cmd(top, ".f.e delete 0 end");
					tk->cmd(top, ".f.e insert end '" + path);
					tk->cmd(top, "focus .p.e; update");
				}
				continue;
			'p' =>
				passwd = tk->cmd(top, ".p.e get");	
				if(passwd == "")
					tk->cmd(top, "focus .p.e; update");
				else
					tk->cmd(top, "focus .s.e; update");
				continue;
			's' =>
				signer = tk->cmd(top, ".s.e get");
				if(signer == "")
					tk->cmd(top, "focus .s.e");
				else {
					file = "net!" + signer;
					path = dir + file;
					tk->cmd(top, ".f.e delete 0 end");
					tk->cmd(top, ".f.e insert end " + path);
					tk->cmd(top, "focus .f.e; update");
				}
				continue;
			'f' =>
				path = tk->cmd(top, ".f.e get");
				if(path == "") {
					tk->cmd(top, "focus .f.e; update");
					continue;
				}

				tk->cmd(top, "cursor -bitmap cursor.wait");
				addr := "net!"+signer+"!inflogin";
				(ok, lc) := sys->dial(addr, nil);
				tk->cmd(top, "cursor -default");
				if(ok < 0){
					wmlib->dialog(top, "warning -fg yellow", "Warning", 
						"Server <" + addr + "> not found", 0, "Continue"::nil);
					tk->cmd(top, ".s.e delete 0 end");
					tk->cmd(top, "focus .s.e");
					continue;
				}

				(err, c) := ssl->connect(lc.dfd);
				if(c == nil){
					wmlib->dialog(top, "error -fg red", "Error", 
						"Connection to SSL failed", 0, "Exit"::nil);
					exit;
				}
				lc.dfd = nil;
				lc.cfd = nil;

				# client hello
				agf := "/licensedb/agreement.sig";
				(c, err) = login->chello(usr, agf, c);
				if(err != nil){
					wmlib->dialog(top, "error -fg red", "Error", 
						err, 0, "Exit"::nil);
					tk->cmd(top, ".u.e delete 0 end");
					tk->cmd(top, "focus .u.e");
					continue;
				}
				fd := sys->open(agf, sys->OREAD);
				if(fd == nil){
					wmlib->dialog(top, "error -fg red", "Error", 
						"Can not open " + agf + " for reading", 0, "Exit"::nil);
					exit;
				}
				buf := array[16384] of byte;
				n := sys->read(fd, buf, len buf);
				if(n < 0){
					wmlib->dialog(top, "error -fg red", "Error", 
						"Read " + agf + " failed", 0, "Exit"::nil);
					exit;
				}

				# view agreement doc
				tk->cmd(top, "cursor -default");
				spawn text(ctxt, "Agreement", buf, n);
				buf = nil;

				ans := wmlib->dialog(top, "info -fg green", "About Agreement",
					"By pressing Accept, you are\nbound to the Agreement", 0, "Accept" :: "Decline" :: nil);
				if(ans == 0){
					kr->putstring(c.dfd, "agree");
				}
				else {
					kr->putstring(c.dfd, "don't agree");
					exit;
				}
				
				# start encrypt key exchange
				tk->cmd(top, "cursor -bitmap cursor.wait");			
				(info, err) = login->ckeyx(usr, passwd, c);
				tk->cmd(top, "cursor -default");
				if(info == nil){
					wmlib->dialog(top, "warning -fg yellow", "Warning", 
						err, 0, "Continue"::nil);
					tk->cmd(top, ".p.e delete 0 end");
					tk->cmd(top, "focus .p.e");
					continue;
				}

				# save the info for later access
				save := tk->cmd(top, "variable save");
				(dir, file) = str->splitr(path, "/");
				if(save[0] == 't')
					spawn save2file(dir, file);

				tk->cmd(top, "cursor -default");			
				if(kr->writeauthinfo(path, info) < 0){
					wmlib->dialog(top, "error -fg red", "Error", 
						"Can't write to " + path, 0, "Exit"::nil);
					exit;
				}	
				if(save[0] == 'p')
					wmlib->dialog(top, "info -fg green", "Notice", 
						"Authentication information is\nsaved in file:\n" 
						+ path, 0, "OK"::nil);
				else
					wmlib->dialog(top, "info -fg green", "Notice", 
						"Authentication information is\nheld in a temporary file:\n" 
						+ path, 0, "OK"::nil);

				return;

			} # end-case
		} # end-alt
	} # end-for	
}


text_cfg := array[] of {
	"frame .ft -relief raised -bd 2",
	"text .ft.t -yscrollcommand {.ft.s set} -width 66w -height 18h",
	"scrollbar .ft.s -command {.ft.t yview}",
	"pack .ft.t .ft.s -side left -fill y",
	"pack .ft -fill x",
	"pack propagate . 0",
	"update",
};

text(ctxt: ref Draw->Context, title: string, buf: array of byte, n: int)
{
	(t, menubut) := wmlib->titlebar(ctxt.screen, "-x 5 -y 5", title, 0);
	cmd := chan of string;
	#tk->namechan(t, cmd, "cmd");
	wmlib->tkcmds(t, text_cfg);
	tk->cmd(t, "focus .ft");

	for(i := 0; i < n; i++)
		if(buf[i] == byte '\r')
			buf[i] = byte ' ';

	content := string buf[0:n];

	tk->cmd(t, ".ft.t insert end '" + content);
	tk->cmd(t, "update");

	for(;;){
		menu := <-menubut;
		if(menu[0] == 'e')
			exit;
		wmlib->titlectl(t, menu);
	}
}


user(): string
{
	sys = load Sys Sys->PATH;

	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil)
		return "";

	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0)
		return "";

	return string buf[0:n];	
}

save2file(dir, file: string)
{
	if(sys->bind("#s", dir, Sys->MBEFORE) < 0)
		exit;

	fileio := sys->file2chan(dir, file);
	if(fileio != nil)
		exit;

	data: array of byte;
	off, nbytes, fid: int;
	rc: Sys->Rread;
	wc: Sys->Rwrite;

	infodata := array[0] of byte;

	sys->pctl(Sys->NEWPGRP, nil);

	for(;;) alt {
	(off, nbytes, fid, rc) = <-fileio.read =>
		if(rc == nil)
			break;
		if(off > len infodata){
			rc <-= (infodata[off:off], nil);
		} else {
			if(off + nbytes > len infodata)
				nbytes = len infodata - off;
			rc <-= (infodata[off:off+nbytes], nil);
		}

	(off, data, fid, wc) = <-fileio.write =>
		if(wc == nil)
			break;

		if(off != len infodata){
			wc <-= (0, "cannot be rewritten");
		} else {
			nid := array[len infodata+len data] of byte;
			nid[0:] = infodata;
			nid[len infodata:] = data;
			infodata = nid;
			wc <-= (len data, nil);
		}
		data = nil;
	}
}
