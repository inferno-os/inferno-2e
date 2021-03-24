implement httpd;

include "sys.m";
	sys: Sys;

Dir: import sys;
FD : import sys;

include "draw.m";

include "bufio.m";
	bufmod: Bufio;
Iobuf: 	import bufmod;

include "string.m";
	str: String;

include "readdir.m";
	rddir: Readdir;

include "daytime.m";
	daytime : Daytime;

include "content.m";
	cont: Cont;
Content: import cont;

include "cache.m";
	cache : Cache;

include "httpd.m";

include "parser.m";
	pars : Parser;

include "date.m";
	date: Date;

include "redirect.m";
	redir : Redirect;

include "cgi.m";

include "alarm.m";
	alm : Alm;
Alarm : import alm;

httpd: module
{
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};



# globals 

cache_size : int;
port := "80";
stderr : ref FD;
dbg_log,logfile : ref FD;
debug : int;
my_domain : string;

usage(){
	sys->fprint(stderr, "usage: httpd [-c num -D -p port#]\n");
	exit;
}

atexit(g : ref Private_info){
	debug_print(g,"At exit from httpd, closing fds. \n");
	g.bin.close();	
	g.bout.close();
	g.bin=nil;
	g.bout=nil;
	exit;
}

debug_print(g : ref Private_info,message : string) {
	if (g.dbg_log!=nil)
		sys->fprint(g.dbg_log,"%s",message);
}

parse_args(args : list of string){
	while(args!=nil){
		case (hd args){
			"-c" =>
				args = tl args;
				cache_size = int hd args;
			"-D" =>
				debug=1;
			"-p" =>
				args = tl args;
				port = hd args;
		}
		args = tl args;
	}
}

init(ctxt: ref Draw->Context, argv: list of string) {	
	if (ctxt==nil);
	# Load global modules.
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);

	bufmod = load Bufio Bufio->PATH;	
	if (bufmod==nil){
		sys->fprint(stderr,"bufmod load: %r\n");
		exit;
	}
	str = load String String->PATH;
	if (str == nil){
		sys->fprint(stderr,"string module load: %r\n");
		return;
	}
	date = load Date Date->PATH;
	if(date == nil){
		sys->fprint(stderr,"date module load: %r\n");
		return;
	}
	rddir = load Readdir Readdir->PATH;
	if(rddir == nil){
		sys->fprint(stderr,"Readdir module load: %r\n");
		return;
	}
	daytime = load Daytime Daytime->PATH;
	if(daytime == nil){
		sys->fprint(stderr,"Daytime module load: %r\n");
		return;
	}
	cont = load Cont Cont->PATH;
	if(cont == nil){
		sys->fprint(stderr,"cont module load: %r\n");
		return;
	}
	cache = load Cache Cache->PATH;
	if(cache == nil){
		sys->fprint(stderr,"cache module load: %r\n");
		return;
	}
	alm = load Alm Alm->PATH;
	if(alm == nil){
		sys->fprint(stderr,"httpd alm module load: %r\n");
		return;
	}
	redir = load Redirect Redirect->PATH;
	if(redir == nil){
		sys->fprint(stderr,"redir module load: %r\n");
		return;
	}
	pars = load Parser Parser->PATH;
	if(pars == nil){
		sys->fprint(stderr,"httpd pars module load: %r\n");
		return;
	}
	logfile=sys->create(HTTPLOG,Sys->ORDWR,8r666);
	if (logfile==nil){
		sys->print("logfile open: %r\n");
		exit;
	}
	
	# parse arguments to httpd.

	cache_size=5000;
	debug = 0;
	parse_args(argv);
	if (debug==1){
		dbg_log=sys->create(DEBUGLOG,Sys->ORDWR,8r666);
		if (dbg_log==nil){
			sys->print("debug log open: %r\n");
			exit;
		}
	}else 
		dbg_log=nil;
	sys->fprint(dbg_log,"started at %s \n",daytime->time());

	# initialisation routines
	cont->contentinit(dbg_log);
	cache->cache_init(dbg_log,cache_size);
	redir->redirect_init(REWRITE);
	date->init();
	pars->init();
	my_domain=sysname();
	(ok, c) := sys->announce("tcp!*!" + port);
	if(ok < 0) {
		sys->fprint(stderr, "can't announce %s: %r\n", port);
		exit;
	}
	sys->fprint(logfile,"************ Charon Awakened at %s\n",
			daytime->time());
	for(;;)
		doit(c);
	exit;
}


doit(c: Sys->Connection){
	(ok, nc) := sys->listen(c);
	if(ok < 0) {
		sys->fprint(stderr, "listen: %r\n");
		exit;
	}
	if (dbg_log!=nil)
		sys->fprint(dbg_log,"spawning connection.\n");
	spawn service_req(nc);
}

service_req(nc : Sys->Connection){
	buf := array[64] of byte;
	l := sys->open(nc.dir+"/remote", sys->OREAD);
	n := sys->read(l, buf, len buf);
	if(n >= 0)
		if (dbg_log!=nil)
			sys->fprint(dbg_log,"New client http: %s %s", nc.dir, 
							string buf[0:n]);
	#  wait for a call (or an error)
	#  start a process for the service
	g:= ref Private_info;
	g.bufmod = bufmod;
	g.dbg_log=dbg_log;
	g.logfile = logfile;
	g.modtime=0;
	g.entity = pars->initarray();
	g.mydomain = my_domain;
	g.version = "HTTP/1.0";
	g.cache = cache;
	g.okencode=nil;
	g.oktype=nil;
	g.getcerr="";
	g.parse_eof=0;
	g.eof=0;
	g.remotesys=getendpoints(nc.dir);
	debug_print(g,"opening in for "+string buf[0:n]+"\n");
	g.bin= bufmod->open(nc.dir+"/data",bufmod->OREAD);
	if (g.bin==nil){
		sys->print("bin open: %r\n");
		exit;
	}
	debug_print(g,"opening out for "+string buf[0:n]+"\n");
	g.bout= bufmod->open(nc.dir+"/data",bufmod->OWRITE);
	if (g.bout==nil){
		sys->print("bout open: %r\n");
		exit;
	}
	debug_print(g,"calling parsereq for "+string buf[0:n]+"\n");
	parsereq(g);
	atexit(g);
}

parsereq(g: ref Private_info) {
	meth, v,magic,search,uri,origuri,extra : string;
	# 15 minutes to get request line
	a := Alarm.alarm(15*1000*60);
	meth = getword(g);
	if(meth == nil){
		pars->logit(g,sys->sprint("no method%s", g.getcerr));
		a.stop();
		pars->fail(g,Syntax,"");
	}
	uri = getword(g);
	if(uri == nil || len uri == 0){
		pars->logit(g,sys->sprint("no uri: %s%s", meth, g.getcerr));
		a.stop();
		pars->fail(g,Syntax,"");
	}
	v = getword(g);
	extra = getword(g);
	a.stop();
	if(extra != nil){
			pars->logit(g,sys->sprint(
				"extra header word '%s'%s", 
					extra, g.getcerr));
			pars->fail(g,Syntax,"");
	}
	case v {
		"" =>
			if(meth!="GET"){
				pars->logit(g,sys->sprint("unimplemented method %s%s", meth, g.getcerr));
				pars->fail(g,Unimp, meth);
			}
	
		"HTTP/V1.0" or "HTTP/1.0" or "HTTP/1.1" =>
			if((meth != "GET")  && (meth!= "HEAD") && (meth!="POST")){
				pars->logit(g,sys->sprint("unimplemented method %s", meth));
				pars->fail(g,Unimp, meth);
			}	
		* =>
			pars->logit(g,sys->sprint("method %s uri %s%s", meth, uri, g.getcerr));
			pars->fail(g,UnkVers, v);
	}

	# the fragment is not supposed to be sent
	# strip it because some clients send it

	(uri,extra) = str->splitl(uri, "#");
	if(extra != nil)
		pars->logit(g,sys->sprint("fragment %s", extra));
	
	 # munge uri for search, protection, and magic	 
	(uri, search) = stripsearch(uri);
	uri = compact_path(pars->urlunesc(uri));
	if(uri == SVR_ROOT)
		pars->fail(g,NotFound, "no object specified");
	(uri, magic) = stripmagic(uri);
	debug_print(g,"stripmagic=("+uri+","+magic+")\n");

	 # normal case is just file transfer
	if(magic == nil || (magic == "httpd")){
		if (meth=="POST")
			pars->fail(g,Unimp,meth);	# /magic does handles POST
		g.host = g.mydomain;
		origuri = uri;
		pars->httpheaders(g,v);
		uri = redir->redirect(origuri);
		# must change this to implement proxies
		if(uri==nil){
			send(g,meth, v, origuri, search);
		}else{
			g.bout.puts(sys->sprint("%s 301 Moved Permanently\r\n", g.version));
			g.bout.puts(sys->sprint("Date: %s\r\n", daytime->time()));
			g.bout.puts("Server: Charon\r\n");
			g.bout.puts("MIME-version: 1.0\r\n");
			g.bout.puts("Content-type: text/html\r\n");
			g.bout.puts(sys->sprint("URI: <%s>\r\n",pars->urlconv(uri)));
			g.bout.puts(sys->sprint("Location: %s\r\n",pars->urlconv(uri)));
			g.bout.puts("\r\n");
			g.bout.puts("<head><title>Object Moved</title></head>\r\n");
			g.bout.puts("<body><h1>Object Moved</h1>\r\n");
			g.bout.puts(sys->sprint(
				"Your selection moved to <a href=\"%s\"> here</a>.<p></body>\r\n",
							 pars->urlconv(uri)));
			g.bout.flush();
		}
		atexit(g);
	}

	# for magic we init a new program
	args := list of {meth,v,uri,search};
	do_magic(g,magic,uri,origuri,args);
}

do_magic(g : ref Private_info,file,uri,origuri : string,args : list of string){
	buf:=sys->sprint("%s%s.dis",MAGICPATH, file);
	debug_print(g,"looking for "+buf+"\n");
	c:= load Cgi buf;
	if (c==nil){
		pars->logit(g,sys->sprint("no magic %s uri %s", file, uri));
		pars->fail(g,NotFound, origuri);
	}
	c->init(g,args);
}
	

send(g: ref Private_info,name, vers, uri, search : string) {
	typ,enc : ref Content;
	w : string;
	n, bad, force301: int;
	if(search!=nil)
		pars->fail(g,NoSearch, uri);

	# figure out the type of file and send headers
	debug_print( g, "httpd->send->open(" + uri + ")\n" );
	fd := sys->open(uri, sys->OREAD);
	if(fd == nil){
		dbm := sys->sprint( "open failed: %r\n" );
		debug_print( g, dbm );
		notfound(g,uri);
	}
	(i,dir):=sys->fstat(fd);
	if(i< 0)
		pars->fail(g,Internal,"");
	if(dir.mode & sys->CHDIR){
		(nil,p) := str->splitr(uri, "/");
		if(p == nil){
			w=sys->sprint("%sindex.html", uri);
			force301 = 0;
		}else{
			w=sys->sprint("%s/index.html", uri);
			force301 = 1; 
		}
		fd1 := sys->open(w, sys->OREAD);
		if(fd1 == nil){
			pars->logit(g,sys->sprint("%s directory %s", name, uri));
			if(g.modtime >= dir.mtime)
				pars->notmodified(g);
			senddir(g,vers, uri, fd, ref dir);
		} else if(force301 != 0 && vers != ""){
			g.bout.puts(sys->sprint("%s 301 Moved Permanently\r\n", g.version));
			g.bout.puts(sys->sprint("Date: %s\r\n", daytime->time()));
			g.bout.puts("Server: Charon\r\n");
			g.bout.puts("MIME-version: 1.0\r\n");
			g.bout.puts("Content-type: text/html\r\n");
			(nil, reluri) := str->splitstrr(pars->urlconv(w), SVR_ROOT);
			g.bout.puts(sys->sprint("URI: </%s>\r\n", reluri));
			g.bout.puts(sys->sprint("Location: http://%s/%s\r\n", 
				pars->urlconv(g.host), reluri));
			g.bout.puts("\r\n");
			g.bout.puts("<head><title>Object Moved</title></head>\r\n");
			g.bout.puts("<body><h1>Object Moved</h1>\r\n");
			g.bout.puts(sys->sprint(
				"Your selection moved to <a href=\"/%s\"> here</a>.<p></body>\r\n",
					reluri));
			atexit(g);
		}
		fd = fd1;
		uri = w;
		(i,dir)=sys->fstat(fd);
		if(i < 0)
			pars->fail(g,Internal,"");
	}
	pars->logit(g,sys->sprint("%s %s %d", name, uri, dir.length));
	if(g.modtime >= dir.mtime)
		pars->notmodified(g);
	n = -1;
	if(vers != ""){
		(typ, enc) = cont->uriclass(uri);
		if(typ == nil)
			typ = cont->mkcontent("application", "octet-stream");
		bad = 0;
		if(!cont->checkcontent(typ, g.oktype, "Content-Type")){
			bad = 1;
			g.bout.puts(sys->sprint("%s 406 None Acceptable\r\n", g.version));
			pars->logit(g,"no content-type ok");
		}else if(!cont->checkcontent(enc, g.okencode, "Content-Encoding")){
			bad = 1;
			g.bout.puts(sys->sprint("%s 406 None Acceptable\r\n", g.version));
			pars->logit(g,"no content-encoding ok");
		}else
			g.bout.puts(sys->sprint("%s 200 OK\r\n", g.version));
		g.bout.puts("Server: Charon\r\n");
		g.bout.puts(sys->sprint("Last-Modified: %s\r\n", date->dateconv(dir.mtime)));
		g.bout.puts(sys->sprint("Version: %uxv%ux\r\n", dir.qid.path, dir.qid.vers));
		g.bout.puts(sys->sprint("Message-Id: <%uxv%ux@%s>\r\n",
			dir.qid.path, dir.qid.vers, g.mydomain));
		g.bout.puts(sys->sprint("Content-Type: %s/%s", typ.generic, typ.specific));

#		if(typ.generic== "text")
#			g.bout.puts(";charset=unicode-1-1-utf-8");

		g.bout.puts("\r\n");
		if(enc != nil){
			g.bout.puts(sys->sprint("Content-Encoding: %s", enc.generic));
			g.bout.puts("\r\n");
		}
		g.bout.puts(sys->sprint("Content-Length: %d\r\n", dir.length));
		g.bout.puts(sys->sprint("Date: %s\r\n", daytime->time()));
		g.bout.puts("MIME-version: 1.0\r\n");
		g.bout.puts("\r\n");
		if(bad)
			atexit(g);
	}
	if(name == "HEAD")
		atexit(g);
	# send the file if it's a normal file
	g.bout.flush();
	# find if its in hash....
	# if so, retrieve, if not add..
	conts : array of byte;
	(i,conts) = cache->find(uri, dir.qid);
	if (i==0){
		# add to cache...
		conts = array[dir.length] of byte;
		sys->seek(fd,0,0);
		n = sys->read(fd, conts, len conts);
		cache->insert(uri,conts, len conts, dir.qid);
	}
	sys->write(g.bout.fd, conts, len conts);
}



# classify a file
classify(d: ref Dir): (ref Content, ref Content){
	typ, enc: ref Content;
	
	if(d.qid.path&sys->CHDIR)
		return (cont->mkcontent("directory", nil),nil);
	(typ, enc) = cont->uriclass(d.name);
	if(typ == nil)
		typ = cont->mkcontent("unknown ", nil);
	return (typ, enc);
}




# read in a directory, format it in html, and send it back
senddir(g : ref Private_info,vers,uri: string, fd: ref FD, mydir :ref Dir){
	d := array[100] of Dir;
	myname: string;
	n : int;
	myname = uri;
	if (myname[len myname-1]!='/')
		myname[len myname]='/';
	n = 0;
	for(;;){
		if(len d - n == 0){
			# resize dir array
			nd := array[2 * len d] of Dir;
			nd[0:] = d;
			d = nd;
		}
		nr := sys->dirread(fd, d[n:]);
		if(nr < 0){
			n=0;
			break;
		}
		if(nr == 0)
			break;
		n += nr;
	}
	fd=nil;
	# shell sort on name
	a := array[n] of ref Dir;
	for(i := 0; i < n; i++)
		a[i] = ref d[i];
	(a,i)=rddir->sortdir(a,rddir->NAME);
	if(vers != ""){
		pars->okheaders(g);
		g.bout.puts("Content-Type: text/html\r\n");
		g.bout.puts(sys->sprint("Date: %s\r\n", daytime->time()));
		g.bout.puts(sys->sprint("Last-Modified: %d\r\n", 
				mydir.mtime));
		g.bout.puts(sys->sprint("Message-Id: <%d%d@%s>\r\n",
			mydir.qid.path, mydir.qid.vers, g.mydomain));
		g.bout.puts(sys->sprint("Version: %d\r\n", mydir.qid.vers));
		g.bout.puts("\r\n");
	}
	g.bout.puts(sys->sprint("<head><title>Contents of directory %s.</title></head>\n",
		uri));
	g.bout.puts(sys->sprint("<body><h1>Contents of directory %s.</h1>\n",
		uri));
	g.bout.puts("<table>\n");
	for(i = 0; i < n; i++){
		(typ, enc) := classify(a[i]);
		g.bout.puts(sys->sprint("<tr><td><a href=\"%s%s\">%s</A></td>",
			myname, a[i].name, a[i].name));
		if(typ != nil){
			if(typ.generic!=nil)
				g.bout.puts(sys->sprint("<td>%s", typ.generic));
			if(typ.specific!=nil)
				g.bout.puts(sys->sprint("/%s", 
						typ.specific));
			typ=nil;
		}
		if(enc != nil){
			g.bout.puts(sys->sprint(" %s", enc.generic));
			enc=nil;
		}
		g.bout.puts("</td></tr>\n");
	}
	if(n == 0)
		g.bout.puts("<td>This directory is empty</td>\n");
	g.bout.puts("</table></body>\n");
	g.bout.flush();
	atexit(g);
}



stripmagic(uri : string): (string, string){
	prog,newuri : string;
	prefix := SVR_ROOT+"magic/";
	if (!str->prefix(prefix,uri) || len newuri == len prefix)
		return(uri,nil);
	uri=uri[len prefix:];
	(prog,newuri)=str->splitl(uri,"/");
	return (newuri,prog);
}


stripsearch(uri : string): (string,string){
	search : string;
	(uri,search) = str->splitl(uri, "?");
	if (search!=nil)
		search=search[1:];
	return (uri, search);
}

# get rid of "." and ".." path components; make absolute
compact_path(origpath:string): string {
	if(origpath == nil)
		origpath = "";
	(origpath,nil) = str->splitl(origpath, "`;| "); # remove specials
	(nil,olpath) := sys->tokenize(origpath, "/");
	rlpath : list of string;
	for(p := olpath; p != nil; p = tl p) {
		if(hd p == "..") {
			if(rlpath != nil)
				rlpath = tl rlpath;
		} else if(hd p != ".")
			rlpath = (hd p) :: rlpath;
	}
	cpath := "";
	if(rlpath!=nil){		
		cpath = hd rlpath;
		rlpath = tl rlpath;
		while( rlpath != nil ) {
			cpath = (hd rlpath) + "/" +  cpath;
			rlpath = tl rlpath;
		}
	}
	return SVR_ROOT + cpath;
}


getword(g : ref Private_info): string {
	buf : string;
	c  : int;

	while((c = getc(g)) == ' ' || c == '\t' || c == '\r')
		;
	if(c == '\n')
		return nil;
	for(;;){
		case c{
		' ' or '\t' or '\r' or '\n'=>
			return buf[0:len buf];
		}
		buf[len buf] = c;
		c = getc(g);
	}
	return nil;
}

 
getc(g : ref Private_info): int {
	# do we read buffered or unbuffered?
	# buf : array of byte;
	n : int;
	if(g.eof){
		debug_print(g,"eof is set in httpd\n");
		return '\n';
	}
	n = g.bin.getc();
	if (n<=0) { 
		if(n == 0)
			g.getcerr=": eof";
		else
			g.getcerr=sys->sprint(": n == -1: %r");
		g.eof = 1;
		return '\n';
	}
	n &= 16r7f;
	if(n == '\n')
		g.eof = 1;
	return n;
}


# couldn't open a file
# figure out why and return and error message
notfound(g : ref Private_info,url : string) {
	buf := sys->sprint("%r!");
	(nil,chk):=str->splitstrl(buf, "file does not exist");
	if (chk!=nil) 
		pars->fail(g,NotFound, url);
	(nil,chk)=str->splitstrl(buf,"permission denied");
	if(chk != nil)
		pars->fail(g,Unauth, url);
	pars->fail(g,NotFound, url);
}



sysname(): string{
	n : int;
	fd : ref FD;
	buf := array[128] of byte;
	
	fd = sys->open("#c/sysname", sys->OREAD);
	if(fd == nil)
		return "";
	n = sys->read(fd, buf , len buf);
	if(n <= 0)
		return "";
	
	return string buf[0:n];
}


sysdom(): string{
	dn : string;
	dn = csquery("sys" , sysname(), "dom");
	if(dn == nil)
		dn = "who cares";
	return dn; 
}


#  query the connection server
csquery(attr, val, rattr : string): string {
	token : string;
	buf := array[4096] of byte;
	fd : ref FD;
	n: int;
	if(val == "" ){
		return nil;
	}
	fd = sys->open("/net/cs", sys->ORDWR);
	if(fd == nil)
		return nil;
	sys->fprint(fd, "!%s=%s", attr, val);
	sys->seek(fd, 0, 0);
	token = sys->sprint("%s=", rattr);
	for(;;){
		n = sys->read(fd, buf, len buf);
		if(n <= 0)
			break;
		name:=string buf[0:n];
		(nil,p) := str->splitstrl(name, token);
		if(p != nil){	
			(p,nil) = str->splitl(p, " \n");
			if(p == nil)
				return nil;
			return p[4:];
		}
	}
	return nil;
}


getendpoint(dir,file : string ): (string,string) {
	n : int;
	fd : ref FD;
	fto,sysf,serv,tmp : string;
	buf := array[128] of byte;

	sysf = serv = nil;

	fto=sys->sprint("%s/%s", dir, file);
	fd = sys->open(fto, sys->OREAD);
	if(fd !=nil){
		n = sys->read(fd, buf, len buf);
		if(n>0){
			buf = buf[0:n-1];
			(tmp,serv) = str->splitl(string buf, "!");
			serv = serv[1:];
		}
		sysf=tmp;		
	}
	if(serv == nil)
		serv = "unknown";
	if(sysf == nil)
		sysf = "unknown";
	return (sysf,serv);
}

getendpoints(dir : string): string{
	lsys : string;
	lserv : string;
	rsys : string;
	rserv : string;
	(lsys, lserv) = getendpoint(dir, "local");
	(rsys, rserv) = getendpoint(dir, "remote");
	return rsys;
}
