implement Datef;

include "sys.m";
stderr: ref Sys->FD;
stdout: ref Sys->FD;

include "draw.m";
include "daytime.m";

Context: import Draw;

Datef: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

sys: Sys;

init(nil: ref Context, args: list of string)
{
	sys = load Sys Sys->PATH;

	stderr = sys->fildes(2);

	daytime := load Daytime Daytime->PATH;
	if(daytime == nil) {
	  sys->fprint(stderr, "datef: load %s: %r\n", Daytime->PATH);
	  return;
	}
	tm := daytime->local(daytime->now());

	buf : string;

	if (args != nil)
	  args = tl args;

	fmt : list of string;
	if (args != nil)
	  (nil, fmt) = sys->tokenize(hd args, "%");

	str : string;

	while(fmt != nil) {
	  op := hd fmt;
	  if (len op > 1) {
	    str = op[1:];
	    op = op[0:1];
	  }
	  case op {
	      "Y" =>
		buf += string (tm.year+1900);
	      "y" =>
		buf += string tm.year;
	      "m" =>
		if (tm.mon < 10)
		  buf += "0";
	        buf += string (tm.mon+1);
	      "d" =>
		if (tm.mday < 10)
		  buf += "0";
	        buf += string tm.mday;
	      "H" =>
		if (tm.hour < 10)
		  buf += "0";
	        buf += string tm.hour;
	      "M" =>
		if (tm.min < 10)
		  buf += "0";
	        buf += string tm.min;
	      "S" =>
		if (tm.sec < 10)
		  buf += "0";
	        buf += string tm.sec;
	      "n" =>
	        buf += "\n";
	      * =>
		  usage();
	  }
	  if (str != nil)
	    buf += str;
	  str = nil;
	  fmt = tl fmt;
	}

	if (buf == nil)
	  buf = daytime->time();

	sys->print("%s\n", buf);

}

usage()
{
  sys->fprint(stderr, "usage: datef [%s]\n", "%Y%y%m%d%n");
  exit;
}
