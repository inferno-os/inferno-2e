# compose kernel name for gzip -N option
# store name in file and kernel type in file_
implement Kername;

Mod : con "kername";

include "sys.m";
sys : Sys;
stderr : ref Sys->FD;

include "draw.m";

include "version.m";

Kername: module
{
        init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

rwa : con 8r666;
init(nil : ref Draw->Context, args : list of string)
{
  if (args != nil)
    args = tl args;
  sys = load Sys Sys->PATH;
  stderr = sys->fildes(2);

  if (len args < 2) {
    sys->fprint(stderr, "usage: "+Mod+" file devtype [kerntype] [release]\n       create file and file_\n");
    return;
  }

  file := hd args;
  typ := hd tl args;
  ktyp := typ;
  rel : string;
  if (len args > 3) {
    ktyp = hd tl tl args;
    rel = hd tl tl tl args;
  } else if (len args == 3)
    ktyp = hd tl tl args;
  
  kername := typ;

  if(stailp(ktyp, "nfail"))
    ktyp = "kern1";
  else if(stailp(ktyp, "dev"))
    ktyp = "kern0";
  else if(typ == ktyp)
    ktyp = "kern2";

  fd := sys->create(file+"_", sys->OWRITE, rwa);
  
  if (fd == nil) {
    sys->fprint(stderr, "Error["+Mod+"]: cannot write %s %r\n", file+"_");
    return;
  }
  sys->fprint(fd, "%s\n", ktyp);

  kername += "_"+ktyp+"_";

  (bld, secu) := getversion();
  if (bld != nil) {
    if (rel == nil)
      rel = "Inf"+bld;
    kername += rel;
    kername += secu;
  }
  
  fd = sys->create(file, sys->OWRITE, rwa);
  
  if (fd == nil) {
    sys->fprint(stderr, "Error["+Mod+"]: cannot write %s %r\n", file);
    return;
  }
  sys->fprint(fd, "%s\n", kername);
}

stailp(s, tail : string) : int
{
  return len s > len tail && s[len s - len tail:] == tail;
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
