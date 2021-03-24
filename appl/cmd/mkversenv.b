# mkversenv [devtype] [file (default /version.env)]
# -obc
# add env fields to an env <file>
# type=<devtype>
# security=<u/i>
# build=<infx.x-yy>
# date=<dir.mfile>
#
implement Mkversenv;

Mod : con "mkversenv";

include "sys.m";
sys : Sys;
stderr : ref Sys->FD;

include "draw.m";

include "version.m";

Mkversenv: module
{
	VENV : con "/version.env";
        init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

rwa : con 8r666;
init(nil : ref Draw->Context, args : list of string)
{
  if (args != nil)
    args = tl args;
  sys = load Sys Sys->PATH;
  stderr = sys->fildes(2);

  if (args != nil && hd args == "-?") {
    sys->fprint(stderr, "usage: "+Mod+" [devtype] [file]\n");
    return;
  }
  file := VENV;
  typ : string;
  if (args != nil) {
    typ = hd args;
    if (tl args != nil)
      file = hd tl args;
  }

  (ok, dir) := sys->stat(file);
  if (ok < 0) {
    sys->create(file, sys->OWRITE, rwa);
    (ok, dir) = sys->stat(file);
    if (ok < 0) {
      sys->fprint(stderr, "Error["+Mod+"]: cannot create %s %r\n", file);
      return;
    }
  }
  dir.mode = rwa;
  if (sys->wstat(file, dir) < 0)
    sys->fprint(stderr, Mod+": can't wstat %s: %r\n", file);

  (env, l) := readenv(file);
  
  if (typ != nil)
    env = addenv(sys->sprint("type=%s", typ), env);

  (bld, secu) := getversion();
  if (bld != nil) {
    env = addenv(sys->sprint("build=inf%s", bld), env);
    env = addenv(sys->sprint("security=%s", secu), env);
  }

  env = addenv(sys->sprint("date=%d", dir.mtime), env);
  
  fd := sys->create(file, sys->OWRITE, rwa);
  
  if (fd == nil) {
    sys->fprint(stderr, "Error["+Mod+"]: cannot write %s %r\n", file);
    return;
  }
  printenv(env, fd);
}

getversion() : (string, string)
{
  bld, secu : string;
  vers := Version->VERSION;
  (n, lv) := sys->tokenize(vers, " \t");
  if (n < 4)
    sys->fprint(stderr, "Error["+Mod+"]: unexpected version.m %s, %d\n", vers, n);
  else {
    bld = hd tl lv;
    secu = hd tl tl tl lv;
    n = int secu;
    secu = secu[len string n:];
    bld += "-" + string n;
  }
  return (bld, secu);
}

addenv(vval : string, env : list of string) : list of string
{
  return rappend(rdelenv(vval, env), vval :: nil);
}

delenv(vval : string, env : list of string) : list of string
{
  return reverse(rdelenv(vval, env));
}

rdelenv(vval : string, env : list of string) : list of string
{
  res : list of string;
  for(; env != nil; env = tl env)
    if (!vvaleq(vval, hd env))
      res = hd env :: res;
  return res;
}

delim : con '=';
vvaleq(vval1, vval2 : string) : int
{
  return (p := pos(delim, vval1)) == pos(delim, vval2) && p >= 0 && vval1[0:p] == vval2[0:p];
}

readenv(path : string) : (list of string, int)
{
  (ok, dir) := sys->stat(path);
  if (ok < 0) {
    sys->fprint(stderr, "Error["+Mod+"]: %s %r\n", path);
    return (nil, 0);
  }
  l := dir.length;
  buf := array[l] of byte;
  fd := sys->open(path, Sys->OREAD);
  if (fd == nil) {
    sys->fprint(stderr, "Error["+Mod+"]: %s %r\n", path);
    return (nil, l);
  }
  n := sys->read(fd, buf, l);
  (nil, ll) := sys->tokenize(string buf, "\n");
  env : list of string;
  for(; ll != nil; ll = tl ll)
    if (pos(delim, hd ll) >= 0)
      env = hd ll :: env;
    else
      break;
  return (reverse(env), l);
}

printenv(env : list of string, fd : ref Sys->FD)
{
  for(; env != nil; env = tl env)
    sys->fprint(fd, "%s\n", hd env);
}

pos(e : int, s : string) : int
{
  for(i := 0; i < len s; i++)
    if (s[i] == e)
      return i;
  return -1;
}

reverse(l : list of string) : list of string
{
  r : list of string;
  for(; l != nil; l = tl l)
    r = hd l :: r;
  return r;
}

rappend(r, t : list of string) : list of string
{
  for(; r != nil; r = tl r)
    t = hd r :: t;
  return t;
}

append(h, t : list of string) : list of string
{
  r := reverse(h);
  for(; r != nil; r = tl r)
    t = hd r :: t;
  return t;
}
