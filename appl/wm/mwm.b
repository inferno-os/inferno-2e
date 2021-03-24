# minimal wm - obc
implement Mwm;

include "sys.m";
   sys : Sys;
   stderr : ref Sys->FD;

include "draw.m";
   draw: Draw;
   Display, Context, Screen, Image : import draw;

include "tk.m";
   tk : Tk;

toplevel : ref tk->Toplevel;

include "wmlib.m";
   wmlib : Wmlib;

Mwm : module
{ 	
        PATH: con "/dis/wm/mwm.dis";
	APP : con "/dis/wm/msh.dis";
	init: fn(ctxt: ref Draw->Context, args: list of string);
	mwm:  fn (ctxt: ref Draw->Context, rgb : string) : ref Draw->Context;
};

include "sh.m";
sh : Command;

init(ctxt: ref Draw->Context, args: list of string)
{
  sys = load Sys Sys->PATH;
  stderr = sys->fildes(2);
  app, color : string;
  if(args != nil)
     args = tl args;
  for(;args != nil;) {
    arg := hd args;
    args = tl args;
    if ('0' <= arg[0] && arg[0] <= '9')
      color = arg;
    else {
      app = arg;
      break;
    }
  }
  ctxt = mwm(ctxt, color);
  if(ctxt == nil) {
    sys->fprint(stderr, "Mwm: cannot initialize %r\n");
    return;
  }
  if(app == nil)
    app = Mwm -> APP;

  sh = load Command app;
  if(sh == nil)
    sys->fprint(stderr, "Mwm: cannot load %s %r\n", app);
  else
    spawn sh->init(ctxt, app :: args);
  # Shell stopper
  for(;;)
    sys->sleep(1000000);
}

ones: ref Draw->Image;
screen: ref Draw->Screen;
restart := 0;
mwm(ctxt: ref Draw->Context, rgb : string) : ref Draw->Context
{
	sys  = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk   = load Tk Tk->PATH;
	
	if (ctxt != nil) {
	  restart = 1;
	  screen = ctxt.screen;
	  spawn mpointer(screen);
	  spawn mkeyboard(screen);
	  return ctxt;
	}
	  
	wmlib = load Wmlib Wmlib->PATH;

	sys->bind("#p", "/prog", sys->MREPL);

	(nil, rgbl) := sys->tokenize(rgb, ".");
	r := 64; g := 128; b := 128;
	if (len rgbl == 3) {
	  r = int hd rgbl;
	  g = int hd tl rgbl;
          b = int hd tl tl rgbl;
 	}

	display := Display.allocate(nil);
	if(display == nil) {
		sys->print("can't initialize display: %r\n");
		return nil;
	}

	ones = display.ones;
	disp := display.image;
	screen = Screen.allocate(disp, display.rgb(r, g, b), 1);

	disp.draw(disp.r, screen.fill, display.ones, disp.r.min);
	ctxt = ref Draw->Context;
	ctxt.screen = screen;
	ctxt.display = display;

	spawn mpointer(screen);
	spawn mkeyboard(screen);

	toplevel = tk->toplevel(screen, "-x 0 -y 0");
	
	# Processes spawned after this should not kill mouse and keyboard
	sys->pctl(sys->NEWPGRP, nil);
	wmlib->init();
	return ctxt;
}

mkeyboard(scr: ref Draw->Screen)
{
	dfd := sys->open("/dev/keyboard", sys->OREAD);
	if(dfd == nil){
	  if(!restart)
		wmdialog("error -fg red", "Wm error",
			sys->sprint("can't open /dev/keyboard: %r"),
			0, "Exit"::nil);
		return;
	}

	b:= array[1] of byte;
	buf := array[10] of byte;
	i := 0;
	for(;screen != nil;) {
		n := sys->read(dfd, buf[i:], len buf - i);
		if(n < 1){
			wmdialog("error -fg red", "Wm error",
				sys->sprint("keyboard read error: %r"),
				0, "Exit"::nil);
			break;
		}
		i += n;
		while(i >0 && (nutf := sys->utfbytes(buf, i)) > 0){
			s := string buf[0:nutf];
			tk->keyboard(scr, int s[0]);
			buf[0:] = buf[nutf:i];
			i -= nutf;
		}
	}
}

wmdialog(ico, title, msg: string, dflt: int, labs: list of string) : int
{
	#dt := tk->toplevel(screen, "-x 0 -y 0");
	return wmlib->dialog(toplevel, ico, title, msg, dflt, labs);
}

mpointer(scr: ref Draw->Screen)
{
	count := 0;
	fd := sys->open("/dev/pointer", sys->OREAD);
	while ((fd == nil) && (count < 20)) {
		sys->sleep(1);	
		fd = sys->open("/dev/pointer", sys->OREAD);
		count++;
	}

	if (fd == nil && !restart) {
		sys->print("open: pointer: %r\n");
		return;
	}
	n := 0;
	buf := array[100] of byte;
	for(;screen != nil;) {
		n = sys->read(fd, buf, len buf);
		if(n <= 0)
			break;

		if(int buf[0] != 'm' || n != 37)
			continue;

		x := int(string buf[ 1:13]);
		y := int(string buf[12:25]);
		b := int(string buf[24:37]);
		tk->mouse(scr, x, y, b);
	}
}
