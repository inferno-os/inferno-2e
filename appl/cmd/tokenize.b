implement Tokenize;

Mod : con "tokenize";

include "sys.m";
sys : Sys;
stderr : ref Sys->FD;

include "draw.m";

Tokenize: module
{
	init:	fn(nil: ref Draw->Context, nil: list of string);
};

usage()
{
	  sys->fprint(stderr, "Usage:\t"+Mod+" string delimiters\n");
	  return;
}

init(nil: ref Draw->Context, args : list of string)
{
        sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	args = tl args;
	if (args == nil) {
	  usage();
	  return;
	}
	case len args {
	  2 => {
	    (nil, l) := sys->tokenize(hd args, hd tl args);
	    printl(l);
	  }
	  * => usage(); return;
	}
}

printl(l : list of string)
{
  for(;l != nil; l = tl l)
    sys->print("%s\n", hd l);
}
