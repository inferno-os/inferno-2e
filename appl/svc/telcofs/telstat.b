implement Telstat;

include "sys.m";
sys: Sys;
FD, Dir: import sys;
fildes, open, fstat, read, dirread, fprint, print, tokenize: import sys;

include "draw.m";
Context: import Draw;

Telstat: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

stderr: ref FD;

init(nil: ref Context, nil: list of string)
{
	sys = load Sys Sys->PATH;

	stderr = fildes(2);

	tstat("/net/telco");
}

tstat(file: string)
{
	dir: Dir;
	fd: ref FD;
 	i, n, ok: int;
	d := array[100] of Dir;

	fd = open(file, sys->OREAD);
	if(fd == nil) {
		fprint(stderr, "telstat: telco not running\n");
		return;
	}

	(ok, dir) = fstat(fd);
	if(ok == -1) {
		fprint(stderr, "telstat: fstat %s: %r\n", file);
		fd = nil;
		return;
	}
	if((dir.mode&sys->CHDIR) == 0) {
		fprint(stderr, "telstat: not a protocol directory: %s\n", file);
		return;
	}
	for(;;) {
		n = dirread(fd, d);
		if(n <= 0)
			break;

		for(i = 0; i < n; i++)
			if(d[i].name != "clone")
				nsprint(file+"/"+d[i].name, d[i].uid);		
	}
}

fc(file: string): string
{
	fd := open(file, sys->OREAD);
	if(fd == nil)
		return "??";

	buf := array[32] of byte;
	n := read(fd, buf, len buf);
	if(n <= 1)
		return "??";

	return string buf[0:n-1];
}

nsprint(name, user: string)
{
	n: int;
	s: list of string;


	sr := fc(name+"/status");
	(n, s) = tokenize(sr, " ");
	if(n < 3)
		return;
	s = tl s;
	if(int hd s == 0)
		return;
	s = tl s;

	print("%-10s %-10s %-20s %-20s %s\n",
		name[5:len name],
		user,
		fc(name+"/local"),
		fc(name+"/remote"),
		hd s);
}
