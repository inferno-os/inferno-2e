# Store password information as a colon-separated, variable-length ascii string. Entries are
# stored in the format
# 	<id>:<pw>:<exp>:<comment>
# where <pw> is a Base64 encoding of the SHA digest of the password for <id>, <exp>
# is the expiration date of the password entry in epoch-seconds, and <comment> is
# some undefined data ending with a newline.

implement Password;

include "sys.m";
	sys: Sys;
	OREAD, OWRITE, OTRUNC: import sys;

include "keyring.m";
	kr: Keyring;
	IPint: import kr;

include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;

include "security.m";
include "draw.m";

Pwfile: con "/keydb/password";

#  get: read and parse a password entry; return the entry or nil on error
get(id: string): ref PW
{
	sys = load Sys Sys->PATH;
	kr = load Keyring Keyring->PATH;
	bufio = load Bufio Bufio->PATH;
	if (bufio == nil)
		sys->raise(sys->sprint("fail: can't load module Bufio: %r"));
	iob := bufio->open(Pwfile, OREAD);
	if(iob == nil)
		return nil;

	while ((s := iob.gets('\n')) != nil) {
		(n, tokl) := sys->tokenize(s, ":\n");
		if (n < 3)
			return nil;
		if (hd tokl == id) {
			pw := ref PW;
			pw.id = hd tokl;
			pw.pw = IPint.b64toip(hd tl tokl).iptobytes();
			pw.expire = int hd tl tl tokl;
			if (n==3)
				pw.other = nil;
			else
				pw.other = hd tl tl tl tokl;
			return pw;
		}
	}
	return nil;
}

# put:  write a password entry; return > 0 on success, <= 0 otherwise
put(pw: ref PW): int
{
	sys = load Sys Sys->PATH;
	kr = load Keyring Keyring->PATH;
	bufio = load Bufio Bufio->PATH;
	if (bufio == nil)
		sys->raise(sys->sprint("fail: can't load module Bufio: %r"));

	NGROW: con 2;
	nalloc := NGROW;
	nused := 0;
	found := 0;
	pwarray := array[nalloc] of string;
	
	iob: ref Iobuf;
	if ((iob = bufio->open(Pwfile, OREAD)) != nil) {		# read Pwfile entries into pwarray
		s: string;
		while ((s = iob.gets('\n')) != nil) {
			if (!found) {
				(n, tokl) := sys->tokenize(s, ":\n");
				if (n >= 3 && hd tokl == pw.id) {
					s = sys->sprint("%s:%s:%d:%s\n", pw.id, IPint.bytestoip(pw.pw).iptob64(), pw.expire, pw.other);
					found = 1;
				}
			}
			if (nused == nalloc) {
				nalloc *= NGROW;
				newpwarray := array[nalloc] of string;
				newpwarray[0:] = pwarray[0:];
				pwarray = newpwarray;
			}
			pwarray[nused++] = s;
		}
		iob.close();
	}

	if ((iob = bufio->open(Pwfile, OWRITE | OTRUNC)) == nil)
	if ((iob = bufio->create(Pwfile, OWRITE, 8r600)) == nil)
		return -1;
	for (i := 0; i < nused; i++) {
		if (iob.puts(pwarray[i]) != len pwarray[i]) {
			iob.close();
			return -1;
		}
	}
	if (!found) {
		s := sys->sprint("%s:%s:%d:%s\n", pw.id, IPint.bytestoip(pw.pw).iptob64(), pw.expire, pw.other);
		if (iob.puts(s) != len s) {
			iob.close();
			return -1;
		}
	}
	iob.close();
	return 1;
}
