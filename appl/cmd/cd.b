implement Cd;

include "sys.m";
include "draw.m";

FD: import Sys;
Context: import Draw;

Cd: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

sys: Sys;
stderr: ref FD;

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;

	stderr = sys->fildes(2);

	argv = tl argv;
	if(argv == nil)
		argv = "/usr/"+user() :: nil;

	if(tl argv != nil) {
		sys->fprint(stderr, "Usage: cd directory\n");
		return;
	}

	if(sys->chdir(hd argv) < 0)
		sys->fprint(stderr, "cd: %s: %r\n", hd argv);
}

user(): string
{
	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil)
		return "inferno";

	buf := array[Sys->NAMELEN] of byte;
	n := sys->read(fd, buf, len buf);
	if(n <= 0)
		return "inferno";

	return string buf[0:n];
}
