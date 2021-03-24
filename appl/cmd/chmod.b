implement Chmod;

include "sys.m";
include "draw.m";
include "string.m";

Chmod: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

sys:	Sys;
stderr: ref Sys->FD;

str:	String;

User:	con 8r700;
Group:	con 8r070;
Other:	con 8r007;
All:	con User | Group | Other;

Read:	con 8r444;
Write:	con 8r222;
Exec:	con 8r111;

usage()
{
	sys->fprint(stderr, "usage: chmod [8r]777 file ... or chmod [augo][+-=][rwx] file ...\n");
}

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	str = load String String->PATH;
	if(str == nil){
		sys->fprint(stderr, "chmod: can't load string module '%s'\n", String->PATH);
		return;
	}

	if(len argv < 2){
		usage();
		return;
	}
	argv = tl argv;
	m := hd argv;
	argv = tl argv;

	mask := All;
	if (str->prefix("8r", m))
		m = m[2:];
	(mode, s) := str->toint(m, 8);
	if(s != ""){
		ok := 0;
		(ok, mask, mode) = parsemode(m);
		if(!ok){
			sys->fprint(stderr, "chmod: bad mode '%s'\n", m);
			usage();
			return;
		}
	}
	for(; argv != nil; argv = tl argv){
		f := hd argv;
		(ok, dir) := sys->stat(f);
		if(ok < 0){
			sys->fprint(stderr, "chmod: can't stat %s: %r\n", f);
			continue;
		}
		dir.mode = (dir.mode & ~mask) | (mode & mask);
		if(sys->wstat(f, dir) < 0)
			sys->fprint(stderr, "chmod: can't wstat %s: %r\n", f);
	}
}

parsemode(spec: string): (int, int, int)
{
	mask := 0;
loop:	for(i := 0; i < len spec; i++){
		case spec[i] {
		'u' =>
			mask |= User;
		'g' =>
			mask |= Group;
		'o' =>
			mask |= Other;
		'a' =>
			mask |= All;
		* =>
			break loop;
		}
	}
	if(i == len spec)
		return (0, 0, 0);
	if(!mask)
		mask |= All;

	op := spec[i++];
	if(op != '+' && op != '-' && op != '=')
		return (0, 0, 0);

	mode := 0;
	for(; i < len spec; i++){
		case spec[i]{
		'r' =>
			mode |= Read;
		'w' =>
			mode |= Write;
		'x' =>
			mode |= Exec;
		* =>
			return (0, 0, 0);
		}
	}
	if(op == '+' || op == '-')
		mask &= mode;
	if(op == '-')
		mode = ~mode;
	return (1, mask, mode);
}
