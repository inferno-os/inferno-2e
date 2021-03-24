#
# 	Module:		Touchcal
#	Author:		Frank Barrus
#	Purpose:	Calibrate touch screen
#

implement TouchCal;

#
# Included Modules
#

include "sys.m";
	sys:	Sys;
	FD:	import sys;

include "draw.m";
	draw:	Draw;
	Context, Rect: import draw;

#
# Globals
#

stderr:	ref FD;						# standard error FD

#
# Constants
#

vidwid: int;				# width of video screen
vidhgt: int;				# height of video screen

TouchCal: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};


# 
# Main Routine
#

init(nil: ref Draw->Context, nil: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	stderr = sys->fildes(2);
	touchcal();
}

#
# Touchscreen calibration
#

dotouchcal(f: ref FD, p, x, y: int)
{
	Display, Image: import draw;
	display := draw->Display.allocate(nil);
	disp := display.image;
	r := disp.r;
	vidwid = r.dx();
	vidhgt = r.dy();
	white := display.color(Draw->White);
	black := display.color(Draw->Black);
	grey := display.rgb(192,192,192);
	ones := display.ones;
	for(i := 1; i<vidwid; i+= 10) {
		disp.draw(((i,0),(i+1,vidhgt)), white, ones, (0,0));
		disp.draw(((i-1,0),(i,vidhgt)), black, ones, (0,0));
	}
	for(i = 1; i<vidhgt; i+= 10) {
		disp.draw(((0,i),(vidwid,i+1)), white, ones, (0,0));
		disp.draw(((0,i-1),(vidwid,i)), black, ones, (0,0));
	}
	disp.draw(((x-17,y-17),(x+18,y+18)), grey, ones, (0,0));
	disp.draw(((x-9,y-9),(x+10,y+10)), white, ones, (0,0));
	disp.draw(((x,y-4),(x+1,y+5)), black, ones, (0,0));
	disp.draw(((x-4,y),(x+5,y+1)), black, ones, (0,0));
	s := sys->sprint("c%d %d %d\n", p, x, y);
	b := array of byte s;
	if (sys->write(f, b, len b) < 0) 
		sys->fprint(stderr, "write #T/touchctl failed: %r\n");
	else
		disp.draw(((x-10,y-10),(x+10,y+10)), black, ones, (0,0));
}

touchcal()
{
	sys->print("Touch screen calibration is required:\n");
	for(;;) {
		f := sys->open("#T/touchctl", sys->OWRITE);
		if (f == nil) {
			sys->fprint(stderr, "open #T/touchctl failed: %r\n");
			continue;
		}
		dotouchcal(f, 0, 20, 20);
		dotouchcal(f, 1, vidwid-20, 20);
		dotouchcal(f, 2, vidwid-20, vidhgt-20);
		dotouchcal(f, 3, 20, vidhgt-20);
		b := array of byte "C\n";
		if(sys->write(f, b, len b) < 0) 
			sys->print("calibration failed!\n");
		else
			break;
	}
	sys->print("Touch screen calibration complete\n");
}

