implement Init;
#
# init program for standalone wm using TK
#
include "sys.m";
sys: Sys;
FD, Connection, sprint, Dir: import sys;
print, fprint, open, bind, mount, dial, sleep, read: import sys;

include "security.m";
auth:	Auth;

include "draw.m";
draw: Draw;
Context: import draw;

include "keyring.m";
kr: Keyring;

Init: module
{
	init:	fn();
};

Logon: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

rootfs(server: string): int
{
	ok, n: int;
	c: Connection;
	alg: string;
	newfd : ref Sys->FD;
	id_or_err : string;

	(ok, c) = dial("tcp!" + server + "!6666", nil);
	if(ok < 0)
		return -1;

	sys->print("Connected ...");
	if(kr != nil){
		sys->print("Authenticate ...");
		ai := kr->readauthinfo("/nvfs/default");
		if(ai == nil) {
			print("No auth info found...attempting noauth\n");
			alg = Auth->NOAUTH;
		} else {
			alg = Auth->RC4_40;
		}
		id_or_err = auth->init();
		if (id_or_err != nil) {
			sys->print("auth initialization failed: %s\n",id_or_err);
			return 0;
		}
		(newfd, id_or_err) = auth->client( alg, ai, c.dfd );

		sys->print("%s\n",id_or_err);
	}

	sys->print("mount ...");

	c.cfd = nil;
	n = mount(newfd, "/", sys->MREPL, "");
	if(n > 0)
		return 0;
	return -1;
}

Bootpreadlen: con 128;

init()
{
	spec: string;

	sys = load Sys Sys->PATH;
	kr = load Keyring Keyring->PATH;
	auth = load Auth Auth->PATH;
	
	if (auth==nil) {
		sys->print("panic: no auth library in root file system\n");
		exit;
	}

	sys->print("**\n** Inferno\n** Lucent Technologies\n**\n");

	sys->print("Setup boot net services ...\n");
	
	#
	# Setup what we need to call a server and
	# Authenticate
	#
	bind("#l", "/net", sys->MREPL);
	bind("#I", "/net", sys->MAFTER);
	bind("#c", "/dev", sys->MAFTER);
	nvramfd := sys->open("#H/hd0nvram", sys->ORDWR);
	if(nvramfd != nil){
		spec = sys->sprint("#F%d", nvramfd.fd);
		if(bind(spec, "/nvfs", sys->MAFTER) < 0)
			print("init: bind %s: %r\n", spec);
	}

	setsysname();

	sys->print("bootp...");

	fd := open("/net/ipifc/clone", sys->OWRITE);
	if(fd == nil) {
		print("init: open /net/ipifc/clone: %r\n");
		exit;
	}
	cfg := array of byte "bind ether ether0";
	if(sys->write(fd, cfg, len cfg) != len cfg) {
		sys->print("could not bind interface: %r\n");
		exit;
	}
	cfg = array of byte "bootp";
	if(sys->write(fd, cfg, len cfg) != len cfg) {
		sys->print("could not bootp: %r\n");
		exit;
	}

	fd = open("/net/bootp", sys->OREAD);
	if(fd == nil) {
		print("init: open /net/bootp: %r");
		exit;
	}

	buf := array[Bootpreadlen] of byte;
	nr := read(fd, buf, len buf);
	fd = nil;
	if(nr <= 0) {
		print("init: read /net/bootp: %r");
		exit;
	}

	(ntok, ls) := sys->tokenize(string buf, " \t\n");
	while(ls != nil) {
		if(hd ls == "fsip"){
			ls = tl ls;
			break;
		}
		ls = tl ls;
	}
	if(ls == nil) {
		print("init: server address not in bootp read");
		exit;
	}

	srv := hd ls;
	sys->print("server %s\nConnect ...\n", srv);

	retrycount := 0;
	while(rootfs(srv) < 0 && retrycount++ < 5)
		sleep(1000);

	sys->print("done\n");

	#
	# default namespace
	#
	bind("#c", "/dev", sys->MREPL);			# console
	bind("#t", "/dev", sys->MAFTER);		# serial port
	if(spec != nil)
		bind(spec, "/nvfs", sys->MBEFORE|sys->MCREATE);	# our keys
	bind("#E", "/dev", sys->MBEFORE);		# mpeg
	bind("#l", "/net", sys->MBEFORE);		# ethernet
	bind("#I", "/net", sys->MBEFORE);		# TCP/IP
	bind("#V", "/dev", sys->MAFTER);		# hauppauge TV
	bind("#p", "/prog", sys->MREPL);		# prog device

	sys->print("clock...\n");
	setclock();

	sys->print("logon...\n");

	logon := load Logon "/dis/wm/logon.dis";
	if(logon == nil) {
		print("init: load /dis/wm/logon.dis: %r");
		logon = load Logon "/dis/sh.dis";
	}
	dc: ref Context;
	spawn logon->init(dc, nil);
}

setclock()
{
	(ok, dir) := sys->stat("/");
	if (ok < 0) {
		print("init: stat /: %r");
		return;
	}

	fd := sys->open("/dev/time", sys->OWRITE);
	if (fd == nil) {
		print("init: open /dev/time: %r");
		return;
	}

	# Time is kept as microsecs, atime is in secs
	b := array of byte sprint("%d000000", dir.atime);
	if (sys->write(fd, b, len b) != len b)
		print("init: write /dev/time: %r");
}

#
# Set system name from nvram
#
setsysname()
{
	fd := open("/nvfs/ID", sys->OREAD);
	if(fd == nil)
		return;
	fds := open("/dev/sysname", sys->OWRITE);
	if(fds == nil)
		return;
	buf := array[128] of byte;
	nr := sys->read(fd, buf, len buf);
	if(nr <= 0)
		return;
	sys->write(fds, buf, nr);
}
