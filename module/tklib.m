Tklib: module
{
	PATH:		con "/dis/lib/tklib.dis";

	init:		fn(ctxt: ref Draw->Context);
	tkquote:	fn(s: string): string;
	is_err:		fn(e: string): int;
	tkcmds:		fn(t: ref Tk->Toplevel, cmds: array of string);
	dialog:		fn(parent: ref Tk->Toplevel, msgs: string, dflt: int, labs: list of string): int;
	getstring:	fn(parent: ref Tk->Toplevel, s: string): string;
	notice:		fn(parent: ref Tk->Toplevel, message: string);

	mktabs:		fn(parent: ref Tk->Toplevel, who: string, tabs: array of (string, string), dflt: int): chan of string;
	tabsctl:	fn(parent: ref Tk->Toplevel, who: string, tabs: array of (string, string), id: int, s: string): int;
};
