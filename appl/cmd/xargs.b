# apply cmd to args list read from stdin
# obc
implement Xargs;

Mod : con "xargs";

include "sys.m";
include "draw.m";
include "sh.m";
include "rtoken.m";
rt : Rtoken;
Id : import rt;

Xargs: module
{
        init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

sys: Sys;
stderr : ref Sys->FD;

usage()
{
  sys->fprint(stderr, "Usage: "+Mod+" command [command args] <[list of last command arg]\n");
}

init(ctxt: ref Draw->Context, args: list of string)
{
  if (!ldrt())
    return;
  if (args == nil) return;
  if (args != nil)
    args = tl args;
  if (args == nil) {
    usage();
    return;
  }
  cmd := hd args;
  args = tl args;
  if(len cmd < 4 || cmd[len cmd -4:]!=".dis")
    cmd += ".dis";
  sh := load Command cmd;
  if (sh == nil)
    cmd = "/dis/"+cmd;
  sh = load Command cmd;
  if (sh == nil)
    sys->fprint(stderr, "Error: "+Mod+" load %s %r\n", cmd);

  stdin := sys->fildes(0);
  id := rt->id();
  id.seteot(rt->NOEOT);
  while((t := rt->readtoken(stdin, "\n", id)) != nil) {
    (nil, rargs) := sys->tokenize(t, " \t");
    if (rargs == nil)
      continue;
    if (args == nil)
      rargs = cmd :: rargs;
    else
      rargs = append(cmd :: args, rargs);
    sh->init(ctxt, rargs);
  }
}

ldrt() : int
{
  if (sys == nil) {
    sys = load Sys Sys->PATH;
    stderr = sys->fildes(2);
  }
  if (rt == nil) {
    rt = load Rtoken Rtoken->PATH;

    if (rt == nil) {
      sys->fprint(stderr, "Error["+Mod+"]: %s %r\n", Rtoken->PATH);
      return 0;
    }
  }
  return 1;
}

reverse(l : list of string) : list of string
{
  r : list of string;
  for(; l != nil; l = tl l)
    r = hd l :: r;
  return r;
}

append(h, t : list of string) : list of string
{
  r := reverse(h);
  for(; r != nil; r = tl r)
    t = hd r :: t;
  return t;
}
