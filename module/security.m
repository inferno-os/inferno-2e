#
#  security routines implemented in limbo
#


Virgil: module
{
	PATH:	con "/dis/lib/virgil.dis";

	virgil:	fn(args: list of string): string;
};

Random: module
{
	PATH:	con "/dis/lib/random.dis";

	ReallyRandom:	con 0;
	NotQuiteRandom:	con 1;

	randomint: fn(which: int): int;
	randombuf: fn(which, n: int): array of byte;
};

#
#  read and write password entries in the password file
#
Password: module
{
	PATH:	con "/dis/lib/password.dis";

	PW: adt {
		id:	string;			# user id
		pw:	array of byte;	# password
		expire:	int;		# expiration time (epoch seconds)
		other:	string;		# about the account	
	};

	get: fn(id: string): ref PW;
	put: fn(pass: ref PW): int;

	PWlen: con 4;
};

#
#  secure socket layer emulator
#
SSL: module
{
	# Caller is expected to bind the security device to /n/ssl.

	PATH:	con "/dis/lib/ssl.dis";

	connect: fn(fd: ref Sys->FD): (string, ref Sys->Connection);
	secret: fn(c: ref Sys->Connection, secretin, secretout: array of byte): string;
};


#
#  Encrypted Key Exchange protocol
#
Login: module 
{
	PATH:	con "/dis/lib/login.dis";

	DHrandlen:	con 512; # size of random number for dh exponent
	PKmodlen:	con 512; # size in bits of modulus for public keys
	DHmodlen:	con 512; # size in bits of modulus for diffie hellman

	init: fn(): string;
	chello: fn(id, agreefile: string, c: ref Sys->Connection): 
		(ref Sys->Connection, string);
	ckeyx: fn(id, passwd: string, c: ref Sys->Connection): 
		(ref Keyring->Authinfo, string);
	shello: fn(agreefile: string, c: ref Sys->Connection): 
		(ref Password->PW, string);
	skeyx: fn(pw: ref Password->PW, info: ref Keyring->Authinfo, c: ref Sys->Connection): 
		(ref Keyring->Authinfo, string);
	defaultsigner: fn(): string;
	signerkey: fn(file: string): (ref Keyring->Authinfo, string);
};

#
#  Station To Station protocol
#
Auth: module
{
	PATH:	con "/dis/lib/auth.dis";

	# level of security
	NOAUTH:			con "noauth";
	NOSSL:			con "nossl";
	CLEAR:			con "clear";
	SHA:			con "sha";
	MD5:			con "md5";
	RC4:			con "rc4";				# DEPRECATED -- same as RC4_40 ("hybrid" kernel)
	SHA_RC4:		con "sha/rc4";			# DEPRECATED -- use SHA_RC4_X
	SHA_DESCBC:		con "sha/descbc";		# DEPRECATED -- use SHA_DES_56_CBC
	SHA_DESECB:		con "sha/desecb";		# DEPRECATED -- use SHA_DES_56_ECB
	MD5_RC4:		con "md5/rc4";			# DEPRECATED -- use MD5_RC4_X
	MD5_DESCBC:		con "md5/descbc";		# DEPRECATED -- use MD5_DES_56_CBC
	MD5_DESECB:		con "md5/desecb";		# DEPRECATED -- use MD5_DES_56_ECB
	
	# new algorithms for "hybrid kernel"
	RC4_256:		con "rc4_256";			# 256-bit RC4 (domestic kernel only)
	RC4_128:		con "rc4_128";			# 128-bit RC4 (domestic kernel only)
	RC4_40: 		con "rc4_40";			# 40-bit  RC4
	DES_56_CBC:		con "des_56_cbc";		# 56-bit  DES (CBC mode, domestic kernel only)
	DES_56_ECB:		con "des_56_ecb";		# 56-bit  DES (ECB mode, domestic kernel only)
	DES_40_CBC:		con "des_40_cbc";		# 40-bit  DES (CBC mode)
	DES_40_ECB:		con "des_40_ecb";		# 40-bit  DES (ECB mode)
	SHA_RC4_256: 	con "sha/rc4_256";		# domestic kernel only
	SHA_RC4_128: 	con "sha/rc4_128";		# domestic kernel only
	SHA_RC4_40: 	con "sha/rc4_40";
	SHA_DES_56_CBC:	con "sha/des_56_cbc";	# domestic kernel only
	SHA_DES_56_ECB:	con "sha/des_56_ecb";	# domestic kernel only
	SHA_DES_40_CBC:	con "sha/des_40_cbc";
	SHA_DES_40_ECB:	con "sha/des_40_ecb";
	MD5_RC4_256:	con "md5/rc4_256";		# domestic kernel only
	MD5_RC4_128:	con "md5/rc4_128";		# domestic kernel only
	MD5_RC4_40:		con "md5/rc4_40";
	MD5_DES_56_CBC:	con "md5/des_56_cbc";	# domestic kernel only
	MD5_DES_56_ECB:	con "md5/des_56_ecb";	# domestic kernel only
	MD5_DES_40_CBC:	con "md5/des_40_cbc";
	MD5_DES_40_ECB:	con "md5/des_40_ecb";


	init: fn(): string;
	server: fn(algs: list of string, ai: ref Keyring->Authinfo, fd: ref Sys->FD): (ref Sys->FD, string);
	client: fn(alg: string, ai: ref Keyring->Authinfo, fd: ref Sys->FD): (ref Sys->FD, string);
        # Required to invoke server alg without changing user id
	serverwid : fn(algs: list of string, ai: ref Keyring->Authinfo, fd: ref Sys->FD, wid : int): (ref Sys->FD, string);
};

