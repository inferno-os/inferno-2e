
Deflate: module
{
	PATH:	con "/dis/lib/deflate.dis";

	LZstate: adt
	{
		hist:		array of byte;		# [HistSize];
		epos:		int;			# end of history buffer
		pos:		int;			# current location in history buffer
		eof:		int;
		hash:		array of int;		# [Nhash] hash chains
		nexts:		array of int;		# [MaxOff]
		me:		int;			# pos in hash chains
		dot:		int;			# dawn of time in history
		prevlen:	int;			# lazy matching state
		prevoff:	int;
		maxchars:	int;			# compressor tuning
		maxdefer:	int;

		outbuf:		array of byte;		# current output buffer;
		out:		int;			# current position in the output buffer
		bits:		int;			# bit shift register
		nbits:		int;

		verbose:	int;
		debug:		int;

		lzb:		ref LZblock;
		slop:		array of byte;
		dlitlentab:	array of Huff;		# [Nlitlen]
		dofftab:	array of Huff;		# [Noff];
		hlitlentab:	array of Huff;		# [Nlitlen];
		dyncode:	ref Dyncode;
		hdyncode:	ref Dyncode;
	};

	#
	# lempel-ziv compressed block
	#
	LZblock: adt
	{
		litlen:		array of byte;			# [MaxUncBlock+1];
		off:		array of int;			# [MaxUncBlock+1];
		litlencount:	array of int;			# [Nlitlen];
		offcount:	array of int;			# [Noff];
		entries:	int;				# entries in litlen & off tables
		bytes:		int;				# consumed from the input
		excost:		int;				# cost of encoding extra len & off bits
	};

	#
	# encoding of dynamic huffman trees
	#
	Dyncode: adt
	{
		nlit:		int;
		noff:		int;
		nclen:		int;
		ncode:		int;
		codetab:	array of Huff;		# [Nclen];
		codes:		array of byte;		# [Nlitlen+Noff];
		codeaux:	array of byte;		# [Nlitlen+Noff];
	};

	#
	# huffman code table
	#
	Huff: adt
	{
		bits:		int;				# length of the code
		encode:		int;				# the code
	};

	DeflateBlock:	con 64*1024-258-1;
	DeflateOut:	con 258+10;

	deflate:	fn(lz: ref LZstate, buf: array of byte, nbuf, eof: int, out: array of byte): int;
	init:		fn();
	reset:		fn(lz: ref LZstate, level, verbose, debug: int): ref LZstate;
};
