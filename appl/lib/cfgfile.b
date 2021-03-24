implement CfgFile;

include "sys.m";
	sys: Sys;
	Dir: import sys;

include "draw.m";
include "sh.m";

include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;

include "cfgfile.m";

# Detect/Copy/Create
verify( default: string, path: string ): ref Sys->FD
{
	if (sys == nil)
		sys = load Sys Sys->PATH;

	fd := sys->open(path, Sys->ORDWR);
	if (fd == nil) {	# not there
		# first try a copy
		copy := load Command "/dis/cp.dis";
		copy->init( nil, ("cp"::(default::(path::nil))));
		# change permissions
		(ok, pathstat) := sys->stat(path);
		if ( ok != -1 ) {
			pathstat.mode = 8r666;
			ok = sys->wstat(path, pathstat);

			fd = sys->open(path, Sys->ORDWR);
		} 
	}
	if (fd == nil) {
		# then try a create
		fd = sys->create( path, Sys->ORDWR, 8r666 );
		if (fd == nil) {
			sys->raise("fail: Couldn't open, copy, or create");
			return nil;
		}
	}
	return fd;
}

init(file:string): ref ConfigFile
{
	sys = load Sys Sys->PATH;
	bufio = load Bufio Bufio->PATH;

	if(sys == nil || bufio == nil)
		return nil;

	me := ref ConfigFile;
	me.file = file;
	me.readonly = 0;
	f := bufio->open(file,Sys->ORDWR);

	if(f == nil) {
			f = bufio->open(file,Sys->OREAD);
			if (f == nil)
				return nil;
			else
				me.readonly = 1;
	}

	while((l := f.gett("\r\n")) != nil) {
		if(l[(len l)-1] == '\n')
			l = l[0:(len l)-1];
		me.lines = l :: me.lines;
	}

	return me;
}

ConfigFile.flush(me: self ref ConfigFile) : string
{
	if (me.readonly)
		sys->raise("fail: Config file is readonly");
	if((fd := sys->create(me.file,Sys->OWRITE,0644)) == nil)
		return sys->sprint("%r");
	if((f := bufio->fopen(fd,Sys->OWRITE)) == nil)
		return sys->sprint("%r");

	l := me.lines;
	while(l != nil) {
		f.puts(hd l+"\n");
		l = tl l;
	}
	if(f.flush() == -1) {
		return sys->sprint("%r");
	}

	return "";
}

ConfigFile.getcfg(me:self ref ConfigFile, field:string): list of string
{
	l := me.lines;

	while(l != nil) {
		(n,fields) := sys->tokenize(hd l," \t");
		if(n >= 1 && field == hd fields)
			return tl fields;
		l = tl l;
	}
	return nil;
}

ConfigFile.setcfg(me:self ref ConfigFile, field:string, val:string)
{
	l := me.lines;
	newlist: list of string;

	if (me.readonly)
		sys->raise("fail: Config file is readonly");

	matched := 0;

	while(l != nil) {
		(n,fields) := sys->tokenize(hd l," \t");
		if(!matched && n >= 1 && field == hd fields) {
			newlist = field+"\t"+val::newlist;
			matched = 1;
		}
		else
			newlist = hd l::newlist;
		l = tl l;
	}
	if(!matched) {
		newlist = field+"\t"+val::newlist;
	}

	me.lines = newlist;
}

ConfigFile.delete(me:self ref ConfigFile, field:string)
{
	l := me.lines;
	newlist: list of string;

	if (me.readonly)
		sys->raise("fail: Config file is readonly");

	matched := 0;

	while(l != nil) {
		(n,fields) := sys->tokenize(hd l," \t");
		if(!matched && n >= 1 && field == hd fields) {
			matched = 1;
		}
		else
			newlist = hd l::newlist;
		l = tl l;
	}

	me.lines = newlist;
}
