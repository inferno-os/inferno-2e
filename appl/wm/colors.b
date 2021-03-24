implement Colors;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Context, Display, Point, Rect, Image, Screen, Font: import draw;

include "tk.m";
	tk: Tk;
	Toplevel: import tk;

include	"wmlib.m";
	wmlib: Wmlib;

Colors: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

screen: ref Screen;
display: ref Display;
t: ref Toplevel;

task_cfg := array[] of {
	"label .l -text { } -width 256",
	"canvas .c -height 256 -width 256",
	"pack .l",
	"pack .c -side bottom -fill both -expand 1",
	"pack propagate . 0",
	"bind .c <Button-1> {send cmd push %x %y}",
	"bind .c <ButtonRelease-1> {send cmd release}",
};

init(ctxt: ref Context, argv: list of string)
{
	spawn init1(ctxt, argv);
}

font: ref Font;
lastcol := 0;

init1(ctxt: ref Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;

	display = ctxt.display;
	screen = ctxt.screen;

	wmlib->init();

	tkargs := "";
	argv = tl argv;
	if(argv != nil) {
		tkargs = hd argv;
		argv = tl argv;
	}

	menubut: chan of string;
	(t, menubut) = wmlib->titlebar(screen, tkargs, "Colors", Wmlib->Appl);

	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");

	wmlib->tkcmds(t, task_cfg);

	tk->cmd(t, "bind . <Configure> {send cmd resize}");
	tk->cmd(t, "bind . <Map> {send cmd resize}");
	tk->cmd(t, "update");
	font = Font.open(display, "/fonts/lucidasans/unicode.8.font");

	cr := redraw(t);
	grab := 0;

	for(;;) alt {
	menu := <-menubut =>
		case menu{
		"exit" =>
			return;
		}
		wmlib->titlectl(t, menu);

	press := <-cmd =>
		(n, word) := sys->tokenize(press, " ");
		case hd word {
		"push" =>
			if(grab == 0)
				tk->cmd(t, "grab set .c");
			color(int hd tl word, int hd tl tl word, cr, lastcol);
		"release" =>
			tk->cmd(t, "grab release .c");
			grab = 0;
		"resize" =>
			cr = redraw(t);
		}
	}
}

redraw(t: ref Toplevel): Rect
{
	cr := posn(t, ".c");
	cr.max.x = t.image.r.max.x-2;
	cr.max.y = t.image.r.max.y-2;
	color(0, 0, cr, -1);
	cmap(cr);
	return cr;
}

posn(t: ref Toplevel, s: string): Rect
{
	r: Rect;

	r.min.x = int tk->cmd(t, s+" cget -actx") + int tk->cmd(t, ".dx get");
	r.min.y = int tk->cmd(t, s+" cget -acty") + int tk->cmd(t, ".dy get");
	r.max.x = r.min.x + int tk->cmd(t, s+" cget -width") + int tk->cmd(t, ".dw get");
	r.max.y = r.min.y + int tk->cmd(t, s+" cget -height") + int tk->cmd(t, ".dh get");

	return r;
}

color(x, y: int, cr: Rect, prev: int)
{
	if(prev < 0)
		col := lastcol;
	else{
		p := cr.min.add((x, y));
		if(p.in(cr)){
			x = (16*x)/cr.dx();
			y = (16*y)/cr.dy();
			col = 16*y+x;
		}else{
			b := array[1] of byte;
			rr := Rect(((cr.min.x+x),(cr.min.y+y)),((cr.min.x+x+1),(cr.min.y+y+1)));
			ok := display.image.readpixels(rr, b);
			if(ok != 1)
				return;
			col = int b[0];
		}
	}
	if(prev>=0 && col==lastcol)
		return;
	lastcol = col;
	(r, g, b) := display.cmap2rgb(col);
	s := sys->sprint("  col: %d   R: %d   G: %d   B: %d", col, r, g, b);
	rect := posn(t, ".l");
	t.image.draw(rect, display.color(16r22), display.ones, (0,0));
	t.image.text(rect.min, display.ones, (0,0), font, s);
}

cmap(cr: Rect)
{
	# use writepixels because it's much faster than allocating all those colors.
	tmp := display.newimage(((0,0),(cr.dx(),cr.dy()/16+1)), 3, 0, 0);
	if(tmp == nil)
		return;
	buf := array[tmp.r.dx()*tmp.r.dy()] of byte;
	dx := cr.dx();
	dy := cr.dy();
	for(y:=0; y<16; y++){
		for(i:=tmp.r.dx()-1; i>=0; --i)
			buf[i] = byte (16*y+(16*i)/dx);
		for(k:=tmp.r.dy()-1; k>=1; --k)
			buf[dx*k:] = buf[0:dx];
		tmp.writepixels(tmp.r, buf);
		r: Rect;
		r.min.x = cr.min.x;
		r.max.x = cr.max.x;
		r.min.y = cr.min.y+(dy*y)/16;
		r.max.y = cr.min.y+(dy*(y+1))/16;
		t.image.draw(r, tmp, nil, tmp.r.min);
	}
}
