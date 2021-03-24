implement P;
# Original by Steve Arons, based on Plan 9 p

include "sys.m"; 
	sys: Sys;
	FD:	import Sys;
include "draw.m";
include "string.m";
	str: String;
include "bufio.m";
	bufmod: Bufio;
	Iobuf: import bufmod;
include "sh.m";


stdin, stderr, stdout, waitfd: ref FD;
outb, cons: ref Iobuf;

nlines := 22;
progname: string;

P: module
{
	init:  fn(ctxt:  ref Draw->Context, argv:  list of string);
};


init(nil: ref Draw->Context, argv:  list of string)
{
	sys     = load Sys Sys->PATH;
	bufmod  = load Bufio Bufio->PATH;
	str     = load String String->PATH;

	stdout = sys->fildes(1);
	stderr = sys->fildes(2);
	stdin  = sys->fildes(0);

	progname = str->take(hd argv, "^.");        # strip off ".dis"
	argv = tl argv;
	if(argv != nil){
		s := hd argv;
		if(s[0] == '-' && len s >= 2){          # not "-" for stdin
			(x, y) := str->toint(s[1:],10);
			if(y == "" && x > 0)
				nlines = x;
			else
				usage();
			argv = tl argv;
		}
	}
	if(argv == nil)
		argv = "stdin" :: nil;
	# do these once
	outb = bufmod->fopen(stdout, bufmod->OWRITE);
	if(outb == nil){
		sys->fprint(stderr, "p: can't open stdout, %r\n");
		return;
	}
	cons = bufmod->open("/dev/cons", bufmod->OREAD);
	if(cons == nil){
		sys->fprint(stderr, "p: can't open /dev/cons, %r\n");
		return;
	}
	
	while(argv != nil){
		file := hd argv;
		if(file == "-")
			file = "stdin";
		page(file);
		argv = tl argv;
		if(argv != nil)
			pause();
	}
}

pause()
{
	for(;;){
		cmdline := cons.gets('\n');
		if(cmdline == nil || cmdline[0] == 'q') # catch ^d
			exit;
		else if(cmdline[0] == '!') 
			command(cmdline[1:]);
		else
			break;
	}
}

usage()
{
	sys->fprint(stderr, "Usage: p [-number] [file...]\n");
	exit;
}

page(file : string)
{
	inb: ref Iobuf;
	nl: int;

	if(file == "stdin") 
		inb = bufmod->fopen(stdin, bufmod->OREAD);
	else
		inb = bufmod->open(file, bufmod->OREAD);
	if(inb == nil){
		sys->fprint(stderr,"%s can't open %s, %r\n", progname, file);
		return;
	}
	nl = nlines;
	while((line := inb.gets('\n')) != nil){
		outb.puts(line);        
		nl--;
		if(nl == 0){
			outb.flush();
			nl = nlines;
			pause();
		}
	}
	outb.flush();   
}

command(cmdline: string)
{
	(argc, arglist) := sys->tokenize(cmdline, " \t\n");
	if(argc == 0)
		return;
	cname := hd arglist;

	# --------------------------------------------------------
	# Taken, "with the lofty joy thy speech infuses into me"
	# (Par. Can. 8, Longfellow), from time.b
	#---------------------------------------------------------
	
	if(len cname < 4 || cname[len cname-4:] != ".dis")
		cname += ".dis";
	arglist = tl arglist;

	err := "";
	c := load Command cname;
	if(c == nil){
		err = sys->sprint("%r");
		if(err == "file does not exist"){
			c = load Command "/dis/" + cname;
			if (c == nil)
				err = sys->sprint("%r");
		}
	}
	if(c == nil){
		sys->fprint(stderr, "%s: %s - %s\n", progname, cname, err);
		return;
	}
	
	waitfd = sys->open("#p/"+string sys->pctl(0, nil)+"/wait", sys->OREAD);
	if(waitfd == nil){
		sys->fprint(stderr, "time: open wait: %r\n");
		return;
	}
	
	pidc := chan of int;
	spawn realcommand(c, pidc, cname :: arglist);   
	waitfor(<-pidc);
}
	
realcommand(c: Command, pidc: chan of int, arglist: list of string)
{
	pidc <-= sys->pctl(sys->FORKNS, nil);
	c->init(nil, arglist);
}

# more lofty joy
waitfor(pid: int)
{
	buf := array[sys->WAITLEN] of byte;
	status := "";
	for(;;){
		n := sys->read(waitfd, buf, len buf);
		if(n < 0){
			sys->fprint(stderr, "sh: read wait: %r\n");
			return;
		}
		status = string buf[0:n];
		if(status[len status-1] != ':')
			sys->fprint(stderr, "%s\n", status);
		who := int status;
		if(who != 0) {
			if(who == pid)
				return;
		}
	}
}
