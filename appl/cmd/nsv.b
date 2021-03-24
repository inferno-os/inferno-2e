implement Nsview;

# read /prog/<pid>/ns or #p/<pid>/ns
# to view namespace
#
# version 1.0 April 5, 1999
# E. V. Bacher Lucent/Inferno

include "sys.m";
	sys:	Sys;
	stderr,
	stdout:	ref sys->FD;
	print,
	fprint,
	sprint: import sys;

include "draw.m";

include "bufio.m";
	bufio:	Bufio;
	Iobuf: import bufio;
	inbuf, outbuf: ref Iobuf;

include "string.m";
	str: String;

##
# ADT definitions

member: adt {
	name: string;
	create: int;
};

uniondir: adt {
	name: string;
	contents: list of member;

	uprint: fn(this: self uniondir);
	ulist: fn(this: self uniondir);
};

# print a union directory
uniondir.uprint(this: self uniondir)
{
	cflag: string;
	ulength := len this.name;
		
	cflag = "";
	outbuf.puts(sprint("%s", this.name));

	# print union name and top element
	pad := "";
	cflag = "";
	for (i :=0; i < (len _spacer - len this.name); i++)
		pad += " ";
	first := hd this.contents;
	if (first.create)
		cflag = _mcreate;
	outbuf.puts(sprint("%s\t%s %s\n", pad, first.name, cflag) );

	for (members := tl this.contents; members != nil; members = tl members) {
		mem := hd members;
		cflag = "";
		if (mem.create)
			cflag = _mcreate;
		outbuf.puts(sprint("%s\t%s %s\n", _spacer, mem.name, cflag));
	}
	outbuf.puts(sprint("\n"));
}

# print union directory for machine consumption
uniondir.ulist(this: self uniondir)
{
	cflag: string;
	ulength := len this.name;
		
	outbuf.puts(sprint("%s   ", this.name));

	for (members := this.contents;  members != nil; members = tl members) {
		mem := hd members;
		cflag = "0";
		if (mem.create)
			cflag = "4";
		outbuf.puts(sprint("%s %s   ", mem.name, cflag));
	}
	outbuf.puts(sprint("\n"));
}


##
# global variables

_debug: int;
_spacer: string;		# spacer for printing
_display: int;			# print for display or machine-readable?

_ns: list of uniondir;
_progname: string;
_found: int;
_gotit: int;

##
# global constants:
_mcreate: con "  MCREATE";	# label for MCREATE flag

# bind, mount flag constants

REPLACE: con 0;
BEFORE,
AFTER,
CREATE:	con 1<<iota;


Nsview: module  
{
	init:   fn(ctxt: ref Draw->Context, argv: list of string);
};

usage()
{
	fprint(stderr, "usage: %s [-m] [-?] [pid]\n", _progname);
}

##
# init

init(nil: ref Draw->Context, argv: list of string)
{
	sys  = load Sys Sys->PATH;
	stdout = sys->fildes(1);
    	stderr = sys->fildes(2);

# initialize these globals for each call of init()
    	_ns = nil;
	_debug = 0;
	_spacer = "";			# spacer for printing
	_display = 1;			# print for display or machine-readable?
	_found = 0;
	_gotit = 0;

    	str = load String String->PATH;
	if (str == nil) {
		sys->fprint(stderr, "%s: couldn't load String: %r\n", _progname);
		return;
	}

	bufio = load Bufio Bufio->PATH;

	(_progname, argv) = (hd argv, tl argv);

# process options (if any)
	if (argv != nil) {
		for (; argv != nil; argv = tl argv) {
			if ((hd argv)[0] == '-') {
				case hd argv {
					"-d" =>
						_debug = 1;
					"-m" =>
						_display = 0;
					"-?" or "-h" =>
						usage();
						return;
					* =>
						usage();
						return;
				}
			} else
				break;
		}
	}

# get pid
	pid: int;
	if (argv == nil) {
		pid = sys->pctl(0, nil);		
	} else {
		(pid, nil) = str->toint(hd argv, 10);
	}
	

# open /prog/pid/ns file (or #p/prog/ns) for reading
	nsfd := bufio->open(sys->sprint("prog/%d/ns", pid), bufio->OREAD);
	if (nsfd == nil) {
		nsfd = bufio->open(sys->sprint("#p/%d/ns", pid), bufio->OREAD);
		if (nsfd == nil) {
			fprint(stderr, "%s: unable to open ns for pid %d: %r\n", _progname, pid);
			return;
		}
	}

	outbuf = bufio->fopen(stdout, bufio->OWRITE);
	if (outbuf == nil) {
		fprint(stderr, "%s: could not open stdout: %r\n", _progname);
	}

	if (_display)
		outbuf.puts(sprint("Namespace for pid %d:\n\n", pid));

# read and parse lines of ns file
	for (line := nsfd.gets('\n'); line != nil; line = nsfd.gets('\n') ) {
		(ok, toks) := sys->tokenize(line, " \t");
		(flags, target, source) := (int hd toks, hd tl toks, hd tl tl toks);

		source = cleansource(source);
		target = cleantarget(target);
				
		item := member(source, flags & CREATE);

# build union lists
		new: list of uniondir = nil;
		for (all := _ns; all != nil; all = tl all) {
			current := hd all;
			if (current.name == target) {
				_found = 1;
				_gotit = 1;
				dbprint(1, "target found", target);
				if (flags & REPLACE)
					current.contents = list of {item};
				else if (flags & BEFORE)
					current.contents = item :: current.contents;
				else if (flags & AFTER)
					current.contents = addmember(current.contents, item);
			} else {
				_found = 0;
			}
				
			new = current :: new;
		}

		# reconstruct namespace
		_ns = nil;
		for (; new != nil; new = tl new)
			_ns = hd new :: _ns;


# if target not found and dealt with, create new union directory
		if (!_gotit) {
			dbprint(1, "target not found", target);
			newunion := uniondir(target, list of {item});
			_ns = addunion(_ns, newunion);
			
			extra := len target - len _spacer;
			if (extra > 0)
				for (i := 0; i < extra; i++)
					_spacer += " ";
		}

		if (_debug) {
			dbprint(1, "target", target);
			nsprint(_ns);
		}
		_gotit = 0;	# reset before next line of ns file
	}

	nsprint(_ns);
}


##
# other functions  

cleansource(source: string): string
{
	# hack to remove '\n' from end -  could write a chomp() function
	# but we should be OK here since we're not dealing with MS or Mac files
	source = source[:len source - 1];

	# remove unnecessary #/'s and #U's
	#if ((source[0:2] == "#/") || (source[0:2] == "#U"))
	#	if ((source != "#/") && (source != "#U"))
	#		source = source[2:];
	
	return source;
}

cleantarget(target: string): string
{
	if (target[0:2] != "#M") {
		target = target[2:];
		if (target == "")
			target = "/";
	}
	return target;
}

addunion(ulist: list of uniondir, u: uniondir): list of uniondir
{
	revlist: list of uniondir = nil;
	for (; ulist != nil; ulist = tl ulist)
		revlist = hd ulist :: revlist;
	revlist = u :: revlist;

	ulist = nil;	# should be unnecessary -- this is defensive
	for (; revlist != nil; revlist = tl revlist)
		ulist = hd revlist :: ulist;
	return ulist;
}

addmember(contents: list of member, newmember: member): list of member
{
	revlist: list of member = nil;
	for (; contents != nil; contents = tl contents)
		revlist = hd contents :: revlist;
	revlist = newmember :: revlist;

	contents = nil;
	for (; revlist != nil; revlist = tl revlist)
		contents = hd revlist :: contents;
	return contents;
}


# debug printer
dbprint(fd: int, message, variable: string)
{
	if (_debug) {
		if (message == "\n") {
			fprint(sys->fildes(fd), "\n");
			return;
		}
		fprint(sys->fildes(fd), "%s = %s\n", message, variable);
	}
}

# print a namespace
nsprint(ns: list of uniondir)
{
	
	if (ns == nil) print("ns is nil!\n");
	for(; ns != nil; ns=tl ns) {
		u := hd ns;
		if (_display)
			u.uprint();
		else
			u.ulist();
	}
	outbuf.flush();
}


