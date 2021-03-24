implement Mkrm;

include "sys.m";
	sys: Sys;
	Dir, sprint, fprint: import sys;

include "draw.m";

include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;

include "string.m";
	str: String;

include "arg.m";
	arg: Arg;

Mkrm: module
{
	init:	fn(nil: ref Draw->Context, nil: list of string);
};

LEN: con Sys->ATOMICIO;
NFLDS: con 6;		# filename, modes, uid, gid, mtime, bytes
MAXFILELIST: con 2048;

bin: ref Iobuf;
b2in: ref Iobuf;
uflag := 0;
hflag := 0;
vflag := 0;
stderr: ref Sys->FD;
filterprotofd: ref Sys->FD;
filterprotofd2: ref Sys->FD;
bout: ref Iobuf;
argv0 := "mkrm"; 

init(nil: ref Draw->Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	bufio = load Bufio Bufio->PATH;
	if(bufio == nil)
		error(sys->sprint("can't load %s: %r", Bufio->PATH));
	str = load String String->PATH;
	if(str == nil)
		error(sys->sprint("can't load %s: %r", String->PATH));
	arg = load Arg Arg->PATH;
	if(arg == nil)
		error(sys->sprint("can't load %s: %r", Arg->PATH));
	
  	index := 0;

	filelist := array[MAXFILELIST] of string;

	sys->pctl(Sys->NEWPGRP|Sys->FORKNS|Sys->FORKFD, nil);

	stderr = sys->fildes(2);

	destdir := "";
	filterproto := "";

	arg->init(args);
	while((c := arg->opt()) != 0)
		case c {
		'd' =>
			destdir = arg->arg();
			if(destdir == nil)
				error("destination directory name missing\n");
		'l' =>
			filterproto = arg->arg();
			if(filterproto == nil)
				error("filter file name missing\n");
		'v' =>
			vflag = 1;
		* =>
			usage();
		}
	args = arg->argv();


 	filterprotofd = sys->open(filterproto, Sys->OREAD);
	if(filterprotofd == nil) {
	  error(sys->sprint("could not open filter file %s: %r",filterproto));
	}

	bin = bufio->fopen(filterprotofd, Sys->OREAD);
	if(bin == nil)
		error(sys->sprint("can't access filter file: %r"));

	# First Pass
	while((p := bin.gets('\n')) != nil){

		if(p == "end of filter file\n"){
			break;
		}

		(nf, fields) := sys->tokenize(p, " \t\n");

		name := hd fields;
		if (name == "?") {
		    	fields = tl fields;
		    	name = hd fields;
			if (name == "d")
				name = hd (tl fields);
		    	if (vflag)
				sys->print(" Not removing %s \n", name);
		    	continue;
		}
		
		if ( name == "d" ) {  # It is a directory
			fields = tl fields;
			p = hd fields;
			filelist[index] = p;
			index += 1;
			continue;
		}

		name = destdir + name;
		if (vflag)
			sys->print("removing %s \n", name);

		if (sys->remove(name) == -1)
			sys->print("Could not remove %s\n",name);

	}
	bin.close();

#	sys->print("Second Pass\n");
	i := index;
	while(i >= 0) {
	   name := destdir + filelist[i];

	   if (vflag)
	     sys->print("Removing %s \n", name);

     	   if (sys->remove(name) == -1)
	     sys->print("Could not Remove %s\n", name); 
	   i--;
	}
	quit();
}


quit()
{
	exit;
}


error(s: string)
{
	fprint(stderr, "%s: %s\n", argv0, s);
	quit();
}

warn(s: string)
{
	fprint(stderr, "%s: %s\n", argv0, s);
}

usage()
{
	fprint(stderr, "usage: mkrm [-v] [-d destdir] [ -l filterfile]\n");
	exit;
}
