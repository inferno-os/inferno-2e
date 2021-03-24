implement Dump;

include "sys.m";
include "draw.m";
include "string.m";

Context: import Draw;

Dump: module
{
	init:	fn(nil: ref Context, argv: list of string);
};

sys: Sys;
str: String;

init(nil: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	str = load String String->PATH;

	if(argv == nil)
		exit;

	(offset, nil) := str->toint( hd (tl argv), 10);
	(length, nil) := str->toint( hd (tl (tl argv)), 10);

	buf := array[ length ] of byte;

	fd := sys->open("#Z/kmem",sys->OREAD);
	sys->seek( fd, offset, Sys->SEEKSTART );
	sys->read( fd, buf, length );
	
	count2 := 0;
	for (count := 0; count < length; count ++) {
		if (count2 == 0)
			sys->print("\n[%8.8ux]: ", offset+count);
		sys->print("%2.2ux ",int buf[count]);
		if (count2++ >= 7) 
			count2 = 0;
	}
}
