implement Pwd;

include "sys.m";
include "draw.m";
include "workdir.m";

Context: import Draw;

Pwd: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

init(nil: ref Context, nil: list of string)
{
	sys := load Sys Sys->PATH;
	gwd := load Workdir Workdir->PATH;

	wd := gwd->init();
	if(len wd == 0) {
		stderr := sys->fildes(2);
		sys->fprint(stderr, "pwd: %r\n");
		return;
	}
	sys->print("%s\n", wd);
}
