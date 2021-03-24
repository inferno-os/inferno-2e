implement Dataflash;

include "sys.m";
include "dataflash.m";

UTFself:	con 16r80;	# ascii and UTF sequences are the same (<)
maxrune:	con 8;
bufsize:	con (8*1024);
sys:		Sys;

flushbufs: list of ref SFbuf;	# only write buffers need flushing

##############################################
open( filename: string, mode: int ): ref SFbuf
{
	ib: ref SFbuf;
	buffer := array[bufsize+maxrune] of byte;
	name := filename;

	rtn := array[1] of byte;
	msg := "o" + filename;

	if (sys == nil) sys = load Sys Sys->PATH;

	#open the flash device
	dataflashFD := sys->open( "/dev/sflash01", mode );
	if( dataflashFD == nil ) return nil;

	#open the file on the flash
	aaa := array of byte msg;
	foo1 := sys->write( dataflashFD, aaa, len( aaa ) );
	foo2 := sys->read(  dataflashFD, rtn, len( rtn ) );
	while( foo2 == 0 )
	{
		#wait for the response
		foo2 = sys->read(  dataflashFD, rtn, len( rtn ) );
	}
	
	#test for no file of that name or no free space for a new file
	if( 0 == int rtn[0] ) return nil;
	
	#store information about this file
	ib = ref SFbuf( dataflashFD, filename, buffer, 0, 0, 0, 0, 0, mode, mode );

	#clean the buffer
	if( mode != OREAD ) flushbufs = ib :: flushbufs;
	return ib;
}

#######
flush()
{
	ibs: list of ref SFbuf;

	for (ibs = flushbufs; ibs != nil; ibs = tl ibs)
		(hd ibs).flush();
}

##############################
readchunk( b: ref SFbuf ): int
{
	sys->print("\nreadchunk");
	if( b.filpos != b.bufpos + b.size )
	{
		b.filpos = b.bufpos + b.size;
	}

	#create the command
	xxx := string b.filpos;
	msg := "r" + b.filename + ":" + xxx;
	aaa := array of byte msg;

	#open the file on the flash
	foo1 := sys->write( b.fd, aaa, len( aaa ) );
	i := sys->read(  b.fd, b.buffer, len( b.buffer ) );
	while( i == 0 )
	{
		#wait for the response
		i = sys->read(  b.fd, b.buffer, len( b.buffer ) );
	}

	if( i <= 0 )
	{
		return EOF;
	}

	b.size   += i;
	b.filpos += i;

	return i;
}

##########
writechunk( b: ref SFbuf ): int
{
	sys->print("\nwrite chunk");
	if( b.filpos != b.bufpos)
	{
		#if( (s := sys->seek( b.fd, b.bufpos, 0 )) < 0 )
		#{
			#sys->print("bufio: seek to %d returned %d\n", b.bufpos, s);
			#exit;
		#}
		b.filpos = b.bufpos;
	}

	if( (size := b.size) > bufsize )
	{
		size = bufsize;
	}

	#if( sys->write( b.fd, b.buffer, size ) != size )
	#{
		#return ERROR;
	#}

	#create the command
	xxx := string b.filpos;
	msg := "w" + b.filename + ":" + xxx + ":";
	aaa := array of byte msg;
	bbb := array[len aaa + 256] of byte;
	bbb[0:] = aaa;
	bbb[len aaa:] = b.buffer[0:64];

	#open the file on the flash
	foo1 := sys->write( b.fd, bbb, len bbb );
	i := sys->read(  b.fd, b.buffer, len b.buffer );
	while( i == 0 )
	{
		#wait for the response
		i = sys->read(  b.fd, b.buffer, len b.buffer );
	}

	b.filpos += size;
	b.size   -= size;
	if( b.size )
	{
		b.dirty      = 1;
		b.buffer[0:] = b.buffer[bufsize:bufsize+b.size];
	}
	else
	{
		b.dirty = 0;
	}

	b.bufpos += size;
	b.index  -= size;

	return size;
}

SFbuf.close(b: self ref SFbuf)
{
	ibs : list of ref SFbuf;

	if (b.fd == nil)
		return;
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

SFbuf.flush(b: self ref SFbuf): int
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

##########
SFbuf.seek( b: self ref SFbuf, off, start: int ): int
{
	npos: int;

	if( b.fd == nil )
	{
		return ERROR;
	}

	case( start )
	{
		0 =>	# absolute address
			npos = off;
		1 =>	# offset from current location
			npos = b.bufpos + b.index + off;
		2 =>	# offset from EOF
			npos = -1;
		* =>	return ERROR;
	}

	if( b.bufpos <= npos && npos < b.bufpos + b.size )
	{
		b.index = npos - b.bufpos;
		return npos;
	}

	if( b.dirty && b.flush() < 0 )
	{
		return ERROR;
	}

	b.size  = 0;
	b.index = 0;

	if( (s := sys->seek( b.fd, off, start )) < 0 )
	{
		b.filpos = b.bufpos = 0;
		return ERROR;
	}

	b.bufpos = b.filpos = s;
	return b.bufpos = b.filpos = s;
}

###############################
write2read( b: ref SFbuf ): int
{
	while( b.dirty ) 
	{
		if( b.flush() < 0 )
		{
			return ERROR;
		}
	}

	b.bufpos = b.filpos;
	b.size   = 0;
	b.lastop = OREAD;

	if( readchunk( b ) < 0 )
	{
		return EOF;
	}
	if( b.index > b.size )
	{
		return EOF;
	}

	return 0;
}

##########
SFbuf.read( b: self ref SFbuf, buf: array of byte, n: int ): int
{
	sys->print("\nread");
	k := n;

	if( b.fd == nil || b.mode == OWRITE )
	{
		return ERROR;
	}

	if( b.lastop != OREAD )
	{
		if( write2read( b ) < 0 )
		{
			sys->print(" C");
			return EOF;
		}
	}
	sys->print(" D");

	while( b.size - b.index < k)
	{
		buf[0:] = b.buffer[b.index:b.size];
		buf     = buf[b.size - b.index:];
		k      -= b.size - b.index;

		b.bufpos += b.size;
		b.index   = 0;
		b.size    = 0;
		if( readchunk( b ) < 0 )
		{
			return n-k;
		}
	}
	buf[0:]  = b.buffer[b.index:b.index+k];
	b.index += k;
	return n;
}

SFbuf.getb(b: self ref SFbuf): int
{
	if(b.lastop != OREAD){
		if(b.fd == nil || b.mode == OWRITE)
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

SFbuf.ungetb(b: self ref SFbuf): int
{
	if(b.fd == nil || b.mode == OWRITE || b.lastop != OREAD)
		return ERROR;
	b.index--;
	return 1;
}

SFbuf.getc(b: self ref SFbuf): int
{
	r, i, s:	int;

	if(b.lastop != OREAD){
		if(b.fd == nil || b.mode == OWRITE)
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

SFbuf.ungetc(b: self ref SFbuf): int
{
	if(b.fd == nil || b.mode == OWRITE || b.lastop != OREAD)
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

SFbuf.gets(b: self ref SFbuf, term: int): string
{
	i, s: int;

	if(b.fd == nil || b.mode == OWRITE)
		return nil;
	if(b.lastop != OREAD && write2read(b) < 0)
		return nil;
	str: string;
	ch := -1;
	for (;;) {
		start := b.index;
		n := 0;
		while(b.index < b.size){
			(ch, i, s) = sys->byte2char(b.buffer[0:b.size], b.index);
#			if(s == 0)
#				break;
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

SFbuf.gett(b: self ref SFbuf, s: string): string
{
	r := "";
	ch, i: int;

	if (b.fd == nil || b.mode == OWRITE || (ch = b.getc()) < 0)
		return nil;
	do {
		r[len r] = ch;
		for (i=0; i<len(s); i++)
			if (ch == s[i]) return r;
	} while ((ch = b.getc()) >= 0);
	return r;
}

##########################
read2write( b: ref SFbuf )
{
	# last operation was a read
	b.bufpos += b.index;
	b.size    = 0;
	b.index   = 0;
	b.lastop  = OWRITE;
}

###########
SFbuf.write( b: self ref SFbuf, buf: array of byte, n: int ): int
{
	sys->print("\nWRITE");

	k := n;

	if( b.lastop != OWRITE )
	{
		if( b.fd == nil || b.mode == OREAD )
		{
			return ERROR;
		}
		read2write( b );
	}

	start := 0;
	while( k > 0 )
	{
		nw := bufsize - b.index;
		if( nw > k )
		{
			nw = k;
		}

		end               := start + nw;
		b.buffer[b.index:] = buf[start:end];
		start              = end;
		b.index           += nw;
		k                 -= nw;

		if( b.index > b.size )
		{
			b.size = b.index;
		}

		b.dirty = 1;
		if( b.size == bufsize && writechunk( b ) < 0 )
		{
			sys->print(" error");
			return ERROR;
		}
	}
	return n;
}

SFbuf.putb(b: self ref SFbuf, c: byte): int
{
	if(b.lastop != OWRITE) {
		if(b.fd == nil || b.mode == OREAD)
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

SFbuf.putc(b: self ref SFbuf, c: int): int
{
	if(b.lastop != OWRITE) {
		if (b.fd == nil || b.mode == OREAD)
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

SFbuf.puts(b: self ref SFbuf, s: string): int
{
	if(b.lastop != OWRITE) {
		if (b.fd == nil || b.mode == OREAD)
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

}
