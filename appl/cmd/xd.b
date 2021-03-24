 implement HexDump;

# author: E. V. Bacher

include "sys.m";
	sys:	Sys;
	stdin, stdout, stderr:	ref sys->FD;
	fprint,
	sprint,
	raise,
	open,
	read,
	ATOMICIO,
	OREAD:  import sys;
include "draw.m";
include "bufio.m";
	bufmod: Bufio;
	Iobuf: import bufmod;
	
#
# globals
#
readfile: string;
inbuf, outbuf: ref Iobuf;
pname: string;
buf : array of byte;

debug,					# flag for debug information
address, 				# starting address
bytesleft :	int = 0;	# bytes in last line

printchars,				# flag for showing printable characters
width :	int = 1;     	# default number of bytes in output clusters

charwidth := 2;			# byte takes up two hex chars

linebytes := 16;  		# bytes per line

HexDump: module  
{
	init:   fn(ctxt: ref Draw->Context, argv: list of string);
};

usage()
{
	fprint(stderr, "\nusage: %s  [-[1|2|4|8]]   [-P] [-p] [file]\n", pname);
	fprint(stderr, "          (bytes/cluster)\n");
	fprint(stderr, "\"-\" or no args: take input from stdin, Ctrl-d terminates\n\n");
	exit;
}
 

init(nil: ref Draw->Context, argv: list of string)
{
	sys  = load Sys Sys->PATH;
	bufmod = load Bufio Bufio->PATH;
	if (bufmod == nil)
		raise(sprint("fail: could not load Bufio from %s: %r\n", Bufio->PATH));

	stdin  = sys->fildes(0);
	stdout = sys->fildes(1);
	stderr = sys->fildes(2);
	
	pname = hd argv; argv = tl argv;
	if (argv == nil) 
		argv = "stdin" :: nil;

	outbuf = bufmod->fopen(stdout, bufmod->OWRITE);

	fd : ref sys->FD;
	while (argv != nil) {
		readfile = hd argv;
		argv = tl argv;

		case readfile {

			"-h" or "-?"	=>	usage();

			"-" or "stdin"		=>
				readfile = "stdin";
				inbuf = bufmod->fopen(stdin, bufmod->OREAD);
				if (inbuf == nil)
					raise(sprint("fail: could not open %s: %r\n", readfile));

# reset address and read data from inbuf
					
				fd = nil;
				address = 0;
				readit(inbuf, nil);

			"-P"	=>
				printchars = 0;
				if (argv == nil) argv = "stdin" :: nil;

			"-p"	=>
				printchars = 1;
				if (argv == nil) argv = "stdin" :: nil;
				
			"-d"	=>
				debug = 1;
				if (argv == nil) argv = "stdin" :: nil;

			"-1" or "-2" or "-4" or "-8"	=>
				width = int readfile[1:];
				if (argv == nil) argv = "stdin" :: nil;
				
			*		=>
				fd = open(readfile, OREAD);
				if (fd == nil)
					raise(sprint("fail: could not open %s: %r\n", readfile));

# reset address and read data from fd
					
				inbuf = nil;
				address = 0;
				readit(nil, fd);
		}
	}
}

#
# read from inbuf or fd, starting at address 000
#

readit(inbuf: ref Iobuf, fd: ref sys->FD)
{
	buf = array[ATOMICIO] of byte;
	for (;;)
	{
		if (fd != nil) 
			n := read(fd, buf, len buf);
		
		if (inbuf != nil) {
			buf = array of byte inbuf.gets('\n');
			n = len buf;
		}

		if (debug) fprint(stderr, "read %d bytes from %s\n", n, readfile);

		if (n == 0) {
			break;
		}
		dump(buf[:n], n);
	}
	outbuf.flush();
}

dump(buf: array of byte, n: int)
{
	
# the bytes (in lines of linebytes one-byte chunks), followed by
# a string s that shows printable characters
# increment address after each byte
	
	s := " ";
	line := "";
	for (i := 0 ; i < n ; i++) {
		if ((i % linebytes) == 0) {
			if (printchars) {
				line += sprint("%s\n", s);
			} else {
				line += sprint("\n");
			}
			line += sprint("%#8.8x: ", address);
			s = " ";
		} 
		line += sprint("%2.2x", int buf[i]);
		if (int buf[i] >= 32 && int buf[i] < 127)
			s[len s] = int buf[i];
		else
			s += ".";

		if ((i + 1) % width == 0) {
			line += sprint(" ");
			bytesleft = linebytes;
		}
		address++;
	}

# now we're done with all the bytes, so
# fill out the last line of bytes with spaces
# then print the last string
	
	linelen := linebytes * 2 + linebytes/width;
	bytesleft = i%linebytes;
	spaceused := bytesleft * 2 + bytesleft/width;
	
	if (spaceused == 0 ) {
		spaceused = linelen;
	}
	for (i = 0  ; i < (linelen - spaceused) ; i++){
		line += sprint(" ");
	}

	if (printchars) {
		line += sprint("%s\n", s);
	} else {
		line += sprint("\n");
	}

	outbuf.puts(line);
}
