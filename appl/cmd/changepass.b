implement Changepass;

include "sys.m";
	sys: Sys;
	FD, Connection: import Sys;
	stderr: ref FD;

include "draw.m";
	Context: import Draw;

include "keyring.m";
	kr: Keyring;

include "security.m";
	rand: Random;

Changepass: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	kr = load Keyring Keyring->PATH;

	stderr := sys->fildes(2);

	# connect to signer
	argv0 := hd argv;
	argv = tl argv;
	if(argv != nil)
		signer := hd argv;
	else
		signer = "$SIGNER";

	(err, c) := connect(signer);
	if(err != nil){
		sys->fprint(stderr, "%s: %s\n", err);
		exit;
	}
		
}

connect(signer: string): (string, ref Sys->Connection)
{
	lc: Sys->Connection;
	c: ref Sys->Connection;
	ok: int;

	ssl := load SSL SSL->PATH;
	if(ssl == nil)
		return ("can't load ssl", nil);

	info := ref Keyring->Authinfo;

	if(dest == nil)
		dest = "$SIGNER";

	# get connection
	(ok, lc) = sys->dial("tcp!"+dest+"!infpassword", nil);
	if(ok < 0)
		return ("can't contact login daemon", nil);

	# push ssl, leave in clear mode for now
	(ok, c) = ssl->connect(lc.dfd, nil);
	if(ok < 0)
		return ("can't push ssl", nil);

	return (nil, c);
}
