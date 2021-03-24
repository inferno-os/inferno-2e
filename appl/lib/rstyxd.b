implement Rstyxd;

include "sys.m";
	sys: Sys;
	stdin, stderr: ref Sys->FD;

include "draw.m";

include "keyring.m";
include "security.m";
	kr: Keyring;
	auth: Auth;

include "sh.m";
	sh: Command;

Rstyxd: module {
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

# argv is a list of Inferno supported algorithms such as
#
#		Auth->NOAUTH ::
#               Auth->NOSSL :: 
#               Auth->CLEAR :: 
#               Auth->SHA :: 
#               Auth->MD5 :: 
#               Auth->RC4 ::
#               Auth->SHA_RC4 ::
#               Auth->MD5_RC4 ::
#               nil;
#
init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stdin = sys->fildes(0);
	stderr = sys->open("/dev/cons", sys->OWRITE);

	auth = load Auth Auth->PATH;
	if(auth == nil){
		sys->fprint(stderr, "Error: rstyxd: can't load module Auth: %r\n");
		exit;
	}

	err := auth->init();
	if(err != nil){
		sys->fprint(stderr, "Error: rstyxd: %s\n", err);
		exit;
	}

	sh = load Command "/dis/sh.dis";
	if(sh == nil){
		sys->fprint(stderr, "Error: rstyxd: can't load module sh: %r\n");
		exit;
	}

	user := user();
	kr = load Keyring Keyring->PATH;
	ai := kr->readauthinfo("/usr/"+user+"/keyring/default");
	#
	# let auth->server handle nil ai
	# if(ai == nil){
	#	sys->fprint(stderr, "Error: rstyxd: readauthinfo failed: %r\n");
	#	exit;
	# }
	#

	#do this before using auth
	if(sys->bind("#D", "/n/ssl", Sys->MREPL) < 0){
		sys->fprint(stderr, "Error: rstyxd: can't bind #D: %r\n");
		exit;
	}

	fd := ref Sys->FD;
	(fd, err) = auth->server(argv, ai, stdin);
	if(fd == nil){
		sys->fprint(stderr, "Error: rstyxd: %s\n", err);
		exit;
	}

	dorstyx(fd);

	sh->init(nil, nil);
}

dorstyx(fd: ref Sys->FD)
{
	sys->pctl(sys->FORKFD, fd.fd :: nil);

	args := readargs(fd);
	if(args == nil){
		sys->fprint(stderr, "exec: read args: %r\n");
		exit;
	}

	cmd := hd args;
	sys->fprint(stderr, "exec: cmd: %s\n",cmd);
	mod: Command;
	if(cmd == "sh")
		mod = sh;
	else{
		file := cmd + ".dis";
		mod = load Command file;
		if(mod == nil)
			mod = load Command "/dis/"+file;
		if(mod == nil) {
			sys->fprint(stderr, "exec: %s not found: %r\n", cmd);
			exit;
		}
	}

	sys->pctl(sys->FORKNS, nil);

	if(sys->mount(fd, "/n/client", sys->MREPL, "") < 0) {
		sys->fprint(stderr, "exec: mount: %r\n");
		exit;
	}

	if(sys->bind("/n/client/dev", "/dev", sys->MBEFORE) < 0) {
		sys->fprint(stderr, "exec: bind client: %r\n");
		exit;
	}

	fd = sys->open("/dev/cons", sys->OREAD);
	sys->dup(fd.fd, 0);
	fd = sys->open("/dev/cons", sys->OWRITE);
	sys->dup(fd.fd, 1);
	sys->dup(fd.fd, 2);

	mod->init(nil, args);
}

readargs(fd: ref Sys->FD): list of string
{
	buf := array[15] of byte;
	c := array[1] of byte;
	for(i:=0; ; i++){
		if(i>=len buf || sys->read(fd, c, 1)!=1)
			return nil;
		buf[i] = c[0];
		if(c[0] == byte '\n')
			break;
	}
	nb := int string buf;
	if(nb <= 0)
		return nil;
	args := readn(fd, nb);
	(nil, a) := sys->tokenize(string args[0:nb], " \n");
	return a;
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

user(): string
{
	sys = load Sys Sys->PATH;

	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil){
		sys->fprint(stderr, "Error: styxd: can't open /dev/user %r\n");
		exit;
	}

	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0){
		sys->fprint(stderr, "Error: styxd: failed to read /dev/user %r\n");
		exit;
	}

	return string buf[0:n]; 
}

