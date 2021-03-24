implement StyxReply;

#
#	Module:		Styx
#	Author:		Eric Van Hensbergen
#	Purpose:	Pack Styx Messages
#	History:		Based on original StyxReply module by Bruce Ellis
#

include "sys.m";
	sys:	Sys;

include "draw.m";
include "styx.m";
	styx:	Styx;
	Tnop, Rnop, Terror, Rerror, Tflush, Rflush, Tclone, Rclone,
	Twalk, Rwalk, Topen, Ropen, Tcreate, Rcreate, Tread, Rread,
	Twrite, Rwrite, Tclunk, Rclunk, Tremove, Rremove, Tstat, Rstat,
	Twstat, Rwstat, Tattach, Rattach :import Styx;
	put2, put4, put8, name2byte:import styx;

#
#	Called by StyxServer module after it has initialized.
#

init(f: ref Sys->FD)
{
	if (sys == nil)
		sys = load Sys Sys->PATH;

	if (styx == nil)
		styx = load Styx Styx->PATH;

	fd = f;
}

#
#	Send the reply.
#

sendreply(buf: array of byte, n: int)
{
	if (sys->write(fd, buf, n) < 0)
		errorstatus = sys->sprint("write error: %r");
}

#
#	Common formats.
#

typetag(buf: array of byte, mtype: byte, tag: int)
{
	buf[0] = mtype;
	put2(buf[1:],tag);
	sendreply(buf, 3);
}

typetagfid(buf: array of byte, mtype: byte, tag, fid: int)
{
	buf[0] = mtype;
	
	put2(buf[1:],tag);
	put2(buf[3:],fid);
	sendreply(buf, 5);
}

typetagfidqid(buf: array of byte,mtype: byte, tag, fid: int, qid: Sys->Qid)
{
	buf[0] = mtype;
	
	put2(buf[1:],tag);
	put2(buf[3:],fid);
	put4(buf[5:],qid.path);
	put4(buf[9:],qid.vers);
	sendreply(buf, 13);
}

#
#	Reply messages.
#

nopR(buf: array of byte, tag: int)
{
	typetag(buf, Styx->Rnop, tag);
}

errorR(buf: array of byte, tag: int, ename: string)
{
	buf[0] = Styx->Rerror;
	put2(buf[1:], tag);
	l := len ename;
	name2byte(buf[3:],ename, Sys->ERRLEN);

	sendreply(buf, 3 + Sys->ERRLEN);
}

flushR(buf: array of byte, tag: int)
{
	typetag(buf, Styx->Rflush, tag);
}

cloneR(buf: array of byte,tag, fid: int)
{
	typetagfid(buf, Styx->Rclone, tag, fid);
}

walkR(buf: array of byte,tag, fid: int, qid: Sys->Qid)
{
	typetagfidqid(buf, Styx->Rwalk, tag, fid, qid);
}

openR(buf: array of byte,tag, fid: int, qid: Sys->Qid)
{
	typetagfidqid(buf, Styx->Ropen, tag, fid, qid);
}

createR(buf: array of byte,tag, fid: int, qid: Sys->Qid)
{
	typetagfidqid(buf, Styx->Rcreate, tag, fid, qid);
}

readR(buf: array of byte,tag, fid, count: int, data: array of byte)
{
	if (count < 0 || count > Sys->ATOMICIO) {
		errorstatus = sys->sprint("read size error: %d", count);
		return;
	}

	buf[0] = Styx->Rread;
	put2(buf[1:], tag);
	put2(buf[3:], fid);
	put2(buf[5:], count);
	buf[7] = byte 0;
	buf[8:] = data[:count];
	sendreply(buf, 8 + count);
}

writeR(buf: array of byte,tag, fid, count: int)
{
	buf[0] = Styx->Rwrite;
	put2(buf[1:], tag);
	put2(buf[3:], fid);
	put2(buf[5:], count);
	sendreply(buf, 7);
}

clunkR(buf: array of byte,tag, fid: int)
{
	typetagfid(buf, Styx->Rclunk, tag, fid);
}

removeR(buf: array of byte,tag, fid: int)
{
	typetagfid(buf, Styx->Rremove, tag, fid);
}

statR(buf: array of byte,tag, fid: int, stat: array of byte)
{
	if (len stat < Styx->STATSZ) {
		errorstatus = sys->sprint("stat size error: %d", len stat);
		return;
	}
	
	buf[0] = Styx->Rstat;
	put2(buf[1:], tag);
	put2(buf[3:], fid);
	buf[5:] = stat[:Styx->STATSZ];
	sendreply(buf, 5 + Styx->STATSZ);
}

wstatR(buf: array of byte,tag, fid: int)
{
	typetagfid(buf, Styx->Rwstat, tag, fid);
}

attachR(buf: array of byte,tag, fid: int, qid: Sys->Qid)
{
	typetagfidqid(buf, Styx->Rattach, tag, fid, qid);
}
