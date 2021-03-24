Keyboard: module
{
        PATH:           con "/dis/wm/wmkb.dis";
	#PATH:           con "/dis/wm/azertykb.dis";
        initialize:     fn(t: ref Tk->Toplevel, ctxt : ref Draw->Context,
                         dot: string): chan of string;
        chaninit:       fn(t: ref Tk->Toplevel, ctxt : ref Draw->Context,
                         dot: string, rc: chan of string): chan of string;
        enable_move_key:        fn(t: ref Tk->Toplevel); # delete this late;
        disable_move_key:       fn(t: ref Tk->Toplevel); # delete this late;
};
