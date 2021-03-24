implement Charon_code;

include "common.m";
include "rgb.b";
include "ycbcr.b";
include "charon_code.m";

sys: Sys;
CU: CharonUtils;
	Header, ByteSource, MaskedImage, ImageCache, ResourceState: import CU;
D: Draw;
	Point, Rect, Image, Display: import D;
E: Events;
	Event: import E;
G: Gui;

init()
{
	D = load Draw Draw->PATH;
}

remeasure_font(str:string,fnt:int):int
{
	return fnt;
}
convCode(buf:array of byte,index:int):(int,int)
{
	return (-1,index);
}
getRightcode(s:string):string
{
	return s;
}
