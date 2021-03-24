#
#	Module:		TimedIO
#	Author:		Eric Van Hensbergen (ericvh@lucent.com)
#	Purpose:	Simple timeout-based i/o module which mimicks the
#			standard i/o routines, but provides a millisecond-based
#			timeout.
#
#

TimedIO: module
{
	PATH:		con "/dis/lib/timedio.dis";

	EOF:	con 0;
	ERROR:	con -1;
	TIMEOUT: con -6;

#
# procedure:	read
# arguments:	fd - reference to an open file descriptor
#				buf - receive buffer to be filled in by read
#				nbytes - number of bytes expected
#				timeout - timeout in milliseconds
#

read: fn( fd: ref Sys->FD, buf: array of byte, nbytes: int, timeout: int) :int;

#
# procedure:	write
# arguments:	fd - reference to an open file descriptor
#				buf - buffer to be written
#				nbytes - number of bytes to write
#				timeout - timeout in milliseconds
#

write: fn( fd: ref Sys->FD, buf: array of byte, nbytes: int, timeout: int) :int;

};

