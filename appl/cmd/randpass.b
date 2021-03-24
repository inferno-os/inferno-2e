implement randpass;

include "sys.m";
	sys: Sys;
	stdout, stderr: ref Sys->FD;

include "draw.m";

include "keyring.m";
	kr : Keyring;
	IPint: import kr;

randpass: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};


init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	stdout = sys->fildes(1);
	stderr = sys->fildes(2);

	kr = load Keyring Keyring->PATH;

	argv = tl argv;
	pwlen := 8;
	if(argv != nil){
		pwlen = str2num(hd argv);
		if(pwlen == 0 || pwlen > 124){
			sys->fprint(stderr, "Usage: randpass [password-length(<125, default=8)]\n");
			exit;
		}
	}

	rbig := IPint.random(pwlen*8, pwlen*16);
	rstr := rbig.iptob64();

	sys->fprint(stdout, "%s\n", rstr[0:pwlen]);
}

str2num(s: string): int
{
	n, i : int;

	for(i=0; i<len s; i++)
	{
		n = int s[i];
		if(n<48 || n>57)
			return 0;
	}

	return int s;
}

