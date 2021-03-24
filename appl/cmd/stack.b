implement Command;

include "sys.m";
sys: Sys;
print, fprint, FD: import sys;
stderr: ref FD;

include "draw.m";

include "debug.m";
debug: Debug;
Prog, Module, Exp: import debug;

Command: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

usage()
{
	sys->fprint(stderr, "usage: stack [-v] pid\n");
}

init(nil: ref Draw->Context, argv: list of string)
{

	sys = load Sys Sys->PATH;
	if(sys == nil)
		return;
	stderr = sys->fildes(2);

	if(argv == nil)
		return;
	verbose := 0;
	for(argv = tl argv; argv != nil; argv = tl argv){
		s := hd argv;
		if(s[0] != '-')
			break;
		case s[1]{
		'v' =>
			verbose = 1;
		* =>
			usage();
		}
	};

	if(len argv != 1){
		usage();
		return;
	}

	debug = load Debug Debug->PATH;
	if(debug == nil){
		fprint(stderr, "stack: debugger not loaded\n");
		return;
	}

	debug->init();
	(p, err) := debug->prog(int hd argv);
	if(err != nil){
		fprint(stderr, "stack: %s\n", err);
		return;
	}
	stk : array of ref Exp;
	(stk, err) = p.stack();

	if(err != nil){
		fprint(stderr, "stack: %s\n", err);
		return;
	}

	for(i := 0; i < len stk; i++){
		stk[i].m.stdsym();
		stk[i].findsym();
		fprint(stderr, "%s(", stk[i].name);
		vs := stk[i].expand();
		if(verbose && vs != nil){
			for(j := 0; j < len vs; j++){
				if(vs[j].name == "args"){
					d := vs[j].expand();
					s := "";
					for(j = 0; j < len d; j++){
						fprint(stderr, "%s%s=%s", s, d[j].name, d[j].val().t0);
						s = ", ";
					}
					break;
				}
			}
		}
		fprint(stderr, ") %s\n", stk[i].srcstr());
		if(verbose && vs != nil){
			for(j := 0; j < len vs; j++){
				if(vs[j].name == "locals"){
					d := vs[j].expand();
					for(j = 0; j < len d; j++)
						fprint(stderr, "\t%s=%s\n", d[j].name, d[j].val().t0);
					break;
				}
			}
		}
	}
}
