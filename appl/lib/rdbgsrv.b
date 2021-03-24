implement RDbgSrv;

include "sys.m";
	sys: Sys;
include "draw.m";

RDbgSrv: module
{
	init: fn(nil: ref Draw->Context, argv: list of string);
};

SlideProto: adt
{
	rfd: ref Sys->FD;
	wfd: ref Sys->FD;

	start: fn(name: string): ref SlideProto;
	read: fn(me: self ref SlideProto, buf: array of byte, nbytes: int): int;
	write: fn(me: self ref SlideProto, buf: array of byte, nbytes: int): int;

	get2: fn(me: self ref SlideProto): int;
	put2: fn(me: self ref SlideProto, n: int): int;
};


debug:=	0;
dev:=		"/dev/eia0";
speed:=	38400;

init(nil: ref Draw->Context, av: list of string)
{
	sys = load Sys Sys->PATH;
	if(sys == nil)
		return;

	arginit(av);
	while(o := opt())
		case o {
		'd' =>
			d := arg();
			if(d == nil)
				usage();
			debug = int d;
		's' =>
			s := arg();
			if(s == nil)
				usage();
			speed = int s;
		'f' =>
			s := arg();
			if(s == nil)
				usage();
			dev = s;
		'h' =>
			usage();
		}

	mtpt := arg();
	if(mtpt == nil)
		usage();

	ctl := dev + "ctl";
	cfd := sys->open(ctl, Sys->OWRITE);
	if(cfd == nil)
		sys->raise(sys->sprint("fail: open %s: %r\n", ctl));

	sys->fprint(cfd, "b%d", speed);
	sys->fprint(cfd, "l8");
	sys->fprint(cfd, "pn");
	sys->fprint(cfd, "s1");

	proto := SlideProto.start(dev);
	if(proto == nil)
		sys->raise("fail: proto start");

	fds := array[2] of ref Sys->FD;

	if(sys->pipe(fds) == -1)
		sys->raise(sys->sprint("fail: pipe: %r"));

	if(debug)
		sys->print("%s: starting server\n", progname);

	rc := chan of int;
	spawn tranceiver(fds[1], proto, rc);
	rpid := <- rc;

	if(sys->mount(fds[0], mtpt, Sys->MREPL, nil) == -1) {
		killpid(rpid);
		sys->raise(sys->sprint("fail: mount: %r"));
	}
}

usage()
{
	sys->raise("fail: usage: [-d n] [-s speed] [-f dev] mountpoint");
}

killpid(pid: int)
{
	if(pid == 0)
		return;
	fd := sys->open("/prog/"+string pid+"/ctl", sys->OWRITE);
	if(fd == nil)
		return;

	sys->write(fd, array of byte "kill", 4);
}

tranceiver(fd: ref Sys->FD, proto: ref SlideProto, pidchan: chan of int)
{
	pidchan <-= sys->pctl(0, nil);

	buf := array[Sys->ATOMICIO+64] of byte;
	e := ref Sys->Exception;
	if(sys->rescue("*", e)) {
		sys->print("%s: server: %s: exiting\n", progname, e.name);
		return;
	}

	for(;;) {
		n := sys->read(fd, buf, len buf);
		if(n == -1)
			sys->raise(sys->sprint("server: read: %r"));
		if(proto.write(buf, n) != n)
			sys->raise("server: proto.write failed");

		n = proto.read(buf, len buf);
		if(n == -1)
			sys->raise("server: proto.read failed");

		if(sys->write(fd, buf, n) != n)
			sys->raise(sys->sprint("server: write: %r"));
	}
}

#
# Arg parsing from Roger Peppe <rog@ohm.york.ac.uk>
#
progname := "";
args: list of string;
curropt: string;

arginit(argv: list of string)
{
	if(argv == nil)
		return;
	progname = hd argv;
	args = tl argv;
}

# don't allow any more options after this function is invoked
argv() : list of string
{
	ret := args;
	args = nil;
	return ret;
}

# get next option argument
arg() : string
{
	if (curropt != "") {
		ret := curropt;
		curropt = nil;
		return ret;
	}

	if (args == nil)
		return nil;

	ret := hd args;
	if (ret[0] == '-')
		ret = nil;
	else
		args = tl args;
	return ret;
}

# get next option letter
# return 0 at end of options
opt() : int
{
	if (curropt != "") {
		opt := curropt[0];
		curropt = curropt[1:];
		return opt;
	}

	if (args == nil)
		return 0;

	nextarg := hd args;
	if (nextarg[0] != '-' || len nextarg < 2)
		return 0;

	if (nextarg == "--") {
		args = tl args;
		return 0;
	}

	opt := nextarg[1];
	if (len nextarg > 2)
		curropt = nextarg[2:];
	args = tl args;
	return opt;
}

SlideProto.start(name:string): ref SlideProto
{
	p: SlideProto;

	if(sys == nil)
			return nil;

	p.rfd = sys->open(name, Sys->OREAD);
	p.wfd = sys->open(name, Sys->OWRITE);
	if(p.rfd == nil || p.wfd == nil)
			return nil;

	s := array of byte "go";

	if(sys->write(p.wfd, s, len s) != len s)
			return nil;

	c := array[1] of byte;
	state := 0;
	for(;;) {
		if(sys->read(p.rfd, c, 1) != 1)
			return nil;

		if(state == 0 && c[0] == byte 'o')
			state = 1;
		else if(state == 1 && c[0] == byte 'k')
			break;
		else
			state = 0;
	}

	return ref p;
}

SlideProto.read(me: self ref SlideProto, buf: array of byte, nbytes: int): int
{
	n := me.get2();

	b := array[Sys->ATOMICIO+16] of byte;

	if(nbytes < n) {
		r := 0;
		for(i := 0; i < n; i += r) {
			r = sys->read(me.rfd, b, n);
			if(r == -1)
				return -1;
		}
		return -1;
	}

	r := 0;
	for(i := 0; i < n; i += r) {
		r = sys->read(me.rfd, b, n);
		if(r == -1)
			return -1;
		buf[i:] = b[:r];
	}

	if(debug & 1)
		trace("read", buf);
	if(debug & 2)
		dump(sys->sprint("proto.read (%d bytes)", n), buf, n);

	return n;
}

SlideProto.write(me: self ref SlideProto, buf: array of byte, nbytes: int): int
{
	if(me.put2(nbytes) == -1)
		return -1;
	if(debug & 4) {
		i := 0;
		while(i < nbytes) {
			if(sys->write(me.wfd, buf[i++:], 1) != 1)
				return -1;
			sys->sleep(1);
		}
	} else {
		if(sys->write(me.wfd, buf, nbytes) != nbytes)
			return -1;
	}

	if(debug &1)
		trace("write", buf);
	if(debug & 2)
		dump("proto.write", buf, nbytes);
	return nbytes;
}

SlideProto.get2(me: self ref SlideProto): int
{

	buf:= array[1] of byte;

	n := sys->read(me.rfd, buf, 1);
	if(n  != 1)
		sys->raise(sys->sprint("fail: SlideProto.get2: read %d: %r",n));
	val := int buf[0];

	n = sys->read(me.rfd, buf, 1);
	if(n  != 1)
		sys->raise(sys->sprint("fail: SlideProto.get2: read %d: %r",n));
	val |= int buf[0] << 8;

	return val;
}

SlideProto.put2(me: self ref SlideProto, n: int): int
{
	buf:= array[2] of byte;

	buf[0] = byte n;
	buf[1] = byte (n>>8);

	if(sys->write(me.wfd, buf, 2) != 2)
		return -1;
	return 2;
}

trace2(buf: array of byte): int
{
	return int buf[0] | (int buf[1] << 8);
}
Tnop:		con  0;
Rnop:		con  1;
Terror:		con  2;
Rerror:		con  3;
Tflush:		con  4;
Rflush:		con  5;
Tclone:		con  6;
Rclone:		con  7;
Twalk:		con  8;
Rwalk:		con  9;
Topen:		con  10;
Ropen:		con  11;
Tcreate:		con  12;
Rcreate:		con  13;
Tread:		con  14;
Rread:		con  15;
Twrite:		con  16;
Rwrite:		con  17;
Tclunk:		con  18;
Rclunk:		con  19;
Tremove:		con  20;
Rremove:		con  21;
Tstat:			con  22;
Rstat:			con  23;
Twstat:		con  24;
Rwstat:		con  25;
Tattach:		con  28;
Rattach:		con  29;

trace(sourcept: string,  op: array of byte ) 
{
	case int op[0] {
	 Tnop =>
		sys->print("%s: Tnop tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Tflush =>
		sys->print("%s: Tflush tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Tclone =>
		sys->print("%s: Tclone tag(%d) fid(%d) newfid(%d)\n", sourcept, trace2(op[1:]), trace2(op[3:]), trace2(op[5:]));
	 Twalk =>
		sys->print("%s: Twalk tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Topen =>
		sys->print("%s: Topen tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Tcreate =>
		sys->print("%s: Tcreate tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]));
	 Tread =>
		sys->print("%s: Tread tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Twrite =>
		sys->print("%s: Twrite tag(%d) fid(%d) count(%d)\n", sourcept, trace2(op[1:]), trace2(op[3:]), trace2(op[13:]) );
	 Tclunk =>
		sys->print("%s: Tclunk tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Tremove =>
		sys->print("%s: Tremove tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]));
	 Tstat =>
		sys->print("%s: Tstat tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Twstat =>
		sys->print("%s: Twstat tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Tattach =>
		sys->print("%s: Tattach tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]));
	 Rnop =>
		sys->print("%s: Rnop tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Rflush =>
		sys->print("%s: Rflush tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Rclone =>
		sys->print("%s: Rclone tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Rwalk =>
		sys->print("%s: Rwalk tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Ropen =>
		sys->print("%s: Ropen tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Rcreate =>
		sys->print("%s: Rcreate tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]));
	 Rread =>
		sys->print("%s: Rread tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Rwrite =>
		sys->print("%s: Rwrite tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Rclunk =>
		sys->print("%s: Rclunk tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Rremove =>
		sys->print("%s: Rremove tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]));
	 Rstat =>
		sys->print("%s: Rstat tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Rwstat =>
		sys->print("%s: Rwstat tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]) );
	 Rattach =>
		sys->print("%s: Rattach tag(%d) fid(%d) \n", sourcept, trace2(op[1:]), trace2(op[3:]));
	 Rerror =>
		s := "";
		for(i:=0;i<64;i++) {
			if(op[3+i] != byte 0)
				s[len s] = int op[3+i];
			else
				break;
		}
		sys->print("%s: Rerror tag(%d) %s\n", sourcept, trace2(op[1:]), s);
	}
}

dump(msg: string, buf: array of byte, n: int)
{
	sys->print("%s: [%d bytes]: ", msg, n);
	s := "";
	for(i:=0;i<n;i++) {
		if((i % 20) == 0) {
			sys->print(" %s\n", s);
			s = "";
		}
		sys->print("%2.2x ", int buf[i]);
		if(int buf[i] >= 32 && int buf[i] < 127)
			s[len s] = int buf[i];
		else
			s += ".";
	}
	for(i %= 20; i < 20; i++)
		sys->print("   ");
	sys->print(" %s\n\n", s);
}
