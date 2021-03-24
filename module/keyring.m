#
#  security routines implemented in C
#
Keyring: module
{
	PATH:	con	"$Keyring";

	# infinite precision integers
	IPint: adt
	{
		x:	int;	# dummy for C compiler for runt.h

		# conversions
		iptob64:		fn(i: self ref IPint): string;
		b64toip:		fn(str: string): ref IPint;
		iptobytes:		fn(i: self ref IPint): array of byte;
		bytestoip:		fn(buf: array of byte): ref IPint;
 		bytestoip_sm:	fn(sign: int, mag: array of byte): ref IPint;		# sign < 0 => mag is 2's complement
		inttoip:		fn(i: int): ref IPint;
		iptoint:		fn(i: self ref IPint): int;
		iptostr:		fn(i: self ref IPint, base: int): string;
		strtoip:		fn(str: string, base: int): ref IPint;

		# create a random large integer using the accelerated generator
		random:		fn(minbits, maxbits: int): ref IPint;

		# operations
		bits:		fn(i: self ref IPint): int;
		expmod:	fn(base: self ref IPint, exp, mod: ref IPint): ref IPint;
		add:		fn(i1: self ref IPint, i2: ref IPint): ref IPint;
		sub:		fn(i1: self ref IPint, i2: ref IPint): ref IPint;
		neg:		fn(i: self ref IPint): ref IPint;
		mul:		fn(i1: self ref IPint, i2: ref IPint): ref IPint;
		div:		fn(i1: self ref IPint, i2: ref IPint): (ref IPint, ref IPint);
		eq:		fn(i1: self ref IPint, i2: ref IPint): int;
		cmp:		fn(i1: self ref IPint, i2: ref IPint): int;
	};

	# signature algorithm
	SigAlg: adt
	{
		name:	string;
		# C function pointers are hidden
	};
	
	# generic public key
	PK: adt
	{
		sa:	ref SigAlg;	# signature algorithm
		owner:	string;		# owner's name
		# key and system parameters are hidden
	};
	
	# generic secret key
	SK: adt
	{
		sa:	ref SigAlg;	# signature algorithm
		owner:	string;		# owner's name
		# key and system parameters are hidden
	};

	# generic certificate
	Certificate: adt
	{
		sa:	ref SigAlg;	# signature algorithm
		ha:	string;		# hash algorithm
		signer:	string;		# name of signer
		exp:	int;		# expiration date
		# actual signature is hidden
	};

	# state held while creating digests
	DigestState: adt
	{
		x:	int;		# dummy for C compiler for runt.h
		# all the state is hidden
	};

	# expanded DES key + state for chaining
	DESstate: adt
	{
		x:	int;		# dummy for C compiler for runt.h
		# all the state is hidden
	};

	# authentication info
	Authinfo: adt
	{
		mysk:	ref SK;			# my private key
		mypk:	ref PK;			# my public key
		cert:	ref Certificate;	# signature of my public key
		spk:	ref PK;			# signers public key
		alpha:	ref IPint;		# diffie helman parameters
		p:	ref IPint;
	};

	# convert types to byte strings
	certtostr: fn (c: ref Certificate): string;
	pktostr: fn (pk: ref PK): string;
	sktostr: fn (sk: ref SK): string;

	# parse byte strings into types
	strtocert: fn (s: string): ref Certificate;
	strtopk: fn (s: string): ref PK;
	strtosk: fn (s: string): ref SK;

	# create and verify signatures
	sign: fn (sk: ref SK, exp: int, state: ref DigestState, ha: string):
		ref Certificate;
	verify: fn (pk: ref PK, cert: ref Certificate, state: ref DigestState):
		int;

	# generate keys
	genSK: fn (algname, owner: string, length: int): ref SK; 
	genSKfromPK: fn (pk: ref PK, owner: string): ref SK;
	sktopk: fn (sk: ref SK): ref PK;

	# digests
	cloneDigestState: fn(state: ref DigestState): ref DigestState;
	sha: fn(buf: array of byte, n: int, digest: array of byte, state: ref DigestState):
		ref DigestState;
	md4: fn(buf: array of byte, n: int, digest: array of byte, state: ref DigestState):
		ref DigestState;
	md5: fn(buf: array of byte, n: int, digest: array of byte, state: ref DigestState):
		ref DigestState;

	# DES interfaces
	Encrypt:	con 0;
	Decrypt:	con 1;
	dessetup: fn(key: array of byte, ivec: array of byte): ref DESstate;
	desecb: fn(state: ref DESstate, buf: array of byte, n: int, direction: int);
	descbc: fn(state: ref DESstate, buf: array of byte, n: int, direction: int);

	# create an alpha and p for diffie helman exchanges
	dhparams: fn(nbits: int): (ref IPint, ref IPint);

	# comm link authentication is symetric
	auth: fn(fd: ref Sys->FD, info: ref Authinfo, setid: int): (string, array of byte);

	# auth io
	readauthinfo: fn(filename: string): ref Authinfo;
	writeauthinfo: fn(filename: string, info: ref Authinfo): int;

	# message io on a delimited connection (ssl for example)
	#  messages > 4096 bytes are truncated
	#  errors > 64 bytes are truncated
	# getstring and getbytearray return (result, error).
	getstring: fn(fd: ref Sys->FD): (string, string);
	putstring: fn(fd: ref Sys->FD, s: string): int;
	getbytearray: fn(fd: ref Sys->FD): (array of byte, string);
	putbytearray: fn(fd: ref Sys->FD, a: array of byte, n: int): int;
	puterror: fn(fd: ref Sys->FD, s: string): int;

	# to send and receive messages when ssl isn't pushed
	getmsg: fn(fd: ref Sys->FD): array of byte;
	sendmsg: fn(fd: ref Sys->FD, buf: array of byte, n: int): int;

	# algorithms
	DEScbc:		con 0;
	DESecb:		con 1;
	SHA:		con 2;
	MD5:		con 3;
	MD4:		con 4;

	SHAdlen:	con 20;
	MD5dlen:	con 16;
	MD4dlen:	con 16;
};
