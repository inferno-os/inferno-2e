implement Telnet;

include "sys.m";
	sys: Sys;
	Connection: import sys;

include "draw.m";
	draw: Draw;
	Context: import draw;

Telnet: module
{
	init:	fn(ctxt: ref Draw->Context, args: list of string);
};

Iob: adt
{
	fd:	ref Sys->FD;
	out:	cyclic ref Iob;
	buf:	array of byte;
	ptr:	int;
	nbyte:	int;
};

BS:		con 8;		# ^h backspace character
BSW:		con 23;		# ^w bacspace word
BSL:		con 21;		# ^u backspace line
EOT:		con 4;		# ^d end of file
ESC:		con 27;		# hold mode

HIWAT:	con 2000;	# maximum number of lines in transcript
LOWAT:	con 1500;	# amount to reduce to after high water

Name:	con "Telnet";
ctxt:	ref Context;
cmds:	chan of string;
net:	Connection;
stderr: ref Sys->FD;
mcrlf:	int;
netinp:	ref Iob;

# control characters
Se:		con 240;	# end subnegotiation
NOP:		con 241;
Mark:		con 242;	# data mark
Break:		con 243;
Interrupt:	con 244;
Abort:		con 245;	# TENEX ^O
AreYouThere:	con 246;
Erasechar:	con 247;	# erase last character
Eraseline:	con 248;	# erase line
GoAhead:	con 249;	# half duplex clear to send
Sb:		con 250;	# start subnegotiation
Will:		con 251;
Wont:		con 252;
Do:		con 253;
Dont:		con 254;
Iac:		con 255;

# options
Binary,	Echo,	SGA,	Stat,	Timing,
Det,	Term,	EOR,	Uid,	Outmark,
Ttyloc,	M3270,	Padx3,	Window,	Speed,
Flow,	Line,	Xloc,	Extend: con iota;

Opt: adt
{
	name:	string;
	code:	int;
	noway:	int;	
	remote:	int;		# remote value
	local:	int;		# local value
};

opt := array[] of
{
	Binary	=> Opt("binary",		0,	0,	0, 	0),
	Echo	=> Opt("echo",			1,  	0, 	0,	0),
	SGA	=> Opt("suppress Go Ahead",	3,  	0, 	0,	0),
	Stat	=> Opt("status",		5,  	1, 	0,	0),
	Timing	=> Opt("timing",		6,  	1, 	0,	0),
	Det	=> Opt("det",			20, 	1, 	0,	0),
	Term	=> Opt("terminal",		24, 	0, 	0,	0),
	EOR	=> Opt("end of record",		25, 	1, 	0,	0),
	Uid	=> Opt("uid",			26, 	1, 	0,	0),
	Outmark	=> Opt("outmark",		27, 	1, 	0,	0),
	Ttyloc	=> Opt("ttyloc",		28, 	1, 	0,	0),
	M3270	=> Opt("3270 mode",		29, 	1, 	0,	0),
	Padx3	=> Opt("pad x.3",		30, 	1, 	0,	0),
	Window	=> Opt("window size",		31, 	1, 	0,	0),
	Speed	=> Opt("speed",			32, 	1, 	0,	0),
	Flow	=> Opt("flow control",		33, 	1, 	0,	0),
	Line	=> Opt("line mode",		34, 	0, 	0,	0),
	Xloc	=> Opt("X display loc",		35, 	1, 	0,	0),
	Extend	=> Opt("Extended",		255, 	1, 	0,	0),
};

menuindex := "0";
holding := 0;

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;

	sys->pctl(Sys->NEWPGRP, nil);
	stderr = sys->fildes(2);

	argv = tl argv;
	host := hd argv;
	argv = tl argv;
	if(argv == nil)
		port := "23";
	else
		port = hd argv;
	connect(host, port);
}

isalnum(s: string): int
{
	if(s == "")
		return 0;
	c := s[0];
	if('a' <= c && c <= 'z')
		return 1;
	if('A' <= c && c <= 'Z')
		return 1;
	if('0' <= c && c <= '9')
		return 1;
	if(c == '_')
		return 1;
	if(c > 16rA0)
		return 1;
	return 0;
}

kill()
{
	path := sys->sprint("#p/%d/ctl", sys->pctl(0, nil));
	fd := sys->open(path, sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "killgrp");
}

ccfd: ref Sys->FD;

raw(on: int)
{
	if(ccfd == nil) {
		ccfd = sys->open("/dev/consctl", Sys->OWRITE);
		if(ccfd == nil) {
			sys->print("/dev/consctl: %r\n");
			return;
		}
	}
	if(on)
		sys->fprint(ccfd, "rawon");
	else
		sys->fprint(ccfd, "rawoff");
}

connect(host: string, port: string)
{
	addr := sys->sprint("tcp!%s!%s", host, port);
	sys->print("addr=%s\n", addr);
	ok: int;
	(ok, net) = sys->dial(addr, nil);
	if(ok < 0) {
		sys->print("Connection to host failed: %r");
		return;
	}

	spawn fromnet();

	b := array[1024] of byte;
	raw(1);
	while((n := sys->read(sys->fildes(0), b, len b)) >= 0) {
		sys->print("<n=%d>", n);
		sys->write(net.dfd, b, n);
	}
}

iobnew(fd: ref Sys->FD, out: ref Iob, size: int): ref Iob
{
	iob := ref Iob;
	iob.fd = fd;
	iob.out = out;
	iob.buf = array[size] of byte;
	iob.nbyte = 0;
	iob.ptr = 0;
	return iob;
}

iobget(iob: ref Iob): int
{
	if(iob.nbyte == 0) {
		if(iob.out != nil)
			iobflush(iob.out);
		iob.nbyte = sys->read(iob.fd, iob.buf, len iob.buf);
		if(iob.nbyte <= 0)
			return -1;
		iob.ptr = 0;
	}
	iob.nbyte--;
	return int iob.buf[iob.ptr++];
}

iobput(iob: ref Iob, c: int)
{
	iob.buf[iob.ptr++] = byte c;
	if(iob.ptr == len iob.buf)
		iobflush(iob);
}

iobflush(iob: ref Iob)
{
	if(iob.fd == nil) {
		sys->write(sys->fildes(1), iob.buf, iob.ptr);
		iob.ptr = 0;
	}
}

fromnet()
{
	conout := iobnew(nil, nil, 2048);
	netinp = iobnew(net.dfd, conout, 2048);

	crnls := 0;
	freenl := 0;

loop:	for(;;) {
		c := iobget(netinp);	
		case c {
		-1 =>
			cmds <-= "dis";
			return;
		'\n' =>				# skip nl after string of cr's */
			if(!opt[Binary].local && !mcrlf) {
				crnls++;
				if(freenl == 0)
					break;
				freenl = 0;
				continue loop;
			}
		'\r' =>
			if(!opt[Binary].local && !mcrlf) {
				if(crnls++ == 0){
					freenl = 1;
					c = '\n';
					break;
				}
				continue loop;
			}
		Iac  =>
			c = iobget(netinp);
			if(c == Iac)
				break;
			iobflush(conout);
			if(control(netinp, c) < 0)
				return;

			continue loop;	
		}
		iobput(conout, c);
	}
}

control(bp: ref Iob, c: int): int
{
	case c {
	AreYouThere =>
		sys->fprint(net.dfd, "Inferno telnet V1.0\r\n");
	Sb =>
		return sub(bp);
	Will =>
		return will(bp);
	Wont =>
		return wont(bp);
	Do =>
		return doit(bp);
	Dont =>
		return dont(bp);
	Se =>
		sys->fprint(stderr, "telnet: SE without an SB\n");
	-1 =>
		return -1;
	*  =>
		break;
	}
	return 0;
}

sub(bp: ref Iob): int
{
	subneg: string;
	i := 0;
	for(;;){
		c := iobget(bp);
		if(c == Iac) {
			c = iobget(bp);
			if(c == Se)
				break;
			subneg[i++] = Iac;
		}
		if(c < 0)
			return -1;
		subneg[i++] = c;
	}
	if(i == 0)
		return 0;

#	sys->fprint(stderr, "sub %d %d n = %d\n", subneg[0], subneg[1], i);

	for(i = 0; i < len opt; i++)
		if(opt[i].code == subneg[0])
			break;

	if(i >= len opt)
		return 0;

	case i {
	Term =>
		sbsend(opt[Term].code, array of byte "vt100");	
	}

	return 0;
}

sbsend(code: int, data: array of byte): int
{
	buf := array[4+len data+2] of byte;
	o := 4+len data;

	buf[0] = byte Iac;
	buf[1] = byte Sb;
	buf[2] = byte code;
	buf[3] = byte 0;
	buf[4:] = data;
	buf[o] = byte Iac;
	o++;
	buf[o] = byte Se;

	return sys->write(net.dfd, buf, len buf);
}

will(bp: ref Iob): int
{
	c := iobget(bp);
	if(c < 0)
		return -1;

#	sys->fprint(stderr, "will %d\n", c);

	for(i := 0; i < len opt; i++)
		if(opt[i].code == c)
			break;

	if(i >= len opt) {
		send3(bp, Iac, Dont, c);
		return 0;
	}

	rv := 0;
	if(opt[i].noway)
		send3(bp, Iac, Dont, c);
	else
	if(opt[i].remote == 0)
		rv |= send3(bp, Iac, Do, c);

	if(opt[i].remote == 0)
		rv |= change(bp, i, Will);
	opt[i].remote = 1;
	return rv;
}

wont(bp: ref Iob): int
{
	c := iobget(bp);
	if(c < 0)
		return -1;

#	sys->fprint(stderr, "wont %d\n", c);

	for(i := 0; i < len opt; i++)
		if(opt[i].code == c)
			break;

	if(i >= len opt)
		return 0;

	rv := 0;
	if(opt[i].remote) {
		rv |= change(bp, i, Wont);
		rv |= send3(bp, Iac, Dont, c);
	}
	opt[i].remote = 0;
	return rv;
}

doit(bp: ref Iob): int
{
	c := iobget(bp);
	if(c < 0)
		return -1;

#	sys->fprint(stderr, "do %d\n", c);

	for(i := 0; i < len opt; i++)
		if(opt[i].code == c)
			break;

	if(i >= len opt || opt[i].noway) {
		send3(bp, Iac, Wont, c);
		return 0;
	}
	rv := 0;
	if(opt[i].local == 0) {
		rv |= change(bp, i, Do);
		rv |= send3(bp, Iac, Will, c);
	}
	opt[i].local = 1;
	return rv;
}

dont(bp: ref Iob): int
{
	c := iobget(bp);
	if(c < 0)
		return -1;

#	sys->fprint(stderr, "dont %d\n", c);

	for(i := 0; i < len opt; i++)
		if(opt[i].code == c)
			break;

	if(i >= len opt || opt[i].noway)
		return 0;

	rv := 0;
	if(opt[i].local){
		opt[i].local = 0;
		rv |= change(bp, i, Dont);
		rv |= send3(bp, Iac, Wont, c);
	}
	opt[i].local = 0;
	return rv;
}

change(bp: ref Iob, o: int, what: int): int
{
	return 0;
}

send3(bp: ref Iob, c0: int, c1: int, c2: int): int
{
	buf := array[3] of byte;

	buf[0] = byte c0;
	buf[1] = byte c1;
	buf[2] = byte c2;

	t: string;
	case c1 {
	Will => t = "Will";
	Wont => t = "Wont";
	Do =>	t = "Do";
	Dont => t = "Dont";
	}
#	if(t != nil)
#		sys->fprint(stderr, "r %s %d\n", t, c2);

	r := sys->write(bp.fd, buf, 3);
	if(r != 3)
		return -1;
	return 0;
}

