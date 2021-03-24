implement StartServer;

include "sys.m";
	sys: Sys;
include "draw.m";
include "sh.m";


StartServer: module
{
	init: fn(ctxt: ref Draw->Context, nil: list of string);
};


init(ctxt: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	stderr := sys->fildes(2);

	user := username();
	if(user == "") {
		sys->fprint(stderr, "setupuser: unable to determine username from /dev/user: %r\n");
		return;
	}

	(status,nil) := sys->stat("/usr/" + user + "/keyring/default");
	if(status != 0)
		sys->print("\nnote: you should run setupuser to get a default certificate.\n\n");

	sys->print("\nStarting services...\n");
	libsrv := load Command "/dis/lib/srv.dis";
	if (libsrv == nil) {
		sys->fprint(stderr, "startserver: couldn't load /dis/lib/srv.dis: %r\n");
		return;
	}
	libsrv->init(ctxt, "srv" :: nil);

	sys->sleep(1000);
	sys->print("\n\nServer is ready. Waiting for client requests...\n\n");
}

username(): string
{
	fd := sys->open("/dev/user", Sys->OREAD);
	if(fd == nil)
		return "";
	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) 
		return "";
	return string buf[0:n];
}
