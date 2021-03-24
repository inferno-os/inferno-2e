implement Addpassword;

# This is actually a modified version of changelogin 
# that defaults the password and free form comments.

include "sys.m";
sys: Sys;

include "daytime.m";
daytime: Daytime;

include "draw.m";

include "keyring.m";
kr: Keyring;

include "security.m";
pass: Password;

Addpassword: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

stderr, stdin, stdout: ref Sys->FD;

init(nil: ref Draw->Context, args: list of string)
{
	ok: int;
	word: string;

	sys = load Sys Sys->PATH;
	kr = load Keyring Keyring->PATH;

	stdin = sys->fildes(0);
	stdout = sys->fildes(1);
	stderr = sys->fildes(2);

	argv0 := hd args;
	args = tl args;

	if(args == nil){
		sys->fprint(stderr, "usage: %s userid\n", argv0);
		exit;
	}

	daytime = load Daytime Daytime->PATH;
	if(daytime == nil) {
		sys->fprint(stderr, "%s: load Daytime: %r\n", argv0);
		exit;
	}
	pass = load Password Password->PATH;
	if(pass == nil) {
		sys->fprint(stderr, "%s: load Password: %r\n", argv0);
		exit;
	}

	# calculate expiration time (midnight of date specified)
	npw := ref Password->PW;
	now := daytime->now();
	tm := daytime->local(now);
	tm.sec = 59;
	tm.min = 59;
	tm.hour = 23;
	expsecs := now + 365*24*60*60;
	otm := daytime->local(expsecs);
	npw.expire = expsecs;
	
	# get password
	id := hd args;
	pw := pass->get(id);
	if(pw == nil)
		sys->print("new account\n");
	npw.id = id;
	for(;;){
		if(pw != nil)
			sys->print("password already exists [default = don't change]: ");
		else
			sys->print("password: ");
		(ok, word) = readline(stdin, "rawon");
		if(!ok)
			exit;
		if(word == "" && pw != nil)
			break;
		if(len word >= 7)
			break;
		sys->print("!password must be at least 7 characters\n");
	}
	if(word == "")
		npw.pw = pw.pw;
	else {
		pwbuf := array of byte word;
		npw.pw = array[Keyring->SHAdlen] of byte;
		kr->sha(pwbuf, len pwbuf, npw.pw, nil);
	}

	# default the free form field
	npw.other = "";

	if(pass->put(npw) <= 0)
		sys->fprint(stderr, "%s: error writing entry: %r\n", argv0);
	else
		if (word == "")
			sys->print("Password for user %s is unchanged.\n\n", id);
		else
			sys->print("Created password for user %s\n\n", id);
}
 
readline(io: ref Sys->FD, mode: string): (int, string)
{
	r : int;
	line : string;
	buf := array[8192] of byte;
	fdctl : ref Sys->FD;
	rawoff := array of byte "rawoff";

	if(mode == "rawon"){
		# Change console mode to rawon
		fdctl = sys->open("/dev/consctl", sys->OWRITE);
		if(fdctl == nil || sys->write(fdctl, array of byte mode, len mode) != len mode){
			sys->fprint(stderr, "unable to change console mode");
			return (0,nil);
		}
	}

	# Read up to the CRLF
	line = "";
	for(;;) {
		r = sys->read(io, buf, len buf);
		if(r <= 0){
			sys->fprint(stderr, "error read from console mode");
			if(mode == "rawon")
				sys->write(fdctl,rawoff,6);
			return (0, nil);
		}

		line += string buf[0:r];
		if ((len line >= 1) && (line[(len line)-1] == '\n')){
			if(mode == "rawon"){
				r = sys->write(stdout,array of byte "\n",1);
				if(r <= 0){
					sys->write(fdctl,rawoff,6);
					return (0, nil);
				}
			}
			break;
		}
		else {
			if(mode == "rawon"){
				r = sys->write(stdout, array of byte "*",1);
				if(r <= 0){
					sys->write(fdctl,rawoff,6);
					return (0, nil);
				}
			}
		}
	}

	if(mode == "rawon")
		sys->write(fdctl,rawoff,6);

	# Total success!
	return (1, line[0:len line - 1]);
}
