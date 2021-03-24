implement Ps;

include "sys.m";
include "draw.m";

FD, Dir: import Sys;
Context: import Draw;

Ps: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

sys: Sys;
stderr: ref FD;

init(nil: ref Context, nil: list of string)
{
	fd: ref FD;
 	i, n: int;
	d := array[100] of Dir;

	sys = load Sys Sys->PATH;

	stderr = sys->fildes(2);

	fd = sys->open("/prog", sys->OREAD);
	if(fd == nil)
		sys->raise(sys->sprint("fail: /prog: %r"));

	for(;;) {
		n = sys->dirread(fd, d);
		if(n <= 0)
			break;

		for(i = 0; i < n; i++)
			ps(d[i].name);		
	}
	if(n < 0)
		sys->raise(sys->sprint("fail: dirread /prog: %r"));
}

ps(proc: string)
{
	proc = "/prog/"+proc+"/status";
	fd := sys->open(proc, sys->OREAD);
	if(fd == nil) {
		sys->fprint(stderr, "ps: %s: %r\n", proc);
		return;
	}
	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) {
		sys->fprint(stderr, "ps: read %s: %r\n", proc);
		return;
	}
	sys->print("%s\n", string buf[0:n]);
}
