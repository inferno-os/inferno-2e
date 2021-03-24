implement Mount;

include "sys.m";
	sys: Sys;

include "draw.m";
include "keyring.m";
include "security.m";

Mount: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

usage()
{
	sys->fprint(sys->fildes(2), "Usage: mount [-rabcA] [-C cryptoalg] [-f keyring] <net!addr!file> <mountpoint>\n");
	exit;
}

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	
	# dump module name
	argv = tl argv;

	# process arguments
	(doauth, keyfile, flags, alg, addr, mountpoint) := getargs(argv);

	# open stream
	fd := do_connect(addr);

	# authenticate if necessary
	if (doauth)
		fd = do_auth(keyfile, alg, fd);

	# add to namespace
	do_mount(fd, mountpoint, flags);
}

# process arguments
getargs(argv: list of string): (int, string, int, string, string, string)
{

	copt := 0;
	doauth := 1;
	flags := sys->MREPL;
	alg := Auth->NOSSL;
	keyfile := "default";
	while(argv != nil) {
		s := hd argv;
		if(s[0] != '-')
			break;
	opt:    for(i := 1; i < len s; i++) {
			case s[i] {
			'a' =>
				flags = sys->MAFTER;
			'b' =>
				flags = sys->MBEFORE;
			'r' =>
				flags = sys->MREPL;
			'c' =>
				copt++;
			'C' =>
				alg = s[i+1:];
				if(alg == nil) {
					argv = tl argv;
					if(argv != nil) {
						alg = hd argv;
						if(alg[0] == '-')
							usage();
					} else
						usage();
				}
				break opt;
			'f' =>
				keyfile = s[i+1:];
				if(keyfile == nil) {
					argv = tl argv;
					if(argv != nil) {
						keyfile = hd argv;
						if(keyfile[0] == '-')
							usage();
					} else
						usage();
				}
				break opt;
			'A' =>
				doauth = 0;
			*   =>
				usage();
			}
		}
		argv = tl argv;
	}
	if(copt)
		flags |= sys->MCREATE;
	if(len argv != 2)
		usage();

	return (doauth, keyfile, flags, alg, hd argv, hd tl argv);
}

# either make network connection or open file
do_connect(dest: string): ref Sys->FD
{
        fd : ref Sys->FD;
        (n, nil) := sys->tokenize(dest, "!");
	case n {
	1 =>
                fd = sys->open(dest, Sys->ORDWR);
                if (fd == nil)
			sys->raise(sys->sprint("fail: %r"));
	2 =>
		dest = dest + "!styx";
		(ok, c) := sys->dial(dest, nil);
                if(ok < 0) 
                        sys->raise(sys->sprint("fail: %r"));
                fd = c.dfd;
	3 =>
                (ok, c) := sys->dial(dest, nil);
                if(ok < 0) 
			sys->raise(sys->sprint("fail: %r"));
                fd = c.dfd;
	* =>
		sys->raise(sys->sprint("fail: bad address"));
        }

	return fd;
}

# authenticate if necessary
do_auth(keyfile, alg: string, dfd: ref Sys->FD): ref Sys->FD
{
	kr := load Keyring Keyring->PATH;
	if(kr == nil)
		sys->raise(sys->sprint("fail: Keyring: %r"));

	cert := "/usr/" + user() + "/keyring/" + keyfile;
	ai := kr->readauthinfo(cert);

	if (ai == nil)
	  if (alg == Auth->NOAUTH)
	    sys->fprint(sys->fildes(2), "mount:\t(warning) no certificate in %s;\n\tuse getauthinfo\n", cert);
	  else
	    sys->raise(sys->sprint("fail: readauthinfo: %r"));

	au := load Auth Auth->PATH;
	if(au == nil)
		sys->raise(sys->sprint("fail: Auth: %r"));

	err := au->init();
	if(err != nil)
		sys->raise(sys->sprint("fail: Auth: %s", err));

	fd := ref Sys->FD;
	(fd, err) = au->client(alg, ai, dfd);
	if(fd == nil)
		sys->raise(sys->sprint("fail: Auth: %s", err));

	return fd;
}

user(): string
{
	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil)
		return "";

	buf := array[Sys->NAMELEN] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0)
		return "";

	return string buf[0:n]; 
}

# do the actual mount
do_mount(fd: ref Sys->FD, dir: string, flags: int)
{
	# check connection with TNOP's
	c := chan of int;
	pid := chan of int;

	spawn rx(pid, fd, c);
	rx_pid := <- pid;
	spawn timer(pid, c);
	timer_pid := <- pid;
	spawn tx(pid, fd, c);
	tx_pid := <- pid;

	rc := <- c;

	kill(rx_pid);
	kill(tx_pid);
	kill(timer_pid);

	if (rc < 0)
		sys->raise("fail: styx server timeout");

	# all ok so mount it
	ok := sys->mount(fd, dir, flags, "");
	if(ok < 0)
		sys->raise(sys->sprint("fail: %r"));
}

kill(pid: int)
{
	fd := sys->open("#p/" + string pid + "/ctl", sys->OWRITE);
	if (fd == nil)
		return;

	msg := array of byte "kill";
        sys->write(fd, msg, len msg);
}

Tnop: con 0;
Rnop: con 1;

# send TNOP's to chack that other end is alive
tx(pid: chan of int, fd: ref Sys->FD, c: chan of int)
{
	pid <-= sys->pctl(0, nil);

	tnop := array[] of { byte 16r00, byte 16rff, byte 16rff };
	for(;;) {
		# send TNOP
		n := sys->write(fd, tnop, len tnop);
		if (n < 0)
			c <-= n;

		# sleep
		sys->sleep(500);
	}
}

# listen for RNOP messages
rx(pid: chan of int, fd: ref Sys->FD, c: chan of int)
{
	pid <-= sys->pctl(0, nil);

	buf := array[1] of byte;
	pat := array[] of { byte Rnop, byte 16rff, byte 16rff };
	i := 0;
	for(;;) {
		# wait for RNOP
		n := sys->read(fd, buf, 1);
		if (n < 0)
			c <-= n;
		if (buf[0] != pat[i]) {
			sys->print("Read unknown data [%x] %c\n", int buf[0], int buf[0]);
			i = 0;
			continue;
		}
		if(++i == len pat) {
			c <-= 1;
			return;
		}
	}
}

# timeout for trying TNOP/RNOP
timer(pid: chan of int, c: chan of int)
{
	pid <-= sys->pctl(0, nil);

	# sleep 10 sec.
	sys->sleep(10000);

	# send timeout
	c <-= -1;
}
