#
# builtin image decoding interface
#

Readimage: module
{
	PATH:	con	"$Readimage";

	readimagedata:	fn(fd: ref Iobuf, multi: int): (array of ref Rawimage, string);
	remap:	fn(i: ref Rawimage, d: ref Draw->Display, errdiff: int): (ref Draw->Image, string);

##### from bufio.m : must match contents of Bufio->Iobuf
	OREAD:		con Sys->OREAD;
	OWRITE:		con Sys->OWRITE;
	ORDWR:		con Sys->ORDWR;

	Iobuf: adt {
		# Internal variables
		fd:		ref Sys->FD;	# the file
		buffer:	array of byte;	# the buffer
		index:	int;		# read/write pointer in buffer
		size:		int;		# characters remaining/written
		dirty:	int;		# needs flushing
		bufpos:	int;		# position in file of buf[0]
		filpos:	int;		# current file pointer
		lastop:	int;		# OREAD or OWRITE
		mode:	int;		# mode of open
	};

##### from imagefile.m : must match contents of RImagefile->Rawimage
	CRGB:	con 0;	# three channels, no map
	CY:		con 1;	# one channel, luminance
	CRGB1:	con 2;	# one channel, map present
	CINF:	con 3;	# one channel, inferno format
	CCINF:	con 4;	# one channel, compressed inferno format
	
	Rawimage: adt
	{
		r:		Draw->Rect;
		cmap:	array of byte;
		transp:	int;	# transparency flag (only for nchans=1)
		trindex:	byte;	# transparency index
		nchans:	int;
		chans:	array of array of byte;
		chandesc:	int;
	
		fields:	int;    # defined by format
	};
};
