Devpointer: module
{
	PATH:	con	"/dis/lib/devpointer.dis";

	Size:		con 1+3*12;	# 'm' plus 3 12-byte decimal integers

	init: 		fn(file: string, posn: chan of ref Draw->Pointer): int;
	bytes2ptr:	fn(b: array of byte): ref Draw->Pointer;
	ptr2bytes:	fn(p: ref Draw->Pointer): array of byte;
};
