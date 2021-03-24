implement Modem;

include "sys.m";
	sys: Sys;

include "lock.m";
	lock: Lock;

include "draw.m";

include "kill.m";
include "script.m";
include "pppclient.m";

include "modem.m";

# modem return codes
Ok, Success, Failure, Abort, Noise, Found: con iota;

maxspeed: con 38400;

#
#  modem return messages
#
Msg: adt {
	text: 		string;
	code: 		int;
};

msgs: array of Msg = array [] of {
	("OK", 			Ok),
	("NO CARRIER", 	Failure),
	("ERROR", 		Failure),
	("NO DIALTONE", Failure),
	("BUSY", 		Failure),
	("NO ANSWER", 	Failure),
	("CONNECT", 	Success),
};

kill(pid: int)
{
	killmod := load Kill Kill->PATH;
	if (killmod == nil) {
		sys->raise("fail: couldn't load kill module");
		return;
	}
	killmod->killpid( string pid, nil );
}

#
# Remove references to a modem device
# 	and if necessary clean-up it's monitor process
#

close( m: ref Device )
{
	if (m == nil)
		return;
	if (m.pid) 
		kill(m.pid);

	m.ctl = nil;
	m.data = nil;		
}

#
# Send a string to the modem
#

send(d: ref Device, x: string): int
{
	if (d == nil)
		return -1;
	
	a := array of byte x;
	f := sys->write(d.data,a, len a);
	if (f < 0) {
		# lets attempt to close & reopen the modem
		close(d);
		openserial(d);
		f = sys->write(d.data,a, len a);
	}
	sys->print("->%s\n",x);
	return f;
}

#
#  apply a string of commands to modem & look for a response
#

apply(d: ref Device, s: string, substr: string, secs: int): int
{
	if (d == nil)
		return Abort;
	m := Ok;
	buf := "";
	for(i := 0; i < len s; i++){
		c := s[i];
		buf[len buf] = c;		# assume no Unicode
		if(c == '\r' || i == (len s -1)){
			if(c != '\r')
				buf[len buf] = '\r';
			if(send(d, buf) < 0)
				return Abort;
			(m, nil) = readmsg(d, secs, substr);
			buf = "";
		}
	}
	return m;
}

#
#  get modem into command mode if it isn't already
#

ATT_WAIT: con 250;

attention(d: ref Device): int
{
	if (d==nil)
		return Abort;

	for(i := 0; i < 2; i++){
		if(apply(d, "ATZH0", nil, 2) == Ok)
			return Ok;
		sys->sleep(ATT_WAIT);
		if(send(d, "+") < 0) 
			return Abort;
		sys->sleep(ATT_WAIT);
		if(send(d, "+") < 0)
			return Abort;
		sys->sleep(ATT_WAIT);
		if(send(d, "+") < 0)
			return Abort;
		sys->sleep(ATT_WAIT);
		readmsg(d, 0, nil);
	}
	return Failure;
}

#
#  apply a command type
#

applyspecial(d: ref Device, cmd: string): int
{
	if (d == nil)
		return Abort;

	if(cmd == nil)
		return Failure;
	return apply(d, cmd, nil, 2);
}

#
#  hang up any connections in progress
#

onhook(d: ref Device)
{
	if (d == nil)
		return;
	# if no monitor then spawn one off
	if (d.pid == 0) {
		ctlchan := chan of int;
		spawn monitor( d, ctlchan );
		d.pid =<- ctlchan;
		if (d.pid < 0) {
			sys->raise("fail: Couldn't spawn modem monitor");
			return;
		}
	}

	# lets try and do it the old fashion way
	if (attention(d) == Ok)
		return;
	# now the hardware approach
	if (d.ctl != nil) {
		sys->fprint(d.ctl,"d0\n");
		sys->fprint(d.ctl,"r0\n");
		sys->fprint(d.ctl, "h\n");	# hangup on native serial 
		sys->sleep(250);
		sys->fprint(d.ctl,"r1\n");
		sys->fprint(d.ctl,"d1\n");
	}
	# ok, now we have to close & reopen the device
	close(d);
	openserial(d);
}

#
#	Substring thingy - why isn't this in the strings module?
#

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

#
#  read till we see a message or we time out
#
readmsg(d: ref Device, secs: int, substr: string): (int, string)
{
	start: int;
	if (d == nil)
		return (Abort, "device not initialized");
	found := 0;
	secs *= 1000;
	limit := 1000;		# pretty arbitrary
	s := "";

	for(start = sys->millisec(); sys->millisec() <= start+secs;){
		lock->d.lock.obtain();
		a := getinput(d,1);
		lock->d.lock.release();
		if(len a == 0){
			if(limit){
				sys->sleep(1);
				continue;
			}
			break;
		}
		if(a[0] == byte '\n' || a[0] == byte '\r' || limit == 0){
			if (len s) {
				if (s[(len s)-1] == '\r')
					s[(len s)-1] = '\n';
				sys->print("<-%s\n",s);
			}
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

setspeed(d: ref Device, baud: int)
{
	if (d== nil)
		return;
	if(d.ctl == nil)
		return;
	sys->fprint(d.ctl, "b%d", baud);
	sys->fprint(d.ctl, "m1");
}


#
# prepare a modem port
#
openserial(d: ref Device)
{
	if (d==nil) {
		sys->raise("fail: device not initialized");
		return;
	}

	d.data = sys->open(d.local, Sys->ORDWR);
	if(d.data == nil) {
		sys->raise("fail: can't open "+d.local);
		return;
	}

	d.ctl = sys->open(d.local+"ctl", Sys->ORDWR);
	if(d.ctl == nil) {
		sys->raise("can't open "+d.local+"ctl");
		return;
	}

	d.speed = maxspeed;
	d.avail = nil;
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
#  a process to read input from a modem.
#
monitor(d: ref Device, ctlchan: chan of int )
{
	if (d==nil)
		ctlchan <-= -1;

	a := array[Sys->ATOMICIO] of byte;
	for(;;) {
		openserial(d);

		ctlchan <-= sys->pctl(0,nil);
		lock->d.lock.obtain();
		d.status = "Idle";
		d.remote = "";
		setspeed(d, d.speed);
		lock->d.lock.release();
		# shuttle bytes
		while (d.data != nil) {			
			n := sys->read(d.data, a, len a);
			if (n <=  0) 
				break;
			b := array[n] of byte;
			b[0:] = a[0:n];
			lock->d.lock.obtain();
			if (len d.avail < Sys->ATOMICIO) {
				na := array[len d.avail + n] of byte;
				na[0:] = d.avail[0:];
				na[len d.avail:] = b[0:];
				d.avail = na;
			}				
			lock->d.lock.release();
		}
		d.ctl = nil;
		d.data = nil;
	}
}

#
#  get bytes input by monitor() (we assume d is locked)
#
getinput(d: ref Device, n: int): array of byte
{
	if (d==nil)
		return nil;

	if(len d.avail == 0)
		return nil;
	if(n > len d.avail)
		n = len d.avail;
	baud := d.avail[0:n];
	d.avail = d.avail[n:];
	return baud;
}

getc(m: ref Device, timo: int): int
{
	start := sys->millisec();
	while(len (b  := getinput(m, 1)) == 0) {
		if (timo && sys->millisec() > start+timo)
			return 0;
		sys->sleep(1);
	}
	return int b[0];
}

init(modeminfo: ref ModemInfo ): ref Device
{
	if (sys == nil) {
		sys = load Sys Sys->PATH;
		lock = load Lock Lock->PATH;
		if (lock == nil) {
			sys->raise("fail: Couldn't load lock module");
			return nil;
		}
	}

	# do something constructive with modeminfo (like equivilent of add)
	newdev := ref Device;
	newdev.lock = lock->init();
	newdev.local = modeminfo.path;
	newdev.pid = 0;
	newdev.t = modeminfo;

	return newdev;
}


#
#  dial a number
#
dial(d: ref Device, number: string)
{
	if (d==nil) {
		sys->raise("fail: Device not initialized");
		return;
	}

	# if no monitor then spawn one off
	if (d.pid == 0) {
		ctlchan := chan of int;
		spawn monitor( d, ctlchan );

		d.pid =<- ctlchan;
		if (d.pid < 0) {
			sys->raise("fail: Couldn't spawn modem monitor");
			return;
		}
	}

	# modem type should already be established, but just in case
	sys->print("Attention\n");
	x := attention(d);
	if (x != Ok)
		sys->print("Attention failed\n");
	#
	#  extended Hayes commands, meaning depends on modem (VGA all over again)
	#
	sys->print("Init\n");
	if(d.t.init != nil)
		applyspecial(d, d.t.init);
	applyspecial(d, d.t.errorcorrection);

	compress := Abort;
	if(d.t.mnponly != nil)
			compress = applyspecial(d, d.t.mnponly);
	if(d.t.compression != nil)
			compress = applyspecial(d, d.t.compression);

	rateadjust := Abort;
	if(compress != Ok)
		rateadjust = applyspecial(d, d.t.rateadjust);
	applyspecial(d, d.t.flowctl);

	# finally, dialout
	sys->print("Dialing\n");
	if(send(d, sys->sprint("ATDT%s\r", number)) < 0) {
		sys->raise("can't dial "+number);
		return;
	}

	(i, msg) := readmsg(d, 120, nil);
	if(i != Success) {
		sys->raise("fail: "+msg);
		return;
	}

	# change line rate if not compressing
	if(rateadjust == Ok)
		setspeed(d, getspeed(msg, d.speed));
}