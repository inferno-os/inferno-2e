implement Styxd;

include "sys.m";
	sys: Sys;
	stdin, stderr: ref Sys->FD;

include "keyring.m";
	kr: Keyring;

include "security.m";
	auth: Auth;
	ssl: SSL;

include "draw.m";

include "sd.m";

Styxd: module 
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

# argv is a list of Inferno supported algorithms such as
#
#		Auth->NOAUTH ::
#		Auth->NOSSL :: 
#		Auth->CLEAR :: 
#		Auth->SHA :: 
#		Auth->MD5 :: 
#		Auth->RC4 ::
#		Auth->SHA_RC4 ::
#		Auth->MD5_RC4 ::
#		nil;
#
init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stdin = sys->fildes(0);
	stderr = sys->open("/dev/cons", sys->OWRITE);

	sd	: SD;
	if(hd argv == "-m"){
					argv = tl argv;
		sdfile := hd argv;	argv = tl argv;
		if((sd  = load SD sdfile) == nil){
			sys->fprint(stderr,
			"Error: styxd: can't load SD module: %s: %r\n",
			sdfile);
			exit;
		}
	}

	auth = load Auth Auth->PATH;
	if(auth == nil){
		sys->fprint(stderr, "Error: styxd: can't load module Auth\n");
		exit;
	}


	error := auth->init();
	if(error != nil){
		sys->fprint(stderr, "Error: styxd: %s\n", error);
		exit;
	}

	user := user();

	kr = load Keyring Keyring->PATH;
	ai := kr->readauthinfo("/usr/"+user+"/keyring/default");
	#
	# let auth->server handle nil ai
	# if(ai == nil){
	#	sys->fprint(stderr, "Error: styxd: readauthinfo failed: %r\n");
	#	exit;
	# }
	#

	#do this before using auth
	if(sys->bind("#D", "/n/ssl", Sys->MREPL) < 0){
		sys->fprint(stderr, "Error: can't bind #D: %r\n");
		exit;
	}

	if(argv == nil){
		sys->fprint(stderr, "Error: styxd: no algorithm list\n");
		exit;
	}

	(fd, info_or_err) := auth->server(argv, ai, stdin);
	if(fd == nil ){
		sys->fprint(stderr, "Error: styxd: %s\n", info_or_err);
		exit;
	}

	if(sd == nil)
		sys->pctl(sys->FORKNS, nil);
	else{ 	
		if((emsg :=sd->init(info_or_err)) != nil){
			sys->fprint(stderr,
				"Error: styxd: setup(namespace): %s\n", emsg);
				exit;
		}
	}

	if(sys->export(fd, sys->EXPASYNC) < 0)
		sys->fprint(stderr, "Error: styxd: file export %r\n");
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

