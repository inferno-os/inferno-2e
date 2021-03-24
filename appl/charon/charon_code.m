Charon_code:module
{
	PATH: con "/dis/charon/charon_code.dis";
	init :fn();
	convCode:fn(buf: array of byte, index:int):(int,int);
	remeasure_font:fn(str:string,fnt:int):int;
	getRightcode:fn(s:string):string;
};
