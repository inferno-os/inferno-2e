# Kludge passing args to emu shell -- use while emu args not available
# obc
implement Command;

KLUDGE : con "/tmp/.shargs";

include "sys.m";
	sys: Sys;

include "draw.m";

include "sh.m";
sh : Command;

init(ctxt: ref Draw->Context, argv: list of string)
{
  sys = load Sys Sys->PATH;
  ok := chmod(KLUDGE, rwa);
  sh = load Command Command->PATH;
  sh->init(ctxt, Command->PATH :: KLUDGE :: nil);
  if (ok) {
    sys->print("shargs: rm %s\n", KLUDGE);
    sys->remove(KLUDGE);
  }
}

rwa: con 8r666;
rwxa : con 8r777;
mask : con rwxa;

chmod(f : string, mode : int) : int
{
  (ok, dir) := sys->stat(f);
  if(ok < 0)
    return 0;
  dir.mode = (dir.mode & ~mask) | (mode & mask);
  return !(sys->wstat(f, dir) < 0);
}
