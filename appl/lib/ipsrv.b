implement Ipsrv;

Mod : con "ipsrv";

include "sys.m";
	sys: Sys;
	stderr: ref Sys->FD;

include "draw.m";
include "qidcmp.m";
qi : Qidcmp;
Cdir : import qi;

# Caching option
include "scache.m";
sc : Scache;

include "ipsrv.m";

srvfile := "/services.txt";
dnsfile := "/services/dns/db";
iterations := 10;

init(nil : ref Draw->Context, args : list of string)
{
	if (args != nil) args = tl args;
	initsrv(args);
}

initsrv(args : list of string) : int
{
	if(sys == nil) {
	  sys = load Sys Sys->PATH;
	  stderr = sys->fildes(2);
	}
	for(;args != nil; args = tl args) {
	  key := hd args;
	  if (key != nil && tl args == nil) {
	    sys->fprint(stderr, Mod+": missing arg after %s\n", key);
	    return 0;
	  }
	  args = tl args;
	  val := hd args;
	  case key {
	    "-m" => if (val == "dnsport") dialdnsport = 1; else dialdnsport = 0;
	    "-s" => srvfile = val;
	    "-d" => dnsfile = val;
	    "-i" => iterations = int val;
	    "-c" => {
	      (nil, cache) := sys->tokenize(val, " \t");
	      if (len cache != 3) {
		sys->fprint(stderr, Mod+": unexpected %s %s\n", key, val);
		return 0;
	      }
	      sc = load Scache Scache->PATH;
	      if (sc != nil) sc->initcache(int hd cache, int hd tl cache, int hd tl tl cache);
	      else {
		sys->fprint(stderr, Mod+": load %s %r\n", Scache->PATH);
		return 0;
	      }
	    }
	    "iph2a" => lprint(iph2a(val));
	    "ipa2h" => lprint(ipa2h(val));
	    "ipn2p" => {
	      if (tl args == nil) {
		sys->fprint(stderr, Mod+": missing arg after %s\n", key);
		return 0;
	      }
	      args = tl args;
	      val2 := hd args;
	      sys->print("%s\n", ipn2p(val, val2));
	    }
	    * => {
	      usage();
	      return 0;
	    }
	  }
	}
	# for init only
	srvupdate();
	dnsupdate();
	return 1;
}

usage()
{

  sys->fprint(stderr, "Usage:\t"+Mod+" [options] cmd string [string2]\n\tcmd\t-- entry into ipsrv.m\n\toptions\t-- -m -s -d -i -c\n");
}

lprint(l : list of string)
{
  for(; l != nil; l = tl l)
    sys->print("%s\n", hd l);
}

#srvfile: con "/services.txt";

#
# Bufio is not multithread safe use rtoken instead
#

#include "bufio.m";

#FD, FileIO: import Sys;
#Iobuf: import Bufio;


#
# Maps key strings to value strings from flat file database. Rereads
# database file if mod time changes.
#

Serv: adt {
	port: string;
	net: string;
	names: array of string;
};
srvlist: list of Serv;
mtime: int;
servers : list of string;

# for namespace-safe update
cdir : ref Cdir;


include "rtoken.m";
rt : Rtoken;
Id : import rt;

srvupdate(): int
{
#	bufio := load Bufio Bufio->PATH;
#	if(bufio == nil) {
#		sys->fprint(stderr, "ipn2p: failed to load bufio: %r\n");
#		return 0;
#	}
#	if((mapfp := bufio->open(srvfile, Bufio->OREAD)) == nil){
#		sys->fprint(stderr, "srvopen: Can't open dbfile %s %r\n", srvfile);
#		return 0;
#	}
	if (rt == nil) {
	  rt = load Rtoken Rtoken->PATH;
	  if (rt == nil) {
	    sys->fprint(stderr, "srv: load %s %r\n", Rtoken->PATH);
	    return 0;
	  }
	}
	fd := sys->open(srvfile, Sys->OREAD);
	if (fd == nil) {
	  sys->fprint(stderr, "srv: open %s %r\n", Rtoken->PATH);
	  return 0;
	}
	id := rt->id();

	nsrvlist: list of Serv;
	serv: Serv;
#	while((srvstr := bufio->mapfp.gets('\n')) != nil){}
	while((srvstr := rt->readtoken(fd, "\n", id)) != nil) {
		n := 0;
		while(n < len srvstr) {
			if(srvstr[n] == '#') break;
			n++;
		}
		if(n < len srvstr)
			srvstr = srvstr[0:n];
		(nf, slist) := sys->tokenize(srvstr, " \t\r\n");
		if (nf == 0)	# blank line
			continue;
		if(nf < 2){
			sys->fprint(stderr, "ipn2p: services record with %d field(s)\n", nf);
			nsrvlist = nil;
			break;
		}
		serv.names = array[n] of string;
		s := 0;
		serv.names[s++] = hd slist;
		slist = tl slist;
		(nr, nlist) := sys->tokenize(hd slist, "/");
		if(nr != 2) {
			sys->fprint(stderr, "ipn2p: services port/net entry with %d field(s)\n", nr);
			nsrvlist = nil;
			break;
		}
		serv.port = hd nlist;
		serv.net = hd tl nlist;
		nlist = nil;
		while(slist != nil && s < len serv.names) {
			serv.names[s++] = hd slist;
			slist = tl slist;
		}
		nsrvlist = serv :: nsrvlist;
	}
	if(nsrvlist != nil)
		srvlist = nsrvlist;
	return 1;
}

ipn2p(net, service: string): string
{
	if(sys == nil){
		sys = load Sys Sys->PATH;
		stderr = sys->fildes(2);
	}
	if(net == nil){
		sys->fprint(stderr, "ipn2p: Attempt to map nil net\n");
		return nil;
	}
	if(service == nil){
		sys->fprint(stderr, "ipn2p: Attempt to map nil service\n");
		return nil;
	}

	#
	# (re)read the services file as necessary.  There is a race here, but
	# it will typically result in loss of efficiency rather than incorrect
	# results (we may read the file multiple times in separate threads).
	#
	(n, dir) := sys->stat(srvfile);
	if(n < 0) {
		sys->fprint(stderr, "ipn2p: cannot stat %s: %r\n", srvfile);
		return nil;
	}
	if(dir_changep(ref dir)){
		if(!srvupdate())
			return nil;
		mtime = dir.mtime;
	}

	for(nsrvlist := srvlist; nsrvlist != nil; nsrvlist = tl nsrvlist){
		serv := hd nsrvlist;
		if(net == serv.net)
			for(n = 0; n < len serv.names; n++) {
				if(serv.names[n] == service) {
					return serv.port;
				}
			}
	}
	return nil;
}

dir_changep(dir : ref Sys->Dir) : int
{
  ldqi();
  if (qi != nil && cdir == nil)
    cdir = ref Cdir(nil, Qidcmp->SAME);
  if (cdir != nil)
    return cdir.cmp(dir);
  else
    return mtime == 0 || mtime != dir.mtime;
}

warned := 0;
ldqi()
{
  if((qi = load Qidcmp Qidcmp->PATH) == nil) {
    if (!warned) {
      sys->fprint(stderr, "srv: load %s%r\n", Qidcmp->PATH);
      warned = 1;
    }
  }
  else qi->init(nil, nil);
}

#
# DNS Name Resolution Segment
#

dialdnsport := 0;
#dnsfile: con "/services/dns/db";
netroot: con "/net/tcp/";
dnsport: con 53;
#iterations: con 10;

 ######################
 ### Header Section ###

QR_MASK:	con 16r80;
QR_OFFSET:	con 7;
QR_QUERY:	con 0;
QR_RESPONSE:	con 1;

OP_MASK:	con 16r78;
OP_OFFSET:	con 3;
OP_QUERY:	con 0;
OP_IQUERY:	con 1;
OP_STATUS:	con 2;

AA_MASK:	con 16r04;
AA_OFFSET:	con 2;
AA_AUTHORITY:	con 1;

TC_MASK:	con 16r02;
TC_OFFSET:	con 1;
TC_TRUNCATED:	con 1;

RD_MASK:	con 16r01;
RD_OFFSET:	con 0;
RD_RNDESIRED:	con 0;
RD_RDESIRED:	con 1;

 ### Octet 3 ###
RA_MASK:	con 16r80;
RA_OFFSET:	con 7;
RA_RAVAIL:	con 1;

Z_MASK:		con 16r70;
Z_OFFSET:	con 4;
Z_ZERO:		con 0;

RCODE_MASK:	con 16r0F;
RCODE_OFFSET:	con 0;
RCODE_NOERR:	con 0;
RCODE_FMTERR:	con 1;
RCODE_SRVERR:	con 2;
RCODE_NAMERR:	con 3;
RCODE_NOTIMPL:	con 4;
RCODE_REFUSED:	con 5;

 ########################
 ### Question Section ###

 ### Octets 0 - n ###
 # QNAME: Sequence of labels, ending with null/root label

 ### Octets n+1, n+2 ###
 # QTYPE
 # + all TYPE_* cons

QTYPE_AXFR,
QTYPE_MAILB,
QTYPE_MAILA,
QTYPE_ALL:	con 250+iota;

 ### Octets n+3, n+4 ###
 # QCLASS
 # + all CLASS_* cons

QCLASS_ALL:	con 255;

 ###############################
 ### Resource Record Section ###

 ### Octets 0 - n ###
 # NAME: domain-name label

NAME_PTR:	con 16rC0;
NAME_OFFMASK:	con 16r3F;

 ### Octets n+1, n+2 ###
 # TYPE

TYPE_A,		TYPE_NS,
TYPE_MD,	TYPE_MF,
TYPE_CNAME,	TYPE_SOA,
TYPE_MB,	TYPE_MG,
TYPE_MR,	TYPE_NULL,
TYPE_WKS,	TYPE_PTR,
TYPE_HINFO,	TYPE_MINFO,
TYPE_MX,
TYPE_TXT:	con 1+iota;

 ### Octets n+3, n+4 ###
 # CLASS

CLASS_IN,
CLASS_CS,
CLASS_CH,
CLASS_HS:	con 1+iota;

 ### Octets n+5 - n+8 ###
 # TTL
 # cache time (32 bit uint)

 ### Octets n+7, n+8 ###
 # RDLENGTH

 #################
 ### Protocols ###
ICMP,
IGMP,
GGP:		con 1+iota;	# 1-3

ST,		TCP,
UCL,		EGP,
IGP,		BBN_RCC_MON,
NVP_II,		PUP,
ARGUS,		EMCON,
XNET,		CHAOS,
UDP,		MUX,
DCN_MEAS,	HMP,
PRM,		XNS_IDP,
TRUNK_1,	TRUNK_2,
LEAF_1,		LEAF_2,
RDP,		IRTP,
ISO_TP4,	NETBLT,
MFE_NSP,	MERIT_INP,
SEP:		con 5+iota;	# 5-33

ANYINTERNAL,	CFTP,
ANYLOCAL,	SAT_EXPAK,
MIT_SUBNET,	RVD,
IPPC,		ANYDIST,
SAT_MON:	con 61+iota;	# 61-69

IPCV:		con 71;

BR_SAT_MON:	con 76;

WB_MON,
WB_EXPAK:	con 78+iota;	# 78-79

RESERVED:	con 255;

dnsques: adt {
	qname: string;
	qtype: int;
	qclass: int;
};

dnsrr: adt {
	name: string;
	dtype: int;
	class: int;
	ttl: int;
	rdlength: int;
	rdata: array of byte;
};

dnsmsg: adt {
	id: int;

	qr: int;
	opcode: int;
	aa: int;
	tc: int;
	rd: int;
	ra: int;
	z: int;
	rcode: int;

	qdcount: int;
	ancount: int;
	nscount: int;
	arcount: int;

	qdlist: list of dnsques;
	anlist: list of dnsrr;
	nslist: list of dnsrr;
	arlist: list of dnsrr;
};

rrlen(rrl: list of dnsrr): int
{
	l := 0;
	while(rrl != nil) {
		# if name is formatted correctly, the length of name
		# should be the same as the length of the string + 2
		l += (len (hd rrl).name+2) + 10 + (hd rrl).rdlength;
		rrl = tl rrl;
	}
	return l;
}


putword(val: int, aob: array of byte, loc: int): (array of byte, int)
{
	if(loc + 2 > len aob)
		return (aob, loc * -1);

	h := byte ((val & 16r0000FF00)>>8 & 16r000000FF);
	l := byte (val & 16r000000FF);

	aob[loc++] = h;
	aob[loc++] = l;

	return (aob, loc);
}

putlong(val: int, aob: array of byte, loc: int): (array of byte, int)
{
	if(loc + 4 > len aob)
		return (aob, loc * -1);

	h := int (big val & 16rFFFF0000)>>16 & 16r0000FFFF;
	l := val & 16r0000FFFF;

	(aob, loc) = putword(h, aob, loc);
	(aob, loc) = putword(l, aob, loc);

	return (aob, loc);
}


getword(aob: array of byte, loc: int): (int, int)
{
	if(loc + 2 > len aob)
		return (0, loc * -1);

	h := int aob[loc++];
	l := int aob[loc++];

	val := ((h & 16r000000FF)<<8 & 16r0000FF00) | (l & 16r000000FF)
		& 16r0000FFFF;

	return (val, loc);
}

getlong(aob: array of byte, loc: int): (int, int)
{
	if(loc + 4 > len aob)
		return (0, loc * -1);

	h, l: int;
	(h, loc) = getword(aob, loc);
	(l, loc) = getword(aob, loc);

	val := int (big (h & 16r0000FFFF)<<16 & 16rFFFF0000)
		| (l & 16r0000FFFF);

	return (val, loc);
}

putdn(str: string, aob: array of byte, loc: int): (array of byte, int)
{
	l := 0;
	while(l < len str) {
		flen: int;
		for(c := l; c < len str && int str[c] != '.'; c++);
		if(c >= len str || int str[c] != '.') {
			flen = len str - l;
		}
		else {
			flen = c - l;
		}
		if(flen < 1 || loc >= len aob)
			return (aob, loc * -1);
		aob[loc++] = byte flen;
		for(; flen > 0; flen--) {
			case int str[l] {
			'0' to '9' or 'a' to 'z' or 'A' to 'Z' or '-' =>
				if(l >= len str || loc >= len aob)
					return (aob, loc * -1);
				aob[loc++] = byte str[l++];
			* =>
				# illegal character
				return (aob, loc * -1);
			}
		}
		l++; # skip dot
	}
	if(loc >= len aob)
		return (aob, loc * -1);
	aob[loc++] = byte 0;
	return (aob, loc);
}

getdn(aob: array of byte, loc: int): (string, int)
{
	name := "";
	while(loc < len aob && int aob[loc] != 0) {
		l := aob[loc++];
		if((l & byte 16rC0) == byte 16rC0) {
			aob[loc-1] = aob[loc-1] & byte 16r3F;
			ploc: int;
			(ploc, loc) = getword(aob, loc-1);
			if(ploc >= len aob)
				return ("", loc * -1);
			pname: string;
			(pname, ploc) = getdn(aob, ploc);
			name += pname;
			if(ploc < 1) {
				return (name, ploc);
			}
			else {
				return (name, loc); # pointer ends dn
			}
		}
		else if((l & byte 16rC0) == byte 0) {
			if(loc + int l > len aob)
				return (name, loc * -1);
			name += string aob[loc:loc+int l] + ".";
			loc += int l;
		}
		else
			return (name, loc * -1);
	}
	return (name, loc + 1);
}

putquesl(dnsquesl: list of dnsques, aob: array of byte, loc: int):
		(array of byte, int)
{
	quesl := dnsquesl;
	while(quesl != nil) {
		(aob, loc) = putdn((hd quesl).qname, aob, loc);
		if(loc < 1)
			return (aob, loc);
		(aob, loc) = putword((hd quesl).qtype, aob, loc);
		if(loc < 1)
			return (aob, loc);
		(aob, loc) = putword((hd quesl).qclass, aob, loc);
		quesl = tl quesl;
	}
	return (aob, loc);
}

getquesl(nq: int, aob: array of byte, loc: int): (list of dnsques, int)
{
	quesl: list of dnsques;
	for(i := 0; i < nq; i++) {
		qd: dnsques;
		(qd.qname, loc) = getdn(aob, loc);
		if(loc < 1)
			return (quesl, loc);
		(qd.qtype, loc) = getword(aob, loc);
		if(loc < 1)
			return (quesl, loc);
		(qd.qclass, loc) = getword(aob, loc);
		if(loc < 1)
			return (quesl, loc);
		quesl = qd :: quesl;
	}
	q: list of dnsques;
	while(quesl != nil) {
		q = (hd quesl) :: q;
		quesl = tl quesl;
	}
	return (q, loc);
}

putrrl(dnsrrl: list of dnsrr, aob: array of byte, loc: int):
		(array of byte, int)
{
	rrl := dnsrrl;
	while(rrl != nil) {
		if(len (hd rrl).rdata != (hd rrl).rdlength)
			return (aob, loc * -1);
		(aob, loc) = putdn((hd rrl).name, aob, loc);
		if(loc < 1)
			return (aob, loc);
		(aob, loc) = putword((hd rrl).dtype, aob, loc);
		if(loc < 1)
			return (aob, loc);
		(aob, loc) = putword((hd rrl).class, aob, loc);
		if(loc < 1)
			return (aob, loc);
		(aob, loc) = putlong((hd rrl).ttl, aob, loc);
		if(loc < 1)
			return (aob, loc);
		(aob, loc) = putword((hd rrl).rdlength, aob, loc);
		if(loc < 1)
			return (aob, loc);
		if(loc + (hd rrl).rdlength > len aob)
			return (aob, loc * -1);
		aob[loc:] = (hd rrl).rdata[0:(hd rrl).rdlength];
		loc += (hd rrl).rdlength;
		rrl = tl rrl;
	}
	return (aob, loc);
}

getrrl(nr: int, aob: array of byte, loc: int): (list of dnsrr, int)
{
	rrl: list of dnsrr;
	rrl = nil;
	for(i := 0; i < nr; i++) {
		rr: dnsrr;
		(rr.name, loc) = getdn(aob, loc);
		if(loc < 1)
			return (rrl, loc);
		(rr.dtype, loc) = getword(aob, loc);
		if(loc < 1)
			return (rrl, loc);
		(rr.class, loc) = getword(aob, loc);
		if(loc < 1)
			return (rrl, loc);
		(rr.ttl, loc) = getlong(aob, loc);
		if(loc < 1)
			return (rrl, loc);
		(rr.rdlength, loc) = getword(aob, loc);
		if(loc < 1)
			return (rrl, loc);
		if(rr.rdlength>0) {
			n: int = loc+rr.rdlength;
			if(n > len aob)
				n = len aob;
			rr.rdata = array[rr.rdlength] of byte;
			rr.rdata[0:] = aob[loc:n];
			loc = n;
		}
		else
			rr.rdata = nil;
		rrl = rr :: rrl;
	}
	r: list of dnsrr;
	r = nil;
	while(rrl != nil) {
		r = (hd rrl) :: r;
		rrl = tl rrl;
	}
	return (r, loc);
}

msg2aob(msg: dnsmsg): array of byte
{
	ml := 12; # header length in octets
	if(len msg.qdlist != msg.qdcount ||
		len msg.anlist != msg.ancount ||
		len msg.nslist != msg.nscount ||
		len msg.arlist != msg.arcount)
	{
		return array of byte("");
	}
	qdl := msg.qdlist;
	while(qdl != nil) {
		# if name is formatted correctly, the length of qname
		# should be the same as the length of the string + 2
		ml += (len (hd qdl).qname+2) + 4;
		qdl = tl qdl;
	}
	ml += rrlen(msg.anlist);
	ml += rrlen(msg.nslist);
	ml += rrlen(msg.arlist);

	aob := array[ml+2] of byte;
	octet: byte;

	l := 0;
	(aob, l) = putword(ml, aob, l);
	if(l < 1 || l >= len aob)
		return array of byte("");

	(aob, l) = putword(msg.id, aob, l);
	if(l < 1 || l >= len aob)
		return array of byte("");

	octet = byte
		(16r00 |
		(msg.qr<<QR_OFFSET)&QR_MASK |
		(msg.opcode<<OP_OFFSET)&OP_MASK |
		(msg.aa<<AA_OFFSET)&AA_MASK |
		(msg.tc<<TC_OFFSET)&TC_MASK |
		(msg.rd<<RD_OFFSET)&RD_MASK);
	aob[l++] = octet;

	octet = byte
		(16r00 |
		(msg.ra<<RA_OFFSET)&RA_MASK |
		(msg.z<<Z_OFFSET)&Z_MASK |
		(msg.rcode<<RCODE_OFFSET)&RCODE_MASK);
	if(l >= len aob)
		return array of byte("");
	aob[l++] = octet;

	(aob, l) = putword(msg.qdcount, aob, l);
	if(l < 1)
		return array of byte("");

	(aob, l) = putword(msg.ancount, aob, l);
	if(l < 1)
		return array of byte("");

	(aob, l) = putword(msg.nscount, aob, l);
	if(l < 1)
		return array of byte("");

	(aob, l) = putword(msg.arcount, aob, l);
	if(l < 1)
		return array of byte("");

	(aob, l) = putquesl(msg.qdlist, aob, l);
	if(l < 1)
		return array of byte("");

	(aob, l) = putrrl(msg.anlist, aob, l);
	if(l < 1)
		return array of byte("");

	(aob, l) = putrrl(msg.nslist, aob, l);
	if(l < 1)
		return array of byte("");

	(aob, l) = putrrl(msg.arlist, aob, l);
	if(l < 1)
		return array of byte("");
	
	return aob;
}

aob2msg(aob: array of byte): dnsmsg
{
	msg: dnsmsg;
	l := 0;

	(msg.id, l) = getword(aob, l);
	if(l < 1 || l >= len aob)
		return msg;
	octet := aob[l++];
	msg.qr = (int octet & QR_MASK)>>QR_OFFSET & 16r00000001;
	msg.opcode = (int octet & OP_MASK)>>OP_OFFSET & 16r0000000F;
	msg.aa = (int octet & AA_MASK)>>AA_OFFSET & 16r00000001;
	msg.tc = (int octet & TC_MASK)>>TC_OFFSET & 16r00000001;
	msg.rd = (int octet & RD_MASK)>>RD_OFFSET & 16r00000001;
	if(l >= len aob)
		return msg;
	
	octet = aob[l++];
	msg.ra = (int octet & RA_MASK)>>RA_OFFSET & 16r00000001;
	msg.z = (int octet & Z_MASK)>>Z_OFFSET & 16r00000007;
	msg.rcode = (int octet & RCODE_MASK)>>RCODE_OFFSET & 16r0000000F;
	if(l >= len aob)
		return msg;
	
	(msg.qdcount, l) = getword(aob, l);
	if(l < 1)
		return msg;
	
	(msg.ancount, l) = getword(aob, l);
	if(l < 1)
		return msg;
	
	(msg.nscount, l) = getword(aob, l);
	if(l < 1)
		return msg;
	
	(msg.arcount, l) = getword(aob, l);
	if(l < 1)
		return msg;
	
	(msg.qdlist, l) = getquesl(msg.qdcount, aob, l);
	if(l < 1)
		return msg;
	
	(msg.anlist, l) = getrrl(msg.ancount, aob, l);
	if(l < 1)
		return msg;
	
	(msg.nslist, l) = getrrl(msg.nscount, aob, l);
	if(l < 1)
		return msg;
	
	(msg.arlist, l) = getrrl(msg.arcount, aob, l);
	if(l < 1)
		return msg;
	
	return msg;
}


readn(fd: ref Sys->FD, nb: int): array of byte
{
	m: int;
	buf:= array[nb] of byte;
	b1:= array[nb] of byte;

	for(n:=0; n<nb; n+=m){
		m = sys->read(fd, b1, nb-n);
		if(m <= 0)
			return nil;
		for(j:=0; j<m; j++)
			buf[n+j] = b1[j];
	}
	return buf;
}

isdigit(c: int): int
{
	return (c >= '0' && c <= '9');
}

isnum(num: string): int
{
	for(i:=0; i<len num; i++)
		if(!isdigit(num[i]))
			return 0;
	return 1;
}

isipaddr(addr: string): int
{
	(nil, fields) := sys->tokenize(addr, ".");
	if(fields == nil)
		return 0;
	for(; fields != nil; fields = tl fields) {
		if(!isnum(hd fields) ||
			int hd fields < 0 ||
			int hd fields > 255)
			return 0;
	}
	return 1;
}

# rotation on servers list
nrot := 0;

# last mod time of dns/db - obsolete
smtime := 0;

# for namespace-safe update
scdir : ref Cdir;

# domain name from dns/db
domain : string;

dnsupdate()
{
  readservers(dnsfile, Mod);
}

readservers(fname, err: string): list of string
{
	(ok, dir) := sys->stat(fname);
	if(ok != 0) {
		sys->fprint(stderr, "%s: unable to stat %s\n", err, fname);
		return nil;
	}
	if(!sdir_changep(ref dir) && servers != nil) {
		# required performance improvement -- move pass bad dns!
		if (nrot) {
#		  	sys->print("rotate %d into %d\n", nrot, len servers);
			servers = rotate(servers, nrot);
			nrot = 0;
		}
		return servers;
	}
	smtime = dir.mtime;
#	sys->print("readservers: open %s \n", fname);
	fd := sys->open(fname, Sys->OREAD);
	if(fd == nil) {
		sys->fprint(stderr, "%s: unable to open %s %r\n", err, fname);
		return nil;
	}
	buf := array[2048] of byte;
	n := sys->read(fd, buf, len buf);
	(nil, retservers) := sys->tokenize(string buf[0:n], "\r\n");
	buf = nil;
	if(len retservers < 1) {
		sys->fprint(stderr, "%s: empty %s\n", err, fname);
		return nil;
	}
	ls: list of string;
	ndomain : string;
	for(; retservers != nil; retservers = tl retservers) {
		if(isipaddr(hd retservers)) {
			ls = hd retservers :: ls;
		}
		else if(alphanamep(hd retservers)) {
		  ndomain = hd retservers;
		}
		else if(!char0p('#', hd retservers)) # Comment on first char only
			sys->fprint(stderr, "%s: invalid server %s\n", err,
				hd retservers);
	}
	if(len ls < 1) {
		sys->fprint(stderr, "%s: no valid servers\n", err);
		return nil;
	}
	for(; ls != nil; ls = tl ls)
		retservers = hd ls :: retservers;
	domain = ndomain;
	return servers = retservers;
}

sdir_changep(dir : ref Sys->Dir) : int
{
  ldqi();
  if (qi != nil && scdir == nil)
    scdir = ref Cdir(nil, Qidcmp->SAME);
  if (scdir != nil)
    return scdir.cmp(dir);
  else
    return smtime == 0 || smtime != dir.mtime;
}

rotate(l : list of string, n : int) : list of string
{
	m := n % (len l);
	if (m == 0) return l;
	if (m < 0) m = (len l) + m;
	nl : list of string;
	for(i := 0; i++ < m; l = tl l) nl = hd l :: nl;
	nl = reverse(nl);
	return append(l, nl);
}

reverse(l : list of string) : list of string
{
	# sys->print("reverse\n");
	r : list of string;
	for(; l != nil; l = tl l) r = hd l :: r;
	return r;
}

append(h, t : list of string) : list of string
{
	r := reverse(h);
	for(; r != nil; r = tl r) t = hd r :: t;
	return t;
}

char0p(c : int, s : string) : int
{
	return s != nil && s[0] == c;
}

# names that will not hang iph2a
alphanamep(s : string) : int
{
  if (s == nil) return 0;
  for (i := 0; i < len s; i++) {
    c := s[i];
    if (('0' <= c && c <= '9') ||
	('a' <= c && c <= 'z') ||
	('A' <= c && c <= 'Z') ||
	c == '.') continue;
    else return 0;
  }
  return 1;
}

# add subdomains - domain is read from dns/db
indomain(name : string) : list of string
{
  inl : list of string;
  if (domain != nil) {
    hn := count('.', name);
    dn := count('.', domain);
    for (n := dn -1; n >= 0; n--)
      inl = name+"."+subcount('.', n, domain) :: inl;
  }
  return name :: inl;
}

count(e : int, s : string) : int
{
  for ((i, n) := (0, 0); i < len s; i++)
    if (s[i] == e) n++;
  return n;
}

subcount(e, n : int, s : string) : string
{
  if (!n || s == nil) return s;
  for ((i, m) := (0, 0); i < len s; i++)
    if (s[i] == e && ++m == n) break;
  if (++i < len s) return s[i:];
  return nil;
}

iph2a(mach: string): list of string
{
	if (!alphanamep(mach)) {
		sys->fprint(stderr, "iph2a: bad host name: %s\n", mach);
		return nil;
	}
	tmpservers := readservers(dnsfile, "iph2a");
	iter := 0;
	good := 0;
WSVR: while(tmpservers != nil && iter++ < iterations) {
	(ok, tcpfd) := opentcpfd("iph2a", hd tmpservers);
	tmpservers = tl tmpservers;
	if (ok == -1) {
		err := sys->sprint("%r");
		# don not retry modem dialup on every DNS server!
		if (err[0:10] == "cs: dialup")
			sys->raise("fail: " + err[4:]);
		continue WSVR;
	}
	# remember a good server so cs performance goes up!
	if (!good) {nrot = iter -1; good = 1;}
	for (lm := indomain(mach); lm != nil; lm = tl lm)
	  if ((al := iph2a_fd(tcpfd, hd lm, tl lm == nil)) != nil)
	    if (hd al == "!") continue WSVR;
	    else return al;
	}
	return nil;
}

iph2a_fd(tcpfd : ref Sys->FD, mach: string, lastp : int): list of string
{
	fail := 0;
WSVR: while(!fail) {
	fail = 1;	# really a single pass
	dm: dnsmsg;

	dm.id = 1;
	dm.qr = QR_QUERY;
	dm.opcode = OP_QUERY;
	dm.aa = 0;
	dm.tc = 0;
	dm.rd = RD_RDESIRED;
	dm.ra = 0;
	dm.z = Z_ZERO;
	dm.rcode = 0;

	dm.qdcount = 1;
	dm.ancount = 0;
	dm.nscount = 0;
	dm.arcount = 0;

	qd: dnsques;
	qd.qname = mach;
	qd.qtype = TYPE_A;
	qd.qclass = CLASS_IN;
	dm.qdlist = qd :: nil;

	aob := msg2aob(dm);

	#tcpfd := conn.dfd;

	n := sys->write(tcpfd, aob, len aob);
	if(n != len aob) {
		sys->fprint(stderr, "iph2a: error writing tcpfd %r\n");
		return nil;
	}

	sys->seek(tcpfd, 0, 0);
	buf := readn(tcpfd, 2);
	if(buf == nil) {
		sys->fprint(stderr, "iph2a: error reading tcpfd %r\n");
		return nil;
	}

	(mlen, nil) := getword(buf, 0);

	sys->seek(tcpfd, 0, 0);
	buf = readn(tcpfd, mlen);
	if(buf == nil) {
		sys->fprint(stderr, "iph2a: error reading tcpfd %r\n");
		return nil;
	}

	dm2: dnsmsg;
	dm2 = aob2msg(buf);
	case dm2.rcode {
		RCODE_NOERR =>
			;
		RCODE_FMTERR =>
			sys->fprint(stderr, "iph2a: format error\n");
			return nil;
		RCODE_SRVERR =>
			continue WSVR;
		RCODE_NAMERR =>
			if (lastp) sys->fprint(stderr, "iph2a: name error\n");
			return nil;
		RCODE_NOTIMPL =>
			continue WSVR;
		RCODE_REFUSED =>
			continue WSVR;
		* =>
			continue WSVR;
	}
	#printmsg(stderr, dm2);
	ips: list of string;
	anl := dm2.anlist;
	while(anl != nil) {
		an := hd anl;
		anl = tl anl;
		if(an.dtype == TYPE_A) {
			rd := an.rdata;
			if(len rd == 4) {
				ipa := sys->sprint("%d.%d.%d.%d",
					int rd[0],
					int rd[1],
					int rd[2],
					int rd[3]);
				ips = ipa :: ips;
			}
		}
		else if(an.dtype == TYPE_CNAME) {
			#an alias; look for this now
			(cname, err) := getdn(an.rdata, 0);
			if(err>0) {
				#we have the alias
				return iph2a(cname);
			}
			#otherwise, keep going and see if something happens
		}
	}
	if(ips != nil) {
#		iter = 0;
		return ips;
	}
	}
	return "!" :: nil;
}

ipa2h(addr: string): list of string
{
	tmpservers := readservers(dnsfile, "iph2a");
        iter := 0;
	good := 0;
WSVR: while(tmpservers != nil && iter++ < iterations) {
	(ok, tcpfd) := opentcpfd("ipa2h", hd tmpservers);
	if (ok == -1) {
		err := sys->sprint("%r");
		# do not retry modem dialup on every DNS server!
		if (err[0:10] == "cs: dialup")
			sys->raise("fail: " + err[4:]);
		continue WSVR;
	}

	# remember a good server for cs performance!
	if (!good) {nrot = iter -1; good = 1;}

	dm: dnsmsg;

	dm.id = 1;
	dm.qr = QR_QUERY;
	dm.opcode = OP_IQUERY;
	dm.aa = 0;
	dm.tc = 0;
	dm.rd = RD_RNDESIRED;
	dm.ra = 0;
	dm.z = Z_ZERO;
	dm.rcode = 0;

	dm.qdcount = 0;
	dm.ancount = 1;
	dm.nscount = 0;
	dm.arcount = 0;

	ad: dnsrr;
	ad.name = "root";
	ad.dtype = TYPE_A;
	ad.class = CLASS_IN;
	ad.ttl = 0;
	ad.rdlength = 4;

	ipseg: list of string;

	n : int;
	(n, ipseg) = sys->tokenize(addr, ".");
	if (n != 4) {
		sys->fprint(stderr, "ipa2h: bad address %s\n", addr);
		return nil;
	}

	ad.rdata = array[4] of byte;
	ad.rdata[0] = byte hd ipseg;
	ad.rdata[1] = byte hd tl ipseg;
	ad.rdata[2] = byte hd tl tl ipseg;
	ad.rdata[3] = byte hd tl tl tl ipseg;

	dm.anlist = ad::nil;

	aob := msg2aob(dm);


	#tcpfd := conn.dfd;


	n = sys->write(tcpfd, aob, len aob);
	if(n != len aob) {
		sys->fprint(stderr, "ipa2h: error writing tcpfd %r\n");
		return nil;
	}

	sys->seek(tcpfd, 0, 0);
	buf := readn(tcpfd, 2);

	(mlen, nil) := getword(buf, 0);

	sys->seek(tcpfd, 0, 0);
	buf = readn(tcpfd, mlen);
	if(buf == nil) {
		sys->fprint(stderr, "ipa2h: error reading tcpfd %r\n");
		return nil;
	}

	dm2: dnsmsg;
	dm2 = aob2msg(buf);
	case dm2.rcode {
		RCODE_NOERR =>
			;
		RCODE_FMTERR =>
			sys->fprint(stderr, "ipa2h: format error\n");
			return nil;
		RCODE_SRVERR =>
			continue WSVR;
		RCODE_NAMERR =>
			sys->fprint(stderr, "ipa2h: name error\n");
			return nil;
		RCODE_NOTIMPL =>
			continue WSVR;
		RCODE_REFUSED =>
			continue WSVR;
		* =>
			continue WSVR;
	}
	dms: list of string;
	qdl := dm2.qdlist;
	while(qdl != nil) {
		qd := hd qdl;
		qdl = tl qdl;
		rd := qd.qname[0:(len qd.qname)-1];
		dms = rd :: dms;
	}
	if(dms != nil) {
		iter = 0;
		return dms;
	}
	else {
		nsl: list of string;
		arl := dm2.arlist;
		while(arl != nil) {
			ar := hd arl;
			arl = tl arl;
			rd := ar.rdata;
			if(len rd == 4) {
				ipa := sys->sprint("%d.%d.%d.%d",
					int rd[0],
					int rd[1],
					int rd[2],
					int rd[3] );
				nsl = ipa :: nsl;
			}
		}
		s: list of string;
		while(tmpservers != nil) {
			s = hd tmpservers :: s;
			tmpservers = tl tmpservers;
		}
		while(nsl != nil) {
			s = hd nsl :: s;
			nsl = tl nsl;
		}
		servers = nil;
		while(s != nil) {
			tmpservers = hd s :: tmpservers;
			s = tl s;
		}
	}
} # while servers != nil
	return nil;
}

# Unification code

opentcpfd_dns(owner, server : string) : (int, ref Sys->FD)
{
	(ok, conn) := sys->dial( "tcp!"+server+"!"+(string dnsport), nil);
	if (ok == -1) {
		sys->fprint(stderr, "%s: error connecting to dns server: %r\n", owner);
		return (ok, nil);
	}
	return (ok, conn.dfd);
}

opentcpfd(owner, server : string) : (int, ref Sys->FD)
{
	if (dialdnsport) return opentcpfd_dns(owner, server);

	fname := netroot + "clone";
	cfd := sys->open(fname, Sys->ORDWR);
	if(cfd == nil) {
		sys->fprint(stderr, "%s: unable to open %s %r\n", owner, fname);
		return (-1, nil);
	}
	buf := array[2048] of byte;
	cmsg := array of byte sys->sprint("connect %s!%d", server, dnsport);
	n := sys->write(cfd, cmsg, len cmsg);
	if(n != len cmsg) {
		sys->fprint(stderr, "%s: error writing to %s %r\n", owner, fname);
		return (-1, nil);
	}
	sys->seek(cfd, 0, 0);
	n = sys->read(cfd, buf, len buf);
	if(n <= 0) {
		sys->fprint(stderr, "%s: error reading %s %r\n", owner, fname);
		return (-1, nil);
	}
	fname = netroot + string buf[0:n] + "/data";
	tcpfd := sys->open(fname, Sys->ORDWR);
	if(tcpfd == nil) {
		sys->fprint(stderr, "%s: unable to open %s %r\n", owner, fname);
		return (-1, nil);
	}
	return (0, tcpfd);
}
