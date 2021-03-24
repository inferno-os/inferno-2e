implement Bind;

include "sys.m";
include "draw.m";

FD: import Sys;
Context: import Draw;

Bind: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

sys: Sys;
copt: int;
stderr: ref FD;

init(nil: ref Context, argv: list of string)
{
	s: string;

	sys = load Sys Sys->PATH;

	stderr = sys->fildes(2);

	argv = tl argv;
	flags := sys->MREPL;
	while(argv != nil) {
		s = hd argv;
		if(s[0] != '-')
			break;
		for(i := 1; i < len s; i++) {
			case s[i] {
			'a' =>
				flags = sys->MAFTER;
			'b' =>
				flags = sys->MBEFORE;
			'r' =>
				flags = sys->MREPL;
			'c' =>
				copt++;	
			* =>
				sys->fprint(stderr, "Usage: bind [-rabc] source target\n");
				return;
			}
		}
		argv = tl argv;
	}
	if(copt)
		flags |= sys->MCREATE;

	if(len argv != 2) {
		sys->fprint(stderr, "Usage: bind [-rabc] source target\n");
		return;
	}

	if(sys->bind(hd argv, hd tl argv, flags) < 0)
		sys->fprint(stderr, "bind: %r\n");
}
