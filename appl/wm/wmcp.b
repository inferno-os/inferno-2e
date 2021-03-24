# Customized WM for CP:
#   - removed toolbar and Inferno button
#   - autostarts toolbar
#   - spawns event timer

implement Wm;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Screen, Display, Image: import draw;

include "tk.m";
	tk: Tk;

include "wmlib.m";
	wmlib: Wmlib;

ToolHeight:	con 0;
Maxsetup:	con 2048;

Wm: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

Command: module {
   init: fn(ctxt: ref Draw->Context, argv: list of string);
};

# This should be the command to start the program manager
xpmgr : con "/dis/wm/toolbar.dis";

rband: int;
rr, origrr: Draw->Rect;
rubberband := array[8] of ref Image;
nilband := array[8] of ref Image;
execlist: list of list of string;
ones: ref Draw->Image;
screen: ref Draw->Screen;
gx: int;
gy: int;
snarf: array of byte;

t: ref Tk->Toplevel;

WinMinX:	con	100;
WinMinY:	con	80;

RbTotk, RbMove, RbTrack, RbSize, RbDrag: con iota;
DragT, DragB, DragL, DragR: con 1<<iota;

wmIO: ref Sys->FileIO;

Rdreq: adt
{
	off:	int;
	nbytes:	int;
	fid:	int;
	rc:	chan of (array of byte, string);
};
rdreq: Rdreq;

Icon: adt
{
	name:	string;
	repl:	int;
	wc:	Sys->Rwrite;
};
icons: list of Icon;

stderr : ref sys->FD;
idlefd : ref sys->FD;

# for touch-click support
BUTTON_PRESS : con 7;
do_touch := int 0;

# menu_config := array[] of {
# 	"menu .m",
# 	".m add command -label About -command {send cmd about}",
# 	".m add separator",
# 	".m add command -label {Introduction\n To Inferno} -command {send cmd dsp}",
# 	".m add separator",
# 	".m add command -label Local -command {send cmd dir}",
# 	".m add command -label Remote -command {send cmd rmtdir}",
# 	".m add command -label Getauthinfo -command {send cmd wmgetauthinfo}",
# 	".m add command -label Tasks -command {send cmd task}",
# 	".m add command -label Notepad -command {send cmd edit}",
# 	".m add command -label Shell -command {send cmd sh}",
# };

init(nil: ref Draw->Context, nil: list of string)
{
	sys  = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	draw = load Draw Draw->PATH;
	tk   = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;

	sys->bind("#p", "/prog", sys->MREPL);
	sys->bind("#s", "/chan", sys->MREPL);

	sys->pctl(sys->NEWPGRP, nil);

	display := Display.allocate(nil);
	if(display == nil) {
		sys->print("can't initialize display: %r\n");
		return;
	}

	ones = display.ones;
	disp := display.image;
	#screen = Screen.allocate(disp, display.rgb(161, 195, 209), 1);
	screen = Screen.allocate(disp, display.rgb(64, 128, 128), 1);
	disp.draw(disp.r, screen.fill, display.ones, disp.r.min);

	ctxt := ref Draw->Context;
	ctxt.screen = screen;
	ctxt.display = display;

	rbdone := chan of int;
	bandinit();

	spawn mouse(screen, rbdone);
	spawn keyboard(screen);

	t = tk->toplevel(screen, "-y "+string (screen.image.r.max.y-ToolHeight));
	wmlib->init();

	cmd := chan of string;
	exec := chan of string;
	task := chan of string;
	tk->namechan(t, cmd, "cmd");
	tk->namechan(t, exec, "exec");
	tk->namechan(t, task, "task");

# 	tk->cmd(t, "frame .toolbar -height 48 -width "+string screen.image.r.max.x);
# 	tk->cmd(t, "button .toolbar.start -bitmap inferno.bit -command {send cmd post}");
# 	tk->cmd(t, "pack propagate .toolbar 0");
# 	tk->cmd(t, "pack .toolbar.start -side left");
# 	tk->cmd(t, "pack .toolbar");

#	wmlib->tkcmds(t, menu_config);

#	readsetup(t);

	mh := int tk->cmd(t, ".m cget height");
	postcmd := ".m post 0 " + string (screen.image.r.max.y - ToolHeight - mh - 4);

	tk->cmd(t, "update");

	rband = RbTotk;

	sys->bind("#s","/chan",sys->MBEFORE);
	wmIO = sys->file2chan("/chan", "wm");
	if(wmIO == nil) {
		wmdialog("error -fg red", "Wm startup",
			"Failed to make /chan/wm:\n"+sys->sprint("%r"),
			0, "Exit"::nil);
		return;
	}
	
	sys->bind("#s","/chan",sys->MBEFORE);
	snarfIO := sys->file2chan("/chan", "snarf");
	if(snarfIO == nil) {
		wmdialog("error -fg red", "Wm startup",
			"Failed to make /chan/snarf:\n"+sys->sprint("%r"),
			0, "Exit"::nil);
		return;
	}
	rdreq.fid = -1;
	
###	# start the slayer process
###	slayer := load Command "/dis/slayer.dis";
###	if(slayer != nil)
###		slayer->init(nil,nil);

	# start the idle timer
	makeidletmr();
	idlefd = sys->open("/dev/idletimer",sys->OWRITE);

	# start the program manager
	runcommand(ctxt,(xpmgr::nil));

# 	for(elist := execlist; elist != nil; elist = tl elist)
# 		runcommand(ctxt, hd elist);

	req := rdreq;
	for(;;) alt {
	req = <-wmIO.read =>
		if(req.rc == nil)	# not interested in EOF
			break;
		if(rdreq.fid != -1)
			req.rc <-= (nil, "busy");
		else
		if(rband == RbTotk)
			req.rc <-= (array of byte sys->sprint("%5d %5d %5d %5d",
				rr.min.x, rr.min.y, rr.max.x,rr.max.y), nil);
		else
			rdreq = req;
	(off, data, fid, wc) := <-wmIO.write =>
		if(wc == nil)		# not interested in EOF
			break;
		#
		# m rect - request move from this rect
		# s rect - request size change from this rect
		# t name - move to toolbar
		# r name - restore from tool bar
		# c [0|1] - stop/start touch-click (new for Shannon)
		#
		case int data[0] {
		 *  =>
			wc <-= (0, "bad req len");
		's' or 'm' =>
			setrr(data);
			rband = RbSize;
			if(int data[0] == 'm')
				rband = RbMove;
			else
				band(DragT|DragB|DragR|DragL);
			wc <-= (len data, nil);
		't' =>
			iconame := iconify(string data[1:len data], fid);
			icons = Icon(iconame, len data, wc) :: icons;
		'c' =>
			if(len data > 1 && int data[1] == '1')
				do_touch = BUTTON_PRESS;
			else
				do_touch = 0;
			wc <-= (len data, nil);
		}
		data = nil;
	moved := <-rbdone =>
		if(!moved)
			for(i:=0; i<8; i++)
				offscreen(i);
		if(rdreq.fid != -1) {
			rdreq.rc <-= (array of byte sys->sprint("%5d %5d %5d %5d",
				rr.min.x, rr.min.y, rr.max.x,rr.max.y), nil);
			rdreq.fid = -1;
		}
	s := <-cmd =>
		case s {
		"post" =>
			tk->cmd(t, postcmd);
		"edit" =>
			wmedit := load Wm "/dis/wm/edit.dis";
			spawn applinit(wmedit, ctxt,  "edit" :: geom() :: nil);
			wmedit = nil;
		"dsp" =>
			wmdsp := load Wm "/demo/dsp/runprofile.dis";
			spawn applinit(wmdsp, ctxt,  "dsp" :: geom() :: nil);
			wmdsp = nil;
		 * =>
			mwm := load Wm "/dis/wm/"+s+".dis";
			if(mwm == nil) {
				wmdialog("error -fg red",
					"Start application",
					"Failed to load application\n\""+s+
					"\"\nError: "+sys->sprint("%r"),
					0, "Continue (nothing loaded)" :: nil);
				break;
			}
			spawn applinit(mwm, ctxt,  s :: geom() :: "/" :: nil);
			mwm = nil;
		}
	e := <-exec =>
		runcommand(ctxt, e :: nil);
	detask := <-task =>
		deiconify(detask);
	(off, data, fid, wc) := <-snarfIO.write =>
		if(wc == nil)
			break;
		snarf = data;
		wc <-= (len data, "");
	req = <-snarfIO.read =>
		if(req.rc == nil)
			break;
		sl := len snarf;
		if(req.off >= sl)
			req.nbytes = 0;
		if(req.nbytes > sl)
			req.nbytes = sl;
		req.rc <-= (snarf[0:req.nbytes], "");		
	}
}

runcommand(ctxt: ref Draw->Context, args: list of string)
{
	wm := load Wm hd args;
	if(wm != nil) {
		spawn applinit(wm, ctxt, (hd args) :: geom() :: (tl args));
		return;
	}
	wmdialog("error -fg red",
		"Start application",
		"Failed to load application\n\""+hd args+
		"\"\nError: "+sys->sprint("%r"),
		0, "Continue (nothing loaded)" :: nil);
}

applinit(mod: Wm, ctxt: ref Draw->Context, args: list of string)
{
	sys->pctl(sys->NEWPGRP|sys->FORKFD, nil);
	spawn mod->init(ctxt, args);
}

setrr(data: array of byte)
{
	rr.min.x = int string data[1:6];
	rr.min.y = int string data[6:12];
	rr.max.x = int string data[12:18];
	rr.max.y = int string data[18:];
	origrr = rr;
}

iconify(label: string, fid: int): string
{
	n := sys->sprint(".toolbar.%d", fid);
	c := sys->sprint("button %s -command {send task %s} -text '%s",
				n, n, label);
	tk->cmd(t, c);
	tk->cmd(t, "pack "+n+" -side left -fill y; update");
	return n;
}

deiconify(name: string)
{
	tmp: list of Icon;
	while(icons != nil) {
		i := hd icons;
		if(i.name == name) {
			alt {
			i.wc <-= (i.repl, nil) =>
				break;
			* =>
				break;
			}
		}
		else
			tmp = i :: tmp;
		icons = tl icons;
	}
	icons = tmp;

	tk->cmd(t, "destroy "+name);
	tk->cmd(t, "update");
}

geom(): string
{
	if(gx > 130) {
		gx = 0;
		gy = 0;
	}
	gx += 20;
	gy += 20;
	return "-x "+string gx+" -y "+string gy;
}

mouse(scr: ref Draw->Screen, rbdone: chan of int)
{
	xr: Draw->Rect;
	mode, xa, ya: int;
	moving: ref Draw->Image;

	count := 0;
	fd := sys->open("/dev/pointer", sys->OREAD);
	while ((fd == nil) && (count < 20)) {
		sys->sleep(1);	
		fd = sys->open("/dev/pointer", sys->OREAD);
		count++;
	}
	count =0;
	ofd := sys->open("#c/pointer", sys->OWRITE);
	while ((ofd == nil) && (count < 20)) {
		sys->sleep(1);	
		ofd = sys->open("#c/pointer", sys->OREAD);
		count++;
	}

	if (fd == nil) {
		sys->print("open: pointer: %r\n");
		return;
	}

	n := 0;
	buf := array[100] of byte;
	prevb := 0;
	for(;;) {
		n = sys->read(fd, buf, len buf);
		# echo it to local device to update mouse
		if(n <= 0)
			break;

		if(int buf[0] != 'm' || n != 37)
			continue;

		sys->write(ofd, buf, len buf);	

		# Reset event timer
		sys->write(idlefd,buf,1);
#		sys->print("Resetting event timer on mouse event\n");

		x := int(string buf[ 1:13]);
		y := int(string buf[12:25]);
		b := int(string buf[24:37]);

		# generate touch-click on 'button' event, but not
		# on continuous presses (i.e. drag)
		#sys->print("wmcp: x=%d y=%d b=%d\n", x, y, b);
		if ((b & do_touch) && prevb != b)
			gen_touch_click();
		prevb = b;

		case rband {
		RbTotk =>
			tk->mouse(scr, x, y, b);
		RbMove =>
			if((b & 1) == 0) {
				moving = nil;
				rband = RbTotk;
				rbdone <-= 1;
				break;
			}
			# rr.min is known to be on top now
			win := tk->intop(scr, rr.min.x, rr.min.y);
			xa = x;
			ya = y;
			# if mouse is moving when click happens, can get behind.
			# adjust starting point to compensate.
			if(xa < rr.min.x)
				xa = rr.min.x+5;
			if(ya < rr.min.y)
				ya = rr.min.y+5;
			if(xa >= rr.max.x)
				xa = rr.max.x-5;
			if(ya >= rr.max.y)
				ya = rr.max.y-5;
			xr = rr;
			if(win != nil) {
				moving = win.image;
				win = nil;
			}
			if(moving != nil && (xa != x || ya != y))
				moving.origin(origrr.min, rr.min);
			rband = RbTrack;
		RbTrack=>
			if((b & 1) == 0) {
				moving = nil;
				rband = RbTotk;
				rbdone <-= 1;
				break;
			}
			rr = draw->xr.addpt((x-xa, y-ya));
			if(moving != nil)
				moving.origin(origrr.min, rr.min);
		RbSize =>
			band(DragL|DragT|DragR|DragB);
			if(b == 0)
				break;
			mode = 0;
			tt := draw->rr.dx()/3;
			if(x > rr.min.x && x < rr.min.x+tt)
				mode |= DragL;
			else
			if(x > rr.max.x-tt && x < rr.max.x)
				mode |= DragR;
			tt = draw->rr.dy()/3;
			if(y > rr.min.y && y < rr.min.y+tt)
				mode |= DragT;
			else
			if(y > rr.max.y-tt && y < rr.max.y)
				mode |= DragB;
			if(mode == 0) {
				rband = RbTotk;
				rbdone <-= 0;
				break;
			}
			rband = RbDrag;
			xa = x;
			ya = y;
			xr = rr;
		RbDrag =>
			if((b & 1) == 0) {
				rband = RbTotk;
				rbdone <-= 0;
				break;
			}
			dx := x - xa;
			dy := y - ya;
			if(mode & DragL)
				rr.min.x = xr.min.x + dx;
			if(mode & DragR)
				rr.max.x = xr.max.x + dx;
			if(mode & DragT)
				rr.min.y = xr.min.y + dy;
			if(mode & DragB)
				rr.max.y = xr.max.y + dy;
			band(mode);
		}
	}
}

bandr := array[8] of {
	((0, 0), (4, 20)),
	((0, 0), (20, 4)),
	((-4, -20), (0, 0)),
	((-20, -4), (0, 0)),
	((-20, 0), (0, 4)),
	((-4, 0), (0, 20)),
	((0, -20), (4, 0)),
	((0, -4), (20, 0)),
};

bandinit()
{

	for(i:=0; i<8; i++){
		rubberband[i] = screen.newwindow(bandr[i], Draw->Red);
		offscreen(i);
	}
}

offscreen(i: int)
{
	rubberband[i].origin((0, 0), (-64, -64));
}

band(m: int)
{
	r0, r1: Draw->Rect;

	if(draw->rr.dx() < WinMinX)
		rr.max.x = rr.min.x + WinMinX;
	if(draw->rr.dy() < WinMinY)
		rr.max.y = rr.min.y + WinMinY;

	if(m & (DragT|DragL)) {
		r0 = (rr.min, (rr.min.x+4, rr.min.y+20));
		r1 = (rr.min, (rr.min.x+20, rr.min.y+4));
		rubberband[0].origin((0, 0), r0.min);
		rubberband[1].origin((0, 0), r1.min);
	}
	else {
		offscreen(0);
		offscreen(1);
	}

	if(m & (DragB|DragR)) {
		r0 = ((rr.max.x-4, rr.max.y-20), rr.max);
		r1 = ((rr.max.x-20, rr.max.y-4), rr.max);
		rubberband[2].origin((0, 0), r0.min);
		rubberband[3].origin((0, 0), r1.min);
	}
	else {
		offscreen(2);
		offscreen(3);
	}

	if(m & (DragT|DragR)) {
		r0 = ((rr.max.x-20, rr.min.y), (rr.max.x, rr.min.y+4));
		r1 = ((rr.max.x-4, rr.min.y), (rr.max.x, rr.min.y+20));
		rubberband[4].origin((0, 0), r0.min);
		rubberband[5].origin((0, 0), r1.min);
	}
	else {
		offscreen(4);
		offscreen(5);
	}

	if(m & (DragB|DragL)) {
		r0 = ((rr.min.x, rr.max.y-20), (rr.min.x+4, rr.max.y));
		r1 = ((rr.min.x, rr.max.y-4), (rr.min.x+20, rr.max.y));
		rubberband[6].origin((0, 0), r0.min);
		rubberband[7].origin((0, 0), r1.min);
	}
	else {
		offscreen(6);
		offscreen(7);
	}
}

keyboard(scr: ref Draw->Screen)
{
        cmd := array[10] of byte;
	dfd := sys->open("/dev/keyboard", sys->OREAD);
	if(dfd == nil){
		wmdialog("error -fg red", "Wm error",
			sys->sprint("can't open /dev/keyboard: %r"),
			0, "Exit"::nil);
		return;
	}

	b:= array[1] of byte;
	buf := array[10] of byte;
	i := 0;
	for(;;) {
		n := sys->read(dfd, buf[i:], len buf - i);
		if(n < 1){
			wmdialog("error -fg red", "Wm error",
				sys->sprint("keyboard read error: %r"),
				0, "Exit"::nil);
			break;
		}

		# Reset event timer
		sys->write(idlefd,buf,1);
#		sys->print("Resetting event timer on keyboard event\n");

		i += n;
		while(i >0 && (nutf := sys->utfbytes(buf, i)) > 0){
                        (c, ln, status) := sys->byte2char(buf, 0);
                        if ( c == 57446 ) # Call Server button pressed
                        {
                          fd := sys->open("/chan/key", sys->OWRITE);
                          num := sys->char2byte( 's', cmd, 0 );
                          tmp := sys->write(fd, cmd, num);
                          fd = nil;
                        }
			s := string buf[0:nutf];
			tk->keyboard(scr, int s[0]);
			buf[0:] = buf[nutf:i];
			i -= nutf;
		}
	}
}

readsetup(t: ref Tk->Toplevel)
{
	fd := sys->open("wmsetup", sys->OREAD);
	if(fd == nil)
		return;

	igl := "Ignore wmsetup" :: nil;
	buf := array[Maxsetup] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0) {
		wmdialog("error -fg red", "Wm startup",
			"Error reading wmsetup:\n"+sys->sprint("%r"), 0, igl);
		return;
	}
	if(n >= len buf) {
		wmdialog("error -fg red", "Wm startup",
			"Wmsetup file is too big", 0,  igl);
		return;
	}
	(nline, line) := sys->tokenize(string buf[0:n], "\r\n");
	while(line != nil) {
		s := hd line;
		line = tl line;
		if(s[0] == '#')
			continue;
		if(s[0:5] == "exec "){
			(nil, args) := sys->tokenize(s[5:], " \t");
			if(args != nil)
				execlist = args :: execlist;
			continue;
		}
		(nfield, field) := sys->tokenize(s, ":");
		if(nfield != 3) {
			wmdialog("error -fg red", "Wm startup",
				"Error parsing wmsetup line " + s,
				0, "Ignore line" :: nil);
			continue;
		}
		menu := hd field;
		mpath := ".m."+menu;
		for(j := 0; j < len mpath; j++)
			if(mpath[j] == ' ')
				mpath[j] = '_';
		e := tk->cmd(t, mpath+" cget -width");
		if(e[0] == '!') {
			tk->cmd(t, "menu "+mpath);
			tk->cmd(t, ".m insert 1 cascade -label {"+menu+"} -menu "+mpath);
		}
		field = tl field;
		name := hd field;
		field = tl field;
		tk->cmd(t, mpath+" add command -label {"+name+
			"} -command {send exec "+hd field+"}");
	}
}

wmdialog(ico, title, msg: string, dflt: int, labs: list of string)
{
	dt := tk->toplevel(screen, "-x 0 -y 0");
	wmlib->dialog(dt, ico, title, msg, dflt, labs);
}

makeidletmr () : int
{
    if(sys->bind("#s","/dev", Sys->MBEFORE) < 0){
	sys->fprint(stderr, "Error: (makeidletmr) can't bind file channel %r\n");
	return -1;
    }

    fileio := sys->file2chan("/dev","idletimer");
    if(fileio == nil){
	sys->fprint(stderr, "Error: (makeidletmr) file2chan failed %r\n");
	return -1;
    }
    spawn idletimer(fileio);
    return 0;
}

# Reading the idle timer gives the elapsed time (in msec) since it was last
#   written to.  It is written to by wmcp on keyboard and mouse events.

idletimer(fileio : ref sys->FileIO)
{
    off, nbytes, fid : int;
    wdata : array of byte;
    rc : Sys->Rread;
    wc : Sys->Rwrite;
    startime,endtime : int;
    idletime : int;

    sys->pctl(Sys->NEWPGRP, nil);

    startime = sys->millisec();

    for(;;) alt {
		(off, nbytes, fid, rc) = <-fileio.read => {
	    	if(rc == nil)
				break;
	    
	   		endtime = sys->millisec();
	    	idletime = endtime - startime;
	    	idlestr := string idletime;

	    	if(len idlestr > nbytes) {
				rc <-= (array of byte (idlestr[0:nbytes]), nil);
	    	} else {
				rc <-= (array of byte idlestr, nil);
	    	}
		}

		(off, wdata, fid, wc) = <-fileio.write => {
	    	if(wc == nil)
				break;


	    	startime = sys->millisec();

	    	wc <-= (len wdata,nil);
	    	wdata = nil;
		}
    }
}

# from /appl/shannon/config/devicesSh.b
touch_fd : ref sys->FD;

gen_touch_click()
{
#sys->print("wmcp: beep!\n");
	if (touch_fd == nil){
		touch_fd = sys->open("/dev/touch2dsp", Sys->OWRITE);
		if (touch_fd == nil){
			touch_fd = sys->open("#T/touch2dsp", Sys->OWRITE);
			if (touch_fd ==  nil)
				return;
		}
	}
	beep := array of byte "beep";
	sys->write(touch_fd, beep, len beep);
}

