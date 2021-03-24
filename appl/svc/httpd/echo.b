implement echo;

include "sys.m";
sys : Sys;
stderr : ref Sys->FD;
include "bufio.m";
Iobuf : import Bufio;

include "draw.m";
draw : Draw;

include "content.m";
Content : import Cont;

include "cache.m";
	
include "httpd.m";

include "cgiparse.m";
cgiparse : CgiParse;

echo : module
{
    init : fn( g : ref Private_info, argv: list of string);
};


init( g : ref Private_info, argv: list of string ) 
{	
    sys = load Sys "$Sys";
    draw = load Draw "$Draw";
    stderr = sys->fildes(2);	
    cgiparse = load CgiParse CgiParse->PATH;
    if( cgiparse == nil ){
	sys->fprint( stderr, "Echo load CgiParse: %r\n" );
	return;
    }

    cgidata := cgiparse->cgipar( g, argv );

    send( g, cgidata );
    return;
}


send( g : ref Private_info, cgidata : ref CgiData ) 
{
    k := g.bufmod;
    Iobuf : import k;
    if( cgidata == nil ){
	g.bout.flush();
	return;
    }
    
    g.bout.puts( cgidata.httphd );
    
    g.bout.puts("<head><title>Echo</title></head>\r\n");
    g.bout.puts("<body><h1>Echo</h1>\r\n");
    g.bout.puts(sys->sprint("You requested a %s on %s", 
			    cgidata.method, cgidata.uri));
    if(cgidata.search!=nil)
	g.bout.puts(sys->sprint(" with search string %s", cgidata.search));
    g.bout.puts(".\n");

    g.bout.puts("Your client sent the following headers:<p><pre>");
    g.bout.puts( "Client: " + cgidata.remote + "\n" );
    g.bout.puts( "Date: " + cgidata.tmstamp + "\n" );
    g.bout.puts( "Version: " + cgidata.version + "\n" );
    while( cgidata.header != nil ){
	(tag, val) := hd cgidata.header;
	g.bout.puts( tag + " " + val + "\n" );
	cgidata.header = tl cgidata.header;
    }

    g.bout.puts("</pre>\n");	
    if (cgidata.method=="POST"){
	i := 0;
	g.bout.puts("</pre>");
	g.bout.puts("Your client sent the following form data:<p>");
	g.bout.puts("<table>\n");
	while(cgidata.form!=nil){
	    (tag, val) := hd cgidata.form;
	    g.bout.puts(sys->sprint("<tr><td>%d</td><td><I> ",i));
	    g.bout.puts(tag);
	    g.bout.puts("</I></td> ");
	    g.bout.puts("<td><B> ");
	    g.bout.puts(val);
	    g.bout.puts("</B></td></tr>\n");
	    g.bout.puts("\n");
	    cgidata.form = tl cgidata.form;
	    i++;
	}
	g.bout.puts("</table>\n");
    }	
    g.bout.puts("</body>\n");
    g.bout.flush();
    return;
}

