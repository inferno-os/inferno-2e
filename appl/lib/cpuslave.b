implement CPUslave;

include "sys.m";
	sys: Sys;
include "draw.m";
	draw: Draw;
	Context, Display, Screen: import draw;
include "devpointer.m";
	ptr: Devpointer;

include "sh.m";

stderr: ref Sys->FD;

CPUslave: module
{
	init: fn(ctxt: ref Context, args: list of string);
};

init(nil: ref Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	ptr = load Devpointer Devpointer->PATH;
	stderr = sys->fildes(2);

	if(len args < 3){
		sys->fprint(stderr, "usage: cpuslave -s3 command args\n");
		return;
	}

	args = tl args;
	arg := hd args;
	if(len arg<2 || arg[0:2]!="-s"){
		sys->fprint(stderr, "usage: cpuslave -s3 command args\n");
		return;
	}
	screenid := int arg[2:len arg];
	args = tl args;

	file := hd args + ".dis";
	cmd := load Command file;
	if(cmd == nil)
		cmd = load Command "/dis/"+file;
	if(cmd == nil){
		sys->fprint(stderr, "cpuslave: can't load %s: %r\n", hd args);
		return;
	}

	display := Display.allocate(nil);
	if(display == nil){
		sys->fprint(stderr, "can't initialize display: %r\n");
		return;
	}

	screen: ref Screen;
	if(screenid >= 0){
		screen = display.publicscreen(screenid);
		if(screen == nil){
			sys->fprint(stderr, "can't establish screen id %d: %r\n", screenid);
			return;
		}
	}

	ctxt := ref Context;
	ctxt.screen = screen;
	ctxt.display = display;
	
	spawn cmd->init(ctxt, args);
}
