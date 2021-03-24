# This is somewhat like Plan9 du, except that
# the option -b is removed,
# the options -n and -t are added for netlib format.

implement du;

include "sys.m";
	sys: Sys;
	sprint: import sys;
include "draw.m";
include "string.m";
	strmod: String;
include "readdir.m";
	readdir: Readdir;
include "bufio.m";
	bufio: Bufio;
	Iobuf: import bufio;

aflag, nflag, sflag, tflag: int;
stderr: ref Sys->FD;
bout: ref Iobuf;

du: module{
	init:	fn(nil: ref Draw->Context, arg: list of string);
};

unsafe(filename: string): int{
	# check for shell specials, which would confuse later tools
	n := len filename;
	for(i:=0; i<n; i++){
		c := int filename[i];
		if(c<int ' ' || strmod->in(c,"\"'`$#;&|^<>()\\"))
			return(1);
	}
	return 0;
}

report(name: string, mtime, l, chksum: int){
	if(nflag==1){
		if(unsafe(name)){
			bout.puts(sprint("# unsafe filename %s\n",name));
		}else if(tflag==0)
			bout.puts(sprint("%s\n",name));
		else
			bout.puts(sprint("%s %d %d %d\n",name,mtime,l,chksum));
	}else if(aflag==1){
		bout.puts(sprint("%-4d %s\n",(l+1023)/1024,name));
	}
}


# Avoid loops in tangled namespaces.
NCACHE: con 2048; # must be power of two
cache := array[NCACHE] of list of ref sys->Dir;

seen(dir: ref sys->Dir): int{
	savlist := cache[dir.qid.path&(NCACHE-1)];
	for(c := savlist; c!=nil; c = tl c){
		sav := hd c;
		if(dir.qid.path==sav.qid.path &&
			dir.dtype==sav.dtype && dir.dev==sav.dev)
			return 1;
	}
	cache[dir.qid.path&(NCACHE-1)] = dir :: savlist;
	return 0;
}

dir(dirname: string): int{
	prefix := dirname+"/";
	if(dirname==".") prefix = nil;
	sum := 0;
	(de, nde) := readdir->init(dirname,readdir->NAME);
	for(i := 0; i < nde; i++) {
		s := de[i].name;
		if(de[i].mode&sys->CHDIR){
			if(!seen(de[i]))
				sum += dir(prefix+s);
		}else{
			l := de[i].length;
			sum += l;
			report(prefix+s,de[i].mtime,l,0);
		}
	}
	if(sflag==1)
		bout.puts(sprint("%-4d %s\n",(sum+1023)/1024,dirname));
	return sum;
}

init(nil: ref Draw->Context, argv: list of string){
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	bufio = load Bufio Bufio->PATH;
	strmod = load String String->PATH;
	readdir = load Readdir Readdir->PATH;
	if(strmod==nil || readdir==nil){
		sys->fprint(stderr, "du: load Error: %r\n");
		exit;
	}
	bout = bufio->fopen(sys->fildes(1),bufio->OWRITE);
	argv = tl argv;	# chop cmd
	aflag = nflag = tflag = 0;
	sflag = 1;
	for(; argv!=nil; argv = tl argv){
		a := hd argv;
                if(a[0]!='-')
                        break;
		for(i:=1; i<len a; i++){
			case(a[i]){
			'a' =>
				aflag = 1;
				sflag = 0;
				nflag = 0;
			'n' =>
				nflag = 1;
				aflag = 0;
				sflag = 0;
			's' =>
				sflag = 1;
				aflag = 0;
				nflag = 0;
			't' =>
				tflag = 1;
			* =>
				sys->fprint(stderr, "usage: du [-sant]\n");
				break;
			}
		}
	}

	if(argv==nil)
		argv = "." :: nil;
	for(; argv!=nil; argv = tl argv){
		a := hd argv;
		(rc,sbuf) := sys->stat(a);
		if(rc==-1){
			bout.puts(sprint("# can't stat %s: %r\n",a));
		}else if(sbuf.mode&Sys->CHDIR){
			dir(a);
		}else{
			if(nflag==0)
				bout.puts(sprint("%d\t%s\n",(sbuf.length+1023)/1024,a));
			else
				report(a,sbuf.mtime,sbuf.length,0);
		}
	}
	bout.close();
}
