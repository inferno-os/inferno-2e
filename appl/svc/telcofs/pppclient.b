implement PPPClient;

include "sys.m";
	sys: Sys;
	FD, Connection: import Sys;
	stderr, stdin: ref FD;
	dial, open, read, write, print, fprint, mount, tokenize, millisec: import sys;
include "draw.m";
	Context: import Draw;
include "string.m";
	str: String;
include "telco.m";
	telco: Telco;

PPPClient: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

Modem: adt {
	pid:	int;
	fd:		ref FD;
	avail:	array of byte;
	rd:		chan of array of byte;
};

debug:		int = 0;
modem:		ref Modem;
argv0:		con "pppclient";
usagestr:		con "usage: pppclient [-d delim] [-b baud] [-t timeout] [-i myipaddr [-m myipmask [-p peeraddr]]] dialdevice telno [expect-send ...]";
delim:		string = "-";	#expect-send delimiter
baud:		string;
myipaddr:		string = "0.0.0.0";
myipmask:	string = "255.255.255.255";
peeraddr:		string = "0.0.0.0";
timeout:		string = "20";	# expect sequence timeout
dialdev:		string;	# serial device for telco
telno:		string;	# phone number to dial
conv:		list of string;	# expect-send sequence once connected
maxmtu := "512";
secret: string;

args(argl: list of string)
{
	while(argl != nil) {
		s := hd argl;
		if(s[0] != '-')
			break;
	opt:	for(i := 1; i < len s; i++) {
			case s[i] {
			'v' =>
				debug=1;
			'b' or 'd' or 'i' or 'm' or 'p' or 't' or 'k' =>
				arg := s[i+1:];
				if(arg == nil) {
					argl = tl argl;
					if(argl != nil) {
						arg = hd argl;
						if(arg[0] == '-')
							usage();
					}
					else
						usage();
				}
				case s[i] {
				'b' => baud = arg;
				'd' => delim = arg;
				'i' => myipaddr = arg;
				'm' => myipmask = arg;
				'p' => peeraddr = arg;
				't' => timeout = arg;
				'k' => secret = arg;
				}
				break opt;
			*   =>
				usage();
			}
		}
		argl = tl argl;
	}
	if (len argl < 2)
		usage();
	dialdev = hd argl;
	argl = tl argl;
	telno = hd argl;
	argl = tl argl;
	conv = argl;
	# peeradd, requires myipmask which requires myipaddr
	if (peeraddr != nil && myipmask == nil || myipmask != nil && myipaddr == nil)
		usage();
	# accept a port number too
	case dialdev[0:1] {
	"0" to "9" =>	dialdev = "#t/eia" + dialdev;
	}
}

#
# post-dial conversation session
#
session(fd: ref FD)
{
	modem = ref Modem (0, fd, array[0] of byte, chan of array of byte);
	spawn monitor(modem);
	if (matchseq(modem, conv, int timeout) == 0)
		err("connection failed");
	print("connection succeeded\n");
	if (modem.pid != 0)
		killpid(modem.pid);
	modem = nil;
}

init(nil: ref Context, argl: list of string)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	stdin = sys->fildes(0);
	str = load String String->PATH;
	if (str == nil)
		err("can't load " + String->PATH);
	telco = load Telco Telco->PATH;
	if (telco == nil)
		err("can't load " + Telco->PATH);
	if (len argl < 2) 
		usage();
	argl = tl argl;
	args(argl);
	a := dialdev :: nil;
	if (debug)
		a = "-d" :: a;
	e := telco->initr(a);
	if (e != nil)
		err(e);
	if (baud != nil)
		telno += "!"+baud;
	(ok, c) := dial("telco!"+telno, nil);
	if (ok < 0)
		err("can't dial telco!"+telno);
	print("dial succeeded\n");
	if (conv != nil)
		session(c.dfd);
	print("configure PPP...\n");
	fd := open("/net/ipifc", Sys->OWRITE);
	if (fd == nil)
		err("can't open /net/ipifc");
	dialdev = fc(c.dir + "/local");
	if (dialdev == nil)
		err("can't determine dial device");
	ifc := "bind ppp " + dialdev;
	ifc += " " + myipaddr;
	ifc += " " + peeraddr;
	ifc += " " + maxmtu;
	ifc += " 1";		# framing
	if(secret != nil)
		ifc += " " + secret;
	if (fprint(fd, "%s", ifc) < 0)
		err("can't write /net/ipifc");
	if (debug)
		print("%s -> /net/ipifc\n", ifc);
	fd = nil;
	telco->shutdown();
	print("done\n");
}

usage()
{
	fprint(stderr, "%s\n", usagestr);
	exit;
}

err(msg: string)
{
	fprint(stderr, "%s: %s\n", argv0, msg);
	if (modem != nil)
		if (modem.pid != 0)
			killpid(modem.pid);
	if (telco != nil)
		telco->shutdown();
	exit;
}

#
# Match a sequence of [expect]-[send] strings
# either part may be omitted
#
matchseq(m: ref Modem, conv: list of string, tout: int): int
{
	tend := millisec() + 1000*tout;
	while (conv != nil) {
		e, s:	string = nil;
		p := hd conv;
		conv = tl conv;
		if (len p == 0)
			continue;
		if (p[0] == '-') {	# just send
			if (len p == 1)
				continue;
			s = p[1:];
		} else {
			(n, esl) := tokenize(p, delim);
			if (n > 0) {
				e = hd esl;
				esl = tl esl;
				if (n > 1)
					s = hd esl;
			}
		}
		if (e  != nil) {
			if (match(m, special(e), tend-millisec()) == 0) {
				return 0;
			}
			#if(debug)
				#print("<-%s", e);
		}
		if (s != nil)
			send(m, special(s));
	}
	return 1;
}

dumpa(a: array of byte): string
{
	s := "";
	for(i:=0; i<len a; i++){
		b := int a[i];
		if(b >= ' ' && b < 16r7f)
			s[len s] = b;
		else
			s += sys->sprint("\\%.2x", b);
	}
	return s;
}

#
# Expand special control characters
#
special(s: string): string
{
	r: string = "";
	for(i:=0; i < len s; i++) {
		c := s[i];
		if (c == '\\'  && i+1 < len s) {
			c = s[++i];
			case c {
			't' 	=> c = '\t';
			'n'	=> c = '\n';
			'r'	=> c = '\r';
			'b'	=> c = '\b';
			'a'	=> c = '\a';
			'v'	=> c = '\v';
			'0'	=> c = '\0';
			'u'	=> 
				if (i+4 < len s) {
					i++;
					(c, nil) = str->toint(s[i:i+4], 16);
					i+=3;
				}
			};
		}
		r[len r] = c;
	}
	return r;
}

killpid(pid: int)
{
	fd := sys->open("#p/"+(string pid)+"/ctl", sys->OWRITE);
	if(fd == nil)
		return;
	sys->fprint(fd, "kill");
}

send(m: ref Modem, x: string): int
{
	#if(debug)
		#print("->%s", x);
	a := array of byte x;
	return write(m.fd, a, len a);
}

readline(fd: ref FD): string
{
	l := array [128] of byte;
	nb := read(fd, l, len l);
	if(nb <= 1)
		return "";
	return string l[0:nb-1];
}

#
#  a process to read input from a modem.
#
monitor(m: ref Modem)
{
	m.pid = sys->pctl(0, nil);
	a := array[Sys->ATOMICIO] of byte;
	while((n := read(m.fd, a, len a)) > 0){
		b := array[n] of byte;
		b[0:] = a[0:n];
		m.rd <-= b;
	}
	if(n < 0)
		print("monitor: read error: %r\n");
	else
		print("monitor: EOF\n");
	m.rd <-= nil;
}

#
#  get bytes input by monitor()
#
getinput(m: ref Modem, n: int): array of byte
{
	if(len m.avail == 0){
		alt {
		a := <-m.rd =>
			m.avail = a;
			if(a == nil)
				return nil;

		* =>
			return nil;
		}
	}
	if(n > len m.avail)
		n = len m.avail;
	b := m.avail[0:n];
	m.avail = m.avail[n:];
	return b;
}

getc(m: ref Modem, timo: int): int
{
	start := sys->millisec();
	while(len (b  := getinput(m, 1)) == 0) {
		if (timo && sys->millisec() > start+timo)
			return 0;
		sys->sleep(250);
	}
	return int b[0];
}

match(m: ref Modem, s: string, timo: int): int
{
	for(;;) {
		c := getc(m, timo);
		if(debug)
			print("%c", c);
		if (c == 0)
			return 0;
	head:
		while(c == s[0]) {
			i := 1;
			while(i < len s) {
				c = getc(m, timo);
				if(debug)
					sys->print("%c", c);
				if(c == 0)
					return 0;
				if(c != s[i])
					continue head;
				i++;
			}
			return 1;
		}
		if(c == '~')
			return 1;	# assume PPP for now
	}
}

fc(file: string): string
{
	fd := open(file, sys->OREAD);
	if(fd == nil)
		return nil;

	buf := array[32] of byte;
	n := read(fd, buf, len buf);
	if(n <= 1)
		return nil;		# we expect a newline (too)

	return string buf[0:n-1];	# ditto
}

