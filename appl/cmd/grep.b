implement Grep;

include "sys.m";
	sys: Sys;
	FD: import Sys;
	stdin, stderr, stdout: ref FD;

include "draw.m";
	Context: import Draw;

include "regex.m";
	regex: Regex;
	Re, compile, execute: import regex;

include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;

multi: int;

Grep: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

lflag: int;
nflag: int;
vflag: int;

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;

	stdin = sys->fildes(0);
	stdout = sys->fildes(1);
	stderr = sys->fildes(2);

	argv = tl argv;
	if(len argv < 1)
		usage();

	regex = load Regex Regex->PATH;
	if(regex == nil) {
		sys->fprint(stderr, "grep: load regex: %r\n");
		return;
	}
	bufio = load Bufio Bufio->PATH;
	if(bufio == nil) {
		sys->fprint(stderr, "grep: load bufio: %r\n");
		return;
	}

	pattern: string;
	while(argv != nil && (arg := hd argv)[0] == '-' && len arg > 1){
		case arg[1] {
		'l' =>
			lflag = 1;
		'n' =>
			nflag = 1;
		'v' =>
			vflag = 1;
		'e' =>
			argv = tl argv;
			if(pattern != nil || argv == nil)
				usage();
			pattern = hd argv;
		}
		argv = tl argv;
	}


	if(pattern == nil){
		if(argv == nil)
			usage();
		pattern = hd argv;
		argv = tl argv;
	}
	re := compile(pattern,0);
	if(re == nil) {
		sys->fprint(stderr, "grep: invalid regular expression\n");
		return;
	}

	if(argv == nil)
		argv = "-" :: nil;
	while(argv != nil) {
		if(tl argv != nil)
			multi++;
		grep(re, hd argv);
		argv = tl argv;
	}
}

usage()
{
	sys->fprint(stderr, "usage: grep [-l] [-n] [-v] [-e] pattern [files...]\n");
	exit;
}

grep(re: Re, file: string)
{
	f: ref Iobuf;

	if(file == "-") {
		f = bufio->fopen(stdin, Bufio->OREAD);
		file = "<stdin>";
	} else
		f = bufio->open(file, Bufio->OREAD);
	if(f == nil) {
		sys->fprint(stderr, "grep: open %s: %r\n", file);
		return;
	}
	for(line := 1; ; line++) {
		s := f.gets('\n');
		if(s == nil)
			break;
		if((execute(re, s[0:len s-1]) != nil) ^ vflag) {
			if(lflag){
				sys->print("%s\n", file);
				return;
			}
			if(nflag)
				if(multi)
					sys->print("%s:%d: %s", file, line, s);
				else
					sys->print("%d:%s", line, s);
			else
				if(multi)
					sys->print("%s: %s", file, s);
				else
					sys->print("%s", s);
		}
	}
}
