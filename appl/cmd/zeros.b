implement zeros;

include "sys.m";
	sys: Sys;

include "draw.m";

zeros: module
{
	init: fn(nil: ref Draw->Context, argv: list of string);
};

init(nil: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	if(sys == nil)
		return;

	bs := 0;
	if(len argv > 1)
		bs = int hd tl argv;
	else
		bs = 1;
	if(bs == 0)
		sys->raise("fail: usage: zeros blksize");

	if(sys->rescue("*", nil))
		return;

	z := array[bs] of byte;
	for(i:=0;i<bs;i++)
		z[i] = byte 0;
	for(;;)
		sys->write(sys->fildes(1), z, bs);
}
