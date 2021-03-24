implement Lock;

include "sys.m";
	sys:	Sys;
include "lock.m";

#
# Semaphore Operations
#

Semaphore.obtain(l: self ref Semaphore)
{
	buffer := array [5] of byte;
	sys->read( l.q[1], buffer, 4 );
}

Semaphore.release(l: self ref Semaphore)
{
	sys->write( l.q[0], array of byte "lock", 4 );
}

Semaphore.init(l: self ref Semaphore)
{
	if (sys == nil)
		sys = load Sys Sys->PATH;
	l.q = array[2] of ref Sys->FD;
	if (sys->pipe( l.q ) < 0)
		sys->raise("Lock: can't allocate a pipe");
	sys->write( l.q[0], array of byte "lock", 4 );
}

#
# Module
#

init() : ref Semaphore
{
	newsemaphore := ref Semaphore;
	newsemaphore.init();
	return newsemaphore;
}
