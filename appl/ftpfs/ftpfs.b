implement StyxServer;

include "sys.m";
include "draw.m";
include "bufio.m";
include "daytime.m";
include "string.m";
include "styx.m";

sys:		Sys;
time:		Daytime;
str:		String;
bufio:		Bufio;
Iobuf:		import bufio;
Tm:		import time;

FD:		import Sys;
FileIO:		import Sys;
Connection:	import Sys;
Rread:		import Sys;
Rwrite:		import Sys;
Dir:		import Sys;

#
#	File system node.  Refers to parent and file structure.
#	Siblings are linked.  The head is parent.children.
#

Node : adt
{
	dir:		Dir;
	uniq:		int;
	parent:		cyclic ref Node;
	sibs:		cyclic ref Node;
	children:	cyclic ref Node;
	file:		cyclic ref File;
	depth:		int;
	longname:	string;
	cached:		int;
	valid:		int;

	extendpath:	fn(parent: self ref Node, elem: string) : ref Node;
	fixsymbolic:	fn(n: self ref Node);
	invalidate:	fn(n: self ref Node);
	markcached:	fn(n: self ref Node);
	uncache:	fn(n: self ref Node);
	uncachedir:	fn(parent: self ref Node, child: ref Node);

	convD2M:	fn(n: self ref Node, x: int);
	qid:		fn(n: self ref Node) : big;

	fileget:	fn(n: self ref Node) : ref File;
	filefree:	fn(n: self ref Node);
	fileclean:	fn(n: self ref Node);
	fileisdirty:	fn(n: self ref Node) : int;
	filedirty:	fn(n: self ref Node);
	fileread:	fn(n: self ref Node, b: array of byte, off, c: int) : int;
	filewrite:	fn(n: self ref Node, b: array of byte, off, c: int) : int;

	action:		fn(n: self ref Node, cmd: string) : int;
	createdir:	fn(n: self ref Node) : int;
	createfile:	fn(n: self ref Node) : int;
	changedir:	fn(n: self ref Node) : int;
	docreate:	fn(n: self ref Node) : int;
	mkunique:	fn(parent: self ref Node, off: int) : int;
	pathname:	fn(n: self ref Node) : string;
	readdir:	fn(n: self ref Node) : int;
	readfile:	fn(n: self ref Node) : int;
	removedir:	fn(n: self ref Node) : int;
	removefile:	fn(n: self ref Node) : int;
};

#
#	Styx protocol file identifier.
#

Fid : adt
{
	fid:	int;
	node:	ref Node;
	busy:	int;
};

#
#	Foreign file with cache.
#

File : adt
{
	cache:		array of byte;
	length:		int;
	offset:		int;
	fd:		ref FD;
	inuse, dirty:	int;
	atime:		int;
	node:		cyclic ref Node;
	tempname:	string;

	createtmp:	fn(f: self ref File) : ref FD;
};

ftp:		Connection;
dfid:		ref FD;
buff:		array of byte;
tbuff:		array of byte;
rbuff:		array of byte;

fids:		list of ref Fid;

BSZ:		con 8192;
Chunk:		con 1024;
Nfiles:		con 128;

CHSYML:		con 16r40000000;

user:		string = "ftp";
password:	string = "anon@nowhere";
hostname:	string = "kremvax";
mountpoint:	string = "/ftp";
anon:		string = "anon";

firewall:	string = "tcp!$PROXY!402";
myname:		string = "anon";
myhost:		string = "lucent.com";
proxyid:	string;
proxyhost:	string;

errstr:		string;

Enosuchfile:	con "file does not exist";
Eftpproto:	con "ftp protocol error";
Eshutdown:	con "remote shutdown";
Eioerror:	con "io error";
Enotadirectory:	con "not a directory";
Eisadirectory:	con "is a directory";
Epermission:	con "permission denied";
Ebadoffset:	con "bad offset";
Ebadlength:	con "bad length";
Enowstat:	con "wstat not implemented";
Emesgmismatch:	con "message size mismatch";

remdir:		ref Node;
remroot:	ref Node;
remrootpath:	string;

#
#	FTP protocol codes are 3 digits >= 100.
#	The code type is obtained by dividing by 100.
#

Syserr:		con -2;
Syntax:		con -1;
Shutdown:	con 0;
Extra:		con 1;
Success:	con 2;
Incomplete:	con 3;
TempFail:	con 4;
PermFail:	con 5;
Impossible:	con 6;
Err:		con 7;

debug:		con 0;

proxy:		int = 0;

mcon, scon:	ref FileIO;
mountfd:	ref FD;

Wm: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

#
#	Set up FDs for service.
#

connect() : string
{
	s := "ftp." + string time->now();
	n0 := s + ".0";
	n1 := s + ".1";
	if (sys->bind("#s", "/chan", Sys->MBEFORE) < 0)
		return sys->sprint("bind #s failed: %r");
	mcon = sys->file2chan("/chan", n0);
	scon = sys->file2chan("/chan", n1);
	if (mcon == nil || scon == nil)
		return sys->sprint("file2chan failed: %r");
	n0 = "/chan/" + n0;
	n1 = "/chan/" + n1;
	f0 := sys->open(n0, sys->ORDWR);
	f1 := sys->open(n1, sys->ORDWR);
	if (f0 == nil || f1 == nil)
		return sys->sprint("chan open failed: %r");
	mountfd = f0;
	reply->init(f1);
	return nil;
}

#
#	Serve mount point.  The sequence is write from mount, read
#	from server, write from server, read from mount.
#

server()
{
	rc:	Rread;
	wc:	Rwrite;
	b:	array of byte;
	c:	int;

	for (;;) {
		(nil, b, nil, wc) = <- mcon.write;
		if (wc == nil) {
			shut("mount");
			return;
		}
		(nil, c, nil, rc) = <- scon.read;
		if (rc == nil) {
			shut("server");
			return;
		}
		if (c < len b)
			break;
		wc <- = (len b, nil);
		rc <- = (b, nil);
		(nil, b, nil, wc) = <- scon.write;
		if (wc == nil) {
			shut("server");
			return;
		}
		(nil, c, nil, rc) = <- mcon.read;
		if (rc == nil) {
			shut("mount");
			return;
		}
		if (c < len b)
			break;
		wc <- = (len b, nil);
		rc <- = (b, nil);
	}
	rc <- = (nil, Emesgmismatch);
	wc <- = (0, Emesgmismatch);
}

shut(s: string)
{
	sys->print("ftpfs: %s shutdown\n", s);
}

#
#	Mount server.  Must be spawned because it does
#	an attach transaction.
#

mount()
{
	if (sys->mount(mountfd, mountpoint, sys->MAFTER | sys->MCREATE, nil) < 0) {
		sys->print("mount %s failed: %r\n", mountpoint);
		shutdown();
	}
	if (ctxt != nil) {
		dir := load Wm "/dis/wm/dir.dis";
		if (dir == nil)
			sys->print("load /dis/wm/dir.dis failed: %r\n");
		else {
			m := mountpoint;
			if (m[len m - 1] != '/')
				m = m + "/";
			dir->init(ctxt, "dir" :: geometry :: m :: hostname :: nil);
		}
	}
}

#
#	Keep the link alive.
#

beatquanta:	con 10;
beatlimit:	con 10;
beatcount:	int;
activity:	int;
transfer:	int;

heartbeat()
{
	for (;;) {
		sys->sleep(beatquanta * 1000);
		if (activity || transfer) {
			beatcount = 0;
			activity = 0;
			continue;
		}
		beatcount++;
		if (beatcount == beatlimit) {
			acquire();
			sendrequest("NOOP", 0);
			getreply(0);
			release();
			beatcount = 0;
			activity = 0;
		}
	}
}

#
#	Control lock.
#

grant:	chan of int;
notify:	chan of int;

controlmanager()
{
	for (;;) {
		<- grant;
		<- notify;
	}
}

acquire()
{
	grant <- = 0;
}

release()
{
	notify <- = 0;
}

#
#	Data formatting routines.
#

bytecp(d: array of byte, dx: int, s: array of byte, sx: int, n: int)
{
	for (i := 0; i < n; i++)
		d[dx++] = s[sx++];
}

namecp(dx: int, s: string, n: int)
{
	b := array of byte s;
	l := len b;
	if (l > Sys->NAMELEN)
		l = Sys->NAMELEN;
	for (i := 0; i < l; i++)
		tbuff[dx++] = b[i];
	while (i < n) {
		tbuff[dx++] = byte 0;
		i++;
	}
}

nameof(b: array of byte) : string
{
	s := string b;
	l := len s;
	for (i := 0; i < l; i++)
		if (s[i] == 0)
			return s[0:i];
	return s;
}

put4(x: int, v: int)
{
	tbuff[x] = byte v;
	tbuff[x + 1] = byte (v >> 8);
	tbuff[x + 2] = byte (v >> 16);
	tbuff[x + 3] = byte (v >> 24);
}

rerror(tag: int, s: string)
{
	if (debug)
		sys->print("error: %s\n", s);
	reply->errorR(tag, array of byte s);
}

seterr(e: int, s: string) : int
{
	case e {
	Syserr =>
		errstr = Eioerror;
	Syntax =>
		errstr = Eftpproto;
	Shutdown =>
		errstr = Eshutdown;
	* =>
		errstr = s;
	}
	return -1;
}

#
#	Node routines.
#

anode:	Node;
npath:	int	= 1;

newnode(parent: ref Node, name: string) : ref Node
{
	n := ref anode;
	n.dir.name = name;
	n.dir.atime = time->now();
	n.children = nil;
	n.longname = name;
	if (parent != nil) {
		n.parent = parent;
		n.sibs = parent.children;
		parent.children = n;
		n.depth = parent.depth + 1;
		n.valid = 0;
	} else {
		n.parent = n;
		n.sibs = nil;
		n.depth = 0;
		n.valid = 1;
		n.dir.uid = anon;
		n.dir.gid = anon;
		n.dir.mtime = n.dir.atime;
	}
	n.file = nil;
	n.uniq = npath++;
	n.cached = 0;
	return n;
}

Node.extendpath(parent: self ref Node, elem: string) : ref Node
{
	n: ref Node;

	for (n = parent.children; n != nil; n = n.sibs)
		if (n.dir.name == elem)
			return n;
	return newnode(parent, elem);
}

Node.markcached(n: self ref Node)
{
	n.cached = 1;
	n.dir.atime = time->now();
}

Node.uncache(n: self ref Node)
{
	if (n.fileisdirty())
		n.createfile();
	n.filefree();
	n.cached = 0;
}

Node.uncachedir(parent: self ref Node, child: ref Node)
{
	sp: ref Node;

	if (parent == nil || parent == child)
		return;
	for (sp = parent.children; sp != nil; sp = sp.sibs)
		if (sp != child && sp.file != nil && !sp.file.dirty && sp.file.fd != nil) {
			sp.filefree();
			sp.cached = 0;
		}
}

Node.invalidate(node: self ref Node)
{
	n: ref Node;

	node.uncachedir(nil);
	for (n = node.children; n != nil; n = n.sibs) {
		n.cached = 0;
		n.invalidate();
		n.valid = 0;
	}
}

Node.fixsymbolic(n: self ref Node)
{
	if (n.changedir() == 0) {
		n.dir.mode |= Sys->CHDIR; 
		n.uniq |= Sys->CHDIR;
	}
	n.dir.mode &= ~CHSYML; 
}

Node.convD2M(n: self ref Node, x: int)
{
	namecp(x, n.dir.name, Sys->NAMELEN);
	x += Sys->NAMELEN;
	namecp(x, n.dir.uid, Sys->NAMELEN);
	x += Sys->NAMELEN;
	namecp(x, n.dir.gid, Sys->NAMELEN);
	x += Sys->NAMELEN;
	put4(x, n.uniq);
	x += 4;
	put4(x, 0);
	x += 4;
	put4(x, n.dir.mode);
	x += 4;
	put4(x, n.dir.atime);
	x += 4;
	put4(x, n.dir.mtime);
	x += 4;
	put4(x, n.dir.length);
	x += 4;
	put4(x, 0);
	x += 4;
	put4(x, 'f');
}

Node.qid(n: self ref Node) : big
{
	return big n.uniq;
}

#
#	File routines.
#

ntmp:	int;
files:	list of ref File;
nfiles:	int;
afile:	File;
atime:	int;

#
#	Allocate a file structure for a node.  If too many
#	are already allocated discard the oldest.
#

Node.fileget(n: self ref Node) : ref File
{
	f, o: ref File;
	l: list of ref File;

	if (n.file != nil)
		return n.file;
	o = nil;
	for (l = files; l != nil; l = tl l) {
		f = hd l;
		if (f.inuse == 0)
			break;
		if (!f.dirty && (o == nil || o.atime > f.atime))
			o = f;
	}
	if (l == nil) {
		if (nfiles == Nfiles && o != nil) {
			o.node.uncache();
			f = o;
		}
		else {
			f = ref afile;
			files = f :: files;
			nfiles++;
		}
	}
	n.file = f;
	f.node = n;
	f.atime = atime++;
	f.inuse = 1;
	f.dirty = 0;
	f.length = 0;
	f.fd = nil;
	return f;
}

#
#	Create a temporary file for a local copy of a file.
#	If too many are open uncache parent.
#

File.createtmp(f: self ref File) : ref FD
{
	t := "/tmp/ftp." + string time->now() + "." + string ntmp;
	if (ntmp >= 16)
		f.node.parent.uncachedir(f.node);
	f.fd = sys->create(t, Sys->ORDWR | Sys->ORCLOSE, 8r600);
	f.tempname = t;
	f.offset = 0;
	ntmp++;
	return f.fd;
}

#
#	Read 'c' bytes at offset 'off' from a file into buffer 'b'.
#

Node.fileread(n: self ref Node, b: array of byte, off, c: int) : int
{
	f: ref File;
	t, i: int;

	f = n.file;
	if (off + c > f.length)
		c = f.length - off;
	for (t = 0; t < c; t += i) {
		if (off >= f.length)
			return t;
		if (off < Chunk) {
			i = c;
			if (off + i > Chunk)
				i = Chunk - off;
			bytecp(b, t, f.cache, off, i);
		}
		else {
			if (f.offset != off) {
				if (sys->seek(f.fd, off, Sys->SEEKSTART) < 0) {
					f.offset = -1;
					return seterr(Err, sys->sprint("seek temp failed: %r"));
				}
			}
			if (t == 0)
				i = sys->read(f.fd, b, c - t);
			else
				i = sys->read(f.fd, rbuff, c - t);
			if (i < 0) {
				f.offset = -1;
				return seterr(Err, sys->sprint("read temp failed: %r"));
			}
			if (i == 0)
				break;
			if (t > 0)
				bytecp(b, t, rbuff, 0, i);
			f.offset = off + i;
		}
		off += i;
	}
	return t;
}

#
#	Write 'c' bytes at offset 'off' to a file from buffer 'b'.
#

Node.filewrite(n: self ref Node, b: array of byte, off, c: int) : int
{
	f: ref File;
	t, i: int;

	f = n.fileget();
	if (f.cache == nil)
		f.cache = array[Chunk] of byte;
	for (t = 0; t < c; t += i) {
		if (off < Chunk) {
			i = c;
			if (off + i > Chunk)
				i = Chunk - off;
			bytecp(f.cache, off, b, t, i);
		}
		else {
			if (f.fd == nil) {
				if (f.createtmp() == nil)
					return seterr(Err, sys->sprint("temp file: %r"));
				if (sys->write(f.fd, f.cache, Chunk) != Chunk) {
					f.offset = -1;
					return seterr(Err, sys->sprint("write temp failed: %r"));
				}
				f.offset = Chunk;
				f.length = Chunk;
			}
			if (f.offset != off) {
				if (off > f.length) {
					# extend the file with zeroes
					# sparse files may not be supported
				}
				if (sys->seek(f.fd, off, Sys->SEEKSTART) < 0) {
					f.offset = -1;
					return seterr(Err, sys->sprint("seek temp failed: %r"));
				}
			}
			i = sys->write(f.fd, b[t:len b], c - t);
			if (i != c - t) {
				f.offset = -1;
				return seterr(Err, sys->sprint("write temp failed: %r"));
			}
		}
		off += i;
		f.offset = off;
	}
	if (off > f.length)
		f.length = off;
	return t;
}

Node.filefree(n: self ref Node)
{
	f: ref File;

	f = n.file;
	if (f == nil)
		return;
	if (f.fd != nil) {
		ntmp--;
		f.fd = nil;
		f.tempname = nil;
	}
	f.cache = nil;
	f.length = 0;
	f.inuse = 0;
	f.dirty = 0;
	n.file = nil;
}

Node.fileclean(n: self ref Node)
{
	if (n.file != nil)
		n.file.dirty = 0;
}

Node.fileisdirty(n: self ref Node) : int
{
	return n.file != nil && n.file.dirty;
}

Node.filedirty(n: self ref Node)
{
	f: ref File;

	f = n.fileget();
	f.dirty = 1;
}

#
#	Fid management.
#

afid:	Fid;

getfid(fid: int) : ref Fid
{
	l: list of ref Fid;
	f, ff: ref Fid;

	ff = nil;
	for (l = fids; l != nil; l = tl l) {
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
		ff = ref afid;
		fids = ff :: fids;
	}
	ff.node = nil;
	ff.fid = fid;
	return ff;
}

#
#	FTP protocol.
#

fail(s: int, l: string) : string
{
	case s {
	Syserr =>
		return sys->sprint("read fail: %r");
	Syntax =>
		return Eftpproto;
	Shutdown =>
		return Eshutdown;
	* =>
		return "unexpected response: " + l;
	}
}

getreply(echo: int) : (int, string)
{
	n := sys->read(dfid, buff, BSZ);
	if (n < 0)
		return (Syserr, nil);
	if (n == 0)
		return (Shutdown, nil);
	if (n < 5)
		return (Syntax, nil);
	if (buff[n - 1] == byte '\n') {
		n--;
		if (buff[n - 1] == byte '\r')
			n--;
	}
	s := string buff[0:n];
	if (debug || echo)
		sys->print("%s\n", s);
	c := int string buff[0:3];
	if (c < 100)
		return (Syntax, nil);
	return (c / 100, s);
}

sendrequest(req: string, echo: int) : int
{
	activity = 1;
	if (debug || echo)
		sys->print("%s\n", req);
	b := array of byte (req + "\r\n");
	n := sys->write(dfid, b, len b);
	if (n < 0)
		return Syserr;
	if (n != len b)
		return Shutdown;
	return Success;
}

sendfail(s: int) : string
{
	case s {
	Syserr =>
		return sys->sprint("write fail: %r");
	Shutdown =>
		return Eshutdown;
	* =>
		return "internal error";
	}
}

dataport(l: list of string) : string
{
	s := "tcp!" + hd l;
	l = tl l;
	s = s + "." + hd l;
	l = tl l;
	s = s + "." + hd l;
	l = tl l;
	s = s + "." + hd l;
	l = tl l;
	return s + "!" + string ((int hd l * 256) + (int hd tl l));
}

commas(l: list of string) : string
{
	s := hd l;
	l = tl l;
	while (l != nil) {
		s = s + "," + hd l;
		l = tl l;
	}
	return s;
}

third(cmd: string) : ref FD
{
	acquire();
	for (;;) {
		(n, data) := sys->dial(firewall, nil);
		if (n < 0) {
			if (debug)
				sys->print("dial %s failed: %r\n", firewall);
			break;
		}
		t := sys->sprint("\n%s!*\n\n%s\n%s\n1\n-1\n-1\n", proxyhost, myhost, myname);
		b := array of byte t;
		n = sys->write(data.dfd, b, len b);
		if (n < 0) {
			if (debug)
				sys->print("firewall write failed: %r\n");
			break;
		}
		b = array[256] of byte;
		n = sys->read(data.dfd, b, len b);
		if (n < 0) {
			if (debug)
				sys->print("firewall read failed: %r\n");
			break;
		}
		(c, k) := sys->tokenize(string b[:n], "\n");
		if (c < 2) {
			if (debug)
				sys->print("bad response from firewall\n");
			break;
		}
		if (hd k != "0") {
			if (debug)
				sys->print("firewall connect: %s\n", hd tl k);
			break;
		}
		p := hd tl k;
		if (debug)
			sys->print("portid %s\n", p);
		(c, k) = sys->tokenize(p, "!");
		if (c < 3) {
			if (debug)
				sys->print("bad portid from firewall\n");
			break;
		}
		n = int hd tl tl k;
		(c, k) = sys->tokenize(hd tl k, ".");
		if (c != 4) {
			if (debug)
				sys->print("bad portid ip address\n");
			break;
		}
		t = sys->sprint("PORT %s,%d,%d", commas(k), n / 256, n & 255);
		sendrequest(t, 0);
		(r, m) := getreply(0);
		if (r != Success)
			break;
		sendrequest(cmd, 0);
		(r, m) = getreply(0);
		if (r != Extra)
			break;
		n = sys->read(data.dfd, b, len b);
		if (n < 0) {
			if (debug)
				sys->print("firewall read failed: %r\n");
			break;
		}
		b = array of byte "0\n?\n";
		n = sys->write(data.dfd, b, len b);
		if (n < 0) {
			if (debug)
				sys->print("firewall write failed: %r\n");
			break;
		}
		release();
		return data.dfd;
	}
	release();
	return nil;
}

passive(cmd: string) : ref FD
{
	acquire();
	sendrequest("PASV", 0);
	(r, m) := getreply(0);
	release();
	if (r != Success)
		return nil;
	(nil, p) := str->splitl(m, "(");
	if (p == nil)
		str->splitl(m, "0-9");
	else
		p = p[1:len p];
	(c, l) := sys->tokenize(p, ",");
	if (c < 6) {
		sys->print("data: %s\n", m);
		return nil;
	}
	a := dataport(l);
	if (debug)
		sys->print("data dial %s\n", a);
	(s, d) := sys->dial(a, nil);
	if (s < 0)
		return nil;
	acquire();
	sendrequest(cmd, 0);
	(r, m) = getreply(0);
	release();
	if (r != Extra)
		return nil;
	return d.dfd;
}

data(cmd: string) : ref FD
{
	if (proxy)
		return third(cmd);
	else
		return passive(cmd);
}

#
#	File list cracking routines.
#

shorten(name: string, off: int) : string
{
	l := len name;
	if (l < Sys->NAMELEN)
		return name;
	return name[0:Sys->NAMELEN - off - 1] + "*" + name[l - off:l];
}

Node.mkunique(parent: self ref Node, off: int) : int
{
	n, p: ref Node;

	change := 0;
	for (n = parent.children; n != nil; n = n.sibs) {
		for (p = n.sibs; p != nil; p = p.sibs) {
			if (n.dir.name != p.dir.name)
				continue;
			p.dir.name = shorten(p.longname, off);
			change = 1;
		}
	}
	return change;
}

fields(l: list of string, n: int) : array of string
{
	a := array[n] of string;
	for (i := 0; i < n; i++) {
		a[i] = hd l;
		l = tl l;
	}
	return a;
}

now:	ref Tm;
months:	con "janfebmaraprmayjunjulaugsepoctnovdec";

cracktime(month, day, year, hms: string) : int
{
	tm: Tm;

	if (now == nil)
		now = time->local(time->now());
	tm = *now;
	if (month[0] >= '0' && month[0] <= '9') {
		tm.mon = int month - 1;
		if (tm.mon < 0 || tm.mon > 11)
			tm.mon = 5;
	}
	else if (len month >= 3) {
		month = str->tolower(month[0:3]);
		for (i := 0; i < 36; i += 3)
			if (month == months[i:i+3]) {
				tm.mon = i / 3;
				break;
			}
	}
	tm.mday = int day;
	if (hms != nil) {
		(h, z) := str->splitl(hms, "apAP");
		(a, b) := str->splitl(h, ":");
		tm.hour = int a;
		if (b != nil) {
			(c, d) := str->splitl(b[1:len b], ":");
			tm.min = int c;
			if (d != nil)
				tm.sec = int d[1:len d];
		}
		if (z != nil && str->tolower(z)[0] == 'p')
			tm.hour += 12;
	}
	if (year != nil) {
		tm.year = int year;
		if (tm.year >= 1900)
			tm.year -= 1900;
	}
	else {
		if (tm.mon > now.mon || (tm.mon == now.mon && tm.mday > now.mday+1))
			tm.year--;
	}
	return time->tm2epoch(ref tm);
}

crackmode(p: string) : int
{
	flags := 0;
	case len p {
	10 =>	# unix and new style plan 9
		case p[0] {
		'l' =>
			return CHSYML | 0777;
		'd' =>
			flags = Sys->CHDIR;
		}
		p = p[1:10];
	11 =>	# old style plan 9
		if (p[0] == 'l')
			flags = Sys->CHDIR;
		p = p[2:11];
	* =>
		return Sys->CHDIR | 0777;
	}
	mode := 0;
	n := 0;
	for (i := 0; i < 3; i++) {
		mode <<= 3;
		if (p[n] == 'r')
			mode |= 4;
		if (p[n+1] == 'w')
			mode |= 2;
		case p[n+2] {
		'x' or 's' or 'S' =>
			mode |= 1;
		}
		n += 3;
	}
	return mode | flags;
}

crackdir(p: string) : (string, Dir)
{
	d: Dir;
	ln, a: string;

	(n, l) := sys->tokenize(p, " \t\r\n");
	f := fields(l, n);
	if (n > 2 && f[n - 2] == "->")
		n -= 2;
	case n {
	8 =>	# ls -l
		ln = f[7];
		d.uid = f[2];
		d.gid = f[2];
		d.mode = crackmode(f[0]);
		d.length = int f[3];
		(a, nil) = str->splitl(f[6], ":");
		if (len a != len f[6])
			d.atime = cracktime(f[4], f[5], nil, f[6]);
		else
			d.atime = cracktime(f[4], f[5], f[6], nil);
	9 =>	# ls -lg
		ln = f[8];
		d.uid = f[2];
		d.gid = f[3];
		d.mode = crackmode(f[0]);
		d.length = int f[4];
		(a, nil) = str->splitl(f[7], ":");
		if (len a != len f[7])
			d.atime = cracktime(f[5], f[6], nil, f[7]);
		else
			d.atime = cracktime(f[5], f[6], f[7], nil);
	10 =>	# plan 9
		ln = f[9];
		d.uid = f[3];
		d.gid = f[4];
		d.mode = crackmode(f[0]);
		d.length = int f[5];
		(a, nil) = str->splitl(f[8], ":");
		if (len a != len f[8])
			d.atime = cracktime(f[6], f[7], nil, f[8]);
		else
			d.atime = cracktime(f[6], f[7], f[8], nil);
	4 =>	# NT
		ln = f[3];
		d.uid = anon;
		d.gid = anon;
		if (f[2] == "<DIR>") {
			d.length = 0;
			d.mode = Sys->CHDIR | 8r777;
		}
		else {
			d.mode = 8r666;
			d.length = int f[2];
		}
		(n, l) = sys->tokenize(f[0], "/-");
		if (n == 3)
			d.atime = cracktime(hd l, hd tl l, f[2], f[1]);
	1 =>	# ls
		ln = f[0];
		d.uid = anon;
		d.gid = anon;
		d.mode = 0777;
		d.atime = 0;
	* =>
		return (nil, d);
	}
	if (ln == "." || ln == "..")
		return (nil, d);
	d.mtime = d.atime;
	d.name = shorten(ln, 0);
	return (ln, d);
}

longls	: int = 1;

Node.readdir(n: self ref Node) : int
{
	f: ref FD;
	p: ref Node;

	if (n.changedir() < 0)
		return -1;
	for (;;) {
		if (longls) {
			f = data("LIST -l");
			if (f == nil) {
				longls = 0;
				continue;
			}
		}
		else {
			f = data("LIST");
			if (f == nil)
				return seterr(Err, Enosuchfile);
		}
		break;
	}
	b := bufio->fopen(f, sys->OREAD);
	if (b == nil)
		return seterr(Err, Eioerror);
	while ((s := b.gets('\n')) != nil) {
		if (debug)
			sys->print("%s", s);
		(l, d) := crackdir(s);
		if (l == nil)
			continue;
		p = n.extendpath(l);
		p.dir = d;
		p.uniq |= d.mode & Sys->CHDIR;
		p.valid = 1;
	}
	for (i := 0; i < Sys->NAMELEN-5; i++)
		if (n.mkunique(i) == 0)
			break;
	(r, nil) := getreply(0);
	if (r != Success)
		return seterr(Err, Enosuchfile);
	return 0;
}

Node.readfile(n: self ref Node) : int
{
	c: int;

	if (n.parent.changedir() < 0)
		return -1;
	f := data("RETR " + n.longname);
	if (f == nil)
		return seterr(Err, Enosuchfile);
	transfer = 1;
	off := 0;
	while ((c = sys->read(f, tbuff, BSZ)) > 0) {
		if (n.filewrite(tbuff, off, c) != c) {
			off = -1;
			break;
		}
		off += c;
	}
	transfer = 0;
	if (c < 0)
		return seterr(Err, Eioerror);
	n.filewrite(tbuff, off, 0);
	(s, nil) := getreply(0);
	if (s != Success)
		return seterr(s, Enosuchfile);
	return off;
}

path(a, b: string) : string
{
	if (a == nil)
		return b;
	if (b == nil)
		return a;
	if (a[len a - 1] == '/')
		return a + b;
	else
		return a + "/" + b;
}

Node.pathname(n: self ref Node) : string
{
	s: string;

	while (n != n.parent) {
		s = path(n.longname, s);
		n = n.parent;
	}
	return path(remrootpath, s);
}

Node.changedir(n: self ref Node) : int
{
	t: ref Node;
	d: string;

	t = n;
	if (t == remdir)
		return 0;
	if (n.depth == 0)
		d = remrootpath;
	else
		d = n.pathname();
	remdir.uncachedir(nil);
	acquire();
	sendrequest("CWD " + d, 0);
	(e, nil) := getreply(0);
	release();
	case e {
	Success or Incomplete =>
		remdir = n;
		return 0;
	* =>
		return seterr(e, Enosuchfile);
	}
}

Node.docreate(n: self ref Node) : int
{
	f: ref FD;

	f = data("STOR " + n.longname);
	if (f == nil)
		return -1;
	transfer = 1;
	off := 0;
	for (;;) {
		r := n.fileread(buff, off, BSZ);
		if (r <= 0)
			break;
		if (sys->write(f, buff, r) < 0) {
			off = -1;
			break;
		}
		off += r;
	}
	transfer = 0;
	return off;
}

Node.createfile(n: self ref Node) : int
{
	if (n.parent.changedir() < 0)
		return -1;
	off := n.docreate();
	if (off < 0)
		return -1;
	(r, nil) := getreply(0);
	if (r != Success)
		return -1;
	return off;
}

Node.action(n: self ref Node, cmd: string) : int
{
	if (n.parent.changedir() < 0)
		return -1;
	acquire();
	sendrequest(cmd + " " + n.dir.name, 0);
	(r, nil) := getreply(0);
	release();
	if (r != Success)
		return -1;
	return 0;
}

Node.createdir(n: self ref Node) : int
{
	return n.action("MKD");
}

Node.removefile(n: self ref Node) : int
{
	return n.action("DELE");
}

Node.removedir(n: self ref Node) : int
{
	return n.action("RMD");
}

pwd(s: string) : string
{
	(nil, s) = str->splitl(s, "\"");
	if (s == nil || len s < 2)
		return "/";
	(s, nil) = str->splitl(s[1:len s], "\"");
	return s;
}

#
#	Arguments.  hostname mountpoint [user password]
#

getargs(a: string)
{
	(nil, l) := sys->tokenize(a, "\n");
	if (l == nil)
		return;
	hostname = hd l;
	if (len hostname > 6 && hostname[:6] == "proxy!") {
		hostname = hostname[6:];
		proxy = 1;
	}
	l = tl l;
	if (l == nil)
		return;
	mountpoint = hd l;
	l = tl l;
	if (l == nil)
		return;
	user = hd l;
	l = tl l;
	if (l == nil)
		return;
	password = hd l;
	l = tl l;
	if (l == nil)
		return;
	case hd l {
	"proxy" =>
		proxy = 1;
	}
}

#
#	User info for firewall.
#
getuser()
{
	b := array[128] of byte;
	f := sys->open("/dev/user", Sys->OREAD);
	if (f != nil) {
		n := sys->read(f, b, len b);
		if (n > 0)
			myname = string b[:n];
		else if (n == 0)
			sys->print("warning: empty /dev/user\n");
		else
			sys->print("warning: could not read /dev/user: %r\n");
	} else
		sys->print("warning: could not open /dev/user: %r\n");
	f = sys->open("/dev/sysname", Sys->OREAD);
	if (f != nil) {
		n := sys->read(f, b, len b);
		if (n > 0)
			myhost = string b[:n];
		else if (n == 0)
			sys->print("warning: empty /dev/sysname\n");
		else
			sys->print("warning: could not read /dev/sysname: %r\n");
	} else
		sys->print("warning: could not open /dev/sysname: %r\n");
	if (debug)
		sys->print("proxy %s for %s@%s\n", firewall, myname, myhost);
}

#
#	Entry point.  Load modules and initiate protocol.
#

init(a: string, r: StyxReply) : string
{
	s: int;
	l: string;

	if (sys == nil)
		sys = load Sys Sys->PATH;
	getargs(a);
	time = load Daytime Daytime->PATH;
	if (time == nil)
		return sys->sprint("load %s failed: %r", Daytime->PATH);
	str = load String String->PATH;
	if (str == nil)
		return sys->sprint("load %s failed: %r", String->PATH);
	bufio = load Bufio Bufio->PATH;
	if (bufio == nil)
		return sys->sprint("load %s failed: %r", Bufio->PATH);
	reply = r;
	if (proxy) {
		sys->print("dial firewall service %s\n", firewall);
		(s, ftp) = sys->dial(firewall, nil);
		if (s < 0)
			return sys->sprint("dial %s failed: %r", firewall);
		dfid = ftp.dfd;
		getuser();
		t := sys->sprint("\ntcp!%s!tcp.21\n\n%s\n%s\n0\n-1\n-1\n", hostname, myhost, myname);
		if (debug)
			sys->print("request%s\n", t);
		b := array of byte t;
		s = sys->write(dfid, b, len b);
		if (s < 0)
			return sys->sprint("firewall write failed: %r");
		b = array[256] of byte;
		s = sys->read(dfid, b, len b);
		if (s < 0)
			return sys->sprint("firewall read failed: %r");
		(c, k) := sys->tokenize(string b[:s], "\n");
		if (c < 2)
			return "bad response from firewall";
		if (hd k != "0")
			return sys->sprint("firewall connect: %s", hd tl k);
		proxyid = hd tl k;
		if (debug)
			sys->print("proxyid %s\n", proxyid);
		(c, k) = sys->tokenize(proxyid, "!");
		if (c < 3)
			return "bad proxyid from firewall";
		proxyhost = (hd k) + "!" + (hd tl k);
		if (debug)
			sys->print("proxyhost %s\n", proxyhost);
	} else {
		d := "tcp!" + hostname + "!ftp";
		sys->print("dial %s\n", d);
		(s, ftp) = sys->dial(d, nil);
		if (s < 0)
			return sys->sprint("dial %s failed: %r", d);
		dfid = ftp.dfd;
	}
	buff = array[BSZ] of byte;
	tbuff = array[BSZ] of byte;
	rbuff = array[BSZ] of byte;
	(s, l) = getreply(1);
	if (s != Success)
		return fail(s, l);
	s = sendrequest("USER " + user, 0);
	if (s != Success)
		return sendfail(s);
	(s, l) = getreply(0);
	if (s != Success) {
		if (s != Incomplete)
			return fail(s, l);
		s = sendrequest("PASS " + password, 0);
		if (s != Success)
			return sendfail(s);
		(s, l) = getreply(0);
		if (s != Success)
			return fail(s, l);
	}
	s = sendrequest("TYPE I", 0);
	if (s != Success)
		return sendfail(s);
	(s, l) = getreply(0);
	if (s != Success)
			return fail(s, l);
	s = sendrequest("PWD", 0);
	if (s != Success)
		return sendfail(s);
	(s, l) = getreply(0);
	if (s != Success)
			return fail(s, l);
	remrootpath = pwd(l);
	remroot = newnode(nil, "/");
	remroot.uniq |= Sys->CHDIR;
	remroot.dir.mode = Sys->CHDIR | 8r777;
	remdir = remroot;
	l = connect();
	if (l != nil)
		return l;
	grant = chan of int;
	notify = chan of int;
	spawn server();
	spawn mount();
	spawn controlmanager();
	spawn heartbeat();
	return nil;
}

shutdown()
{
}

#
#	Styx transactions.
#

nopT(tag: int)
{
	reply->nopR(tag);
}

flushT(tag, nil: int)
{
	reply->flushR(tag);
}

cloneT(tag, fid, newfid: int)
{
	f, n: ref Fid;

	f = getfid(fid);
	n = getfid(newfid);
	n.busy = 1;
	n.node = f.node;
	reply->cloneR(tag, fid);
}

walkT(tag, fid: int, name: array of byte)
{
	f: ref Fid;
	n: ref Node;

	f = getfid(fid);
	s := nameof(name);
	if ((f.node.uniq & Sys->CHDIR) == 0) {
		rerror(tag, Enotadirectory);
		return;
	}
	else if (s == "..")
		f.node = f.node.parent;
	else if (s != ".") {
		n = f.node;
		if (s == ".flush.ftpfs") {
			n.invalidate();
			n.readdir();
		}
		n = n.extendpath(s);
		if (n.parent.cached) {
			if (!n.valid) {
				rerror(tag, Enosuchfile);
				return;
			}
			if ((n.dir.mode & CHSYML) != 0)
				n.fixsymbolic();
		} else if (!n.valid) {
			if (n.changedir() == 0)
				n.uniq |= Sys->CHDIR;
			else
				n.uniq &= ~Sys->CHDIR;
		}
		f.node = n;
	}
	reply->walkR(tag, fid, f.node.qid());
}

openT(tag, fid, mode: int)
{
	f: ref Fid;

	f = getfid(fid);
	if ((f.node.uniq & Sys->CHDIR) != 0 && mode != Sys->OREAD) {
		rerror(tag, Epermission);
		return;
	}
	if ((mode & Sys->OTRUNC) != 0) {
		f.node.uncache();
		f.node.parent.uncache();
		f.node.filedirty();
	} else if (!f.node.cached) {
		f.node.filefree();
		if ((f.node.uniq & Sys->CHDIR) != 0) {
			f.node.invalidate();
			if (f.node.readdir() < 0) {
				rerror(tag, Enosuchfile);
				return;
			}
		}
		else {
			if (f.node.readfile() < 0) {
				rerror(tag, errstr);
				return;
			}
		}
		f.node.markcached();
	}
	reply->openR(tag, fid, f.node.qid());
}

createT(tag, fid: int, name: array of byte, perm, mode: int)
{
	f: ref Fid;

	f = getfid(fid);
	if ((f.node.uniq & Sys->CHDIR) == 0) {
		rerror(tag, Enotadirectory);
		return;
	}
	s := nameof(name);
	f.node = f.node.extendpath(s);
	f.node.uncache();
	if ((perm & Sys->CHDIR) != 0) {
		if (f.node.createdir() < 0) {
			rerror(tag, Epermission);
			return;
		}
	}
	else
		f.node.filedirty();
	f.node.parent.invalidate();
	f.node.parent.uncache();
	reply->createR(tag, fid, f.node.qid());
}

readT(tag, fid: int, offset: big, count: int)
{
	f: ref Fid;
	p: ref Node;
	rv: int;

	f = getfid(fid);
	if (count > Sys->ATOMICIO)
		count = Sys->ATOMICIO;
	if ((f.node.uniq & Sys->CHDIR) != 0) {
		rv = 0;
		if (((int offset) % Styx->STATSZ) != 0) {
			rerror(tag, Ebadoffset);
			return;
		}
		if (count < Styx->STATSZ) {
			rerror(tag, Ebadlength);
			return;
		}
		n := int offset / Styx->STATSZ;
		for (p = f.node.children; n > 0 && p != nil; p = p.sibs)
			if (p.valid)
				n--;
		if (n == 0) {
			n = count / Styx->STATSZ;
			for (; n > 0 && p != nil; p = p.sibs) {
				if (p.valid) {
					if ((p.dir.mode & CHSYML) != 0)
						p.fixsymbolic();
					p.convD2M(rv);
					rv += Styx->STATSZ;
					n--;
				}
			}
		}
	}
	else {
		if (!f.node.cached && f.node.readfile() < 0) {
			rerror(tag, errstr);
			return;
		}
		f.node.markcached();
		rv = f.node.fileread(tbuff, int offset, count);
		if (rv < 0) {
			rerror(tag, errstr);
			return;
		}
	}
	reply->readR(tag, fid, rv, tbuff);
}

writeT(tag, fid: int, offset: big, count: int, data: array of byte)
{
	f: ref Fid;

	f = getfid(fid);
	if ((f.node.uniq & Sys->CHDIR) != 0) {
		rerror(tag, Eisadirectory);
		return;
	}
	count = f.node.filewrite(data, int offset, count);
	if (count < 0) {
		rerror(tag, errstr);
		return;
	}
	f.node.filedirty();
	reply->writeR(tag, fid, count);
}

clunkT(tag, fid: int)
{
	f: ref Fid;

	f = getfid(fid);
	if (f.node.fileisdirty()) {
		if (f.node.createfile() < 0)
			sys->print("ftpfs: could not create %s\n", f.node.pathname());
		f.node.fileclean();
		f.node.uncache();
	}
	f.busy = 0;
	reply->clunkR(tag, fid);
}

removeT(tag, fid: int)
{
	f: ref Fid;

	f = getfid(fid);
	if ((f.node.uniq & Sys->CHDIR) != 0) {
		if (f.node.removedir() < 0) {
			rerror(tag, errstr);
			return;
		}
	}
	else {
		if (f.node.removefile() < 0) {
			rerror(tag, errstr);
			return;
		}
	}
	f.node.parent.uncache();
	f.node.uncache();
	f.node.valid = 0;
	f.busy = 0;
	reply->removeR(tag, fid);
}

statT(tag, fid: int)
{
	f: ref Fid;
	n: ref Node;

	f = getfid(fid);
	n = f.node.parent;
	if (!n.cached) {
		n.invalidate();
		n.readdir();
		n.markcached();
	}
	if (!f.node.valid) {
		rerror(tag, Enosuchfile);
		return;
	}
	f.node.convD2M(0);
	reply->statR(tag, fid, tbuff);
}

wstatT(tag, fid: int, stat: array of byte)
{
	rerror(tag, Enowstat);
}

attachT(tag, fid: int, uid: array of byte, aname: array of byte)
{
	f: ref Fid;

	f = getfid(fid);
	f.busy = 1;
	f.node = remroot;
	reply->attachR(tag, fid, remroot.qid());
}
