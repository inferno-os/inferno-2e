implement Command;

Mod : con "ldir";

include "sys.m";
sys: Sys;
Dir: import sys;
stderr : ref Sys->FD;

include "draw.m";

include "readdir.m";

include "sh.m";

init(ctxt : ref Draw->Context, args : list of string)
{
  sys = load Sys Sys->PATH;
  stderr = sys->fildes(2);
  if (args != nil)
    args = tl args;

  opt : string;
  files : list of string;
  if (args != nil) {
    opt = hd args;
    if (opt != nil && (opt[0] == '-' || opt[0] == '+'))
      files = tl args;
    else {
      opt = nil;
      files = args;
    }
  }

  sk := Readdir->NONE;
  rk := Readdir->DIR | Readdir->FILE;
  if (opt != nil)
    for(i := 0; i < len opt; i++)
      case opt[i] {
    '-' => sk |= Readdir->DESCENDING;
    '+' => sk &= ~Readdir->DESCENDING;
    'n' => sk = Readdir->NAME;
    'a' => sk = Readdir->ATIME;
    'm' => sk = Readdir->MTIME;
    's' => sk = Readdir->SIZE;
    'c' => rk |= Readdir->COMPACT;
    'd' => rk |= Readdir->DIR; rk &= ~Readdir->FILE;
    'f' => rk |= Readdir->FILE; rk &= ~Readdir->DIR;
    * => usage();
  }
  
  rd := load Readdir Readdir->PATH;
  if (rd == nil) {
    sys->fprint(stderr, "Error["+Mod+"]: %s %r\n", Readdir->PATH);
    return;
  }
  multi := len files > 1;
  for (; files != nil; files = tl files) {
    if (multi) sys->print("%s:\n", hd files);
    (dirs, n) := rd->init(hd files, sk|rk);
    if (n < 0) {
      sys->fprint(stderr, Mod+": %s %r\n", hd files);
      continue;
    }
    for (i = 0; i < n; i++)
      sys->print("%s\n", dirs[i].name);
  }
}

usage()
{
  sys->print("Usage:\t"+Mod+" [+-namscdf] file-list\n\t+ ascending\n\t- descending\n\tn name\n\ta atime\n\tm mtime\n\ts size\n\tc compact (no dupplicates)\n\td directories only\n\tf files only\n");
}

