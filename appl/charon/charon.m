Charon : module
{
	PATH: con "/dis/charon.dis";

	init: fn(ctxt: ref Draw->Context, argv: list of string);
	toolbarinit: fn(ctxt: ref Draw->Context, argv: list of string, wc, rc: chan of string);
	showstatus: fn(msg: string);
	histinfo: fn(): (int, string, string, string);
};
