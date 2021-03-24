implement Wmset;

Mod : con "wmset";

include "sys.m";
	sys: Sys;

include "draw.m";
	draw : Draw;
	Image : import draw;

include "tk.m";
	tk: Tk;

include "wmlib.m";
	wmlib: Wmlib;

include "sh.m";

include "wm.m";
	wm: Wm;
	wms: Wms;
	wmdialog, execlist, menu_config, Icon, icons, screen, geom, applinit : import wm;

include "wmset.m";

ToolHeight:	con 48;
Maxsetup:	con 2048;
top: ref Tk->Toplevel;

#
# Implement Wmset API - Wm extension
#

menu_orig : array of string;
initme(me : Wm, s : Sys, d : Draw, t : Tk, w : Wmlib) : Wms
{
	if (wm != nil) return wms;
	wm = me; sys = s; draw = d; tk = t; wmlib = w;
	execlist = nil;

	CmdTable = nil;
	wms = load Wms SELF;
	return wms;
}

readsetup(t: ref Tk->Toplevel)
{
	top = t;
	resetmenu(t);
	drawmenuhead(t);

	file := "wmset";
	(ok, dir) := sys->stat(file);
	files : list of string;
	if (ok >= 0) {
	  if (dir.mode & sys->CHDIR)
	    files = listfiles(file);
	  else
	    files = file :: nil;
	}
	else
	  files = "wmsetup" :: nil;

	for(; files != nil; files = tl files) {
	  file = hd files;
	  #sys->print("file=%s\n", file);
	  if (file[len file -1] != '~')
	    readsetfile(t, file);
	}
	drawmenutail(t);
}

include "rtoken.m";
rt : Rtoken;
Id, readtoken : import rt;

readsetfile(t: ref Tk->Toplevel, file : string)
{
	fd := sys->open(file, sys->OREAD);
	if (fd == nil)
		return;
	if (rt == nil) {
		rt = load Rtoken Rtoken->PATH;
		if (rt == nil) {sys->print(Mod+": error loading %s %r\n", Rtoken->PATH); return;}
	}
	id := rt->id();
	tokens := ":";
	delim := "\r\n";
	max := Maxsetup;
	for(i := 0; (s := readtoken(fd, delim, id)) != nil; i++) {
		if (memberp(s[0], delim))
			continue;
		if (s[0] == '#')	continue;
		if (memberp(s[len s -1], delim))
			s = s[0:len s -1];
		if (i > max) {
			yn := rwmdialog("error -fg red", "Wm startup",
				       "Wmsetup file is very large\nkeep reading it?", 0, "Yes" :: "No" :: nil);
			if (yn)
				return;
			max *=2;
		}
		readcommand(t, s, tokens, file, i);
	}
	execlist = lreverse(execlist);
	if(id.n < 0)
		wmdialog("error -fg red", "Wm startup",
			"Error reading wmsetup:\n"+sys->sprint("%r"), 0, "Ignore wmsetup" :: nil);
}

Post : con "post";
runcommand(ctxt: ref Draw->Context, args: list of string)
{
	args = runcommandgrp(ctxt, args);
	if (args == nil) return;

	nosp := 0;
	(nosp, args) = nospawn(spawnable(args, 1));
	s := wmarg(hd args);

	# Extend post to user defined menus
	if (eqkeyp(Post, s)) {
	  t := top;
	  label := s[len Post:];
	  mh := int tk->cmd(t, label+" cget height");
	  postcmd := label+" post 0 " + string (screen.image.r.max.y - ToolHeight - mh - 4);
	  tk->cmd(t, postcmd);
	  return;
	}

	if (s != nil && tl args == nil && (vargs := findcmd(s)) != nil) {
		if (find_deiconify(s)) return;
		else {
			(nosp, args) = nospawn(spawnable(vargs, 1));
			if (args != nil) s = hd args;
			else {sys->print("error: findcmd [%s]\n", s); return;}
		}
	}
	case s {
	"shutdown" => return wmshutdown(top, ctxt);
	"reset" => return wmreset(top);
	# Non /dis/wm geometry added only if Geom argument matched
    	"" =>	args = hd args :: addgeom(tl args, 0);
    	* =>	args = hd args :: addgeom(tl args, 1);
	}
	runcmd(ctxt, args, nosp);
}

tokenize(s, t : string) : (int, list of string)
{
	return kotenizer(s, t, 0);
}

newicon(i : Icon, task : chan of string)
{
	if (Task == nil) Task = task;
	b := findimap(namefid(i.name));
	if (b != nil) b.icons = ref i :: b.icons;
	wm->newicon(i, task);
}

Imap : list of (int, ref Button);
iconify(label: string, fid: int): string
{
	#sys->print("iconify(%s, %d)\n", label, fid);
	cmd := cmdicon_name(label);
	bn := findbutton(cmd, nil);
	if (bn != nil) {
	  rmimap(fid);
	  Imap = (fid, bn) :: Imap;
	}
	return wm->iconify(label, fid);
}

deiconify(name: string, fid: int)
{
	bid := namefid(name);
	b := rmimap(bid);
	if (b != nil) rmicon(bid, b);
	#sys->print("deiconify(%s, %d)\n", name, fid);
	wm->deiconify(name, fid);
}

#
# Command button: deiconify previous tasks
#

find_deiconify(s : string) : int
{
  b := findbutton(s, nil);
  if (b != nil && b.icons != nil) {
    i := hd b.icons;
    #sys->print("runcommand: icon %s\n", i.name);
    spawn sendtask(i.name);
    return 1;
  }
  return 0;
}

deiconifies(is : list of Icon, revp : int)
{
  if (revp)
    is = reverseIlist(is);
  for(;is != nil; is = tl is)
    spawn sendtask((hd is).name);
}

reverseIlist(l : list of Icon) : list of Icon
{
  r : list of Icon;
  for(; l != nil; l = tl l)
    r = hd l :: r;
  return r;
}

Task : chan of string;
sendtask(s : string)
{
  if (Task != nil) Task <- = s;
}

rmicon(bid : int, b : ref Button) : ref Icon
{
  r : list of ref Icon;
  i : ref Icon;
  for(l := b.icons; l != nil; l = tl l)
    if ((hd l).fid != bid) r = hd l :: r;
    else {
      i = hd l;
      #sys->print("rmicon: rm Icon %s\n", i.name);
    }
  l = nil;
  for(; r != nil; r = tl r)
    l = hd r :: l;
  b.icons = l;
  return i;
}

namefid(s : string) : int
{
  p := tokenpre(s, ".");
  if (p > 0) return int s[p+1:];
  return 0;
}

rmimap(fid : int) : ref Button
{ 
  bi : ref Button;
  r : list of (int, ref Button);
  for (l := Imap; l != nil; l = tl l) {
    (i, b) := hd l;
    if (i == fid) bi = b;
    else r = hd l :: r;
  }
  Imap = r;
  return bi;
}

findimap(fid : int) : ref Button
{
  for (l := Imap; l != nil; l = tl l) {
    (i, b) := hd l;
    if (i == fid) return b;
  }
  return nil;
}

# from window label to command name

cmdicon_name(label : string) : string
{
  (nil, l) := kotenizer(label, ": -", 0);
  if (l != nil) label = hd l;
  return label;
}

# list files in dir (enable toolbar updates)

include "readdir.m";
rd : Readdir;

listfiles(dir : string) : list of string
{
  if (rd == nil)
    rd = load Readdir Readdir->PATH;
  if (rd == nil) {
    ioredirect();
    sys->fprint(stderr, Mod+": error loading %s %r\n", Readdir->PATH);
    return nil;
  }
  (a, n) := rd->init(dir, Readdir->NAME|Readdir->COMPACT|Readdir->FILE);
  files : list of string;
  for(i := 0; i < n; i++)
    files = dir+"/"+a[i].name :: files;
  return reverse(files);
}

#
# Extensions to Wm
#

readcommand(t : ref Tk->Toplevel, s, tokens, file : string, i : int)
{
  if (delcommand(s, tokens) >= 0) return;
  exe := "exec ";
  if (eqkeyp(exe, s)) s = s[len exe:]; else exe = nil;

  if(topcommandp(s, tokens)) {
    (n, l) := kotenizer(s, ";", 0);
    for(; l != nil; l = tl l) {
      s = trimtokens(hd l, " \t");
      (nil, args) := tokenize(s, " \t");
      if(args != nil) {
	args = disenable(spawnable(args, exe != nil));
	execlist = args :: execlist;
      }
    }
    return;
  }

  if (s[0] == ':') {cmdbutton(t, s[1:], tokens); return;}
  (nfield, field) := kotenizer(s, tokens, 0);
  if (nfield < 2) {
    wmdialog("error -fg red", "Wm startup",
	     "Error parsing "+file+" (line "+(string (i+1))+"): "+s,
	     0, "Ignore line" :: nil);
    return;
  }
  menucascade(t, nil, field);
}

# Process group commands separated by ';'

cmdgroup(cmd : string) : (int, string)
{
  if (cmd == nil) return (0, cmd);
  if (cmd[0] == '[') return (1, cmd);
  (n, l) := kotenizer(cmd, ";", 0);
  if (n > 1) {
    cmd = "["+cmd+"]";
#    sys->print("cmdgroup=%s\n", cmd);
    return (1, cmd);
  }
  return (0, cmd);
}

runcommandgrp(ctxt: ref Draw->Context, args: list of string) : list of string
{
  if ((hd args)[0] == '[') {
    cmd := stringargs(args);
#    sys->print("grp=%s\n", cmd);
    cmd = cmd[1:];
    if ((p := pos(']', cmd)) > 0)
      cmd = cmd[0:p];

    (fork, cmds) := parsecmdgrp(cmd);
    if (fork)
      spawn runcmdgrp(ctxt, cmds, 1);
    else
      runcmdgrp(ctxt, cmds, 0);
  }
  else {
    args = ldel(Quote, args);
    return disenable(spawnable(args, 1));
  }
  return nil;
}

parsecmdgrp(cmd : string) : (int, list of list of string)
{
  (n, cmds) := kotenizer(cmd, ";", 0);
  fork := 1;
  largs : list of list of string;
  for (; cmds != nil; cmds = tl cmds) {
    cmd = hd cmds;
#    sys->print("cmd=%s\n", cmd);
    (nil, args) := kotenizer(cmd, " \t", 0);
    args = ldel(Quote, args);
    if (tl cmds == nil && tl args == nil && hd args == NoSpawn)
      fork = 0;
    else {
      args = disenable(spawnable(args, 0));
      largs = args :: largs;
    }
  }
  return (fork, lreverse(largs));
}

runcmdgrp(ctxt: ref Draw->Context, cmds : list of list of string, fork : int)
{
  if (fork) {
    sys->pctl(sys->NEWPGRP, nil);
    sys->pctl(sys->FORKNS, nil);
    #sys->print("forked\n");
  }
  for(; cmds != nil; cmds = tl cmds) {
    (nosp, args) := nospawn(hd cmds);
    #sys->print("spawn=%d cmd=%s\n", !nosp, hd args);
    runcmd(ctxt, args, nosp);
  }
}

runcmd(ctxt: ref Draw->Context, args: list of string, nsp : int)
{
  if (args == nil) {sys->print("error: runcmd nil\n"); return;}

  wm := load Command hd args;
  if(wm != nil) {
    ioredirect();
    if (nsp)
      wm->init(ctxt, args);
    else
      spawn applinit(wm, ctxt, args);
  }
  else
    wmdialog("error -fg red",
	     "Start application",
	     "Failed to load application\n\""+hd args+
	     "\"\nError: "+sys->sprint("%r"),
	     0, "Continue (nothing loaded)" :: nil);
}

disenable(args : list of string) : list of string
{
  if (args == nil) return args;
  if (hd args == NoSpawn)
    return hd args :: discmd(hd tl args) :: tl tl args;
  else
    return discmd(hd args) :: tl args;
}

discmd(cmd : string) : string
{
  case cmd {"shutdown" or "reset" or "" => return cmd;}
  if (cmd[0] != '/' && !eqkeyp("/dis/", cmd))
    cmd = "/dis/"+cmd;
  if (!eqendp(".dis", cmd))
    cmd += ".dis";
  return cmd;
}

stdout, stderr : ref Sys->FD;
ioredirect()
{
  if (stdout == nil) {
    stdout = sys->open("/chan/wmstdout", sys->ORDWR);
    if (stdout != nil)
      sys->dup(stdout.fd, 1);
  }

  if (stderr == nil) {
    stderr = sys->open("/chan/wmstderr", sys->ORDWR);
    if(stderr != nil)
      sys->dup(stderr.fd, 2);
  }
}

# Extract command name out

wmarg(p : string) : string
{
  head := "/dis/wm/";
  tail := ".dis";
  if (eqkeyp(head, p))
    p = p[len head:];
  else
    return nil;
  if (eqendp(tail, p))
    return p[0:len p - len tail];
  return nil;
}

# Geometry insertion in second arg

Geom : con "$GEOM";
addgeom(args : list of string, force : int) : list of string
{
  if (args != nil && (p := search(Geom, hd args)) >= 0)
    return (hd args)[0:p]+geom()+(hd args)[p + len Geom:] :: tl args;
  if (force && (args == nil || tkarg(hd args) == nil))
    return geom() :: args;
  return args;
}

# Look for tk rectangle argument

tkarg(a : string) : string
{
  (nil, l) := sys->tokenize(a, " \t");
  if (l != nil)
    case hd l {"-x" or "-y" or "-w" or "-h" => return a;};
  return nil;
}

# Support unspawned commands

Spawn : con "&";
NoSpawn : con "!&";
spawnable(args : list of string, amp : int) : list of string
{
  if (args == nil) return nil;
  if (hd args == NoSpawn) return args;
  e := hd last(args);
  ea := NoSpawn;
  if (!eqendp(ea, e)) ea = Spawn;
  if (!eqendp(ea, e)) ea = nil;
  if (ea != nil) {
    (args, nil) = slicelast(args);
    if (len e > len ea)
      args = append(args, e[0:len e - len ea] :: nil);
    if (ea == NoSpawn)
      args = NoSpawn :: args;
  } else if (!amp)
    args = NoSpawn :: args;
  return args;
}

nospawn(args : list of string) : (int, list of string)
{
  if (args == nil) return (0, nil);
  if (hd args == NoSpawn) return (1, tl args);
  return (0, args);
}

# Built-in shutdown option

Shutdown: module
{
	PATH : con "/dis/shutdown.dis";
};

wmshutdown(t : ref Tk->Toplevel, ctxt : ref Draw->Context)
{
  if (rwmdialog("error -fg red", "Shutdown",
		"Terminate all applications",
		0, "Cancel" :: "Shutdown" :: nil))
    {
      display := ctxt.display;
      disp := display.image;
      disp.draw(disp.r, screen.fill, display.ones, disp.r.min);
      tk->cmd(t, "destroy "+Toolbar);
      tk->cmd(t, "destroy .");
      tk->cmd(t, "update");
      shd := load Command Shutdown->PATH;
      if (shd != nil)
	shd->init(ctxt, Shutdown->PATH :: "-h" :: nil);
      wmlib->titlectl(t, "exit");  
      ctxt.screen = nil;
      ctxt.display = nil;
      screen = nil;
    }
}

rwmdialog(ico, title, msg: string, dflt: int, labs: list of string) : int
{
  dt := tk->toplevel(screen, "-x 0 -y 0");
  return wmlib->dialog(dt, ico, title, msg, dflt, labs);
}

# Built-in reset option

wmreset(t : ref Tk->Toplevel)
{
  deiconifies(icons, 1);
  resetmenu(t);
  rmmenus(t);
  rmbuttons(t);
# load new version
  wmset := load Wmset Wmset->PATH;
  wm->ws = wms = wmset->initme(wm, sys, draw, tk, wmlib);
  wm->ws->readsetup(t);
  tk->cmd(t, "update");
}

menu_sep : con ".m add separator";
drawmenuhead(t : ref Tk->Toplevel)
{
  if (t != nil)
    wmlib->tkcmds(t, ahead(menu_sep, menu_config));
}

drawmenutail(t : ref Tk->Toplevel)
{
  if (t != nil)
    wmlib->tkcmds(t, atail(menu_sep, menu_config));
}

resetmenu(t : ref Tk->Toplevel)
{
  if (t != nil)
    tk->cmd(t, "destroy .m");
  if (menu_orig == nil) {
    menu_orig = menu_config;
    menu_config = array[len menu_orig +1] of string;
    menu_config[0:] = menu_orig;
    # add shutdown option
    menu_config[len menu_orig] = ".m add command -label Shutdown -command {send cmd shutdown}";
  }
  else {
    menu_config = array[len menu_orig] of string;
    menu_config[0:] = menu_orig;
  }
  if (t != nil)
    tk->cmd(t, "menu .m");
}

Toolbar : con ".toolbar";
resettoolbar(t : ref Tk->Toplevel)
{
  tk->cmd(t, "frame "+Toolbar+" -height 48 -width "+string screen.image.r.max.x);
  tk->cmd(t, "button "+Toolbar+".start -bitmap inferno.bit -command {send cmd post}");
	
  tk->cmd(t, "pack propagate "+Toolbar+" 0");

  tk->cmd(t, "pack "+Toolbar+".start -side left");
  tk->cmd(t, "pack "+Toolbar);
}

# Support n levels cascading menu

MenuList : list of string;
rmmenu(t : ref Tk->Toplevel, m : string) : string
{
  rm : string; r : list of string;
  for(l := reverse(MenuList); l != nil; l = tl l)
    if (hd l == m) tk->cmd(t, "destroy "+(rm = m));
    else r = hd l :: r;
  MenuList = r;
  return rm;
}

rmmenus(t : ref Tk->Toplevel)
{
  for(l := MenuList; l != nil; l = tl l)
    tk->cmd(t, "destroy "+hd l);
  MenuList = nil;
}

findmenu(m : string) : int
{
  if (MenuList == nil) MenuList = ".m" :: nil;
  for(l := MenuList; l != nil; l = tl l)
    if (hd l == m) return 1;
  return 0;
}

ulabel(m : string) : string
{
  mlabel := ".m";
  if (m != nil) mlabel+="_"+uname(m);
  return mlabel;
}

menucascade(t : ref Tk->Toplevel, m : string, field : list of string)
{
  mlabel := ulabel(m);
  while (mlabel != nil)
    (mlabel, field) = addmenulevel(t, mlabel, field);
}

afind(e : string, a : array of string) : int
{
  for (i := 0; i < len a; i++)
    if (a[i] == menu_sep) break;
  return i;
}

ahead(e : string, a : array of string) : array of string
{
  n := afind(e, a);
  if (n < len a) return a[0:n];
  return nil;
}

atail(e : string, a : array of string) : array of string
{ 
  n := afind(e, a);
  if (n < len a) return a[n:];
  return a;
}

addmenulevel(t : ref Tk->Toplevel, mlabel : string, items : list of string) : (string, list of string)
{
  menu := del(Quote, hd items);
  items = tl items;
  if (tl items != nil) {
    mpath := mlabel+"."+uname(menu);
    e := tk->cmd(t, mpath+" cget -width");
    if(e[0] == '!') {
      tk->cmd(t, "menu "+mpath);
      tk->cmd(t, mlabel+" insert 1 cascade -label {"+menu+"} -menu "+mpath);
    }
    return (mpath, items);
  }
  else {
    i := delcommandlabel(mlabel, menu);
    cmd := preparsecmd(hd items);
    tkc := mlabel+" add command -label {"+menu+"} -command {send exec "+cmd+"}";
    if (i > 0)
      menu_config[i] = tkc;
    else {
      tmp := menu_config;
      menu_config = array[len tmp +1] of string;
      menu_config[0:] = tmp;
      menu_config[len tmp] = tkc;
    }
  }
  return (nil, nil);
}

findar(e : string, a : array of string) : int
{
  for (i := 0; i < len a; i++)
    if (a[i] == e) break;
  return i;
}

preparsecmd(cmd : string) : string
{
  (gp, cmdgp) := cmdgroup(cmd);
  if (gp)
    return cmdgp;

  (nil, args) := kotenizer(cmd, " \t", 0);
  return stringargs(disenable(spawnable(args, 1)));
}

stringargs(args : list of string) : string
{
  if (args == nil) return nil;
  cmd := hd args;
  for(args = tl args; args != nil; args = tl args)
    cmd += " "+hd args;
  return cmd;
}

delcommandlabel(mlabel, menu : string) : int
{
  if (mlabel != ".m")
    return -1;

  # for menu_config contents only
  addcmd := mlabel+" add command -label ";
  for(i := 0; i < len menu_config; i++) {
    label := menu_config[i];
    if (eqkeyp(addcmd, label)) {
      mp := len addcmd;
      if (label[mp] == '{')
	mp++;
      lp := mp + len menu;
      if (lp <= len label && label[mp:lp] == menu) {
	menu_config[i] = "";
	#sys->print("Deleted |%s|\n", menu);
	return i;
      }
    }
  }
  return -1;
}

delcommand(s, tokens :string) : int
{
  dc := -1;
  m : string;
  if (s == nil) return dc;
  if (memberp(s[0], tokens)) {
    i := tokenpos(s[1:], tokens);
    if (i < 0) return dc;
    m = s[1:i];
    s = s[i:];
  }
  mlabel := ulabel(m);
  cmd : string;
  for((i, ls) := (0, len s); i < ls; i++)
    if (memberp(s[i], tokens)) {
      cmd = s[0:i];
      if (cmd == nil)
	break;
      if (i == ls -1) {
	dc = 0;
	break;
      }
      (n, r) := sys->tokenize(s[i+1:], " \t");
      if (r == nil)
	dc = 0;
      break;
    }
  if (dc >= 0)
    return delcommandlabel(mlabel, cmd);
  return dc;
}

# Top executable command

topcommandp(s, tokens : string) : int
{
  if (s == nil) return 0;
  if (memberp(s[0], tokens) || memberp(s[len s -1], tokens))
    return 0;
  (n, l) := kotenizer(s, tokens, 0);
  return n == 1;
}

# add toolbar button definitions

cmdbutton(t : ref Tk->Toplevel, s, tokens : string)
{
  dis := 1;
  if (memberp(s[len s -1], tokens))
    dis = 0;
  (nf, defs) := kotenizer(s, tokens, 0);
  if (dis) {
    if (nf == 2)
      addbuttondef(t, defs);
    else if (nf > 2) {
      #sys->print("cmdbutton: %s->\n", s); print1(defs);
      m := addmenubutton(t, hd defs);
      menucascade(t, m, tl defs);
    }
  }
  else if (defs != nil) {
    (but, ignore) := buttondef(hd defs);
    b := rmmenu(t, ulabel(but));
    if (b == nil) b = but;
    rmbutton(t, tbname(Toolbar, b));
  }
}

BBG : con " -bg #AAAAAA";
addmenubutton(t : ref Tk->Toplevel, def : string) : string
{
  (m, tkdef) := splittkdef(del(Quote, def));
  mlabel := ulabel(m);
  if (!findmenu(mlabel)) {
    MenuList = mlabel :: MenuList;
    tk->cmd(t, "menu "+mlabel);
    if (tkdef != nil)
      def = "{"+mlabel+"}"+tkdef;
    else
      def = "{"+mlabel+"} -text {"+m+"}"+BBG;
    addbutton(t, Toolbar, def, mlabel, nil);
  }
  return m;
}

addbuttondef(t : ref Tk->Toplevel, defs : list of string)
{
  def := del(Quote, hd defs);
  cmd := hd tl defs;
  (nil, cmd) = cmdgroup(cmd);
  (name, tkdef) := splittkdef(def);
  if (tkdef != nil)
    def = "{"+name+"}"+tkdef;
  else
    def = "{"+def+"} -text {"+def+"}"+BBG;
  addbutton(t, Toolbar, def, nil, cmd);
}

splittkdef(m : string) : (string, string)
{
  i := search(" -", m);
  if (i < 0)
    i = search("\t-", m);
  if (i < 0)
    return (m, nil);
  if (search(" -bg", m) < 0) m +=BBG;
  return (m[0:i], m[i:]);
}

Button : adt
{
  name : string;
  tbname : string;
  icons : list of ref Icon;
};

Blist : list of ref Button;
rmbutton(t : ref Tk->Toplevel, b : string) : string
{
  rm : string; r : list of ref Button;
  for(l := reverseBl(Blist); l != nil; l = tl l)
    if ((hd l).tbname == b) tk->cmd(t, "destroy "+(rm = b));
    else r = hd l :: r;
  Blist = r;
  return rm;
}

reverseBl(l : list of ref Button) : list of ref Button
{
  r : list of ref Button;
  for(; l != nil; l = tl l)
    r = hd l :: r;
  return r;
}

newbutton(b, tb : string)
{
  if (tb == nil) tb = tbname(Toolbar, uname(b));
  Blist = ref Button(b, tb, nil) :: Blist;
}

findbutton(b, tb : string) : ref Button
{
  if (tb == nil) tb = tbname(Toolbar, uname(b));
  for(l := Blist; l != nil; l = tl l)
    if ((hd l).tbname == tb)
      return hd l;
  return nil;
}

rmbuttons(t : ref Tk->Toplevel)
{
  for(; Blist != nil; Blist = tl Blist)
    tk->cmd(t, "destroy "+(hd Blist).tbname);
}

addbutton(t : ref Tk->Toplevel, where, def, menu, cmd : string)
{
  #sys->print("addbutton: %s, %s, %s, %s\n", where, def, menu, cmd);
  (name, opts) := buttondef(def);
  if (name == nil) return;

  bname := tbname(where, name);
  newbutton(name, bname);

  bcmd := "button "+bname+" "+opts;
  (n, args) := tokenize(cmd, " \t");
  if (menu != nil)
    tk->cmd(t, bcmd+" -command {send cmd post"+menu+"}");
  else if (args == nil) {
    sys->print("error: addbutton: empty command: [%s]\n", cmd);
    return;
  }
  else {
    args = disenable(spawnable(args, 1));
    addcmdtable(name, args);
    tk->cmd(t, bcmd+" -command {send cmd "+name+"}");
  }

  tk->cmd(t, "pack propagate "+where+" 0");
  tk->cmd(t, "pack "+bname+" -side left -fill y");
  tk->cmd(t, "pack "+where);
}

buttondef(def : string) : (string, string)
{
  if (def[0] == '{') {
    i := pos('}', def);
    if (i < 0) return (uname(def[1:len def]), nil);
    return (uname(def[1:i]), def[i+1:]);
  }
  i := tokenpos(def, " \t");
  if (i >= 0) return (uname(def[0:i]), def[i:]);
  return (uname(def), nil);
}

tbname(where, name : string) : string
{
  bname := where;
  if (name == nil) return nil;
  if (name[0] != '.')
    bname += ".";
  return bname += name;
}

uname(name : string) : string
{
  name = del(Quote, name);
  return nsubst('_', ' ', name);
}

# Manage extended command table

Cmdelt: adt
{
	key:	string;
	args:	list of string;
};

CmdTable : list of ref Cmdelt;

addcmdtable(key : string, args : list of string)
{
  CmdTable = ref Cmdelt(key, args) :: CmdTable;
}

findcmd(key : string) : list of string
{
  #sys->print("findcmd %s\n", key);
  for(cs := CmdTable; cs != nil; cs = tl cs) {
    ce := hd cs;
    #sys->print("entry %s\n", hd ce.args);
    if (eqkeyp(key, ce.key)) {
      return ce.args;}
  }
  return nil;
}

# A tiny parser for command line args

Quote : con ''';	#'
Comment : con '#';
kotenizer(s, t : string, del : int) : (int, list of string)
{
  l : list of string;
  p := len s;
  a := 0;
  q := 0;
  cnt := 0;

  for(i := 0; i < p; i++)
    if (s[i] == Quote) q = !q;
    else if (s[i] == Comment && !q) {p = i; break;}
    else if (!q && memberp(s[i], t)) {
      if (i > a) {cnt++; l = s[a:i] :: l;}
      a = i +1;
    }
  if (q)
    sys->print(Mod+": missing [%c]: %s<-\n", Quote, s);
  if (a < p) {cnt++; l = s[a:p] :: l;}
  if (del) {
    l = reversendel(Quote, l);
    return (len l, l);
  }
  return (cnt, reverse(l));
}

del(e : int, s : string) : string
{
  o := 0;
  for(i := 0; i < len s; i++) {
    if (o) s[i-o] = s[i];
    if (s[i] == e) o++;
  }
  return s[0:len s - o];
}

reversendel(e : int, l : list of string) : list of string
{
  r : list of string;
  for(; l != nil; l = tl l)
    if ((d := del(e, hd l)) != nil)
      r = d :: r;
  return r;
}

ldel(e : int, l : list of string) : list of string
{
  return reverse(reversendel(e, l));
}

# string utilities

eqkeyp(k, s : string) : int
{
  return len s >= len k  && s[0:len k] == k;
}

eqendp(k, s : string) : int
{
  return len s >= len k  && s[len s - len k:] == k;
}

nsubst(new, old : int, s : string) : string
{
  for(i := 0; i < len s; i++)
    if (s[i] == old)
      s[i] = new;
  return s;
}

pos(e : int, s : string) : int
{
  for(i := 0; i < len s; i++)
    if (e == s[i]) return i;
  return -1;
}

pre(e : int, s : string) : int
{
  for(i := len s -1; i >= 0; i--)
    if (e == s[i]) return i;
  return -1;
}

trimtokens(s, t : string) : string
{
  p := untokenpos(s, t);
  q := untokenpre(s, t);
  if (p < 0) p = 0;
  if (q < 0) q = len s -1;
  return s[p:q+1];
}

tokenpos(s, t : string) : int
{
  for(i := 0; i < len s; i++)
    if (memberp(s[i], t)) return i;
  return -1;
}

untokenpos(s, t : string) : int
{
  for(i := 0; i < len s; i++)
    if (!memberp(s[i], t)) return i;
  return -1;
}

tokenpre(s, t : string) : int
{
  for(i := len s -1; i >= 0; i--)
    if (memberp(s[i], t)) return i;
  return -1;
}

untokenpre(s, t : string) : int
{
  for(i := len s -1; i >= 0; i--)
    if (!memberp(s[i], t)) return i;
  return -1;
}

memberp(c : int, s : string) : int
{
  for(i := 0; i < len s; i++)
    if (c == s[i]) return 1;
  return 0;
}

search(p, s : string) : int
{
  for(i := 0; i < len s - len p; i++) {
    r := i;
    for(j := 0; j < len p; j ++)
      if (s[i+j] != p[j]) {r = -1; break;}
    if (r >= 0) return r;
  }
  return -1;
}

# list utilities

reverse(l : list of string) : list of string
{
  r : list of string;
  for(; l != nil; l = tl l)
    r = hd l :: r;
  return r;
}

lreverse(l : list of list of string) : list of list of string
{
  r : list of list of string;
  for(; l != nil; l = tl l)
    r = hd l :: r;
  return r;
}

append(h, t : list of string) : list of string
{
  if (h == nil) return t;
  if (t == nil) return h;
  r := reverse(h);
  for(; r != nil; r = tl r)
    t = hd r :: t;
  return t;
}

last(l : list of string) : list of string
{
  p : list of string;
  for(i := 0; l != nil; l = tl l)
    p = l;
  return p;
}

slicelast(l : list of string) : (list of string, list of string)
{
  if (l == nil)
    return (l, l);
  k, p : list of string;
  for(i := 0; l != nil; l = tl l) {
    p = l; k = hd l :: k;
  }
  return (reverse(tl k), p);
}

# Debug utilities
#
#tkcmd(t : ref Tk->Toplevel, cmd : string)
#{
#  sys->print("tkcmd: %s\n", cmd);
#  tk->cmd(t, cmd);
#}
#
#print1(l : list of string)
#{
#  for(; l != nil; l = tl l)
#    sys->print("%s|", hd l);
#  sys->print("\n");
#}
#
#printl(l : list of string)
#{
#  for(; l != nil; l = tl l)
#    sys->print("%s, ", hd l);
#  sys->print("\n");
#}
#

