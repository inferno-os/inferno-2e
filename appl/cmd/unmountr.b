implement Unmountr;

Mod : con "Unmountr";

include "sys.m";
include "draw.m";

FD: import Sys;
Context: import Draw;

include "bindr.m";

Unmountr: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

sys: Sys;
stderr: ref FD;

usage()
{
	sys->fprint(stderr, "Usage:\tunmountr [-[tszZ]uwf] [source-list] target\n\tunmountr -t*\t-- test only version\n\tunmountr -[uwf]\t-- a recursive union, wunion, or files replaced\n\tunmountr -s*\t-- recurse on source tree (instead of target)\n\tunmountr -z*\t-- read recurse work list from stdin\n\tunmountr -Z* FN\t-- read recurse work list from FileName\n");
}

init(nil: ref Context, argv: list of string)
{
	r: int;

	sys = load Sys Sys->PATH;

	stderr = sys->fildes(2);

	argv = tl argv;

	br := load Bindr Bindr->PATH;
	if (br == nil) {
	  sys->fprint(stderr, Mod+": %s %r\n", Bindr->PATH);
	  return;
	}
	br->init(nil, nil);

	flags := 0;
	test := 0;
	srcr := 0;
	zio := 0;
	fio : string;

	while(argv != nil) {
		s := hd argv;
		if(s[0] != '-')
			break;
		for(i := 1; i < len s; i++) {
			case s[i] {
			'u' =>
			  	flags = br->MUNION;
			'w' =>
			  	flags = br->MWNION;
			'f' =>
				flags = br->MFILE;
			't' =>
			  	test = 1;
			's' =>
			  	srcr = 1;
			'z' =>
			  	zio = 1;
			'Z' =>  {
				argv = tl argv;
				if (argv == nil) {usage(); return;}
				fio = hd argv;
				if (fio[0] != '-') fio = "-"+fio;
				}
			* =>
				usage();
				return;
			}
		}
		argv = tl argv;
	}
	
	if (test)
	  	flags |= br->MTEST;
	if (srcr)
	  	flags |= br->MSRCR;

	if (fio != nil) {
		if (argv == nil) {usage(); return;}
		if (zio) r = br->unmountrrch(fio :: argv, flags, nil);
		else r = br->unmountrr(fio :: argv, flags);
	}
	else case len argv {
		0 =>
			usage();
			return;
		1 =>
			if (zio) r = br->unmountrch(nil, hd argv, flags, nil);
			else r = br->unmountr(nil, hd argv, flags);
		2 =>
			if (zio) r = br->unmountrch(hd argv, hd tl argv, flags, nil);
			else r = br->unmountr(hd argv, hd tl argv, flags);
		* =>
			if (zio) r = br->unmountrrch(argv, flags, nil);
			else r = br->unmountrr(argv, flags);
		}

	if(r < 0)
		sys->fprint(stderr, Mod+": %r\n");
}
