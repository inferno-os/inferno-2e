implement Verify;

include "sys.m";
sys: Sys;

include "draw.m";

Verify: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

stderr, stdin: ref Sys->FD;

pro := array[] of {
	"alpha", "bravo", "charlie", "delta", "echo", "foxtrot", "golf",
	"hotel", "india", "juliet", "kilo", "lima", "mike", "nancy", "oscar",
	"poppa", "quebec", "romeo", "sierra", "tango", "uniform",
	"victor", "whiskey", "xray", "yankee", "zulu"
};

init(nil: ref Draw->Context, args: list of string)
{
	s: string;

	sys = load Sys Sys->PATH;

	stdin = sys->fildes(0);
	stderr = sys->fildes(2);

	if(args != nil)
		args = tl args;
	if(args == nil){
		sys->fprint(stderr, "usage: verify boxid\n");
		exit;
	}

	sys->pctl(Sys->FORKNS, nil);
	if(sys->chdir("/keydb") < 0){
		sys->fprint(stderr, "signer: no key database\n");
		exit;
	}

	boxid := hd args;
	file := "signed/"+boxid;
	fd := sys->open(file, Sys->OREAD);
	if(fd == nil){
		sys->fprint(stderr, "signer: no file %s\n", file);
		exit;
	}
	certbuf := getmsg(fd);
	digest := getmsg(fd);
	if(digest == nil || certbuf == nil){
		sys->fprint(stderr, "signer: can't read %s\n", file);
		exit;
	}

	for(i := 0; i < len digest; i++){
		s = s + (string (2*i)) + ": " + pro[((int digest[i])>>4)%len pro] + "\t";
		s = s + (string (2*i+1)) + ": " + pro[(int digest[i])%len pro] + "\n";
	}

	sys->print("%s\naccept (y or n)? ", s);
	buf := array[5] of byte;
	n := sys->read(stdin, buf, len buf);
	if(n < 1 || buf[0] != byte 'y'){
		sys->print("\nrejected\n");
		exit;
	}
	sys->print("\naccepted\n");

	nfile := "countersigned/"+boxid;
	fd = sys->create(nfile, Sys->OWRITE, 8r600);
	if(fd == nil){
		sys->fprint(stderr, "signer: can't create %s\n", nfile);
		exit;
	}
	if(sendmsg(fd, certbuf, len certbuf) < 0){
		sys->fprint(stderr, "signer: can't write %s\n", nfile);
		exit;
	}
}

#
#  messages start with a newline terminated string specifying the length and then
#  the message as a string
#
getmsg(fd: ref Sys->FD): array of byte
{
	buf := readn(fd, 5);
	if(buf == nil)
		return nil;
	n := int (string buf);
	if(n <= 0 || n > 8*1024)
		return nil;
	buf = readn(fd, n);
	if(buf == nil)
		return nil;
	return buf;
}
sendmsg(fd: ref Sys->FD, buf: array of byte, n: int): int
{
	if(sys->fprint(fd, "%4.4d\n", n) < 0)
		return -1;
	if(sys->write(fd, buf, n) < 0)
		return -1;
	return 0;
}
readn(fd: ref Sys->FD, n: int): array of byte
{
	i: int;

	buf := array[n] of byte;
	tmp := array[n] of byte;
	for(sofar := 0; sofar < n; sofar += i){
		i = sys->read(fd, tmp, n - sofar);
		if(i <= 0)
			return nil;
		for(j := 0; j < i; j++)
			buf[sofar+j] = tmp[j];
	}
	return buf;
}
