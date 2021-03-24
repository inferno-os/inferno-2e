implement Ls;

include "sys.m";
	sys: Sys;
	FD, Dir: import Sys;

include "draw.m";
	Context: import Draw;

include "daytime.m";
	daytime: Daytime;

include "readdir.m";
	readdir: Readdir;

include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;

include "string.m";
	str: String;

Ls: module
{
	init:	fn(ctxt: ref Context, argv: list of string);
};

lopt:	int;
popt:	int;
qopt:	int;
dopt:	int;
now:	int;
sortby:	int;

out: ref Bufio->Iobuf;
stderr: ref FD;

dwIndex: int;
dwQueue: array of Dir;


init(nil: ref Context, argv: list of string)
{
	i: int;
	s: string;

	sys = load Sys Sys->PATH;

	stderr = sys->fildes(2);

	bufio = load Bufio Bufio->PATH;
	if(bufio == nil) {
		sys->fprint(stderr, "ls: load Bufio: %r\n");
		return;
	}

	out = bufio->fopen(sys->fildes(1), Bufio->OWRITE);

	readdir = load Readdir Readdir->PATH;
	if(readdir == nil) {
		sys->fprint(stderr, "ls: load Readdir: %r\n");
		return;
	}

	str = load String String->PATH;
	if(str == nil) {
		sys->fprint(stderr, "ls: load String: %r\n");
		return;
	}

	rev := 0;
	sortby = Readdir->NAME;

	if (argv !=nil)
		argv = tl argv;
	while(argv != nil) {
		s = hd argv;
		if(s[0] != '-')
			break;
		for(i = 1; i < len s; i++) case s[i] {
		'l' =>
			lopt++;
			daytime = load Daytime Daytime->PATH;
			if(daytime == nil) {
				sys->fprint(stderr, "ls: load Daytime: %r\n");
				return;
			}
			now = daytime->now();
		'p' =>
			popt++;
		'q' =>
			qopt++;
		'd' =>
			dopt++;
		't' =>
			sortby = Readdir->MTIME;
		'u' =>
			sortby = Readdir->ATIME;
		's' =>
			sortby = Readdir->SIZE;
		'r' =>
			rev = Readdir->DESCENDING;
		* =>
			sys->fprint(stderr, "Usage: ls [-dlpqrstu] [files]\n");
			return;
		}
		argv = tl argv;
	}
	sortby |= rev;

	if(argv == nil){
		argv = list of {"."};
		popt++;
	}

	while(argv != nil) {
		ls(hd argv);
		argv = tl argv;
	}
	delayWrite();
	out.flush();
}

ls(file: string)
{
	dir: Dir;
 	i, ok: int;

	(ok, dir) = sys->stat(file);
	if(ok == -1) {
		sys->fprint(stderr, "ls: stat %s: %r\n", file);
		return;
	}
	if(dopt || (dir.mode&sys->CHDIR) == 0) {
		# delay write: save it in the queue to sort by sortby
		if (dwIndex == 0) 
			dwQueue = array[30] of Dir;
		else if (len dwQueue == dwIndex) {
			# expand dwQueue
			tmp := array[2 * dwIndex] of Dir;
			tmp[0:] = dwQueue;
			dwQueue = tmp;
		}

		(dirname, nil) := str->splitstrr(file, "/");
		if(dirname != "")
			dir.name = dirname + dir.name;
		dwQueue[dwIndex++] = dir; 
		return;
	}

	delayWrite();

	dr := load Readdir Readdir->PATH;
	if(dr == nil) {
		sys->fprint(stderr, "ls: load Readdir: %r\n");
		return;
	}

	(d, n) := dr->init(file, sortby);
	if( n < 0)
		sys->fprint(stderr, "ls: Readdir: %s: %r\n", file);
	else
		for(i = 0; i < n; i++)
			lsprint(file, d[i].name, d[i]);		
}

delayWrite() 
{
	if (dwIndex == 0)
		return;
	
	# shell sort 
	a := array[dwIndex] of ref Dir;
	for (i := 0; i < dwIndex; i++)
		a[i] = ref dwQueue[i];
	(b, n) := readdir->sortdir(a, sortby);
	
	# print out
	for (j := 0; j < n; j++)
		lsprint("", b[j].name, b[j]);
	
	# reset dwIndex
	dwIndex = 0;
	dwQueue = nil;
}

lsprint(dirname, name: string, dir: ref Dir)
{
	if(qopt)
		out.puts(sys->sprint("%.8ux.%.8ux ", dir.qid.path, dir.qid.vers));

	file := name;
	if(popt)
		file = dir.name;
	else if(dirname != "") {
		if(dirname[len dirname-1] == '/')
			file = dirname + file;
		else
			file = dirname + "/" + file;
	}

	if(lopt)
		out.puts(sys->sprint("%s %c %4d %10s %10s %8d %s %s\n",
			modes(dir.mode), dir.dtype, dir.dev,
			dir.uid, dir.gid, dir.length,
			daytime->filet(now, dir.mtime), file));
	else
		out.puts(file+"\n");
}

mtab := array[] of {
	"---",	"--x",	"-w-",	"-wx",
	"r--",	"r-x",	"rw-",	"rwx"
};

modes(mode: int): string
{
	s: string;

	if(mode & Sys->CHDIR)
		s = "d";
	else
		s = "-";
	s += mtab[(mode>>6)&7]+mtab[(mode>>3)&7]+mtab[mode&7];
	return s;
}
