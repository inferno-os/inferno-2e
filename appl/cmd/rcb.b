###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### Copyright (C) 1997 Lucent Technologies
###
### Written by B.A. Westergren
### ed -obc
implement Rcb;

Mod : con "Rcb";

include "sys.m";
	sys: Sys;
	stderr: ref Sys->FD;

include "draw.m";
	Context: import Draw;

include "keyring.m";

include "security.m";

Rcb: module
{
  init :	fn(ctxt: ref Context, argv: list of string);
  realinit :	fn(ctxt: ref Draw->Context, argv: list of string) : int;
};

service : con "6675";
countretry : con 12;
timeretry : con 10000;

init(ctxt : ref Draw->Context, args : list of string)
{
  if(!realinit(ctxt, args))
    exit;
}

realinit(nil: ref Context, argv: list of string) : int
{
  sys = load Sys Sys->PATH;
  stderr = sys->fildes(2);

  if (argv != nil)
    argv = tl argv;

  alg := Auth->NOSSL;
  while(argv != nil) {
    s := hd argv;
    if(s[0] != '-')
      break;
    case s[1] {
      'C' =>
	alg = s[2:];
        if(alg == nil || alg == "") {
	  argv = tl argv;
	  if(argv != nil)
	    alg = hd argv;
	  else
	    return usage();
	}
	* =>
        return usage();
    }
    argv = tl argv;
  }
  
  mach := "$RCBSERVER";
  args : string;
  if(argv != nil) {
    case len argv {
      0 =>
	return usage();
      1 =>
	return usage();
      * =>
	mach = hd argv;
	a := tl argv;
	while(a != nil) {
	  args += " " + hd a;
	  a = tl a;
	}
    }
  }
  else
    return usage();
  
  result : int;
  c : sys->Connection;
  
  addr := "tcp!"+mach;
  
  i : int;
  sys->sleep(timeretry/3);
  for (i = 0; i < countretry; i++) {
    (result, c) = sys->dial(addr+"!"+service, nil);
    if(result > -1)
      break;
    sys->sleep(timeretry);
  }
  if(result < 0) {
    sys->fprint(stderr, "Error["+Mod+"]: dial server failed: %r\n");
    return 0;
  }

  kr := load Keyring Keyring->PATH;
  if(kr == nil){
    sys->fprint(stderr, "Error["+Mod+"]: can't load module Keyring %r\n");
    return 0;
  }

  user := user();
  kd := "/usr/" + user + "/keyring/";
  cert := kd + addr;
  (ok, nil) := sys->stat(cert);
  if(ok < 0){
    cert = kd + "default";
    (ok, nil) = sys->stat(cert);
    if(ok<0)
      sys->fprint(stderr, "Warning: no certificate found in %s; use getauthinfo\n", kd);
  }

  ai := kr->readauthinfo(cert);
  if(ai == nil){
    sys->fprint(stderr, "Error["+Mod+"]: certificate for %s not found.\n", addr);
    return 0;
  }

  au := load Auth Auth->PATH;
  if(au == nil){
    sys->fprint(stderr, "Error["+Mod+"]: can't load module Login %r\n");
    return 0;
  }

  err := au->init();
  if(err != nil){
    sys->fprint(stderr, "Error["+Mod+"]: %s\n", err);
    return 0;
  }

  fd := ref Sys->FD;
  (fd, err) = au->client(alg, ai, c.dfd);
  if(fd == nil){
    sys->fprint(stderr, "Error["+Mod+"]: authentication failed: %s\n", err);
    return 0;
  }

  # this will only cast 252 bytes maximum
  #t := array of byte sys->sprint("%d\n%s\n", len (array of byte args)+1, args);

  # Calculate the length of the args to write
  size := len args;
  (b1, b2) := twobytes(size);
  buf := array[size+2] of byte;
  buf[0] = b1;
  buf[1] = b2;
  buf[2:] = array of byte args;

  if (sys->write(fd, buf, len buf) != len buf) {
    sys->fprint(stderr, "Error["+Mod+"]: length args write, %r\n");
    return 0;
  }

  if(sys->export(fd, sys->EXPASYNC) < 0) {
    sys->fprint(stderr, "Error["+Mod+"]: export: %r\n");
    return 0;
  }
  return 1;
}

twobytes(length : int) : (byte, byte)
{
  q1 := length >> 8;
  q2 := length - (q1 << 8);
  return (byte q1, byte q2);
}

usage() : int
{
  sys->fprint(stderr, "Usage: rcb [-C cryptoalg] mach cmd(s)\n");
  return 0;
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
