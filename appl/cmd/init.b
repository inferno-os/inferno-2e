implement Init;

include "sys.m";
sys: Sys;
Connection: import sys;
print, bind, mount, dial, sleep: import sys;

include "draw.m";
draw: Draw;
Context, Display, Font, Rect, Image, Screen: import draw;

include "prefab.m";
prefab: Prefab;
Environ, Element, Compound, Style: import prefab;

include "string.m";
str: String;

include "newns.m";

include "keyring.m";
kr: Keyring;

include "security.m";
virgil: Virgil;

include "sh.m";

Init: module
{
	init:	fn(nil: ref Context, argv: list of string);
};

Mux: module
{
	init:	fn(nil: ref Context, argv: list of string);
};

Signon_mess:	con "Dialing Local Service Provider\nWait a moment ...";
Login_mess:	con "Connected to Service Provider";

rootfs(argv: list of string): int
{
	ok, n: int;
	c: Connection;

	targv := str->append("$FILESERVER", argv);
	if (targv == nil) {
		sys->print("cannot append $FILESERVER to argv\n");
		return -1;
	}
	fs := virgil->virgil(targv);
	targv = nil;
	if(fs == nil){
		sys->print("can't find file server address\n");
		return -1;
	}

	sys->print("dialing %s ...\n", "tcp!"+fs);
	(ok, c) = dial("tcp!"+fs+"!6666", nil);
	if(ok < 0)
		return -1;
	c.cfd = nil;
	sys->print("connected ...\n");

	ai := kr->readauthinfo("/nvfs/default");
	if(ai == nil){
		user := rf("/dev/user");
		af := "tcp!"+fs;
		if(len af >= Sys->NAMELEN)
			af = af[0:Sys->NAMELEN-2];
		ai = kr->readauthinfo("/usr/"+user+"/keyring/"+af);
	}
	if(ai == nil){
		manf := load Command "/dis/manufacture.dis";
		if(manf!=nil){
			sys->print("Please enter boxid: ");
			manf->init(nil, "manufacture" :: readline() :: nil);
			reg := load Command "/dis/mux/register.dis";
			if(reg!=nil){
				reg->init(nil, argv);
				ai = kr->readauthinfo("/nvfs/default");
				if(ai == nil)
					sys->print("readauthinfo failed after registration\n");
			}
			else{
				sys->print("failed to load /dis/mux/register.dis\n");
			}
		}
		else{
			sys->print("failed to load /dis/manufacture.dis\n");
		}
	}
	sys->print("authenticating ...\n");
	(id_or_err, secret) := kr->auth(c.dfd, ai, 0);
	if(secret == nil)
		sys->print("authentication failed: %s\n", id_or_err);
	else {
		if(id_or_err != "xyzzy")
			sys->print("authentication succeeded but not our service-provider: %s\n", id_or_err);
		# no line encryption or hashing
		algbuf := array of byte "none";
		kr->sendmsg(c.dfd, algbuf, len algbuf);
	}

	n = mount(c.dfd, "/n/remote", sys->MBEFORE, "");
	if(n > 0) {
		sys->print("mounted ...\n");
		return 0;
	}
	sys->print("rootfs: mount %s: %r\n", "/n/remote");
	return -1;
}

ones: ref Image;
screen: ref Screen;
menuenv: ref Environ;

init(nil: ref Context, argv: list of string)
{
	mux: Mux;
	c: ref Compound;
	phone, logo: ref Image;
	le, te, xe: ref Element;
	spec: string;
	buf := array[100] of byte;

	sys = load Sys Sys->PATH;
	if (sys == nil)
		return;
	draw = load Draw Draw->PATH;
	if (draw == nil) {
		sys->print("init: cannot load draw\n");
		return;
	}
	prefab = load Prefab Prefab->PATH;
	if (prefab == nil) {
		sys->print("init: cannot load prefab\n");
		return;
	}
	kr = load Keyring Keyring->PATH;
	if (kr == nil) {
		sys->print("init: cannot load kr\n");
		return;
	}
	virgil = load Virgil Virgil->PATH;
	if (virgil == nil) {
		sys->print("init: cannot load virgil\n");
		return;
	}
	str = load String String->PATH;
	if (str == nil) {
		sys->print("init: cannot load str\n");
		return;
	}

	display := Display.allocate(nil);
	disp := display.image;
	ones = display.ones;
	yellow := display.color(draw->Yellow);
	red := display.color(draw->Red);

	textfont := Font.open(display, "*default*");

	screencolor := display.rgb(161, 195, 209);
	menustyle := ref Style(
			textfont,			# titlefont
			textfont,			# textfont
			display.color(16r55),		# elemcolor
			display.color(draw->Black),	# edgecolor
			yellow,				# titlecolor	
			display.color(draw->Black),	# textcolor
			display.color(draw->White));	# highlightcolor

	screen = Screen.allocate(disp, screencolor, 0);
	screen.image.draw(screen.image.r, screencolor, ones, (0, 0));
	menuenv = ref Environ(screen, menustyle);

	logo = display.open("/icons/lucent.bit");
	if(logo == nil) {
		print("open: /icons/lucent.bit: %r\n");
		exit;
	}

	phone = display.open("/icons/phone.bit");
	if(phone == nil) {
		print("open: /icons/phone.bit: %r\n");
		exit;
	}

	#
	#  bind network for dialing root fs
	#
	bind("#I", "/net", Sys->MBEFORE);
	cs := load Command "/dis/lib/cs.dis";
	if(cs==nil)
		sys->print("init: failed to load cs\n");
	cs->init(nil, nil);

	#
	#  find a key file system
	#	first try local disk,
	#	then a file
	#
	nvramfd := sys->open("#H/hd0nvram", sys->ORDWR);
	if(nvramfd == nil)
		nvramfd = sys->open("/dev/nvram", sys->ORDWR);
	if(nvramfd != nil){
		spec = sys->sprint("#F%d", nvramfd.fd);
		if(bind(spec, "/nvfs", sys->MBEFORE|sys->MCREATE) < 0)
			print("init: bind %s: %r\n", spec);
	}

	le = Element.icon(menuenv, logo.r, logo, ones);
	le = Element.elist(menuenv, le, Prefab->EVertical);
	xe = Element.icon(menuenv, phone.r, phone, ones);
	xe = Element.elist(menuenv, xe, Prefab->EHorizontal);
	te = Element.text(menuenv, Signon_mess, Rect((0,0), (0,0)), Prefab->EText);
	xe.append(te);
	xe.adjust(Prefab->Adjpack, Prefab->Adjleft);
	le.append(xe);
	le.adjust(Prefab->Adjpack, Prefab->Adjup);
	c = Compound.box(menuenv, (150, 100),
		Element.text(menuenv, "Inferno", ((0,0),(0,0)), Prefab->ETitle), le);
	c.draw();

	for(;;) {
		if(rootfs(argv) == 0)
			break;

		sleep(1000);
	}

	#
	# default namespace
	#
	(ok, nil) := sys->stat("services/namespace.init");
	if(ok >= 0) {
		ns := load Newns Newns->PATH;
		if(ns == nil)
			print("Failed to load namespace builder: %r\n");
		else
			ns->newns(nil, "services/namespace.init");
	}
	if(spec != nil)
		bind(spec, "/nvfs", sys->MBEFORE|sys->MCREATE);	# our keys

	sleep(3000);

	zr := Rect((0,0), (0,0));
	le = Element.icon(menuenv, logo.r, logo, ones);
	le = Element.elist(menuenv, le, Prefab->EVertical);
	xe = Element.text(menuenv, Login_mess, zr, Prefab->EText);
	le.append(xe);

	i := display.newimage(Rect((0, 0), (320, 240)), 3, 0, 0);
	i.draw(i.r, menustyle.elemcolor, ones, i.r.min);
	xe = Element.icon(menuenv, i.r, i, ones);
	le.append(xe);

	le.adjust(Prefab->Adjpack, Prefab->Adjup);
	c = Compound.box(menuenv, (160, 50),
		Element.text(menuenv, "Inferno", ((0,0),(0,0)), Prefab->ETitle), le);
	c.draw();

	i2 := display.open("/icons/delight.bit");
	i.draw(i.r, i2, ones, i2.r.min);
	i2 = nil;
	le.append(Element.text(menuenv, "The Garden of Delights\nHieronymus Bosch", le.r, Prefab->EText));
	le.adjust(Prefab->Adjpack, Prefab->Adjup);
	c = Compound.box(menuenv, (160, 50),
		Element.text(menuenv, "Inferno", ((0,0),(0,0)), Prefab->ETitle), le);
	c.draw();
	sleep(5000);
	sys->bind("#U/dis", "/dis", sys->MAFTER);

	mux = load Mux "/dis/mux/mux.dis";
	if(mux == nil) {
		print("init: load /dis/mux/mux.dis: %r, halted ...");
		exit;
	}
	mux->init(ref Context(screen, display, nil, nil, nil, nil, nil), nil);
}

readline() : string
{
	reply : string;
 
	reply = nil;
	buf := array[1] of byte;
	for(;;){
		if(sys->read(sys->fildes(0), buf, 1) != 1)
			break;
		if('\r' == int buf[0] || '\n' == int buf[0])
			break;
		reply = reply + string buf;
	}
	return reply;
}

rf(file: string): string
{
	fd := sys->open(file, sys->OREAD);
	if(fd == nil)
		return "";

	buf := array[128] of byte;
	n := sys->read(fd, buf, len buf);
	if(n < 0)
		return "";

	return string buf[0:n];	
}
