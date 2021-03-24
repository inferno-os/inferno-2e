Wmlib: module
{
	PATH:		con "/dis/lib/wmlib.dis";

	Resize,
	Hide,
	Help,
	OK:		con 1 << iota;

	Appl:		con Resize | Hide;

	init:		fn();
	titlebar:	fn(scr: ref Draw->Screen, where, name: string,
				buts: int): (ref Tk->Toplevel, chan of string);
	untaskbar:	fn();
	titlectl:	fn(t: ref Tk->Toplevel, request: string);
	taskbar:	fn(t: ref Tk->Toplevel, name: string): string;
	geom:		fn(t: ref Tk->Toplevel): string;
	snarfput:	fn(buf: string);
	snarfget:	fn(): string;

	tkquote:	fn(s: string): string;
	tkcmds:		fn(top: ref Tk->Toplevel, a: array of string);
	dialog:		fn(parent: ref Tk->Toplevel, ico, title, msg: string,
				dflt: int, labs : list of string): int;
	getstring:	fn(parent: ref Tk->Toplevel, msg: string): string;

	filename:	fn(scr: ref Draw->Screen, top: ref Tk->Toplevel,
				title: string,
				pat: list of string,
				dir: string): string;

	mktabs:		fn(t: ref Tk->Toplevel, dot: string,
				tabs: array of (string, string),
				dflt: int): chan of string;

	tabsctl:	fn(t: ref Tk->Toplevel,
				dot: string,
				tabs: array of (string, string),
				id: int,
				s: string): int;
};
