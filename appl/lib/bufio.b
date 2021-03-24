implement Bufio;

include "sys.m";
include "bufio.m";

UTFself:	con 16r80;	# ascii and UTF sequences are the same (<)
maxrune:	con 8;
bufsize:	con (8*1024);
sys:		Sys;

filler: adt
{
	iobuf:	ref Iobuf;
	fill:	BufioFill;
	next:	cyclic ref filler;
};

fillers:	ref filler;
flushbufs:	list of ref Iobuf;	# only write buffers need flushing

create(filename: string, mode, perm: int): ref Iobuf
{
	fd: ref Sys->FD;
	ib: ref Iobuf;
	buffer := array[bufsize+maxrune] of byte;

	if (sys == nil)
		sys = load Sys Sys->PATH;

	if ((fd = sys->create(filename, mode, perm)) == nil)
		return nil;
	ib = ref Iobuf(fd, buffer, 0, 0, 0, 0, 0, mode, mode);
	if (mode != OREAD)
		flushbufs = ib :: flushbufs;
	return ib;
}

open(filename: string, mode: int): ref Iobuf
{
	fd: ref Sys->FD;
	ib: ref Iobuf;
	buffer := array[bufsize+maxrune] of byte;

	if (sys == nil)
		sys = load Sys Sys->PATH;

	if ((fd = sys->open(filename, mode)) == nil)
		return nil;
	ib = ref Iobuf(fd, buffer, 0, 0, 0, 0, 0, mode, mode);
	if (mode != OREAD)
		flushbufs = ib :: flushbufs;
	return ib;
}

fopen(fd: ref Sys->FD, mode: int): ref Iobuf
{
	ib:	ref Iobuf;
	buffer := array[bufsize+maxrune] of byte;

	if (sys == nil)
		sys = load Sys Sys->PATH;

	if ((filpos := sys->seek(fd, 0, 1)) < 0)
		filpos = 0;
	ib = ref Iobuf(fd, buffer, 0, 0, 0, filpos, filpos, mode, mode);
	if (mode != OREAD)
		flushbufs = ib :: flushbufs;
	return ib;
}

sopen(input: string): ref Iobuf
{
	if (sys == nil)
		sys = load Sys Sys->PATH;
	b := array of byte input;
	return ref Iobuf(nil, b, 0, len b, 0, 0, 0, OREAD, OREAD);
}

flush()
{
	ibs: list of ref Iobuf;

	for (ibs = flushbufs; ibs != nil; ibs = tl ibs)
		(hd ibs).flush();
}

fill(b: ref Iobuf, dofill: int): int
{
	prev: ref filler;
	for (f := fillers; f != nil; f = f.next) {
		if (f.iobuf == b) {
			n := EOF;
			if (dofill)
				n = f.fill->fill(b);
			if (n == EOF) {
				if (prev == nil)
					fillers = f.next;
				else
					prev.next = f.next;
			}
			return n;
		}
		prev = f;
	}
	return EOF;
}

readchunk(b: ref Iobuf): int
{
	if (b.fd == nil)
		return fill(b, 1);
	if (b.filpos != b.bufpos + b.size) {
		if ((s := sys->seek(b.fd, b.bufpos + b.size, 0)) < 0) {
			sys->print("bufio: seek to %d returned %d\n",
				b.bufpos + b.size, s);
exit;
		}
		b.filpos = b.bufpos + b.size;
	}
	if ((i := sys->read(b.fd, b.buffer[b.size:], bufsize)) <= 0)
		return EOF;
	b.size += i;
	b.filpos += i;
	return i;
}

writechunk(b: ref Iobuf): int
{
	if (b.fd == nil)
		return ERROR;
	if (b.filpos != b.bufpos) {
		if ((s := sys->seek(b.fd, b.bufpos, 0)) < 0) {
			sys->print("bufio: seek to %d returned %d\n",
				b.bufpos, s);
exit;
		}
		b.filpos = b.bufpos;
	}
	if ((size := b.size) > bufsize) size = bufsize;
	if (sys->write(b.fd, b.buffer, size) != size)
		return ERROR;
	b.filpos += size;
	b.size -= size;
	if (b.size) {
		b.dirty = 1;
		b.buffer[0:] = b.buffer[bufsize:bufsize+b.size];
	} else
		b.dirty = 0;
	b.bufpos += size;
	b.index -= size;
	return size;
}

Iobuf.close(b: self ref Iobuf)
{
	ibs : list of ref Iobuf;

	if (b.fd == nil) {
		fill(b, 0);
		return;
	}
	if (b.dirty)
		b.flush();
	b.fd = nil;
	b.buffer = nil;
	ibs = flushbufs;
	flushbufs = nil;
	while(ibs != nil) {
		if (hd ibs != b)
			flushbufs = (hd ibs) :: flushbufs;
		ibs = tl ibs;
	}
}

Iobuf.flush(b: self ref Iobuf): int
{
	if (b.fd == nil)
		return ERROR;
	while (b.dirty) {
		if (writechunk(b) < 0)
			return ERROR;
		if (b.index < 0) {
			b.bufpos += b.index;
			b.index = 0;
		}
	}
	return 0;
}

Iobuf.seek(b: self ref Iobuf, off, start: int): int
{
	npos: int;

	if (b.fd == nil) return ERROR;
	case (start) {
	0 =>	# absolute address
		npos = off;
	1 =>	# offset from current location
		npos = b.bufpos + b.index + off;
	2 =>	# offset from EOF
		npos = -1;
	* =>	return ERROR;
	}
	if (b.bufpos <= npos && npos < b.bufpos + b.size) {
		b.index = npos - b.bufpos;
		return npos;
	}
	if (b.dirty && b.flush() < 0)
		return ERROR;
	b.size = 0;
	b.index = 0;
	if ((s := sys->seek(b.fd, off, start)) < 0) {
		b.filpos = b.bufpos = 0;
		return ERROR;
	}
	b.bufpos = b.filpos = s;
	return b.bufpos = b.filpos = s;
}

write2read(b: ref Iobuf): int
{
	while (b.dirty) 
		if (b.flush() < 0)
			return ERROR;
	b.bufpos = b.filpos;
	b.size = 0;
	b.lastop = OREAD;
	if (readchunk(b) < 0)
		return EOF;
	if (b.index > b.size)
		return EOF;
	return 0;
}

Iobuf.read(b: self ref Iobuf, buf: array of byte, n: int): int
{
	k := n;

	if (b.mode == OWRITE)
		return ERROR;
	if (b.lastop != OREAD)
		if (write2read(b) < 0) return EOF;
	while (b.size - b.index < k) {
		buf[0:] = b.buffer[b.index:b.size];
		buf = buf[b.size - b.index:];
		k -= b.size - b.index;

		b.bufpos += b.size;
		b.index = 0;
		b.size = 0;
		if (readchunk(b) < 0)
			return n-k;
	}
	buf[0:] = b.buffer[b.index:b.index+k];
	b.index += k;
	return n;
}

Iobuf.getb(b: self ref Iobuf): int
{
	if(b.lastop != OREAD){
		if(b.mode == OWRITE)
			return ERROR;
		if(write2read(b) < 0)
			return EOF;
	}
	if (b.index == b.size) {
		b.bufpos += b.index;
		b.index = 0;
		b.size = 0;
		if (readchunk(b) < 0)
			return EOF;
	}
	return int b.buffer[b.index++];
}

Iobuf.ungetb(b: self ref Iobuf): int
{
	if(b.mode == OWRITE || b.lastop != OREAD)
		return ERROR;
	b.index--;
	return 1;
}

Iobuf.getc(b: self ref Iobuf): int
{
	r, i, s:	int;

	if(b.lastop != OREAD){
		if(b.mode == OWRITE)
			return ERROR;
		if(write2read(b) < 0)
			return EOF;
	}
	for(;;) {
		if(b.index < b.size) {
			r = int b.buffer[b.index];
			if(r < UTFself){
				b.index++;
				return r;
			}
			(r, i, s) = sys->byte2char(b.buffer[0:b.size], b.index);
			if (i != 0) {
				b.index += i;
				return r;
			}
			b.buffer[0:] = b.buffer[b.index:b.size];
		}
		b.bufpos += b.index;
		b.size -= b.index;
		b.index = 0;
		if (readchunk(b) < 0)
			return EOF;
	}
	# Not reached:
	return -1;
}

Iobuf.ungetc(b: self ref Iobuf): int
{
	if(b.mode == OWRITE || b.lastop != OREAD)
		return ERROR;
	stop := b.index - Sys->UTFmax;
	if(stop < 0)
		stop = 0;
	buf := b.buffer[0:b.size];
	for(i := b.index-1; i >= stop; i--){
		(r, n, s) := sys->byte2char(buf, i);
		if(s && i + n == b.index){
			b.index = i;
			return 1;
		}
	}

	b.index--;
	return 1;
}

Iobuf.gets(b: self ref Iobuf, term: int): string
{
	i: int;

	if(b.mode == OWRITE)
		return nil;
	if(b.lastop != OREAD && write2read(b) < 0)
		return nil;
	str: string;
	ch := -1;
	for (;;) {
		start := b.index;
		n := 0;
		while(b.index < b.size){
			(ch, i, nil) = sys->byte2char(b.buffer[0:b.size], b.index);
			if(i == 0)	# too few bytes for full Rune
				break;
			n += i;
			b.index += i;
			if(ch == term)
				break;
		}
		if(n > 0)
			str += string b.buffer[start:start+n];
		if(ch == term)
			return str;
		b.buffer[0:] = b.buffer[b.index:b.size];
		b.bufpos += b.index;
		b.size -= b.index;
		b.index = 0;
		if (readchunk(b) < 0)
			break;
	}
	return str;	# nil at EOF
}

Iobuf.gett(b: self ref Iobuf, s: string): string
{
	r := "";
	ch, i: int;

	if (b.mode == OWRITE || (ch = b.getc()) < 0)
		return nil;
	do {
		r[len r] = ch;
		for (i=0; i<len(s); i++)
			if (ch == s[i]) return r;
	} while ((ch = b.getc()) >= 0);
	return r;
}

read2write(b: ref Iobuf)
{
	# last operation was a read
	b.bufpos += b.index;
	b.size = 0;
	b.index = 0;
	b.lastop = OWRITE;
}

Iobuf.write(b: self ref Iobuf, buf: array of byte, n: int): int
{
	k := n;

	if(b.lastop != OWRITE) {
		if(b.mode == OREAD)
			return ERROR;
		read2write(b);
	}
	start := 0;
	while(k > 0){
		nw := bufsize - b.index;
		if(nw > k)
			nw = k;
		end := start + nw;
		b.buffer[b.index:] = buf[start:end];
		start = end;
		b.index += nw;
		k -= nw;
		if(b.index > b.size)
			b.size = b.index;
		b.dirty = 1;
		if(b.size == bufsize && writechunk(b) < 0)
			return ERROR;
	}
	return n;
}

Iobuf.putb(b: self ref Iobuf, c: byte): int
{
	if(b.lastop != OWRITE) {
		if(b.mode == OREAD)
			return ERROR;
		read2write(b);
	}
	b.buffer[b.index++] = c;
	if(b.index > b.size)
		b.size = b.index;
	b.dirty = 1;
	if(b.size >= bufsize) {
		if (b.fd == nil)
			return ERROR;
		if (writechunk(b) < 0)
			return ERROR;
	}
	return 0;
}

Iobuf.putc(b: self ref Iobuf, c: int): int
{
	if(b.lastop != OWRITE) {
		if (b.mode == OREAD)
			return ERROR;
		read2write(b);
	}
	if(c < UTFself)
		b.buffer[b.index++] = byte c;
	else
		b.index += sys->char2byte(c, b.buffer, b.index);
	if (b.index > b.size)
		b.size = b.index;
	b.dirty = 1;
	if (b.size >= bufsize) {
		if (writechunk(b) < 0)
			return ERROR;
	}
	return 0;
}

Iobuf.puts(b: self ref Iobuf, s: string): int
{
	if(b.lastop != OWRITE) {
		if (b.mode == OREAD)
			return ERROR;
		read2write(b);
	}
	n := len s;
	ind := b.index;
	buf := b.buffer;
	for(i := 0; i < n; i++){
		c := s[i];
		if(c < UTFself)
			buf[ind++] = byte c;
		else
			ind += sys->char2byte(c, buf, ind);
		if(ind >= bufsize){
			b.index = ind;
			if(ind > b.size)
				b.size = ind;
			b.dirty = 1;
			if(writechunk(b) < 0)
				return ERROR;
			ind = b.index;
		}
	}
	b.dirty = b.index != ind;
	b.index = ind;
	if(ind > b.size)
		b.size = ind;
	return n;

#	r:	int;
#
#	a := array of byte s;
#	if ((r = b.write(a, len a)) < len a) {
#		if (r <= 0)
#			return r;
#		return len string a[0:r];
#	}
#	return len s;
}

Iobuf.setfill(b: self ref Iobuf, fill: BufioFill)
{
	for (f := fillers; f != nil; f = f.next) {
		if (f.iobuf == b) {
			f.fill = fill;
			return;
		}
	}
	fillers = ref filler(b, fill, fillers);
}
