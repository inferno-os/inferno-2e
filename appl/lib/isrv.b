#  Unauthenticated srv

implement Server;

include "sys.m";
sys: Sys;
FD, Connection: import sys;

include "draw.m";
Context: import Draw;

include "srv.m";
srv: Srv;

include "newns.m";

include "keyring.m";
kr: Keyring;

include "security.m";

include "sh.m";

Server: module
{
	init: fn(ctxt: ref Context, args: list of string);
};

sh: Command;
stderr: ref FD;

init(nil: ref Context, nil: list of string)
{
	cs: Command;

	sys = load Sys Sys->PATH;
	srv = load Srv Srv->PATH;
	kr = load Keyring Keyring->PATH;

	stderr = sys->fildes(2);

	sys->print("srv: initialize services\n");

	(ok, nil) := sys->stat("services/namespace");
	if(ok >= 0) {
		ns := load Newns Newns->PATH;
		if(ns == nil)
			sys->fprint(stderr, "Failed to load namespace builder: %r\n");
		else {
			nserr := ns->newns(nil, "services/namespace");
			if(nserr != nil) {
				sys->fprint(stderr,
					"Error in user namespace file: %s\n", nserr);
			}
		}
	}

	#
	# cs must be started before listen
	#
	cs = load Command "/dis/lib/cs.dis";
	if(cs == nil)
		sys->fprint(stderr, "srv: cs module load failed: %r\n");
	else
		cs->init(nil, nil);

	spawn gensrv("/dis/lib/signer.dis", "tcp", "infsigner");
	spawn gensrv("/dis/lib/countersigner.dis", "tcp", "infcsigner");
	spawn gensrv("/dis/lib/logind.dis", "tcp", "inflogin");
	spawn startservice("/dis/lib/virgild.dis", stderr);
	spawn filesrv("tcp");

	sh = load Command "/dis/sh.dis";

	spawn execsrv("tcp");

	if(sh == nil)
		sys->fprint(stderr, "srv: sh module load failed: %r\n");
	else
		sh->init(nil, nil);
}

filesrv(net: string)
{
	ok: int;
	c: Connection;

	(ok, c) = sys->announce(net+"!*!styx");
	if(ok < 0) {
		sys->fprint(stderr, "can't announce file service: %r\n");
		exit;
	}

	for(;;)
		filemnt(c);
}

filemnt(c: Connection)
{
	(ok, nc) := sys->listen(c);
	if(ok < 0) {
		sys->fprint(stderr, "listen: %r\n");
		exit;
	}

	buf := array[64] of byte;

	l := sys->open(nc.dir+"/remote", sys->OREAD);
	n := sys->read(l, buf, len buf);
	if(n >= 0)
		sys->print("New client (STYX): %s %s", nc.dir, string buf[0:n]);

	nc.dfd = sys->open(nc.dir+"/data", sys->ORDWR);
	if(nc.dfd == nil) {
		sys->fprint(stderr, "open: %s: %r\n", nc.dir);
		exit;
	}
	spawn dostyx(nc.dfd);
}

dostyx(fd: ref Sys->FD)
{
#	if(kr != nil)
#		fd = auth(fd);

	if(fd!=nil && sys->export(fd, sys->EXPASYNC) < 0)
		sys->fprint(stderr, "export %r\n");
}

execsrv(net: string)
{
	(ok, c) := sys->announce(net+"!*!rstyx");
	if(ok < 0) {
		sys->fprint(stderr, "can't announce rstyx service: %r\n");
		exit;
	}
	for(;;)
		execdoer(c);
}

execdoer(c: Connection)
{
	(ok, nc) := sys->listen(c);
	if(ok < 0) {
		sys->fprint(stderr, "listen: %r\n");
		exit;
	}

	buf := array[64] of byte;
	l := sys->open(nc.dir+"/remote", sys->OREAD);
	n := sys->read(l, buf, len buf);
	if(n >= 0)
		sys->print("New client (REXEC): %s %s", nc.dir, string buf[0:n]);
	l = nil;

	nc.dfd = sys->open(nc.dir+"/data", sys->ORDWR);
	if(nc.dfd == nil) {
		sys->fprint(stderr, "open: %s: %r\n", nc.dir);
		exit;
	}

	spawn exec(nc.dfd);
}

readn(fd: ref FD, nb: int): array of byte
{
	m: int;
	buf:= array[nb] of byte;
	b1:= array[nb] of byte;

	for(n:=0; n<nb; n+=m){
		m = sys->read(fd, b1, nb-n);
		if(m <= 0)
			return nil;
		for(j:=0; j<m; j++)
			buf[n+j] = b1[j];
	}
	return buf;
}

readargs(fd: ref FD): list of string
{
	buf := array[15] of byte;
	c := array[1] of byte;
	for(i:=0; ; i++){
		if(i>=len buf || sys->read(fd, c, 1)!=1)
			return nil;
		buf[i] = c[0];
		if(c[0] == byte '\n')
			break;
	}
	nb := int string buf;
	if(nb <= 0)
		return nil;
	args := readn(fd, nb);
	(nil, a) := sys->tokenize(string args[0:nb], " \n");
	return a;
}

exec(fd: ref FD)
{
	if(kr == nil){
		sys->fprint(stderr, "Can't load module Keyring %r\n");
		return;
	}
#	fd = auth(fd);

	sys->pctl(sys->FORKFD, fd.fd :: nil);

	args := readargs(fd);
	if(args == nil){
		sys->fprint(stderr, "exec: read args: %r\n");
		return;
	}

	cmd := hd args;
	sys->fprint(stderr, "exec: cmd: %s\n",cmd);
	mod: Command;
	if(cmd == "sh")
		mod = sh;
	else{
		file := cmd + ".dis";
		mod = load Command file;
		if(mod == nil)
			mod = load Command "/dis/"+file;
		if(mod == nil) {
			sys->fprint(stderr, "exec: %s not found: %r\n", cmd);
			return;
		}
	}

	sys->pctl(sys->FORKNS, nil);

	if(sys->mount(fd, "/n/client", sys->MREPL, "") < 0) {
		sys->fprint(stderr, "exec: mount: %r\n");
		return;
	}

	if(sys->bind("/n/client/dev", "/dev", sys->MBEFORE) < 0) {
		sys->fprint(stderr, "exec: bind client: %r\n");
		return;
	}

	fd = sys->open("/dev/cons", sys->OREAD);
	sys->dup(fd.fd, 0);
	fd = sys->open("/dev/cons", sys->OWRITE);
	sys->dup(fd.fd, 1);
	sys->dup(fd.fd, 2);

	mod->init(nil, args);
}

gensrv(cmdname, net, service: string)
{
	(ok, c) := sys->announce(net+"!*!"+service);
	if(ok < 0) {
		sys->fprint(stderr, "%s: can't announce %s service: %r\n", service, service);
		exit;
	}
	for(;;)
		gendoer(cmdname, c);
}

gendoer(cmdname: string, c: Connection)
{
	(ok, nc) := sys->listen(c);
	if(ok < 0) {
		sys->fprint(stderr, "listen: %r\n");
		return;
	}

	buf := array[64] of byte;
	l := sys->open(nc.dir+"/remote", sys->OREAD);
	n := sys->read(l, buf, len buf);
	if(n >= 0)
		sys->print("New client (%s): %s %s", cmdname, nc.dir, string buf[0:n]);

	nc.dfd = sys->open(nc.dir+"/data", sys->ORDWR);
	if(nc.dfd == nil) {
		sys->fprint(stderr, "open: %s: %r\n", nc.dir);
		return;
	}

	spawn startservice(cmdname, nc.dfd);
}

startservice(cmdname: string, fd: ref Sys->FD)
{
	cmd := load Command cmdname;
	if(cmd == nil){
		sys->fprint(stderr, "%s: command not found\n", cmdname);
		return;
	}

	sys->pctl(sys->NEWFD, fd.fd :: nil);
	sys->dup(fd.fd, 0);
	sys->dup(fd.fd, 1);

	cmd->init(nil, nil);
}

rf(file: string): string
{
	fd := sys->open(file, sys->OREAD);
	if(fd == nil)
		return "";

	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0)
		return "";

	return string buf[0:n];	
}
