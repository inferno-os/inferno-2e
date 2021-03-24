implement Cp;

include "sys.m";
include "draw.m";
include "readdir.m";

FD, Dir: import Sys;
Context: import Draw;

Cp: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

sys: Sys;
stderr: ref FD;

readdir : Readdir;

recursopt : int;

DA : adt
{
	sname : string;
	dname : string;
	mode : int; 
};

init(nil: ref Context, argv: list of string)
{
	argc, todir, i: int;
	tmp: list of string;
	s: string;

	sys = load Sys Sys->PATH;

	stderr = sys->fildes(2);

	# Skip over the command name
	tmp = tl argv;

	recursopt = 0;
	while (tmp != nil) {
		s = hd tmp;
		if (s[0] != '-')
			break;

		for (i = 1; i < len s; i++) case s[i] {
		'r' =>
				recursopt = 1;
		* =>
			sys->fprint(stderr, "cp: invalid switch -%c\n", s[i]);
			cpusage();
			return;
		}

		tmp = tl tmp;
	}

	if ((argc = len tmp) < 2) {
		cpusage();
		return;
	}

	# Save the pointer to command line args
	argv = tmp;

	# Get the last arg
	while (tmp != nil) {
		s = hd tmp;
		tmp = tl tmp;
	}

	(ok, dir) := sys->stat(s);
	todir = (ok != -1 && (dir.mode & sys->CHDIR));
	if (argc > 2 && !todir) {
		sys->fprint(stderr, "cp: %s  not a directory\n", s);
		return;
	}
	if (!recursopt) {
		while ((argc -= 1) > 0) {
			cp(hd argv, s, todir);
			argv = tl argv;
		}
	} else {
		cpdir(argv, argc, s);
	}
	return;
}

basename(s : string): string
{
	ls : list of string;
	ok : int;

	(ok, ls) = sys->tokenize(s, "/");
	#if (ls == nil) {
	#	sys->fprint(stderr, "cp: tokenize returned nil\n");
	#	return (nil);
	#}
	while (ls != nil) {
		s = hd ls;
		ls = tl ls;
	}
	return s;
}

cp(src, dst: string, todir: int)
{
	ok: int;
	dirs, dird : Dir;

	if (todir) {
		#s : string;

		#if ((s = basename(src)) == nil) {
		#	return;
		#}
		s := basename(src);
		if (s == "/")
			dst = s;
		else
			dst = dst + "/" + basename(src);
	}
	(ok, dirs) = sys->stat(src);
	if (ok < 0) {
		sys->fprint(stderr, "cp: can't stat %s: %r\n", src);
		return;
	}
	if (dirs.mode & sys->CHDIR) {
		sys->fprint(stderr, "cp: %s is a directory\n", src);
		cpusage();
		return;
	}
	(ok, dird) = sys->stat(dst);
	if (ok != -1)
	if (dirs.qid.path==dird.qid.path && dirs.qid.vers==dird.qid.vers)
	if (dirs.dev==dird.dev && dirs.dtype==dird.dtype) {
		sys->fprint(stderr, "cp: %s and %s are the same file\n", 
					src, dst);
		return;
	}
	sfd := sys->open(src, sys->OREAD);
	if (sfd == nil) {
		sys->fprint(stderr, "cp: can't open %s: %r\n", src);
		return;
	}
	dfd := sys->create(dst, sys->OWRITE, dirs.mode);
	if (dfd == nil) {
		sys->fprint(stderr, "cp: can't open %s: %r\n", dst);
		return;
	}
	copy(sfd, dfd, src, dst);

	sfd = nil;
	dfd = nil;
}

mkdir(d : string, rwx : int): int
{

	ok : int;
	dird : Dir;
	dfd : ref FD;

	(ok, dird) = sys->stat(d);
	# File 'd' exits.
	if (ok != -1) {
		if (!(dird.mode & sys->CHDIR)) {
			sys->fprint(stderr, 
			"cp: %s: not a directory.\n", d);
			return -1;
		}
		return chmod(d, (dird.mode | rwx));
	} else {
		dfd = sys->create(d, sys->OREAD, sys->CHDIR + rwx);
		if (dfd == nil) {
			sys->fprint(stderr, "cp: can't create %s: %r\n", d);
			return -1;
		}
		dfd = nil;
	}
	return 1;
}

copy(sfd, dfd : ref FD, src, dst : string)
{
	buf := array[Sys->ATOMICIO] of byte;

	for (;;) {
		r := sys->read(sfd, buf, Sys->ATOMICIO);
		if (r < 0) {
			sys->fprint(stderr, "cp: error reading %s: %r\n", src);
			break;
		}
		if (r == 0)
			break;
		if (sys->write(dfd, buf, r) != r) {
			sys->fprint(stderr, "cp: error writing %s: %r\n", dst);
			break;
		}
	}
}

cpusage()
{
	sys->fprint(stderr, "usage:\tcp fromfile tofile\n");
	sys->fprint(stderr, "\tcp fromfile ... todir\n");
	sys->fprint(stderr, "\tcp -r fromdir ... todir\n");
}

cpdir(argv : list of string, argc : int, dst : string)
{
	src, tmp : string;
	ok, dexists : int;
	ds, dd : Dir;

	readdir = load Readdir Readdir->PATH;
	if (readdir == nil) {
		sys->fprint(stderr, "cp: load Readdir: %r\n");
		return;
	}
	dexists = 0;

	(ok, dd) = sys->stat(dst);
	 # Destination file exits
	if (ok != -1) {
		if (!(dd.mode & sys->CHDIR)) {
			sys->fprint(stderr, "cp: %s: not a directory.\n", dst);
			return;
		}
		dexists = 1;
		# Enable write
		if (chmod(dst, (dd.mode | 8r700)) == -1) {
			return;
		}
	}
	while ((argc -= 1) > 0) {
		src = hd argv;
		(ok, ds) = sys->stat(src);
		if (ok < 0) {
			sys->fprint(stderr, "cp: can't stat %s: %r\n", src);
			argv = tl argv;
			continue;
		}
		if (!(ds.mode & sys->CHDIR)) {
			sys->fprint(stderr, "cp: %s: not a directory.\n", src);
			argv = tl argv;
			continue;
		}
		if (dexists) {

			# Should we allow copying a directory onto itself?

			if (ds.qid.path==dd.qid.path && 
				ds.qid.vers==dd.qid.vers)
			if (ds.dev==dd.dev && ds.dtype==dd.dtype) {
				sys->fprint(stderr, "cp: %s and %s are the same directories\n", 
							src, dst);
				argv = tl argv;
				continue;
			}
			tmp = dst + "/" + basename(src);
			copydir(src, tmp, ds.mode);
		} else {
			copydir(src, dst, ds.mode);
		}
		argv = tl argv;
	}

	# Restore attributes
	if (dexists)
		chmod(dst, dd.mode);
}

getfilesindir(src, dst : string, lsrc : list of DA) : list of DA
{
	i : int;
	da : DA;

	(d, n) := readdir->init(src, Readdir->NAME);

	for (i=0; i<n; i++) {

		sf := src + "/" + d[i].name;
		da.sname = sf;
		da.mode = d[i].mode;
		if (d[i].mode & sys->CHDIR) {
			tmp := dst + "/" + d[i].name;
			da.dname = tmp;
			lsrc = da :: lsrc;
			lsrc = getfilesindir(sf, tmp, lsrc);
		} else {
			da.dname = dst;
			lsrc = da :: lsrc;
		}
	}
	return lsrc;
}

copydir(src, dst : string, mode : int)
{
	da : DA;

	(d, n) := readdir->init(src, Readdir->NAME);

	# First get a list of all files in src

	lsrc := getfilesindir(src, dst, nil);
	xx : list of DA;
	xx = nil;

	# Reverse the list
	while (lsrc != nil) {
		xx = hd lsrc :: xx;
		lsrc = tl lsrc;
	}
	lsrc = xx;

	if (mkdir(dst, 8r700) == -1) {
		return;
	}

	while (lsrc != nil) {
		da = hd lsrc;
		if (da.mode & sys->CHDIR) {
			mkdir(da.dname, 8r700);
		} else {
			cp(da.sname, da.dname, 1);
		}
		lsrc = tl lsrc;
	}
	chmod(dst, mode);
	while (xx !=nil) {
		da = hd xx;
		if (da.mode & sys->CHDIR)
			chmod(da.dname, da.mode);
		xx = tl xx;
	}
	lsrc = nil;
	xx = nil;
}

chmod(s: string, mode: int) : int
{
	(ok, d) := sys->stat(s);
	if (ok < 0) {
		return -1;
	}
	d.mode = mode;
	if (sys->wstat(s, d) < 0) {
		sys->fprint(stderr, "cp: can't wstat %s: %r\n", s);
		return -1;
	}
	return 1;
}
