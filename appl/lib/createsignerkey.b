implement Createsignerkey;

include "sys.m";
sys: Sys;

include "daytime.m";
daytime: Daytime;

include "draw.m";

include "keyring.m";
kr: Keyring;

include "security.m";
random: Random;

# signer key never expires
SKexpire:       con 0;

# size in bits of modulus for public keys
PKmodlen:		con 512;

# size in bits of modulus for diffie hellman
DHmodlen:		con 512;

stderr: ref Sys->FD;

Createsignerkey: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

init(nil: ref Draw->Context, argv: list of string)
{
	err : string;

	sys = load Sys Sys->PATH;
	kr = load Keyring Keyring->PATH;

	stderr = sys->open("/dev/cons", Sys->OWRITE);

	argv = tl argv;

	if(argv == nil)
		usage();
	owner := hd argv;
	argv = tl argv;

    	expire := SKexpire;
	if(argv != nil){
		if(len hd argv == 8){
			 (err, expire) = checkdate(hd argv);
			 if(err != nil){
			        sys->fprint(stderr, "createsignerkey: %s\n", err);
					exit;
		     }
		     argv = tl argv;
		}
	}

	bits := PKmodlen;
	if(argv != nil){
		bits = int hd argv;
		argv = tl argv;
	    if(bits < 32 || bits > 4096){
		     sys->fprint(stderr, "createsignerkey: modulus must be in range of 32 to 4096 bits\n");
		     exit;
	    }
	}

	filename := "/keydb/signerkey";
	if(argv != nil){
		filename = hd argv;
		if(len filename >= Sys->NAMELEN){
			filename = filename[0:Sys->NAMELEN-2];
			sys->fprint( stderr, 
					"createsignerkey: warning: filename length > %d, truncated to %s\n", 
					Sys->NAMELEN-1, filename);
		}
	}

	# generate a local key
	info := ref Keyring->Authinfo;
	info.mysk = kr->genSK("elgamal", owner, bits);
	info.mypk = kr->sktopk(info.mysk);
	info.spk = kr->sktopk(info.mysk);
	myPKbuf := array of byte kr->pktostr(info.mypk);
	state := kr->sha(myPKbuf, len myPKbuf, nil, nil);
	info.cert = kr->sign(info.mysk, expire, state, "sha");
	(info.alpha, info.p) = kr->dhparams(DHmodlen);

	if(kr->writeauthinfo(filename, info) < 0)
		sys->fprint(stderr, "createsignerkey: can't write signerkey file: %r\n");
}

usage()
{
	sys->fprint(stderr, "usage: createsignerkey name-of-owner [expiration date [size-in-bits [file]]]\n");
	exit;
}

checkdate(word: string): (string, int)
{
    	daytime = load Daytime Daytime->PATH;
	now := daytime->now();

	tm := daytime->local(now);
	tm.sec = 59;
	tm.min = 59;
	tm.hour = 24;

	tm.mday = int word[0:2];
	if(tm.mday > 31 || tm.mday < 1)
		return ("!bad day of month", 0);

	tm.mon = int word[2:4] - 1;
	if(tm.mon > 11 || tm.mday < 0)
		return ("!bad month", 0);

	tm.year = int word[4:8] - 1900;
	if(tm.year < 70)
	    return ("!bad year", 0);

	newdate := daytime->tm2epoch(tm);
	if(newdate < now)
	    return ("!expiration date must be in the future", 0);

    return (nil, newdate);	
}
