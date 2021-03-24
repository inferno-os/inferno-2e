implement Devpointer;

include "sys.m";
include "draw.m";
include "devpointer.m";

sys: Sys;
draw: Draw;

FD: import Sys;
Pointer: import Draw;

init(file: string, posn: chan of ref Pointer): int
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	if(draw == nil)
		return -1;

	if(file == nil)
		file = "/dev/pointer";
	dfd := sys->open(file, sys->OREAD);
	if(dfd == nil)
		return -1;
	spawn reader(posn, dfd);
	return 0;
}

reader(posn: chan of ref Pointer, dfd: ref FD)
{
	n: int;
	b:= array[Size] of byte;

	for(;;) {
		n = sys->read(dfd, b, len b);
		if(n < Size)
			break;
		posn <-= bytes2ptr(b);
	}
}

bytes2ptr(b: array of byte): ref Pointer
{
	if(len b<Size || int b[0]!= 'm')
		return nil;
	x := int string b[1:12];
	y := int string b[13:25];
	but := int string b[25:37];
	return ref Pointer (but, (x, y));
}

ptr2bytes(p: ref Pointer): array of byte
{
	if(p == nil)
		return nil;
	return array of byte sys->sprint("m%11d %11d %11d ", p.xy.x, p.xy.y, p.buttons);
}
