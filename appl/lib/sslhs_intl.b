# SSL handshake protocol - exportable version
implement SSLHS;

include "sys.m";
	sys: Sys;

include "keyring.m";
	keyring: Keyring;
	IPint, SHA, MD5, DEScbc, DigestState: import keyring;
	RC4 : con MD5+1;

include "draw.m";

include "security.m";
	random: Random;
	ssl: SSL;

include "daytime.m";
	daytime: Daytime;

include "asn1.m";
	asn1: ASN1;
	Elem, Tag, Value, Oid: import asn1;

include "sslhs.m";

Session: adt {
	peer: string;
	ctime: int;
	cipher: int;
	sid: array of byte;
	masterkey: array of byte;
	keyarg: array of byte;
	connid: array of byte;
};

ClientMsg: adt {
	pick {
	Error =>
		code: int;
	Hello =>
		version: int;
		ciphers: array of int; # indices into cipher_kind
		sid: array of byte;
		challenge: array of byte;
	MasterKey =>
		cipher: int;
		clearkey: array of byte;
		encryptedkey: array of byte;
		keyarg: array of byte;
	Certificate =>
		certtype: int;
		cert: array of byte;
		response: array of byte;
	Finished =>
		connid: array of byte;
	}
};

ServerMsg: adt {
	pick {
	Error =>
		code: int;
	Hello =>
		sidhit: int;
		certtype: int;
		version: int;
		cert: array of byte;
		ciphers: array of int;
		connid: array of byte;
	Verify =>
		challenge: array of byte;
	RequestCertificate =>
		authtype: int;
		certchallenge: array of byte;
	Finished =>
		sid: array of byte;
	}
};

RSAKey : adt {
	modulus : ref IPint;
	modlen: int;
	exponent: ref IPint;
};


# protocol version codes
SSL_CLIENT_VERSION : con 2;
SSL_SERVER_VERSION : con 2;

# protocol message codes
SSL_MT_ERROR : con 0;
SSL_MT_CLIENT_HELLO : con 1;
SSL_MT_CLIENT_MASTER_KEY : con 2;
SSL_MT_CLIENT_FINISHED : con 3;
SSL_MT_SERVER_HELLO : con 4;
SSL_MT_SERVER_VERIFY : con 5;
SSL_MT_SERVER_FINISHED : con 6;
SSL_MT_REQUEST_CERTIFICATE : con 7;
SSL_MT_CLIENT_CERTIFICATE : con 8;

# error codes
SSL_PE_NO_CIPHER : con 1;
SSL_PE_NO_CERTIFICATE : con 2;
SSL_PE_BAD_CERTIFICATE : con 4;
SSL_PE_UNSUPPORTED_CERTIFICATE_TYPE : con 6;

# CIPHER-KIND codes used in the 
# CLIENT-HELLO and SERVER-HELLO messages.
SSL_CK_RC4_128_WITH_MD5,
	SSL_CK_RC4_128_EXPORT40_WITH_MD5,
	SSL_CK_RC2_128_CBC_WITH_MD5,
	SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5,
	SSL_CK_IDEA_128_CBC_WITH_MD5,
	SSL_CK_DES_64_CBC_WITH_MD5,
	SSL_CK_DES_192_EDE3_CBC_WITH_MD5,
	SSL_CK_NULL_WITH_MD5	# v3, for testing
	: con iota;

# 3-byte sequences corresponding to above
# For SSL_CK_... entries (cipher kinds)
cipher_kind := array[] of {
SSL_CK_RC4_128_WITH_MD5 => array[] of {byte 1, byte 0, byte 16r80},
SSL_CK_RC4_128_EXPORT40_WITH_MD5 => array[] of {byte 2, byte 0, byte 16r80},
SSL_CK_RC2_128_CBC_WITH_MD5 => array[] of {byte 3, byte 0, byte 16r80},
SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5 => array[] of {byte 4, byte 0, byte 16r80},
SSL_CK_IDEA_128_CBC_WITH_MD5 => array[] of {byte 5, byte 0, byte 16r80},
SSL_CK_DES_64_CBC_WITH_MD5 => array[] of {byte 6, byte 0, byte 16r40},
SSL_CK_DES_192_EDE3_CBC_WITH_MD5 => array[] of {byte 7, byte 0, byte 16rC0},
SSL_CK_NULL_WITH_MD5 => array[] of {byte 0, byte 0, byte 0},
};

CipherInfo : adt {
	keylen: int;	# in bytes
	clearlen: int;	# in bytes
	keyarglen: int;	# in bytes
	cryptalg:int;	# RC4 or DEScbc or -1
	hashalg:int;	# SHA or MD5
};
cipher_info := array[] of {
SSL_CK_RC4_128_WITH_MD5 => CipherInfo(16, 0, 0, RC4, MD5),
SSL_CK_RC4_128_EXPORT40_WITH_MD5 => (16, 16-5, 0, RC4, MD5),
SSL_CK_RC2_128_CBC_WITH_MD5 => (16, 0, 8, -1, MD5),
SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5 => (16, 16-5, 8, -1, MD5),
SSL_CK_IDEA_128_CBC_WITH_MD5 => (16, 0, 8, -1, MD5),
SSL_CK_DES_64_CBC_WITH_MD5 => (8, 0, 8, DEScbc, MD5),
SSL_CK_DES_192_EDE3_CBC_WITH_MD5 => (24, 0, 8, DEScbc, MD5),
SSL_CK_NULL_WITH_MD5 => (0, 0, 0, -1, MD5)
};

# certificate type codes
SSL_CT_X509_CERTIFICATE : con 1;

# authentication type codes in REQUEST-CERTIFICATE
SSL_AT_MD5_WITH_RSA_ENCRYPTION : con 1;

# Cipher-kinds we can handle, in order of preference
inferno_ciphers := array[] of {
	#SSL_CK_RC4_128_WITH_MD5,
	#SSL_CK_DES_64_CBC_WITH_MD5,
	SSL_CK_RC4_128_EXPORT40_WITH_MD5
};

CertX509: adt {
	serial: int;
	issuer: string;
	validity_start: string;
	validity_end: string;
	subject: string;
	publickey_alg: int;
	publickey: array of byte;
	signature_alg: int;
	signature: array of byte;
};

#  Some algorithm object-ids
ALG_rsaEncryption,
ALG_md2WithRSAEncryption,
ALG_md4WithRSAEncryption,
ALG_md5WithRSAEncryption: con iota;

alg_oid_tab := array[] of {
ALG_rsaEncryption => Oid(array[] of {1, 2, 840, 113549, 1, 1, 1}),
ALG_md2WithRSAEncryption => Oid(array[] of {1, 2, 840, 113549, 1, 1, 2}),
ALG_md4WithRSAEncryption => Oid(array[] of {1, 2, 840, 113549, 1, 1, 3}),
ALG_md5WithRSAEncryption => Oid(array[] of {1, 2, 840, 113549, 1, 1, 4})
};

# Used several places
bytearr0 := array [] of { byte '0' };
bytearr1 := array [] of { byte '1' };
bytearr2 := array [] of { byte '2' };

sessions: list of ref Session;
logfd : ref Sys->FD;

# session id timeout
TIMEOUT_SECS : con 5*60;
debug : con 0;

# Load needed modules.
# vers is the version that we want to talk as client
# (may be overridden in various sessions by the servers)
init(vers: int)
{
	sys = load Sys Sys->PATH;
	logfd = sys->fildes(1);
	keyring = load Keyring Keyring->PATH;
	random = load Random Random->PATH;
	ssl = load SSL SSL->PATH;
	asn1 = load ASN1 ASN1->PATH;
	daytime = load Daytime Daytime->PATH;
	if(keyring == nil || random == nil || ssl == nil || asn1 == nil || daytime == nil) {
		log("sslhs couldn't load a module\n");
		return;
	}
	asn1->init();
	(n, nil) := sys->stat("/n/ssl/clone");
	if(n < 0) {
		n = sys->bind("#D", "/n/ssl", sys->MREPL);
		if(n < 0)
			log(sys->sprint("sslhs: can't bind ssl device: %r"));
	}
	if(vers != 2)
		log("sslhs: wrong version\n");
	sessions = nil;

	sys->print("exportable version of sslhs\n");
}

# Given fd, the raw (non-ssl) connection to a server,
# push the ssl device and then do the initial handshake to
# set up the ssl parameters.
# Return (nil, ssl connection to server) or (error string, nil).
client(fd: ref Sys->FD, servname: string) : (string, Sys->Connection)
{
	if(debug)
		log("client handshake\n");
	nullc := sys->Connection(nil, nil, "");
	(e, c) := ssl->connect(fd);
	if(e != nil)
		return (e, nullc);
	session := getsession(servname);

	# Phase 1: establish private communications
	challenge := random->randombuf(Random->NotQuiteRandom, 16);
	cm := ref ClientMsg.Hello(SSL_CLIENT_VERSION, inferno_ciphers, session.sid, challenge);
	e = send_cm(c, cm);
	if(e != nil)
		return (e, nullc);
	sm1 : ref ServerMsg;
	(e, sm1) = recv_sm(c);
	if(e != nil)
		return (e, nullc);
	srvhello: ref ServerMsg.Hello;
	pick s := sm1 {
		Hello =>
			srvhello = s;
		* =>
			return ("protocol error: expected hello", nullc);
	}
	if(srvhello.version != SSL_SERVER_VERSION)
		return ("protocol error: wrong server verson", nullc);
	session.connid = srvhello.connid;
	ck: int;
	if(srvhello.sidhit)
		ck = session.cipher;
	else {
		ck = -1;
		for(i := 0; i < len srvhello.ciphers; i++) {
			x := srvhello.ciphers[i];
			if(x >= 0) {
				ck = x;
				break;
			}
		}
		if(ck == -1)
			return ("protocol problem: no common cipher", nullc);
		session.cipher = ck;
		if(debug)
			log("chosen cipher kind: " + cstr(ck) + "\n");
	}
	ci := cipher_info[ck];

	ha := ci.hashalg;
	if(!srvhello.sidhit) {
		pk : ref RSAKey;
		if(srvhello.certtype == SSL_CT_X509_CERTIFICATE) {
			ct : ref CertX509;
			(e, ct) = decode_cert(srvhello.cert);
			if(e != nil)
				return ("protocol error: decoding certificate: " + e, nullc);
			if(ct.publickey_alg == ALG_rsaEncryption) {
				(e, pk) = decode_rsapubkey(ct.publickey);
				if(e != nil)
					return ("protocol error: decoding public key: " + e, nullc);
			}
		}

		masterkeylen := ci.keylen;
		if(masterkeylen > 0) {
			session.masterkey = random->randombuf(Random->ReallyRandom, masterkeylen);
			if(debug)
				log("master key: " + bastr(session.masterkey) + "\n");
		}
		secretkeylen := masterkeylen;
		clearkeylen := 0;
		clearkey, secretkey : array of byte;
		if(ci.clearlen != 0) {
			clearkeylen = ci.clearlen;
			clearkey = session.masterkey[0:clearkeylen];
		}
		if(masterkeylen > clearkeylen)
			secretkey = session.masterkey[clearkeylen:];
		keyarglen := ci.keyarglen;
		if(keyarglen > 0)
			session.keyarg = random->randombuf(Random->NotQuiteRandom, keyarglen);
		if(pk == nil)
			return ("protocol error: need a public key for key exchange", nullc);
		encryptedkey : array of byte;
		(e, encryptedkey) = pkcs1_encrypt(secretkey, pk, 2);
		if(e != nil)
			return (e, nullc);
		cmmk := ref ClientMsg.MasterKey(ck, clearkey, encryptedkey, session.keyarg);
		e = send_cm(c, cmmk);
		if(e != nil)
			return (e, nullc);
	}

	# Set up encryption
	(client_read_key, client_write_key) := sessionkeys(session, challenge);
	e = setsslencryption(c, session, client_read_key, client_write_key);
	if(e != "")
		return (e, nullc);

	# Phase 2: Authentication
	cmf := ref ClientMsg.Finished(session.connid);
	e = send_cm(c, cmf);
	if(e != nil)
		return (e, nullc);

	sm2 : ref ServerMsg;
	(e, sm2) = recv_sm(c);
	if(e != nil)
		return (e, nullc);
	pick sv := sm2 {
		Verify =>
			if(!barr_eq(sv.challenge, challenge))
				return ("protocol error: challenge mismatch", nullc);
		* =>
			return ("protocol error: expected Server-Verify", nullc);
	}

	sm3 : ref ServerMsg;
	(e, sm3) = recv_sm(c);
	if(e != nil)
		return (e, nullc);
	pick sf := sm3 {
		Finished =>
			session.sid = sf.sid;
		RequestCertificate =>
			return ("protocol problem: need a client certificate", nullc);
	}


	if(debug)
		log("client handshake done\n");
	return ("", *c);
}

# Given fd, the raw (non-ssl) connection to a client,
# push the ssl device and then do the initial handshake to
# set up the ssl parameters.
# cert is an X509 certificate, and privkey is the private key
# corresponding to the public key in cert.
# Return (nil, ssl connection to client) or (error string, nil).
server(fd: ref Sys->FD, clientname: string, cert: array of byte, pk: ref Key) :
			(string, Sys->Connection)
{
	if(debug)
		log("server handshake\n");
	nullc := sys->Connection(nil, nil, "");
	(e, c) := ssl->connect(fd);
	if(e != nil)
		return (e, nullc);
	session := getsession(clientname);

	privkey := ref RSAKey(IPint.bytestoip(pk.modulus), pk.modlen, IPint.bytestoip(pk.exponent));
	cm1: ref ClientMsg;
	(e, cm1) = recv_cm(c);
	if(e != nil)
		return (e, nullc);
	chello: ref ClientMsg.Hello;
	pick cc := cm1 {
		Hello =>
			chello = cc;
		* =>
			return ("protocol error: expected hello", nullc);
	}
	sidhit := (session.sid != nil);
	chal := array of byte chello.challenge;
	session.connid = random->randombuf(Random->NotQuiteRandom, 16);
	shello : ref ServerMsg;
	if(sidhit)
		shello = ref ServerMsg.Hello(sidhit, 0, SSL_SERVER_VERSION, nil, nil, session.connid);
	if(!sidhit) {
		n := len chello.ciphers;
		ciphers := array[n] of int;
		j := 0;
		for(i := 0; i < n; i++) {
			ci := chello.ciphers[i];
			if(ci != -1 && cipher_info[ci].cryptalg != -1)
				ciphers[j++] = ci;
		}
		if(j == 0) {
			sme := ref ServerMsg.Error(SSL_PE_NO_CIPHER);
			send_sm(c, sme);
			return ("no common cipher", nullc);
		}
		ciphers = ciphers[0:j];
		shello = ref ServerMsg.Hello(sidhit, SSL_CT_X509_CERTIFICATE, SSL_SERVER_VERSION,
					cert, ciphers, session.connid);
	}
	e = send_sm(c, shello);
	if(e != "")
		return (e, nullc);

	ci : CipherInfo;
	if(sidhit)
		ci = cipher_info[session.cipher];
	else {
		cm2: ref ClientMsg;
		(e, cm2) = recv_cm(c);
		if(e != nil)
			return (e, nullc);
		cmk: ref ClientMsg.MasterKey;
		pick cd := cm2 {
			MasterKey =>
				cmk = cd;
			* =>
				return ("protocol error: expected master key", nullc);
		}
		if(cmk.cipher == -1 || cipher_info[cmk.cipher].cryptalg == -1)
			return ("protocol error: bad cipher in masterkey", nullc);
		session.cipher = cmk.cipher;
		masterkey : array of byte = nil;
		decryptedkey : array of byte = nil;
		dklen := 0;
		cklen := len cmk.clearkey;
		mklen := cklen;
		if(len cmk.encryptedkey > 0) {
			(e, decryptedkey) = pkcs1_decrypt(cmk.encryptedkey, privkey, 0);
			if(e != "")
				return (e, nullc);
			dklen = len decryptedkey;
			mklen += dklen;
		}
		if(mklen > 0) {
			masterkey = array[mklen] of byte;
			if(cklen != 0)
				masterkey[0:] = cmk.clearkey;
			if(dklen != 0)
				masterkey[cklen:] = decryptedkey;
		}
		if(mklen != ci.keylen)
			return ("protocol error: bad key length for cipher", nullc);
		session.masterkey = masterkey;
	}

	# Set up encryption
	(server_write_key, server_read_key) := sessionkeys(session, chal);
	e = setsslencryption(c, session, server_read_key, server_write_key);
	if(e != "") {
		session.sid = nil;
		return (e, nullc);
	}

	# Phase 2: Authentication
	cm3 : ref ClientMsg;
	(e, cm3) = recv_cm(c);
	if(e != nil)
		return (e, nullc);
	cmf : ref ClientMsg.Finished;
	pick ce := cm3 {
		Finished =>
			cmf = ce;
		* =>
			session.sid = nil;
			return ("protocol error: expected Client-Finished", nullc);
	}
	if(!barr_eq(cmf.connid, session.connid)) {
		session.sid = nil;
		return ("protocol error: connection id didn't match", nullc);
	}

	smv := ref ServerMsg.Verify(chal);
	e = send_sm(c, smv);
	if(e != "") {
		session.sid = nil;
		return (e, nullc);
	}

	if(session.sid == nil)
		session.sid = random->randombuf(Random->NotQuiteRandom, 16);
	smf := ref ServerMsg.Finished(session.sid);
	e = send_sm(c, smf);
	if(e != "") {
		session.sid = nil;
		return (e, nullc);
	}

	return (e, nullc);
}

# Return (client read key, client write key) for session with given challenge.
sessionkeys(session: ref Session, challenge: array of byte) : (array of byte, array of byte)
{
	client_read_key, client_write_key: array of byte;
	nmk := len session.masterkey;
	nch := len challenge;
	ncid := len session.connid;
	kmsrc := array[nmk+1+nch+ncid] of byte;
	kmsrc[0:] = session.masterkey;
	kmsrc[nmk] = byte '0';
	kmsrc[nmk+1:] = challenge;
	kmsrc[nmk+1+nch:] = session.connid;
	case cipher_info[session.cipher].keylen {
	0 or 8 =>
		km := hash(kmsrc);
		client_read_key = km[0:8];
		client_write_key = km[8:16];
	16 =>
		km0 := hash(kmsrc);
		kmsrc[nmk]++;
		km1 := hash(kmsrc);
		client_read_key = km0[0:16];
		client_write_key = km1[0:16];
	24 =>
		km0 := hash(kmsrc);
		kmsrc[nmk]++;
		km1 := hash(kmsrc);
		kmsrc[nmk]++;
		km2 :=hash(kmsrc);
		client_read_key = array[24] of byte;
		client_read_key[0:] = km0[0:8];
		client_read_key[8:] = km0[8:16];
		client_read_key[16:] = km1[0:8];
		client_write_key = array[24] of byte;
		client_write_key[0:] = km1[8:16];
		client_write_key[8:] = km2[0:8];
		client_write_key[16:] = km2[8:16];
	}
	return (client_read_key, client_write_key);
}

# Tell the ssl device about our algorithm and keys
setsslencryption(c: ref Sys->Connection, session: ref Session, readkey, writekey: array of byte) : string
{
	e := sslsecrets(c, readkey, writekey);
	if(e != "")
		return e;
	algspec := "alg ";
	ci := cipher_info[session.cipher];
	case ci.cryptalg {
	RC4 =>
		algspec += "rc4_128";
	DEScbc =>
		algspec += "des_56_cbc";
	}
	case ci.hashalg {
	SHA =>
		algspec += " sha";
	MD5 =>
		algspec += " md5";
	}
	e = sslctl(c, algspec);
	return e;
}

# Look up name in session cache;
# Expire the session id if necessary.
# Return a new one if not found.
getsession(name: string) : ref Session
{
	session : ref Session = nil;
	now := daytime->now();

	for(l := sessions; l != nil; l = tl l) {
		ss := hd l;
		if(ss.peer == name) {
			session = ss;
			if(now > session.ctime+TIMEOUT_SECS) {
				session.ctime = now;
				session.sid = nil;
			}
			session = dupsession(session);
			break;
		}
	}
	if(session == nil) {
		session = ref Session(name, now, -1, nil, nil, nil, nil);
		sessions = session :: sessions;
	}

	return session;
}

dupsession(ss: ref Session) : ref Session
{
	s := ref Session(ss.peer, ss.ctime, ss.cipher, nil, nil, nil, nil);
	s.sid = array [len ss.sid] of byte;
	s.sid[0:] = ss.sid;
	s.masterkey = array [len ss.masterkey] of byte;
	s.masterkey[0:] = ss.masterkey;
	s.keyarg = array [len ss.keyarg] of byte;
	s.keyarg[0:] = ss.keyarg;
	s.connid = array [len ss.connid] of byte;
	s.connid[0:] = ss.connid;

	return s;
}

# Do MD5 hash on a
hash(a: array of byte) : array of byte
{
	if(debug)
		log("hash " + bastr(a) + "\n");
	ans := array[Keyring->MD5dlen] of byte;
	keyring->md5(a, len a, ans, nil);
	if(debug)
		log("answer " + bastr(ans) + "\n");
	return ans;
}

# Decode and parse an X.509 Certificate, defined by this ASN1:
#	Certificate ::= SEQUENCE {
#		certificateInfo CertificateInfo,
#		signatureAlgorithm AlgorithmIdentifier,
#		signature BIT STRING }
#
#	CertificateInfo ::= SEQUENCE {
#		version [0] INTEGER DEFAULT v1 (0),
#		serialNumber INTEGER,
#		signature AlgorithmIdentifier,
#		issuer Name,
#		validity Validity,
#		subject Name,
#		subjectPublicKeyInfo SubjectPublicKeyInfo }
#
#	(version v2 has two more fields, optional
#	 unique identifiers for issuer and subject; since we
#	 ignore these anyway, we won't parse them)
#
#	Validity ::= SEQUENCE {
#		notBefore UTCTime,
#		notAfter UTCTime }
#
#	SubjectPublicKeyInfo ::= SEQUENCE {
#		algorithm AlgorithmIdentifier,
#		subjectPublicKey BIT STRING }
#
#	AlgorithmIdentifier ::= SEQUENCE {
#		algorithm OBJECT IDENTIFER,
#		parameters ANY DEFINED BY ALGORITHM OPTIONAL }
#
#	Name ::= SEQUENCE OF RelativeDistinguishedName
#
#	RelativeDistinguishedName ::= SET SIZE(1..MAX) OF AttributeTypeAndValue
#
#	AttributeTypeAndValue ::= SEQUENCE {
#		type OBJECT IDENTIFER,
#		value DirectoryString }
#
#	(selected attributes have these Object Ids:
#		commonName {2 5 4 3}
#		countryName {2 5 4 6}
#		localityName {2 5 4 7}
#		stateOrProvinceName {2 5 4 8}
#		organizationName {2 5 4 10}
#		organizationalUnitName {2 5 4 11}
#	)
#
#	DirectoryString ::= CHOICE {
#		teletexString TeletexString,
#		printableString PrintableString,
#		universalString UniversalString }
#
decode_cert(a: array of byte) : (string, ref CertX509)
{
	(err, ecert) := asn1->decode(a);
	if(err != "")
		return (err, nil);
	if(debug)
		log("parsing certificate\n");
parse:
	# loop executes only once; just here so can break from it
	# on syntax error, avoiding deep nesting or many returns
	for(;;) {
		c := ref CertX509;

		# Certificate
		(ok, el) := ecert.is_seq();
		if(!ok || len el != 3)
			break parse;
		ecertinfo := hd el;
		el = tl el;
		esigalg := hd el;
		el = tl el;
		esig := hd el;

		# CertificateInfo
		(ok, el) = ecertinfo.is_seq();
		if(!ok || len el < 6)
			break parse;
		eserial := hd el;
		el = tl el;
		# check for optional version, marked by explicit context tag 0
		if(eserial.tag.class == ASN1->Context && eserial.tag.num == 0) {
			eserial = hd el;
			if(len el < 7)
				break parse;
			el = tl el;
		}
		ecisig := hd el;
		el = tl el;
		eissuer := hd el;
		el = tl el;
		evalidity := hd el;
		el = tl el;
		esubj := hd el;
		el = tl el;
		epubkey := hd el;
		(ok, c.serial) = eserial.is_int();
		if(!ok) {
			(ok, nil) = eserial.is_bigint();
			if(!ok)
				break parse;
			c.serial = -1;
		}
		if(debug)
			log("serial #: " + string c.serial + "\n");
		(ok, c.issuer) = parse_name(eissuer);
		if(!ok)
			break parse;
		if(debug)
			log("issuer: " + c.issuer + "\n");

		# Validity
		(ok, el) = evalidity.is_seq();
		if(!ok || len el != 2)
			break parse;
		(ok, c.validity_start) = (hd el).is_time();
		if(!ok)
			break parse;
		(ok, c.validity_end) = (hd tl el).is_time();
		if(!ok)
			break parse;
		if(debug)
			log("validity: " + c.validity_start + " to " + c.validity_end + "\n");

		# resume CertificateInfo
		(ok, c.subject) = parse_name(esubj);
		if(!ok)
			break parse;
		if(debug)
			log("subject: " + c.subject + "\n");

		# SubjectPublicKeyInfo
		(ok, el) = epubkey.is_seq();
		if(!ok || len el != 2)
			break parse;
		(ok, c.publickey_alg) = parse_alg(hd el);
		if(!ok)
			break parse;
		if(debug)
			log("public key alg: " + string c.publickey_alg + "\n");
		unused : int;
		(ok, unused, c.publickey) = (hd tl el).is_bitstring();
		if(!ok || unused != 0)
			break parse;
		if(debug)
			log("public key: " + bastr(c.publickey) + "\n");

		# resume Certificate
		(ok, c.signature_alg) = parse_alg(esigalg);
		if(!ok)
			break parse;
		if(debug)
			log("signature algorithm: " + string c.signature_alg + "\n");
		(ok, unused, c.signature) = esig.is_bitstring();
		if(!ok || unused != 0)
			break parse;
		if(debug)
			log("signature: " + bastr(c.signature) + "\n");

		return ("", c);
	}
	return ("syntax error", nil);
}

# Parse the Name ASN1 type.
# The sequence of RelativeDistinguishedName's gives a sort of pathname,
# from most general to most specific.  Each element of the path can be
# one or more (but usually just one) attribute-value pair, such as
# countryName="US".
# We'll just form a "postal-style" address string by concatenating the elements
# from most specific to least specific, separated by commas.
# Return (1 if ok else 0, name-as-string)
parse_name(e: ref Elem) : (int, string)
{
parse:
	# dummy loop for breaking out of
	for(;;) {
		(ok, el) := e.is_seq();
		if(!ok)
			break parse;

		parts : list of string = nil;
		while(el != nil) {
			es := hd el;
			setel : list of ref Elem;
			(ok, setel) = es.is_set();
			if(!ok)
				break parse;
			while(setel != nil) {
				eat := hd setel;
				atel : list of ref Elem;
				(ok, atel) = eat.is_seq();
				if(!ok || len atel != 2)
					break parse;
				s : string;
				(ok, s) = (hd tl atel).is_string();
				if(!ok)
					break parse;
				parts = s :: parts;
				setel = tl setel;
			}
			el = tl el;
		}
		ans := "";
		while(parts != nil) {
			ans += hd parts;
			parts = tl parts;
			if(parts != nil)
				ans += ", ";
		}
		return (1, ans);
	}
	return (0, "");
}

# Parse an AlgorithmIdentifer ASN1 type.
# Look up the oid in oid_tab and return one of OID_rsaEncryption, etc..
# For now, ignore parameters, since none of our algorithms need them.
parse_alg(e: ref Elem) : (int, int)
{
	(ok, el) := e.is_seq();
	if(!ok || el == nil)
		return (0, 0);
	oid: ref Oid;
	(ok, oid) = (hd el).is_oid();
	if(!ok)
		return (0, 0);
	alg := asn1->oid_lookup(oid, alg_oid_tab);
	return (1, alg);
}

# Decode an RSAPublicKey ASN1 type, defined as:
#
#	RSAPublickKey :: SEQUENCE {
#		modulus INTEGER,
#		publicExponent INTEGER
#	}
#
decode_rsapubkey(a: array of byte) : (string, ref RSAKey)
{
	(err, e) := asn1->decode(a);
	if(err != "")
		return (err, nil);
	if(debug)
		log("parsing RSA public key\n");
parse:
	# dummy loop for breaking out of
	for(;;) {
		(ok, el) := e.is_seq();
		if(!ok || len el != 2)
			break parse;
		modbytes, expbytes: array of byte;
		(ok, modbytes) = (hd el).is_bigint();
		if(!ok)
			break parse;
		modulus := IPint.bytestoip(modbytes);
		# get modlen this way, because sometimes it
		# comes with leading zeros that are to be ignored!
		mbytes := modulus.iptobytes();
		modlen := len mbytes;
		if(debug)
			log("public key modulus: " + bastr(mbytes) + "\n");
		(ok, expbytes) = (hd tl el).is_bigint();
		if(!ok)
			break parse;
		exponent := keyring->IPint.bytestoip(expbytes);
		if(debug)
			log("public key exponent: " + exponent.iptostr(10) + "\n");
		return ("", ref RSAKey(modulus, modlen, exponent));
	}
	return ("decoding public key: syntax error", nil);
}

# Encrypt data according to PKCS#1, with given blocktype,
# using given key.
pkcs1_encrypt(data: array of byte, key: ref RSAKey, blocktype: int) : (string, array of byte)
{
	k := key.modlen;
	dlen := len data;
	if(k < 12 || dlen > k-11)
		return ("bad parameters for pkcs#1", nil);
	padlen := k-3-dlen;
	pad := random->randombuf(Random->NotQuiteRandom, padlen);
	for(i:=0; i < padlen; i++) {
		if(blocktype == 0)
			pad[i] = byte 0;
		else if(blocktype == 1)
			pad[i] = byte 16rff;
		else if(pad[i] == byte 0)
			pad[i] = byte 1;
		}
	eb := array[k] of byte;
	eb[0] = byte 0;
	eb[1] = byte blocktype;
	eb[2:] = pad[0:];
	eb[padlen+2] = byte 0;
	eb[padlen+3:] = data[0:];
	return ("", rsacomp(eb, key));
}

# Decrypt data according to PKCS#1, with given key.
# If public is true, expect a block type of 0 or 1, else 2.
pkcs1_decrypt(data: array of byte, key: ref RSAKey, public: int) : (string, array of byte)
{
	eb := rsacomp(data, key);
	k := key.modlen;
	if(len eb == k) {
		bt := int eb[1];
		if(int eb[0] == 0 && ((public && (bt == 0 || bt == 1)) || (!public && bt == 2))) {
			for(i := 2; i < k; i++)
				if(eb[i] == byte 0)
					break;
			if(i < k-1) {
				ans := array[k-(i+1)] of byte;
				ans[0:] = eb[i+1:];
				return ("", ans);
			}
		}
	}
	return ("pkcs1 decryption error", nil);
}

# Do RSA computation on block according to key, and pad
# result on left with zeros to make it key.modlen long.
rsacomp(block: array of byte, key: ref RSAKey) : array of byte
{
	x := keyring->IPint.bytestoip(block);
	y := x.expmod(key.exponent, key.modulus);
	ybytes := y.iptobytes();
	k := key.modlen;
	ylen := len ybytes;
	if(ylen < k) {
		a := array[k] of { * =>  byte 0};
		a[k-ylen:] = ybytes[0:];
		ybytes = a;
	}
	else if(ylen > k) {
		# assume it has leading zeros (mod should make it so)
		a := array[k] of byte;
		a[0:] = ybytes[ylen-k:];
		ybytes = a;
	}
	return ybytes;
}

# Encode client msg according to protocol and send it along the connection.
# Return error message.
send_cm(c: ref Sys->Connection, msg: ref ClientMsg) : string
{
	a : array of byte = nil;
	n := 0;
	i : int;
	pick m := msg {
	Error =>
		n = 3;
		a = array[n] of byte;
		a[0] = byte  SSL_MT_ERROR;
		put2(a, 1, m.code);
	Hello =>
		cilen := 3*len m.ciphers;
		slen := len m.sid;
		chlen := len m.challenge;
		n = 9+cilen+slen+chlen;
		a = array[n] of byte;
		a[0] = byte SSL_MT_CLIENT_HELLO;
		put2(a, 1, m.version);
		put2(a, 3, cilen);
		put2(a, 5, slen);
		put2(a, 7, chlen);
		i = 9;
		for(k := 0; k < len m.ciphers; k++)
			i = putck(a, i, m.ciphers[k]);
		i = putarr(a, i, m.sid);
		putarr(a, i, m.challenge);
	MasterKey =>
		cklen := len m.clearkey;
		eklen := len m.encryptedkey;
		kalen := len m.keyarg;
		n = 10+cklen+eklen+kalen;
		a = array[n] of byte;
		a[0] = byte SSL_MT_CLIENT_MASTER_KEY;
		putck(a, 1, m.cipher);
		put2(a, 4, cklen);
		put2(a, 6, eklen);
		put2(a, 8, kalen);
		i = putarr(a, 10, m.clearkey);
		i = putarr(a, i, m.encryptedkey);
		putarr(a, i, m.keyarg);
	Certificate =>
		clen := len m.cert;
		rlen := len m.response;
		n = 6+clen+rlen;
		a = array[n] of byte;
		a[0] = byte SSL_MT_CLIENT_CERTIFICATE;
		a[1] = byte m.certtype;
		put2(a, 2, clen);
		put2(a, 4, rlen);
		i = putarr(a, 6, m.cert);
		putarr(a, i, m.response);
	Finished =>
		clen := len m.connid;
		n = 1+clen;
		a = array[n] of byte;
		a[0] = byte SSL_MT_CLIENT_FINISHED;
		putarr(a, 1, m.connid);
	}
	if(n == 0)
		return "can't send unknown message type";
	if(debug)
		log(cmsgstring(msg));
	ret := sys->write(c.dfd, a, n);
	err := "";
	if(ret < 0) {
		err = sys->sprint("connection write error: %r");
		if(debug)
			log(sys->sprint("%s\n", err));
	}
	else if(debug)
		log("sent\n");
	return err;
}

# Receive a server message from c and decode it according to the protocol,
# returning (error string, msg).
recv_sm(c: ref Sys->Connection) : (string, ref ServerMsg)
{
	buf := array [Sys->ATOMICIO] of byte;
	ans : ref ServerMsg;
	err := "";
	if(debug)
		log("receive\n");
	n := sys->read(c.dfd, buf, len buf);
	if(n < 0)
		err = sys->sprint("connection read error: %r");
	else if(n == 0)
		err = "connection read null message";
	else {
		a := buf;
		i : int;
		case int a[0] {
		SSL_MT_ERROR =>
			if(n == 3) {
				code := get2(a, 1);
				err = errstring(code);
			}
		SSL_MT_SERVER_HELLO =>
			if(n >= 11) {
				sidhit := int a[1];
				certtype := int a[2];
				v := get2(a, 3);
				celen := get2(a, 5);
				cilen := get2(a, 7);
				colen := get2(a, 9);
				if(n == 11+celen+cilen+colen && (cilen%3)==0) {
					cert := a[11 : 11+celen];
					i = 11+celen;
					m := cilen/3;
					ciphers := array[m] of int;
					for(k := 0; k < m; k++) {
						ciphers[k] = getck(a, i);
						i += 3;
					}
					connid := a[i : i+colen];
					ans = ref ServerMsg.Hello(sidhit, certtype, v, cert, ciphers, connid);
				}
			}
		SSL_MT_SERVER_VERIFY =>
			challenge := a[1:n];
			ans = ref ServerMsg.Verify(challenge);
		SSL_MT_REQUEST_CERTIFICATE =>
			if(n >= 2) {
				authtype := int a[1];
				certchallenge := a[2:n];
				ans = ref ServerMsg.RequestCertificate(authtype, certchallenge);
			}
		SSL_MT_SERVER_FINISHED =>
			sid := a[1:n];
			ans = ref ServerMsg.Finished(sid);
		* =>
			err = "unknown message type received";
		}
	}
	if(ans == nil && err == "")
		err = "bad message length";
	if(debug) {
		if(ans != nil)
			log(smsgstring(ans));
		else
			log(sys->sprint("error=%s\n", err));
	}
	return (err, ans);
}

# Encode server msg according to protocol and send it along the connection.
# Return error message.
send_sm(c: ref Sys->Connection, msg: ref ServerMsg) : string
{
	a : array of byte = nil;
	n := 0;
	i : int;
	pick m := msg {
	Error =>
		n = 3;
		a = array[n] of byte;
		a[0] = byte  SSL_MT_ERROR;
		put2(a, 1, m.code);
	Hello =>
		celen := len m.cert;
		cilen := 3*len m.ciphers;
		colen := len m.connid;
		n = 11+celen+cilen+colen;
		a = array[n] of byte;
		a[0] = byte SSL_MT_SERVER_HELLO;
		a[1] = byte m.sidhit;
		a[2] = byte m.certtype;
		put2(a, 3, m.version);
		put2(a, 5, celen);
		put2(a, 7, cilen);
		put2(a, 9, colen);
		i = putarr(a, 11, m.cert);
		for(k := 0; k < len m.ciphers; k++)
			i = putck(a, i, m.ciphers[k]);
		i = putarr(a, i, m.connid);
	Verify =>
		clen := len m.challenge;
		n = 1+clen;
		a = array[n] of byte;
		a[0] = byte SSL_MT_SERVER_VERIFY;
		putarr(a, 1, m.challenge);
	RequestCertificate =>
		clen := len m.certchallenge;
		n = 2+clen;
		a = array[n] of byte;
		a[0] = byte SSL_MT_REQUEST_CERTIFICATE;
		a[1] = byte m.authtype;
		putarr(a, 2, m.certchallenge);
	Finished =>
		slen := len m.sid;
		n = 1+slen;
		a = array[n] of byte;
		a[0] = byte SSL_MT_SERVER_FINISHED;
		putarr(a, 1, m.sid);
	}
	if(n == 0)
		return "can't send unknown message type";
	if(debug)
		log(smsgstring(msg));
	ret := sys->write(c.dfd, a, n);
	err := "";
	if(ret < 0) {
		err = sys->sprint("connection write error: %r");
		if(debug)
			log(sys->sprint("%s\n", err));
	}
	else if(debug)
		log("sent\n");
	return err;
}

# Receive a client message from c and decode it according to the protocol,
# returning (error string, msg).
recv_cm(c: ref Sys->Connection) : (string, ref ClientMsg)
{
	buf := array [Sys->ATOMICIO] of byte;
	ans : ref ClientMsg;
	err := "";
	if(debug)
		log("receive\n");
	n := sys->read(c.dfd, buf, len buf);
	if(n < 0)
		err = sys->sprint("connection read error: %r");
	else if(n == 0)
		err = "connection read null message";
	else {
		a := buf;
		i : int;
		case int a[0] {
		SSL_MT_ERROR =>
			if(n == 3) {
				code := get2(a, 1);
				err = errstring(code);
			}
		SSL_MT_CLIENT_HELLO =>
			if(n >= 9) {
				v := get2(a, 1);
				cilen := get2(a, 3);
				slen := get2(a, 5);
				chlen := get2(a, 7);
				if(n == 9+cilen+slen+chlen && (cilen%3)==0) {
					i = 9;
					m := cilen/3;
					ciphers := array[m] of int;
					for(k := 0; k < m; k++) {
						ciphers[k] = getck(a, i);
						i += 3;
					}
					sid := a[i : i+slen];
					chal := a[i+slen: i+slen+chlen];
					ans = ref ClientMsg.Hello(v, ciphers, sid, chal);
				}
			}
		SSL_MT_CLIENT_MASTER_KEY =>
			if(n >= 10) {
				cipher := getck(a, 1);
				cklen := get2(a, 4);
				eklen := get2(a, 6);
				kalen := get2(a, 8);
				if(n == 10+cklen+eklen+kalen) {
					clearkey := a[10: 10+cklen];
					i = 10+cklen;
					encryptedkey := a[i: i+eklen];
					i += eklen;
					keyarg := a[i: i+kalen];
					ans = ref ClientMsg.MasterKey(cipher, clearkey, encryptedkey, keyarg);
				}
			}
		SSL_MT_CLIENT_CERTIFICATE =>
			if(n >= 6) {
				cty := int a[1];
				clen := get2(a, 2);
				rlen := get2(a, 4);
				if(n == 6+clen+rlen) {
					cert := a[6: 6+clen];
					i = 6+clen;
					resp := a[i: i+rlen];
					ans = ref ClientMsg.Certificate(cty, cert, resp);
				}
			}
		SSL_MT_CLIENT_FINISHED =>
			connid := a[1:n];
			ans = ref ClientMsg.Finished(connid);
		* =>
			err = "unknown message type received";
		}
	}
	if(ans == nil && err == "")
		err = "bad message length";
	if(debug) {
		if(ans != nil)
			log(cmsgstring(ans));
		else
			log(sys->sprint("error=%s\n", err));
	}
	return (err, ans);

}

sslctl(c: ref Sys->Connection, s: string) : string
{
	if(debug)
		log("sslctl: " + s + "\n");
	a := array of byte s;
	if(sys->write(c.cfd, a, len a) < 0)
		return sys->sprint("error writing sslctl: %r");
	return "";
}

sslsecrets(c: ref Sys->Connection, sin: array of byte, sout: array of byte) : string
{
	fsin := sys->open(c.dir + "/secretin", Sys->OWRITE);
	fsout := sys->open(c.dir + "/secretout", Sys->OWRITE);
	if(fsin == nil || fsout == nil)
		return sys->sprint("can't open ssl secret files: %r\n");
	if(sin != nil) {
		if(debug)
			log("writing secretin: " + bastr(sin) + "\n");
		if(sys->write(fsin, sin, len sin) < 0)
			return sys->sprint("error writing secret: %r");
	}
	if(sout != nil) {
		if(debug)
			log("writing secretout: " + bastr(sout) + "\n");
		if(sys->write(fsout, sout, len sout) < 0)
			return sys->sprint("error writing secret: %r");
	}
	return "";
}

# Return 1 if array a equals array b in all elements
barr_eq(a: array of byte, b: array of byte) : int
{
	n := len a;
	if(len b != n)
		return 0;
	for(i := 0; i < n; i++)
		if(a[i] != b[i])
			return 0;
	return 1;
}

# Helper routines for encoding
put2(a: array of byte, i, v: int)
{
	a[i++] = byte (v>>8);
	a[i] = byte v;
}

putck(a: array of byte, i, ck: int) : int
{
	ca := cipher_kind[ck];
	a[i++] = ca[0];
	a[i++] = ca[1];
	a[i++] = ca[2];
	return i;
}

putarr(a: array of byte, i: int, b: array of byte) : int
{
	a[i:] = b;
	return i+len b;
}

# Helper routines for decoding
get2(a: array of byte, i: int) : int
{
	v := int a[i++];
	v = (v<<8) | int a[i];
	return v;
}

getck(a: array of byte, i: int) : int
{
	for(j := 0; j < len cipher_kind; j++) {
		b := cipher_kind[j];
		if(b[0]==a[i] && b[1]==a[i+1] && b[2]==a[i+2])
			return j;
	}
	if(debug)
		log(sys->sprint("unknown cipher kind: %2x %2x %2x\n", int a[i], int a[i+1], int a[i+2]));
	return -1;
}

errstring(code: int) : string
{
	err : string;
	case code {
	SSL_PE_NO_CIPHER => err = "no cipher";
	SSL_PE_NO_CERTIFICATE => err = "no certificate";
	SSL_PE_BAD_CERTIFICATE => err = "bad certificate";
	SSL_PE_UNSUPPORTED_CERTIFICATE_TYPE => err = "unsupported certificate";
	* => err = "unknown error";
	}
	return err;
}

# For debugging

cmsgstring(msg: ref ClientMsg) : string
{
	ans := "Unknown client message";
	pick m := msg {
	Error =>
		ans = "Error " + errstring(m.code);
	Hello =>
		ans = "Client-Hello version=" + string m.version + "\n\tciphers=" + castr(m.ciphers)
			+ "\n\tsid=" + bastr(m.sid) + "\n\tchallenge=" + bastr(m.challenge) + "\n";
	MasterKey =>
		ans = "Client-Master-Key cipher=" + cstr(m.cipher)
			+ "\n\tclearkey=" + bastr(m.clearkey)
			+ "\n\tencryptedkey=" + bastr(m.encryptedkey)
			+ "\n\tkeyarg=" + bastr(m.keyarg) + "\n";
	Certificate =>
		ans = "Client-Certificate certtype=" + string m.certtype
			+ "\n\tcert=" + bastr(m.cert) + "\n\tresponse=" + bastr(m.response) + "\n";
	Finished =>
		ans = "Client-Finished\n\tconnid=" + bastr(m.connid) + "\n";
	}
	return ans;
}

smsgstring(msg: ref ServerMsg): string
{
	ans := "Unknown server message";
	pick m := msg {
	Error =>
		ans = "Error " + errstring(m.code);
	Hello =>
		ans = "Server-Hello sidhit=" + string m.sidhit + " certtype=" + string m.certtype
			+ " version=" + string m.version
			+ "\n\tcert=" + bastr(m.cert) + "\n\tciphers=" + castr(m.ciphers)
			+ "\n\tconnid=" + bastr(m.connid) + "\n";
	Verify =>
		ans = "Server-Verify" + "\n\tchallenge=" + bastr(m.challenge) + "\n";
	RequestCertificate =>
		ans = "Request-Certificate authtype=" + string m.authtype
			+ "\n\tcertchallenge=" + bastr(m.certchallenge) + "\n";
	Finished =>
		ans = "Server-Finished" + "\n\tsid=" + bastr(m.sid) + "\n";
	}
	return ans;
}

bastr(a: array of byte) : string
{
	ans := "";
	for(i := 0; i < len a; i++) {
		if(i < len a - 1 && i%10 == 0)
			ans += "\n\t\t";
		ans += sys->sprint("%2x ", int a[i]);
	}
	return ans;
}

cstr(i: int) : string
{
	ans := "unk";
	case i {
	SSL_CK_RC4_128_WITH_MD5 => ans = "rc4/md5";
	SSL_CK_RC4_128_EXPORT40_WITH_MD5 => ans = "rc4export/md5";
	SSL_CK_RC2_128_CBC_WITH_MD5 => ans = "rc2/md5";
	SSL_CK_RC2_128_CBC_EXPORT40_WITH_MD5 => ans = "rc2export/md5";
	SSL_CK_IDEA_128_CBC_WITH_MD5 => ans = "idea/md5";
	SSL_CK_DES_64_CBC_WITH_MD5 => ans = "des64/md5";
	SSL_CK_DES_192_EDE3_CBC_WITH_MD5 => ans = "des192ede3/md5";
	SSL_CK_NULL_WITH_MD5 => ans = "null/md5";
	}
	return ans;
}

castr(a: array of int) : string
{
	ans := "";
	for(i := 0; i < len a; i++)
		ans += cstr(a[i]) + " ";
	return ans;
}

log(s: string)
{
	a := array of byte s;
	sys->write(logfd, a, len a);
}
