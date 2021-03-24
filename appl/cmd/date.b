implement Date;

include "sys.m";
include "draw.m";
include "daytime.m";

Context: import Draw;

Date: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

sys: Sys;

init(nil: ref Context, nil: list of string)
{
	sys = load Sys Sys->PATH;

	stderr := sys->fildes(2);

	daytime := load Daytime Daytime->PATH;
	if(daytime == nil) {
		sys->fprint(stderr, "date: load Daytime: %r\n");
		return;
	}

	sys->print("%s\n", daytime->time());
}
