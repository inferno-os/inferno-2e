implement CsGet;

include "sys.m";
  sys : Sys;
  stderr : ref Sys->FD;

include "draw.m";
include "string.m";
include "csget.m";

init(nil : ref Draw->Context, args : list of string)
{ 
  name : string = "";
  if (tl args != nil)
    name = hd tl args;
  (truename, address, other) := hostinfo(name);
  if (truename != nil) {
    sys->print("hostname: %s, address: %s, other addrs: ", truename, address);
    printlstring(other);
  }
}

BUFLEN : con 64;

hostinfo(name : string) : (string, string, list of string)
{
  if (sys == nil) {
    sys = load Sys Sys->PATH;
    if (sys == nil)
      return (nil, nil, nil);
    stderr = sys->fildes(2);
  }

  localname : string;
  if (name == nil) {
    localname = hostname();
  }

  address : string = nil;
  maddrs : list of string;

  address = bootpaddr();
  if (address != nil && (localname != nil || name == localname))
    return (localname, address, nil);
  if (name == nil)
    name = localname;

  fd := sys->open("/net/cs", sys->ORDWR);
  if (fd == nil) {
    sys->fprint(stderr, "Cs unavailable: %r\n");
    return (nil, nil, nil);
  }

  (n, nil) := sys->tokenize(name, "!");
  query : array of byte;
  if (n == 1)
    query = array of byte ("tcp!"+name+"!styx");
  else if (n == 3)
    query = array of byte (name);
  else {
    sys->fprint(stderr, "CsGet incomplete host name: %s\n", name);
    return (nil, nil, nil);
  }

  n = sys->write(fd, query, len query);
  if (n < 0) {
    sys->fprint(stderr, "Cs can't write: %r\n");
    return (nil, nil, nil);
  }
  addrl : list of string;

  buf := array[BUFLEN] of byte;
  for(;;) {
    n = sys->seek(fd, 0, sys->SEEKSTART);
    n = sys->read(fd, buf, len buf);
    if (n > 0) {
      address = string buf[0:n];
      (ok, addresses) := sys->tokenize(address, " \t");
      if (ok > 0) {
	if (tl addresses == nil || hd tl addresses == nil) {
	  sys->fprint(stderr, "CsGet bad address format: %s\n", address);
	  return (nil, nil, nil);
	}
        address = hd tl addresses;
	(nil, addrl) = sys->tokenize(address, "!");
	if (addrl != nil)
	  maddrs = hd addrl :: maddrs;
      }
    }
    else
      break;
  }
  if (maddrs == nil)
    return (name, nil, nil);
  maddrs = reverselstring(maddrs);
  return (name, hd maddrs, tl maddrs);
}

hostname() : string
{
  buf := array[BUFLEN] of byte;
  fd := sys->open("/dev/sysname", sys->OREAD);
  n := sys->read(fd, buf, len buf);
  if (n <= 0)
    return "*";
  else
    return string buf[0:n];
}

bootpaddr() : string
{
  fd := sys->open("/net/bootp", Sys->OREAD);
  if (fd == nil)
    return nil;

  match := retrieve(fd, "ipaddr" :: nil);
  (nil, lst) := sys->tokenize(match, " \t\n\r");  # to be safe
  if (lst != nil && tl lst != nil)
    return hd tl lst;
  else {
    sys->fprint(stderr, "[Bootpaddr] Error: bad ipaddr format%r\n");
    return nil;
  }
}

#
# Utilities
#

retrieve(fd : ref sys->FD, keys : list of string) : string
{
  for(;;) {
    (ok, result) := readtoken(fd, "\r\n");
    if (result != nil) {
      if (!commentp(result) && matchkeys(result, keys))
	return result;
    }
    else
      break;
  }
  return nil;
}

commentp(line : string) : int
{
  if(line != nil && line[0] == '#')
    return 1;
  return 0;
}

matchkeys(str : string, keys : list of string) : int
{
  for(; keys != nil; keys = tl keys) {
    key := hd keys;
    if (len str >= len key && str[0:len key] == key)
      return 1;
  }
  return 0;
}

BL : con 64;
readtoken(fd : ref sys->FD, tokens : string) : (int, string)
{
  #libinit();
  ar := array[BL] of byte;
  result, tmp : array of byte;
  n := 0;
  for((nr, tp, done) := (0, -1, 0); (n = sys->read(fd, ar, BL)) >= 0;) {
    for(i := 0; i < n; i++)
      if (memberp(int ar[i], tokens)) {
	if (tp >= 0) {
	  sys->seek(fd, (i-n)+1, Sys->SEEKRELA);
	  n = i;
	  done = 1;
	  break;
	}
      }
      else if (tp < 0)
	tp = i;
    if (tp >= 0 && n > tp) {
      (tmp, result) = (result, array[nr + (n-tp)] of byte);
      (result[0:], result[nr:]) = (tmp[0:nr], ar[tp:n]);
      nr += n-tp;
      tp = 0;
    }
    if (!n || done)
      return (nr, string result);
  }
  return (n, string result);
}

memberp(c : int, s : string) : int
{
  ls := len s;
  for(i := 0; i < ls; i++)
    if (c == s[i])
      return 1;
  return 0;
}

printlstring(args: list of string)
{
  sys->print("(");
  if (args != nil)
    for(;; args = tl(args)) {
      sys->print("%s", hd(args));
      if( tl(args) == nil)
	break;
      else
	sys->print(" ");
    }
  sys->print(")\n");
}

reverselstring(l: list of string) : list of string
{
  t : list of string;
  for(; l != nil; l = tl l)
    t = hd l :: t;
  return t;
}
