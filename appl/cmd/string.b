implement Shstring;

Mod : con "Shstring";

include "sys.m";
sys : Sys;
stderr : ref Sys->FD;

include "draw.m";

include "string.m";

Shstring: module
{
	init:	fn(nil: ref Draw->Context, nil: list of string);
};

usage()
{
	  sys->fprint(stderr, "Usage: string cmd string [string2]\n       cmd -- entry into string.m\n");
	  return;
}

init(nil: ref Draw->Context, args : list of string)
{
        sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	str := load String String->PATH;
	args = tl args;
	if (args == nil) {
	  usage();
	  return;
	}
	cmd := hd args;
	args = tl args;
	case len args {
	  0 => usage(); return;
	  1 => case cmd {
	    "tolower" => sys->print("%s\n", str->tolower(hd args));
	    "toupper" => sys->print("%s\n", str->toupper(hd args));
	    * => usage(); return;
	  }
	  2 => case cmd {
	    "splitl" => (s1, s2) := str->splitl(hd args, hd tl args); sys->print("%s\n%s\n", s1, s2);
	    "splitr" => (s1, s2) := str->splitr(hd args, hd tl args); sys->print("%s\n%s\n", s1, s2);
	    "take" => sys->print("%s\n", str->take(hd args, hd tl args));
	    "drop" => sys->print("%s\n", str->drop(hd args, hd tl args));
	    "in" => sys->print("%d\n", str->in(int hd args, hd tl args));
	    "splitstrl" => (s1, s2) := str->splitstrl(hd args, hd tl args); sys->print("%s\n%s\n", s1, s2);
	    "splitstrr" => (s1, s2) := str->splitstrr(hd args, hd tl args); sys->print("%s\n%s\n", s1, s2);
	    "prefix" => sys->print("%d\n", str->prefix(hd args, hd tl args));
	    "toint" => (v1, s2) := str->toint(hd args, int hd tl args); sys->print("%d\n%s\n", v1, s2);
	    * => usage(); return;
	  }
	  * => case cmd {
	    "append" => printl(str->append(hd args, tl args));
	    * => usage(); return;
	  }
	}
}

printl(l : list of string)
{
  for(;l != nil; l = tl l)
    sys->print("%s\n", hd l);
}
