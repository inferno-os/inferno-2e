#
#	Common reply module.
#

StyxReply: module
{
	PATH:		con "/dis/svc/telcofs/styxreply.dis";

	init:		fn(f: ref Sys->FD);
	shutdown:	fn();

	nopR:		fn(tag: int);
	errorR:		fn(tag: int, ename: string);
	flushR:		fn(tag: int);
	cloneR:		fn(tag, fid: int);
	walkR:		fn(tag, fid: int, qid: Sys->Qid);
	openR:		fn(tag, fid: int, qid: Sys->Qid);
	createR:	fn(tag, fid: int, qid: Sys->Qid);
	readR:		fn(tag, fid, count: int, data: array of byte);
	writeR:		fn(tag, fid, count: int);
	clunkR:		fn(tag, fid: int);
	removeR:	fn(tag, fid: int);
	statR:		fn(tag, fid: int, stat: array of byte);
	wstatR:		fn(tag, fid: int);
	attachR:	fn(tag, fid: int, qid: Sys->Qid);

	fd:		ref Sys->FD;
	errorstatus:	string;
	buffer:		array of byte;
};

#
#	Server specific module.
#

StyxServer: module
{
	FTPFSPATH:	con "/dis/lib/ftpfs.dis";
	ISOFSPATH:	con "/dis/svc/cdfs/cdfs.dis";
	TELCOFSPATH:	con "/dis/svc/telcofs/telcofs.dis";

	init:		fn(a: list of string, r: StyxReply) : string;
	shutdown:	fn();

	nopT:		fn(tag: int);
	flushT:		fn(tag, oldtag: int);
	cloneT:		fn(tag, fid, newfid: int);
	walkT:		fn(tag, fid: int, name: array of byte);
	openT:		fn(tag, fid, mode: int);
	createT:	fn(tag, fid: int, name: array of byte, perm, mode: int);
	readT:		fn(tag, fid: int, offset: big, count: int);
	writeT:		fn(tag, fid: int, offset: big, count: int, data: array of byte);
	clunkT:		fn(tag, fid: int);
	removeT:	fn(tag, fid: int);
	statT:		fn(tag, fid: int);
	wstatT:		fn(tag, fid: int, stat: array of byte);
	attachT:	fn(tag, fid: int, uid: array of byte, aname: array of byte);

	reply:		StyxReply;
	ctxt:		ref Draw->Context;
	geometry:	string;
};

#
#	Styx protocol module.
#

Styx: module
{
	PATH:		con "/dis/svc/telcofs/styx.dis";

	MAXMSG:		con 16 + Sys->ATOMICIO;
	STATSZ:		con 116;

	Tnop:		con byte 0;
	Rnop:		con byte 1;
	Terror:		con byte 2;
	Rerror:		con byte 3;
	Tflush:		con byte 4;
	Rflush:		con byte 5;
	Tclone:		con byte 6;
	Rclone:		con byte 7;
	Twalk:		con byte 8;
	Rwalk:		con byte 9;
	Topen:		con byte 10;
	Ropen:		con byte 11;
	Tcreate:	con byte 12;
	Rcreate:	con byte 13;
	Tread:		con byte 14;
	Rread:		con byte 15;
	Twrite:		con byte 16;
	Rwrite:		con byte 17;
	Tclunk:		con byte 18;
	Rclunk:		con byte 19;
	Tremove:	con byte 20;
	Rremove:	con byte 21;
	Tstat:		con byte 22;
	Rstat:		con byte 23;
	Twstat:		con byte 24;
	Rwstat:		con byte 25;
	Tattach:	con byte 28;
	Rattach:	con byte 29;

	serve:		fn(a: list of string, s: StyxServer) : string;

	server:		StyxServer;
	reply:		StyxReply;
};
