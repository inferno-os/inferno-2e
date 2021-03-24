implement Bootpd;

include "sys.m";
	sys: Sys;

include "draw.m";

include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;

Bootpd: module
{
	init: fn(nil: ref Draw->Context, argv: list of string);
};

stderr: ref Sys->FD;
debug: int;
sniff: int;
verbose: int;

siaddr: array of byte;
sysname: string;

NEED_HA: con 1;
NEED_IP: con 0;
NEED_BF: con 0;
NEED_SM: con 0;
NEED_GW: con 0;
NEED_FS: con 0;
NEED_AU: con 0;

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	if(sys == nil)
		return;
	bufio = load Bufio Bufio->PATH;
	if(bufio == nil)
		sys->raise("fail: load bufio");
	stderr = sys->fildes(2);

	fname := "/services/bootp/db";
	verbose = 1;
	sniff = 0;
	debug = 0;
	if(argv != nil) {
		progname := hd argv;
		if(progname == nil) {
			progname = "bootp";
		}
		for(argv = tl argv; argv != nil; argv = tl argv) {
			if((hd argv)[0] == '-') {
				arg := hd argv;
				if(len arg >= 2 && arg[0:2] == "-x") continue;
				for(i := 1; i < len arg; i++) case arg[i] {
				'd' =>
					debug = 1;
				'D' =>
					debug = 2;
				's' =>
					sniff = 1;
					debug = 255;
				'q' =>
					verbose = 0;
				'v' =>
					verbose = 1;
				* =>
					sys->print("Usage: %s [ -dDsqv ] [ <file> ]\n", progname);
					sys->print("\tfile: specify bootptab file; defaults to /services/bootp/db\n");
					sys->print("\t-d: print debugging output\n");
					sys->print("\t-D: print more debugging output\n");
					sys->print("\t-s: 'sniff'; display packets, don't serve requests\n");
					sys->print("\t-q: quiet; only display major errors\n");
					sys->print("\t-v: verbose: display cs-style feedback (default)\n");
					exit;
				}
				continue;
			}
			fname = hd argv;
		}
	}

	if(tabopen(fname))
		sys->raise("fail: open database");
	
	if(sniff) sys->fprint(stderr, "bootp: SNIFF: running in sniff mode.");
	if(debug) sys->fprint(stderr, "bootp: DEBUG: debug level %d.\n", debug);

	if(!sniff && (err := update_records()) != nil) {
		sys->fprint(stderr,  "bootp: update_records failed: %s\n", err);
		sys->fprint(stderr,  "bootp: exiting\n");
		return;
	}

	if(debug) sys->fprint(stderr, "bootp: DEBUG: announcing \"/net/udp!*!67\"\n");
	(ok, c) := sys->announce("/net/udp!*!67");
	if(ok < 0) {
		sys->fprint(stderr, "bootp: announce %d: %r\n", ok);
		return;
	}
	get_ip();
	get_sysname();

	sys->fprint(c.cfd, "headers4"); # put src/port at head of read

	if(debug) sys->fprint(stderr, "bootp: DEBUG: opening \"%s/data\"\n", c.dir);
	c.dfd = sys->open(c.dir+"/data", sys->ORDWR);
	if(c.dfd == nil) {
		sys->fprint(stderr, "bootp: open: %s: %r\n", c.dir);
		return;
	}

	buf := array[2048] of byte;
	badread := 0;
	for(;;) {
		if(debug) sys->fprint(stderr, "bootp: DEBUG: listening for bootp requests...\n");
		n := sys->read(c.dfd, buf, len buf);
		if(n <0) {
			if (badread++ > 10)
				break;
			continue;
		}
		badread = 0;
		if(n < 12) {
			if(debug) sys->fprint(stderr, "bootp: DEBUG: short request of %d bytes\n", n - 12);
			continue;
		}
		if(debug) sys->fprint(stderr, "bootp: DEBUG: received request from %d.%d.%d.%d!%d\n",
			int buf[0], int buf[1], int buf[2], int buf[3], int nhgets(buf[8:10]));
		if(n < 12+300) {
			if(debug) sys->fprint(stderr, "bootp: DEBUG: short request of %d bytes\n", n - 12);
			continue;
		}

		bootp: ref BootpPKT;
		(err, bootp) = M2S(buf[12:]);
		if(err != nil) {
			if(debug) sys->fprint(stderr, "bootp: DEBUG: M2S failed: %s\n", err);
			continue;
		}
		if(debug >= 2) ppkt(bootp);
		if(sniff)
			continue;
		if(bootp.htype != byte 1 || bootp.hlen != byte 6) {
			# if it isn't ether, we don't do it
			if(debug) sys->fprint(stderr, "bootp: DEBUG: hardware type not ether; ignoring.\n");
			continue;
		}
		if((err = update_records()) != nil) {
			sys->fprint(stderr,  "bootp: getreply: update_records failed: %s\n", err);
				continue;
		}
		rec := lookup(bootp);
		if(rec == nil) {
			# we can't answer this request
			if(debug) sys->fprint(stderr, "bootp: DEBUG: cannot answer request.\n");
				continue;
		}
		if(debug) { sys->fprint(stderr, "bootp: DEBUG: found a matching entry:\n"); pinfbp(rec); }
		mkreply(bootp, rec);
		if(verbose) sys->print("bootp: %s -> %s %s\n", dtox(rec.ha), rec.hostname, iptoa(rec.ip));
		if(debug >= 2) { sys->fprint(stderr, "bootp: DEBUG: reply message:\n"); ppkt(bootp); }
		repl:= S2M(bootp);

		if(debug) sys->fprint(stderr, "bootp: DEBUG: sending reply.\n");
		arpenter(iptoa(rec.ip), dtox(rec.ha));
		send(repl);
	}
	sys->fprint(stderr, "bootp: %d read errors: %r\n", badread);
}

arpenter(ip, ha: string)
{
	if(debug) sys->fprint(stderr, "bootp: DEBUG: arp: %s -> %s\n", ip, ha);
	fd := sys->open("/net/arp", Sys->OWRITE);
	if(fd == nil) {
		if(debug) sys->fprint(stderr, "bootp: DEBUG: arp open failed: %r\n");
		return;
	}
	b := array of byte ("add " + ip + " " + ha);
	n := sys->write(fd, b, len b);
	if(n != len b)
		if(debug) sys->fprint(stderr, "bootp: DEBUG: short arp write: %r\n");
}


get_ip()
{
	siaddr = array[4] of { * => byte 0 };
	fname:= "/dev/sysname";
	iob := bufio->open(fname, Sys->OREAD);
	if(iob == nil) {
		if(debug) sys->fprint(stderr, "bootp: DEBUG: cannot open %s for reading: %r.\n", fname);
		return;
	}
	buf:= iob.gets('\n');
	iob.close();
	if(buf== nil) {
		if(debug) sys->fprint(stderr, "bootp: DEBUG: error reading %s: %r.\n", fname);
		return;
	}

	fd := sys->open("/net/cs", Sys->ORDWR);
	if(fd == nil) {
		if(debug) sys->fprint(stderr, "bootp: DEBUG: cannot open /net/cs for reading: %r.\n");
		return;
	}
	b:= array of byte ("net!" + buf + "!0");
	if(sys->write(fd, b, len b) != len b) {
		if(debug) sys->fprint(stderr, "bootp: DEBUG: write %s to /net/cs: %r.\n", string b);
		return;
	}
	a:= array[1024] of byte;
	n:= sys->read(fd, a, len a);
	if(n < 0) {
		if(debug) sys->fprint(stderr, "bootp: DEBUG: read from /net/cs: %r.\n");
		return;
	}
	if(debug) sys->fprint(stderr, "bootp: DEBUG: read %s from /net/cs\n", string a[:n]);

	(l, addr):= sys->tokenize(string a[:n], " ");
	if(l != 2) {
		if(debug) sys->fprint(stderr, "bootp: DEBUG: bad format from cs\n");
		return;
	}
	(l, addr) = sys->tokenize(hd tl addr, "!");
	if(l != 2) {
		if(debug) sys->fprint(stderr, "bootp: DEBUG: short addr from cs\n");
		return;
	}
	err:= "";
	(err, siaddr) = get_ipaddr(hd addr);
	if(err != nil || siaddr == nil) {
		if(debug) sys->fprint(stderr, "bootp: DEBUG: invalid local IP addr %s.\n", hd tl addr);
		siaddr = array[4] of { * => byte 0 };
	};
	if(debug) sys->fprint(stderr, "bootp: DEBUG: local IP address is %s.\n", iptoa(siaddr));
}

get_sysname()
{
	fd := sys->open("#c/sysname", sys->OREAD);
	if(fd == nil) {
		sysname = "anon";
		return;
	}
	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) {
		sysname = "anon";
		return;
	}
	sysname = string buf[0:n];
}

#	byte	op;		/* opcode */
#	byte	htype;		/* hardware type */
#	byte	hlen;		/* hardware address len */
#	byte	hops;		/* hops */
#	byte	xid[4];		/* a random number */
#	byte	secs[2];	/* elapsed snce client started booting */
#	byte	pad[2];
#	byte	ciaddr[4];	/* client IP address (client tells server) */
#	byte	yiaddr[4];	/* client IP address (server tells client) */
#	byte	siaddr[4];	/* server IP address */
#	byte	giaddr[4];	/* gateway IP address */
#	byte	chaddr[16];	/* client hardware address */
#	byte	sname[64];	/* server host name (optional) */
#	byte	file[128];	/* boot file name */
#	byte	vend[128];	/* vendor-specific goo */

BootpPKT: adt
{
	op:	byte;		# Start of udp datagram
	htype:	byte;
	hlen:	byte;
	hops:	byte;
	xid:	int;
	secs:	int;
	ciaddr:	array of byte;
	yiaddr:	array of byte;
	siaddr:	array of byte;
	giaddr:	array of byte;
	chaddr:	array of byte;
	sname:	string;
	file:	string;
	vend:	array of byte;
};

InfBP: adt {
	hostname: string;

	ha: array of byte;	# hardware addr
	ip: array of byte;	# client IP addr
	bf: array of byte;	# boot file path
	sm: array of byte;	# subnet mask
	gw: array of byte;	# gateway IP addr
	fs: array of byte;	# file server IP addr
	au: array of byte;	# authentication server IP addr
};

records: array of ref InfBP;

tabbio: ref Bufio->Iobuf;
tabname: string;
mtime: int;

tabopen(fname: string): int
{
	if(sniff) return 0;
	tabname = fname;
	if((tabbio = bufio->open(tabname, bufio->OREAD)) == nil) {
		sys->fprint(stderr, "bootp: tabopen: Can't open %s: %r\n", tabname);
		return 1;
	}
	return 0;
}

send(msg: array of byte)
{
	if(debug) sys->fprint(stderr, "bootp: DEBUG: dialing udp!broadcast!68\n");
	(n, c) := sys->dial("udp!255.255.255.255!68", "67");
	if(n < 0) {
		sys->fprint(stderr, "bootp: send: error calling dial: %r\n");
		return;
	}
	if(debug) sys->fprint(stderr, "bootp: DEBUG: writing to %s/data\n", c.dir);
	n = sys->write(c.dfd, msg, len msg);
	if(n <=0) {
		sys->fprint(stderr, "bootp: send: error writing to %s/data: %r\n", c.dir);
		return;
	}
	if(debug) sys->fprint(stderr, "bootp: DEBUG: successfully wrote %d bytes to %s/data\n", n, c.dir);
	return;
}

mkreply(bootp: ref BootpPKT, rec: ref InfBP)
{
	bootp.op = byte 2; # boot reply
	bootp.yiaddr = rec.ip;
	bootp.siaddr = siaddr;
	bootp.giaddr = array[4] of { * => byte 0 };
	bootp.sname = sysname;
	bootp.file = string rec.bf;
	bootp.vend = array of byte sys->sprint("p9  %s %s %s %s", iptoa(rec.sm), iptoa(rec.fs), iptoa(rec.au), iptoa(rec.gw));
}

lookup(bootp: ref BootpPKT): ref InfBP
{
	for(i := 0; i < len records; i++) {
		if(arreq(bootp.chaddr[0:6], records[i].ha) || arreq(bootp.ciaddr, records[i].ip)) {
			return records[i];
		}
	}
	return nil;
}

update_records(): string
{
	(n, dir) := sys->fstat(tabbio.fd);
	if(n < 0) {
		return sys->sprint("cannot fstat %s.", tabname);
	}
	if(mtime == 0 || mtime != dir.mtime) {
		if(bufio->tabbio.seek(0, Sys->SEEKSTART) < 0) {
			return sys->sprint("error seeking to start of %s.", tabname);
		}
		mtime = dir.mtime;
		lnum: int = 0;
		trecs: list of ref InfBP;
LINES:	while((line := bufio->tabbio.gets('\n')) != nil) {
			lnum++;
			if(line[0] == '#')	# comment
				continue LINES;
			fields: list of string;
			(n, fields) = sys->tokenize(line, ":\r\n");
			if(n <= 0) {	# blank line or colons
				if(len line > 0) {
					sys->fprint(stderr, "bootp: update_records: %s: %d empty entry.\n", tabname, lnum);
				}
				continue LINES;
			}
			rec := ref InfBP;
			rec.hostname = hd fields;
			fields = tl fields;
			err: string;
FIELDS:		for(; fields != nil; fields = tl fields) {
				field := hd fields;
				if(len field <= len "xx=") {
					sys->fprint(stderr, "bootp: update_records: %s:%d invalid field \"%s\" in entry for %s",
						tabname, lnum, field, rec.hostname);
					continue FIELDS;
				}
				err = nil;
				case field[0:3] {
				"ha=" =>
					if(rec.ha != nil) {
						sys->fprint(stderr,
							"bootp: warning: %s:%d hardware address redefined for %s.\n",
							tabname, lnum, rec.hostname);
					}
					(err, rec.ha) = get_haddr(field[3:]);
				"ip=" =>
					if(rec.ip != nil) {
						sys->fprint(stderr, "bootp: warning: %s:%d IP address redefined for %s.\n",
							tabname, lnum, rec.hostname);
					}
					(err, rec.ip) = get_ipaddr(field[3:]);
				"bf=" =>
					if(rec.bf != nil) {
						sys->fprint(stderr, "bootp: warning: %s:%d bootfile redefined for %s.\n",
							tabname, lnum, rec.hostname);
					}
					(err, rec.bf) = get_path(field[3:]);
				"sm=" =>
					if(rec.sm != nil) {
						sys->fprint(stderr, "bootp: warning: %s:%d subnet mask redefined for %s.\n",
							tabname, lnum, rec.hostname);
					}
					(err, rec.sm) = get_ipaddr(field[3:]);
				"gw=" =>
					if(rec.gw != nil) {
						sys->fprint(stderr, "bootp: warning: %s:%d gateway redefined for %s.\n",
							tabname, lnum, rec.hostname);
					}
					(err, rec.gw) = get_ipaddr(field[3:]);
				"fs=" =>
					if(rec.fs != nil) {
						sys->fprint(stderr, "bootp: warning: %s:%d file server redefined for %s.\n",
							tabname, lnum, rec.hostname);
					}
					(err, rec.fs) = get_ipaddr(field[3:]);
				"au=" =>
					if(rec.au != nil) {
						sys->fprint(stderr,
							"bootp: warning: %s:%d authentication server redefined for %s.\n",
							tabname, lnum, rec.hostname);
					}
					(err, rec.au) = get_ipaddr(field[3:]);
				* =>
					sys->fprint(stderr,
						"bootp: update_records: %s:%d invalid or unsupported tag \"%s\" in entry for %s.\n",
						tabname, lnum, field[0:2], rec.hostname);
					continue FIELDS;
				}
				if(err != nil) {
					sys->fprint(stderr,
						"bootp: update_records: %s:%d %s for %s.\nbootp: skipping entry for %s.\n", 
						tabname, lnum, err, rec.hostname,
						rec.hostname);
					continue LINES;
				}
			}
			if(rec.ha == nil) {
				if(NEED_HA) {
					sys->fprint(stderr, "bootp: update_records: %s:%d no hardware address defined for %s.\n",
						tabname, lnum, rec.hostname);
					sys->fprint(stderr, "bootp: skipping entry for %s.\n", rec.hostname);
					continue LINES;
				}
			}
			if(rec.ip == nil) {
				if(NEED_IP) {
					sys->fprint(stderr, "bootp: update_records: %s:%d no IP address defined for %s.\n",
						tabname, lnum, rec.hostname);
					sys->fprint(stderr, "bootp: skipping entry for %s.\n", rec.hostname);
					continue LINES;
				}
			}
			if(rec.bf == nil) {
				if(NEED_BF) {
					sys->fprint(stderr, "bootp: update_records: %s:%d no bootfile defined for %s.\n",
						tabname, lnum, rec.hostname);
					sys->fprint(stderr, "bootp: skipping entry for %s.\n", rec.hostname);
					continue LINES;
				}
			}
			if(rec.sm == nil) {
				if(NEED_SM) {
					sys->fprint(stderr, "bootp: update_records: %s:%d no subnet mask defined for %s.\n",
						tabname, lnum, rec.hostname);
					sys->fprint(stderr, "bootp: skipping entry for %s.\n", rec.hostname);
					continue LINES;
				}
			}
			if(rec.gw == nil) {
				if(NEED_GW) {
					sys->fprint(stderr, "bootp: update_records: %s:%d no gateway defined for %s.\n",
						tabname, lnum, rec.hostname);
					sys->fprint(stderr, "bootp: skipping entry for %s.\n", rec.hostname);
					continue LINES;
				}
			}
			if(rec.fs == nil) {
				if(NEED_FS) {
					sys->fprint(stderr, "bootp: update_records: %s:%d no file server defined for %s.\n",
						tabname, lnum, rec.hostname);
					sys->fprint(stderr, "bootp: skipping entry for %s.\n", rec.hostname);
					continue LINES;
				}
			}
			if(rec.au == nil) {
				if(NEED_AU) {
					sys->fprint(stderr,
						"bootp: update_records: %s:%d no authentication server defined for %s.\n",
						tabname, lnum, rec.hostname);
					sys->fprint(stderr, "bootp: skipping entry for %s.\n", rec.hostname);
					continue LINES;
				}
			}
			if(debug) pinfbp(rec);
			trecs = rec :: trecs;
		}
		if(trecs == nil) {
			sys->fprint(stderr, "bootp: update_records: no valid entries in %s.\n", tabname);
			if(records != nil) {
				sys->fprint(stderr, "bootp: reverting to previous state.\n");
				return nil;
			}
			else {
				return "no entries.";
			}
		}
		records = array[len trecs] of ref InfBP;
		for(n = len records; n > 0; trecs = tl trecs) {
			records[--n] = hd trecs;
		}
	}
	return nil;
}

ishex(str: string): int
{
	if(str == nil) {
		return 0;
	}
	for(i := 0; i < len str; i++) {
		case str[i] {
		'0' to '9' or 'a' to 'f' or 'A' to 'F' =>
			continue;
		}
		return 0;
	}
	return 1;
}

isbyte(str: string): int
{
	if(str == nil) {
		return 0;
	}
	for(i := 0; i < len str; i++) {
		if(str[i] < '0' || str[i] > '9') {
			return 0;
		}
	}
	if(int str < 0 || int str > 255) {
		return 0;
	}
	return 1;
}

xval(c: int): int
{
	case c {
	'0' to '9' =>
		return c;
	'a' to 'f' =>
		return 10 + c - 'a';
	'A' to 'F' =>
		return 10 + c - 'A';
	}
	return -1;
}

get_haddr(str: string): (string, array of byte)
{
	if(len str != 12 || !ishex(str)) {
		return (sys->sprint("invalid hardware address \"%s\"", str), nil);
	}
	addr := array[6] of byte;
	for(i := 0; i < len addr; i++) {
		addr[i] = byte(16rF0 & (xval(str[2*i])<<4) | 16r0F & xval(str[2*i+1]));
	}
	return (nil, addr);
}

get_ipaddr(str: string): (string, array of byte)
{
	(n, fields) := sys->tokenize(str, ".");
	if(n != 4) {
		return (sys->sprint("invalid IP address \"%s\"", str), nil);
	}
	addr := array[4] of byte;
	for(n = 0; n < len addr; n++) {
		if(!isbyte(hd fields)) {
			return (sys->sprint("invalid IP address \"%s\"", str), nil);
		}
		addr[n] = byte hd fields;
		fields = tl fields;
	}
	return (nil, addr);
}

get_path(str: string): (string, array of byte)
{
	if(str == nil) {
		return ("nil path", nil);
	}
	path := array of byte str;
	if(len path > 128) {
		return (sys->sprint("path too long (>128 bytes) \"%s...\"", string path[0:16]), nil);
	}
	return (nil, path);
}

pipaddr(name: string, addr: int)
{
	sys->print("%s %d.%d.%d.%d ",
		name,
		(addr>>24) & 16rff,
		(addr>>16) & 16rff,
		(addr>>8)  & 16rff,
		addr & 16rff);
}

iptoa(addr: array of byte): string
{
	if(len addr != 4) {
		return "0.0.0.0";
	}
	return sys->sprint("%d.%d.%d.%d",
		int addr[0],
		int addr[1],
		int addr[2],
		int addr[3]);
}

dtoa(data: array of byte): string
{
	if(data == nil) {
		return nil;
	}
	result: string;
	for(i:=0; i < len data; i++) {
		result += sys->sprint(".%d", int data[i]);
	}
	return result[1:];
}

dtox(data: array of byte): string
{
	if(data == nil) {
		return nil;
	}
	result: string;
	for(i:=0; i < len data; i++) {
		result += sys->sprint(":%.02x", int data[i]);
	}
	return result[1:];
}

bptohw(bp: ref BootpPKT): string
{
	if(int bp.hlen > 0 && int bp.hlen < len bp.chaddr) {
		return dtox(bp.chaddr[0:int bp.hlen]);
	}
	return "";
}

ctostr(cstr: array of byte): string
{
	for(i:=0; i<len cstr; i++) {
		if(cstr[i] == byte 0) {
			break;
		}
	}
	if(i > 0) {
		return string cstr[0:i];
	}
	return "";
}

strtoc(str: string): array of byte
{
	cstr := array[1 + len array of byte str] of byte;
	cstr[0:] = array of byte str;
	cstr[len cstr - 1] = byte 0;
	return cstr;
}

ppkt(bootp: ref BootpPKT)
{
	sys->fprint(stderr, "BootpPKT {\n");
	sys->fprint(stderr, "\top == %d\n", int bootp.op);
	sys->fprint(stderr, "\thtype == %d\n", int bootp.htype);
	sys->fprint(stderr, "\thlen == %d\n", int bootp.hlen);
	sys->fprint(stderr, "\thops == %d\n", int bootp.hops);
	sys->fprint(stderr, "\txid == %d\n", bootp.xid);
	sys->fprint(stderr, "\tsecs == %d\n", bootp.secs);
	sys->fprint(stderr, "\tC client == %s\n", dtoa(bootp.ciaddr));
	sys->fprint(stderr, "\tY client == %s\n", dtoa(bootp.yiaddr));
	sys->fprint(stderr, "\tserver == %s\n", dtoa(bootp.siaddr));
	sys->fprint(stderr, "\tgateway == %s\n", dtoa(bootp.giaddr));
	sys->fprint(stderr, "\thwaddr == %s\n", bptohw(bootp));
	sys->fprint(stderr, "\thost == %s\n", bootp.sname);
	sys->fprint(stderr, "\tfile == %s\n", bootp.file);
	sys->fprint(stderr, "\tmagic == %s\n", magic(bootp.vend[0:4]));
	if(magic(bootp.vend[0:4]) == "plan9") {
		(n, strs) := sys->tokenize(string bootp.vend[4:], " \r\n");
		if(strs != nil) {
			sys->fprint(stderr, "\t\tsm == %s\n", hd strs);
			strs = tl strs;
		}
		if(strs != nil) {
			sys->fprint(stderr, "\t\tfs == %s\n", hd strs);
			strs = tl strs;
		}
		if(strs != nil) {
			sys->fprint(stderr, "\t\tau == %s\n", hd strs);
			strs = tl strs;
		}
		if(strs != nil) {
			sys->fprint(stderr, "\t\tgw == %s\n", hd strs);
			strs = tl strs;
		}
	}
	sys->fprint(stderr, "}\n\n");
}

arreq(a1: array of byte, a2: array of byte): int
{
	if(len a1 != len a2) {
		return 0;
	}
	for(i := 0; i < len a1; i++) {
		if(a1[i] != a2[i]) {
			return 0;
		}
	}
	return 1;
}

magic(cookie: array of byte): string
{
	if(arreq(cookie, array[] of { byte 'p', byte '9', byte ' ', byte ' ' })) {
		return "plan9";
	}
	if(arreq(cookie, array[] of { byte 99, byte 130, byte 83, byte 99 })) {
		return "rfc1048";
	}
	if(arreq(cookie, array[] of { byte 'C', byte 'M', byte 'U', byte 0 })) {
		return "cmu";
	}
	return dtoa(cookie);
}

pinfbp(rec: ref InfBP)
{
	sys->fprint(stderr, "Bootp entry {\n");
	sys->fprint(stderr, "\tha == %s\n", dtox(rec.ha));
	sys->fprint(stderr, "\tip == %s\n", dtoa(rec.ip));
	sys->fprint(stderr, "\tbf == %s\n", string rec.bf);
	sys->fprint(stderr, "\tsm == %s\n", dtoa(rec.sm));
	sys->fprint(stderr, "\tgw == %s\n", dtoa(rec.gw));
	sys->fprint(stderr, "\tfs == %s\n", dtoa(rec.fs));
	sys->fprint(stderr, "\tau == %s\n", dtoa(rec.au));
	sys->fprint(stderr, "}\n");
}

M2S(data: array of byte): (string, ref BootpPKT)
{
	if(len data < 300)
		return ("too short", nil);

	bootp := ref BootpPKT;

	bootp.op = data[0];
	bootp.htype = data[1];
	bootp.hlen = data[2];
	bootp.hops = data[3];
	bootp.xid = nhgetl(data[4:8]);
	bootp.secs = nhgets(data[8:10]);
	# data[10:12] unused
	bootp.ciaddr = data[12:16];
	bootp.yiaddr = data[16:20];
	bootp.siaddr = data[20:24];
	bootp.giaddr = data[24:28];
	bootp.chaddr = data[28:44];
	bootp.sname = ctostr(data[44:108]);
	bootp.file = ctostr(data[108:236]);
	bootp.vend = data[236:300];

	return (nil, bootp);
}

S2M(bootp: ref BootpPKT): array of byte
{
	data := array[364] of { * => byte 0 };

	data[0] = bootp.op;
	data[1] = bootp.htype;
	data[2] = bootp.hlen;
	data[3] = bootp.hops;
	data[4:] = nhputl(bootp.xid);
	data[8:] = nhputs(bootp.secs);
	# data[10:12] unused
	data[12:] = bootp.ciaddr;
	data[16:] = bootp.yiaddr;
	data[20:] = bootp.siaddr;
	data[24:] = bootp.giaddr;
	data[28:] = bootp.chaddr;
	data[44:] = array of byte bootp.sname;
	data[108:] = array of byte bootp.file;
	data[236:] = bootp.vend;

	return data;
}

nhgetl(data: array of byte): int
{
	return (int data[0]<<24) | (int data[1]<<16) |		
	       (int data[2]<<8) | int data[3];
}

nhgets(data: array of byte): int
{
	return (int data[0]<<8) | int data[1];
}

nhputl(value: int): array of byte
{
	return array[] of {
		byte (value >> 24),
		byte (value >> 16),
		byte (value >> 8),
		byte (value >> 0),
	};
}

nhputs(value: int): array of byte
{
	return array[] of {
		byte (value >> 8),
		byte (value >> 0),
	};
}

