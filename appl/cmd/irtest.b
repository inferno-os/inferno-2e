implement Irtest;

include "sys.m";
include "draw.m";
include "ir.m";

FD: import Sys;
Context: import Draw;

Irtest: module
{
	init:	fn(nil: ref Context, argv: list of string);
};

ir: Ir;
sys: Sys;
stderr: ref FD;

init(nil: ref Context, nil: list of string)
{
	c: int;
	x := chan of int;
	p := chan of int;

	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	ir = load Ir Ir->PATH;
	if(ir == nil)
		ir = load Ir Ir->SIMPATH;
	if(ir == nil) {
		sys->fprint(stderr, "load ir: %r\n");
		return;
	}

	if(ir->init(x,p) != 0) {
		sys->fprint(stderr, "Ir->init: %r\n");
		return;
	}
	<-p;

	names := array[] of {
		"Zero",
		"One",
		"Two",
		"Three",
		"Four",
		"Five",
		"Six",
		"Seven",
		"Eight",
		"Nine",
		"ChanUP",
		"ChanDN",
		"VolUP",
		"VolDN",
		"FF",
		"Rew",
		"Up",
		"Dn",
		"Select",
		"Power",
	};

	for(;;) {
		c = <-x;
		if(c == ir->Error)
			sys->print("Error\n");
		else
			sys->print("%s\n", names[c]);
	}	
}
