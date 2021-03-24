implement Kfswiz;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;

Kfswiz: module
{
	init: fn(nil: ref Draw->Context, args: list of string);
};

Defbufsize: con 1024;
NAMELEN: con 28;
NDBLOCK: con 6;
Tnone, Tsuper, Tdir, Tind1, Tind2, Tfile, Tfree,
	Tbuck, Tvirgo, Tcache, MAXTAG : con iota;

stderr: ref sys->FD;
bigend: int;
debug: int;

Buffer: adt {
	buf: array of byte;
	index: int;

	init: fn(bsize: int): ref Buffer;
	get: fn(b: self ref Buffer, fd: ref sys->FD): int;
	put: fn(b: self ref Buffer, fd: ref sys->FD): int;

	tag: fn(b: self ref Buffer, bigend: int): int;
	badsize: fn(b: self ref Buffer, caller: string);

	skip: fn(b: self ref Buffer, n: int);
	shorts: fn(b: self ref Buffer, n: int);
	longs: fn(b: self ref Buffer, n: int);
};

init(nil: ref Draw->Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;

	bufsize := Defbufsize;
	infile := sys->fildes(0);
	outfile := sys->fildes(1);
	stderr = sys->fildes(2);

	e := ref Sys->Exception;
	if (sys->rescue("fail: usage", e) == Sys->EXCEPTION) {
		sys->fprint(stderr, "usage: kfswiz [-b bsize] [infile]\n");
		exit;
# following commented out because a module needs to be rebuilt...
#		sys->rescued(sys->RAISE, nil);
	}

	args = tl args;
	while (args != nil && (hd args)[0] == '-') case hd args {
	"-b" =>
		args = tl args;
		if (args == nil)
			sys->raise("fail: usage");
		bufsize = int (hd args);
		args = tl args;
	"-d" =>
		args = tl args;
		debug++;
	* =>
		sys->raise("fail: usage");
	}
	if (args != nil) {
		if (tl args != nil)
			sys->raise("fail: usage");
		infile = sys->open(hd args, sys->OREAD);
		if (infile == nil) {
			sys->fprint(stderr, "kfswiz: can't open %s: %r\n", hd args);
			exit;
		}
	}
#	sys->unrescue();
	kfswiz(infile, outfile, bufsize);
}

kfswiz(in: ref sys->FD, out: ref sys->FD, bsize: int)
{
	buf := array[bsize] of byte;
	rbsize := bsize-8;	# ``real'' buf size, ie how much data we store after tags subtracted

	sys->fprint(stderr, "kfswiz: Block size %d\n", bsize);
	b := Buffer.init(bsize);

	# First block is magic number+blocksize (in text) (?)
	if (!b.get(in))
		exit;
	b.skip(bsize);
	if (!b.put(out))
		exit;

	# Get the superblock.  Determine endianness.
	if (!b.get(in))
		exit;
	if (b.tag(0) == Tsuper) {
		bigend = 0;
	}
	else if (b.tag(1) == Tsuper) {
		bigend = 1;
	}
	else {
		sys->fprint(stderr, "kfswiz: superblock tag invalid (%x)\n", b.tag(0));
		exit;
	}
	endian := array[2] of { "little", "big" };
	sys->fprint(stderr, "kfswiz: Input file is %s-endian.  Output will be %s-endian.\n",
		endian[bigend], endian[!bigend]);
	b.longs(rbsize/4);	# all the fields in superblock itself are longs
	b.skip(2);		# pad
	b.shorts(1);	# tag
	b.longs(1);	# path
	if (!b.put(out))
		exit;

	dirsize := NAMELEN+3*2+2+2*4+5*4+NDBLOCK*4;
	rdirsize := (dirsize+3)&~3;		# structure padding
	dirperbuf := rbsize/rdirsize;
	indperbuf := rbsize/4;
	for (i := 0;; i++) {
		if (!b.get(in))
			exit;
		if (debug) sys->fprint(stderr, "Block %d:", i);
		tag := b.tag(bigend);
		case tag {
		Tdir =>
			if (debug) sys->fprint(stderr, "directory\n");
			for (j := 0; j < dirperbuf; j++) {
				b.skip(NAMELEN);
				b.shorts(3+1);			# 3 shorts+padding
				b.longs(7+NDBLOCK);
				b.skip(rdirsize-dirsize);
			}
			b.skip(rbsize-rdirsize*dirperbuf);
		Tind1 or Tind2 =>
			if (debug) sys->fprint(stderr, "indirect\n");
			b.longs(indperbuf);
		Tfile or Tnone =>
			# Nothing to do...
			if (debug) sys->fprint(stderr, "file (or none)\n");
			b.skip(rbsize);
		Tfree =>
			if (debug) sys->fprint(stderr, "free\n");
			b.longs(indperbuf);
		* =>
			sys->fprint(stderr, "kfswiz: bad tag value %d\n", tag);
			exit;
		}
		b.skip(2);		# pad
		b.shorts(1);	# tag
		b.longs(1);	# path
		if (!b.put(out))
			exit;
	}
	sys->fprint(stderr, "kfswiz: finished writing file\n");
}

Buffer.init(bsize: int): ref Buffer
{
	b := ref Buffer;
	b.index = 0;
	b.buf = array[bsize] of byte;
	return b;
}

Buffer.get(b: self ref Buffer, fd: ref sys->FD): int
{
	b.index = 0;
	n := sys->read(fd, b.buf, len b.buf);
	if (n != len b.buf) {
		if (n == 0)
			return 0;
		else if (n == -1)
			sys->fprint(stderr, "kfswiz: read failed: %r\n");
		else
			sys->fprint(stderr, "kfswiz: short read\n");
		return 0;
	}
	return 1;
}

Buffer.put(b: self ref Buffer, fd: ref sys->FD): int
{
	if (b.index != len b.buf) {
		b.badsize("put");
		return 0;
	}
	n := sys->write(fd, b.buf, len b.buf);
	if (n != len b.buf) {
		sys->fprint(stderr, "kfswiz: write failed: %r\n");
		return 0;
	}
	return 1;
}

Buffer.tag(b: self ref Buffer, bigend: int): int
{
	off := len b.buf - 6;
	tag := b.buf[off:off+2];
	if (bigend)
		return (int tag[0] << 8)|(int tag[1]);
	else
		return (int tag[1] << 8)|(int tag[0]);
}

Buffer.badsize(b: self ref Buffer, caller: string)
{
	sys->fprint(stderr, "kfswiz: internal error: caller %s tag %d index %d bsize %d\n",
		caller, b.tag(bigend), b.index, len b.buf);
}

Buffer.skip(b: self ref Buffer, n: int)
{
	b.index += n;
	if (b.index > len b.buf) {
		b.badsize("skip");
		exit;
	}
}

Buffer.shorts(b: self ref Buffer, n: int)
{
	newindex := b.index + 2*n;
	if (newindex > len b.buf) {
		b.index = newindex;
		b.badsize("shorts");
		exit;
	}
	for (i := b.index; i < newindex; i += 2) {
		if (debug) sys->fprint(stderr, "short %x %x\n", int b.buf[i], int b.buf[i+1]);
		(b.buf[i], b.buf[i+1]) = (b.buf[i+1], b.buf[i]);
	}
	b.index = newindex;
}

Buffer.longs(b: self ref Buffer, n: int)
{
	newindex := b.index + 4*n;
	if (newindex > len b.buf) {
		b.index = newindex;
		b.badsize("longs");
		exit;
	}
	for (i := b.index; i < newindex; i += 4) {
		if (debug) sys->fprint(stderr, "long %x %x %x %x\n", int b.buf[i], int b.buf[i+1], int b.buf[i+2], int b.buf[i+3]);
		(b.buf[i], b.buf[i+1], b.buf[i+2], b.buf[i+3]) = (b.buf[i+3], b.buf[i+2], b.buf[i+1], b.buf[i]);
	}
	b.index = newindex;
}
