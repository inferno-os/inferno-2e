implement Mkdir;

# mkdir(1)

include "sys.m";
	sys: Sys;
	FD: import Sys;
	stderr: ref FD;

include "draw.m";
	Context: import Draw;

usage: con "usage: mkdir dirname ...";

Mkdir: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;

	stderr = sys->fildes(2);

	argv = tl argv;
	if (argv == nil) {
		sys->fprint(stderr, "%s\n", usage);
		return;
	}
	
	while (argv != nil) {
		dir := hd argv;
		(ok, nil) := sys->stat(dir);
		if(ok < 0) {
			f := sys->create(dir, sys->OREAD, sys->CHDIR + 8r777);
			if (f == nil)
				sys->fprint(stderr, "mkdir: can't create %s: %r\n", dir);
			f = nil;
		}
		else 
			sys->fprint(stderr, "mkdir: %s already exists\n", dir);
		argv = tl argv;
	}
}
