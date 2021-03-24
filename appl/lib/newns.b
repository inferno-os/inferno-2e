implement Newns;
#
# Build a new namespace from a file
#
#	new	create a new namespace from current directory (use cd)
#	fork	split the namespace before modification
#	nodev	disallow device attaches
#	bind	[-abrci] from to
#	mount	[-abrci] [net!]machine to [spec]
#	unmount	[-i] [from] to
#   	cd	directory
#
#	-i to bind/mount/unmount means continue in the face of errors
#
include "sys.m";
	sys: Sys;
	FD, FileIO, Connection: import Sys;
	stderr: ref FD;

include "draw.m";
	Context: import Draw;

include "bufio.m";
	bio: Bufio;
	Iobuf: import Bufio;

include "newns.m";

include "sh.m";

include "keyring.m";
	kr: Keyring;

include "security.m";
	au: Auth;

ignore: int;


newns(user: string, nsfile: string): string
{
	# Load reqd modules 
	sys = load Sys Sys->PATH;
	kr = load Keyring Keyring->PATH;
	stderr = sys->fildes(2);

	# Could do some authentication here, and bail if no good FIXME
	if(user == nil);

	bio = load Bufio Bufio->PATH;
	if(bio == nil)
		return "newns: can't load module bufio";

	au = load Auth Auth->PATH;
	if(au == nil)
		return "newns: can't load module Auth";
	err := au->init();
	if(err != nil)
		return "newns: " + err;

	if(nsfile == nil)
		nsfile = "namespace"; 

	mfp := bio->open(nsfile, bio->OREAD);
	if(mfp==nil)
      		return sys->sprint("newns: can't open %s for read %r\n", nsfile);

	e := "";
	for(;;) {
		cmdline, tpline: string = nil;
		e = "";
		tpval: int = '\n';
		tpline = bio->mfp.gets(tpval);
		if(tpline == nil)
			break;
		cmdline = tpline[0:len tpline-1];
		(n, slist) := sys->tokenize(cmdline, " \t\r");
		if (n <= 0)
			continue;
		ignore = 0;
		e = nsop(slist);
		if(e != "" && ignore == 0)
			break;
   	}

	bio->mfp.close();
	return e;
}

nsop(argv: list of string): string
{
	# ignore comments 
	if((hd argv)[0] == '#')
		return nil;
 
	e := "";
	c := 0;
	cmdstr := hd argv;
	case cmdstr {
	"new" =>
		c = sys->NEWNS;
	"fork"  =>
		c = sys->FORKNS;
	"nodev" =>
		c = sys->NODEVS;
	"bind" =>
		e = bind(tl argv);
	"mount" =>
		e = mount(tl argv);
	"unmount" =>
		e = unmount(tl argv);
   	"cd" =>
   		if(len tl argv != 1)
			return "cd: must have one argument";   
		if(sys->chdir(hd tl argv) < 0)
			return sys->sprint("%r");
	* =>
      		e = "invalid namespace command";
	}
	if(c != 0) {
		if(sys->pctl(c, nil) < 0)
			return sys->sprint("%r");
	}
	return e;
}

rev(l: list of string): list of string
{
	t: list of string;

	while(l != nil) {
		t = hd l :: t;
		l = tl l;
	}
	return t;
}

Moptres: adt {
	argv: list of string;
	flags: int;
	alg: string;
};

mopt(argv: list of string): (ref Moptres, string)
{
	r := ref Moptres(nil, 0, Auth->NOSSL);

	while(argv != nil) {
		s := hd argv;
		argv = tl argv;
		if(s[0] != '-') {
			r.argv = s :: r.argv;
			continue;
		}
	opt:	for(i := 1; i < len s; i++) {
			case s[i] {
			'i' => ignore++;
			'a' => r.flags |= sys->MAFTER;
			'b' => r.flags |= sys->MBEFORE;
			'c' => r.flags |= sys->MCREATE;
			'r' => r.flags |= sys->MREPL;
			'C' =>
				r.alg = s[2:];
				if(r.alg == nil || r.alg == "") {
					if(argv == nil)
						return (nil, "no arg to C option");

					r.alg = hd argv;
					argv = tl argv;
				}
				break opt;

			 *  => return (nil, "bad command option " + s[i:]);
			}
		}
	}
	r.argv = rev(r.argv);
	return (r, nil);
}

bind(argv: list of string): string
{
	(r, err) := mopt(argv);
	if(err != nil)
		return err;

	if(len r.argv < 2)
		return "bind: too few args";

	from := hd r.argv;
	r.argv = tl r.argv;
	todir := hd r.argv;
	if(sys->bind(from, todir, r.flags) < 0)
		return sys->sprint("bind %s %s: %r", from, todir);

	return nil;
}

mount(argv: list of string): string
{
	spec: string;
	fd: ref Sys->FD;

	(r, err) := mopt(argv);
	if(err != nil)
		return err;

	if(len r.argv < 2)
		return "mount: too few args";

	addr := hd r.argv;
	r.argv = tl r.argv;
	dest := addr+"!styx";
	(ok, c) := sys->dial(dest, nil);
	if(ok < 0)
		return sys->sprint("dial: %s: %r", dest);

	user := user();
	kd := "/usr/" + user + "/keyring/";
	cert := kd + addr;
	(ok, nil) = sys->stat(cert);
	if(ok < 0){
		cert = kd + "default";
		(ok, nil) = sys->stat(cert);
		if(ok<0)
			sys->fprint(stderr, "Warning: no certificate found in %s; use getauthinfo\n", kd);
	}

	ai := kr->readauthinfo(cert);
	#
	# let auth->client handle nil ai
	# if(ai == nil)
	#	return sys->sprint("key for %s not found, please use getauthinfo first", addr);
	#

	err = au->init();
	if (err != nil)
		return sys->sprint("auth->init: %r");

  # do this before using module auth
	if(sys->bind("#D", "/n/ssl", Sys->MREPL) < 0)
		return sys->sprint("can't bind #D: %r");
		
	(fd, err) = au->client(r.alg, ai, c.dfd);
	if(fd == nil)
		return sys->sprint("auth: %r");

	dir := hd r.argv;
	r.argv = tl r.argv;
	if(r.argv != nil)
		spec = hd r.argv;
	if(sys->mount(fd, dir, r.flags, "") < 0)
		return sys->sprint("mount: %r");

	return nil;
}

unmount(argv: list of string): string
{
	(r, err) := mopt(argv);
	if(err != nil)
		return err;

	from, tu: string;
	case len r.argv {
	* =>
		return "unmount: takes 1 or 2 args";
	1 =>
		from = nil;
		tu = hd r.argv;
	2 =>
		from = hd r.argv;
		tu = hd tl r.argv;
	}

	if(sys->unmount(from, tu) < 0)
		return sys->sprint("unmount: %r");

	return nil;
}

user(): string
{
	sys = load Sys Sys->PATH;

	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil)
		return "";

	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0)
		return "";

	return string buf[0:n];	
}

