# Inferno authentication protocol
implement Auth;

include "sys.m";
	sys: Sys;

include "keyring.m";
	kr: Keyring;

include "security.m";
	ssl: SSL;

# load needed modules
init(): string
{
	if(sys == nil)
		sys = load Sys Sys->PATH;

	if(kr == nil)
		kr = load Keyring Keyring->PATH;

	ssl = load SSL SSL->PATH;
	if(ssl == nil)
		return sys->sprint("can't load ssl: %r");

	return nil;	
}

server(algs: list of string, ai: ref Keyring->Authinfo, fd: ref Sys->FD): (ref Sys->FD, string)
{
  return serverwid(algs, ai, fd, 1);
}

# Required to invoke server alg with/out changing user id
serverwid(algs: list of string, ai: ref Keyring->Authinfo, fd: ref Sys->FD, wid : int): (ref Sys->FD, string)
{
	# doing mutual authentication
	(id_or_err, secret) := kr->auth(fd, ai, wid); 

	# get algorithm from client
	algbuf := string kr->getmsg(fd);
	if(algbuf == nil){
		return (nil, sys->sprint("auth server: can't get alg: %r"));
	}

	# check if the client algorithm is in the server algorithm list
	# client algorithm ::= ident ( ident)*
	# where ident is defined by /services/server/config
	# build an algorithm string to initialize SSL with
	alg := "";
	(nil, cAlgList) := sys->tokenize(string algbuf, " ");		# get the client algs as a list
	while (cAlgList != nil) {
		calg := hd cAlgList;
		sAlgList := algs;								# initialize a fresh server alg list
		while (sAlgList != nil) {						# check each component of cAlgList against sAlgList
			if (hd sAlgList == calg)
				break;
			sAlgList = tl sAlgList;
		}
		if (sAlgList == nil)
			return (nil, "auth server: unknown client algorithm: " + calg);

		alg += calg + " ";
		cAlgList = tl cAlgList;
	}
	alg = alg[0:len alg - 1];

	# don't go further if server supports no authentication	# WE SHOULD GET RID OF THIS OPTION
	if(alg == Auth->NOAUTH){
		return (fd, nil);
	}

	if(secret == nil)
		return (nil, "auth server: " + id_or_err);

	# don't go further if no Authinfo
	if(ai == nil){
		return (nil, "auth server: no server authinfo");
	}

	# don't push ssl if server supports nossl
	if(alg == Auth->NOSSL){
		return (fd, id_or_err + " with security: nossl");
	}

	# push ssl and turn on algorithms
	(c, err) := pushssl(fd, secret, secret, alg);
	if(c == nil){
		return (nil, "auth server: " + err);
	}
	else{
		return (c, id_or_err + " with security: " + alg);
	}
}

# negotiate level of security by sending alg 
client(alg: string, ai: ref Keyring->Authinfo, fd: ref Sys->FD): (ref Sys->FD, string)
{
	# mutual authentication
	(id_or_err, secret) := kr->auth(fd, ai, 0);

	# sending algorithm
	buf := array of byte alg;
	if(kr->sendmsg(fd, buf, len buf) < 0)
		return (nil, sys->sprint("auth client: send alg failed: %r"));

	# negotiate connection with no authentication
	if(alg == Auth->NOAUTH){
		return (fd, nil);
	}

	if(secret == nil)
		return (nil, "auth client: " + id_or_err);

	# don't push ssl if server supports no ssl connection
	if(alg == Auth->NOSSL){
		return (fd, id_or_err + " with security: nossl");
	}

	# push ssl and turn on algorithm
	(c, err) := pushssl(fd, secret, secret, alg);
	if(c == nil)
		return (nil, "auth client: " + err);

	return (c, id_or_err + " with security: " + alg);
}

# push an SSLv2 Record Layer onto the fd
pushssl (fd: ref Sys->FD, secretin, secretout: array of byte, alg: string): (ref Sys->FD, string)
{
	if(sys->bind("#D", "/n/ssl", Sys->MREPL) < 0)
		return (nil, sys->sprint("can't bind #D: %r"));

	(err, c) := ssl->connect(fd);
	if(err != nil)
		return (nil, "can't connect ssl: " + err);

	err = ssl->secret(c, secretin, secretout);
	if(err != nil)
		return (nil, "can't write secret: " + err);

	if(sys->fprint(c.cfd, "alg %s", alg) < 0)
		return (nil, sys->sprint("can't push algorithm %s: %r", alg));

	return (c.dfd, nil);
}
