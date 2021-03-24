implement Dev;

#
# 	Module:		dev.b
#	Author:		Eric Van Hensbergen
#	Purpose:		Generic helper functions for sythetic file systems
#	History:		Based on dev.c
#

include "sys.m";
	sys:	Sys;
	Qid, Dir,CHDIR: import Sys;

include "draw.m";
include "styx.m";
	styx:		Styx;
	convD2M:	import styx;

include "dev.m";

#
# Globals
#

theuser: 	string;
starttime: 	int;
eve:		con "inferno";
devname:	string;

#
# Helper Functions
#

eqqid( qid: Sys->Qid , qid2: Sys->Qid ) : int
{
	return ((qid.path == qid2.path) && (qid.vers == qid2.vers));
}

getuser(): string
{
	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil)
		return eve;

	buf := array[Sys->NAMELEN] of byte;
	n := sys->read(fd, buf, len buf);
	if(n <= 0)
		return eve;

	return string buf[0:n];
}

readstr(off: int, buf: array of byte, n: int, str: string) : int
{
	size := len str;

	if (off > size)
		return 0;
	if (off + n > size)
		n = size-off;
	tmpbuf :=  array of byte str[off:(off+n)];
	buf[0:] = tmpbuf[0:];
	return len tmpbuf;
}

init( m: Styx, name: string )
{
	sys = load Sys Sys->PATH;
	devname = name;
	styx = m;
	if (styx == nil)
		sys->raise("fail: Nil styx module passed");

	theuser = getuser();
	starttime = sys->millisec()/1000;

}

#
# generic functions
#

devdir(qid: Qid, n: string, length: int, user: string, perm: int, db: ref Dir)
{
	db.name = n;
	db.qid = qid;
	db.dtype = 0;	
	db.dev = 0;	
	if(qid.path & CHDIR)
		db.mode = CHDIR|perm;
	else
		db.mode = perm;

	db.atime = sys->millisec() / 1000;
	db.mtime = starttime;
	db.length = length;
	db.uid =  user;
	db.gid = user;
}

devgen(i: int, tab: array of Dirtab, dp: ref Dir) :int
{
	if (tab == nil || i>=len tab)
		return -1;
	
	devdir(tab[i].qid, tab[i].name, tab[i].length, theuser, tab[i].perm, dp);
	return 1;
}

devattach(c: ref Chan, nil: string): ref Chan
{
	qid: Qid;

	qid = (Qid)(CHDIR, 0);
	c.path = devname;
	c.busy = 1;
	c.qid = qid;
	c.offset = 0;
	c.open = 0;
	return c;
}

devclone(c: ref Chan, nc: ref Chan): ref Chan
{
	if (c.open) {
		sys->raise("fail: "+Eisopen);
		return nil;
	}

	if (c.busy == 0) {
		sys->raise("fail: "+Enotexist);
		return nil;
	}

	nc.busy = 1;
	nc.open = 0;
	nc.qid = c.qid;
	nc.path = c.path;
	nc.uname = c.uname;
	return nc;
}

devwalk(c: ref Chan, name: string, tab: array of Dirtab): int
{
	dir := ref Dir;

	if ((c.qid.path & CHDIR) == 0) {
		sys->raise("fail: "+Enotdir);
		return -1;
	}

	if (name == ".")
		return 1;
	else for (i:=0;;i++) {
		if (devgen(i, tab, dir)  < 0) {
			sys->raise("fail: "+Enotexist);
			return -1;
		}
		if (name == dir.name)  
			break;
	}

	c.path = name;
	c.qid = dir.qid;
	return 1;
}

devclunk(c: ref Chan)
{
	c.path = nil;
	c.uname = nil;
	c.busy = 0;
	c.offset = 0;
	c.open = 0;
}

devstat(c: ref Chan, db: array of byte, tab: array of Dirtab)
{
	i:	int;
	dir:=	ref Dir;

	for(i=0;; i++)
		case devgen(i, tab, dir){
		-1 => 
			if (c.qid.path & CHDIR) {
				devdir(c.qid, c.path, i*Styx->STATSZ, theuser, CHDIR|8r555, dir);
				convD2M(dir,db);
				return;
			}
			sys->raise("fail: "+Enotexist);
		0 =>
			break;
		1 =>
			if (eqqid(c.qid,dir.qid)) {
				convD2M(dir, db);
				return;
			}
			break;
		}
}

devdirread(c: ref Chan, db:array of byte, n: int, tab: array of Dirtab): int
{
	k, m:	int;
	dir := ref Dir;
	pos:= 0;
	k = c.offset/Styx->STATSZ;

	for(m=0; m<n; k++) {
		case devgen(k, tab, dir){
		-1 =>
			return m;

		0 =>
			c.offset += Styx->STATSZ;
			break;

		1 =>
			convD2M(dir, db[pos:]);
			m += Styx->STATSZ;
			pos += Styx->STATSZ;
			break;
		}
	}

	return m;
}

devopen(c: ref Chan, omode: int, tab: array of Dirtab): ref Chan
{
	i :int;

	dir:= ref Dir;
	mode, t: int;
	access :=  array[] of { 8r400, 8r200, 8r600, 8r100 };

	if (c.open) {
		sys->raise("fail: "+Eisopen);
		return nil;
	}

loop:	for(i=0;; i++) {
		case devgen(i, tab, dir){
		-1 =>
			break loop;
		0 =>
			break;
		1 =>
			if(eqqid(c.qid,dir.qid)) {
				if (getuser() == theuser)
					mode = dir.mode;
				else
					mode = dir.mode << 6;
				t = access[omode&3];
				if((t & mode) == t)
					break loop;
				sys->raise("fail: "+Eperm);
				return nil;
			}
			break;
		}
	}
	if ((c.qid.path&CHDIR) && omode!=Sys->OREAD)
		sys->raise("fail: "+Eperm);
	c.open = 1;
	c.offset = 0;
	return c;
}
	 
devcreate(nil: ref Chan, nil: string, nil, nil: int)
{
	sys->raise("fail: "+Eperm);
}


devremove(nil: ref Chan)
{
	sys->raise("fail: "+Eperm);
}


devwstat(nil: ref Chan, nil: array of byte)
{
	sys->raise("fail: "+Eperm);
}
