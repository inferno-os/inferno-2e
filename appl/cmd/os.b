implement Os;

include "sys.m";
include "draw.m";

FD: import Sys;
Context: import Draw;

Os: module
{
	init:	fn(nil: ref Context, argv: list of string);
};

sys: Sys;
stderr: ref FD;

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	argv = tl argv;
	if(argv == nil) {
		sys->fprint(stderr, "Usage: os cmd ...\n");
		return;
	}

	sys->pctl(Sys->FORKNS, nil);
	ok := sys->bind("#p", "/prog", Sys->MREPL);
	if (ok < 0){
		sys->fprint(stderr, "os: bind #p /prog: %r\n");
		return;
	}

	cmd := "exec ";
	while(argv != nil) {
		cmd += hd argv;
		argv = tl argv;
		if(argv != nil)
			cmd += " ";
	}			
	cfd := sys->open("/cmd/clone", sys->ORDWR);
	if(cfd == nil) {
		sys->fprint(stderr, "os: open /cmd/clone: %r\n");
		sys->raise("fail: couldn't access command device");
		return;
	}
	
	buf := array[32] of byte;
	n := sys->read(cfd, buf, len buf);
	if(n <= 0) {
		sys->fprint(stderr, "os: read /cmd/#/ctl: %r\n");
		sys->raise("fail: read ctl failed");
		return;
	}
	dir := "/cmd/"+string buf[0:n];

	# Start the Command
	n = sys->fprint(cfd, "%s", cmd);
	if(n <= 0) {
		sys->fprint(stderr, "os: exec: %r\n");
		sys->raise("fail: Execution Failed");
		return;
	}

	io := sys->open(dir+"/data", sys->ORDWR);
	if(io == nil) {
		sys->fprint(stderr, "os: open /cmd/#/data: %r\n");
		#sys->raise("fail: Execution Failed");
		return;
	}

	sys->pctl(sys->NEWPGRP, nil);
	spawn copy(sys->fildes(0), io);
	copy(io, sys->fildes(1));
	killgrp(sys->pctl(0, nil));
}

copy(f, t: ref FD)
{
	buf := array[8192] of byte;
	for(;;) {
		r := sys->read(f, buf, len buf);
		if(r <= 0)
			break;
		w := sys->write(t, buf, r);
		if(w != r)
			break;
	}
}

killgrp(pid: int)
{
	fd := sys->open("/prog/"+string pid+"/ctl", sys->OWRITE);
	sys->fprint(fd, "killgrp");
}
