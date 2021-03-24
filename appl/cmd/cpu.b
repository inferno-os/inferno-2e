implement CPU;

include "sys.m";
	sys: Sys;
	stderr: ref Sys->FD;

include "draw.m";
	Context: import Draw;

include "keyring.m";
include "security.m";

DEFCMD:	con "wm/sh";

CPU: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

# The default level of security is NOSSL
init(ctxt: ref Context, argv: list of string)
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

	args := "lib/cpuslave -s";
	if(ctxt != nil && ctxt.screen != nil)
		args += string ctxt.screen.id;
	else
		args += "-1";

	mach: string;
	case len argv{
	0 =>
		usage();
	1 =>
		mach = hd argv;
		args += " " + DEFCMD;
	* =>
		mach = hd argv;
		a := tl argv;
		while(a != nil){
			args += " " + hd a;
			a = tl a;
		}
	}

	# To make visible remotely
	(ok, dir) := sys->stat("/dev/draw/new");
	if(ok < 0)
		sys->bind("#d", "/dev/draw", sys->MAFTER);

	addr := "tcp!"+mach;
	c : Sys->Connection;
	(ok, c) = sys->dial(addr+"!rstyx", nil);
	if(ok < 0){
		sys->fprint(stderr, "Error: cpu: dial: %r\n");
		return;
	}

	kr := load Keyring Keyring->PATH;
	if(kr == nil){
		sys->fprint(stderr, "Error: cpu: can't load module Keyring %r\n");
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
	#	sys->fprint(stderr, "Error: cpu: key for %s not found %r\n", addr);
	#	exit;
	# }
	#

	au := load Auth Auth->PATH;
	if(au == nil){
		sys->fprint(stderr, "Error: cpu: can't load module Login %r\n");
		exit;
	}

	err := au->init();
	if(err != nil){
		sys->fprint(stderr, "Error: cpu: %s\n", err);
		exit;
	}

	fd := ref Sys->FD;		
	(fd, err) = au->client(alg, ai, c.dfd);
	if(fd == nil){
		sys->fprint(stderr, "Error: cpu: authentication failed: %s\n", err);
		exit;
	}

	t := array of byte sys->sprint("%d\n%s\n", len (array of byte args)+1, args);
	if(sys->write(fd, t, len t) != len t){
		sys->fprint(stderr, "Error: cpu: export args write: %r\n");
		return;
	}

	if(sys->export(fd, sys->EXPWAIT) < 0){
		sys->fprint(stderr, "Error: cpu: export: %r\n");
		return;
	}
}

usage()
{
	sys->fprint(stderr, "Usage: cpu [-C cryptoalg] mach command args...\n");
	exit;
}

user(): string
{
	sys = load Sys Sys->PATH;

	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil){
		sys->fprint(stderr, "Error: styxd: can't open /dev/user %r\n");
		exit;
	}

	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0){
		sys->fprint(stderr, "Error: styxd: failed to read /dev/user %r\n");
		exit;
	}

	return string buf[0:n];	
}


