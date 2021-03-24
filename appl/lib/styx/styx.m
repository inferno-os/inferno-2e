#
#	Common reply module.
#

StyxReply: module
{
	PATH:		con "/dis/lib/styx/styxreply.dis";

	init:		fn(f: ref Sys->FD);

	nopR:		fn(buf: array of byte, tag: int);
	errorR:	fn(buf: array of byte,tag: int, ename: string);
	flushR:	fn(buf: array of byte,tag: int);
	cloneR:	fn(buf: array of byte,tag, fid: int);
	walkR:	fn(buf: array of byte,tag, fid: int, qid: Sys->Qid);
	openR:	fn(buf: array of byte,tag, fid: int, qid: Sys->Qid);
	createR:	fn(buf: array of byte,tag, fid: int, qid: Sys->Qid);
	readR:	fn(buf: array of byte,tag, fid, count: int, data: array of byte);
	writeR:	fn(buf: array of byte,tag, fid, count: int);
	clunkR:	fn(buf: array of byte,tag, fid: int);
	removeR:	fn(buf: array of byte,tag, fid: int);
	statR:		fn(buf: array of byte,tag, fid: int, stat: array of byte);
	wstatR:	fn(buf: array of byte,tag, fid: int);
	attachR:	fn(buf: array of byte,tag, fid: int, qid: Sys->Qid);

	errorstatus:	string;
	fd:		ref Sys->FD;
};

#
#	Server specific module.
#

Chan:	adt {
	qid:		Sys->Qid;
	busy:		int;
	open:		int;
	fid:		int;
	offset:	int;
	path:		string;
	uname:	string;
};

StyxServer: module
{
	init:		fn(ctxt: ref Draw->Context, args: list of string);
	reset:		fn();

	attach:	fn(c: ref Chan, spec: string): ref Chan;
	clone:		fn(c: ref Chan, nc: ref Chan): ref Chan;
	walk:		fn(c: ref Chan, name: string): int;
	stat:		fn(c: ref Chan, db: array of byte);
	open:		fn(c: ref Chan, mode: int): ref Chan;
	create:	fn(c: ref Chan, name: string, mode, perm: int);
	clunk:		fn(c: ref Chan);
	read:		fn(c: ref Chan, buf:array of byte, count: int, offset: int): int;
	write:		fn(c: ref Chan, buf:array of byte, count: int, offset: int): int;
	remove:	fn(c: ref Chan);
	wstat:	fn(c: ref Chan, db: array of byte);
};

#
#	Styx protocol module.
#

Styx: module
{
	PATH:		con "/dis/lib/styx/styx.dis";

	MAXMSG:	con 16 + Sys->ATOMICIO;
	STATSZ:	con 116;

	init:		fn(s: StyxServer, fd: ref Sys->FD);

	Tnop:		con byte 0;
	Rnop:		con byte 1;
	Terror:	con byte 2;
	Rerror:	con byte 3;
	Tflush:	con byte 4;
	Rflush:	con byte 5;
	Tclone:	con byte 6;
	Rclone:	con byte 7;
	Twalk:	con byte 8;
	Rwalk:	con byte 9;
	Topen:	con byte 10;
	Ropen:	con byte 11;
	Tcreate:	con byte 12;
	Rcreate:	con byte 13;
	Tread:	con byte 14;
	Rread:	con byte 15;
	Twrite:	con byte 16;
	Rwrite:	con byte 17;
	Tclunk:	con byte 18;
	Rclunk:	con byte 19;
	Tremove:	con byte 20;
	Rremove:	con byte 21;
	Tstat:		con byte 22;
	Rstat:		con byte 23;
	Twstat:	con byte 24;
	Rwstat:	con byte 25;
	Tattach:	con byte 28;
	Rattach:	con byte 29;

	# Helper Functions
	put2:		fn(a: array of byte, v: int);
	put4:		fn(a: array of byte, v: int);
	put8:		fn(a: array of byte, v: big);

	get2:		fn(a: array of byte): int;
	get4:		fn(a: array of byte): int;
	get8:		fn(a: array of byte): big;

	byte2name:	fn(a: array of byte, n: int): string;
	name2byte:	fn(a: array of byte, s: string, n: int);

	convD2M:	fn(nil: ref Sys->Dir, nil: array of byte): int;
	convM2D:	fn(nil: array of byte, nil: ref Sys->Dir): int;

};
