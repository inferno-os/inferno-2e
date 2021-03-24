implement wish;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;

include "tk.m";
	tk: Tk;

include	"wmlib.m";
	wmlib: Wmlib;

include "bufio.m";
	bufmod : Bufio;
Iobuf :	import bufmod;

include "../lib/tcl.m";
	tcl : Tcl_Core;

wish : module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};



menubut : chan of string;
keyboard,mypid : int;

Wwsh : ref Tk->Toplevel;

init(ctxt: ref Draw->Context, argv: list of string) {
	sys  = load Sys  Sys->PATH;
	draw = load Draw Draw->PATH;
	tk   = load Tk   Tk->PATH;
	wmlib= load Wmlib Wmlib->PATH;
	bufmod = load Bufio Bufio->PATH;
	if (tk==nil || wmlib==nil || bufmod==nil){
		sys->print("Load Error: %r\n");
		exit;
	}
	tcl=load Tcl_Core Tcl_Core->PATH;
	if (tcl==nil){
		sys->print("Cannot load Tcl (%r)\n");
		exit;
	}
	keyboard=1;
	argv = tl argv;
	if (argv!=nil)
		file:=parse_args(argv);
	geom:="";
	mypid=sys->pctl(sys->NEWPGRP, nil);
	wmlib->init();
	Wshinit(ctxt, geom);
	tcl->init(ctxt,argv);
	tcl->set_top(Wwsh);
	shellit(file);
}





parse_args(argv : list of string) : string {
	while (argv!=nil){
		case (hd argv){
			"-k" =>
				keyboard=0;
			"-f" =>
				argv = tl argv;
				return hd argv;
			* =>
				return nil;
		}
		argv = tl argv;
	}
	return nil;
}

shellit(file:string){
	drag:=chan of string;
	tk->namechan(Wwsh, drag, "Wm_drag");
	lines:=chan of string;
	Tcl_Chan:=chan of string;
	tk->namechan(Wwsh, lines, "lines");
	tk->namechan(Wwsh, Tcl_Chan, "Tcl_Chan");
	new_inp:="wish%";
	unfin:="wish>";
	line : string;
	loadfile(file);
	quiet:=0;
	if (keyboard)
		spawn tcl->grab_lines(new_inp,unfin,lines);
	for(;;){
		alt{
			s := <-drag =>
				if(len s < 6 || s[0:5] != "path=")
					break;
				loadfile(s[5:]);
				sys->print("%s ",new_inp);
			line = <-lines =>
				line = tcl->prepass(line);
				msg:= tcl->evalcmd(line,0);
				if (msg!=nil)
					sys->print("%s\n",msg);
				sys->print("%s ", new_inp);
				tcl->clear_error();
			rline := <-Tcl_Chan  =>
				rline = tcl->prepass(rline);
				msg:= tcl->evalcmd(rline,0);
				if (msg!=nil)
					sys->print("%s\n",msg);
				tcl->clear_error();
			menu := <-menubut =>
				if(menu[0] == 'e'){
					kfd := sys->open("#p/"+string mypid+"/ctl", sys->OWRITE);
					if(kfd == nil) 
						sys->print("error opening pid %d (%r)\n",mypid);
						sys->fprint(kfd, "killgrp");
						exit;				
				}
				wmlib->titlectl(Wwsh, menu);
		}
	}
}



loadfile(file :string) {
	iob : ref Iobuf;
	line,input : string;
	line = "";
	if (file==nil)
		return;	
	iob = bufmod->open(file,bufmod->OREAD);
	if (iob==nil){
		sys->print("File %s cannot be opened for reading",file);
		return;
	}
	while((input=iob.gets('\n'))!=nil){
		line+=input;
		if (tcl->finished(line,0)){
			line = tcl->prepass(line);
			msg:= tcl->evalcmd(line,0);
			if (msg!=nil)
				sys->print("%s\n",msg);
			tcl->clear_error();
			line=nil;
		}
	}
}

Wshinit(ctxt: ref Draw->Context, geom: string) {
	(Wwsh, menubut) = wmlib->titlebar(ctxt.screen, geom,
		"WishPad",Wmlib->Appl);
	cmd := chan of string;
	tk->namechan(Wwsh, cmd, "wsh");
	tk->cmd(Wwsh, "update");
}
