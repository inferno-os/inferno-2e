implement Gunzip;

include "sys.m";
	sys:	Sys;
	fprint, sprint: import sys;

include "draw.m";

include "string.m";
	str: String;

include "bufio.m";
	bufio:	Bufio;
	Iobuf:	import bufio;

include "inflate.m";
	inflate: Inflate;

Gunzip: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

GZMAGIC1:	con byte 16r1f;
GZMAGIC2:	con byte 16r8b;

GZDEFLATE:	con byte 8;

GZFTEXT:	con 1 << 0;		# file is text
GZFHCRC:	con 1 << 1;		# crc of header included
GZFEXTRA:	con 1 << 2;		# extra header included
GZFNAME:	con 1 << 3;		# name of file included
GZFCOMMENT:	con 1 << 4;		# header comment included
GZFMASK:	con (1 << 5) - 1;	# mask of specified bits

GZXBEST:	con byte 2;		# used maximum compression algorithm
GZXFAST:	con byte 4;		# used fast algorithm little compression

GZOSFAT:	con byte 0;		# FAT file system
GZOSAMIGA:	con byte 1;		# Amiga
GZOSVMS:	con byte 2;		# VMS or OpenVMS
GZOSUNIX:	con byte 3;		# Unix
GZOSVMCMS:	con byte 4;		# VM/CMS
GZOSATARI:	con byte 5;		# Atari TOS
GZOSHPFS:	con byte 6;		# HPFS file system
GZOSMAC:	con byte 7;		# Macintosh
GZOSZSYS:	con byte 8;		# Z-System
GZOSCPM:	con byte 9;		# CP/M
GZOSTOPS20:	con byte 10;		# TOPS-20
GZOSNTFS:	con byte 11;		# NTFS file system
GZOSQDOS:	con byte 12;		# QDOS
GZOSACORN:	con byte 13;		# Acorn RISCOS
GZOSUNK:	con byte 255;

GZCRCPOLY:	con int 16redb88320;

GZOSINFERNO:	con GZOSUNIX;

argv0:	con "gunzip";

stderr:	ref Sys->FD;

crctab	:= array[256] of int;

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	bufio = load Bufio Bufio->PATH;
	str = load String String->PATH;
	inflate = load Inflate Inflate->PATH;
	if(bufio == nil || str == nil || inflate == nil)
		fatal("can't load library modules" + sprint(": %r"));

	mkcrctab(GZCRCPOLY);
	inflate->init();

	if(argv != nil)
		argv = tl argv;

	ok := 1;
	if(len argv == 0){
		bin := bufio->fopen(sys->fildes(0), Bufio->OREAD);
		bout := bufio->fopen(sys->fildes(1), Bufio->OWRITE);
		(nil, nil, ok) = gunzip(bin, bout);
		bout.close();
	}else{
		for(; argv != nil; argv = tl argv)
			ok &= gunzipf(hd argv);
	}
	exit;
}

gunzipf(file: string): int
{
	bin := bufio->open(file, Bufio->OREAD);
	if(bin == nil){
		fprint(stderr, "%s: can't open %s: %r\n", argv0, file);
		return 0;
	}

	(nil, ofile) := str->splitr(file, "/");
	n := len ofile;
	if(n < 4 || ofile[n-3:] != ".gz"){
		fprint(stderr, "%s: .gz extension required: %s\n", argv0, file);
		bin.close();
		return 0;
	} else
		ofile = ofile[:n-3];
	bout := bufio->create(ofile, Bufio->OWRITE, 8r666);
	if(bout == nil){
		fprint(stderr, "%s: can't open %s: %r\n", argv0, ofile);
		bin.close();
		return 0;
	}

	(mtime, gzname, ok) := gunzip(bin, bout);
	if(ok == 1) {
		(nil, dir) := sys->stat(ofile);
		if(mtime > 0)
			dir.mtime = mtime;
		if(gzname != nil)
			dir.name = gzname;
		sys->wstat(ofile, dir);
	}

	bin.close();
	bout.close();
	return ok;
}

gunzip(bin, bout: ref Iobuf): (int, string, int)
{
	if(byte bin.getb() != GZMAGIC1 || byte bin.getb() != GZMAGIC2) {
		fprint(stderr, "%s: not a gzip file\n", argv0);
		return (0, nil, 0);
	}

	if(byte bin.getb() != GZDEFLATE) {
		fprint(stderr, "%s: not compressed with deflate\n", argv0);
		return (0, nil, 0);
	}

	flags := bin.getb();
	if(flags & ~GZFMASK) {
		fprint(stderr, "%s: reserved flag bits set\n", argv0);
		return (0, nil, 0);
	}

	mtime := bin.getb() | (bin.getb() << 8) | (bin.getb() << 16) | (bin.getb() << 24);
	xfl := bin.getb();
	os := bin.getb();

	# skip optional "extra field"
	if(flags & GZFEXTRA) {
		skip := bin.getb() | (bin.getb() << 8);
		bin.seek(skip, Bufio->SEEKRELA);
	}

	# read optional filename
	file: string;
	if(flags & GZFNAME){
		n := 0;
		while(c := bin.getb())
			file[n++] = c;
	}

	# skip optional comment
	if(flags & GZFCOMMENT) {
		while(bin.getb())
			;
	}

	# skip optional CRC16 field
	if(flags & GZFHCRC) {
		bin.getb();
		bin.getb();
	}

	(crc, tot, uneaten) := inflateit(bin, bout);

	un := 0;

	fcrc: int;
	(fcrc, un) = trailer(bin, uneaten, un);
	if(crc != fcrc) {
		fprint(stderr, "%s: crc mismatch: computed %ux, expected %ux\n", argv0, crc, fcrc);
		return (0, nil, 0);
	}

	ftot: int;
	(ftot, nil) = trailer(bin, uneaten, un);
	if(tot != ftot) {
		fprint(stderr, "%s: byte count mismatch: computed %d, expected %d\n", argv0, tot, ftot);
		return (0, nil, 0);
	}

	return (mtime, file, 1);
}

trailer(bin: ref Iobuf, uneaten: array of byte, un: int): (int, int)
{
	une := len uneaten;
	n := 0;
	for(i := 0; i < 4; i++) {
		nxt: int;
		if(un < une)
			nxt = int uneaten[un++];
		else
			nxt = bin.getb();
		n |= nxt << (8 * i);
	}
	return (n, un);
}

inflateit(bin, bout: ref Iobuf): (int, int, array of byte)
{
	crc := 0;
	tot := 0;
	io := inflate->reset();
	spawn inflate->inflate();
	for(;;) {
		in: int;
		flag_n := <- io.c;
		case(flag_n & Inflate->InflateMask) {
		Inflate->InflateEmptyIn =>
			in = bin.read(io.ibuf, Inflate->InflateBlock);
			if(in < 0) {
				io.c <- = Inflate->InflateError;
				fatal("read failed" + sprint(": %r"));
			}
			io.c <- = Inflate->InflateAck | in;
		Inflate->InflateFlushOut =>
			out := flag_n ^ Inflate->InflateFlushOut;
			if(out > 0) {
				crc = blockcrc(crc, io.obuf, out);
				realout := bout.write(io.obuf, out);
				if(realout != out) {
					io.c <- = Inflate->InflateError;
					fatal("write failed" + sprint(": %r"));
				}
				tot += out;
			}
			io.c <- = Inflate->InflateAck;
		Inflate->InflateDone =>
			uneaten: array of byte;
			n := flag_n ^ Inflate->InflateDone;
			if(n)
				uneaten = io.ibuf[in-n:in];
			return (crc, tot, uneaten);
		Inflate->InflateError =>
			exit;
		* =>
			fatal("bad karma");
		}
	}
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
