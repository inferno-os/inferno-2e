implement IStyxd;

include "sys.m";
	sys: Sys;
	stdin, stderr: ref Sys->FD;

include "draw.m";

IStyxd: module 
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stdin = sys->fildes(0);
	stderr = sys->open("/dev/cons", sys->OWRITE);

	#
	# Set the user id
	#
	fd := sys->open("/dev/user", sys->OWRITE);
	if(fd == nil) {
		sys->fprint(stderr,"failed to open /dev/user: %r");
	}
	b := array of byte "none";
	if(sys->write(fd, b, len b) < 0) {
		sys->fprint(stderr,"failed to write /dev/user\nwith error %r");
	}

	sys->pctl(sys->FORKNS, nil);

	if(sys->export(stdin, sys->EXPASYNC) < 0)
		sys->fprint(stderr, "Error: istyxd: file export %r\n");
}
