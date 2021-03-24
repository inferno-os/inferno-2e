#
# Stream command connects two files for full duplex I/O
# 	Usage: stream <file1> <file2>
#
#

implement Stream;

include "sys.m";
include "draw.m";

Context: import Draw;

Stream: module
{
	init:	fn(nil: ref Context, argv: list of string);
};

sys: Sys;

streamit( file1, file2: ref Sys->FD, bufsize: int)
{
	sys->stream( file1, file2, bufsize );
}

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;

	argv = tl argv;

	if ((len argv) < 2) {
		sys->print("stream usage:\n\tstream <file1path> <file2path>\n\n");
		exit;
	}

	file1 := sys->open(hd argv, Sys->ORDWR);
	if (file1 == nil) {
		sys->print("stream: %s file not accessible\n",hd argv);
		exit;
	}
	argv = tl argv;
	file2 := sys->open(hd argv, Sys->ORDWR);

	if (file2 == nil) {
		sys->print("stream: %s file not accessible\n",hd argv);
		exit;
	}


	spawn streamit( file1, file2, 1);
	spawn streamit( file2, file1, 1);
}
