### Written by B.A. Westergren
### ed - obc

implement Netready;

include "sys.m";
	sys: Sys;
	stdin, stdout, stderr: ref Sys->FD;

include "draw.m";

include "keyring.m";
	kr: Keyring;

include "security.m";
	login: Login;
	ssl: SSL;

include "promptstring.b";

Cs: module
{
  PATH :        con "/dis/lib/cs.dis";
  init :	fn(nil: ref Draw->Context, nil: list of string);
};

Netready: module
{
  init :	fn(ctxt: ref Draw->Context, argv: list of string);
  realinit :   fn(ctxt: ref Draw->Context, argv: list of string) : int;
};

RSrv: module
{
  PATH :   con "/dis/lib/rcbsrv.dis";
  init :   fn(ctxt: ref Draw->Context, argv: list of string);
  realinit :   fn(ctxt: ref Draw->Context, argv: list of string) : int;
};

usage()
{
  sys->print("Usage:\nnetready [options]\n\t-l loginname\n\t-p password\n\t-q quick\n");
  sys->print("\t [RcbSrv options]\n\t-o\t#for ever {default}\n\t-!o\t#not for ever\n\t-t timeout\n");
  exit;
}

logretry : con 3;

init(ctxt: ref Draw->Context, args : list of string)
{
  if(!realinit(ctxt, args))
    exit;
}

realinit(ctxt: ref Draw->Context, args : list of string) : int
{
  sys = load Sys Sys->PATH;
  if(sys == nil)
    exit;

  stdin = sys->fildes(0);
  stdout = sys->fildes(1);
  stderr = sys->fildes(2);
  
  if (sys->open("/net/tcp", Sys->OREAD) == nil)
    if(!dobind("#I", "/net", Sys->MAFTER))
      return 0;
  if(!dobind("#D", "/n/ssl", Sys->MREPL))
    return 0;

  ssl = load SSL SSL->PATH;
  if(ssl == nil){
    sys->fprint(stderr, "Error[Netready]: can't load module ssl\n");
    return 0;
  }

  kr = load Keyring Keyring->PATH;

  login = load Login Login->PATH;
  if(login == nil){
    sys->fprint(stderr, "Error[Netready]: can't load module Login\n");
    return 0;
  }

  error := login->init();
  if(error != nil){
    sys->fprint(stderr, "Netready: %s\n", error);
    return 0;
  }

  rsrv := load RSrv RSrv->PATH;
  if(rsrv == nil){
    sys->fprint(stderr, "Error[Netready]: can't load module RSrv\n");
    return 0;
  }

  forever := 1;
  auto := 0;
  user, passwd, signer, quick : string = "";
  rargs : list of string;
  for (; args != nil; args = tl args) {
    arg := hd args;
    if (arg == "-help" || arg == "?")
      usage();
    # collect rcbsrv args
    else if (arg == "-!o")
      forever = 1;
    else if (arg == "-o")
      forever = 0;
    else if (eqarg(arg, "-t")) {
      (arg, args) = getstringarg(arg, "-t", "", args);
      rargs = "-t" :: arg :: rargs;
    }
    else if (eqarg(arg, "-C")) {
      (arg, args) = getstringarg(arg, "-C", "", args);
      rargs = "-C" :: arg :: rargs;
    }
    # autologin args
    else if (eqarg(arg, "-l")) {
      auto |= 1;
      (user, args) = getstringarg(arg, "-l", user, args);
    }
    else if (eqarg(arg, "-p")) {
      auto |= 2;
      (passwd, args) = getstringarg(arg, "-p", passwd, args);
    }
    else if (eqarg(arg, "-s"))
      (signer, args) = getstringarg(arg, "-s", signer, args);
    else if (eqarg(arg, "-q"))
      (quick, args) = getstringarg(arg, "-q", quick, args);
  }
  # complete rcbsrv args
  if (!forever)
     rargs = "-o" :: rargs;

  # Start connection services
  if (quick == nil) {
    cs := load Cs Cs->PATH;
    if(cs == nil) {
      sys->print("Error[Netready]: cs module load failed: %r\n");
      return 0;
    }
    (nocs, nil) := sys->stat("/net/cs");
    if (nocs)
      cs->init(ctxt, nil);
  }

  pids : list of int;

  # Signer finder
  if (signer == nil)
    signer = login->defaultsigner();
  if (signer == nil) {
    signer = promptstring("Use Signer", signer, RAWOFF);
    ch := chan of int;
    spawn save2file("/services/cs", "db", ch);
    # Need to be able to know when the file2chan is ready.
    pids = <-ch :: pids;
    fd := sys->create("/services/cs/db", sys->OWRITE, 8r777);
    if (fd != nil)
      sys->fprint(fd, "$SIGNER\t%s\n", signer);
    fd = nil;
  }

  port := "!inflogin";
  if (quick != nil)
    port = "!6673";
  (n, nil) := sys->tokenize(signer, "!");
  case n {
    1 =>
      signer = "tcp!"+signer+port;
    2 =>
      signer = signer+port;
    3 => ;
    * => 
      sys->fprint(stderr, "Error[Netready]: unexpected signer %s\n", signer);
      killpids(pids);
      return 0;
  }

  # connect to signer server
  if (quick == nil)
    sys->print("Using Signer %s\n", signer);

  info : ref Keyring->Authinfo;
  i : int;
  
  authpath : string;
  pid : int;
  for(i = 0; i < logretry; i++) {
    (ok, lc) := sys->dial(signer, nil);
    if(ok < 0){
      sys->fprint(stderr, "Error[Netready]: dial login daemon failed %r\n");
      killpids(pids);
      return 0;
    }

    (err, c) := ssl->connect(lc.dfd);
    if(c == nil){
      sys->fprint(stderr, "Error[Netready]: can't push ssl: %s\n", err);
      killpids(pids);
      return 0;
    }
    lc.dfd = nil;
    lc.cfd = nil;

    if (auto != 3) {
      if (user == nil)
        user = getuser();
      user = promptstring("Login", user, RAWOFF);
      passwd = promptstring("Password", passwd, RAWON);
    }

    dir := "/usr/"+user+"/keyring/";
    file := "default";

    if (quick == nil) {
      ensuredir(user, "keyring");
    }
    else
      dir = "/usr/inferno/keyring/";
      
    authpath = dir+file;

    agf := "agreement.sig";

    sigch := chan of int;
    spawn save2file(dir, agf, sigch);
    # Need to be able to know when the file2chan is ready.
    pid = <-sigch;

    # handshake by telling server who you are 
    (c, err) = login->chello(user, dir+agf, c);

    # Do not need the save2file agreement file anymore
    killpid(pid);

    if(c == nil){
      sys->fprint(stderr, "Error[Netready]: %s\n", err);
      continue;
    }
    
    kr->putstring(c.dfd, "agree");

    # request certification
    (info, err) = login->ckeyx(user, passwd, c);

    if(info != nil) {
      if (quick == nil)
        setuserid(user);

      ch := chan of int;
      spawn save2file(dir, file, ch);
      # Need to be able to know when the file2chan is ready.
      pids = <- ch :: pids;

      if(kr->writeauthinfo(dir+file, info) < 0) {
	sys->fprint(stderr, "Error[Netready]: writeauthinfo to %s failed: %r\n", dir+file);
	killpids(pids);
	return 0;
      }
    }
    else {
      sys->fprint(stderr, "Error[Netready]: Failed to get authinfo packet\n");
      continue;
    }
    break;
  }

  if (i >= logretry) {
    sys->fprint(stderr, "Error[Netready]: Login retry count exhausted\n");
    killpids(pids);
    return 0;
  }
    
  result := rsrv->realinit(ctxt, "RcbSrv" :: "-s" :: signer :: "-a" :: authpath :: "-q" :: quick :: rargs);
  spawn killpids(pids);
  return result;
}

killpids(pids : list of int)
{
  for(;pids != nil; pids = tl pids)
    killpid(hd pids);
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

ensuredir(user, subdir : string)
{
  fd : ref sys->FD;
  ok : int;

  upath := "/usr/"+user;
  path := upath+"/"+subdir;
  # if path is already available then return
  (ok, nil) = sys->stat(path);
  if (ok == 0)
    return;

  # otherwise check to see if this is a new user and create a directory
  # if it is not there
  (ok, nil) = sys->stat(upath);
  if (ok == -1) {
    fd = sys->create(upath, sys->OREAD, Sys->CHDIR | 8r777);
    if (fd == nil) {
      sys->print("Error[Netready]: creating directory %s\n", upath);
      return;
    }
  }

  # Now create the proper subdirectory
  fd = sys->create(path, sys->OREAD, Sys->CHDIR | 8r777);
  if (fd == nil) {
    sys->print("Error[Netready]: creating directory %s\n", path);
    return;
  }
}

getuser(): string
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

setuserid(usr : string) : int
{
  fd := sys->open("/dev/user", sys->OWRITE);
  if(fd == nil) {
    sys->print("Error[Netready]: failed to open /dev/user: %r\n");
     return 0;
  }

  b := array of byte usr;
  if(sys->write(fd, b, len b) < 0) {
    sys->print("Error[Netready]: failed to write /dev/user, %r\n");
    return 0;
  }

  return 1;
}

save2file(dir, file: string, ch :chan of int)
{
  pid := sys->pctl(0, nil);

  if(sys->bind("#s", dir, Sys->MBEFORE) < 0){
    sys->fprint(stderr, "Error[Netready]: can't bind file channel %r\n");
    ch<- = pid;
    return;
  }
  fileio := sys->file2chan(dir, file);
  if(fileio == nil){
    sys->fprint(stderr, "Error[Netready]: file2chan failed %r\n");
    ch<- = pid;
    return;
  }

  data : array of byte;
  off, nbytes, fid : int;
  rc : Sys->Rread;
  wc : Sys->Rwrite;

  infodata := array[0] of byte;

  ch<- = pid;

  sys->pctl(Sys->NEWPGRP, nil);

  for(;;) alt {
    (off, nbytes, fid, rc) = <-fileio.read =>
      if(rc == nil)
	break;
    if(off > len infodata){
      rc <-= (infodata[off:off], nil);
    }
    else {
      if(off + nbytes > len infodata)
	nbytes = len infodata - off;
      rc <-= (infodata[off:off+nbytes], nil);
    }
    
    (off, data, fid, wc) = <-fileio.write =>
      if(wc == nil)
	break;
    if(off != len infodata){
      wc <-= (0, "cannot be rewritten");
    }
    else {
      nid := array[len infodata+len data] of byte;
      nid[0:] = infodata;
      nid[len infodata:] = data;
      infodata = nid;
      wc <-= (len data, nil);
    }
    data = nil;
  }
}

killpid(pid : int)
{
  #sys->print("kill: %d\n", pid);
  spid := string pid;
  kfd := sys->open("#p/"+spid+"/ctl", sys->OWRITE);
  if(kfd == nil) {
    sys->fprint(stderr, "Error[Netready]: Cannot kill process %s, %r\n", spid);
    return;
  }

  sys->fprint(kfd, "kill");
}  

dobind(a, b : string, c : int) : int
{
  if (sys->bind(a, b, c) < 0) {
    sys->fprint(stderr, "Error[Netready]: cannot bind %s: %r", a);
    return 0;
  }
  return 1;
}
