implement Inflate;

include "sys.m";
	sys:	Sys;
	fprint:	import sys;
	stderr:	ref Sys->FD;

include "inflate.m";

DeflateUnc:	con 0;			# uncompressed block
DeflateFix:	con 1;			# fixed huffman codes
DeflateDyn:	con 2;			# dynamic huffman codes
DeflateErr:	con 3;			# reserved BTYPE (error)

DeflateEob:	con 256;		# end of block code in lit/len book

LenStart:	con 257;		# start of length codes in litlen
LenEnd:		con 285;		# greatest valid length code
Nlitlen:	con 288;		# number of litlen codes
Noff:		con 30;			# number of offset codes
Nclen:		con 19;			# number of codelen codes

MaxHuffBits:	con 15;			# max bits in a huffman code
RunlenBits:	con 7;			# max bits in a run-length huffman code
MaxOff:		con 32*1024;		# max lempel-ziv distance

# tables from RFC 1951, section 3.2.5
litlenbase := array[Noff] of
{
	3, 4, 5, 6, 7, 8, 9, 10, 11, 13,
	15, 17, 19, 23, 27, 31, 35, 43, 51, 59,
	67, 83, 99, 115, 131, 163, 195, 227, 258
};

litlenextra := array[Noff] of
{
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
	2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4,
	5, 5, 5, 5, 0
};

offbase := array[Noff] of
{
	1, 2, 3, 4, 5, 7, 9, 13, 17, 25,
	33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
	1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

offextra := array[Noff] of
{
	0,  0,  0,  0,  1,  1,  2,  2,  3,  3,
	4,  4,  5,  5,  6,  6,  7,  7,  8,  8,
	9,  9,  10, 10, 11, 11, 12, 12, 13, 13
};

# order of run-length codes
clenorder := array[Nclen] of
{
	16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
};

# fixed huffman tables
litlentab: array of Huff;
offtab: array of Huff;

# their decoding table counterparts
litlendec: ref DeHuff;
offdec: ref DeHuff;

# bit reversal for endian swap of huffman codes
revtab: array of byte;

# for masking low-order n bits of an int
mask: array of int;

# I/O buffers and pointers
io: ref InflateIO;
in: int;		# next byte to consume from input buffer
ein: int;		# valid bytes in input buffer
out: int;		# valid bytes in output buffer
hist: array of byte;	# history buffer for lempel-ziv backward references
usehist: int;		# == 1 if 'hist' is valid

reg: int;		# 24-bit shift register
nbits: int;		# number of valid bits in reg
svreg: int;		# save reg for efficient ungets
svn: int;		# number of bits gotten in last call to getn()
# reg bits are consumed from right to left
# so low-order byte of reg came first in the input stream

init()
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	# byte reverse table
	revtab = array[256] of byte;
	for(i := 0; i < 256; i++){
		revtab[i] = byte 0;
		for(j := 0; j < 8; j++) {
			if(i & (1 << j))
				revtab[i] |= byte 16r80 >> j;
		}
	}

	# bit-masking table
	mask = array[MaxHuffBits+1] of int;
	for(i = 0; i <= MaxHuffBits; i++)
		mask[i] = (1 << i) - 1;

	litlentab = array[Nlitlen] of Huff;

	# static litlen bit lengths
	for(i = 0; i < 144; i++)
		litlentab[i].bits = 8;
	for(i = 144; i < 256; i++)
		litlentab[i].bits = 9;
	for(i = 256; i < 280; i++)
		litlentab[i].bits = 7;
	for(i = 280; i < Nlitlen; i++)
		litlentab[i].bits = 8;

	bitcount := array[MaxHuffBits+1] of { * => 0 };
	bitcount[8] += 144 - 0;
	bitcount[9] += 256 - 144;
	bitcount[7] += 280 - 256;
	bitcount[8] += Nlitlen - 280;

	hufftabinit(litlentab, Nlitlen, bitcount, 9);
	litlendec = decodeinit(litlentab, Nlitlen, 9, 0);

	offtab = array[Noff] of Huff;

	# static offset bit lengths
	for(i = 0; i < Noff; i++)
		offtab[i].bits = 5;

	for(i = 0; i < 5; i++)
		bitcount[i] = 0;
	bitcount[5] = Noff;

	hufftabinit(offtab, Noff, bitcount, 5);
	offdec = decodeinit(offtab, Noff, 5, 0);

	hist = array[InflateBlock] of byte;
}

#
# call prior to processing a deflate-compressed data set
# (e.g., gunzip calls prior to processing each .gz file)
#
reset(): ref InflateIO
{
	if(io == nil) {
		io = ref InflateIO;
		io.ibuf = array[InflateBlock] of byte;
		io.obuf = array[InflateBlock] of byte;
	}
	io.c = chan of int;
	in = 0;
	ein = 0;
	out = 0;
	usehist = 0;
	reg = 0;
	nbits = 0;
	return io;
}

inflate()
{
	for(;;) {
		bfinal := getn(1, 0);
		btype := getn(2, 0);
		case(btype) {
		DeflateUnc =>
			flushbits();
			unclen := int getb() | (int getb() << 8);
			# ignore NLEN
			getb();
			getb();
			for(; unclen > 0; unclen--) {
				# inline putb(getb());
				b := getb();
				if(out >= MaxOff)
					flushout();
				io.obuf[out++] = b;
			}
		DeflateFix =>
			decodeblock(litlendec, offdec);
		DeflateDyn =>
			dynhuff();
		DeflateErr =>
			fatal();
		}
		if(bfinal) {
			if(out) {
				io.c <- = InflateFlushOut | out;
				flag := <- io.c;
				if((flag & InflateAck) == 0)
					exit;
			}
			flushbits();
			# report no. of bytes left unconsumed in io.ibuf
			io.c <- = InflateDone | ((ein - in) + (nbits / 8));
			exit;
		}
	}
}

#
# uncompress a block using given huffman decoding tables
#
decodeblock(litlendec, offdec: ref DeHuff)
{
	b: byte;

	for(;;) {
		sym := decodesym(litlendec);
		if(sym < DeflateEob) {		# literal byte
			# inline putb(byte sym);
			b = byte sym;
			if(out >= MaxOff)
				flushout();
			io.obuf[out++] = b;
		} else if(sym == DeflateEob) {	# End-of-block
			break;
		} else {			# lempel-ziv <length, distance>
			if(sym > LenEnd)
				fatal();
			xbits, xtra: int;
			xbits = litlenextra[sym - LenStart];
			if(xbits)
				xtra = getn(xbits, 0);
			else
				xtra = 0;
			length := litlenbase[sym - LenStart] + xtra;

			sym = decodesym(offdec);
			if(sym >= Noff)
				fatal();
			xbits = offextra[sym];
			if(xbits)
				xtra = getn(xbits, 0);
			else
				xtra = 0;
			dist := offbase[sym] + xtra;
			if(dist > out && usehist == 0)
				fatal();
			for(i := 0; i < length; i++) {
				# inline putb(lzbyte(dist));
				ix := out - dist;
				if(dist <= out)
					b = io.obuf[ix];
				else
					b = hist[MaxOff + ix];
				if(out >= MaxOff)
					flushout();
				io.obuf[out++] = b;
			}
		}
	}
}

#
# decode next symbol in input stream using given huffman decoding table
#
decodesym(dec: ref DeHuff): int
{
	code, bits, n: int;

	l1 := dec.l1;
	nb1 := dec.nb1;
	nb2 := dec.nb2;

	code = getn(nb1, 1);
	l2 := l1[code].l2;
	if(l2 == nil) {		# first level table has answer
		bits = l1[code].bits;
		if(bits == 0)
			fatal();
		if(nb1 > bits) {
			# inline ungetn(nb1 - bits);
			n = nb1 - bits;
			reg = svreg >> (svn - n);
			nbits += n;
		}
		return l1[code].decode;
	}
	# must advance to second-level table
	code = getn(nb2, 1);
	bits = l2[code].bits;
	if(bits == 0)
		fatal();
	if(nb1 + nb2 > bits) {
		# inline ungetn(nb1 + nb2 - bits);
		n = nb1 + nb2 - bits;
		reg = svreg >> (svn - n);
		nbits += n;
	}
	return l2[code].decode;
}

#
# uncompress a block that was encoded with dynamic huffman codes
# RFC 1951, section 3.2.7
#
dynhuff()
{
	hlit := getn(5, 0) + 257;
	hdist := getn(5, 0) + 1;
	hclen := getn(4, 0) + 4;

	runlentab := array[Nclen] of { * => Huff(0, 0) };
	count := array[RunlenBits+1] of { * => 0 };
	for(i := 0; i < hclen; i++) {
		nb := getn(3, 0);
		if(nb) {
			runlentab[clenorder[i]].bits = nb;
			count[nb]++;
		}
	}
	hufftabinit(runlentab, Nclen, count, RunlenBits);
	runlendec := decodeinit(runlentab, Nclen, RunlenBits, 0);

	dlitlendec := decodedyn(runlendec, hlit, Nlitlen, 9);
	doffdec := decodedyn(runlendec, hdist, Noff, 5);
	decodeblock(dlitlendec, doffdec);
}

#
# (1) read a dynamic huffman code from the input stream
# (2) decode it using the run-length huffman code
# (3) return the decode table for the dynamic huffman code
#
decodedyn(runlendec: ref DeHuff, nlen, nsym, nb1: int): ref DeHuff
{
	hufftab := array[nsym] of { * => Huff(0, 0) };
	count := array[MaxHuffBits+1] of { * => 0 };

	maxnb := 0;
	n := 0;
	while(n < nlen) {
		nb := decodesym(runlendec);
		case(nb) {
		0 =>
			n++;
		1 to 15 =>
			if(n >= nsym)
				fatal();
			hufftab[n++].bits = nb;
			count[nb]++;
			if(nb > maxnb)
				maxnb = nb;
		16 =>
			repeat := getn(2, 0) + 3;
			if(n == 0 || n + repeat > nsym)
				fatal();
			prev := hufftab[n-1].bits;
			if(prev == 0) {
				n += repeat;
				break;
			}
			for(i := 0; i < repeat; i++)
				hufftab[n++].bits = prev;
			count[prev] += repeat;
		17 =>
			n += getn(3, 0) + 3;
		18 =>
			n += getn(7, 0) + 11;
		* =>
			fatal();
		}
	}
	hufftabinit(hufftab, nsym, count, maxnb);
	nb2 := 0;
	if(maxnb > nb1)
		nb2 = maxnb - nb1;
	return decodeinit(hufftab, nsym, nb1, nb2);
}

#
# RFC 1951, section 3.2.2
#
hufftabinit(tab: array of Huff, n: int, bitcount: array of int, nbits: int)
{
	nc := array[MaxHuffBits+1] of int;

	code := 0;
	for(bits := 1; bits <= nbits; bits++) {
		code = (code + bitcount[bits-1]) << 1;
		nc[bits] = code;
	}

	for(i := 0; i < n; i++) {
		bits = tab[i].bits;
		# differences from Deflate module:
		#  (1) leave huffman code right-justified in encode
		#  (2) don't reverse it
		if(bits != 0)
			tab[i].encode = nc[bits]++;
	}
}

#
# convert 'array of Huff' produced by hufftabinit()
# into 2-level lookup table for decoding
#
# nb1(nb2): number of bits handled by first(second)-level table
#
decodeinit(tab: array of Huff, n, nb1, nb2: int): ref DeHuff
{
	i, j, k, d: int;

	dehuff := ref DeHuff(array[1<<nb1] of { * => L1(0, 0, nil) }, nb1, nb2);
	l1 := dehuff.l1;

	for(i = 0; i < n; i++) {
		bits := tab[i].bits;
		if(bits == 0)
			continue;
		l1x := tab[i].encode;
		if(l1x >= (1 << bits))
			fatal();
		if(bits <= nb1) {
			d = nb1 - bits;
			l1x <<= d;
			k = l1x + mask[d];
			for(j = l1x; j <= k; j++) {
				l1[j].decode = i;
				l1[j].bits = bits;
			}
			continue;
		}
		# advance to second-level table
		d = bits - nb1;
		l2x := l1x & mask[d];
		l1x >>= d;
		if(l1[l1x].l2 == nil)
			l1[l1x].l2 = array[1<<nb2] of { * => L2(0, 0) };
		l2 := l1[l1x].l2;
		d = (nb1 + nb2) - bits;
		l2x <<= d;
		k = l2x + mask[d];
		for(j = l2x; j <= k; j++) {
			l2[j].decode = i;
			l2[j].bits = bits;
		}
	}

	return dehuff;
}

#
# get next byte from reg
# assumptions:
#  (1) flushbits() has been called
#  (2) ungetn() won't be called after a getb()
#
getb(): byte
{
	if(nbits < 8)
		need(8);
	b := byte reg;
	reg >>= 8;
	nbits -= 8;
	return b;
}

#
# get next n bits from reg; if r != 0, reverse the bits
#
getn(n, r: int): int
{
	if(nbits < n)
		need(n);
	svreg = reg;
	svn = n;
	i := reg & mask[n];
	reg >>= n;
	nbits -= n;
	if(r) {
		if(n <= 8) {
			i = int revtab[i];
			i >>= 8 - n;
		} else {
			i = ((int revtab[i & 16rff]) << 8)
				| (int revtab[i >> 8]);
			i >>= 16 - n;
		}
	}
	return i;
}

#
# ensure that at least n bits are available in reg
#
need(n: int)
{
	while(nbits < n) {
		if(in >= ein) {
			io.c <- = InflateEmptyIn;
			flag_n := <- io.c;
			if((flag_n & InflateAck) == 0)
				exit;
			ein = flag_n ^ InflateAck;
			if(ein == 0)
				fatal();
			in = 0;
		}
		reg = ((int io.ibuf[in++]) << nbits) | reg;
		nbits += 8;
	}
}

# <<< inlined >>>
#
# put most recently gotten n bits back into reg
#
#ungetn(n: int)
#{
#	reg = svreg >> (svn - n);
#	nbits += n;
#}

#
# if partial byte consumed from reg, dispose of remaining bits
#
flushbits()
{
	drek := nbits % 8;
	if(drek) {
		reg >>= drek;
		nbits -= drek;
	}
}

# <<< inlined >>>
#
# put byte b into output buffer
#
#putb(b: byte)
#{
#	if(out >= MaxOff)
#		flushout();
#	io.obuf[out++] = b;
#}

#
# output buffer is full, so flush it
#
flushout()
{
	io.c <- = InflateFlushOut | out;
	flag := <- io.c;
	if((flag & InflateAck) == 0)
		exit;
	buf := hist;
	hist = io.obuf;
	usehist = 1;
	io.obuf = buf;
	out = 0;
}

# <<< inlined >>>
#
# retrieve an earlier byte that was lempel-zivized at distance d
#
#lzbyte(d: int): byte
#{
#	b: byte;
#
#	i := out - d;
#	if(d <= out)
#		b = io.obuf[i];
#	else
#		b = hist[MaxOff + i];
#	return b;
#}

#
# irrecoverable error; invariably denotes data corruption
#
fatal()
{
	fprint(stderr, "inflate: bad format\n");
	io.c <- = InflateError;
	exit;
}
