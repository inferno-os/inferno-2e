Inflate: module
{
	PATH:	con "/dis/lib/inflate.dis";

	#
	# huffman code table
	#
	Huff: adt
	{
		bits:		int;		# length of the code
		encode:		int;		# the code
	};

	#
	# huffman decode table
	#
	DeHuff: adt
	{
		l1:		array of L1;	# the table
		nb1:		int;		# no. of bits in first level
		nb2:		int;		# no. of bits in second level
	};

	#
	# first level of decode table
	#
	L1: adt
	{
		bits:		int;		# length of the code
		decode:		int;		# the symbol
		l2:		array of L2;
	};

	#
	# second level
	#
	L2: adt
	{
		bits:		int;		# length of the code
		decode:		int;		# the symbol
	};

	#
	# conduit for data streaming between inflate and its producer/consumer
	#
	InflateIO: adt
	{
		ibuf:		array of byte;	# input buffer [InflateBlock]
		obuf:		array of byte;	# output buffer [InflateBlock]
		c:		chan of int;	# for inflate <-> server comm.
	};

	InflateBlock:	con 32*1024;

	InflateEmptyIn,
	InflateFlushOut,
	InflateAck,
	InflateDone,
	InflateError:	con 1 << (16 + iota);
	InflateMask:	con InflateEmptyIn | InflateFlushOut | InflateAck
				| InflateDone | InflateError;

	init:		fn();
	reset:		fn(): ref InflateIO;
	inflate:	fn();
};
