PsLib : module 
{
	ERROR, NEWPAGE, OK : con iota;
	PATH:		con "/dis/lib/pslib.dis";

	getfonts: fn(input: string) : string;
	preamble: fn(ioutb : ref Bufio->Iobuf, bbox: Draw->Rect) : string;
	trailer: fn(ioutb : ref Bufio->Iobuf,pages : int) : string;
	printnewpage: fn(pagenum : int,end : int, ioutb : ref Bufio->Iobuf);
	parseTkline: fn(ioutb : ref Bufio->Iobuf,input : string) : string;
	stats: fn() : (int,int,int);
	init : fn(env : ref Draw->Context,t : ref Tk->Toplevel,boxes: int,deb : int) : string;
	deffont : fn() : string;
	image2psfile: fn(ioutb: ref Bufio->Iobuf, im: ref Draw->Image, dpi: int) : string;
};
