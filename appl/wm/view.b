implement View;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Rect, Display, Screen, Image: import draw;

include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;

include "imagefile.m";
	imageremap: Imageremap;
	readgif: RImagefile;
	readjpg: RImagefile;
	readxbitmap: RImagefile;

include "tk.m";
	tk: Tk;
	Toplevel: import tk;

include	"wmlib.m";
	wmlib: Wmlib;

include	"plumbmsg.m";
	plumbmsg: Plumbmsg;
	Msg: import plumbmsg;

stderr: ref Sys->FD;
screen: ref Screen;
display: ref Display;
x := 25;
y := 25;
img_patterns: list of string;
viewer := 0;
plumbed := 0;
background: ref Image;

View: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

init(ctxt: ref Draw->Context, argv: list of string)
{
	spawn realinit(ctxt, argv);
}

realinit(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;
	wmlib->init();

	stderr = sys->fildes(2);
	display = ctxt.display;
	screen = ctxt.screen;
	background = display.color(16r22);

	img_patterns = list of {
		"*.gif (GIF image files)",
		"*.jpg (JPEG image files)",
		"*.jpeg (JPEG image files)",
		"*.bit (Compressed image files)",
		"*.xbm (X Bitmap image files)",
		"* (All Files)"
		};

	imageremap = load Imageremap Imageremap->PATH;
	if(imageremap == nil){
		sys->fprint(stderr, "View: can't load remap: %r\n");
		return;
	}

	bufio = load Bufio Bufio->PATH;
	if(bufio == nil){
		sys->fprint(stderr, "View: can't load bufio: %r\n");
		return;
	}

	plumbmsg = load Plumbmsg Plumbmsg->PATH;
	if(plumbmsg->init(1, "view", 1000) >= 0)
		plumbed = 1;

	# tk args come in with blanks; turn them into separate arguments
	# to extract -x and -y
	a: list of string;
	while(argv != nil){
		(nil, l) := sys->tokenize(hd argv, " ");
		if(l == nil)
			a = hd argv :: a;
		else
			while(l != nil){
				a = hd l :: a;
				l = tl l;
			}
		argv = tl argv;
	}
	while(a != nil){
		argv = hd a :: argv;
		a = tl a;
	}

	argv = tl argv;
	while(argv!=nil && (s:=hd argv)[0]=='-' && len s>1){
		if(s[1]!='x' && s[1]!='y')
			break;
		i := 0;
		if(len s > 2)
			i = int s[2:];
		else{
			argv = tl argv;
			if(argv != nil)
				i = int hd argv;
		}
		if(s[1] == 'x')
			x = i;
		else
			y = i;
		argv = tl argv;
	}

	if(argv == nil){
		t := tk->toplevel(screen, "-x " + string x + " -y " + string y);
		f := wmlib->filename(screen, t, "View file name", img_patterns, nil);
		if(f == "") {
			#spawn view(nil, nil, "", 0);
			return;
		}
		argv = f :: nil;
	}

	errdiff := 1;

	for(;;){
		file: string;
		if(argv != nil){
			file = hd argv;
			argv = tl argv;
			if(file == "-f"){
				errdiff = 0;
				continue;
			}
		}else{
			file = plumbfile();
			if(file == nil)
				break;
			errdiff = 1;	# set this from attributes?
		}

		(ims, masks, err) := readimages(file, errdiff);

		if(ims == nil)
			sys->fprint(stderr, "View: can't read %s: %s\n", file, err);
		else
			spawn view(ims, masks, file, viewer++);
	}
}

readimages(file: string, errdiff: int) : (array of ref Image, array of ref Image, string)
{
	im := display.open(file);

	if(im != nil)
		return (array[1] of {im}, array[1] of ref Image, nil);

	fd := bufio->open(file, Sys->OREAD);
	if(fd == nil)
		return (nil, nil, sys->sprint("%r"));

	(mod, err1) := filetype(file, fd);
	if(mod == nil)
		return (nil, nil, err1);

	(ai, err2) := mod->readmulti(fd);
	if(ai == nil)
		return (nil, nil, err2);
	if(err2 != "")
		sys->fprint(stderr, "View: %s: %s\n", file, err2);
	ims := array[len ai] of ref Image;
	masks := array[len ai] of ref Image;
	for(i := 0; i < len ai; i++){
		masks[i] = transparency(ai[i], file);

		# if transparency is enabled, errdiff==1 is probably a mistake,
		# but there's no easy solution.
		(ims[i], err2) = imageremap->remap(ai[i], display, errdiff);
		if(ims[i] == nil)
			return(nil, nil, err2);
	}
	return (ims, masks, nil);
}

viewcfg := array[] of {
	"canvas .c -height 1 -width 1",
	"menu .m",
	".m add command -label Open -command {send cmd open}",
	".m add command -label Grab -command {send cmd grab}",
	".m add command -label Save -command {send cmd save}",
	"pack .c -side bottom -fill both -expand 1",
	"bind . <Configure> {send cmd resize}",
	"bind .c <Button-3> {send cmd but3 %X %Y}",
	"bind .c <Motion-Button-3> {}",
	"bind .c <ButtonRelease-3> {}",
	"bind .c <Button-1> {send but1 %X %Y}",
	"bind .m <ButtonRelease> {.m tkMenuButtonUp %x %y}",
};

DT: con 100;

timer(dt: int, ticks, pidc: chan of int)
{
	pidc <-= sys->pctl(0, nil);
	for(;;){
		sys->sleep(dt);
		ticks <-= 1;
	}
}

view(ims, masks: array of ref Image, file: string, myviewer: int)
{
	file = lastcomponent(file);
	vx := string (x+20*(myviewer%5));
	vy := string (y+20*(myviewer%5));
	(t, titlechan) := wmlib->titlebar(screen, " -x "+vx+" -y "+vy,
			"View: "+file, Wmlib->Hide);

	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");
	but1 := chan of string;
	tk->namechan(t, but1, "but1");

	wmlib->tkcmds(t, viewcfg);
	tk->cmd(t, "update");

	r := widgposn(t, ".c");
	r = imconfig(t, ims[0]);
	t.image.draw(r, ims[0], masks[0], ims[0].r.min);

	pid := -1;
	ticks := chan of int;
	if(len ims > 1){
		pidc := chan of int;
		spawn timer(DT, ticks, pidc);
		pid = <-pidc;
	}
	imno := 0;
	grabbing := 0;

	lastr := rect(t);

	for(;;) alt{
	s := <-titlechan =>
		if(s == "exit"){
			if(pid >= 0){
				fd := sys->open("/prog/"+string pid+"/ctl", Sys->OWRITE);
				sys->write(fd, array of byte "kill", 4);
			}
			return;
		}
		wmlib->titlectl(t, s);
		if(s == "task")
			t.image.draw(r, ims[imno], masks[imno], ims[0].r.min);

	<-ticks =>
		if(!lastr.eq(t.image.r)) # avoid smashing moving window
			break;
		if(masks[imno] != nil){
			t.image.flush(Draw->Flushoff);
			t.image.draw(r, background, nil, ims[0].r.min);
		}
		++imno;
		if(imno >= len ims)
			imno = 0;
		t.image.draw(r, ims[imno], masks[imno], ims[0].r.min);
		t.image.flush(Draw->Flushnow);

	s := <-cmd =>
		(nil, l) := sys->tokenize(s, " ");
		case (hd l) {
		"resize" =>
			r = widgposn(t, ".c");
			t.image.draw(r, ims[imno], masks[imno], ims[0].r.min);
			lastr = rect(t);
		"open" =>
			spawn open(screen, t);
		"grab" =>
			tk->cmd(t, "cursor -bitmap cursor.drag; grab set .c");
			grabbing = 1;
		"save" =>
			patterns := list of {
				"*.bit (Inferno image files)",
				"*.gif (GIF image files)",
				"*.jpg (JPEG image files)",
				"* (All files)"
			};
			f := wmlib->filename(screen, t, "Save file name",
				patterns, nil);
			if(f != "") {
				fd := sys->create(f, Sys->OWRITE, 8r664);
				if(fd != nil) 
					display.writeimage(fd, ims[0]);
			}
		"but3" =>
			if(!grabbing) {
				xx := int hd tl l - 50;
				yy := int hd tl tl l - int tk->cmd(t, ".m yposition 0") - 10;
				tk->cmd(t, ".m activate 0; .m post "+string xx+" "+string yy+
					"; grab set .m; update");
			}
		}
	s := <- but1 =>
			if(grabbing) {
				(nil, l) := sys->tokenize(s, " ");
				xx := int hd l;
				yy := int hd tl l;
				grabtop := tk->intop(screen, xx, yy);
				if(grabtop != nil) {
					cim := grabtop.image;
					imr := Rect((0,0), (cim.r.dx(), cim.r.dy()));
					im := display.newimage(imr, 3, 0, draw->White);
					im.draw(imr, cim, display.ones, cim.r.min);
					tk->cmd(t, ".Wm_t.title configure -text {View: grabbed}");
					r = imconfig(t, im);
					# Would be nicer if this could be spun off cleanly
					ims = array[1] of {im};
					masks = array[1] of ref Image;
					imno = 0;
					t.image.draw(r, ims[0], masks[0], ims[0].r.min);
					cim = nil;
					grabtop = nil;
				}
				tk->cmd(t, "cursor -default; grab release .c");
				grabbing = 0;
			}
	}
}

# peculiar function to avoid leaving value in temporary,
# so window has no spurious references to hold it up when hidden
rect(t: ref Tk->Toplevel): Rect
{
	return t.image.r;
}

open(screen: ref Screen, t: ref tk->Toplevel)
{
	f := wmlib->filename(screen, t, "View file name", img_patterns, nil);
	t = nil;
	if(f != "") {
		(ims, masks, err) := readimages(f, 1);
		if(ims == nil)
			sys->fprint(stderr, "View: can't read %s: %s\n", f, err);
		else
			view(ims, masks, f, viewer++);
	}
}

lastcomponent(path: string) : string
{
	for(k:=len path-2; k>=0; k--)
		if(path[k] == '/'){
			path = path[k+1:];
			break;
		}
	return path;
}

imconfig(t: ref Toplevel, im: ref Draw->Image): Rect
{
	width := im.r.dx();
	height := im.r.dy();
	tr := widgposn(t, ".Wm_t");
	if(width < tr.dx())
		width = tr.dx();
	tk->cmd(t, ".c configure -width " + string width
		+ " -height " + string height + "; update");
	r := widgposn(t, ".c");
	return widgposn(t, ".c");
}

widgposn(t: ref Toplevel, name: string): Rect
{
	r: Rect;

	r.min.x = int tk->cmd(t, name + " cget -actx");
	r.min.y = int tk->cmd(t, name + " cget -acty");
	r.max.x = r.min.x + int tk->cmd(t, name + " cget -width");
	r.max.y = r.min.y + int tk->cmd(t, name + " cget -height");

	return r;
}

plumbfile(): string
{
	if(!plumbed)
		return nil;
	for(;;){
		msg := Msg.recv();
		if(msg == nil){
			sys->print("View: can't read /chan/plumb.view: %r\n");
			return nil;
		}
		if(msg.kind != "text"){
			sys->print("View: can't interpret '%s' kind of message\n", msg.kind);
			continue;
		}
		file := string msg.data;
		if(len file>0 && file[0]!='/' && len msg.dir>0){
			if(msg.dir[len msg.dir-1] == '/')
				file = msg.dir+file;
			else
				file = msg.dir+"/"+file;
		}
		return file;
	}
}

Tab: adt
{
	suf:	string;
	path:	string;
	mod:	RImagefile;
};

GIF: con 0;
JPG: con 1;
PIC: con 2;
XBM: con 3;

tab := array[] of
{
	GIF => Tab(".gif",	RImagefile->READGIFPATH,	nil),
	JPG => Tab(".jpg",	RImagefile->READJPGPATH,	nil),
	PIC => Tab(".pic",	RImagefile->READPICPATH,	nil),
	XBM => Tab(".xbm",	RImagefile->READXBMPATH,	nil),
};

filetype(file: string, fd: ref Iobuf): (RImagefile, string)
{
	for(i:=0; i<len tab; i++){
		n := len tab[i].suf;
		if(len file>n && file[len file-n:]==tab[i].suf)
			return loadmod(i);
	}

	# sniff the header looking for a magic number
	buf := array[20] of byte;
	if(fd.read(buf, len buf) != len buf)
		return (nil, sys->sprint("%r"));
	fd.seek(0, 0);
	if(string buf[0:6]=="GIF87a" || string buf[0:6]=="GIF89a")
		return loadmod(GIF);
	if(string buf[0:5] == "TYPE=")
		return loadmod(PIC);
	jpmagic := array[] of {byte 16rFF, byte 16rD8, byte 16rFF, byte 16rE0,
		byte 0, byte 0, byte 'J', byte 'F', byte 'I', byte 'F', byte 0};
	for(i=0; i<len jpmagic; i++)
		if(jpmagic[i]>byte 0 && buf[i]!=jpmagic[i])
			break;
	if(i == len jpmagic)
		return loadmod(JPG);
	if(string buf[0:7] == "#define")
		return loadmod(XBM);
	return (nil, "can't recognize file type");
}

loadmod(i: int): (RImagefile, string)
{
	if(tab[i].mod == nil){
		tab[i].mod = load RImagefile tab[i].path;
		if(tab[i].mod == nil)
			sys->fprint(stderr, "View: can't find %s reader: %r\n", tab[i].suf);
		else
			tab[i].mod->init(bufio);
	}
	return (tab[i].mod, nil);
}

transparency(r: ref RImagefile->Rawimage, file: string): ref Image
{
	if(r.transp == 0)
		return nil;
	if(r.nchans != 1){
		sys->fprint(stderr, "View: can't do transparency for multi-channel image %s\n", file);
		return nil;
	}
	i := display.newimage(r.r, 3, 0, 0);
	if(i == nil){
		sys->fprint(stderr, "View: can't allocate mask for %s: %r\n", file);
		return nil;
	}
	pic := r.chans[0];
	npic := len pic;
	mpic := array[npic] of byte;
	index := r.trindex;
	for(j:=0; j<npic; j++)
		if(pic[j] == index)
			mpic[j] = byte 0;
		else
			mpic[j] = byte 16rFF;
	i.writepixels(i.r, mpic);
	return i;
}
