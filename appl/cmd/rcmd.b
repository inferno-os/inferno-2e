implement Rcmd;

include "sys.m";
	sys: Sys;
	stderr: ref Sys->FD;

include "draw.m";
	Context: import Draw;

include "keyring.m";

include "security.m";
	auth: Auth;

Rcmd: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	argv = tl argv;

	alg := Auth->NOSSL;
	while(argv != nil) {
		s := hd argv;
		if(s[0] != '-')
			break;
		case s[1] {
		'C' =>
			alg = s[2:];
			if(alg == nil || alg == "") {
				argv = tl argv;
				if(argv != nil)
					alg = hd argv;
				else
					usage();
			}
		*   =>
			usage();
		}
		argv = tl argv;
	}

	if(argv == nil)
		usage();

	addr := hd argv;
	argv = tl argv;

	args := "";
	while(argv != nil){
		args += " " + hd argv;
		argv = tl argv;
	}
	if(args == "")
		args = "sh";

	# To make visible remotely
	(ok, dir) := sys->stat("/dev/draw/new");
	if(ok < 0)
		sys->bind("#d", "/dev/draw", sys->MBEFORE);

	(ok1, c) := sys->dial(addr+"!rstyx", nil);
	if(ok1 < 0){
		sys->fprint(stderr, "Error: rcmd: dial server failed: %r\n");
		exit;
	}

	kr := load Keyring Keyring->PATH;
	if(kr == nil){
		sys->fprint(stderr, "Error: rcmd: can't load module Keyring %r\n");
		exit;
	}

	user := user();
	kd := "/usr/" + user + "/keyring/";
	cert := kd + addr;
	(ok, nil) = sys->stat(cert);
	if(ok < 0){
		cert = kd + "default";
		(ok, nil) = sys->stat(cert);
		if(ok<0)
			sys->fprint(stderr, "Warning: no certificate found in %s; use getauthinfo\n", kd);
	}

	ai := kr->readauthinfo(cert);
	#
	# let auth->client handle nil ai
	# if(ai == nil){
	#	sys->fprint(stderr, "Error: rcmd: certificate for %s not found\n", addr);
	#	exit;
	# }
	#

	au := load Auth Auth->PATH;
	if(au == nil){
		sys->fprint(stderr, "Error: rcmd: can't load module Login %r\n");
		exit;
	}

	err := au->init();
	if(err != nil){
		sys->fprint(stderr, "Error: rcmd: %s\n", err);
		exit;
	}

	#do this before using auth
	if(sys->bind("#D", "/n/ssl", Sys->MREPL) < 0){
		sys->fprint(stderr, "Error: can't bind #D: %r\n");
		exit;
	}

	fd := ref Sys->FD;
	(fd, err) = au->client(alg, ai, c.dfd);
	if(fd == nil){
		sys->fprint(stderr, "Error: rcmd: authentication failed: %s\n", err);
		exit;
	}

	t := array of byte sys->sprint("%d\n%s\n", len (array of byte args)+1, args);
	if(sys->write(fd, t, len t) != len t){
		sys->fprint(stderr, "Error: rcmd: export args write: %r\n");
		exit;
	}

	if(sys->export(fd, sys->EXPWAIT) < 0)
		sys->fprint(stderr, "Error: rcmd: export: %r\n");
}

usage()
{
	sys->fprint(stderr, "Usage: rcmd [-C cryptoalg] tcp!mach cmd\n");
	exit;
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
