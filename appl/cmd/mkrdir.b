# make dirs recursively
# obc
implement Mkrdir;

Mod : con "mkrdir";

include "sys.m";
	sys: Sys;
	FD: import Sys;
	stderr: ref FD;

include "draw.m";
	Context: import Draw;

Mkrdir: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
	mkrdir:	fn (path : string, quiet : int) : int;
};

init(nil: ref Context, argv: list of string)
{
  sys = load Sys Sys->PATH;

  stderr = sys->fildes(2);

  if (argv == nil)
    return;
  argv = tl argv;
  if (argv == nil)
    sys->fprint(stderr, "Usage:\t"+Mod+" pathname-list\n");
  for (; argv != nil; argv = tl argv) {
    dir := hd argv;
    mkrdir(dir, 0);
  }
}

# make dir recursively
mkrdir(path : string, quiet : int) : int
{
  if (path == nil) return 0;
  (ok, nil) := sys->stat(path);
  if (ok >= 0 && !quiet) {
    sys->fprint(stderr, Mod+": %s already exists\n", path);
    return 1;
  }
  (n, lp) := sys->tokenize(path, "/");
  if (n <= 0)
    return n;
  dir : string;
  if (path[0] != '/')
    dir = ".";
  for(; lp != nil; lp = tl lp) {
    dir = dir + "/" + hd lp;
    (ok, nil) = sys->stat(dir);
    if(ok < 0) {
      f := sys->create(dir, sys->OREAD, sys->CHDIR + 8r777);
      if(f == nil) {
	sys->fprint(stderr, Mod+": can't create %s: %r\n", dir);
	return 0;
      }
    }
  }
  return 1;
}
