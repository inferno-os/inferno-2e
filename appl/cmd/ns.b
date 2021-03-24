# ns - a command line tool to display the construction of the current namespace (copy of plan9's ns)
implement ns;

include "sys.m";
include "draw.m";
include "string.m";

ns: module {
	init: fn(nil: ref Draw->Context, argv: list of string);
};

init(nil: ref Draw->Context, argv: list of string) {

	pid: int;

	sys := load Sys Sys->PATH;
	str := load String String->PATH;

	if ((argv == nil) || (tl argv == nil)) {
		pid = sys->pctl(0, nil);
	} else {
		arg := hd tl argv;
		
		if (arg[0:1] == "-") {
			sys->print("usage: ns [PID]\n");
			return;
		}

		(pid, nil) = str->toint(arg, 10);
	}
	
	nsfd := sys->open(sys->sprint("#p/%d/ns", pid), Sys->OREAD);
	if (nsfd == nil) {
		sys->print("ERROR: unable to open ns for pid %d\n", pid);
		return;
	}

	while (1) {

		# read in data
		buf := array[256] of byte;
		l := sys->read(nsfd, buf, 256);
		if (l < 0)
			sys->raise("Unable to read ns");	
		if (l == 0)
			break;

		# parse string
		(nstr, lstr) := sys->tokenize(string buf, " \n");
		(flags, nil) := str->toint(hd lstr, 10);

		# parse flags
		sflag := "";
		if (flags & 1)
			sflag = sflag + "b";
		if (flags & 2)
			sflag = sflag + "a";
		if (flags & 4)
			sflag = sflag + "c";
		if (sflag != "")
			sflag = "-" + sflag + " ";

		# add "'"'s if # found ...
		src := hd tl tl lstr;
		if ((src[0:2] == "#/") | (src[0:2] == "#U")) # remote unnecesary #/'s and #U's
			if ((src != "#/") && (src != "#U"))
				src = src[2:];
		if (src[0:1] == "#")
			src = "'" + src + "'";

		# is it a mount or a bind?
		cmd := "bind";
		if (src[2:3] == "M")
			cmd = "mount";

		# remove "#?" from beginning of destination path
		dest := hd tl lstr;
		if (dest[0:2] != "#M") {
			dest = dest[2:];
			if (dest == "")
				dest = "/";
		} else
			dest = "'" + dest + "'";

		# print formatted output
		sys->print("%s %s%s %s\n", cmd, sflag, src, dest);
	} 

	return;
}
