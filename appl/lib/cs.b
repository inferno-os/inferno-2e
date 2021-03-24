implement Cs;

#
# Connection server translates net!machine!service into
# /net/tcp/clone 135.104.9.53!564
#
# This simple implementation only knows about tcp/udp
#
# This module also contains the interface to any db in the
# simple format of services/cs/db. It is used by modules like
# getauthinfo so that the prompt has has the value of $SIGNER.
# kvopen & kvmap could be one function in their own module.
#

Mod : con "cs";

include "sys.m";
include "srv.m";
include "draw.m";
#include "bufio.m";
include "qidcmp.m";

FD, FileIO: import Sys;
Context: import Draw;
#Iobuf: import Bufio;

sys: Sys;
srv: Srv;
#bufio: Bufio;
stderr: ref FD;

qi : Qidcmp;
Cdir : import qi;

Cs: module
{
	init:	fn(nil: ref Context, nil: list of string);
	kvopen: fn(dbfile: string): int;
	kvmap:	fn(key: string): string;
};

Reply: adt
{
  fid   : int;
  addrs : list of string;
  rchan : chan of int;
  wait  : int;
  err   : string;
};
rlist: list of ref Reply;

verbose := 0;
init(ctxt: ref Context, args: list of string)
{
	if((sys = load Sys Sys->PATH) == nil)
	    return;
	stderr = sys->fildes(2);

	if (args != nil)
	  args = tl args;
	opts : list of string;
	(opts, args) = strictsubl("-n" :: "-r" :: "-!c" :: "-v" :: nil, args);
	if (findl("-v", opts)) verbose = 1;
	if (findl("-n", opts)) {
	  if ((srv = load Srv Srv->PATH) != nil) {
	    sys->print("cs: native mode\n");
	    # initialize module
	    if (findl("-!c", opts)) srv->iph2a("!c");	# disable caching
	    else srv->iph2a(nil);
	  }
	  else {
	    sys->fprint(stderr, "cs: failed to load %s %r\n", Srv->PATH);
	    return;
	  }
	}
	else if((srv = load Srv Srv->BUILTINPATH) == nil)
		if ((srv = load Srv Srv->PATH) == nil) {
			sys->fprint(stderr, "cs: failed to load Srv: %r\n");
			return;
		}
		# initialize module!
		else if (findl("-!c", opts)) srv->iph2a("!c");	# disable caching
		     else srv->iph2a(nil);

	(ok, nil) := sys->stat("/net/cs");
	if(ok >= 0) {
		if (findl("-r", opts))
			sys->print("cs: restarting service\n");
		else {
			sys->fprint(stderr, "cs: already started\n");
			return;
		}
	}
	sys->bind("#s", "/net", Sys->MBEFORE);
	file := sys->file2chan("/net", "cs");
	if(file == nil) {
		sys->fprint(stderr, "cs: failed to make file: %r\n");
		return;
	}

	ldqi();

	nextsrv(ctxt, args);
	spawn cs(file);
}

strictsubl(l1, l2 : list of string) : (list of string, list of string)
{
  l : list of string;
  for(; l2 != nil; l2 = tl l2)
    if (findl(e := hd l2, l1))
      l = e :: l;
    else
      break;
  return (l, l2);
}

findl(e : string, l : list of string) : int
{
  for(; l != nil; l = tl l)
    if (e == hd l)
      return 1;
  return 0;
}

include "nextsrv.m";
ns : Nextsrv;
nextsrv(ctxt : ref Context, args : list of string)
{
  if (args == nil)
    return;
  npath := hd args;
  ns = load Nextsrv npath;
  if (ns == nil)
    sys->fprint(stderr, "cs: failed to load %s %r\n", npath);
  else
    ns->init(ctxt, args);
}

cs(file: ref FileIO)
{
  off, nbytes, fid : int;
  rc : Sys->Rread;
  wc : Sys->Rwrite;
  buf : array of byte;

  # Monitor for rlist queue
  reqch = chan of int;
  freech = chan of int;	

  spawn cs_mon();
  for (;;) {
    alt {
      (off, buf, fid, wc) = <-file.write =>
	cleanfid(fid);
	if(wc == nil)
	  break;
	if(ns != nil && srvipp(buf))
	  ns->srv();
	spawn process(buf, fid, wc);
	buf = nil;

      (nil, nbytes, fid, rc) = <-file.read =>
	if (rc != nil) {
	  spawn result(off, fid, nbytes, rc);
	  continue;
	}
    }
  }
}

reqch, freech : chan of int;

cs_mon()
{
    while(1) {

	# Wait for request
	<- reqch;

	# Wait for free
	<- freech;
    }
}

cleanfid(fid: int)
{
  reqch <- = 1;
  new : list of ref Reply;

  for(s := rlist; s != nil; s = tl s) {
    r := hd s;
    if(r.fid != fid)
      new = r :: new;
    else
      watchtime(r, 0);
  }

  rlist = new;
  freech <- = 1;
}

watcher()
{
  while(activep()) {
    sys->sleep(Timeout);
    watchtimers();
  }
}

activep() : int
{
  active := 0;
  reqch <-= 1;
  if (rlist != nil)
    active = 1;
  freech <-= 1;
  return active;
}

watchtimers()
{
  new : list of ref Reply = nil;
  reqch <- = 1;
  ct := sys->millisec();
  for(s := rlist; s != nil; s = tl s) {
    r := hd s;
    if (!watchtime(r, ct))
      new = r :: new;
  }
  rlist = new;
  freech <- = 1;
}

# time left to read your reply after DNS responds
Timeout : con 10000;

watchtime(r : ref Reply, ct : int) : int
{
  if ((ct == 0 && r.wait != 0) || (1 < r.wait && r.wait < ct)) {
    # too late - free send process
    <- r.rchan;
    r.wait = 0;
    r.err = "timeout";
    return 1;
  }
  return 0;
}

watch(r : ref Reply)
{ 
  reqch <- = 1;
  if (r.wait == 1)
    r.wait = sys->millisec() + Timeout;
  freech <- = 1;
}

unwatch(r : ref Reply) : int
{
  ready := 0;
  reqch <- = 1;
  if (r.wait) {
    ready = 1;
    r.wait = 0;
  }
  freech <- = 1;
  return ready;
}

result(off, fid, nbytes: int, rc : chan of (array of byte, string))
{
	r: ref Reply;
	err, ipaddr: string;
	s: list of ref Reply;
	for(s = rlist; s != nil; s = tl s) {
	  r = hd s;
	  if(r.fid == fid) {
	    if (unwatch(r))
	      <- r.rchan;
	    if (r.addrs != nil) {
	      ipaddr = hd r.addrs;
	      r.addrs = tl r.addrs;
	    }
	    else
	      err = r.err;
	    if (r.addrs == nil) {
	      cleanfid(fid);
	    }
	    break;
	  }
	}

	off = 0; # offset from file2chan is inconsistent (mr)

        # make sure this is the last thing done since it frees the fid.
        if (ipaddr == nil) {
	  if (err == nil)
	    err = "can't translate address";
	  rc <-= (nil, err);
	}
        else {
          rc <-= srv->reads(ipaddr, off, nbytes);
        }
}

#
# Process the write request. Create a channel to synchronize the read
# request.  Place the reply adt on the reply list with a valid fid and
# sync channel.  Then release the users write request.
#
process(data : array of byte, fid :int, wc : chan of (int, string))
{
  # Channel to sync on result completion
  rchan := chan of int;

  r := ref Reply;
  r.rchan = rchan;
  r.wait = 1;
  r.fid = fid;
  
  reqch <- = 1;
  if (rlist == nil)
    spawn watcher();
  rlist = r :: rlist;
  freech <- = 1;

  # Must create an entry before returning to the user
  wc <-= (len data, nil);

  (addrs, err) := xlate(data);
  r.addrs = addrs;
  r.err = err;
  if (err != nil)
    sys->fprint(stderr, "cs: %s\n", err);

  # Set read result expiration time
  watch(r);

  # Let the reader know the information is ready
  r.rchan <-= 1;
}

NL : list of string;
netwp(s : string) : int
{
  if (findl(s, NL))
    return 1;
  # udp and tcp pass this test in emu even before /net is bound!
  if (sys->open("/net/"+s+"/clone", Sys->ORDWR) != nil) {
    NL = s :: NL;
    return 1;
  }
  return 0;
}

srvipp(data : array of byte) : int
{
  if (len data < 3)
    return 0;
  return netipp(string data[0:3]);
}

netipp(s : string) : int
{
  return s == "net" || s == "tcp" || s == "udp";
}

xlate(data: array of byte): (list of string, string)
{
	n: int;
	l, rl: list of string;
	repl, netw, mach, service, s: string;

	(n, l) = sys->tokenize(string data, "!\n");
	if(n < 2)
		return (nil, "bad format request");

	netw = hd l;
        if (netw == "net")
		netw = "tcp";
	if (!netwp(netw))
		return (nil, "network unavailable "+netw);

	if(!netipp(netw)) {
		repl = "/net/" + netw + "/clone " + s;
		return (repl :: nil, nil);
	}

	if(n != 3)
		return (nil, "bad format request");
	mach = hd tl l;
	service = hd tl tl l;

	if(mach == "*")
		l = "" :: nil;
	else
	if(isipaddr(mach) == 0) {
		# Symbolic server == "$SVC"
		if(mach[0] == '$'
		   && (mach = kvmapmt("/services/cs/db",mach)) == nil)
		  return (nil, "unknown service");
		l = srv->iph2a(mach);
		if(l == nil)
			return (nil, "unknown host");
	}
	else
		l = mach :: nil;

	if(numeric(service) == 0) {
		service = srv->ipn2p(netw, service);
		if(service == nil)
			return (nil, "bad service name");
	}

	while(l != nil) {
		s = hd l;
		l = tl l;
		if(s != nil)
			s[len s] = '!';
		s += service;

		repl = "/net/" + netw + "/clone " + s;
		if (verbose) sys->fprint(stderr, "cs: %s!%s!%s -> %s\n",
					netw, mach, service, repl);

		rl = repl :: rl;
	}

	if (rl != nil && tl rl != nil)
	  return (reverselos(rl), nil);
	else
	  return (rl, nil);
}

reverselos(l: list of string) : list of string
{
  t : list of string;
  for(; l != nil; l = tl l)
    t = hd l :: t;
  return t;
}

isipaddr(a: string): int
{
	i, c: int;

	for(i = 0; i < len a; i++) {
		c = a[i];
		if((c < '0' || c > '9') && c != '.')
			return 0;
	}
	return 1;
}

numeric(a: string): int
{
	i, c: int;

	for(i = 0; i < len a; i++) {
		c = a[i];
		if(c < '0' || c > '9')
			return 0;
	}
	return 1;
}

#
# Maps key strings to value strings from flat file database.
#

# obsolete dbfile reload fails in namespace
mtime: int;

# namespace-safe dbfile reload strategy - obc
dbcdir : ref Cdir;

ldqi()
{
  if((qi = load Qidcmp Qidcmp->PATH) == nil)
    sys->fprint(stderr, "cs: load %s%r\n", Qidcmp->PATH);
  else
    qi->init(nil, nil);
}

KVpair: adt {
key:	string;
value:	string;
};
kvlist: list of KVpair;

lastdb : string;
kvopen(path : string) : int
{
  lastdb = path;
  if (sys == nil) {
    sys = load Sys Sys->PATH;
    stderr = sys->fildes(2);
    ldqi();
  }

  if (qi != nil && dbcdir == nil)
    dbcdir = ref Cdir(nil, Qidcmp->SAME);

  l : list of KVpair;
  readp := 0;

  (n, dir) := sys->stat(path);
  if (n < 0) {
    sys->fprint(stderr, Mod+": kvopen: stat %s %r\n", path);
    kvlist = nil;
    return n;
  }
  if (dbcdir != nil)
    readp = dbcdir.cmp(ref dir);
  else {
    # obsolete strategy
    if(mtime == 0 || mtime != dir.mtime)
      readp = 1;
  }

  if (readp) {
    (n, l) = readkvlist(path);
    if (n < 0) kvlist = nil;
    else kvlist = l;
  }
  if (n >= 0) return 0;
  return n;
}

kvmapmt(path, key : string) : string
{
  kvopen(path);
  return kvlistmap(key, kvlist);
}

kvmap(key: string): string
{
  if (lastdb != nil)
    return kvmapmt(lastdb, key);
  sys->fprint(stderr, Mod+": kvmap: attempt to map key %s from unopened file\n", key);
  return nil;
}

kvlistmap(key: string, l : list of KVpair) : string
{
  for(; l != nil; l = tl l)
    if((hd l).key == key) return (hd l).value;
  return nil;
}

include "rtoken.m";
rt : Rtoken;
Id : import rt;

readkvlist(path : string) : (int, list of KVpair)
{
  if (rt == nil) {
    rt = load Rtoken Rtoken->PATH;
    if (rt == nil) {
      sys->fprint(stderr, Mod+": kvopen: load %s %r\n", Rtoken->PATH);
      return (-1, nil);
    }
  }
  fd := sys->open(path, Sys->OREAD);
  if (fd == nil) {
    sys->fprint(stderr, Mod+": kvopen %s %r\n", Rtoken->PATH);
    return (-1, nil);
  }
  id := rt->id();
  env : list of KVpair;
  cnt := 0;
  while((t := rt->readtoken(fd, "\n", id)) != nil) {
    if (t[0] == '#') continue;
    (n, el) := sys->tokenize(t, " \t\r\n");
    if (n == 0)	# blank line
      continue;
    if (n != 2) {
      sys->fprint(stderr, Mod+": kvopen: in %s record with %d fields\n", path, n);
      return (-1, nil);
    }
    env = KVpair(hd el, hd tl el) :: env;
    cnt++;
  }
  if (id.n < 0) cnt = id.n;
  return (cnt, env);
}
