implement whoami;

include "sys.m";
sys: Sys;

include "draw.m";

whoami: module {
	init: fn(nil: ref Draw->Context, nil: list of string);
};

init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	if (sys == nil)
		return;

	# open the user file
	fd := sys->open("/dev/user", Sys->OREAD);
	if (fd == nil) {
		sys->print("whoami: unable to open /dev/user: %r\n");
		exit;
	}

	# read in the name
	buf := array[Sys->NAMELEN] of byte;
	rc := sys->read(fd, buf, len buf);
	if (rc < 0) {
		sys->print("whoami: unable to read /dev/user: %r\n");
		exit;
	}

	# print the result
	sys->print("%s\n", string buf[:rc]);

}
