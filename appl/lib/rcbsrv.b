###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### Copyright (C) 1997 Lucent Technologies
###
### Written by B.A. Westergren
### ed - obc
implement RcbSrv;

include "sys.m";
	sys: Sys;
	stderr: ref Sys->FD;

include "draw.m";

include "keyring.m";
	auth: Auth;
	kr: Keyring;

include "security.m";
	login: Login;
	ssl: SSL;

include "sh.m";

include "csget.m";
csget : CsGet;

service : con "6675";
User : string;

RcbSrv: module {
  init :     fn(ctxt: ref Draw->Context, argv: list of string);
  realinit :     fn(ctxt: ref Draw->Context, argv: list of string) : int;
};

init(ctxt : ref Draw->Context, args : list of string)
{
  if(!realinit(ctxt, args))
    exit;
}

realinit(ctxt : ref Draw->Context, args : list of string) : int
{
	sys = load Sys Sys->PATH;
	if(sys == nil)
	  exit;

	stderr = sys->fildes(2);

	kr = load Keyring Keyring->PATH;
	if(kr == nil){
	  sys->fprint(stderr, "Error[RcbSrv]: can't load module Keyring: %r\n");
	  return 0;
	}

	login = load Login Login->PATH;
	if(login == nil){
		sys->fprint(stderr, "Error[RcbSrv]: can't load module Login\n");
		return 0;
	}

	error := login->init();
	if(error != nil){
		sys->fprint(stderr, "Error[RcbSrv]: %s\n", error);
		return 0;
	}

	getargs(args);

	sys->pctl(Sys->NEWPGRP, nil);

	return gensrv(ctxt, "tcp", service);
}

readargs(fd : ref Sys->FD) : list of string
{
  bLength := array[2] of byte;
  n := sys->read(fd, bLength, len bLength);
  if (n != 2) {
    sys->fprint(stderr, "Error[RcbSrv]: Reading packet length buffer, %r\n");
    return nil;
  }
  bsize := replysize(bLength[0], bLength[1]);
  rinit(1024);
  (nbytes, bOutput) := readchunk(fd, bsize);
  if (nbytes != bsize) {
      sys->fprint(stderr, "Error[RcbSrv]: Reading args, total read %d, expected %d %r\n", nbytes, bsize);
      return nil;
  }
  return unpack(bOutput);
}

min(a, b : int) : int
{
  if (a > b)
    return b;
  return a;
}

BSZ := 64;
rinit(sz : int)
{
  if (sys == nil)
    sys = load Sys Sys->PATH;
  if (sz == 0)
    return;
  if (sz > 0 && sz <= Sys->ATOMICIO)
    BSZ = sz;
  else
    sys->print("Error[Rdb]: bad buffer size %d\n", sz);
}

readchunk(fd : ref sys->FD, sz : int) : (int, array of byte)
{
  rinit(0);
  ar := array[sz] of byte;
  l, n, p : int = 0;
  while((l = min(sz, BSZ)) > 0 && (n = sys->read(fd, ar[p:p+l], l)) >= 0) {
    p += n;
    sz -= n;
  }
  if (n >= 0)
    return (p, ar[0:p]);
  return (n, nil);
}

unpack(buf : array of byte) : list of string
{
  if (buf == nil)
    return nil;

  (n, cmds) := sys->tokenize(string buf, "\n");
  return cmds;
}

replysize(q1 : byte, q2 : byte) : int
{
  return (int q1)<<8|(int q2);
}

getuser(): string
{
	sys = load Sys Sys->PATH;

	fd := sys->open("/dev/user", sys->OREAD);
	if(fd == nil){
	  sys->fprint(stderr, "Error[RcbSrv]: can't open /dev/user %r\n");
	  return "inferno";
	}

	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0){
	  sys->fprint(stderr, "Error[RcbSrv]: failed to read /dev/user %r\n");
	  return "inferno";
	}

	return string buf[0:n];	
}

gensrv(ctxt : ref Draw->Context, net, service: string) : int
{
  (ok, c) := sys->announce(net+"!*!"+service);
  if(ok < 0) {
    sys->fprint(stderr, "Error[RcbSrv]: can't announce %s service: %r\n", service);
    return 0;
  }
  result := 1;
  while ((result = gendoer(ctxt, c)) && forever);
  return result;
}
 
killus()
{
  pid := string sys->pctl(0, nil);
  fd := sys->open("#p/"+pid+"/ctl", sys->OWRITE);
  if(fd != nil)
    sys->fprint(fd, "kill");
}

reset : int;

gendoer(ctxt : ref Draw->Context, c: sys->Connection) : int
{
  if (timer != nil) {
    forever = 0;
    pid := string sys->pctl(0, nil);
    spawn clock(timer, pid);
  }
  reset = 1;
  (ok, nc) := sys->listen(c);
  reset = 0;
  if(ok < 0) {
    sys->fprint(stderr, "listen: %r\n");
    return 0;
  }

  buf := array[64] of byte;
  l := sys->open(nc.dir+"/remote", sys->OREAD);
  n := sys->read(l, buf, len buf);
  remote := string buf[0:n];
  remotel : list of string;
  (nil, remotel) = sys->tokenize(remote, "!");
  if (remotel != nil)
    remote = hd remotel;
  if (remote == nil)
    remote = "$NOREMOTE";
  sys->print("Remote = %s, Signer = %s\n", remote, signer);

  if(n >= 0)
    sys->fprint(stderr, "RcbSrv: New client: %s %s\n", nc.dir, string buf[0:n]);
  else
    sys->fprint(stderr, "Error[RcbSrv]: New unknown client of (%s)\n", cmdname);
  l = nil;
  
  nc.dfd = sys->open(nc.dir+"/data", sys->ORDWR);
  if(nc.dfd == nil) {
    sys->fprint(stderr, "Error[RcbSrv]: open %s: %r\n", nc.dir);
    return 0;
  }
  
  ok = 0;
  if (isipaddr(signer))
    if (signer == remote)
      ok = 1;
  else {
    csget = load CsGet CsGet->PATH;
    if (csget == nil){
      sys->fprint(stderr, "Error[RcbSrv]: can't load CsGet\n");
      return 0;
    }
    (nil, signeraddr, nil) := csget->hostinfo(signer);
    if (signeraddr == remote)
      ok = 1;
  }

  if (ok)
    return dorstyx(ctxt, nc.dfd);
  else {
    sys->fprint(stderr, "Error[RcbSrv]: Request from unauthorized server (%s)\n", remote);
    return 0;
  }
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

dorstyx(ctxt : ref Draw->Context, fd: ref Sys->FD) : int
{
	# supported algorithms
	algs :=
	  Auth->NOSSL :: 
	  Auth->CLEAR :: 
	  Auth->SHA :: 
	  Auth->MD5 :: 
	  Auth->RC4 ::
	  Auth->SHA_RC4 ::
	  Auth->MD5_RC4 ::
	  nil;

	if (alg == Auth->NOAUTH)
	  algs = alg :: algs;

	User = getuser();

	ai : ref Keyring->Authinfo;
	if (authpath == nil)
	  authpath = "/usr/"+User+"/keyring/default";

	ai = kr->readauthinfo(authpath);
	if(ai == nil){
	  sys->fprint(stderr, "Error[RcbSrv]: readauthinfo failed for %s: %r\n", authpath);
	  return 0;
	}
	
	if (auth == nil) {
	  auth = load Auth Auth->PATH;
	  if(auth == nil){
	    sys->fprint(stderr, "Error[RcbSrv]: can't load module Auth: %r\n");
	    return 0;
	  }
	  err := auth->init();
	  if(err != nil){
	    sys->fprint(stderr, "Error[RcbSrv]: %s\n", err);
	    return 0;
	  }
	}

	err : string;
	# Cannot use auth->server since it assumes kr->auth(fd, ai, 1)
	(fd, err) = auth->serverwid(algs, ai, fd, 0);
	if(fd == nil){
	  sys->fprint(stderr, "Error[RcbSrv]: server auth: %s\n", err);
	  return 0;
	}
	if (alg != Auth->NOAUTH) {
	  # server returns string "id with security: alg" on success
	  id : string;
	  (n, id_and_alg) := sys->tokenize(err, " ");
	  if (n > 0)
	    id = hd id_and_alg;
	  if((id != "inferno") && (id != User)) {
	    sys->fprint(stderr, "Error[RcbSrv]: auth failed, unknown user %s\n", id);
	    return 0;
	  }
	}

	sys->pctl(sys->FORKFD, fd.fd :: nil);

	args := readargs(fd);
	if(args == nil){
		sys->fprint(stderr, "Error[RcbSrv]: Failed to readargs after auth: %r\n");
		return 0;
	}

	result := 1;
	if (authpath != nil)
	  authcmd(nil);
	for (; args != nil && result; args = tl args) {
	  cmdline := hd args;
	  if(!invoke(ctxt, cmdline)) {
	    sys->print("Rcbsrv: stopped at %s\n", cmdline);
	    result = 0;
	  }
	}

	killpids();

	if (!forever)
	  spawn killus();
	return result;
}

Mount: module
{
  PATH :  con "/dis/mount.dis";
  init :  fn(ctxt: ref Draw->Context, argv: list of string);
#  realinit :  fn(ctxt: ref Draw->Context, argv: list of string) : int;
#  mount : fn(flags : int, alg : string, addr : string, dir : string) : int;
};
mnt : Mount;

invoke(ctxt : ref Draw->Context, cmdline : string) : int
{
  (nil, args) := sys->tokenize(cmdline, " ");
  if (args == nil) {
    sys->fprint(stderr, "Error[RcbSrv]: exec: args: %r\n");
    return 0;
  }
    
  cmd := hd args;
  if (cmd == nil || cmd[0] == '#')
    return 1;
  # Assume that all auth files are named default!
  # authcmd(args);

  if (quick == nil)
    sys->fprint(stderr, "exec: %s\n", cmdline);
  case cmd {
    "mount" => {
      mnt = load Mount Mount->PATH;
      if (mnt == nil) {
	sys->fprint(stderr, "Error[RcbSrv]: Cannot load %s %r\n", Mount->PATH);
	return 0;
      }

      e := ref Sys->Exception;
      if(sys->rescue("fail:*", e) == Sys->EXCEPTION){
	sys->fprint(stderr, "%s:%s\n", "rcbsrv", e.name[5:]);
	return 0;
      }
      mnt->init(ctxt, Mount->PATH :: tl args);
      return 1;
    }
    "cd" => {
      if(len tl args != 1) {
        sys->fprint(stderr, "Error[RcbSrv]: cd: must have one argument");  
	return 0;
      }
      return (sys->chdir(hd tl args) >= 0);
    }
    # What external processes will be able to see updates
    # to the namespace from now on?  To implement a recovery strategy
    # thing about it.
    "new" =>
      return (sys->pctl(sys->NEWNS, nil) >= 0);
    "fork"  =>
      return (sys->pctl(sys->FORKNS, nil) >= 0);
    "nodev" =>
      return (sys->pctl(sys->NODEVS, nil) >= 0);
  }

  mod : Command;
  # make sure command has ".dis" as its suffix
  if ((len cmd > 0) && (len cmd < 4))
    cmd = cmd + ".dis";
  else {
    if ((len cmd >= 4) && (cmd[(len cmd)-4:] != ".dis"))
      cmd = cmd + ".dis";
  }

  mod = load Command cmd;
  if(mod == nil) {
    mod = load Command "/dis/"+cmd;
    if(mod == nil) {
      sys->fprint(stderr, "Error[RcbSrv]: %s not found: %r\n", cmd);
      return 0;
    }
  }

  mod->init(ctxt, args);
  # Since init cannot return value yet
  return 1;
}

authcmd(args : list of string)
{
  # get the current user just in case it changed, ie. kr->auth
  destdir := "/usr/"+getuser()+"/keyring/";
  src := destdir+"default";
  origdir := "";
  authdir := "";
  if (authpath != nil) {
    (nil, authdir) = fileofdir(authpath);
    origdir = destdir;
    src = authpath;
    destdir = authdir;
  }

  if(origdir != nil && origdir != authdir) {
    if (quick == nil)
      sys->print("bind -b %s %s\n", authdir, origdir);
    sys->bind(authdir, origdir, sys->MBEFORE);
  }

  if (args == nil)
    return;

  # Monitor for tcp! arguments since we need to temporarily save
  # an auth file for its subsequent usage.
  addr := tcpmember(args);
  if (addr == nil)
    return;

  dest := destdir+addr;
  ai := kr->readauthinfo(dest);
  if(ai == nil) {
    # Use a channel to make sure the file2chan is ready so the 
    # writeauthinfo will complete successfully.
    ch := chan of int;
    spawn save2file(destdir, addr, ch);
    <-ch;
    ai = kr->readauthinfo(src);
    if(ai != nil) {
      if(kr->writeauthinfo(dest, ai) < 0)
	sys->fprint(stderr, "Error[RcbSrv]: writeauthinfo failed: %r\n");
    }
    else
      sys->fprint(stderr, "Error[RcbSrv]: Authinfo %s file not found\n", src);
  } 
}

fileofdir(file : string) : (string, string)
{
  dir := file;
  for (l := len file -1; l > 0; l--)
    if (file[l] == '/') {
      dir = file[0:l];
      file = file[l+1: len file];
      break;
    }
  #sys->print("file=%s, dir=%s\n", file, dir);
  return (file, dir);
}

tcpmember(plist : list of string) : string
{
  if (plist == nil)
    return nil;
  for(; plist != nil; plist = tl plist) {
    item := hd plist;
    if ((len item > 4) && ("tcp!" == item[0:4]))
      return item;
  }
  return nil;
}

# set level of security after authentication
pushssl(fd: ref Sys->FD, secret: array of byte, algstr: string): (ref Sys->FD, string)
{
	err: string;
	c: ref Sys->Connection;
	algfd: ref Sys->FD;

	algfd = fd;

	(n, algs) := sys->tokenize(algstr, "/");
	if(n == 0 || algs == nil)
		return (nil, sys->sprint("can't parse algorithms %r"));

	# must do this yourself in your application
	# if(sys->bind("#D", "/n/ssl", Sys->MREPL) < 0)
	#	return (nil, sys->sprint("can't bind #D: %r"));

	for(;;)
	{
		alg := hd algs; 

		(err, c) = ssl->connect(algfd);
		if(c == nil)
			return (nil, "can't connect ssl: " + err);

		err = ssl->secret(c, secret, secret);
		if(err != nil)
			return (nil, "can't write secret: " + err);

		if(sys->fprint(c.cfd, "alg %s", alg) < 0)
			return (nil, sys->sprint("can't push algorithm %s: %r", alg));

		algfd = c.dfd;
		if((algs = tl algs) == nil)
			break;
	}

	return (algfd, nil);
}

save2file(dir, file: string, cmdchan : chan of int)
{
  if(sys->bind("#s", dir, Sys->MBEFORE) < 0){
    sys->fprint(stderr, "Warning[RcbSrv]: can't bind file channel %r\n");
    return;
  }
  fileio := sys->file2chan(dir, file);
  if(fileio == nil){
    sys->fprint(stderr, "Warning[RcbSrv]: file2chan failed %r\n");
    return;
  }

  data : array of byte;
  off, nbytes, fid: int;
  rc : Sys->Rread;
  wc : Sys->Rwrite;

  infodata := array[0] of byte;

  sys->pctl(Sys->NEWPGRP, nil);

  pid := sys->pctl(0, nil);
  cmdchan<- = pid;
  savepid(pid);

  for(;;) alt {
    (off, nbytes, fid, rc) = <-fileio.read =>
      if(rc == nil)
	break;
      if(off > len infodata){
	rc <-= (infodata[off:off], nil);
      } else {
	if(off + nbytes > len infodata)
	  nbytes = len infodata - off;
	rc <-= (infodata[off:off+nbytes], nil);
      }
    
    (off, data, fid, wc) = <-fileio.write =>
      if(wc == nil)
	break;
    
      if(off != len infodata){
	wc <-= (0, "cannot be rewritten");
      } else {
	nid := array[len infodata+len data] of byte;
	nid[0:] = infodata;
	nid[len infodata:] = data;
	infodata = nid;
	wc <-= (len data, nil);
      }
      data = nil;
  }
}

pids : list of int;
savepid(pid : int)
{
  pids = pid :: pids;
  
}

killpids()
{
  for (; pids != nil; pids = tl pids) {
    spid := string hd pids;
    cmdfd := sys->open("#p/"+spid+"/ctl", sys->OWRITE);
    if(cmdfd == nil)
      sys->fprint(stderr, "Warning[Rcbsrv]: Opening pid %s, %r\n", spid);
    else
      sys->fprint(cmdfd, "kill");
  }
}

signer : string;
authpath : string;
cmdname : string;
timer : string;
forever : int;
alg : string;
quick : string;

getargs(args : list of string)
{	 
  forever = 1;

  if (args != nil) {
    cmdname = hd args;
    args = tl args;
  }
  for (; args != nil; args = tl args) {
    arg := hd args;
    if (arg == "-!o")
      forever = 1;
    else if (arg == "-o")
      forever = 0;
    else if (eqarg(arg, "-t")) {
      (timer, args) = getstringarg(arg, "-t", timer, args);
      if (timer == nil)
	usage("");
    }
    else if (eqarg(arg, "-C")) {
      (alg, args) = getstringarg(arg, "-C", alg, args);
      if (alg == nil)
	usage("");
    }
    else if (eqarg(arg, "-s")) {
      (signer, args) = getstringarg(arg, "-s", signer, args);
      if (signer == nil)
	usage("");
    }
    else if (eqarg(arg, "-a")) {
      (authpath, args) = getstringarg(arg, "-a", authpath, args);
      if (authpath == nil)
	usage("");
    }
    else if (eqarg(arg, "-q"))
      (quick, args) = getstringarg(arg, "-q", quick, args);
    else if (arg == "-help" || arg == "?")
      usage("");
    else 
      usage(arg);
  }
  getsigner();
}

getsigner()
{
  if (signer != nil) {
    (n, nl) := sys->tokenize(signer, "!");
    case n {
	1 => signer = hd nl;
	* => signer = hd tl nl;
    }
  }
  if (signer == nil) 
    signer = login->defaultsigner();
  if(signer == nil){
    sys->fprint(stderr, "Error[RcbSrv]: can't get default signer server name\n");
    signer = "$SIGNER";
  }
}

clock(timer : string, pid : string)
{
  secs := int timer;
  sys->sleep(secs*1000);

  if (reset) {
    sys->fprint(stderr, "RcbSrv: Timeout wait for request\n");
    fd := sys->open("#p/"+pid+"/ctl", sys->OWRITE);
    if(fd != nil) {
      # Must free the listen or the process will not really die!
      host := gethostname();
      sys->dial("tcp!"+host+"!"+service, nil);
      sys->fprint(fd, "kill");
    }
  }
}
	  
gethostname(): string
{
  buf := array[32] of byte;
  fd := sys->open("/dev/sysname", sys->OREAD);
  n := sys->read(fd, buf, 32);
  if (n <= 0)
	return "localhost";
  return string buf[0:n];
}

usage(arg : string)
{
  sys->fprint(stderr, "Usage: rcbsrv [-o] [-t secs] [-C noauth] [-s signer]\n");
  if (arg == nil)
    exit;
  sys->print("RcbSrv: unexpected argument %s\n", arg);
}

eqarg(arg : string, tag : string) : int
{
  ln := len tag;
  if (len arg >= ln && arg[0:ln] == tag)
    return 1;
  else
    return 0;
}

getstringarg(arg : string, tag : string, defval : string, args : list of string) : (string, list of string)
{
  ln := len tag;
  if(len arg > ln)
    return (arg[ln:], args);
  else if (tl args != nil) {
    next := hd tl args;
    if (tag[0] == next[0])
      return (nil, args);
    else
      return (hd tl args, tl args);
  }
  else
    return (defval, args);
}
