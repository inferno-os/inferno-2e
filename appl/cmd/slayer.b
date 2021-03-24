implement Slayer;

include "sys.m";
include "draw.m";

FD, Dir: import Sys;
Context: import Draw;

Slayer: module
{
	init:	fn(nil: ref Context, args: list of string);
};

sys: Sys;
stderr: ref FD;
death: array of byte;
dir := array[100] of Dir;
buf := array[128] of byte;

init(nil: ref Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	s := 60;
	if (args != nil && (t := int hd args) >= 10)
		s = t;
	death = array of byte "kill";
	spawn slayer(s);
}

slayer(s: int)
{
	s *= 1000;
	for (;;) {
		fd := sys->open("/prog", sys->OREAD);
		if (fd == nil) {
			sys->fprint(stderr, "slayer: /prog: %r\n");
			return;
		}
		for (;;) {
			n := sys->dirread(fd, dir);
			if (n < 0)
				sys->fprint(stderr, "slayer: dirread /prog: %r\n");
			if (n == 0)
				break;
			for (i := 0; i < n; i++)
				probe(dir[i].name);		
		}
		sys->sleep(s);
	}
}

probe(pid: string)
{
	proc := "/prog/" + pid + "/status";
	fd := sys->open(proc, sys->OREAD);
	if (fd == nil)
		return;
	n := sys->read(fd, buf, len buf);
	if (n < 0) {
		sys->fprint(stderr, "slayer: read %s: %r\n", proc);
		return;
	}
	(c, l) := sys->tokenize(string buf[0:n], " ");
	if (c >= 4 && hd tl tl tl l == "broken")
		slay(pid);
}

slay(pid: string)
{
	ctl := "/prog/" + pid + "/ctl";
	fd := sys->open(ctl, sys->OWRITE);
	if (fd == nil)
		return;
	sys->write(fd, death, len death);
}
