Wm: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
	execlist: list of list of string;
	screen: ref Draw->Screen;
	menu_config: array of string;
	ws: Wms;

	readsetup: fn(t: ref Tk->Toplevel);
	runcommand: fn(ctxt: ref Draw->Context, args: list of string);
	tokenize: fn(s, t : string) : (int, list of string);

	Icon: adt
	{
		name:	string;
		repl:	int;
		fid:	int;
		wc:	Sys->Rwrite;
	};
	icons: list of Icon;
	newicon: fn(i: Icon, task: chan of string);
	iconify: fn(label: string, fid: int): string;
	deiconify: fn(name: string, fid: int);

	applinit: fn(mod: Command, ctxt: ref Draw->Context, args: list of string);
	wmdialog: fn(ico, title, msg: string, dflt: int, labs: list of string);
	geom: fn(): string;
};

Wms: module 
{
	readsetup: fn(t: ref Tk->Toplevel);
	runcommand: fn(ctxt: ref Draw->Context, args: list of string);
	tokenize: fn(s, t: string) : (int, list of string);

	newicon: fn(i: Wm->Icon, task: chan of string);
	iconify: fn(label: string, fid: int): string;
	deiconify: fn(name: string, fid: int);
};

