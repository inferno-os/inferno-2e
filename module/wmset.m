Wmset: module 
{
	PATH: con "/dis/lib/wmset.dis";
	initme: fn(me: Wm, s: Sys, d: Draw, t: Tk, w: Wmlib) : Wms;
	readsetup: fn(t: ref Tk->Toplevel);
	runcommand: fn(ctxt: ref Draw->Context, args: list of string);
	tokenize: fn(s, t: string) : (int, list of string);

	newicon: fn(i: Icon, task: chan of string);
	iconify: fn(label: string, fid: int): string;
	deiconify: fn(name: string, fid: int);
};
