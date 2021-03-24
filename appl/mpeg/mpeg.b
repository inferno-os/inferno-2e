implement WmMpeg;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Point, Rect, Display, Image: import draw;

include "tk.m";
	tk: Tk;
	Toplevel: import tk;

include	"wmlib.m";
	wmlib: Wmlib;
	ctxt: ref Draw->Context;
	ones: ref Draw->Image;

include "mpegio.m";

mio: Mpegio;
decode: Mpegd;
remap: Remap;
Mpegi: import mio;

WmMpeg: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

Stopped, Playing, Stepping, Paused: con iota;
state	:= Stopped;
depth := -1;
sdepth: int;
cvt: ref Image;

pixelrec: Draw->Rect;

decoders := array[] of {
	Mpegd->PATH4,
	Mpegd->PATH4,
	Mpegd->PATH4,
	Mpegd->PATH,
};

remappers := array[] of {
	Remap->PATH1,
	Remap->PATH2,
	Remap->PATH4,
	Remap->PATH,
};

task_cfg := array[] of {
	"canvas .c",
	"frame .b",
	"button .b.File -text File -command {send cmd file}",
	"button .b.Stop -text Stop -command {send cmd stop}",
	"button .b.Pause -text Pause -command {send cmd pause}",
	"button .b.Step -text Step -command {send cmd step}",
	"button .b.Play -text Play -command {send cmd play}",
	"frame .f",
	"label .f.file -text {File:}",
	"label .f.name",
	"pack .f.file .f.name -side left",
	"pack .b.File .b.Stop .b.Pause .b.Step .b.Play -side left",
	"pack .f -fill x",
	"pack .b -anchor w",
	"pack .c -side bottom -fill both -expand 1",
	"pack propagate . 0",
};

init(xctxt: ref Draw->Context, argv: list of string)
{
	sys  = load Sys  Sys->PATH;
	draw = load Draw Draw->PATH;
	tk   = load Tk   Tk->PATH;
	wmlib= load Wmlib Wmlib->PATH;

	ctxt = xctxt;
	ones = ctxt.display.ones;

	wmlib->init();

	tkargs, darg: string;
	argv = tl argv;
	if (argv != nil) {
		s := hd argv;
		argv = tl argv;
		if (len s > 2 && s[0:1] == "-x") {
			tkargs = s;
			if (argv != nil)
				darg = hd argv;
		} else
			darg = s;
	}
	if (darg != nil)
		depth = int darg;
	sdepth = ctxt.display.image.ldepth;
	if (depth < 0 || depth > sdepth)
		depth = sdepth;
	(t, menubut) := wmlib->titlebar(ctxt.screen, tkargs, "MPEG Player", 0);

	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");

	wmlib->tkcmds(t, task_cfg);

	tk->cmd(t, "bind . <Configure> {send cmd resize}");
	tk->cmd(t, "update");

	mio = load Mpegio Mpegio->PATH;
	decode = load Mpegd decoders[depth];
	remap = load Remap remappers[depth];
	if(mio == nil || decode == nil || remap == nil) {
		wmlib->dialog(t, "error -fg red", "Loading Interfaces",
			"Failed to load the MPEG\ninterface: "+sys->sprint("%r"),
			0, "Exit"::nil);
		return;
	}
	mio->init();

	fname := "";
	ctl := chan of string;
	state = Stopped;

	for(;;) alt {
	menu := <-menubut =>
		if(menu[0] == 'e') {
			state = Stopped;
			return;
		}
		wmlib->titlectl(t, menu);
	press := <-cmd =>
		case press {
		"file" =>
			state = Stopped;
			patterns := list of {
				"*.mpg (MPEG movie files)",
				"* (All Files)"
			};
			fname = wmlib->filename(ctxt.screen, t, "Locate MPEG files",
				patterns, nil);
			if(fname != nil) {
				tk->cmd(t, ".f.name configure -text {"+fname+"}");
				tk->cmd(t, "update");
			}
		"play" =>
			if (state != Stopped) {
				state = Playing;
				continue;
			}
			if(fname != nil) {
				state = Playing;
				spawn play(t, fname);
			}
		"step" =>
			if (state != Stopped) {
				state = Stepping;
				continue;
			}
			if(fname != nil) {
				state = Stepping;
				spawn play(t, fname);
			}
		"pause" =>
			if(state == Playing)
				state = Paused;
		"stop" =>
			state = Stopped;
		}
	}
}

play(t: ref Toplevel, file: string)
{
	sp := list of { "Stop Play" };

	fd := sys->open(file, Sys->OREAD);
	if(fd == nil) {
		wmlib->dialog(t, "error -fg red", "Open MPEG file", sys->sprint("%r"), 0, sp);
		return;
	}
	m := mio->prepare(fd, file);
	m.streaminit(Mpegio->VIDEO_STR0);
	p := m.getpicture(1);
	decode->init(m);
	remap->init(m);

	canvr := canvsize(t);
	o := Point(0, 0);
	dx := canvr.dx();
	if(dx > m.width)
		o.x = (dx - m.width)/2;
	dy := canvr.dy();
	if(dy > m.height)
		o.y = (dy - m.height)/2;
	canvr.min = canvr.min.add(o);
	canvr.max = canvr.min.add(Point(m.width, m.height));

	if (depth != sdepth)
		cvt = ctxt.display.newimage(Rect((0, 0), (m.width, m.height)), depth, 0, 0);

	f, pf: ref Mpegio->YCbCr;
	for(;;) {
		if(state == Stopped)
			break;
		case p.ptype {
		Mpegio->IPIC =>
			f = decode->Idecode(p);
		Mpegio->PPIC =>
			f = decode->Pdecode(p);
		Mpegio->BPIC =>
			f = decode->Bdecode(p);
		}
		while(state == Paused)
			sys->sleep(0);
		if (p.ptype == Mpegio->BPIC) {
			writepixels(t, canvr, remap->remap(f));
			if(state == Stepping)
				state = Paused;
		} else {
			if (pf != nil) {
				writepixels(t, canvr, remap->remap(pf));
				if(state == Stepping)
					state = Paused;
			}
			pf = f;
		}
		if ((p = m.getpicture(1)) == nil) {
			writepixels(t, canvr, remap->remap(pf));
			break;
		}
	}
	state = Stopped;
}

writepixels(t: ref Toplevel, r: Rect, b: array of byte)
{
	if (cvt != nil) {
		cvt.writepixels(cvt.r, b);
		t.image.draw(r, cvt, ones, (0, 0));
	} else
		t.image.writepixels(r, b);
}

canvsize(t: ref Toplevel): Rect
{
	r: Rect;

	r.min.x = int tk->cmd(t, ".c cget -actx");
	r.min.y = int tk->cmd(t, ".c cget -acty");
	r.max.x = r.min.x + int tk->cmd(t, ".c cget -width");
	r.max.y = r.min.y + int tk->cmd(t, ".c cget -height");

	return r;
}
