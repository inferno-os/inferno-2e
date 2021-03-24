Dataflash: module
{
	PATH:		con ".";

	SEEKSTART:	con Sys->SEEKSTART;
	SEEKRELA:	con Sys->SEEKRELA;
	SEEKEND:	con Sys->SEEKEND;

	OREAD:		con Sys->OREAD;
	OWRITE:		con Sys->OWRITE;
	ORDWR:		con Sys->ORDWR;

	EOF:		con -1;
	ERROR:		con -2;

	SFbuf: adt {
		seek:		fn(b: self ref SFbuf, n, where: int): int;

		read:		fn(b: self ref SFbuf, a: array of byte, n: int): int;
		write:		fn(b: self ref SFbuf, a: array of byte, n: int): int;

		getb:		fn(b: self ref SFbuf): int;
		getc:		fn(b: self ref SFbuf): int;
		gets:		fn(b: self ref SFbuf, sep: int): string;
		gett:		fn(b: self ref SFbuf, sep: string): string;

		ungetb:		fn(b: self ref SFbuf): int;
		ungetc:		fn(b: self ref SFbuf): int;

		putb:		fn(b: self ref SFbuf, b: byte): int;
		putc:		fn(b: self ref SFbuf, c: int): int;
		puts:		fn(b: self ref SFbuf, s: string): int;

		flush:		fn(b: self ref SFbuf): int;
		close:		fn(b: self ref SFbuf);

		# Internal variables
		fd:             ref Sys->FD;    # the file
		filename:	string;		# name of the dataflash file
		buffer:		array of byte;	# the buffer
		index:		int;		# read/write pointer in buffer
		size:		int;		# characters remaining/written
		dirty:		int;		# needs flushing
		bufpos:		int;		# position in file of buf[0]
		filpos:		int;		# current file pointer
		lastop:		int;		# OREAD or OWRITE
		mode:		int;		# mode of open
	};

	open:		fn(name: string, mode: int): ref SFbuf;

	flush:		fn();
};
