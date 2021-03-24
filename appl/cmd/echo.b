implement Echo;

include "sys.m";
	sys: Sys;
include "draw.m";

Echo: module
{
	init:	fn(nil: ref Draw->Context, argv: list of string);
};

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;

	argv = tl argv;

	nonewline := 0;
	if (len argv > 0 && hd argv == "-n") {
		nonewline = 1;
		argv = tl argv;
	}

	s := "";
	if (argv == nil)
		s = " ";
		
	for (; argv != nil; argv = tl argv) {
		s += " " + hd argv;
	}

	if (nonewline == 0) 
		s += "\n";

	a := array of byte s[1:];
	if (sys->write(sys->fildes(1), a, len a) != len a)
		sys->raise(sys->sprint("fail: write error: %r"));
}
