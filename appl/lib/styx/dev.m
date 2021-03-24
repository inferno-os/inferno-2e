#
# Generalized Styx Helper Functions
# (see /appl/lib/styx/devXXX.b for example usage)
#

Dev: module {
	PATH:		con "/dis/lib/styx/dev.dis";

	Dirtab: adt {
		name:	string;
		qid:		Sys->Qid;
		length:	int;
		perm: 	int;
	};

	Emesgmismatch:	con "message size mismatch";
	Eperm:		con "permission denied";
	Enotdir:		con "not a directory";
	Enotexist:		con "file does not exist";
	Eio:			con "i/o error";
	Eisopen:		con "file already open for I/O";

	init:			fn(m: Styx, name: string);
	devdir:		fn(qid: Sys->Qid, n: string, length: int, user: string, perm: int, db: ref Sys->Dir);
	devgen:		fn(i: int, tab: array of Dirtab, dp: ref Sys->Dir) :int;
	devdirread:		fn(c: ref Chan, d:array of byte, count: int, tab: array of Dirtab): int;
	devattach:		fn(c: ref Chan, spec: string): ref Chan;
	devclone:		fn(c: ref Chan, nc: ref Chan): ref Chan;
	devwalk:		fn(c: ref Chan, name: string, dirtab: array of Dirtab): int;
	devstat:		fn(c: ref Chan, db: array of byte, dirtab: array of Dirtab);
	devopen:		fn(c: ref Chan, mode: int, tab: array of Dirtab):ref Chan;
	devcreate:		fn(c: ref Chan, name: string, mode: int, perm: int);	
	devclunk:		fn(c: ref Chan);
	devremove:		fn(c: ref Chan);
	devwstat:		fn(c: ref Chan, db: array of byte);

	eqqid:			fn(q1: Sys->Qid, q2: Sys->Qid) :int;
	readstr:		fn(off: int, buf: array of byte, n: int, str: string) :int;

};