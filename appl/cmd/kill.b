implement Kill;

include "sys.m";
include "draw.m";

FD, Dir: import Sys;
Context: import Draw;

include "kill.m";

sys: Sys;

info: int;

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;

	argv = tl argv;

	if(argv == nil) {
		sys->fprint(sys->fildes(2), "Usage: kill [-g] pid|module\n");
		return;
	}
	msg := array of byte "kill";
	if(hd argv == "-g") {
		argv = tl argv;
		msg = array of byte "killgrp";
	}	

	while(argv != nil) {
		s := hd argv;
		if(s[0] >= '0' && s[0] <= '9')
			killpid(s, msg);
		else
			killmod(s, msg);
		argv = tl argv;
	}
}

killpid(pid: string, msg: array of byte)
{
	if (sys == nil)
		sys = load Sys Sys->PATH;

	if (msg == nil)
		msg = array of byte "kill";

		
	fd := sys->open("/prog/"+pid+"/ctl", sys->OWRITE);
	if(fd == nil) {
		sys->fprint(sys->fildes(2), "kill: process %s: %r\n", pid);
		return;
	}

	n := sys->write(fd, msg, len msg);
	if(n < 0)
		sys->fprint(sys->fildes(2), "kill: message for %s: %r\n", pid);
}

killmod(mod: string, msg: array of byte)
{
	n: int;
	if (sys == nil)
		sys = load Sys Sys->PATH;

	if (msg == nil)
		msg = array of byte "kill";

	fd := sys->open("/prog", sys->OREAD);
	if(fd == nil) {
		sys->fprint(sys->fildes(2), "kill: open /prog: %r\n");
		return;
	}

	d := array[100] of Dir;
	for(;;) {
		n = sys->dirread(fd, d);
		if(n <= 0)
			break;

		for(i := 0; i < n; i++)
			killmatch(d[i].name, mod, msg);		
	}
	if(info)
		sys->print("\n");
	if(n < 0)
		sys->fprint(sys->fildes(2), "kill: dirread /prog: %r\n");
}

killmatch(dir, mod: string, msg: array of byte)
{
	status := "/prog/"+dir+"/status";
	fd := sys->open(status, sys->OREAD);
	if(fd == nil)
		return;
	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) {
		sys->fprint(sys->fildes(2), "kill: read %s: %r\n", status);
		return;
	}

	s : string;
	(nil, lst) := sys->tokenize(string buf[0:n], " ");
	while (lst != nil) {
	  s = hd lst;
	  lst = tl lst;
	}

	for(i := 0; i < len s; i++) {
	  if(s[i] == '[') {
	    s = s[0:i];
	    break;
	  }
	}

	if(s == mod) {
		info = 1;
		sys->print("%s ", dir);
		dir = "/prog/"+dir+"/ctl";
		fd = sys->open(dir, sys->OWRITE);
		if(fd == nil) {
		  sys->fprint(sys->fildes(2), "kill: open /prog: %r\n");
		  return;
		}
		n = sys->write(fd, msg, len msg);
		if(n < 0)
			sys->fprint(sys->fildes(2), ": %r, ");
	}
}
