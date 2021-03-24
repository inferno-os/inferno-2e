implement Tklib;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	screen: ref Draw->Screen;

include "tk.m";
	tk: Tk;

include "tklib.m";

include "string.m";
	str: String;

dialog_config := array[] of {
	"frame .top -relief raised -bd 1",
	"frame .bot -relief raised -bd 1",
	"pack .top .bot -side top -fill both",
	"label .top.msg",
	"pack .top.msg -side right -expand 1 -fill both -padx 10 -pady 10",
	"focus ."
};

getstring_config := array[] of {
	"label .lab",
	"entry .ent -relief sunken -bd 2 -width 200",
	"pack .lab .ent -side left",
	"bind .ent <Key-\n> {send f 1}",
	"focus .ent"
};

init(ctxt: ref Draw->Context)
{
	sys = load Sys  Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	str = load String String->PATH;

	screen = ctxt.screen;
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

is_err(s: string): int
{
	return (len s > 0 && s[0] == '!');
}

tkcmds(top: ref Tk->Toplevel, a: array of string)
{
	n := len a;
	for(i := 0; i < n; i++)
		v := tk->cmd(top, a[i]);
}

dialog(parent: ref Tk->Toplevel, msg: string, dflt: int, labs : list of string): int
{
	where := "-x 50 -y 50";

	if(parent != nil) {
		x := int tk->cmd(parent, ". cget -x");
		y := int tk->cmd(parent, ". cget -y");
		where = sys->sprint("-x %d -y %d", x+30, y+30);
	}

	t := tk->toplevel(screen, where + " -borderwidth 2 -relief raised");
	d := chan of string;
	tk->namechan(t, d, "d");

	tkcmds(t, dialog_config);

	tk->cmd(t, ".top.msg configure -text '" + msg);

	n := len labs;
	for(i := 0; i < n; i++) {
		tk->cmd(t, "button .bot.button" +
				string(i) + " -command {send d " +
				string(i) + "} -text '" + hd labs);

		if(i == dflt) {
			tk->cmd(t, "frame .bot.default -relief sunken -bd 1");
			tk->cmd(t, "pack .bot.default -side left -expand 1 -padx 10 -pady 8");
			tk->cmd(t, "pack .bot.button" + string(i) +
				" -in .bot.default -side left -padx 10 -pady 8 -ipadx 8 -ipady 4");
		}
		else
			tk->cmd(t, "pack .bot.button" + string(i) +
				" -side left -expand 1 -padx 10 -pady 10 -ipadx 8 -ipady 4");
		labs = tl labs;
	}

	if(dflt >= 0)
		tk->cmd(t, "bind . <Key-\n> {send d " + string(dflt) + "}");

	tk->cmd(t, "update");

	e := tk->cmd(t, "variable lasterror");
	if(e != "") {
		sys->print("dialog error: %s\n", e);
		return dflt;
	}

	ans := <-d;
	tk->cmd(t, "destroy .");

	return int ans;
}

getstring(parent: ref Tk->Toplevel, msg: string): string
{
	where := "-x 50 -y 50";
	if(parent != nil) {
		x := int tk->cmd(parent, ". cget -x");
		y := int tk->cmd(parent, ". cget -y");
		where = sys->sprint("-x %d -y %d", x+30, y+30);
	}
	t := tk->toplevel(screen, where + " -borderwidth 2 -relief raised");
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
	if(is_err(ans))
		return "";

	tk->cmd(t, "destroy .");

	return ans;
}

notice(parent: ref Tk->Toplevel, message: string)
{
	where := "-x 50 -y 50";
	if(parent != nil) {
		x := int tk->cmd(parent, ". cget -x");
		y := int tk->cmd(parent, ". cget -y");
		where = sys->sprint("-x %d -y %d", x+30, y+30);
	}

	t := tk->toplevel(screen, where+" -borderwidth 2 -relief raised");
	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");
	tk->cmd(t, "frame .f -borderwidth 2 -relief groove -padx 3 -pady 3");
	tk->cmd(t, "frame .f.f");
	tk->cmd(t, "label .f.f.l -bitmap error -foreground red");
	tk->cmd(t, "label .f.f.m -text '"+message);
	tk->cmd(t, "button .f.b -text {  OK  } -command {send cmd done}");
	tk->cmd(t, "pack .f.f.l .f.f.m -side left -expand 1 -padx 10 -pady 10");
	tk->cmd(t, "pack .f.f .f.b -padx 10 -pady 10");
	tk->cmd(t, "pack .f");
	tk->cmd(t, "update; cursor -default");
	<-cmd;
}

# pseudo-widget for folder tab selections
mktabs(t: ref Tk->Toplevel, dot: string, tabs: array of (string, string), dflt: int): chan of string
{
	Xdelta : con 2;
	Xslant : con 5;
	Xoff : con 5;
	Yheight : con 35;
	Ytop : con 10;
	Bord : con 2;

	tk->cmd(t, "canvas "+dot+" -height "+string Yheight);
	tk->cmd(t, "pack propagate "+dot+" 0");
	c := chan of string;
	tk->namechan(t, c, dot[1:]);
	xpos := 2*Xdelta;
	top := 10;
	ypos := Yheight - 3;
	back := tk->cmd(t, dot+" cget -background");
	dark := "#999999";
	light := "#ffffff";
	w := 20;
	h := 30;
	last := "";
	for(i := 0; i < len tabs; i++){
		(lab, widg) := tabs[i];
		tag := lab+"_tag";
		sel := lab+"_sel";
		desel := lab+"_desel";
		xs := xpos;
		xpos += Xslant + Xoff;
		v := tk->cmd(t, dot+" create text "+string xpos+" "+string ypos+" -text "+tabs[i].t0+" -anchor sw -tags "+tag);
		bbox := tk->cmd(t, dot+" bbox "+tag);
		if(bbox[0] == '!')
			break;
		(r, nil) := parserect(bbox);
		r.max.x += Xoff;
		x1 := " "+string xs;
		x2 := " "+string(xs + Xslant);
		x3 := " "+string r.max.x;
		x4 := " "+string(r.max.x + Xslant);
		y1 := " "+string Yheight;
		y2 := " "+string Ytop;
		tk->cmd(t, dot+" create polygon " + x1+y1 + x2+y2 + x3+y2 + x4+y1 + " -fill "+back+" -tags "+tag);
		tk->cmd(t, dot+" create line " + x1+y1 + x2+y2 + x3+y2 + " -fill "+light+" -width 2 -tags "+tag);
		tk->cmd(t, dot+" create line " + x3+y2 + x4+y1 + " -fill "+dark+" -width 2 -tags "+tag);

		x1 = " "+string(xs+1);
		x4 = " "+string(r.max.x + Xslant - 1);
		tk->cmd(t, dot+" create line " + x1+y1 + x4+y1 + " -fill "+back+" -width 2 -tags "+sel);

		tk->cmd(t, dot+" raise "+v);
		tk->cmd(t, dot+" bind "+tag+" <ButtonRelease-1> 'send "+dot[1:]+" "+string i);

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
	xpos += 4*Xslant;
	if(w < xpos)
		w = xpos;
	tk->cmd(t, "frame "+dot+".b -width "+string w+" -height "+string h);

	w += 2*Bord;
	h += 2*Bord + Yheight;

	tk->cmd(t, dot+" create line 0 "+string Yheight+" "+string w+" "+string Yheight+" -width 2 -fill "+light);
	tk->cmd(t, dot+" create line 1 "+string Yheight+" 1 "+string(h-1)+" -width 2 -fill "+light);
	tk->cmd(t, dot+" create line  0 "+string(h-1)+" "+string w+" "+string(h-1)+" -width 2 -fill "+dark);
	tk->cmd(t, dot+" create line "+string(w-1)+" "+string Yheight+" "+string(w-1)+" "+string(h-1)+" -width 2 -fill "+dark);

	tk->cmd(t, dot+" configure -width "+string w+" -height "+string h);
	tk->cmd(t, "pack propagate "+dot+".b 0");
	tk->cmd(t, dot+" create window "+string Bord+" "+string(Yheight+1)+" -window "+dot+".b -anchor nw");
	tk->cmd(t, dot+" configure -scrollregion 0 0 "+string w+" "+string h);
	tabsctl(t, dot, tabs, -1, string dflt);
	return c;
}

tabsctl(t: ref Tk->Toplevel, dot: string, tabs: array of (string, string), id: int, s: string): int
{
	nid := int s;
	if(id == nid)
		return id;
	if(id >= 0){
		(who, w) := tabs[id];
		tk->cmd(t, dot+" lower "+who+"_sel");
		pos := tk->cmd(t, dot+" coords "+who+"_tag");
		if(len pos >= 1 && pos[0] != '!'){
			(p, nil) := parsept(pos);
			tk->cmd(t, dot+" coords "+who+"_tag "+string(p.x+1)+" "+string(p.y+1));
		}
		if(id > 0){
			(prev, nil) := tabs[id-1];
			tk->cmd(t, dot+" lower "+who+"_tag "+prev+"_tag");
		}
		tk->cmd(t, "pack forget "+w);
	}
	id = nid;
	(who, w) := tabs[id];
	pos := tk->cmd(t, dot+" coords "+who+"_tag");
	if(len pos >= 1 && pos[0] != '!'){
		(p, nli) := parsept(pos);
		tk->cmd(t, dot+" coords "+who+"_tag "+string(p.x-1)+" "+string(p.y-1));
	}
	tk->cmd(t, dot+" raise "+who+"_tag");
	tk->cmd(t, dot+" raise "+who+"_sel");
	tk->cmd(t, "pack "+w+" -in "+dot+".b -expand 1 -fill both");
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
