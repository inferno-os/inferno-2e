implement Emuinit;

include "sys.m";
include "draw.m";
include "sh.m";
sys: Sys;

Maxargs: con 8192;

Emuinit: module
{
	init: fn();
};

init()
{
	sys = load Sys Sys->PATH;
	args := getargs();
	if (args != nil)
		args = tl args;	# skip emu
	cmd := Command->PATH;
	while (args != nil) {
		arg := hd args;
		if (arg[0] != '-') {
			if (arg != "/dis/emuinit.dis" && arg != "/appl/cmd/emuinit.dis")
				break;
		}
		else if (arg[1] == 'd')
			cmd = "/dis/lib/srv.dis";
		args = tl args;
	}
	if (args != nil)
		cmd = hd args;
	sh: Command;
	if (cmd[0] == '/')
		sh = load Command cmd;
	else {
		sh = load Command "/dis/"+cmd;
		if (sh == nil)
			sh = load Command "/"+cmd;
	}
	if (sh == nil)
		sys->fprint(sys->fildes(2), "emuinit: unable to load %s: %r\n", cmd);
	else
		sh->init(nil, args);
}

getargs(): list of string
{
	buf := array[Maxargs] of byte;
	fd := sys->open("/dev/emuargs", Sys->OREAD);
	if (fd == nil)
		return nil;
	n := sys->read(fd, buf, len buf-1);
	if (n <= 0)
		return nil;
	delim := "";
	delim[0] = 1;
	(nil, str) := sys->tokenize(string buf[0:n-1], delim);
	return str;
}
