implement Signer;

include "sys.m";
sys: Sys;

include "draw.m";

include "keyring.m";
kr: Keyring;
IPint: import kr;

include "security.m";
random: Random;

Signer: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

# size in bits of modulus for public keys
PKmodlen:		con 512;

# size in bits of modulus for diffie hellman
DHmodlen:		con 512;

stderr, stdin, stdout: ref Sys->FD;

init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	random = load Random Random->PATH;
	kr = load Keyring Keyring->PATH;

	stdin = sys->fildes(0);
	stdout = sys->fildes(1);
	stderr = sys->fildes(2);

	sys->pctl(Sys->FORKNS, nil);
	if(sys->chdir("/keydb") < 0){
		sys->fprint(stderr, "signer: no key database\n");
		exit;
	}

	err := sign();
	if(err != nil)
		sys->fprint(stderr, "signer: %s\n", err);
}

sign(): string
{
	info := signerkey("signerkey");
	if(info == nil)
		return "signer: can't read key";

	# send public part to client
	mypkbuf := array of byte kr->pktostr(kr->sktopk(info.mysk));
	kr->sendmsg(stdout, mypkbuf, len mypkbuf);
	alphabuf := array of byte info.alpha.iptob64();
	kr->sendmsg(stdout, alphabuf, len alphabuf);
	pbuf := array of byte info.p.iptob64();
	kr->sendmsg(stdout, pbuf, len pbuf);

	# get client's public key
	hisPKbuf := kr->getmsg(stdin);
	if(hisPKbuf == nil)
		return "signer: caller hung up";
	hisPK := kr->strtopk(string hisPKbuf);
	if(hisPK == nil)
		return "signer: illegal caller PK";

	# hash, sign, and blind
	state := kr->sha(hisPKbuf, len hisPKbuf, nil, nil);
	cert := kr->sign(info.mysk, 0, state, "sha");

# sanity clause
state = kr->sha(hisPKbuf, len hisPKbuf, nil, nil);
if(kr->verify(info.mypk, cert, state) == 0) sys->fprint(stderr, "bad certificate\n");

	certbuf := array of byte kr->certtostr(cert);
	blind := random->randombuf(random->ReallyRandom, len certbuf);
	for(i := 0; i < len blind; i++)
		certbuf[i] = certbuf[i] ^ blind[i];

	# sum PK's and blinded certificate
	state = kr->md5(mypkbuf, len mypkbuf, nil, nil);
	kr->md5(hisPKbuf, len hisPKbuf, nil, state);
	digest := array[Keyring->MD5dlen] of byte;
	kr->md5(certbuf, len certbuf, digest, state);

	# save sum and blinded cert in a file
	file := "signed/"+hisPK.owner;
	fd := sys->create(file, Sys->OWRITE, 8r600);
	if(fd == nil)
		return "signer: can't create "+file;
	if(kr->sendmsg(fd, blind, len blind) < 0 ||
	   kr->sendmsg(fd, digest, Keyring->MD5dlen) < 0){
		sys->remove(file);
		return "signer: can't write "+file;
	}

	# send blinded cert to client
	kr->sendmsg(stdout, certbuf, len certbuf);

	return nil;
}

signerkey(filename: string): ref Keyring->Authinfo
{
	if(len filename >= Sys->NAMELEN){
		filename = filename[0:Sys->NAMELEN-2];
		sys->fprint( stderr, 
		             "signer: warning: filename length > %d, truncated to %s\n", 
		             Sys->NAMELEN-1, filename);
	}

	info := kr->readauthinfo(filename);
	if(info != nil)
		return info;

	# generate a local key
	info = ref Keyring->Authinfo;
	info.mysk = kr->genSK("elgamal", "*", PKmodlen);
	info.mypk = kr->sktopk(info.mysk);
	info.spk = kr->sktopk(info.mysk);
	myPKbuf := array of byte kr->pktostr(info.mypk);
	state := kr->sha(myPKbuf, len myPKbuf, nil, nil);
	info.cert = kr->sign(info.mysk, 0, state, "sha");
	(info.alpha, info.p) = kr->dhparams(DHmodlen);

	if(kr->writeauthinfo(filename, info) < 0){
		sys->fprint(stderr, "signer: can't write signerkey file: %r\n");
		return nil;
	}

	return info;
}
