implement stats;

include "sys.m";
	sys : Sys;

include "bufio.m";
	bufmod: Bufio;
Iobuf: 	import bufmod;

include "draw.m";
	draw: Draw;

include "content.m";
Content: import Cont;

include "cache.m";
	cache : Cache;

include "httpd.m";
	g : ref Private_info;

include "date.m";
	date : Date;

include "parser.m";
	pars : Parser;

include "daytime.m";
	daytime: Daytime;


stats: module
{
	init: fn(g : ref Private_info, argv: list of string);
};


init(k : ref Private_info, argv: list of string) {	
	sys = load Sys "$Sys";
	draw = load Draw "$Draw";
	stderr := sys->fildes(2);	
	daytime = load Daytime Daytime->PATH;
	if(daytime == nil){
		sys->fprint(stderr,"Daytime module load: %r\n");
		return;
	}
	pars = load Parser Parser->PATH;
	if(pars == nil){
		sys->fprint(stderr,"echo pars module load: %r\n");
		return;
	}
	date = load Date Date->PATH;
	if(date == nil){
		sys->fprint(stderr,"date module load: %r\n");
		return;
	}
	date->init();
	meth := hd argv;
	vers := hd (tl argv);
	uri := hd(tl(tl argv));
	search := hd(tl(tl(tl argv)));
	g=k;
	bufmod=g.bufmod;
	send(meth, vers, uri, search);
	return;
}


send(meth, vers, uri, search : string) {
	if(meth=="");
	if(uri=="");
	if(search=="");
	if(vers != ""){
		if (g.version==nil)
			sys->print("version is unknown.\n");
		if (g.bout==nil)
			sys->print("AHHHHHHHHH! g.bout is nil.\n");
		g.bout.puts(sys->sprint("%s 200 OK\r\n", g.version));
		g.bout.puts("Server: Charon\r\n");
		g.bout.puts("MIME-version: 1.0\r\n");
		g.bout.puts(sys->sprint("Date: %s\r\n", date->dateconv(daytime->now())));
		g.bout.puts("Content-type: text/html\r\n");
		g.bout.puts(sys->sprint("Expires: %s\r\n", date->dateconv(daytime->now())));
		g.bout.puts("\r\n");
	}
	g.bout.puts("<head><title>Cache Information</title></head>\r\n");
	g.bout.puts("<body><h1>Cache Information</h1>\r\n");
	g.bout.puts("These are the pages stored in the server cache:<p>\r\n");
	lis:=(g.cache)->dump();
	while (lis!=nil){
		(a,b,d):=hd lis;
		g.bout.puts(sys->sprint("<a href=\"%s\"> %s</a> \t size %d \t tag %d.<p>\r\n",a,a,b,d));
		lis = tl lis;
	}
	g.bout.flush();
	return;
}
