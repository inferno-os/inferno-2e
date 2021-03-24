# command spawn -- use from mash
# obc
implement Command;

Mod : con "spawn";

include "sys.m";
include "draw.m";
include "sh.m";

sys: Sys;
stderr : ref Sys->FD;

usage()
{
  sys->fprint(stderr, "Usage:\t"+Mod+" [options]\tcommand [command args]\n\toptions:\t-newfd -forkfd -newns -forkns -newpgrp -nodevs\n");
}

init(ctxt: ref Draw->Context, args: list of string)
{
  sys = load Sys Sys->PATH;
  stderr = sys->fildes(2);
  if (args == nil) return;
  if (args != nil)
    args = tl args;
  if (args == nil) {
    usage();
    return;
  }
  pl : list of int;
  (pl, args) = pctlargs(args);
  cmd := hd args;
  args = tl args;
  if(len cmd < 4 || cmd[len cmd -4:]!=".dis")
    cmd += ".dis";
  sh := load Command cmd;
  if (sh == nil)
    cmd = "/dis/"+cmd;
  sh = load Command cmd;
  if (sh == nil) {
    sys->fprint(stderr, "Error: "+Mod+" load %s %r\n", cmd);
    return;
  }
  spawn call(sh, pl, ctxt, cmd :: args);
}

call(sh : Command, pl : list of int, ctxt : ref Draw->Context, args : list of string)
{
  for(; pl != nil; pl = tl pl)
    sys->pctl(hd pl, nil);
  sh->init(ctxt, args);
}

pctlargs(args : list of string) : (list of int, list of string)
{
  pl : list of int;
  for(; args != nil && (hd args)[0] == '-'; args = tl args)
    case hd args {
    "-newfd" => pl = sys->NEWFD :: pl;
    "-forkfd" => pl = sys->FORKFD :: pl;
    "-newns" => pl = sys->NEWNS :: pl;
    "-forkns" => pl = sys->FORKNS :: pl;
    "-newpgrp" => pl = sys->NEWPGRP :: pl;
    "-nodevs" => pl = sys->NODEVS :: pl;
    * => usage(); break;
  }
  r : list of int;
  for(; pl != nil; pl = tl pl)
    r = hd pl :: r;
  return (r, args);
}
