# Inferno Encrypt Key Exchange Protocol
implement Login;

include "sys.m";
	sys: Sys;

include "daytime.m";
	daytime: Daytime;

include "draw.m";
	draw: Draw;
	Context: import draw;

include "keyring.m";
	kr: Keyring;
	IPint: import kr;

include "security.m";
	rand: Random;
	ssl: SSL;
	password: Password;

include "string.m";

# load needed modules
init(): string
{
	if(sys == nil)
		sys = load Sys Sys->PATH;

	if(kr == nil)
		kr = load Keyring Keyring->PATH;

	ssl = load SSL SSL->PATH;
	if(ssl == nil)
		return sys->sprint("Login Init: can't load module ssl %r");

	rand = load Random Random->PATH;
	if(rand == nil)
		return sys->sprint("Login Init: can't load module Random %r");

	password = load Password Password->PATH;
	if(password == nil)
		return sys->sprint("Login Init: can't load module Password %r");

	daytime = load Daytime Daytime->PATH;
	if(daytime == nil) 
		return sys->sprint("Login Init: can't load module Daytime: %r");

	return nil;
}

#Our own interface into the Cs module so we can access kvopen & kvmap
Cs: module
{
	init:   fn(nil: ref Context, nil: list of string);
	kvopen:	fn(dbfile: string): int;
	kvmap:	fn(key: string): string;
};
cs: Cs;


# get default signer server name
defaultsigner(): string
{
	cs = load Cs "/dis/lib/cs.dis";
	if(cs == nil) 
		return nil;

	cs->kvopen("/services/cs/db");

	return cs->kvmap("$SIGNER");
}


# retrieve signer's authinfo
signerkey(filename: string): (ref Keyring->Authinfo, string)
{   
	err: string;

	if(len filename >= Sys->NAMELEN){
		filename = filename[0:Sys->NAMELEN-2];
		err = sys->sprint("Warning: Login signerkey: filename length > %d, truncated to %s", 
			Sys->NAMELEN-1, filename);
	}

	info := kr->readauthinfo(filename);
	if(info == nil)
		return (nil, sys->sprint("Error: Login signerkey: readauthinfo %r"));
	
	# validate signer key
	now := daytime->now();
	if(info.cert.exp != 0 && info.cert.exp < now)
		return (nil, sys->sprint("Login Signerkey: key expired"));

	return (info, err);
}


# client handshake
chello(username, agreefile: string, c: ref Sys->Connection): (ref Sys->Connection, string)
{
	ok: int;
	s, err: string;

	# send name
	if(kr->putstring(c.dfd, username) < 0)
		return (nil, sys->sprint("Login Client Hello: send user name: %r"));

	# get ack
	(s, err) = kr->getstring(c.dfd);
	if(err != nil)
		return (nil, "Login Client Hello: get ack failed: " + err);
	if(s != username)
		return (nil, "Login Client Hello: " + s );

	# get and save agreement
	err = getfile(agreefile, c);
	if(err != nil)
		return (nil, "Login Client Hello: " + err);

	return (c, nil);
}

# client key exchange
ckeyx(id, passwd: string, c: ref Sys->Connection): (ref Keyring->Authinfo, string)
{
	err, s : string;
	info := ref Keyring->Authinfo;

	# create and send an initialization vector
	ivec := rand->randombuf(rand->ReallyRandom, 8);
	if(kr->putbytearray(c.dfd, ivec, len ivec) < 0)
		return (nil, sys->sprint("Login Client: can't send initialization vector: %r"));

	# start encrypting
	pwbuf := array of byte passwd;
	digest := array[Keyring->SHAdlen] of byte;
	kr->sha(pwbuf, len pwbuf, digest, nil);
	pwbuf = array[8] of byte;
	for(i := 0; i < 8; i++)
		pwbuf[i] = digest[i] ^ digest[8+i];
	for(i = 0; i < 4; i++)
		pwbuf[i] ^= digest[16+i];
	for(i = 0; i < 8; i++)
		pwbuf[i] ^= ivec[i];
	err = ssl->secret(c, pwbuf, pwbuf);
	if(err != nil)
		return (nil, "Login Client: " + err);
	if(sys->fprint(c.cfd, "alg rc4") < 0)
		return (nil, "Login Client: can't push alg rc4");
	#if(sys->fprint(c.cfd, "alg desebc") < 0)
	#	return (nil, "Login Client: can't push alg desebc");

	# get P(alpha**r0 mod p)
	(s, err) = kr->getstring(c.dfd);
	if(err != nil){
		if(err == "failure") # calculated secret is wrong
			return (nil, "Login Client: can't get (alpha**r0 mod p)");
		return (nil, "Login Client: " + err);
	}

	# stop encrypting
	if(sys->fprint(c.cfd, "alg clear") < 0)
		return (nil, "Login Client: can't push alg clear");

	# get alpha, p
	alphar0 := IPint.b64toip(s);
	(s, err) = kr->getstring(c.dfd);
	if(err != nil){
		if(err == "failure")
			return (nil, "Login Client: can't get alpha");
		return (nil, "Login Client: " + err);
	}
	info.alpha = IPint.b64toip(s);
	(s, err) = kr->getstring(c.dfd);
	if(err != nil){
		if(err == "failure")
			return (nil, "Login Client: can't get p");
		return (nil, "Login Client: " + err);
	}
	info.p = IPint.b64toip(s);

	# sanity check
	bits := info.p.bits();
	abits := info.alpha.bits();
	if(abits > bits || abits < 2)
		return (nil, "Login Client: bogus diffie hellman constants");

	# generate our random diffie hellman part
	r1 := kr->IPint.random(bits/4, bits);
	alphar1 := info.alpha.expmod(r1, info.p);

	# send alpha**r1 mod p
	if(kr->putstring(c.dfd, alphar1.iptob64()) < 0)
		return (nil, "Login Client: can't send (alpha**r1 mod p)");

	# compute alpha**(r0*r1) mod p
	alphar0r1 := alphar0.expmod(r1, info.p);

	# turn on digesting
	secret := alphar0r1.iptobytes();
	err = ssl->secret(c, secret, secret);
	if(err != nil)
		return (nil, "Login Client: " + err);
	if(sys->fprint(c.cfd, "alg sha") < 0)
		return (nil, "Login Client: can't push alg sha");

	# get signer's public key
	(s, err) = kr->getstring(c.dfd);
	if(err != nil)
		return (nil, "Login Client: can't get signer's public key: " + err);

	info.spk = kr->strtopk(s);

	# generate a key pair
	info.mysk = kr->genSKfromPK(info.spk, id);
	info.mypk = kr->sktopk(info.mysk);

	# send my public key
	if(kr->putstring(c.dfd, kr->pktostr(info.mypk)) < 0)
		return (nil, sys->sprint("Login Client: can't send my public: %r"));

	# get my certificate
	(s, err) = kr->getstring(c.dfd);
	if(err != nil)
		return (nil, "Login Client: can't get certificate: " + err);

	info.cert = kr->strtocert(s);

	return(info, nil);
}

# server handshake
shello(agreefile: string, c: ref Sys->Connection): (ref Password->PW, string)
{
	# get user name
	(s, err) := kr->getstring(c.dfd);
	if(err != nil)
		return (nil, "Login Server Hello: can't get user name from client: " + err);
	name := s;

	# check user account at server side
	pw := password->get(name);
	if(pw == nil){
		if(kr->puterror(c.dfd, "unknown user name") < 0)
			return (nil, sys->sprint("Login Server Hello: send error message failed: %r"));
	    return (nil, "Login Server Hello: "+ s +" account does not exist");
	}

	now := daytime->now();
	if(pw.expire < now) 
	    return (nil, "Login Server Hello: "+ s +" account expired");

	# send ack
	if(kr->putstring(c.dfd, name) < 0)
		return (nil, sys->sprint("Login Server Hello: send ack failed: %r"));
	
	# send agreement
	err = putfile(agreefile, c);
	if(err != nil)
		return (nil, "Login Server Hello: "+err);

	# get ack
	(s, err) = kr->getstring(c.dfd);
	if(err != nil)
		return (nil, "Login Server Hello: can't get client ack: " + err);
	if(s != "agree")
		return (nil, "Login Server Hello: client does not accept agreement");

	return (pw, nil);
}

# Encrypt Key Exchange server part
# info is the signerkey
skeyx(pw: ref Password->PW, info: ref Keyring->Authinfo, c: ref Sys->Connection)
	: (ref Keyring->Authinfo, string)
{
	# get initialization vector
	(ivec, err) := kr->getbytearray(c.dfd);
	if(err != nil)
		return (nil, "Login Server: can't get initialization vector: " + err);

	# generate our random diffie hellman part
	bits := info.p.bits();
	r0 := kr->IPint.random(bits/4, bits);

	# generate alpha0 = alpha**r0 mod p
	alphar0 := info.alpha.expmod(r0, info.p);

	# start encrypting
	pwbuf := array[8] of byte;
	for(i := 0; i < 8; i++)
		pwbuf[i] = pw.pw[i] ^ pw.pw[8+i];
	for(i = 0; i < 4; i++)
		pwbuf[i] ^= pw.pw[16+i];
	for(i = 0; i < 8; i++)
		pwbuf[i] ^= ivec[i];
	err = ssl->secret(c, pwbuf, pwbuf);
	if(err != nil)
		return (nil, "Login Server: " + err);
	if(sys->fprint(c.cfd, "alg rc4") < 0)
		return (nil, "Login Server: can't push alg rc4");
	#if(sys->fprint(c.cfd, "alg desebc") < 0)
	#	return (nil, "Login Server: can't push alg desebc");

	# send P(alpha**r0 mod p)
	if(kr->putstring(c.dfd, alphar0.iptob64()) < 0)
		return (nil, sys->sprint("Login Server: can't send (alpha**r0 mod p): %r"));

	# stop encrypting
	if(sys->fprint(c.cfd, "alg clear") < 0)
		return (nil, "Login Server: can't push alg clear");

	# send alpha, p
	if(kr->putstring(c.dfd, info.alpha.iptob64()) < 0)
		return (nil, sys->sprint("Login Server: can't send alpha: %r"));
	if(kr->putstring(c.dfd, info.p.iptob64()) < 0)
		return (nil, sys->sprint("Login Server: can't send p: %r"));

	# get alpha**r1 mod p
	s : string;
	(s, err) = kr->getstring(c.dfd);
	if(err != nil)
		return (nil, "Login Server: can't get (alpha**r1 mod p): " + err);
	alphar1 := kr->IPint.b64toip(s);

	# compute alpha**(r0*r1) mod p
	alphar0r1 := alphar1.expmod(r0, info.p);

	# turn on digesting
	secret := alphar0r1.iptobytes();
	err = ssl->secret(c, secret, secret);
	if(err != nil)
		return (nil, "Login Server: " + err);
	if(sys->fprint(c.cfd, "alg sha") < 0)
		return (nil, "Login Server: can't push alg sha");

	# send our public key
	if(kr->putstring(c.dfd, kr->pktostr(kr->sktopk(info.mysk))) < 0)
		return (nil, sys->sprint("Login Server: can't send signer's public key: %r"));

	# get his public key
	(s, err) = kr->getstring(c.dfd);
	if(err != nil)
		return (nil, "Login Client: can't get client public key" + err);
	hisPKbuf := array of byte s;
	hisPK := kr->strtopk(s);

	# sign certificate
	hisinfo := ref Keyring->Authinfo;
	hisinfo.mysk = kr->strtosk("\n \n \n ");
	hisinfo.mypk = hisPK;
	hisinfo.spk = info.spk;
	hisinfo.alpha = info.alpha;
	hisinfo.p = info.p;
	state := kr->sha(hisPKbuf, len hisPKbuf, nil, nil);
	hisinfo.cert = kr->sign(info.mysk, pw.expire, state, "sha");

	# send certificate
	if(kr->putstring(c.dfd, kr->certtostr(hisinfo.cert)) < 0)
		return (nil, sys->sprint("Login Server: can't send certificate to client: %r")); 
    
	return (hisinfo, nil);
}

getfile(file: string, c: ref Sys->Connection): string
{
	fd := sys->open(file, Sys->OWRITE | Sys->OTRUNC);
	if(fd == nil)
		fd = sys->create(file, Sys->OWRITE, 8r777);
	if(fd == nil)
		return "can't open or create " + file;

	for(;;){
		(s, err) := kr->getstring(c.dfd);
		if(err != nil) 
		      return "can't get string from remote: " + err;
	   	if(s == "done") 
		      break;
			  n := sys->write(fd, array of byte s, len s);		
		if(n < 0)
		      return "can't write " + file;
	}

	return nil;
}

putfile(file: string, c: ref Sys->Connection): string
{
	fd := sys->open(file, sys->OREAD);
	if(fd == nil)
		return "can't open " + file;

	buf := array[1024] of byte;
	for(;;){
		n := sys->read(fd, buf, len buf);
		if(n <= 0){
			kr->putstring(c.dfd, "done");
			break;
		}
		for(i := 0; i < n; i++){
			if(buf[i] == byte '\r') buf[i] = byte ' ';
		}
		if(kr->putstring(c.dfd, string buf[0:n]) < 0)
			return "can't send " + file;
	}

	return nil;
}
