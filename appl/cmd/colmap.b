implement Colmap;

include "sys.m";
sys: Sys;
open, print, read, tokenize: import sys;

include "draw.m";
draw: Draw;
Context, Font, Rect, Point, Image, Screen: import draw;
rgb, color: import draw;

Colmap: module
{
	init:	fn(ctxt: ref Context; argv: list of string);
};

screen: ref Screen;
window: ref Image;
ones: ref Image;

init(ctxt: ref Context; list of string)
{
	p : Point;

	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;

#	screen = ctxt.screen;
	ones = draw->ones();
#	window = screen.window(Rect((10, 10), (266, 266)));
	window = draw->display();
	for(i := 0; i < 256; i++){
		p.x = i % 16 * 16;
		p.y = i / 16 * 16;
		window.draw(Rect(p, p.add((16, 16))), draw->color(i), ones, (0, 0));
	}
	ones = draw->ones();
	sys->sleep(10*1000);
	fd := sys->open("/dev/cons", sys->OREAD);
	buf:= array[17] of byte;
	sys->read(fd, buf, len buf);
};
