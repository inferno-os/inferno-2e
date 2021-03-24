implement Gzip;

include "sys.m";
	sys:	Sys;
	print, fprint: import sys;

include "draw.m";

include "string.m";
	str: String;

include "daytime.m";
	daytime: Daytime;

include "bufio.m";
	bufio:	Bufio;
	Iobuf: import bufio;

include "deflate.m";
	deflate: Deflate;

Gzip: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

Arg: adt
{
	argv:	list of string;
	c:	int;
	opts:	string;

	init:	fn(argv: list of string): ref Arg;
	opt:	fn(arg: self ref Arg): int;
	arg:	fn(arg: self ref Arg): string;
};

GZMAGIC1:	con byte 16r1f;
GZMAGIC2:	con byte 16r8b;

GZDEFLATE:	con byte 8;

GZFTEXT:	con byte 1 << 0;		# file is text
GZFHCRC:	con byte 1 << 1;		# crc of header included
GZFEXTRA:	con byte 1 << 2;		# extra header included
GZFNAME:	con byte 1 << 3;		# name of file included
GZFCOMMENT:	con byte 1 << 4;		# header comment included
GZFMASK:	con (byte 1 << 5) - byte 1;	# mask of specified bits

GZXFAST:	con byte 2;			# used fast algorithm little compression
GZXBEST:	con byte 4;			# used maximum compression algorithm

GZOSFAT:	con byte 0;			# FAT file system
GZOSAMIGA:	con byte 1;			# Amiga
GZOSVMS:	con byte 2;			# VMS or OpenVMS
GZOSUNIX:	con byte 3;			# Unix
GZOSVMCMS:	con byte 4;			# VM/CMS
GZOSATARI:	con byte 5;			# Atari TOS
GZOSHPFS:	con byte 6;			# HPFS file system
GZOSMAC:	con byte 7;			# Macintosh
GZOSZSYS:	con byte 8;			# Z-System
GZOSCPM:	con byte 9;			# CP/M
GZOSTOPS20:	con byte 10;			# TOPS-20
GZOSNTFS:	con byte 11;			# NTFS file system
GZOSQDOS:	con byte 12;			# QDOS
GZOSACORN:	con byte 13;			# Acorn RISCOS
GZOSUNK:	con byte 255;

GZCRCPOLY:	con int 16redb88320;

GZOSINFERNO:	con GZOSUNIX;

argv0:	con "gzip";

stderr:	ref Sys->FD;

debug	:= 0;
verbose	:= 0;
level	:= 0;
crctab	:= array[256] of int;

usage()
{
	fprint(stderr, "usage: %s [file ...]\n", argv0);
	exit;
}

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	bufio = load Bufio Bufio->PATH;
	str = load String String->PATH;
	daytime = load Daytime Daytime->PATH;
	if(bufio == nil || str == nil || daytime == nil){
		fprint(stderr, "can't load library modules\n");
		exit;
	}
	deflate = load Deflate Deflate->PATH;
	if(deflate == nil){
		fprint(stderr, "can't load compression module\n");
		exit;
	}

	arg := Arg.init(argv);
	level = 6;
	while(c := arg.opt()){
		case c{
		'D' =>
			debug++;
		'v' =>
			verbose++;
		'1' to  '9' =>
			level = c - '0';
		* =>
			usage();
		}
	}

	mkcrctab(GZCRCPOLY);
	deflate->init();

	argv = arg.argv;

	ok := 1;
	if(len argv == 0){
		bin := bufio->fopen(sys->fildes(0), Bufio->OREAD);
		bout := bufio->fopen(sys->fildes(1), Bufio->OWRITE);
		ok = gzip(nil, daytime->now(), bin, bout);
		bout.close();
	}else{
		for(; argv != nil; argv = tl argv)
			ok &= gzipf(hd argv);
	}
	exit;
}

gzipf(file: string): int
{
	bin := bufio->open(file, Bufio->OREAD);
	if(bin == nil){
		fprint(stderr, "%s: can't open %s: %r\n", argv0, file);
		return 0;
	}
	(ok, dir) := sys->fstat(bin.fd);
	if(ok >= 0)
		mtime := dir.mtime;
	else
		mtime = daytime->now();

	(nil, ofile) := str->splitr(file, "/");
	ofile += ".gz";
	bout := bufio->create(ofile, Bufio->OWRITE, 8r666);
	if(bout == nil){
		fprint(stderr, "%s: can't open %s: %r\n", argv0, ofile);
		bin.close();
		return 0;
	}

	ok = gzip(file, mtime, bin, bout);

	bin.close();
	bout.close();
	return ok;
}

gzip(file: string, mtime: int, bin, bout: ref Iobuf): int
{
	flags := byte 0;
	bout.putb(GZMAGIC1);
	bout.putb(GZMAGIC2);
	bout.putb(GZDEFLATE);

	if(file != nil)
		flags |= GZFNAME;
	bout.putb(flags);

	bout.putb(byte(mtime));
	bout.putb(byte(mtime>>8));
	bout.putb(byte(mtime>>16));
	bout.putb(byte(mtime>>24));

	bout.putb(byte 0);
	bout.putb(GZOSINFERNO);

	if((flags & GZFNAME) == GZFNAME){
		bout.puts(file);
		bout.putb(byte 0);
	}

	crc := deflateit(bin, bout);

	bout.putb(byte(crc));
	bout.putb(byte(crc>>8));
	bout.putb(byte(crc>>16));
	bout.putb(byte(crc>>24));

	tot := bin.seek(0, 1);
	bout.putb(byte(tot));
	bout.putb(byte(tot>>8));
	bout.putb(byte(tot>>16));
	bout.putb(byte(tot>>24));

	return 1;
}

deflateit(bin, bout: ref Iobuf): int
{
	lz := deflate->reset(nil, level, verbose, debug);
	buf := array[Deflate->DeflateBlock] of byte;
	obuf := array[Deflate->DeflateBlock + Deflate->DeflateOut] of byte;
	eof := 0;
	crc := 0;
	for(;;) {
		n := 0;
		while(!eof && n < Deflate->DeflateBlock) {
			m := bin.read(buf, Deflate->DeflateBlock - n);
			if(m <= 0) {
				eof = 1;
				break;
			}
			n += m;
		}
		crc = blockcrc(crc, buf, n);

		n = deflate->deflate(lz, buf, n, eof, obuf);
		if(n < 0)
			break;
		bout.write(obuf, n);
	}

	return crc;
}

mkcrctab(poly: int)
{
	for(i := 0; i < 256; i++){
		crc := i;
		for(j := 0; j < 8; j++){
			c := crc & 1;
			crc = (crc >> 1) & 16r7fffffff;
			if(c)
				crc ^= poly;
		}
		crctab[i] = crc;
	}
}

blockcrc(crc: int, buf: array of byte, n: int): int
{
	crc ^= int 16rffffffff;
	for(i := 0; i < n; i++)
		crc = crctab[int(byte crc ^ buf[i])] ^ ((crc >> 8) & 16r00ffffff);
	return crc ^ int 16rffffffff;
}

fatal(msg: string)
{
	fprint(stderr, "%s: %s\n", argv0, msg);
	exit;
}

Arg.init(argv: list of string): ref Arg
{
	if(argv != nil)
		argv = tl argv;
	return ref Arg(argv, 0, nil);
}

Arg.opt(arg: self ref Arg): int
{
	if(arg.opts != ""){
		arg.c = arg.opts[0];
		arg.opts = arg.opts[1:];
		return arg.c;
	}
	if(arg.argv == nil)
		return arg.c = 0;
	arg.opts = hd arg.argv;
	if(len arg.opts < 2 || arg.opts[0] != '-')
		return arg.c = 0;
	arg.argv = tl arg.argv;
	if(arg.opts == "--")
		return arg.c = 0;
	arg.c = arg.opts[1];
	arg.opts = arg.opts[2:];
	return arg.c;
}

Arg.arg(arg: self ref Arg): string
{
	s := arg.opts;
	arg.opts = "";
	if(s != "")
		return s;
	if(arg.argv == nil)
		return "";
	s = hd arg.argv;
	arg.argv = tl arg.argv;
	return s;
}
