# get a certificate from a signer server and stores it in a file
implement Getauthinfo;

include "sys.m";
	sys: Sys;
	stdin, stdout, stderr: ref Sys->FD;

include "draw.m";

include "keyring.m";
	kr: Keyring;

include "security.m";
	login: Login;
	ssl: SSL;

include "string.m";

include "promptstring.b";

Getauthinfo: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

usage := "Usage: getauthinfo net!mach or getauthinfo default";

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stdin = sys->fildes(0);
	stdout = sys->fildes(1);
	stderr = sys->fildes(2);

	# Disable echoing in RAWON mode
	RAWON_STR = nil;

	ssl = load SSL SSL->PATH;
	if(ssl == nil){
		sys->fprint(stderr, "Error: getauthinfo: can't load module ssl");
		exit;
	}

	# push ssl, leave in clear mode for now
	if(sys->bind("#D", "/n/ssl", Sys->MREPL) < 0){
		sys->fprint(stderr, "Error: getauthinfo: cannot bind #D: %r");
		exit;
	}

	argv = tl argv;
	if(argv == nil){
		sys->fprint(stderr, "%s\n", usage);
		exit;
	}
	keyname := hd argv;
	if (len keyname >= sys->NAMELEN){
		keyname = keyname[0:Sys->NAMELEN-2];
		sys->fprint(stderr, 
			"getauthinfo: warning: keyname length > %d, truncated to %s\n", 
			Sys->NAMELEN-1, keyname);
	}

	kr = load Keyring Keyring->PATH;
	str := load String String->PATH;
	if(str == nil){
		sys->fprint(stderr, "getauthinfo: can't load module String\n");
		exit;
	}

	login = load Login Login->PATH;
	if(login == nil){
		sys->fprint(stderr, "getauthinfo: can't load module Login\n");
		exit;
	}

	error := login->init();
	if(error != nil){
		sys->fprint(stderr, "getauthinfo: %s\n", error);
		exit;
	}

	user := user();
	path := "/usr/" + user + "/keyring/" + keyname;
	(dir, file) := str->splitr(path, "/");

	signer := login->defaultsigner();
	if(signer == nil){
		sys->fprint(stderr, "warning: getauthinfo: can't get default signer server name\n");
		signer = "$SIGNER";
	}

	passwd := "";
	view   := "no";
	accept := "yes";
	save   := "yes";
	redo   := "yes";
	for(;;)
	{
		# connect to signer server
		signer = promptstring("use signer", signer, RAWOFF);

		(ok, lc) := sys->dial("net!"+signer+"!inflogin", nil);
		if(ok < 0){
			sys->fprint(stderr, "Error: getauthinfo: dial login daemon failed %r\n");
			exit;
		}

		(err, c) := ssl->connect(lc.dfd);
		if(c == nil){
			sys->fprint(stderr, "Error: getauthinfo: can't push ssl: %s\n", err);
			exit;
		}
		lc.dfd = nil;
		lc.cfd = nil;

		# handshake by telling server who you are 
		user = promptstring("remote user name", user, RAWOFF);
		passwd = promptstring("password", passwd, RAWON);
		
		agf := "/licensedb/agreement.sig";
		(c, err) = login->chello(user, agf, c);
		if(c == nil){
			sys->fprint(stderr, "Error: getauthinfo: %s\n", err);
			exit;
		}

		# accept agreement
		view = promptstring("read agreement now?", view, RAWOFF);
		if(view[0] == 'y')
			viewagreement(agf);
		else
			sys->fprint(stdout, "you may read agreement in %s later\n", agf);
	
		accept = promptstring("accept agreement?", accept, RAWOFF);
		if(accept[0] != 'y'){
			kr->putstring(c.dfd, "don't agree");
			exit;
		}
		if(kr->putstring(c.dfd, "agree") < 0){
			sys->fprint(stderr, "Error: getauthinfo: can't send string: %r");
			exit;
		}
		
		# request certification
		info : ref Keyring->Authinfo;
		(info, err) = login->ckeyx(user, passwd, c);
	
		# save the info somewhere for later access
		save = promptstring("save in file", save, RAWOFF);
		if(save[0] != 'y'){
			if(sys->bind("#s", dir, Sys->MBEFORE) < 0){
				sys->fprint(stderr, "Error: getauthinfo: can't bind file channel %r\n");
				return;
			}
			fileio := sys->file2chan(dir, file);
			if(fileio == nil){
				sys->fprint(stderr, "Error: getauthinfo: file2chan failed %r\n");
				return;
			}
			spawn save2file(fileio);
		}

		if(info != nil)
		{
			if(kr->writeauthinfo(dir+file, info) < 0)
				sys->fprint(stderr, "Error: getauthinfo: writeauthinfo to %s failed: %r\n", file);
			break;
		}
		else
		{
			redo = promptstring("getauthinfo failed! try again?", redo, RAWOFF);
			if (redo != "yes") break;
		}
	}
}

viewagreement(file: string)
{
	sys = load Sys  Sys->PATH;
	stdout := sys->fildes(1);

	fd := sys->open(file, sys->OREAD);
	if(fd == nil){
		sys->fprint(stderr, "Error: getauthinfo: can't open %s %r\n", file);
		exit;
	}

	nbyte := 8192;
	buf := array[nbyte] of byte;
	for(;;){
		nbyte = sys->read(fd, buf, len buf);
		if(nbyte <= 0)
			break;
		if(sys->write(stdout, buf, nbyte) < nbyte){
			sys->fprint(stderr, "Error: getauthinfo: write %s Error %r\n", file);
			exit;
		}
	}
	if(nbyte < 0) {
		sys->fprint(stderr, "Error: getauthinfo: read %s Error %r\n", file);
		exit;
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

save2file(fileio: ref Sys->FileIO)
{
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
