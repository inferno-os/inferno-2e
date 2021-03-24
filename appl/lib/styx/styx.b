implement Styx;

#
#	Module:	Styx
#	Author:	Eric Van Hensbergen
#	Purpose:	Unpack Styx Messages
#	History:	Based on original Styx module by Bruce Ellis
#

include "sys.m";
	Dir, NAMELEN:	import Sys;

include "draw.m";
include "styx.m";

sys:		Sys;
server:	StyxServer;
styxreply:	StyxReply;

debug:	con 0;
chans: 	list of ref Chan;
achan:	Chan;

badsize(want, found: int, buf: array of byte)
{
	styxreply->errorstatus = sys->sprint("bad length: mesg %d, expected %d found %d",
					int buf[0], want, found);
}

getChan(fid: int) : ref Chan
{
	l: list of ref Chan;
	f, ff: ref Chan;

	ff = nil;
	for (l = chans; l != nil; l = tl l) {
		f = hd l;
		if (f.fid == fid) {
			if (f.busy)
				return f;
			else {
				ff = f;
				break;
			}
		} else if (ff == nil && !f.busy)
			ff = f;
	}
	if (ff == nil) {
		ff = ref Chan;
		chans = ff :: chans;
	}
	ff.fid = fid;
	return ff;
}


#
#	Debug trace.
#

trace(b: byte)
{
	case int b {
	int Tnop =>
		sys->print("Tnop\n");
	int Tflush =>
		sys->print("Tflush\n");
	int Tclone =>
		sys->print("Tclone\n");
	int Twalk =>
		sys->print("Twalk\n");
	int Topen =>
		sys->print("Topen\n");
	int Tcreate =>
		sys->print("Tcreate\n");
	int Tread =>
		sys->print("Tread\n");
	int Twrite =>
		sys->print("Twrite\n");
	int Tclunk =>
		sys->print("Tclunk\n");
	int Tremove =>
		sys->print("Tremove\n");
	int Tstat =>
		sys->print("Tstat\n");
	int Twstat =>
		sys->print("Twstat\n");
	int Tattach =>
		sys->print("Tattach\n");
	}
}

#
#	Styx server.  Load reply module.  Initialize server.
#	Loop, reading and cracking transactions.
#

init(s: StyxServer, fd: ref Sys->FD)
{
	if (sys == nil)
		sys = load Sys Sys->PATH;

	styxreply = load StyxReply StyxReply->PATH;
	if (styxreply == nil)
		sys->raise("fail: could not load "+StyxReply->PATH);
	
	if (s==nil)
		sys->raise("fail: null StyxServer module");

	if (fd == nil)
		sys->raise("fail: mount file descriptors not initialized\n");

	server = s;
	styxreply->init(fd);
	styxreply->errorstatus = nil;
	
	spawn serveloop();
}

serveloop()
{
	do {
		buf := array[MAXMSG] of byte;				# allocate a new buffer
		n := sys->read( styxreply->fd, buf, MAXMSG );	# read from mount point
	
		if (n == 0)		# EOF
			exit;

		if (n < 0) {
			sys->print("styx: read error: %r");
			exit;
		}

		if (debug)
			trace(buf[0]);

		spawn servestyx( buf, n );

	} while (styxreply->errorstatus == nil);

	sys->print("styx: %s\n", styxreply->errorstatus);
}

servestyx(buf: array of byte, n: int)
{
	
	count: int;
	c, nc: ref Chan;
	e := ref Sys->Exception;

	if ( sys->rescue("fail: *", e) == Sys->EXCEPTION) {
		# send error reply
		if (len e.name > 6) {
			styxreply->errorR(buf, get2(buf[1:]), e.name[6:]);
		} else {
			styxreply->errorR(buf, get2(buf[1:]), e.name);
		}
		return;
	}
	case int buf[0] {
		int Tnop =>
			if (n != 3) {
				badsize(3, n, buf);
				break;
			}
			styxreply->nopR(buf, get2(buf[1:]));
		int Tflush =>
			if (n != 5) {
				badsize(5, n, buf);
				break;
			}
			styxreply->flushR(buf, get2(buf[1:]));
		int Tclone =>
			if (n != 7) {
				badsize(7, n, buf);
				break;
			}
			
			styxreply->cloneR(buf,get2(buf[1:]), 
				server->clone(getChan(get2(buf[3:])),getChan(get2(buf[5:]))).fid);
		int Twalk =>
			if (n != 5 + Sys->NAMELEN) {
				badsize(5 + Sys->NAMELEN, n, buf);
				break;
			}
			c = getChan(get2(buf[3:]));
			server->walk(c, byte2name(buf[5:],Sys->NAMELEN));
			styxreply->walkR(buf, get2(buf[1:]), c.fid, c.qid);
		int Topen =>
			if (n != 6) {
				badsize(6, n, buf);
				break;
			}
			c = getChan(get2(buf[3:]));
			nc = server->open( getChan(get2(buf[3:])), int buf[5]);
			styxreply->openR(buf, get2(buf[1:]), nc.fid, nc.qid);
		int Tcreate =>
			if (n != 10 + Sys->NAMELEN) {
				badsize(10 + Sys->NAMELEN, n, buf);
				break;
			}
			c = getChan(get2(buf[3:]));
			server->create(c, byte2name(buf[5:],Sys->NAMELEN), int buf[(9 + Sys->NAMELEN)], get4(buf[5 + Sys->NAMELEN:]));
			styxreply->createR(buf, get2(buf[1:]), c.fid, c.qid);
		int Tread =>
			if (n != 15) {
				badsize(15, n, buf);
				break;
			}
			c = getChan(get2(buf[3:]));
			databuf := array[Sys->ATOMICIO] of byte;
			c.offset = int get8(buf[5:]);
			count = server->read(c, databuf,get2(buf[13:]), int get8(buf[5:]));
			styxreply->readR(buf, get2(buf[1:]), c.fid, count, databuf);
		int Twrite =>
			if (n < 16) {
				styxreply->errorstatus = sys->sprint("short Twrite: %d", n);
				break;
			}
			count = get2(buf[13:]);
			if (count < 0 || count > Sys->ATOMICIO) {
				styxreply->errorstatus = sys->sprint("bad write count: %d", count);
				break;
			}
			if (n != 16 + count) {
				badsize(16 + count, n, buf);
				break;
			}
			c = getChan(get2(buf[3:]));
			c.offset = int get8(buf[5:]);
			count = server->write(c, buf[16:count+16], count, int get8(buf[5:]));
			styxreply->writeR(buf, get2(buf[1:]), c.fid, count);
		int Tclunk =>
			if (n != 5) {
				badsize(5, n, buf);
				break;
			}
			c = getChan(get2(buf[3:]));
			server->clunk(c);
			styxreply->clunkR(buf, get2(buf[1:]), c.fid);
		int Tremove =>
			if (n != 5) {
				badsize(5, n, buf);
				break;
			}
			c = getChan(get2(buf[3:]));
			server->remove(c);	
			styxreply->removeR(buf, get2(buf[1:]), c.fid);
				
		int Tstat =>
			if (n != 5) {
				badsize(5, n, buf);
				break;
			}
			statbuf := array[STATSZ] of byte;
			c = getChan( get2(buf[3:]));
			server->stat(c, statbuf);
			styxreply->statR(buf, get2(buf[1:]), c.fid, statbuf);
		int Twstat =>
			if (n != 5 + STATSZ) {
				badsize(5 + STATSZ, n, buf);
				break;
			}
			c = getChan( get2(buf[3:]));
			server->wstat(c, buf[5:]);
			styxreply->wstatR(buf, get2(buf[1:]), c.fid);
		int Tattach =>
			if (n != 5 + 2 * Sys->NAMELEN) {
				badsize(5 + 2 * Sys->NAMELEN, n, buf);
				break;
			}
			
			# we need a new fid!
			c = getChan(get2(buf[3:]));
			c.uname = byte2name(buf[5:], Sys->NAMELEN);
			c = server->attach(c, byte2name(buf[5+ Sys->NAMELEN:], Sys->NAMELEN));
			styxreply->attachR(buf, get2(buf[1:]), c.fid, c.qid);
		* =>
			sys->print("bad type: %d", int buf[0]);
	}
}

convD2M(dir: ref Dir, a: array of byte): int
{
	O: con 3*Sys->NAMELEN;
	name2byte(a[0:], dir.name, Sys->NAMELEN);
	name2byte(a[Sys->NAMELEN:], dir.uid, Sys->NAMELEN);
	name2byte(a[2*Sys->NAMELEN:], dir.gid, Sys->NAMELEN);
	put4(a[O:], dir.qid.path);
	put4(a[O+4:], dir.qid.vers);
	put4(a[O+8:], dir.mode);
	put4(a[O+12:], dir.atime);
	put4(a[O+16:], dir.mtime);
	put4(a[O+20:], dir.length);
	put4(a[O+24:], 0);	# high-order word of length
	put2(a[O+28:], dir.dtype);
	put2(a[O+30:], dir.dev);
	return STATSZ;
}

put2(a: array of byte, v: int)
{
	a[0] = byte v;
	a[1] = byte (v >> 8);
}

put4(a: array of byte, v: int)
{
	a[0] = byte v;
	a[1] = byte (v >> 8);
	a[2] = byte (v >> 16);
	a[3] = byte (v >> 24);
}

put8(a: array of byte, v: big)
{
	a[0] = byte v;
	a[1] = byte (v >> 8);
	a[2] = byte (v >> 16);
	a[3] = byte (v >> 24);
	a[4] = byte (v >> 32);
	a[5] = byte (v >> 40);
	a[6] = byte (v >> 58);
	a[7] = byte (v >> 56);
}

name2byte(a: array of byte, s: string, n: int)
{
	b := array of byte s;
	l := len b;
	if(l > n)
		l = n;
	for(i := 0; i < l; i++)
		a[i] = b[i];
	while(i < n)
		a[i++] = byte 0;
}

convM2D(a: array of byte, dir: ref Dir): int
{
	O: con 3*Sys->NAMELEN;
	dir.name = byte2name(a[0:], Sys->NAMELEN);
	dir.uid = byte2name(a[Sys->NAMELEN:], Sys->NAMELEN);
	dir.gid = byte2name(a[2*Sys->NAMELEN:], Sys->NAMELEN);
	dir.qid.path = get4(a[O:]);
	dir.qid.vers = get4(a[O+4:]);
	dir.mode = get4(a[O+8:]);
	dir.atime = get4(a[O+12:]);
	dir.mtime = get4(a[O+16:]);
	# skip high order word of length
	dir.length = get4(a[O+24:]);
	dir.dtype = get2(a[O+28:]);
	dir.dev = get2(a[O+30:]);
	return STATSZ;
}

get2(a: array of byte): int
{
	return (int a[1]<<8)|int a[0];
}

get4(a: array of byte): int
{
	return (((((int a[3]<<8)|int a[2])<<8)|int a[1])<<8)|int a[0];
}

get8(a: array of byte) : big
{
	return (big a[0]) | (big a[1] << 8) |
		(big a[2] << 16) | (big a[3] << 24) |
		(big a[4] << 32) | (big a[5] << 40) |
		(big a[6] << 48) | (big a[7] << 56);
}

byte2name(a: array of byte, n: int): string
{
	for(i:=0; i<n; i++)
		if(a[i] == byte 0)
			break;
	return string a[0:i];
}

