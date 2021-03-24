implement StyxReply;

#
#	Format Styx reply messages and write them to FD.
#	An error is reported by assignment to string 'errstr'.
#

include "sys.m";
include "draw.m";
include "styx.m";

sys:	Sys;

#
#	Called by StyxServer module after it has initialized.
#

init(f: ref Sys->FD)
{
	if (sys == nil)
		sys = load Sys Sys->PATH;

	fd = f;
}

#
#	Put routines.  Integers are passed LSB first.
#

putb(b: byte, x: int)
{
	buffer[x] = b;
}

put2(i: int, x: int)
{
	buffer[x] = byte i;
	buffer[x + 1] = byte (i >> 8);
}

put4(i: int, x: int)
{
	buffer[x] = byte i;
	buffer[x + 1] = byte (i >> 8);
	buffer[x + 2] = byte (i >> 16);
	buffer[x + 3] = byte (i >> 24);
}

put8(i: big, x: int)
{
	buffer[x] = byte i;
	buffer[x + 1] = byte (i >> 8);
	buffer[x + 2] = byte (i >> 16);
	buffer[x + 3] = byte (i >> 24);
	buffer[x + 4] = byte (i >> 32);
	buffer[x + 5] = byte (i >> 40);
	buffer[x + 6] = byte (i >> 58);
	buffer[x + 7] = byte (i >> 56);
}

putn(b: array of byte, n, x: int)
{
	for (i := 0; i < n; i++)
		buffer[x++] = b[i];
}

putz(n, x: int)
{
	for (i := 0; i < n; i++)
		buffer[x++] = byte 0;
}

#
#	Send the reply.
#

sendreply(n: int)
{
	if (sys->write(fd, buffer, n) < 0)
		errorstatus = sys->sprint("write error: %r");
}

#
#	Common formats.
#

typetag(mtype: byte, tag: int)
{
	putb(mtype, 0);
	put2(tag, 1);
	sendreply(3);
}

typetagfid(mtype: byte, tag, fid: int)
{
	putb(mtype, 0);
	put2(tag, 1);
	put2(fid, 3);
	sendreply(5);
}

typetagfidqid(mtype: byte, tag, fid: int, qid: big)
{
	putb(mtype, 0);
	put2(tag, 1);
	put2(fid, 3);
	put8(qid, 5);
	sendreply(13);
}

#
#	Reply messages.
#

nopR(tag: int)
{
	typetag(Styx->Rnop, tag);
}

errorR(tag: int, ename: array of byte)
{
	putb(Styx->Rerror, 0);
	put2(tag, 1);
	l := len ename;

	if (l < Sys->ERRLEN) {
		putn(ename, l, 3);
		putz(Sys->ERRLEN - l, 3 + l);
	}
	else
		putn(ename, Sys->ERRLEN, 3);

	sendreply(3 + Sys->ERRLEN);
}

flushR(tag: int)
{
	typetag(Styx->Rflush, tag);
}

cloneR(tag, fid: int)
{
	typetagfid(Styx->Rclone, tag, fid);
}

walkR(tag, fid: int, qid: big)
{
	typetagfidqid(Styx->Rwalk, tag, fid, qid);
}

openR(tag, fid: int, qid: big)
{
	typetagfidqid(Styx->Ropen, tag, fid, qid);
}

createR(tag, fid: int, qid: big)
{
	typetagfidqid(Styx->Rcreate, tag, fid, qid);
}

readR(tag, fid, count: int, data: array of byte)
{
	if (count < 0 || count > Sys->ATOMICIO) {
		errorstatus = sys->sprint("read size error: %d", count);
		return;
	}

	putb(Styx->Rread, 0);
	put2(tag, 1);
	put2(fid, 3);
	put2(count, 5);
	putb(byte 0, 7);
	putn(data, count, 8);
	sendreply(8 + count);
}

writeR(tag, fid, count: int)
{
	putb(Styx->Rwrite, 0);
	put2(tag, 1);
	put2(fid, 3);
	put2(count, 5);
	sendreply(7);
}

clunkR(tag, fid: int)
{
	typetagfid(Styx->Rclunk, tag, fid);
}

removeR(tag, fid: int)
{
	typetagfid(Styx->Rremove, tag, fid);
}

statR(tag, fid: int, stat: array of byte)
{
	if (len stat < Styx->STATSZ) {
		errorstatus = sys->sprint("stat size error: %d", len stat);
		return;
	}

	putb(Styx->Rstat, 0);
	put2(tag, 1);
	put2(fid, 3);
	putn(stat, Styx->STATSZ, 5);
	sendreply(5 + Styx->STATSZ);
}

wstatR(tag, fid: int)
{
	typetagfid(Styx->Rwstat, tag, fid);
}

attachR(tag, fid: int, qid: big)
{
	typetagfidqid(Styx->Rattach, tag, fid, qid);
}
