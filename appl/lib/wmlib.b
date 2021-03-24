implement Wmlib;

include "sys.m";
	sys: Sys;
	Dir: import sys;

include "draw.m";
	draw: Draw;
	Screen: import draw;

include "tk.m";
	tk: Tk;
	Toplevel: import tk;

include "string.m";
	str: String;

include "wmlib.m";
	titlefd: ref Sys->FD;

include "workdir.m";
	wd: Workdir;

include "readdir.m";
	rdir: Readdir;

include "filepat.m";
	filepat: Filepat;

init()
{
	sys = load Sys Sys->PATH;
	tk = load Tk Tk->PATH;
	str = load String String->PATH;
}

title_cfg := array[] of {
	"frame .Wm_t -bg #aaaaaa -borderwidth 2",
	"button .Wm_t.e -bitmap exit.bit -command {send wm_title exit}",
	"pack .Wm_t.e -side right",
	"bind .Wm_t <Button-1> { raise .; send wm_title move}",
	"bind .Wm_t <Double-Button-1> {lower .}",
	"bind .Wm_t <Motion-Button-1> {}",
	"bind .Wm_t <Motion> {}",
	"bind .Wm_t.title <Button-1> { raise .;send wm_title move}",
	"bind .Wm_t.title <Double-Button-1> {lower .}",
	"bind .Wm_t.title <Motion-Button-1> {}",
	"bind .Wm_t.title <Motion> {}",
	"bind . <FocusIn> {.Wm_t configure -bg blue;"+
		" .Wm_t.title configure -bg blue;update}",
	"bind . <FocusOut> {.Wm_t configure -bg #aaaaaa;"+
		" .Wm_t.title configure -bg #aaaaaa;update}",
};

#
# Create a window manager title bar called .Wm_t which is ready
# to pack at the top level
#
titlebar(scr: ref Draw->Screen,
		where: string,
		title: string,
		flags: int): (ref Tk->Toplevel, chan of string)
{
	wm_title := chan of string;

	t := tk->toplevel(scr, "-borderwidth 2 -relief raised "+where);

	tk->namechan(t, wm_title, "wm_title");

	tk->cmd(t, "label .Wm_t.title -anchor w -bg #aaaaaa -fg white -text {"+title+"}");

	for(i := 0; i < len title_cfg; i++)
		tk->cmd(t, title_cfg[i]);

	if(flags & OK)
		tk->cmd(t, "button .Wm_t.ok -bitmap ok.bit"+
			" -command {send wm_title ok}; pack .Wm_t.ok -side right");

	if(flags & Hide)
		tk->cmd(t, "button .Wm_t.t -bitmap task.bit"+
			" -command {send wm_title task}; pack .Wm_t.t -side right");

	if(flags & Resize)
		tk->cmd(t, "button .Wm_t.m -bitmap maxf.bit"+
			" -command {send wm_title size}; pack .Wm_t.m -side right");

	if(flags & Help)
		tk->cmd(t, "button .Wm_t.h -bitmap help.bit"+
			" -command {send wm_title help}; pack .Wm_t.h -side right");

	# pack the title last so it gets clipped first
	tk->cmd(t, "pack .Wm_t.title -side left");
	tk->cmd(t, "pack .Wm_t -fill x");
	return (t, wm_title);
}

#
# titlectl implements the default window behavior for programs
# using title bars
#
titlectl(t: ref Toplevel, request: string)
{
	tk->cmd(t, "cursor -default");
	case request {
	"move" or "size" =>
		moveresize(t, request[0]);
	"exit" =>
		pid := sys->pctl(0, nil);
		fd := sys->open("/prog/"+string pid+"/ctl", sys->OWRITE);
		sys->fprint(fd, "killgrp");
		exit;
	"task" =>
		titlefd = sys->open("/chan/wm", sys->ORDWR);
		if(titlefd == nil) {
			sys->print("open wm: %r\n");
			return;
		}
		tk->cmd(t, ". unmap");
		sys->fprint(titlefd, "t%s", tk->cmd(t, ".Wm_t.title cget -text"));
		tk->cmd(t, ". map");
		titlefd	= nil;
	}
}

untaskbar()
{
	if(titlefd != nil)
		sys->fprint(titlefd, "r");
}

#
# find upper left corner for new child window
#
gx, gy: int;
ix, iy: int;
STEP: con 20;

geom(t: ref Toplevel): string
{
	tx := int tk->cmd(t, ". cget x");	# always want to be relative
	ty := int tk->cmd(t, ". cget y");	# to current position of parent

	if ( tx != gx || ty != gy ) {		# reset if parent moved
		gx = tx;
		gy = ty;
		ix = iy = 0;
	}
	else
	if ( ix + iy >= STEP * 20 ) {		# don't march off indefinitely
		ix = ix - iy + STEP;		# offset new series
		iy = 0;
		if ( ix >= STEP * 10 )
			ix = 0;
	}
	ix += STEP;
	iy += STEP;
	return "-x " + string (gx+ix) +" -y " + string (gy+iy);
}

#
# find upper left corner for subsidiary child window (always at constant
# position relative to parent)
#
localgeom(t: ref Toplevel): string
{
	tx := int tk->cmd(t, ". cget x");
	ty := int tk->cmd(t, ". cget y");

	return "-x " + string (tx+STEP) + " -y " + string (ty+STEP);
}

#
# Set the name that will be displayed on the task bar
#
taskbar(t: ref Toplevel, name: string): string
{
	old := tk->cmd(t, ".Wm_t.title cget -text");
	tk->cmd(t, ".Wm_t.title configure -text '"+name);
	return old;
}

#
# Dialog with wm to rubberband the window and return a new position
# or size
#
moveresize(t: ref Toplevel, mode: int)
{
	tk->cmd(t, "raise .");	# it's gonna end up on top, anyway, so let's
				#  make sure it gets events properly
	ox := int tk->cmd(t, ". cget -x");
	oy := int tk->cmd(t, ". cget -y");
	w := int tk->cmd(t, ". cget -width");
	h := int tk->cmd(t, ". cget -height");
	bw := int tk->cmd(t, ". cget -borderwidth");

	h += 2*bw;
	w += 2*bw;
	fd := sys->open("/chan/wm", sys->ORDWR);
	if(fd == nil) {
		sys->print("open wm: %r\n");
		return;
	}
	sys->fprint(fd, "%c%5d %5d %5d %5d", mode, ox, oy, ox+w, oy+h);

	reply := array[128] of byte;
	n := sys->read(fd, reply, len reply);
	if(n <= 0)
		return;

	s := string reply[0:n];
	if( len s < 18 )
		return;
	x := int s;
	y := int s[6:];
	if(mode == 'm') {
		if(ox != x || oy != y)
			tk->cmd(t, ". configure -x "+string x+" -y "+string y+"; update");
		return;
	}
	w = int s[12:] - x - 2*bw;
	h = int s[18:] - y - 2*bw;

	tk->cmd(t, ". configure -x "+ string x +
		   " -y "+string y+
		   " -width "+string w+
		   " -height "+string h+
		   "; update");
}

snarfget(): string
{
	fd := sys->open("/chan/snarf", sys->OREAD);
	if(fd == nil)
		return "";

	buf := array[8192] of byte;
	n := sys->read(fd, buf, len buf);
	if(n <= 0)
		return "";

	return string buf[0:n];
}

snarfput(buf: string)
{
	fd := sys->open("/chan/snarf", sys->OWRITE);
	if(fd != nil)
		sys->fprint(fd, "%s", buf);
}

tkquote(s: string): string
{
	r := "{";

	j := 0;
	for(i:=0; i < len s; i++) {
		if(s[i] == '{' || s[i] == '}' || s[i] == '\\') {
			r = r + s[j:i] + "\\";
			j = i;
		}
	}
	r = r + s[j:i] + "}";
	return r;
}

tkcmds(top: ref Tk->Toplevel, a: array of string)
{
	n := len a;
	for(i := 0; i < n; i++)
		v := tk->cmd(top, a[i]);
}

topopts := array[] of {
	"font"
#	, "bd"			# Wait for someone to ask for these
#	, "relief"		# Note: colors aren't inherited, it seems
};

opts(top: ref Tk->Toplevel) : string
{
	opts := "";
	for ( i := 0; i < len topopts; i++ ) {
		cfg := tk->cmd(top, ". cget " + topopts[i]);
		if ( cfg != "" && cfg[0] != '!' )
			opts += " -" + topopts[i] + " " + tkquote(cfg);
	}
	return opts;
}

dialog_config := array[] of {
	"",			# label .top.ico
	"",			# label .top.msg
	"frame .top -relief raised -bd 1",
	"frame .bot -relief raised -bd 1",
	"pack .top.ico -side left -padx 10 -pady 10",
	"pack .top.msg -side left -expand 1 -fill both -padx 10 -pady 10",
	"pack .Wm_t .top .bot -side top -fill both",
	"focus ."
};

dialog(parent: ref Tk->Toplevel,
	ico: string,
	title:string,
	msg: string,
	dflt: int,
	labs : list of string): int
{
	where := localgeom(parent) + " " + opts(parent);

	(t, tc) := titlebar(parent.image.screen, where, title, 0);

	d := chan of string;
	tk->namechan(t, d, "d");

	dialog_config[0] = "label .top.msg configure -text '" + msg;
	dialog_config[1] = "label .top.ico";
	if(ico != nil)
		 dialog_config[1] += " -bitmap " + ico;
	tkcmds(t, dialog_config);

	n := len labs;
	for(i := 0; i < n; i++) {
		tk->cmd(t, "button .bot.button" +
				string(i) + " -command {send d " +
				string(i) + "} -text '" + hd labs);

		if(i == dflt) {
			tk->cmd(t, "frame .bot.default -relief sunken -bd 1");
			tk->cmd(t, "pack .bot.default -side left -expand 1 -padx 10 -pady 8");
			tk->cmd(t, "pack .bot.button" + string i +
				" -in .bot.default -side left -padx 10 -pady 8 -ipadx 8 -ipady 4");
		}
		else
			tk->cmd(t, "pack .bot.button" + string i +
				" -side left -expand 1 -padx 10 -pady 10 -ipadx 8 -ipady 4");
		labs = tl labs;
	}

	if(dflt >= 0)
		tk->cmd(t, "bind . <Key-\n> {send d " + string dflt + "}");

	tk->cmd(t, "update");

	e := tk->cmd(t, "variable lasterror");
	if(e != "") {
		sys->print("Wmlib.dialog error: %s\n", e);
		return dflt;
	}

	for(;;) alt {
	ans := <-d =>
		return int ans;
	tcs := <-tc =>
		if(tcs[0] == 'e')
			return dflt;
		titlectl(t, tcs);
	}

}

getstring_config := array[] of {
	"label .lab",
	"entry .ent -relief sunken -bd 2 -width 200",
	"pack .lab .ent -side left",
	"bind .ent <Key-\n> {send f 1}",
	"focus .ent"
};

getstring(parent: ref Tk->Toplevel, msg: string): string
{
	where := localgeom(parent) + " " + opts(parent);

	t := tk->toplevel(parent.image.screen, where + " -borderwidth 2 -relief raised");
	f := chan of string;
	tk->namechan(t, f, "f");

	tkcmds(t, getstring_config);
	tk->cmd(t, ".lab configure -text '" + msg + ":   ");
	tk->cmd(t, "update");

	e := tk->cmd(t, "variable lasterror");
	if(e != "") {
		sys->print("getstring error: %s\n", e);
		return "";
	}

	<-f;

	ans := tk->cmd(t, ".ent get");
	if(len ans > 0 && ans[0] == '!')
		return "";

	tk->cmd(t, "destroy .");

	return ans;
}

TABSXdelta : con 2;
TABSXslant : con 5;
TABSXoff : con 5;
TABSYheight : con 35;
TABSYtop : con 10;
TABSBord : con 3;

# pseudo-widget for folder tab selections
mktabs(t: ref Tk->Toplevel, dot: string, tabs: array of (string, string), dflt: int): chan of string
{
	lab, widg: string;

	tk->cmd(t, "canvas "+dot+" -height "+string TABSYheight);
	tk->cmd(t, "pack propagate "+dot+" 0");
	c := chan of string;
	tk->namechan(t, c, dot[1:]);
	xpos := 2*TABSXdelta;
	top := 10;
	ypos := TABSYheight - 3;
	back := tk->cmd(t, dot+" cget -background");
	dark := "#999999";
	light := "#ffffff";
	w := 20;
	h := 30;
	last := "";
	for(i := 0; i < len tabs; i++){
		(lab, widg) = tabs[i];
		tag := lab+"_tag";
		sel := lab+"_sel";
		desel := lab+"_desel";
		xs := xpos;
		xpos += TABSXslant + TABSXoff;
		v := tk->cmd(t, dot+" create text "+string xpos+" "+string ypos+" -text "+lab+" -anchor sw -tags "+tag);
		bbox := tk->cmd(t, dot+" bbox "+tag);
		if(bbox[0] == '!')
			break;
		(r, nil) := parserect(bbox);
		r.max.x += TABSXoff;
		x1 := " "+string xs;
		x2 := " "+string(xs + TABSXslant);
		x3 := " "+string r.max.x;
		x4 := " "+string(r.max.x + TABSXslant);
		y1 := " "+string(TABSYheight - 2);
		y2 := " "+string TABSYtop;
		tk->cmd(t, dot+" create polygon " + x1+y1 + x2+y2 + x3+y2 + x4+y1 +
			" -fill "+back+" -tags "+tag);
		tk->cmd(t, dot+" create line " + x3+y2 + x4+y1 +
			" -fill "+dark+" -width 3 -tags "+tag);
		tk->cmd(t, dot+" create line " + x1+y1 + x2+y2 + x3+y2 +
			" -fill "+light+" -width 3 -tags "+tag);

		x1 = " "+string(xs+2);
		x4 = " "+string(r.max.x + TABSXslant - 2);
		y1 = " "+string(TABSYheight);
		tk->cmd(t, dot+" create line " + x1+y1 + x4+y1 +
			" -fill "+back+" -width 5 -tags "+sel);

		tk->cmd(t, dot+" raise "+v);
		tk->cmd(t, dot+" bind "+tag+" <ButtonRelease-1> 'send "+
			dot[1:]+" "+string i);

		tk->cmd(t, dot+" lower "+tag+" "+last);
		last = tag;

		xpos = r.max.x;
		ww := int tk->cmd(t, widg+" cget -width");
		wh := int tk->cmd(t, widg+" cget -height");
		if(wh > h)
			h = wh;
		if(ww > w)
			w = ww;
	}
	xpos += 4*TABSXslant;
	if(w < xpos)
		w = xpos;

	for(i = 0; i < len tabs; i++){
		(nil, widg) = tabs[i];
		tk->cmd(t, "pack propagate "+widg+" 0");
		tk->cmd(t, widg+" configure -width "+string w+" -height "+string h);
	}

	w += 2*TABSBord;
	h += 2*TABSBord + TABSYheight;

	tk->cmd(t, dot+" create line 0 "+string TABSYheight+
		" "+string w+" "+string TABSYheight+" -width 3 -fill "+light);
	tk->cmd(t, dot+" create line 1 "+string TABSYheight+
		" 1 "+string(h-1)+" -width 3 -fill "+light);
	tk->cmd(t, dot+" create line  0 "+string(h-1)+
		" "+string w+" "+string(h-1)+" -width 3 -fill "+dark);
	tk->cmd(t, dot+" create line "+string(w-1)+" "+string TABSYheight+
		" "+string(w-1)+" "+string(h-1)+" -width 3 -fill "+dark);

	tk->cmd(t, dot+" configure -width "+string w+" -height "+string h);
	tk->cmd(t, dot+" configure -scrollregion {0 0 "+string w+" "+string h+"}");
	tabsctl(t, dot, tabs, -1, string dflt);
	return c;
}

tabsctl(t: ref Tk->Toplevel,
	dot: string,
	tabs: array of (string, string),
	id: int,
	s: string): int
{
	lab, widg: string;

	nid := int s;
	if(id == nid)
		return id;
	if(id >= 0){
		(lab, widg) = tabs[id];
		tk->cmd(t, dot+" lower "+lab+"_sel");
		pos := tk->cmd(t, dot+" coords "+lab+"_tag");
		if(len pos >= 1 && pos[0] != '!'){
			(p, nil) := parsept(pos);
			tk->cmd(t, dot+" coords "+lab+"_tag "+string(p.x+1)+
				" "+string(p.y+1));
		}
		if(id > 0){
			(prev, nil) := tabs[id-1];
			tk->cmd(t, dot+" lower "+lab+"_tag "+prev+"_tag");
		}
		tk->cmd(t, dot+" delete "+lab+"_win");
	}
	id = nid;
	(lab, widg) = tabs[id];
	pos := tk->cmd(t, dot+" coords "+lab+"_tag");
	if(len pos >= 1 && pos[0] != '!'){
		(p, nli) := parsept(pos);
		tk->cmd(t, dot+" coords "+lab+"_tag "+string(p.x-1)+" "+string(p.y-1));
	}
	tk->cmd(t, dot+" raise "+lab+"_tag");
	tk->cmd(t, dot+" raise "+lab+"_sel");
	tk->cmd(t, dot+" create window "+string TABSBord+" "+
		string(TABSYheight+TABSBord)+" -window "+widg+" -anchor nw -tags "+lab+"_win");
	tk->cmd(t, "update");
	return id;
}

parsept(s: string): (Draw->Point, string)
{
	p: Draw->Point;

	(p.x, s) = str->toint(s, 10);
	(p.y, s) = str->toint(s, 10);
	return (p, s);
}

parserect(s: string): (Draw->Rect, string)
{
	r: Draw->Rect;

	(r.min, s) = parsept(s);
	(r.max, s) = parsept(s);
	return (r, s);
}

getfilename_config := array[] of {
	"frame .top",
	"label .top.l -text {Look in:}",
	"frame .top.f -relief sunken -bd 2",
	"entry .top.f.e -bg white -relief flat",
	"bind .top.f.e <Key-\n> {send b R}",
	"button .top.f.b -bitmap debug/open.bit -width 22 -height 24"+
		" -command {send b D}",
	"button .top.up -bitmap dir.up -command {send b U}",
	"pack .top.f.e .top.f.b -side left",
	"pack .top.l .top.f -side left",
	"pack .top.up -side left -padx 10",
	"canvas .cf.c -height 160 -bg white -xscrollcommand {.s set}",
	"bind .cf.c <Button-1> {send b s}",
	"bind .cf.c <Double-Button> {send b S}",
	"frame .cf -relief sunken -bd 2",
	"pack .cf.c -fill x",
	"scrollbar .s -orient horizontal -command {.cf.c xview}",
	"frame .pat",
	"label .pat.l -text {Type:} -width 6w -anchor w",
	"frame .pat.f -relief sunken -bd 2",
	"entry .pat.f.e -width 30w -bg white -relief flat",
	"button .pat.f.b -bitmap debug/open.bit -width 22 -height 24"+
		" -command {send b L}",
	"pack .pat.f.e .pat.f.b -side left",
	"frame .fn",
	"label .fn.l -text {Name:} -width 6w -anchor w",
	"frame .fn.e -relief sunken -bd 2",
	"entry .fn.e.e -width 30w -bg white  -relief flat",
	"bind .fn.e.e <Key-\n> {send b O}",
	"pack .fn.e.e -fill x",
	"button .fn.o -text {Open} -width 8w -command {send b O}",
	"pack .fn.l -side left -padx 4 -pady 4",
	"pack .fn.e -expand 1 -fill x -side left -padx 4 -pady 4",
	"pack .fn.o -side left -padx 20 -pady 4",
	"button .pat.c -text {Cancel} -width 8w -command {send b C}",
	"pack .pat.l .pat.f -side left -padx 4 -pady 4",
	"pack .pat.c -side left -padx 20 -pady 4",
	"pack .Wm_t -fill x",
	"pack .top -pady 4 -anchor w",
	"pack .cf -fill x -padx 10 -pady 4",
	"pack .s -fill x -padx 10",
	"pack .fn -expand 1 -fill x",
	"pack .pat",
	"update",
	"frame .dir",
	"scrollbar .dir.s -command {.dir.l yview}",
	"listbox .dir.l  -height 10h -width [.top.f.e cget -actwidth]"+
		" -yscrollcommand {.dir.s set}",
	"pack .dir.l -padx 2 -side left -fill x",
	"pack .dir.s -side left -fill y",
	"bind .dir.l <Double-Button> +{.dir unpost; send b N}",
	"frame .sel",
	"scrollbar .sel.s -command {.sel.l yview}",
	"listbox .sel.l -height 4h -width [.pat.f.e cget -actwidth]"+
		" -yscrollcommand {.sel.s set}",
	"pack .sel.l -padx 2 -side left -fill x",
	"pack .sel.s -side left -fill y",
	"bind .sel.l <Double-Button> +{.sel unpost; send b P}",
};

filename(scr: ref Screen, top: ref Toplevel,
		title: string,
		pat: list of string,
		dir: string): string
{
	if(wd == nil) {
		wd = load Workdir Workdir->PATH;
		rdir = load Readdir Readdir->PATH;
		filepat = load Filepat Filepat->PATH;
	}

	# start at  directory of previous file
	if(dir == "")
		dir = ".";

	where := localgeom(top) + " " + opts(top);

	(t, tc) := titlebar(scr, where+" -bd 1 -font /fonts/misc/latin1.6x13", "Open "+title, 0);

	b := chan of string;
	tk->namechan(t, b, "b");

	tkcmds(t, getfilename_config);

	cpattern := hd pat;
	tk->cmd(t, ".pat.f.e insert end '"+cpattern);
	for(s := pat; s != nil; s = tl s)
		tk->cmd(t, ".sel.l insert end '"+hd s);

	cpattern = getpattern(cpattern);

	nd := dodir(t, cpattern, dir);
	if(nd == "")
		return "";
	dir = nd;

	dirpost := 0;
	selpost := 0;

	loop:
	for(;;) alt {
	wt := <-tc =>
		if(wt[0] == 'e')
			break loop;
		titlectl(t, wt);
	bs := <-b =>
		case bs[0] {
		# Post directory box
		'D' =>
			if(dirpost != 0) {
				tk->cmd(t, ".dir unpost");
				dirpost = 0;
				break;
			}
			x := int tk->cmd(t, ".top.f cget -actx");
			y := int tk->cmd(t, ".top.f cget -acty");
			y += int tk->cmd(t, ".top.f cget -height");
			postdir := sys->sprint(".dir.l selection clear 0 end;"+
					       " .dir post %d %d", x, y);
			tk->cmd(t, postdir);
			dirpost = 1;
		# Post selection box
		'L' =>
			if(selpost != 0) {
				tk->cmd(t, ".sel unpost");
				selpost = 0;
				break;
			}
			x := int tk->cmd(t, ".pat.f cget -actx");
			y := int tk->cmd(t, ".pat.f cget -acty");
			y += int tk->cmd(t, ".pat.f cget -height");
			postsel := sys->sprint(".sel.l selection clear 0 end;"+
					       " .sel post %d %d", x, y);
			tk->cmd(t, postsel);
			selpost = 1;
		# Select a new pattern
		'P' =>
			selpost = 0;
			sel := tk->cmd(t, ".sel.l get [.sel.l curselection]");
			if(sel == "" || sel[0] == '!')
				break;
			tk->cmd(t, ".pat.f.e delete 0 end; .pat.f.e insert end '"+sel);
			cpattern = getpattern(sel);
			dodir(t, cpattern, dir);
		# Cancel button
		'C' =>
			break loop;
		# Cancel button
		'O' =>
			sel := tk->cmd(t, ".fn.e.e get");
			if(sel == "")
				break;
			return dir+"/"+sel;
		# New directory entered
		'R' =>
			nd = tk->cmd(t, ".top.f.e get");
			if(len nd != 0 && nd[0] != '/')
				nd = dir+nd;
			nd = dodir(t, cpattern, nd);
			if(nd != "")
				dir = nd;
			else
				dodir(t, cpattern, dir);
		# Select new directory
		'N' =>
			dirpost = 0;
			sel := tk->cmd(t, ".dir.l get [.dir.l curselection]");
			if(sel == "" || sel[0] == '!')
				break;
			dir = dodir(t, cpattern, newdir(sel, dir));
		# Up one level
		'U' =>
			dir = dodir(t, cpattern, dir+"/..");
		# Single or double click selection in the canvas
		's' or 'S' =>
			sel := tk->cmd(t, ".cf.c itemcget current -text");
			if(sel == "" || sel[0] == '!')
				break;
			tk->cmd(t, ".fn.e.e delete 0 end");
			if(bs[0] == 'S') {
				if(sel[len sel-1] == '/') {
					dir = dodir(t, cpattern, dir+"/"+sel);
					break;
				}
				return dir+"/"+sel;
			}
			tk->cmd(t, ".fn.e.e insert end '"+sel);
		}
	}
	return nil;
}

newdir(sel, dir: string): string
{
	if(sel[0] != ' ')
		dir += "/..";
	else
	if(len sel > 4 && sel[0:4] == "    ")
		dir += "/" + sel[4:];

	return dir;
}

getpattern(s: string): string
{
	if(s == "")
		return "* (All Files)";

	for(i := 0; i < len s && s[i] != ' '; i++)
		;

	return s[0:i];
}

dodir(t: ref Toplevel, pat: string, dir: string): string
{
	d := array[200] of Dir;

	(cdir, parelem, elem) := canondir(dir);
	if(cdir == "")
		return "";

	(a, n) := rdir->init(dir, rdir->NAME);
	if(a == nil)
		return "";

	tk->cmd(t, ".dir.l delete 0 end; .top.f.e delete 0 end");
	tk->cmd(t, ".top.f.e insert end '"+elem);
	tk->cmd(t, ".cf.c delete all");

	if(!(parelem == "/" && elem == "/"))
		tk->cmd(t, ".dir.l insert end '" + parelem);
	tk->cmd(t, ".dir.l insert end '  " + elem);
	x := 4;
	y := 10;
	max := 0;
	for(i := 0; i < n; i++) {
		disp := 0;
		if(filepat->match(pat, a[i].name))
			disp = 1;
		if(a[i].mode & sys->CHDIR) {
			tk->cmd(t, ".dir.l insert end '    " + a[i].name);
			a[i].name += "/";
			disp = 1;
		}
		if(disp) {
			s := sys->sprint(".cf.c create text %d %d -text {%s} -anchor w",
					x, y, a[i].name);
			tk->cmd(t, s);
			if(len a[i].name > max)
				max = len a[i].name;
			y += 20;
			if(y > 150) {
				x += (max*9)+10;
				y = 10;
				max = 0;
			}
		}
	}
	x += (max*9)+10;
	tk->cmd(t, ".cf.c configure -scrollregion {0 0 "+string x+" 160}");
	tk->cmd(t, ".cf.c xview moveto 0; update");
	return cdir;
}

# Turn dir into an absolute path, and return
# (absolute path, name of parent element, name of dir within parent);
# return ("","","") on error.
# Remove uses of "." and ".."
canondir(dir: string): (string, string, string)
{
	path, ppath, p : list of string;
	canon, elem, parelem : string;
	n : int;

	if(dir == "")
		return ("", "", "");
	if(dir[0] != '/') {
		pwd := wd->init();
		if(pwd == "")
			return ("", "", "");
		(n,path) = sys->tokenize(pwd + "/" + dir, "/");
	}
	else
		(n,path) = sys->tokenize(dir, "/");

	ppath = nil;
	for(p = path; p != nil; p = tl p) {
		if(hd p == "..") {
			if(ppath != nil)
				ppath = tl ppath;
		}
		else if(hd p != ".")
			ppath = (hd p) :: ppath;
	}
	canon = "/";
	elem = canon;
	parelem = canon;
	if(ppath != nil) {
		elem = hd ppath;
		canon = "/" + elem;
		if(tl ppath != nil)
			parelem = hd(tl ppath);
		ppath = tl ppath;
		while(ppath != nil) {
			canon ="/" +  (hd ppath) + canon;
			ppath = tl ppath;
		}
	}
	return (canon, parelem, elem);
}
