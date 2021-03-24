implement Command;

include "sys.m";
include "draw.m";

Command: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

init(nil: ref Draw->Context, argv: list of string)
{
	sys := load Sys Sys->PATH;
	if(sys == nil)
		return;

	n := len argv;
	if(n != 2 && n != 3){
		sys->print("usage: step pid [count]");
		return;
	}
	argv = tl argv;

	pid := hd argv;
	argv = tl argv;
	count := 1;
	if(argv != nil)
		count = int hd argv;
	if(count <= 0 || count > 1024)
		count = 1;
	fd := sys->open("/prog/"+pid+"/dbgctl", sys->OWRITE);
	if(fd == nil){
		sys->print("step: cannot open /prog/%s/dbgctl file: %r\n", pid);
		return;
	}

	buf := array of byte("step "+string count);
	if(sys->write(fd, buf, len buf) != len buf)
		sys->print("stepping failed: %r\n");
}
