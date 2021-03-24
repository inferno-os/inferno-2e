implement StyxServer;

include "sys.m";
	sys: Sys;
	FD, FileIO, Connection, Rread, Rwrite:	import Sys;
	CHDIR, OREAD, OWRITE, ORDWR:		import Sys;
	ATOMICIO, Qid, Dir:					import Sys;
include "draw.m";
	Context: import Draw;
include "daytime.m";
	daytime: Daytime;

include "styx.m";
	STATSZ:	import Styx;
include "dirmod.m";
	dirmod: Dirmod;
	convD2M, convM2D:				import dirmod;

Emesgmismatch:	con "message size mismatch";
Eperm:			con "permission denied";
Enotdir:			con "not a directory";
Enotexist:			con "file does not exist";
Eio:				con "i/o error";
Enoauth:			con "authentication not implimented";
Eisopen:			con "file already open for I/O";
Enodev:			con "no free modems";
Enostream:		con "stream closed prematurely";

Ndev:			con 8;	# maximum number of devices
Devmask:			con (Ndev-1)<<8;

Fid: adt {
	qid:		Qid;
	busy:	int;
	open:	int;
	fid:		int;
	user:		string;
};

Request: adt {
	fid:		ref Fid;
	tag:		int;
	count:	int;
};

Dev: adt {
	lock:		ref Lock;
	# device state
	ctl:		ref Sys->FD;
	data:		ref Sys->FD;
	local:	string;
	remote:	string;
	status:	string;
	speed:	int;
	t:		ref Modem;

	# fs emulation
	open:	int;
	perm:	int;
#	name:	string;
	user:		string;
	r:		array of Request;

	# input reader
	rd:		chan of array of byte;
	avail:	array of byte;
	pid:		int;
};

Modem: adt {
	name: string;
	ident: string;		# inquire request
	response: string;	# inquire response (contains)
	basetype: string;	# name of base type

	init: string;		# default initialisation string
	errorcorrection: string;
	compression: string;
	flowctl: string;		# CTS/RTS
	rateadjust: string;	# follow line speed
	mnponly: string;	# MNP compression only
};

Lock: adt {
	q: chan of int;
	lock:	fn(nil: self ref Lock);
	unlock: fn(nil: self ref Lock);

	init:	fn(nil: self ref Lock);
	end:	fn(nil: self ref Lock);
};

Qlvl1, Qlvl2, Qclone, Qlvl3, Qdata, Qctl, Qlocal, Qremote, Qstatus:	con iota;
Pexec, Pwrite, Pread:					con 1<<iota;
Pother, Pgroup, Powner:				con 1<<(3*iota);
names:=	array [] of {
	Qlvl1	=>	"/",
	Qlvl2	=>	"telco",
	Qclone	=>	"clone",
	Qlvl3	=>	"",
	Qdata	=>	"data",
	Qctl		=>	"ctl",
	Qlocal	=> 	"local",
	Qremote	=>	"remote",
	Qstatus	=>	"status",
};

debug:		int = 0;
stderr:		ref FD;
mcon, scon:	ref FileIO;		# mount() and telco server
serverfd:		ref FD;		# scon client-side
mountfd:		ref FD;		# mcon client-side
mountfile:		string;		# for unmount
ndev:		int;			# number of devices to serve
dev:			array of ref Dev;	# serial devices being served
fids:			list of ref Fid;		# 
tbuff:=		array [Styx->MAXMSG] of byte;	# devgen(), stat() workspace
user:			string;		# telco device owner
mounted		:= 0;			# set if we created /net/telco

#
#	Set up FDs for service.
#

channels() : string
{
	n0 := "telco.mcon";
	n1 := "telco.scon";
#	sys->bind("#s", "/chan", sys->MREPL);
#	mcon = sys->file2chan("/chan", n0, sys->MBEFORE);
#	scon = sys->file2chan("/chan", n1, sys->MBEFORE);
	sys->remove("/chan/telco.mcon");
	sys->remove("/chan/telco.scon");
	mcon = sys->file2chan("/chan", n0);
	scon = sys->file2chan("/chan", n1);
	if (mcon == nil || scon == nil)
		return sys->sprint("file2chan failed: %r");
	n0 = "/chan/" + n0;
	n1 = "/chan/" + n1;
	mountfile = n0;
	mountfd = sys->open(n0, sys->ORDWR);
	serverfd = sys->open(n1, sys->ORDWR);
	if (mountfd == nil || serverfd == nil)
		return sys->sprint("chan open failed: %r");
	reply->init(serverfd);
	serverfd = nil;
	return nil;
}

#
#	Serve mount point with the telco server
#
Mslave()
{
	rc:	Rread;
	wc:	Rwrite;
	b:	array of byte;
	c:	int;
	for (;;) {
		(nil, b, nil, wc) = <- mcon.write;
		(nil, c, nil, rc) = <- scon.read;
		if (c < len b || wc == nil || rc == nil)
			break;
		wc <- = (len b, nil);
		rc <- = (b, nil);
	}
	if (rc != nil)
		rc <- = (nil, Emesgmismatch);
	if (wc != nil)
		wc <- = (0, Emesgmismatch);
}

#
#	Serve telco replies to mount point
#
Tslave()
{
	rc:	Rread;
	wc:	Rwrite;
	b:	array of byte;
	c:	int;
	for (;;) {
		(nil, b, nil, wc) = <- scon.write;
		if (wc == nil) {
			rc = nil;	# normal exit, mountfd is already closed
			break;
		}
		(nil, c, nil, rc) = <- mcon.read;

		if (c < len b || wc == nil || rc == nil)
			break;
		wc <- = (len b, nil);
		rc <- = (b, nil);
	}
	if (rc != nil)
		rc <- = (nil, Emesgmismatch);
	if (wc != nil)
		wc <- = (0, Emesgmismatch);
}

#
#	Mount server.  Must be spawned because it does
#	an attach transaction.
#
mount()
{
	rv := sys->mount(mountfd, "/net" , sys->MAFTER, nil);
	mountfd = nil;
	if (rv < 0) {
		sys->print("mount failed: %r\n");
		shutdown();
	}
	mounted = 1; # /net/telco is ours, so shutdown() should unmount
}

fields(n: int, l: list of string) : array of string
{
	a := array[n] of string;
	for (i := 0; i < n; i++) {
		a[i] = hd l;
		l = tl l;
	}
	return a;
}

args(l: list of string) : (int, list of string)
{
	n := len l;
	while (l != nil) {
		s := hd l;
		if (s[0] != '-')
			break;
		for(i := 1; i < len s; i++) {
			case s[i] {
			'd' =>
				debug++;
			}
		}
		n--;
		l = tl l;
	}
	return (n, l);
}

#
#	Entry point.
#	Argument `a' contains  serial devices to service: devpath1 devpath2 ...
#
aDev:	Dev;
init(a: list of string, r: StyxReply) : string
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	daytime = load Daytime Daytime->PATH;
	if (daytime == nil)
		return sys->sprint("load Daytime failed: %r");
	dirmod = load Dirmod Dirmod->PATH;
	if (dirmod == nil)
		return sys->sprint("load Dirmod failed: %r");
	reply = r;
	(ok, nil) := sys->stat("/net/telco");
	if(ok >= 0)
		return "telcofs: already started";
	user = getuser();
	e := channels();
	if (e != nil)
		return e;
	spawn Tslave();
	spawn Mslave();
	(n, l) := args(a);
	ndev = n;
	paths := fields(n, l);
	dev = array[ndev] of ref Dev;
	for(i:=0; i<ndev; i++) {
		d := ref aDev;
		d.lock = ref Lock;
		d.lock.init();
		d.local = paths[i];
		d.r = array [0] of Request;
		dev[i] = d;
		spawn monitor(d);
		d.open++;
		onhook(d);
		d.open--;
	}
	spawn mount();
	return nil;
}

#
#	Shutdown.  Must be spawned because it results in Styx transactions.
#
shutdown()
{
	done := chan of int; 
	spawn shutd(done);
	<-done;
}

shutd(done: chan of int)
{
	if (mounted)
		sys->unmount(mountfile, "/net");
	for (i:=0; i<ndev; i++) {
		d := dev[i];
		killpid(d.pid);
		d.data = nil;
		d.ctl = nil;
		d.lock.end();
	}
	done <-= 0;
}

#
#	Modem control
#

maxspeed: con 38400;

typetab: array of Modem = array[] of {
 (	"USR Sportster",		"ATI3",	"USRobotics Sportster",	"",
#	"AT&C1&D2",
	"AT&C1&D0S37=9",
	"AT&M4",	# Normal/ARQ
	"AT&K1&B1",	# negotiate for compression, don't change port baud rate
	"AT&H1",	# CTS/RTS flow control
# 	"AT&B0",	# variable port baud rate, follows connection rate
 	"ATS37=9",	# variable port baud rate, follows connection rate
	""
 ),

 (
	"General Magic SoftModem",  "ATI3", "SoftModem", "",
	"AT&F0SS6=10&D2\\N3",	# &F0 		factory defaults
				# S6=10		long dialtone wait
				# &D2		drop DTR means hang up
	"AT\\N3",		# \N3		auto reliable mode
	"",			# default is (v.42bis) (%C3)
	"",			# default is CTS/RTS (&K3)
	"",			# default is automode
	""			# mnponly -- nope
 ),

 (	"Acer/AT&T",	"ATI3",	"AT&T 28.8 Data/14.4 Fax",	"",
	"AT&C1&D2S7=200",
	"AT\\N5",	# auto reliable (V.42, fall back to MNP, to none)
	"AT%C3",	# negotiate for compression
	"AT&K3",	# CTS/RTS flow control
 	"",
	""
 ),

 (	"Rockwell",		"",	"",	"",
	"AT&C1&D2",
	"AT\\N7",	# auto reliable (V.42, fall back to MNP, to none)
	"AT%C1\\J0",	# negotiate for compression, don't change port baud rate
	"AT\\Q3",	# CTS/RTS flow control
 	"AT\\J1",
	""
 ),

 (	"ATT14400",	"ATI9",	"E14400",	"Rockwell",
	"",
	"",
	"",	
	"",
	"",
	""
 ),

 (	"MT1432",	"ATI2",	"MT1432",	nil,
	"",
	"AT&E1",	# auto reliable (V.42, fall back to none)
	"AT&E15$BA0",	# negotiate for compression
	"AT&E4",	# CTS/RTS flow control
	"AT$BA1",	# don't change port baud rate
	""
 ),

 (	"MT2834",	"ATI2",	"MT2834",	"MT1432",
	"",
	"",
	"",
	"",
	"",
	""
 ),

 (	"VOCAL",	"ATI6",	"144DPL+FAX",	"Rockwell",
	"",
	"AT\\N3",	# auto reliable (V.42, fall back to MNP, fall back to none)
	"AT%C3\\J0",	# negotiate for compression, don't change port baud rate */	
	"",
	"",
	""
 ),
 (	"Zoom",	"AT+FMDL?",	"AC/V34",	"Rockwell",
	"AT+FCLASS=0\rAT&C1&D2",
	"ATN1",	# auto reliable (V.42, fall back to MNP, fall back to none)
	"AT%C3&Q5",	# negotiate for compression, negotiate for error-correction
	"AT&K3",	# CTS/RTS flow control
	"AT&Q5",	# asynch. operation in error-correcting mode
	"AT%C1&Q5"	# negotiate for MNP compression only, negotiate for error-correction
 ),
};

# modem return codes
Ok, Success, Failure, Noise, Found: con iota;

#
#  modem return messages
#
Msg: adt {
	text: string;
	code: int;
};

msgs: array of Msg = array [] of {
	("OK", Ok),
	("NO CARRIER", Failure),
	("ERROR", Failure),
	("NO DIALTONE", Failure),
	("BUSY", Failure),
	("NO ANSWER", Failure),
	("CONNECT", Success),
};

send(d: ref Dev, x: string): int
{
	if(debug)
		sys->print("->%s", x);
	a := array of byte x;
	return sys->write(d.data, a, len a);
}

#
#  apply a string of commands to modem
#
apply(d: ref Dev, s: string, substr: string, secs: int): int
{
	m := Ok;
	buf := "";
	for(i := 0; i < len s; i++){
		c := s[i];
		buf[len buf] = c;	# assume no Unicode
		if(c == '\r' || i == (len s -1)){
			if(c != '\r')
				buf[len buf] = '\r';
			if(send(d, buf) < 0)
				return Failure;
			(m, nil) = readmsg(d, secs, substr);
			buf = "";
		}
	}
	return m;
}

#
#  get modem into command mode if it isn't already
#
attention(d: ref Dev): int
{
	for(i := 0; i < 2; i++){
		sys->sleep(250);
		if(send(d, "+") < 0)
			continue;
		sys->sleep(250);
		if(send(d, "+") < 0)
			continue;
		sys->sleep(250);
		if(send(d, "+") < 0)
			continue;
		sys->sleep(250);
		readmsg(d, 0, nil);
		if(apply(d, "ATZH0", nil, 2) == Ok)
			return Ok;
	}
	return Failure;
}

portspeed := array [] of { 57600, 38400, 19200, 14400, 9600, 4800, 2400, 1200, 600, 300};

#
# get the modem's type and speed
#
modemtype(d: ref Dev, limit: int): string
{
	d.t = nil;

	# assume we're at a good speed, try getting attention a few times
	attention(d);

	# find a common port rate
	for(i := 0;; i++){
		if(i >= len portspeed)
			return "can't get modem's attention";
		if(portspeed[i] > limit)
			continue;
		setspeed(d, portspeed[i]);
		if(attention(d) == Ok)
			break;
	}
	d.speed = portspeed[i];
	if (debug)
		sys->print("port speed %d\n", d.speed);

	#
	#  basic Hayes commands everyone implements (we hope)
	#	Q0 = report result codes
	# 	V1 = full word result codes
	#	E0 = don't echo commands
	#	M1 = speaker on until on-line
	#	S0=0 = autoanswer off
	#
	if(apply(d, "ATQ0V1E0M1S0=0", nil, 2) != Ok)
		return "no response to initial AT commands";

	# find modem type
	for(i = 0; i < len typetab; i++){
		t := typetab[i:];
		if(t[0].ident == nil || t[0].response == nil)
			continue;
		if(apply(d, t[0].ident, t[0].response, 2) == Found){
			d.t = ref t[0];
                        sys->print("Modem type: %s\n", t[0].name);
			break;
		}
		readmsg(d, 0, nil);
	}
	readmsg(d, 0, nil);
	if(d.t == nil) {
                sys->print("Modem type: %s\n", typetab[0].name);
		d.t = ref typetab[0];	# default
        }
	if(d.t.basetype != nil){
		for(bt := 0; bt < len typetab; bt++){
			t := typetab[bt:];
			if(t[0].name == d.t.basetype){
				if(d.t.init == nil)
					d.t.init = t[0].init;
				if(d.t.errorcorrection == nil)
					d.t.errorcorrection = t[0].errorcorrection;
				if(d.t.compression == nil)
					d.t.compression = t[0].compression;
				if(d.t.flowctl == nil)
					d.t.flowctl = t[0].flowctl;
				if(d.t.rateadjust == nil)
					d.t.rateadjust = t[0].rateadjust;
				if(d.t.mnponly == nil)
					d.t.mnponly = t[0].mnponly;
				break;
			}
		}
	}
	if(debug)
		sys->print("modem %s\n", d.t.name);
	applyspecial(d, d.t.init);
	return nil;
}

#
#  dial a number
#
dialout(d: ref Dev, number: string): string
{
	mnponly := 0;
	compress := Ok;
	rateadjust := Failure;
	speed := maxspeed;

	(m, fields) := sys->tokenize(number, "!");
	number = hd fields;
	while((fields = tl fields) != nil){
		field := hd fields;
		if(field[0] >= '0' && field[0] <= '9')
			speed = int field;
		else if(field == "nocompress")
			compress = Failure;
		else if(field == "debug")
			debug = 1;
		else if(field == "mnp")
			mnponly = 1;
	}
	
	err := modemtype(d, speed);
	if(err != nil)
		return err;

	#
	#  extended Hayes commands, meaning depends on modem (VGA all over again)
	#
	if(d.t.init != nil)
		applyspecial(d, d.t.init);
	applyspecial(d, d.t.errorcorrection);
	if(compress == Ok){
		if(mnponly)
			compress = applyspecial(d, d.t.mnponly);
		else
			compress = applyspecial(d, d.t.compression);
	}
	if(compress != Ok)
		rateadjust = applyspecial(d, d.t.rateadjust);
	applyspecial(d, d.t.flowctl);

	# finally, dialout
	if(send(d, sys->sprint("ATDT%s\r", number)) < 0)
		return "can't dial "+number;
	(i, msg) := readmsg(d, 120, nil);
	if(i != Success)
		return msg;

	# change line rate if not compressing
	if(rateadjust == Ok)
		setspeed(d, getspeed(msg, d.speed));

	return nil;
}

#
#  apply a command type
#
applyspecial(d: ref Dev, cmd: string): int
{
	if(cmd == nil)
		return Failure;
	return apply(d, cmd, nil, 2);
}

#
#  hang up any connections in progress
#
onhook(d: ref Dev)
{
	sys->fprint(d.ctl, "d0");
	sys->fprint(d.ctl, "r0");
	sys->sleep(250);
	sys->fprint(d.ctl, "r1");
	sys->fprint(d.ctl, "d1");
	sys->fprint(d.ctl, "y1");	# hangup on DSRc (Vita Nuova change to devns16552.c)
}

#
#  read till we see a message or we time out
#
readmsg(d: ref Dev, secs: int, substr: string): (int, string)
{
	start: int;

	found := 0;
	secs *= 1000;
	limit := 1000;
	s := "";
	for(start = sys->millisec(); sys->millisec() <= start+secs;){
		d.lock.lock();
		a := getinput(d,1);
		d.lock.unlock();
		if(len a == 0){
			if(limit){
				sys->sleep(500);
				continue;
			}
			break;
		}
		if(a[0] == byte '\n' || a[0] == byte '\r' || limit == 0){
			if(debug && len s)
				sys->print("<-%s\n", s);
			if(substr != nil && contains(s, substr))
				found = 1;
			for(k := 0; k < len msgs; k++)
				if(len s >= len msgs[k].text &&
				   s[0:len msgs[k].text] == msgs[k].text){
					if(found)
						return (Found, s);
					return (msgs[k].code, s);
				}
			start = sys->millisec();
			s = "";
			continue;
		}
		s[len s] = int a[0];
		limit--;
	}
	s = "No response from modem";
	if(found)
		return (Found, s);
	return (Noise, s);
}

#
#  get baud rate from a connect message
#
getspeed(msg: string, speed: int): int
{
	p := msg[7:];	# skip "CONNECT"
	while(p[0] == ' ' || p[0] == '\t')
		p = p[1:];
	s := int p;
	if(s <= 0)
		return speed;
	else
		return s;
}

#
#  set speed and RTS/CTS modem flow control
#
setspeed(d: ref Dev, baud: int)
{
	if(d.ctl == nil)
		return;
	sys->fprint(d.ctl, "b%d", baud);
	sys->fprint(d.ctl, "m1");
}

#
# prepare a modem port
#
openserial(d: ref Dev): string
{
	d.data = sys->open(d.local, Sys->ORDWR);
	if(d.data == nil)
		sys->print("can't open %s: %r\n", d.local);
	d.ctl = sys->open(d.local+"ctl", Sys->ORDWR);
	if(d.ctl == nil)
		sys->print("can't open %sctl: %r\n", d.local);
	d.speed = 38400;
	d.avail = nil;
	d.rd = chan of array of byte;
	return nil;
}

#
#  a process to read input from a modem.
#
monitor(d: ref Dev)
{
	d.pid = sys->pctl(0, nil);
	a := array[ATOMICIO] of byte;
	for(;;) {
		d.lock.lock();
		e := openserial(d);
		d.lock.unlock();
		if (e != nil)
			error("opening serial");
		d.lock.lock();
		d.status = "Idle";
		d.remote = "";
		setspeed(d, d.speed);
		d.lock.unlock();
		# wait for (ring or) off hook
		while (d.open == 0) {
			n := sys->read(d.data, a, 1);
			if(debug)
				sys->print("monitor (on hook): %d: %s\n", 1, dumpa(a[0:1]));
			if (n < 1)
				continue;
			# could catch incoming calls here
			if (d.open != 0)
				d.rd <-= a[0:1];
		}
		# shuttle bytes till on hook
		while (d.open) {			
			n := sys->read(d.data, a, len a);
			if (n <=  0)
				break;
			b := array[n] of byte;
			b[0:] = a[0:n];
			d.lock.lock();
			if (len d.avail < Sys->ATOMICIO) {
				na := array[len d.avail + n] of byte;
				na[0:] = d.avail[0:];
				na[len d.avail:] = b[0:];
				d.avail = na;
			}				
			serve(d);
			d.lock.unlock();
		}
		d.ctl = nil;
		d.data = nil;
		d.rd <-= nil;
	}
}

#
#  get bytes input by monitor() (we assume d is locked)
#
getinput(d: ref Dev, n: int): array of byte
{
	if(len d.avail == 0)
		return nil;
	if(n > len d.avail)
		n = len d.avail;
	b := d.avail[0:n];
	d.avail = d.avail[n:];
	return b;
}

#
#  fulfil a read request (we assume d is locked)
#
serve(d: ref Dev)
{
	r: Request;

	for(;;) {
		if(len d.r == 0)
			return;
		getinput(d, 0);
		if(len d.avail == 0)
			return;
		r = d.r[0];
		if(r.count > ATOMICIO)
			r.count = ATOMICIO;
		buf := getinput(d, r.count);
		if (len buf == 0)
			return;
		l := array [len d.r - 1] of Request;
		l[0:] = d.r[1:];
		d.r = l;

		reply->readR(r.tag, r.fid.fid, len buf, buf);
	}
}

contains(s, t: string): int
{
	if(t == nil)
		return 1;
	if(s == nil)
		return 0;
	n := len t;
	for(i := 0; i+n <= len s; i++)
		if(s[i:i+n] == t)
			return 1;
	return 0;
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
#	Qid management
#
DEV(q: Qid): int
{
	return (q.path&Devmask)>>8;
}

TYPE(q: Qid): int
{
	return (q.path&((1<<8)-1));
}

MKQID(t: int, i: int): int
{
	return (((i<<8)&Devmask) | t);
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
	ff.fid = fid;
	return ff;
}

#
#	Control Locks
#
Lock.lock(l: self ref Lock)
{
	l.q <-= 1;
}

Lock.unlock(l: self ref Lock)
{
	<-l.q;
}

manager(l: ref Lock)
{
	while(<-l.q)
		l.q <-= 0;
}

Lock.init(l: self ref Lock)
{
	l.q = chan of int;
	spawn manager(l);
}

Lock.end(l: self ref Lock)
{
	l.q <-= 0;
}


#
#	Styx transactions.
#

rerror(tag: int, s: string)
{
	if (debug)
		sys->print("error: %s\n", s);
	reply->errorR(tag, s);
}

#
#  generate a stat structure for a qid
#
devstat(dir: ref Dir, buf: array of byte, conv: int)
{
	d: ref Dev;
	t: int;

	dir.name = "";
	dir.uid = "";
	dir.gid = "";
	t = TYPE(dir.qid);
	if (t != Qlvl3)
		dir.name = names[t];
	else
		dir.name = sys->sprint("%d", DEV(dir.qid));
	dir.mode = 8r755;
	dir.uid = user;
	if (t >= Qlvl3) {
		d = dev[DEV(dir.qid)];
		if (d.open) {
			dir.mode = d.perm;
			dir.uid = d.user;
		}
	}
	if (dir.qid.path & CHDIR)
		dir.mode |= CHDIR;
	dir.gid = user;
	if (t == Qdata) {
		d = dev[DEV(dir.qid)];
		dir.length = len d.avail;
	} else
		dir.length = 0;
	dir.atime = daytime->now();
	dir.mtime = dir.atime;
	if(conv)
		convD2M(dir, buf);
}

#
#  enumerate files we can walk to from q
#
v: int = 0;
devgen(q: Qid, i: int, d: ref Dir, buf: array of byte, conv: int) : int
{
	d.qid.vers = v++;
	case TYPE(q) {
	Qlvl1	=>
		if (i != 0)
			return -1;
		d.qid.path = CHDIR|Qlvl2;
	Qlvl2	=>
		case i {
		-1		=>
			d.qid.path = CHDIR|Qlvl1;
		0		=>
			d.qid.path = Qclone;
		*		=>
			if(i > ndev)
				return -1;
			d.qid.path = MKQID(CHDIR|Qlvl3, i-1);
		}
	Qlvl3	=>
		case i {
		-1		=>
			d.qid.path = CHDIR|Qlvl2;
		0		=>
			d.qid.path = MKQID(Qdata, DEV(q));
		1		=>
			d.qid.path = MKQID(Qctl, DEV(q));
		2		=>
			d.qid.path = MKQID(Qlocal, DEV(q));
		3		=>
			d.qid.path = MKQID(Qremote, DEV(q));
		4		=>
			d.qid.path = MKQID(Qstatus, DEV(q));
		*		=>
			return -1;
		}
	*		=>
		return -1;
	}
	devstat(d, buf, conv);
	return 0;
}

nopT(tag: int)
{
	reply->nopR(tag);
}

flushT(tag, oldtag: int)
{
	for(k:=0; k<ndev; k++) {
		d := dev[k];
		d.lock.lock();
		r := d.r;
		for(i:=0; i<len r; i++)
			if (r[i].tag == oldtag) {
				nr := array [len r - 1] of Request;
				nr[0:] = r[0:i];
				nr[i:] = r[i+1:];
				d.r = nr;
			}
		d.lock.unlock();
	}
	reply->flushR(tag);
}

cloneT(tag, fid, newfid: int)
{
	f, n: ref Fid;

	f = getfid(fid);
	if (f.open) {
		rerror(tag, Eisopen);
		return;
	}
	if (f.busy == 0) {
		rerror(tag, Enotexist);
		return;
	}
	n = getfid(newfid);
	n.busy = 1;
	n.open = 0;
	n.qid = f.qid;
	n.user = f.user;
	reply->cloneR(tag, fid);
}

walkT(tag, fid: int, fname: array of byte)
{
	f: ref Fid;
	dir := ref Dir;
	name := sys->sprint("%s", string fname);


	f = getfid(fid);

	if ((f.qid.path & CHDIR) == 0) {
		rerror(tag, Enotdir);
		return;
	}
	if (name == ".")
		dir.qid = f.qid;
	else if (name == "..") {
		if (devgen(f.qid, -1,  dir, array of byte "", 0) < 0) {
			rerror(tag, Enotexist);
			return;
		}
	} else for(i := 0;; i++) {
		if (devgen(f.qid, i, dir, array of byte "", 0) < 0) {
			rerror(tag, Enotexist);
			return;
		}
		if (name == dir.name) 
			break;
	}
	f.qid = dir.qid;
	reply->walkR(tag, fid, f.qid);
}

openT(tag, fid, mode: int)
{
	f: ref Fid;
	t:	int;

	f = getfid(fid);
	if (f.open) {
		rerror(tag, Eisopen);
		return;
	}
	mode &= 3;
	if (f.qid.path & CHDIR) {
		if(mode != OREAD) {
			rerror(tag, Eperm);
			return;
		}
	} else {
		t = TYPE(f.qid);
		if (t == Qclone) {
			for(i:=0; i<ndev; i++)
				if(dev[i].open == 0)
					break;
			if (i == ndev) {
				rerror(tag, Enodev);
				return;
			}
			f.qid.path = MKQID(Qctl, i);
			t = Qctl;
		}
		case t {
		Qdata or Qctl or Qlocal or Qremote or Qstatus	=>
			d := dev[DEV(f.qid)];
			if (d.open == 0) {
				d.user = f.user;
				d.perm = 8r660;
			} else {
				if (mode==OWRITE || mode==ORDWR)
					if (!perm(f, d, Pwrite)) {
						rerror(tag, Eperm);
						return;
					}
				if (mode==OREAD || mode==ORDWR)
					if (!perm(f, d, Pread)) {
						rerror(tag, Eperm);
						return;
					}
			}
			d.open++;
			break;
		}
		f.open = 1;
	}
	reply->openR(tag, fid, f.qid);
}

createT(tag, nil: int, nil: array of byte, nil, nil: int)
{
	rerror(tag, Eperm);
}

readT(tag, fid: int, offset: big, cnt: int)
{
	f: ref Fid;
	t, n: int;
	dir: Dir;
	off: int = int offset;

	f = getfid(fid);
	if (cnt > ATOMICIO)
		cnt = ATOMICIO;
	n = 0;
	t = TYPE(f.qid);
	case t {
	*	=>
		cnt = (cnt/STATSZ)*STATSZ;
		if (off% STATSZ) {
			rerror(tag, Eio);
			return;
		}
		for(i := off/STATSZ; n < cnt; i++){
			if(devgen(f.qid, i, ref dir, tbuff[n:], 1) < 0)
				break;
			n += STATSZ;
		}
	Qctl	or Qlocal or Qremote or Qstatus	=>
		dno := DEV(f.qid);
		d := dev[dno];
		text := array of byte "";
		case t {
		Qctl =>	text = array of byte sys->sprint("%d", dno);
		Qlocal =>	text = array of byte (d.local + "\n");
		Qremote => text = array of byte (d.remote + "\n");
		Qstatus => text = array of byte sys->sprint("telco/%d %d %s\n", dno, d.open, d.status);
		}
		i := len text;
		if (off < i) {
			n = cnt;
			if (off + n > i)
				n = i - off;
			tbuff[0:] = text[off:off+n];
		} else
			n = 0;
	Qdata	=>
		d := dev[DEV(f.qid)];
		d.lock.lock();
		o := d.r;
		nr := array [len o + 1] of Request;
		nr[0:] = o[0:];
		r: Request;
		r.tag = tag;
		r.count = cnt;
		r.fid = f;
		nr[len o] = r;
		d.r = nr;
		serve(d);
		d.lock.unlock();
		return;		# reply is queued
	}
	reply->readR(tag, fid, n, tbuff);
}

writeT(tag, fid: int, offset: big, cnt: int, data: array of byte)
{
	f: ref Fid;
	off := int offset;

	f = getfid(fid);
	case TYPE(f.qid) {
	*		=>
		rerror(tag, "file is a directory");
		return;
	Qctl		=>
		d := dev[DEV(f.qid)];
		cmsg := "connect";
		clen := len cmsg;
		if (cnt < clen || sys->sprint("%s", string data[0:clen]) != "connect") {
			#
			#  send control message to real control file
			#
			if (sys->seek(d.ctl, off, 0) < 0 || sys->write(d.ctl, data, cnt) < 0) {
				rerror(tag, sys->sprint("%r"));
				return;
			}
		} else {
			#
			#  connect
			#
			args := string data[clen+1:cnt];
			cp := dialout(d, sys->sprint("%s", args));
			if (cp != nil) {
				rerror(tag, cp);
				return;
			}
			d.status = "Connected";
			d.remote = args;
		}
		break;
	Qdata	=>
		d := dev[DEV(f.qid)];
		if (sys->write(d.data, data, cnt) < 0) {
			rerror(tag, sys->sprint("%r"));
			return;
		}
		break;
	Qlocal or Qremote or Qstatus	=>
		# nothing to do here
		break;
	}
	reply->writeR(tag, fid, cnt);
}

clunkT(tag, fid: int)
{
	f: ref Fid;

	f = getfid(fid);
	if (f.open)
		case TYPE(f.qid) {
		Qdata or Qctl or Qlocal or Qremote or Qstatus	=>
			d := dev[DEV(f.qid)];
			if (d.open == 1)
				onhook(d);
			d.open--;
			break;
		}
	f.user = nil;
	f.busy = 0;
	f.open = 0;
	reply->clunkR(tag, fid);
}

removeT(tag, nil: int)
{
	rerror(tag, Eperm);
	return;
}

statT(tag, fid: int)
{
	f: ref Fid;
	d := ref Dir;

	f = getfid(fid);
	d.qid = f.qid;
	devstat(d, tbuff, 1);
	reply->statR(tag, fid, tbuff);
}

wstatT(tag, fid: int, stat: array of byte)
{
	f: ref Fid;
	dir:= ref Dir;

	f = getfid(fid);
	if (TYPE(f.qid) < Qlvl3) {
		rerror(tag, Eperm);
		return;
	}

	convM2D(stat, dir);
	d := dev[DEV(f.qid)];

	#
	# To change mode, must be owner
	#
	if (d.perm != dir.mode) {
		if (f.user != d.user)
		if (f.user != user) {
			rerror(tag, Eperm);
			return;
		}
	}

	# all ok; do it
	d.perm = dir.mode & ~CHDIR;
	reply->wstatR(tag, fid);
}

attachT(tag, fid: int, uid: array of byte, nil: array of byte)
{
	f: ref Fid;

	f = getfid(fid);
	f.busy = 1;
	f.qid.path = CHDIR | Qlvl1;
	f.qid.vers = 0;
	if (len uid > 0)
		f.user = sys->sprint("%s", string uid);
	else
		f.user = "none";
	reply->attachR(tag, fid, f.qid);
}

#
# Miscellaneous
#

killpid(pid: int)
{
	fd := sys->open("#p/"+(string pid)+"/ctl", Sys->OWRITE);
	if (fd == nil)
		return;
	sys->fprint(fd, "kill");
}

perm(f: ref Fid, d: ref Dev, p: int): int
{
	if ((p*Pother) & d.perm)
		return 1;
	if (f.user == user && ((p*Pgroup) & d.perm))
		return 1;
	if(f.user == d.user && ((p*Powner) & d.perm))
		return 1;
	return 0;
}

error(msg: string)
{
	sys->fprint(stderr, "telcofs: %s: %r\n", msg);
	shutdown();
}

getuser(): string
{
	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil)
		return "inferno";

	buf := array[Sys->NAMELEN] of byte;
	n := sys->read(fd, buf, len buf);
	if(n <= 0)
		return "inferno";

	return string buf[0:n];
}
