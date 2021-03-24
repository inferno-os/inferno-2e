implement StartClient;

include "sys.m";
	sys: Sys;
include "draw.m";
include "sh.m";


StartClient: module
{
	init: fn(ctxt: ref Draw->Context, nil: list of string);
};


init(ctxt: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	stderr := sys->fildes(2);

	sys->print("\nBinding the ip interfaces to /net");	
	ret := sys->bind("#I", "/net", Sys->MREPL);
	if (ret == -1) {
		sys->fprint(stderr, "startclient: unable to bind the ip interfaces to /net: %r\n");
		return;
	}

	sys->print(" and starting the connection server...\n");
	cs := load Command "/dis/lib/cs.dis";
	if (cs == nil) {
		sys->fprint(stderr, "startclient: couldn't load /dis/lib/cs.dis: %r\n");
		return;
	}
	cs->init(ctxt, "cs" :: nil);

	sys->print("The following interfaces have been created:\n");
	ls := load Command "/dis/ls.dis";
	if (ls == nil) {
		sys->fprint(stderr, "startclient: couldn't load /dis/ls.dis: %r\n");
		return;
	}
	ls->init(ctxt, "ls" :: "/net" :: nil);
}
