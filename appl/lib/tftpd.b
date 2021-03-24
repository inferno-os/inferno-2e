implement Tftpd;

include "sys.m";
	sys: Sys;
	stderr: ref Sys->FD;

include "draw.m";

Tftpd: module
{
	init: fn (nil: ref Draw->Context, argv: list of string);
};

dir:=  "/services/tftpd";
net:=  "/net";

Tftp_READ: con 1;
Tftp_WRITE: con 2;
Tftp_DATA: con 3;
Tftp_ACK: con 4;
Tftp_ERROR: con 5;

Segsize: con 512;

dbg := 0;
restricted := 0;
port := 69;

tftpcon: Sys->Connection;
tftpreq: ref Sys->FD;

dokill(pid: int)
{
	fd := sys->open("/prog/" + string pid + "/ctl", sys->OWRITE);
	if(fd == nil)
		fd = sys->open("#p/" + string pid + "/ctl", sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "kill");
}

kill(pid: int) { dokill(pid); }
killgrp(pid: int) { dokill(pid); }
killme() { kill(sys->pctl(0,nil)); }
killus() { killgrp(sys->pctl(0,nil)); }

DBG(s: string)
{
	if(dbg)
		sys->fprint(stderr, "DEBUG: %d: %s\n", sys->pctl(0,nil), s);
}

bool: type int;
true: con 1;
false: con 0;

Timer: adt {
	KILL: con -1;
	ALARM: con -2;
	RETRY: con -3;
	sig: chan of int;
	create: fn(): ref Timer;
	destroy: fn(t: self ref Timer);
	set: fn(t: self ref Timer, msec, nretry: int);

	ticker: fn(t: self ref Timer);
	ticking: bool;
	wakeup: int;
	timeout: int;
	nretry: int;
};

Timer.create(): ref Timer
{
	t := ref Timer;
	t.wakeup = 0;
	t.ticking = false;
	t.sig = chan of int;
	return t;
}

Timer.destroy(t: self ref Timer)
{
	DBG("Timer.destroy");
	alt {
		t.sig <-= t.KILL =>
			DBG("sent final msg");
		* =>
			DBG("couldn't send final msg");
	}
	DBG("Timer.destroy done");
}

Timer.ticker(t: self ref Timer)
{
	DBG("spawn: ticker");
	t.ticking = true;
	while(t.wakeup > sys->millisec()) {
		DBG("Timer.ticker sleeping for "
			+string (t.wakeup-sys->millisec()));
		sys->sleep(t.wakeup-sys->millisec());
	}
	if(t.wakeup) {
		DBG("Timer.ticker wakeup");
		if(t.nretry) {
			alt { t.sig <-= t.RETRY => ; }
			t.ticking = false;
			t.set(t.timeout, t.nretry-1);
		} else
			alt { t.sig <-= t.ALARM => ; }
	}	
	t.ticking = false;
	DBG("unspawn: ticker");
}

Timer.set(t: self ref Timer, msec, nretry: int)
{
	DBG(sys->sprint("Timer.set(%d, %d)", msec, nretry));
	if(msec == 0) {
		t.wakeup = 0;
		t.timeout = 0;
		t.nretry = 0;
	} else {
		t.wakeup = sys->millisec()+msec;
		t.timeout = msec;
		t.nretry = nretry;
		if(!t.ticking)
			spawn t.ticker();
	}
}

killer(c: chan of int, pgid: int)
{
	DBG("spawn: killer");
	cmd := <- c;
	DBG(sys->sprint("killer has awakened (flag=%d)", cmd));
	if(cmd == Timer.ALARM) {
		killgrp(pgid);
		DBG(sys->sprint("group %d has been killed", pgid));
	}
	DBG("unspawn killer");
}

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	if(argv != nil)
		argv = tl argv;

	for(; argv != nil; argv = tl argv) {
		case hd argv {
			"-d" => dbg++;
			"-h" => argv = tl argv;
				if(argv != nil)
					dir = hd argv;
				else {
					sys->fprint(stderr,
						"missing homedir\n");
					return;
				}
			"-r" => restricted = 1;
			"-p" => argv = tl argv;
				if(argv != nil)
					port = int hd argv;
				else {
					sys->fprint(stderr,
						"missing port\n");
					return;
				}
			* =>
				if((hd argv)[0] == '-') {
					sys->fprint(stderr, "usage: tftpd "
						+"[-dr] [-h homedir] "
						+"[network-dir]\n");
					return;
				} else
					net = hd argv;
		}	
	}

	if(sys->chdir(dir) < 0)
		fatal("can't get to directory " + dir);

	spawn mainthing();
}

mainthing()
{
	DBG("spawn: mainthing");
	bigbuf := array[32768] of byte;
	
	openlisten();
	setuser();
	for(;;) {
		dlen := sys->read(tftpreq, bigbuf, len bigbuf);
		if(dlen < 0)
			fatal("listen");
		if(dlen < 14)
			continue;

		raddr := net + "/udp!"
			+ string bigbuf[0] + "."
			+ string bigbuf[1] + "."
			+ string bigbuf[2] + "."
			+ string bigbuf[3] 
			+ "!"
			+ string ((int bigbuf[8]<<8) | int bigbuf[9]);

		DBG(sys->sprint("raddr=%s\n", raddr));
		(err, cx) := sys->dial(raddr, nil);
		if(err < 0)
			fatal("dialing "+raddr);

#		showbuf("bigbuf", bigbuf[0:dlen]);

		op := (int bigbuf[12]<<8) | int bigbuf[13];
		mbuf := bigbuf[14:dlen];		# get past Udphdr and op
		dlen -= 14;

		case op {
		Tftp_READ or Tftp_WRITE =>
			;
		Tftp_ERROR =>
			DBG("tftp error");
			continue;
		* =>
			nak(cx.dfd, 4, "Illegal TFTP operation");
			continue;
		}

#		showbuf("mbuf", mbuf[0:dlen]);

		i := 0;
		while(dlen > 0 && mbuf[i] != byte 0) {
			dlen--;
			i++;
		}

		p := i++;
		dlen--;
		while(dlen > 0 && mbuf[i] != byte 0) {
			dlen--;
			i++;
		}

		path := string mbuf[0:p];
		mode := string mbuf[p+1:i];
		DBG(sys->sprint("path = %s, mode = %s\n", path, mode));

		if(dlen == 0) {
			nak(cx.dfd, 0, "bad tftpmode");
			continue;
		}

		if(restricted){
			if(path[0:3] == "../" || path[0:4] == "/../"
			   		|| path[0:1] == "/"
					&& path[0:len dir+1] == dir+"/") {
				nak(cx.dfd, 4, "Permission denied");
				continue;
			}
		}
		
		if(op == Tftp_READ)
			spawn sendfile(cx.dfd, path, mode);
		else
			spawn recvfile(cx.dfd, path, mode);
	}	
}

showbuf(msg: string, b: array of byte)
{
	sys->fprint(stderr, "%s: size %d: ", msg, len b);
	for(i:=0; i<len b; i++)
		sys->fprint(stderr, "%x ", int b[i]);
	sys->fprint(stderr, "\n");
	for(i=0; i<len b; i++)
		if(int b[i] >= 32 && int b[i] <= 126) 
			sys->fprint(stderr, "%c", int b[i]);
		else
			sys->fprint(stderr, ".");
	sys->fprint(stderr, "\n");
}

sendblock(sig: chan of int, buf: array of byte,
		net: ref sys->FD, ksig: chan of int)
{
	DBG("spawn: sendblocks");
	nbytes := 0;
	loop: for(;;) {
		DBG("sendblock: waiting for cmd");
		cmd := <- sig;
		DBG(sys->sprint("sendblock: cmd=%d\n", cmd));
		case(cmd) {
			Timer.KILL =>
				DBG("sendblock: killed");
				return;
			Timer.RETRY =>
				;
			Timer.ALARM =>
				DBG("too many retries");
				break loop;
			* =>
				nbytes = cmd;
		}
#		showbuf("sendblock", buf[0:nbytes]);
		ret := sys->write(net, buf, 4+nbytes);
		DBG(sys->sprint("ret=%d", ret));

		if(ret < 0) {
			ksig <-= Timer.ALARM;
			fatal("tftp: network write error");
		}
		if(ret != 4+nbytes)
			return;
	}
	DBG("sendblock: exiting");
	alt { ksig <-= Timer.ALARM => ; }
	DBG("unspawn: sendblocks");
}

sendfile(net: ref sys->FD, name: string, mode: string)
{

	DBG(sys->sprint("spawn: sendfile: name=%s mode=%s\n", name, mode));

	pgrp := sys->pctl(Sys->NEWPGRP, nil);
	ack := array[1024] of byte;
	if(name == "") {
		nak(net, 0, "not in our database");
		return;
	}

	file := sys->open(name, Sys->OREAD);
	if(file == nil) {
		DBG(sys->sprint("open failed: %s\n", name));
		errbuf := sys->sprint("%r");
		nak(net, 0, errbuf);
		return;
	}
	DBG(sys->sprint("opened %s\n", name));

	block := 0;
	timer := Timer.create();
	ksig := chan of int;
	buf := array[4+Segsize] of byte;

	spawn killer(ksig, pgrp);
	spawn sendblock(timer.sig, buf, net, ksig);

	mainloop: for(;;) {
		block++;
		buf[0:] = array[] of {byte 0, byte Tftp_DATA,
				byte (block>>8), byte block};
		n := sys->read(file, buf[4:], len buf-4);
		DBG(sys->sprint("n=%d\n", n));
		if(n < 0) {
			errbuf := sys->sprint("%r");
			nak(net, 0, errbuf);
			break;
		}
		DBG(sys->sprint("signalling write of %d to block %d", n, block));
		timer.sig <-= n;
		for(rxl := 0; rxl < 10; rxl++) {
			
			timer.set(1000, 15);
			al := sys->read(net, ack, len ack);
			timer.set(0, 0);
			if(al < 0) {
				timer.sig <-= Timer.ALARM;
				break;
			}
			op := (int ack[0]<<8) | int ack[1];
			if(op == Tftp_ERROR)
				break mainloop;
			ackblock := (int ack[2]<<8) | int ack[3];
			DBG(sys->sprint("got ack: block=%d ackblock=%d\n",
				block, ackblock));
			if(ackblock == block)
				break;
			if(ackblock == 16rffff) {
				block--;
				break;
			}
		}
		if(n < len buf-4)
			break;
	}
	timer.destroy();
	ksig <-= Timer.KILL;
}

recvfile(fd: ref sys->FD, name: string, mode: string)
{
	DBG(sys->sprint("spawn: recvfile: name=%s mode=%s\n", name, mode));

	pgrp := sys->pctl(Sys->NEWPGRP, nil);

	file := sys->create(name, sys->OWRITE, 8r666);
	if(file == nil) {
		errbuf := sys->sprint("%r");
		nak(fd, 0, errbuf);
		return;
	}

	block := 0;
	ack(fd, block);
	block++;

	buf := array[8+Segsize] of byte;
	timer := Timer.create();
	spawn killer(timer.sig, pgrp);

	for(;;) {
		timer.set(15000, 0);
		DBG(sys->sprint("reading block %d", block));
		n := sys->read(fd, buf, len buf);
		DBG(sys->sprint("read %d bytes", n));
		timer.set(0, 0);

		if(n < 0)
			break;
		op := int buf[0]<<8 | int buf[1];
		if(op == Tftp_ERROR)
			break;

#		showbuf("got", buf[0:n]);
		n -= 4;
		inblock := int buf[2]<<8 | int buf[3];
#		showbuf("hdr", buf[0:4]);
		if(op == Tftp_DATA) {
			if(inblock == block) {
				ret := sys->write(file, buf[4:], n);
				if(ret < 0) {
					errbuf := sys->sprint("%r");
					nak(fd, 0, errbuf);
					break;
				}
				block++;
			}
			if(inblock < block) {
				ack(fd, inblock);
				DBG(sys->sprint("ok: inblock=%d block=%d",
					inblock, block));
			} else
				DBG(sys->sprint("FAIL: inblock=%d block=%d",
					inblock, block));
			ack(fd, 16rffff);
			if(n < 512)
				break;
		}
	}
	timer.destroy();
}

ack(fd: ref Sys->FD, block: int)
{
	buf := array[] of {byte 0, byte Tftp_ACK, byte (block>>8), byte block};
#	showbuf("ack", buf);
	if(sys->write(fd, buf, 4) < 0)
		fatal("write ack");
}


nak(fd: ref Sys->FD, code: int, msg: string)
{
sys->print("nak: %s\n", msg);
	buf := array[128] of {byte 0, byte Tftp_ERROR, byte 0, byte code};
	bmsg := array of byte msg;
	buf[4:] = bmsg;
	buf[4+len bmsg] = byte 0;
	if(sys->write(fd, buf, 4+len bmsg+1) < 0)
		fatal("write nak");
}

fatal(msg: string)
{
	sys->fprint(stderr, "tftpd: %s: %r\n", msg);
	killus();
}

openlisten()
{
	name := net+"/udp!*!" + string port;
	err := 0;
	(err, tftpcon) = sys->announce(name);
	if(err < 0)
		fatal("can't announce "+name);
	if(sys->fprint(tftpcon.cfd, "headers4") < 0)
		fatal("can't set header mode");

	tftpreq = sys->open(tftpcon.dir+"/data", sys->ORDWR);
	if(tftpreq == nil)
		fatal("open udp data");
}

setuser()
{
	f := sys->open("/dev/user", sys->OWRITE);
	if(f == nil)
		return;
	sys->fprint(f, "none");
}

