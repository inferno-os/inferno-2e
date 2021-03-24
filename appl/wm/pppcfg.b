#adapted from mailtool.b

implement Pppcfg;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Screen, Display, Image, Context: import draw; #from logon
	ctxt: ref Context; #from logon

include "tk.m";
	tk: Tk;

include "bufio.m";
	bufio: Bufio;
	Iobuf: import Bufio;

include "cfgfile.m";
	cfgfile: CfgFile;

include "sh.m"; #from logon

include "newns.m"; #from logon

include "keyring.m"; #from logon

include "security.m"; #from logon

Wm: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

Pppcfg: module
{
	init:	fn(ctxt: ref Draw->Context, nil: list of string);
};

config_file: con "/services/ppp/pppconfig.txt";

tkcmds(top: ref Tk->Toplevel, a: array of string)
{
	n := len a;
	for(i := 0; i < n; i++)
		v := tk->cmd(top, a[i]);
}

ppp_init(ctxt: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	bufio = load Bufio Bufio->PATH;
	cfgfile = load CfgFile CfgFile->PATH;
	if(sys == nil || draw == nil || tk == nil ||
			bufio == nil || cfgfile == nil)
		return;

	(user, secret, phone, dns, proxy) := get_user_info(ctxt);

	t := tk->toplevel(ctxt.screen,
			" -font /fonts/lucidasans/latin1.7.font" +
			" -borderwidth 2 -relief raised -x 145 -y 0");

	main := chan of string;
	tk->namechan(t, main, "main");

	for(;;) {
		tkcmds(t, mainpanel);

		tk->cmd(t, ".e.user configure -label {" + user + "}");
		slabel: string;
		if(secret != nil)
			slabel = "<password>";
		else
			slabel = "<none>";
		tk->cmd(t, ".e.secret configure -label {" + slabel + "}");
		tk->cmd(t, ".e.phone configure -label {" + phone + "}");
		if(dns != nil)
			slabel = dns;
		else
			slabel = "<none>";
		tk->cmd(t, ".e.dns configure -label {" + slabel + "}");
		if(proxy != nil)
			slabel = proxy;
		else
			slabel = "<none>";
		tk->cmd(t, ".e.proxy configure -label {" + slabel + "}");
		tk->cmd(t, "update");
		s := <-main;
		case s {
			"config" =>
				tk->cmd(t, ". unmap");
				(user1, secret1, phone1, dns1, proxy1) :=
						do_config(ctxt, user, secret, phone, dns, proxy);
				tk->cmd(t, ". map");
				if(user1!=nil && secret1!=nil && phone1!=nil) {
					user = user1;
					secret = secret1;
					phone = phone1;
					dns = dns1;
					proxy = proxy1;
				}
			"dial" =>
				tk->cmd(t, "cursor -bitmap cursor.wait");
				tk->cmd(t, ". unmap");
				ip := connect_ppp(user, secret, phone);
				tk->cmd(t, ". map");
				tk->cmd(t, "cursor -default");
				if(ip != nil) {
					notice(sys->sprint("Connected: local IP address is %s", ip));
					return;
				}
				notice("Dial attempt unsuccessful");
		}
	}
}

good_ip(ip: string): int
{
	(nil, flds) := sys->tokenize(ip, ".");
	if(len flds != 4)
		return 0;
	for(;flds != nil; flds = tl flds)
		if(int hd flds <= 0 || int hd flds > 255)
			return 0;
	return 1;
}

dialpanel := array[] of {
	"pack .Wm_t -side top -fill x ",
	"frame .f",
	"frame .p",
	"frame .t",
	"label .p.phone -bitmap {apps/spin00.bit}",
	"label .t.text -text {Initializing ...} -font /fonts/lucidasans/latin1.8.font",
	"pack .p.phone",
	"pack .t.text",
	"pack .p .t -side left -in .f",
	"pack .f -padx 6 -pady 6",
#	". configure -bd 1 -relief raised",
	"update",
};

tkdialing := ".t.text configure -text {Dialing Service Provider ...}; update";
tkconnecting := ".t.text configure -text {Logging in ...}; update";

connect_ppp(user, secret, phone: string): string
{
	if(user==nil || secret==nil || phone==nil) {
		return nil;
	}
	t := tk->toplevel(ctxt.screen,
			" -font /fonts/lucidasans/latin1.7.font" +
			" -borderwidth 2 -relief raised -x 150 -y 50");
	tkcmds(t, dialpanel);

	spinchan := chan of string;
	spawn spin(t, ".p.phone", spinchan);

	tk->cmd(t, tkdialing);
	ppp_str := sys->sprint("-t 200 -k %s!%s #t/eia1 %s",
			user, secret, phone);

	(nil, ppp_args) := sys->tokenize(ppp_str, " \t\r\n");
	sh := load Command "/dis/svc/telcofs/pppclient.dis";
	if(sh == nil) {
		spinchan <-= "done";
		tk->cmd(t, "destroy .");
		notice("Internal error dialing service provider.");
		return nil;
	}
	sh->init(ctxt, "pppclient"::ppp_args);

	tk->cmd(t, tkconnecting);

	sys->sleep(5000);

	buf := array[1024] of byte;
	for(i:=20; i>0; --i) {
		if((ifd:=sys->open("/net/ipifc", Sys->OREAD)) == nil) {
			spinchan <-= "done";
			tk->cmd(t, "destroy .");
			notice("Internal Error: Unable to open /net/ipifc");
			return nil;
		}
		n := sys->read(ifd, buf, len buf);
		if(n < 1) {
			sys->sleep(1000);
			continue;
		}
		(nflds, flds) := sys->tokenize(string buf[0:n], " \t\r\n");
		if(nflds < 3) {
			sys->sleep(1000);
			continue;
		}
		ip := hd tl tl flds;
		if(good_ip(ip)) {
			spinchan <-= "done";
			tk->cmd(t, "destroy .");
			return ip;
		}
		sys->sleep(1000);
	}
	spinchan <-= "done";
	tk->cmd(t, "destroy .");
	notice("Connection failed.");
	return nil;
}

get_user_info(ctxt: ref Draw->Context):
		(string, string, string, string, string)
{
	#load ppp info from file
	user, secret, phone, dns, proxy: string;
	(user, secret, phone) = load_config(config_file);
	dns = load_dns();
	proxy = load_proxy();

	#load dns info

	if (user != nil && secret != nil && phone != nil)
		return (user, secret, phone, dns, proxy);
    else
		return do_config(ctxt, user, secret, phone, dns, proxy);
}

load_config(fname: string): (string, string, string)
{
	if(
		(sys == nil && (sys = load Sys Sys->PATH) == nil) ||
		(bufio == nil && (bufio = load Bufio Bufio->PATH) == nil)
	)
		return ("", "", "");
	fd := sys->open(fname, Sys->OREAD);
	if(fd == nil)
		return ("", "", "");
	bfd := bufio->fopen(fd, Sys->OREAD);
	if(bfd == nil)
		return ("", "", "");

	line, user, secret, phone: string;

	while((line = bufio->bfd.gett("\r\n")) != nil) {
		(nil, flds) := sys->tokenize(line, "\r\n");
		if(flds == nil)
			break;
		line = hd flds;
		(nil, flds) = sys->tokenize(line, "=");
		if(flds == nil)
			break;
		case hd flds {
			"user" =>
				if(user == nil && len flds > 1)
					user = hd tl flds;
			"secret" =>
				if(secret == nil && len flds > 1)
					secret = hd tl flds;
			"phone" =>
				if(phone == nil && len flds > 1)
					phone = hd tl flds;
			* =>
				#ignore
		}
	}
	return (user, secret, phone);
}

save_config(fname, user, secret, phone, dns, proxy: string): int
{
	save_dns(dns);
	save_proxy(proxy);
	if(sys == nil && (sys = load Sys Sys->PATH) == nil )
		return 0;
	fd := sys->create(fname, Sys->OWRITE, 8r777);
	if(fd == nil)
		return 0;
	sys->fprint(fd, "user=%s\n", user);
	sys->fprint(fd, "secret=%s\n", secret);
	sys->fprint(fd, "phone=%s\n", phone);
	fd = nil;
	if(!sync())
		return 0;
	return 1;
}

load_proxy(): string
{
	proxy: string;

	cfg := cfgfile->init("/services/webget/config");
	if(cfg == nil)
		return nil;
	vals := cfgfile->cfg.getcfg("httpproxy");
	if(vals == nil)
		return nil;
	proxy = hd vals;
	if(proxy == "none")
		proxy = nil;
	return proxy;
}

save_proxy(proxy: string): int
{
	cfg := cfgfile->init("/services/webget/config");
	if(cfg == nil)
		return 0;
	if(proxy == nil)
		proxy = "none";
	cfgfile->cfg.setcfg("httpproxy", proxy);
	cfgfile->cfg.flush();
	if(!sync())
		return 0;
	return 1;
}

load_dns(): string
{
	if((bfd:=bufio->open("/services/dns/db", Sys->OREAD)) == nil)
		return nil;
	line := bufio->bfd.gett("\r\n");
	if(line == nil)
		return nil;
	if(len line > 1)
		line = line[0:len line-1]; #get rid of \r
	else
		return nil;
	return line;
}

save_dns(dns: string): int
{
	if(dns == nil)
		return 1;
	if((fd:=sys->open("/services/dns/db", Sys->ORDWR)) == nil)
		return 0;
	buf := array[1024] of byte; #save first 1K(!) of file
	n := sys->read(fd, buf, len buf);
	lines: list of string;
	if(n > 0) {
		#lines to be saved
		(nil, lines) = sys->tokenize(string buf[0:n], "\r\n");
	}
	l: list of string;
	for(; lines!=nil; lines = tl lines)
		if(hd lines != dns)
			l = hd lines :: l;
	for(lines = nil; l!=nil; l = tl l)
		lines = hd l :: lines;
	#replace first entry
	if(lines != nil)
		lines = dns :: tl lines;
	else
		lines = dns :: nil;
	sys->seek(fd, 0, Sys->SEEKSTART);
	for(; lines != nil; lines = tl lines)
		sys->fprint(fd, "%s\n", hd lines);
	fd = nil;
	return 1;
}

mainpanel := array[] of {
	"pack .Wm_t -side top -fill x ",
	"frame .i",
	"frame .f",
	"frame .b",
	"frame .l",
	"frame .e",
	"label .i.instruct  -bitmap {../logon.bit} -bd 1 -relief sunken",
#	"label .i.instruct  -text {Welcome to Inferno} -font /fonts/lucidasans/latin1.8.font",
	"pack .i.instruct -fill x",
	"label .l.user -text {Username:}  -anchor e -font /fonts/lucidasans/latin1.8.font",
	"label .l.secret -text {Password:}  -anchor e -font /fonts/lucidasans/latin1.8.font",
	"label .l.phone -text {Phone Number:}  -anchor e -font /fonts/lucidasans/latin1.8.font",
	"label .l.dns -text {DNS Address:}  -anchor e -font /fonts/lucidasans/latin1.8.font",
	"label .l.proxy -text {HTTP Proxy:}  -anchor e -font /fonts/lucidasans/latin1.8.font",
	"pack .l.user .l.secret .l.phone .l.dns .l.proxy -fill both -pady 3",
	"label .e.user",
	"label .e.secret",
	"label .e.phone",
	"label .e.dns",
	"label .e.proxy",
	"pack .e.user .e.secret .e.phone .e.dns .e.proxy -fill both -pady 4",
	"pack .l .e -side left -in .f",
	"frame .b.default -relief sunken -bd 1 ",
#	"button .b.config -text {Configure} -width 10w  -command {send main config}",
#	"button .b.dial -text {Dial} -width 10w  -command {send main dial}",
	"button .b.config -bitmap {../register.bit} -command {send main config}",
	"button .b.dial -bitmap {../phone.bit} -command {send main dial}",
	"pack .b.default -side right -padx 8 -pady 8 -anchor e",
	"pack .b.config -side right -padx 8",
	"pack .b.dial -in .b.default -side right -padx 4 -pady 4",
	"pack .i -fill x -padx 6 -pady 6",
	"pack .f -fill both -padx 6 -pady 6",
	"pack .b -fill x -padx 6",
	"focus .e.pop",
	". configure -bd 1 -relief raised -width 320 -height 240",
	"update",
};

#registration panel
regpanel := array[] of {
	"pack .Wm_t -side top -fill x ",
	"frame .i",
	"frame .f",
	"frame .b",
	"frame .l",
	"frame .e",
	"label .i.pic -bitmap {../register.bit}",
	"label .i.instruct -text {Please enter PPP configuration information.} -font /fonts/lucidasans/latin1.8.font",
	"pack .i.pic .i.instruct -fill x",
	"label .l.user -text {Username:}  -anchor e",
	"label .l.secret -text {Password:}  -anchor e",
	"label .l.phone -text {Phone Number:}  -anchor e",
	"label .l.dns -text {DNS Address:}  -anchor e",
	"label .l.proxy -text {HTTP Proxy:}  -anchor e",
	"pack .l.user .l.secret .l.phone .l.dns .l.proxy -fill both -pady 4",
	"entry .e.user",
	"entry .e.secret -show *",
	"entry .e.phone",
	"entry .e.dns",
	"entry .e.proxy",
	"pack .e.user .e.secret .e.phone .e.dns .e.proxy -fill both",
	"bind .e.user <Key-\n> {focus .e.secret}",
	"bind .e.secret <Key-\n> {focus .e.phone}",
	"bind .e.phone <Key-\n> {focus .e.dns}",
	"bind .e.dns <Key-\n> {focus .e.proxy}",
	"bind .e.proxy <Key-\n> {send reg ok}",
	"pack .l .e -side left -in .f",
	"frame .b.default -relief sunken -bd 1 ",
	"button .b.cancel -text {Cancel} -width 10w  -command {send reg cancel}",
	"button .b.save -text {Save} -width 10w  -command {send reg save}",
	"button .b.ok -text {OK} -width 10w  -command {send reg ok}",
#	"pack .b.ok .b.save .b.cancel -side right -padx 8 -anchor e",
	"pack .b.default -side right -padx 8 -pady 8 -anchor e",
	"pack .b.cancel -side right -padx 8 -anchor e",
	"pack .b.save -side right -padx 8",
	"pack .b.ok -in .b.default -side right -padx 4 -pady 4",
	"pack .i -fill x -padx 6 -pady 6",
	"pack .f -fill both -padx 6 -pady 6",
	"pack .b -fill x -padx 6",
	"focus .e.user",
	". configure -bd 1 -relief raised -width 320 -height 240",
	"update",
};

do_config(ctxt: ref Draw->Context, user, secret, phone, dns, proxy: string):
		(string, string, string, string, string)
{
	t := tk->toplevel(ctxt.screen,
			" -font /fonts/lucidasans/latin1.7.font" +
			" -borderwidth 2 -relief raised -x 150 -y 0");

	reg := chan of string;
	tk->namechan(t, reg, "reg");

	tkcmds(t, regpanel);

	#load current values into edit boxes
	tk->cmd(t, ".e.user insert 0  {" + user + "}");
	tk->cmd(t, ".e.secret insert 0 {" + secret + "}");
	tk->cmd(t, ".e.phone insert 0 {" + phone + "}");
	tk->cmd(t, ".e.dns insert 0 {" + dns + "}");
	tk->cmd(t, ".e.proxy insert 0 {" + proxy + "}");

	user1, secret1, phone1, dns1, proxy1: string;
FOR: for(;;) {
		tk->cmd(t, "update");
		s := <-reg;
	    if (s=="save" || s=="ok") {
			#should do error checking of entered values
			user1 = tk->cmd(t, ".e.user get");
			secret1 = tk->cmd(t, ".e.secret get");
			phone1 = tk->cmd(t, ".e.phone get");
			dns1 = tk->cmd(t, ".e.dns get");
			proxy1 = tk->cmd(t, ".e.proxy get");
			if(user1 == nil) {
				notice("Blank username");
				tk->cmd(t, "focus .e.user");
				continue FOR;
			}
			else if(secret1 == nil) {
				notice("Blank password");
				tk->cmd(t, "focus .e.secret");
				continue FOR;
			}
			else if(phone1 == nil) {
				notice("Blank phone number");
				tk->cmd(t, "focus .e.phone");
				continue FOR;
			}
			if(s=="save") {
				if(!save_config(config_file, user1, secret1, phone1,
						dns1, proxy1))
					notice(sys->sprint("Problem while saving:\n%r"));
				else
					notice("Information saved");
			}
			else { #ok
				tk->cmd(t, "destroy .");
				return (user1, secret1, phone1, dns1, proxy1);
			}
	    }
		else if(s=="cancel")
			return (user, secret, phone, dns, proxy);
		else
			return (user1, secret1, phone1, dns1, proxy1); #Why not?
	}
}

sync(): int
{
	if((fd := sys->open("#Kcons/kfscons", Sys->OWRITE)) != nil) {
		sys->fprint(fd, "sync\n");
		return 1;
	}
	return 0;
}

#
# Logon program for Wm environment
#
kfd:	ref Sys->FD;

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

init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;

	sys->pctl(sys->NEWPGRP, nil);

	mfd := sys->open("/dev/pointer", sys->OREAD);
	if(mfd == nil) {
		sys->print("open: /dev/pointer: %r\n");
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

	spawn mouse(ctxt.screen, mfd);
	spawn keyboard(ctxt.screen);

	spid := string sys->pctl(0, nil);
	kfd = sys->open("#p/"+spid+"/ctl", sys->OWRITE);
	if(kfd == nil) {
		notice("error opening pid "+
			spid+"\n"+
			sys->sprint("%r"));
	}
#	t := tk->toplevel(ctxt.screen, "-x 50 -y 50");
#
#	cmd := chan of string;
#	tk->namechan(t, cmd, "cmd");
#
#	for(i := 0; i < len cfg; i++)
#		tk->cmd(t, cfg[i]);
#
#	err := tk->cmd(t, "variable lasterr");
#	if(err != nil)
#		sys->print("logon: tk error: %s\n", err);

	ppp_init(ctxt, nil);
	logon("inferno");

#	tk->cmd(t, "cursor -bitmap cursor.wait");

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

	cs := load Command "/dis/lib/cs.dis";
	if(cs == nil)
		notice("Failed to start connection server");

	charon := load Wm "/dis/wm/chstart.dis";
	if(charon == nil)
		notice("Failed to load web browser");

	sys->fprint(kfd, "killgrp");
	sys->pctl(sys->NEWFD, 0 :: 1 :: 2 :: nil);

	profile(ctxt);

#	tk->cmd(t, "cursor -default");

	if(cs != nil)
		cs->init(nil, "cs"::nil);
	if(charon != nil)
		spawn charon->init(ctxt, "chstart"::nil);
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

#	license(user);

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

keyboard(s: ref Draw->Screen)
{
	dfd := sys->open("/dev/keyboard", sys->OREAD);
	if(dfd == nil)
		return;

	b:= array[1] of byte;
	buf := array[10] of byte;
	i := 0;
	for(;;) {
		n := sys->read(dfd, buf[i:], len buf - i);
		if(n < 1)
			break;
		i += n;
		while(i >0 && (nutf := sys->utfbytes(buf, i)) > 0){
			str := string buf[0:nutf];
			tk->keyboard(s, int str[0]);
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

spin(t: ref Tk->Toplevel, tkvar: string, spinchan: chan of string)
{
	c := chan of int;
	spawn timer(25,c);
	tp := <- c;

	# Wait for exit while spinning
	frame := 1;
	for (;;) alt {
	menu := <- spinchan => {
		killspin(tp);
		tk->cmd(t, tkvar+" configure -bitmap apps/spin00.bit;update");
                return;
        }
        <- c => {
		s := sys->sprint("%s configure -bitmap apps/spin%.2d.bit; update", tkvar, frame % 16);
		tk->cmd(t,s);
		++frame;
        }
    }
}
 
killspin(pid:int)
{
    fd := sys->open("#p/"+string pid+"/ctl",sys->OWRITE);
    if(fd == nil)
	return;
    sys->fprint(fd,"kill");
}
 
timer(n: int, c: chan of int)
{
	c <-= sys->pctl(0,nil);
	for (;;) {
		c <-= sys->pctl(0,nil);
		sys->sleep(n);
	}
}
