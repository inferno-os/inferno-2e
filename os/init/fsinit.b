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
	} else {
		print("Nvram opening failed\n");
	}

	setsysname();

	sys->print("bootp...");

	fd := sys->open("/net/ipifc/clone", sys->OWRITE);
	if(fd == nil) {
		print("init: open /net/ipifc: %r");
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
	if((ls == nil) || (hd ls=="0.0.0.0")){
		print("init: server address not in bootp read - assuming local disk\n");
		mountkfs("#H/hd0fs","fs","/");
	} else {
		srv := hd ls;
		sys->print("server %s\nConnect ...\n", srv);

		retrycount := 0;
		while(rootfs(srv) < 0 && retrycount++ < 5)
			sleep(1000);

		sys->print("done\n");
	}

	#
	# default namespace
	#
	bind("#c", "/dev", sys->MREPL);			# console
	if(spec != nil)
		bind(spec, "/nvfs", sys->MBEFORE|sys->MCREATE);	# our keys
	bind("#l", "/net", sys->MBEFORE);		# ethernet
	bind("#I", "/net", sys->MBEFORE);		# TCP/IP
	bind("#p", "/prog", sys->MREPL);		# prog device

	sys->print("Mounting scsi array\n");
	for (sdcount := 0; sdcount < 1; sdcount++) {
		mountkfs("#w/sd"+string sdcount+"fs","fs"+string sdcount+"0","/n/fs"+string sdcount+"0");
		mountkfs("#w/sd"+string sdcount+"fs2","fs"+string sdcount+"1","/n/fs"+string sdcount+"1");
	}

	sys->print("clock...\n");
	setclock();



	logon := load Logon "/dis/lib/srv.dis";
	if(logon == nil) {
		print("init: load /dis/lib/srv.dis: %r");
		logon = load Logon "/dis/lib/srv.dis";
	}

	logon->init(nil, nil);
	sys->print("logon...\n");
	logon = load Logon "/dis/sh.dis";
	if(logon == nil) {
		print("init: load /dis/wm/logon.dis: %r");
		logon = load Logon "/dis/sh.dis";
	}
	dc: ref Context;
	logon->init(dc, nil);
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

# Perform command on kfscons.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

kfs_cons(cmd : string) : int
{
	return kfs_cmd("cons", cmd);
}

# Perform command on kfsctl.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

kfs_ctl(cmd : string) : int
{
	return kfs_cmd("ctl", cmd);
}

# Send command to kfs.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

kfs_cmd(file : string, cmd : string) : int
{
	fd := sys->open("#Kcons/kfs" + file, sys->OWRITE);
	if (fd == nil) {
		sys->print("could not open #Kcons/kfs%s: %r\n", file);
		return 0;
	}
	b := array of byte cmd;
	if (sys->write(fd, b, len b) < 0) {
		sys->print("#Kcons/kfs%s: %r\n", file);
		return 0;
	}
	return 1;
}

# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

reamkfs(devname, fsname: string)
{
	kfs_ctl("ream " + fsname + " " + devname);
}


# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

mountkfs(devname, fsname, mntpt : string)
{
	kfs_ctl("filsys " + fsname + " " + devname);
	sys->bind("#K"+fsname, mntpt, Sys->MREPL|Sys->MCREATE);
}
