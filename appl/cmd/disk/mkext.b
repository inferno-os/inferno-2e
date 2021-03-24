implement Mkext;

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

Mkext: module
{
	init:	fn(nil: ref Draw->Context, nil: list of string);
};

LEN: con Sys->ATOMICIO;
NFLDS: con 6;		# filename, modes, uid, gid, mtime, bytes

bin: ref Iobuf;
uflag := 0;
hflag := 0;
vflag := 0;
fflag := 0;
stderr: ref Sys->FD;
filterprotofd: ref Sys->FD;
fbufout: ref Iobuf;
bout: ref Iobuf;
argv0 := "mkext";

init(nil: ref Draw->Context, args: list of string)
{
	sys = load Sys Sys->PATH;
	if(sys == nil)
		return;

	bufio = load Bufio Bufio->PATH;
	if(bufio == nil)
		sys->raise(sys->sprint("fail load %s: %r\n", Bufio->PATH));

	str = load String String->PATH;
	if(str == nil)
		sys->raise(sys->sprint("fail load %s: %r\n", String->PATH));

	arg = load Arg Arg->PATH;
	if(arg == nil)
		sys->raise(sys->sprint("fail load %s: %r\n", Arg->PATH));

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
		'f' =>
			fflag = 1;
		'l' =>
			filterproto = arg->arg();
                        filterprotofd = sys->open(filterproto, Sys->ORDWR);
                        if(filterprotofd == nil) {
                          filterprotofd = sys->create(filterproto,sys->ORDWR,8r777);
                          if (filterprotofd == nil)
                                error(sys->sprint("filter file does not exist and can not create\n"));
                        }
                        fbufout = bufio->fopen(filterprotofd, Sys->ORDWR);
                        if(fbufout == nil)
                          error(sys->sprint("cannot open the filter file"));

		'h' =>
			hflag = 1;
			bout = bufio->fopen(sys->fildes(1), Sys->OWRITE);
			if(bout == nil)
				error(sys->sprint("can't access standard output: %r"));
		'u' =>
			uflag = 1;
		'v' =>
			vflag = 1;
		* =>
			usage();
		}
	args = arg->argv();

	bin = bufio->fopen(sys->fildes(0), Sys->OREAD);
	if(bin == nil)
		error(sys->sprint("can't access standard input: %r"));
	while((p := bin.gets('\n')) != nil){
		if(p == "end of archive\n"){
			fprint(stderr, "done\n");
			quit();
		}
		(nf, fields) := sys->tokenize(p, " \t\n");
		if(nf != NFLDS){
			warn("too few fields in file header");
			continue;
		}
		name := hd fields;
		fields = tl fields;
		(mode, nil) := str->toint(hd fields, 8);
		fields = tl fields;
		uid := hd fields;
		fields = tl fields;
		gid := hd fields;
		fields = tl fields;
		(mtime, nil) := str->toint(hd fields, 10);
		fields = tl fields;
		(bytes, nil) := str->toint(hd fields, 10);
		if(args != nil){
			if(!selected(name, args)){
				if(bytes)
					seekpast(bytes);
				continue;
			}
			mkdirs(destdir, name);
		}
#		name = destdir+name;
		if(hflag){
			bout.puts(sys->sprint("%s %s %s %s %ud %d\n",
				name, octal(mode), uid, gid, mtime, bytes));
			if(bytes)
				seekpast(bytes);
			continue;
		}
		if(mode & Sys->CHDIR) {
                  if(fbufout != nil) {
                        if ( sys->open((destdir+name), sys->OREAD) == nil ) {
                                fbufout.puts(sys->sprint("d %s\n",name));
                        }
                        else {  # Directory exists
                                fbufout.puts(sys->sprint("? d %s\n",name));
                        }
                   }
		   mkdir((destdir+name), mode, mtime, uid, gid);
		}
		else {
                  if(fbufout != nil) {
			if(sys->open((destdir+name), sys->OREAD) == nil) {
                        	fbufout.puts(sys->sprint("%s\n",name));
			}
                        else {  # File exists
                                fbufout.puts(sys->sprint("? %s\n",name));
                        }
		  }
		  extract((destdir+name), mode, mtime, uid, gid, bytes);
		}
	}
	fprint(stderr, "premature end of archive\n");
	quit();
}

quit()
{
	if(bout != nil)
		bout.flush();

	if(fbufout != nil) {
                fbufout.puts("end of filter file\n");
                fbufout.flush();
        }

	exit;
}

fileprefix(prefix, s: string): int
{
	n := len prefix;
	m := len s;
	if(n > m || !str->prefix(prefix, s))
		return 0;
	if(m > n && s[n] != '/')
		return 0;
	return 1;
}

selected(s: string, args: list of string): int
{
	for(; args != nil; args = tl args)
		if(fileprefix(hd args, s))
			return 1;
	return 0;
}

mkdirs(basedir, name: string)
{
	(nil, names) := sys->tokenize(name, "/");
	while(names != nil) {
		sys->print("mkdir %s\n", basedir);

                # check if the directory exists
                if (fbufout != nil) {
                  if ( sys->open(basedir, sys->OREAD) == nil ) {
                        fbufout.puts(sys->sprint("d %s \n",basedir));
                  }
                  else {  # directory exists.
                        fbufout.puts(sys->sprint("? d %s\n",basedir));
                  }
                }
		sys->create(basedir, Sys->OREAD, 8r775|Sys->CHDIR);

		if(tl names == nil)
			break;
		basedir = basedir + "/" + hd names;
		names = tl names;
	}
}

mkdir(name: string, mode: int, mtime: int, uid: string, gid: string)
{
	d: Dir;
	i: int;

	fd := sys->create(name, Sys->OREAD, mode);
	if(fd == nil){
		(i, d) = sys->stat(name);
		if(i < 0 || !(d.mode & Sys->CHDIR)){
			warn(sys->sprint("can't make directory %s: %r", name));
			return;
		}
	}else{
		(i, d) = sys->fstat(fd);
		if(i < 0)
			warn(sys->sprint("can't stat %s: %r", name));
		fd = nil;
	}

	(nil, p) := str->splitr(name, "/");
	if(p == nil)
		p = name;
	d.name = p;
	if(uflag){
		d.uid = uid;
		d.gid = gid;
		d.mtime = mtime;
	}
	d.mode = mode;
	if(sys->wstat(name, d) < 0)
		warn(sys->sprint("can't set modes for %s: %r", name));
	if(uflag){
		(i, d) = sys->stat(name);
		if(i < 0)
			warn(sys->sprint("can't reread modes for %s: %r", name));
		if(d.mtime != mtime)
			warn(sys->sprint("%s: time mismatch %ud %ud\n", name, mtime, d.mtime));
		if(uid != d.uid)
			warn(sys->sprint("%s: uid mismatch %s %s", name, uid, d.uid));
		if(gid != d.gid)
			warn(sys->sprint("%s: gid mismatch %s %s", name, gid, d.gid));
	}
}

extract(name: string, mode: int, mtime: int, uid: string, gid: string, bytes: int)
{
	n: int;

	if(vflag)
		sys->print("x %s %d bytes\n", name, bytes);

	b: ref Iobuf;
	b = bufio->create(name, Sys->OWRITE, mode);
	if(b == nil) {
		if(!fflag || sys->remove(name) == -1 ||
			(b = bufio->create(name, Sys->OWRITE, mode)) == nil) {
			warn(sys->sprint("can't make file %s: %r", name));
			seekpast(bytes);
			return;
		}
	}
	buf := array [LEN] of byte;
	for(tot := 0; tot < bytes; tot += n){
		n = len buf;
		if(tot + n > bytes)
			n = bytes - tot;
		n = bin.read(buf, n);
		if(n <= 0)
			error(sys->sprint("premature eof reading %s", name));
		if(b.write(buf, n) != n)
			warn(sys->sprint("error writing %s: %r", name));
	}

	(i, d) := sys->fstat(b.fd);
	if(i < 0)
		warn(sys->sprint("can't stat %s: %r", name));
	(nil, p) := str->splitr(name, "/");
	if(p == nil)
		p = name;
	d.name = p;
	if(uflag){
		d.uid = uid;
		d.gid = gid;
		d.mtime = mtime;
	}
	d.mode = mode;
	if(b.flush() == Bufio->ERROR)
		warn(sys->sprint("error writing %s: %r", name));
	if(sys->fwstat(b.fd, d) < 0)
		warn(sys->sprint("can't set modes for %s: %r", name));
	if(uflag){
		(i, d) = sys->fstat(b.fd);
		if(i < 0)
			warn(sys->sprint("can't reread modes for %s: %r", name));
		if(d.mtime != mtime)
			warn(sys->sprint("%s: time mismatch %ud %ud\n", name, mtime, d.mtime));
		if(d.uid != uid)
			warn(sys->sprint("%s: uid mismatch %s %s", name, uid, d.uid));
		if(d.gid != gid)
			warn(sys->sprint("%s: gid mismatch %s %s", name, gid, d.gid));
	}
	b.close();
}

seekpast(bytes: int)
{
	n: int;

	buf := array [LEN] of byte;
	for(tot := 0; tot < bytes; tot += n){
		n = len buf;
		if(tot + n > bytes)
			n = bytes - tot;
		n = bin.read(buf, n);
		if(n <= 0)
			error("premature eof");
	}
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

octal(i: int): string
{
	s := "";
	do {
		t: string;
		t[0] = '0' + (i&7);
		s = t+s;
	} while((i = (i>>3)&~(7<<29)) != 0);
	return s;
}

usage()
{
	fprint(stderr, "usage: mkext [-h] [-u] [-v] [-f] [-l filterfile] [-d dest-fs] [file ...]\n");
	exit;
}
