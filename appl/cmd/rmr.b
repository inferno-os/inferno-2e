# remove recursively (file and dirs)
# obc
implement Rmr;

Mod : con "rmr";

include "sys.m";
include "draw.m";

FD: import Sys;
Context: import Draw;

include "readdir.m";
rd : Readdir;

Rmr: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
	rmr:	fn(p : string) : int;
};

sys: Sys;
stderr : ref FD;

init(nil: ref Context, argv: list of string)
{
  if (!ldrd())
    return;
  if (argv == nil) return;
  argv = tl argv;
  for(; argv != nil; argv = tl argv)
   rmr(hd argv);
}

ldrd() : int
{
  if (sys == nil) {
    sys = load Sys Sys->PATH;
    stderr = sys->fildes(2);
  }
  rd = load Readdir Readdir->PATH;
  
  if (rd == nil) {
    sys->fprint(stderr, "Error["+Mod+"]: %s %r\n", Readdir->PATH);
    return 0;
  }
  return 1;
}

remove(p : string) : int
{
  if (sys->remove(p) < 0) {
    sys->fprint(stderr, "Error: "+Mod+" cannot rm %s %r\n", p);
    return 0;
  }
  return 1;
}

rmr(p : string) : int
{
  if (p == nil)
    return 0;
  
  (ok, dir) := sys->stat(p);
  if (ok < 0)
    sys->fprint(stderr, Mod+": can't stat %s: %r\n", p);
  else if (dir.mode & sys->CHDIR)
    return rmrdir(p);
  else
    return remove(p);
  return 0;
}

rmrdir(p : string) : int
{
  (ad, nd) := rd->init(p, Readdir->NONE|Readdir->COMPACT);
  if (nd == 0)
    return remove(p);
  if (nd <= 0)
    return 0;

  d := p;
  for (i := 0; i < nd; i++) {
    p = d+"/"+ad[i].name;
    if (ad[i].mode & sys->CHDIR)
      rmrdir(p);
    else
      remove(p);
  }
  return remove(d);
}
