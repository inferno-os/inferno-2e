implement Transport;

include "common.m";
include "transport.m";

# local copies from CU
sys: Sys;
U: Url;
	ParsedUrl: import U;
CU: CharonUtils;
	Netconn, ByteSource, Header, config : import CU;

dbg := 0;

init(c: CharonUtils)
{
	CU = c;
	sys = load Sys Sys->PATH;
	U = load Url Url->PATH;
	dbg = int (CU->config).dbg['n'];
}

connect(nc: ref Netconn, nil: ref ByteSource)
{
	nc.connected = 1;
	nc.state = CU->NCgethdr;
	return;
}

writereq(nil: ref Netconn, nil: ref ByteSource)
{
	return;
}

gethdr(nc: ref Netconn, bs: ref ByteSource)
{
	u := bs.req.url;
	f := u.pstart + u.path;
	hdr := Header.new();
	nc.conn.dfd = sys->open(f, sys->OREAD);
	if(nc.conn.dfd == nil) {
		if(dbg)
			sys->print("file %d: can't open %s: %r\n", nc.id, f);
		# Could examine %r to distinguish between NotFound
		# and Forbidden and other, but string is OS-dependent.
		hdr.code = CU->HCNotFound;
		bs.hdr = hdr;
		nc.connected = 0;
		return;
	}

	(ok, statbuf) := sys->fstat(nc.conn.dfd);
	if(ok < 0) {
		bs.err = "stat error";
		return;
	}

	if (statbuf.mode & Sys->CHDIR) {
		bs.err = "Directories not implemented";
		return;
	}

	# assuming file (not directory)
	n := statbuf.length;
	hdr.length = n;
	if(n > sys->ATOMICIO)
		n = sys->ATOMICIO;
	a := array[n] of byte;
	n = sys->read(nc.conn.dfd, a, n);
	if(dbg)
		sys->print("file %d: initial read %d bytes\n", nc.id, n);
	if(n < 0) {
		bs.err = "read error";
		return;
	}
	hdr.setmediatype(f, a[0:n]);
	hdr.base = hdr.actual = bs.req.url;
	if(dbg)
		sys->print("file %d: hdr has mediatype=%s, length=%d\n",
			nc.id, CU->mnames[hdr.mtype], hdr.length);
	bs.hdr = hdr;
	if(n == len a)
		nc.tbuf = a;
	else
		nc.tbuf = a[0:n];
}

getdata(nc: ref Netconn, bs: ref ByteSource)
{
	buf := bs.data;
	if(nc.tbuf != nil) {
		# initial data
		if(len buf <= len nc.tbuf) {
			bs.data = nc.tbuf;
			bs.edata = len nc.tbuf;
			nc.tbuf = nil;
			if(dbg)
				sys->print("file %d: eof (%d total bytes)\n", nc.id, bs.edata);
			nc.conn.dfd = nil;
			nc.connected = 0;
			return;
		}
		else {
			buf[0:] = nc.tbuf;
			bs.edata = len nc.tbuf;
			nc.tbuf = nil;
		}
	}
	n := sys->read(nc.conn.dfd, buf[bs.edata:], len buf - bs.edata);
	if(dbg > 1)
		sys->print("file %d: read %d bytes\n", nc.id, n);
	if(n <= 0) {
		nc.conn.dfd = nil;
		nc.connected = 0;
		if(n < 0)
			bs.err = sys->sprint("%r");
	}
	else {
		bs.edata += n;
		if(bs.edata == bs.hdr.length) {
			if(dbg)
				sys->print("file %d: eof (%d total bytes)\n", nc.id, bs.edata);
			nc.conn.dfd = nil;
			nc.connected = 0;
		}
	}
}

defaultport(nil: int) : int
{
	return 0;
}
