implement Rm;

# rm(1)

include "sys.m";
include "draw.m";

FD: import Sys;
Context: import Draw;

usage: con "usage: rm file ...";

Rm: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

sys: Sys;
stderr, stdout: ref FD;

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;

	stdout = sys->fildes(1);
	stderr = sys->fildes(2);

	argv = tl argv;
	if (argv == nil) {
		sys->fprint(stderr, "%s\n", usage);
		return;
	}
	
	for (; argv != nil; argv = tl argv) {
		if (sys->remove(hd argv) < 0) {
			
			# explicit check for directory to trap access denied error
			if (sys->sprint("%r") == "Access is denied. ") {
				(ok, dir) := sys->stat(hd argv);
				if (ok < 0)	# should not happen
					sys->fprint(stderr, "rm: \"%s\": %r\n", hd argv);
					
				if (dir.mode & sys->CHDIR)
					sys->fprint(stderr, "rm: \"%s\": Directory not empty.\n", hd argv);
					
				else	# not a dir, so print the error message
					sys->fprint(stderr, "rm: \"%s\": %r\n", hd argv);
			} else {
				sys->fprint(stderr, "rm: \"%s\": %r\n", hd argv);
			}
		}
	}
}
