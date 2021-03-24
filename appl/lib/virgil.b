implement Virgil;

include "sys.m";
include "string.m";
include "keyring.m";
include "draw.m";
include "security.m";

sys: Sys;

stderr: ref Sys->FD;
done: int;

#
#  this module is very udp dependent.  it shouldn't be. -- presotto
#  Call with first element of argv an arbitrary string, which is
#  discarded here.  argv must also contain at least a question.
#
virgil(argv: list of string): string
{
	s,question,reply,r : string;
	addr : list of string;
	noctet, timerpid, readerpid: int;

	if (argv == nil || tl argv == nil || hd (tl argv) == nil)
		return nil;
	done = 0;
	str := load String "/dis/lib/string.dis";
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	# We preserve the convention that the first arg is not an option.
	# Undocumented '-v address' option allows passing in address
	# of virgild, circumventing broadcast.  Used for development,
	# to avoid pestering servers on network.
	argv = tl argv;
	s = hd argv;
	if(s[0] == '-') {
		if(s[1] != 'v')
			return nil;
		argv = tl argv;
		if (argv == nil)
			return nil;
		s = hd argv;
		(noctet, addr) = sys->tokenize(s, ".");
		if (noctet != 4)
			return nil;
		argv = tl argv;
	}

	# Is there a question?
	if (argv == nil)
		return nil;
	question = hd argv;

	(ok, c) := sys->announce("udp!*!0");
	if(ok < 0)
		return nil;
	if(sys->fprint(c.cfd, "headers4") < 0)
		return nil;
	c.dfd = sys->open(c.dir+"/data", sys->ORDWR);
	if(c.dfd == nil)
		return nil;

	readerchan := chan of string;
	timerchan := chan of int;
	readerpidchan := chan of int;

	spawn timer(timerchan);
	timerpid = <-timerchan;
	spawn reader(c.dfd, readerchan, readerpidchan);
	readerpid = <-readerpidchan;

	buf := array[1000] of byte;
	question = getid() + "?" + question;
	qbuf := array of byte question;
	buf[12:] = qbuf;
	if (addr != nil) {
		for (bdx := 0; bdx < noctet; bdx++) {
			buf[bdx] = byte hd addr;
			addr = tl addr;
		}
	} else {
		buf[0] = byte 255;
		buf[1] = byte 255;
		buf[2] = byte 255;
		buf[3] = byte 255;
	}
	buf[8] = byte (2202>>8);
	buf[9] = byte (2202 & 16rFF);
	for(tries := 0; tries < 5; ){
		if(sys->write(c.dfd, buf, 6 + len qbuf) < 0)
			break;

		alt {
		r = <-readerchan =>
			;
		<-timerchan =>
			tries++;
			continue;
		};

		if(str->prefix(question + "=", r)){
			reply = r[len question + 1:];
			break;
		}
	}

	done = 1;
	spawn killpid(readerpid);
	spawn killpid(timerpid);
	return reply;
}

getid(): string
{
	fd := sys->open("/dev/sysname", sys->OREAD);
	if(fd == nil)
		return "unknown";
	buf := array[256] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 1)
		return "unknown";
	return string buf[0:n];
}

reader(fd: ref sys->FD, cstring: chan of string, cpid: chan of int)
{
	pid := sys->pctl(0, nil);
	cpid <-= pid;

	buf := array[2048] of byte;
	n := sys->read(fd, buf, len buf);
	if(n <= 6)
		return;

	# dump cruft
	for(i := 6; i < n; i++)
		if((int buf[i]) == 0)
				break;

	if(!done)
		cstring <-= string buf[6:i];
}

timer(c: chan of int)
{
	pid := sys->pctl(0, nil);
	c <-= pid;
	while(!done){
		sys->sleep(1000);
		if(done)
			break;
		c <-= 1;
	}
}

killpid(pid: int)
{
	# Fork namespace to avoid perturbing /prog for relatives.
	sys->pctl(sys->FORKNS, nil);
	sys->bind("#p", "/prog", sys->MREPL);

	fd := sys->open("/prog/"+(string pid)+"/ctl", sys->OWRITE);
	if(fd == nil)
		return;
	sys->write(fd, array of byte "kill", 4);
}
