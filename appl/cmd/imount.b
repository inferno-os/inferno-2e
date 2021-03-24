#
# Insecure version of mount - used in some native test applications
# (this basically has to be used with isrv, the insecure version of serve
#

implement Mount;

include "sys.m";
	sys: Sys;
	FD, Connection: import Sys;
	stderr: ref FD;

include "draw.m";
	Context: import Draw;

include "keyring.m";

include "security.m";

Mount: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

init(nil: ref Context, argv: list of string)
{
	c: Connection;
	ok: int;

	sys = load Sys Sys->PATH;

	stderr = sys->fildes(2);

	argv = tl argv;

	copt := 0;
	flags := sys->MREPL;
	while(argv != nil) {
		s := hd argv;
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
			*   =>
				usage();
			}
		}
		argv = tl argv;
	}
	if(copt)
		flags |= sys->MCREATE;

	if(len argv != 2)
		usage();

	addr := hd argv;
	dest := addr;
	argv = tl argv;
	fd : ref Sys->FD;
	(n, nil) := sys->tokenize(addr, "!");
	if (n == 1) {
		fd = sys->open(dest, Sys->ORDWR);
		if (fd == nil) {
			sys->fprint(stderr, "open: %s: %r\n", dest);
			return;
		}
	} else {
		if(n < 3)
			dest = dest+"!styx";
		(ok, c) = sys->dial(dest, nil);
		if(ok < 0) {
			sys->fprint(stderr, "dial: %s: %r\n", dest);
			return;
		}
		fd = c.dfd;
	}

	dir := hd argv;
	ok = sys->mount(fd, dir, flags, "");
	if(ok < 0)
		sys->fprint(stderr, "mount: %r\n");
}

usage()
{
	sys->fprint(stderr, "Usage: imount [-rabcA] net!mach old\n");
	exit;
}

