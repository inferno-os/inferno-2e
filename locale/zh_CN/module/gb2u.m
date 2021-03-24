GB2U : module
{
	PATH:	con "/dis/lib/gb2u.dis";
	U2GBFILE: con "/dis/lib/u2gb.txt";
	
	Gb2Uni: fn(g: int): int;
	#init:   fn(ctxt: ref Draw->Context, argv: list of string);
	u2gb: fn(uniWord:int):int;	

};
