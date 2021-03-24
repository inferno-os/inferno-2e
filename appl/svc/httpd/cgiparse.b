implement CgiParse;

include "sys.m";
include "draw.m";
include "string.m";
include "bufio.m";
Iobuf : import Bufio;
include "daytime.m";
include "parser.m";
include "query.m";
include "content.m";
include "cache.m";
include "httpd.m";
include "cgiparse.m";

sys : Sys;
draw : Draw;
strmod : String;
daytime : Daytime;
pars : Parser;
query : Que;


cont : Cont;
Content : import Cont;
Query : import query;

fprint, sprint, tokenize, FD : import sys;
tolower, toupper, splitstrr, splitstrl : import strmod;
stderr : ref FD;

cgipar( g : ref Private_info, argv : list of string ) : ref CgiData
{
    ok : int;
    err : string;
    ret : ref CgiData;
    (ok, err) = LoadModules();
    if( ok < 0 ){
	fprint( stderr, "CgiParse: %s\n", err );
	return nil;
    }

    (ok, err, ret) = DoParse( g, argv );
    
    if( ok < 0 ){
	fprint( stderr, "CgiParse: %s\n", err );
	return nil;
    }
    return ret;
}

LoadModules() : (int, string)
{
    if( sys == nil )
	sys = load Sys Sys->PATH;
    stderr = sys->fildes( 2 );
    if( draw == nil )
	draw = load Draw Draw->PATH;
    if( draw == nil )
	return (-1, sprint( "Load Draw: %r" ) );
    if( daytime == nil )
	daytime = load Daytime Daytime->PATH;
    if( daytime == nil )
	return (-1, sprint( "Load Daytime: %r" ) );
    if( strmod == nil )
	strmod = load String String->PATH;
    if( strmod == nil )
	return (-1, sprint( "Load String: %r" ) );
    if( pars == nil )
	pars = load Parser Parser->PATH;
    if( pars == nil )
	return (-1, sprint( "Load Parser: %r" ) );
    if( query == nil )
	query = load Que Que->PATH;
    if( query == nil )
	return (-1, sprint( "Load Que: %r" ) );
    return (0, nil);
}

DoParse( k : ref Private_info, argv : list of string ) : (int, string, ref CgiData)
{
    g := k;
    bufmod := g.bufmod;
    Iobuf : import bufmod;

    method : string;
    version : string;
    uri : string;
    search : string;
    tmstamp := daytime->time();
    host : string;
    remote : string;
    referer : string;
    httphd : string;
    header : list of (string, string);
    form : list of (string, string);

    if( argv != nil ){
	method = hd argv;
	argv = tl argv;
	if( argv != nil ){
	    version = hd argv;
	    argv = tl argv;
	    if( argv != nil ){
		uri = hd argv;
		argv = tl argv;
		if( argv != nil )
		    search = hd argv;
	    }
	}
    }
    
    c, eof, lastnl : int;

    if( version != "" ){
	if( g.version == nil )
	    return (-1, "version unknown.", nil);
	if( g.bout == nil )
	    return (-1, "internal error, g.bout is nil.", nil);
	if( g.bin == nil )
	    return (-1, "internal error, g.bin is nil.", nil);
	httphd = g.version + " 200 OK\r\n" +
	         "Server: Inferno-Httpd\r\n" +
		 "MIME-version: 1.0\r\n" +
		 "Date: " + tmstamp + "\r\n" +
		 "Content-type: text/html\r\n" +
		 "\r\n";
    }

    hstr := "";
    lastnl = 1;
    eof = 0;
    while( (c = g.bin.getc()) != bufmod->EOF ){
	if( c == '\r' ){
	    hstr[len hstr] = c;
	    c = g.bin.getb();
	    if( c == bufmod->EOF ){
		eof = 1;
		break;
	    }
	}
	hstr[len hstr] = c;
	if( c == '\n' ){
	    if( lastnl )
		break;
	    lastnl = 1;
	}
	else
	    lastnl = 0;
    }
    host = g.host;
    remote = g.remotesys;
    referer = g.referer;
    cnt : int;
    (cnt, header) = parseHeader( hstr );
    if( (method = toupper( method )) == "POST" ){
	str := "";
	while( ! eof && cnt && (c = g.bin.getc()) != '\n' ){
	    str[len str] = c;
	    cnt--;
	    if( c == '\r' )
		eof = 1;
	}
	query->init();
	q_list := query->parsequery( str );
	form = mkFormData( q_list );
    }
    return (0, nil, 
	    ref CgiData(method, version, uri, search, tmstamp, host, remote, referer,
			httphd, header, form));
}


parseHeader( hstr : string ) : (int, list of (string, string))
{
    header : list of (string, string);
    cnt := 0;
    if( hstr == nil || len hstr == 0 )
	return (0, nil);
    (n, sl) := tokenize( hstr, "\r\n" );
    if( n <= 0 )
	return (0, nil);
    while( sl != nil ){
	s := hd sl;
	sl = tl sl;
	for( i := 0; i < len s; i++ ){
	    if( s[i] == ':' ){
		tag := s[0:i+1];
		val := s[i+1:];
		if( val[len val - 1] == '\r' )
		    val[len val - 1] = ' ';
		if( val[len val - 1] == '\n' )
		    val[len val - 1] = ' ';
		header = (tag, val) :: header;
		if( tolower( tag ) == "content-length:" ){
		    if( val != nil && len val > 0 )
			cnt = int val;
		    else
			cnt = 0;
		}
		break;
	    }
	}
    }
    return (cnt, ListRev( header ));
}

mkFormData( q_list : list of Query ) : list of (string, string)
{
    form : list of (string, string);
    while( q_list != nil ){
	form = ((hd q_list).tag, (hd q_list).val) :: form;
	q_list = tl q_list;
    }
    return form;
}

ListRev( s : list of (string, string) ) : list of (string, string)
{
    tmp : list of (string, string);
    while( s != nil ){
	tmp = hd s :: tmp;
	s = tl s;
    }
    return tmp;
}


getBaseIp() : string
{
	buf : array of byte;
	fd := sys->open( "/net/bootp", Sys->OREAD );
	if( fd != nil ){
		(n, d) := sys->fstat( fd );
		if( n >= 0 ){
			if( d.length > 0 )
				buf = array [d.length] of byte;
			else
				buf = array [128] of byte;
			n = sys->read( fd, buf, len buf );
			if( n > 0 ){
				(nil, sl) := sys->tokenize( string buf[0:n], " \t\n" );
				while( sl != nil ){
					if( hd sl == "ipaddr" ){
						sl = tl sl;
						break;
					}
					sl = tl sl;
				}
				if( sl != nil )
					return "http://" + (hd sl);
			}
		}
	}
	return "http://beast2";
}

getBase() : string
{
	fd := sys->open( "/dev/sysname", Sys->OREAD );
	if( fd != nil ){
		buf := array [128] of byte;
		n := sys->read( fd, buf, len buf );
		if( n > 0 )
			return "http://" + string buf[0:n];
	}
	return "http://beast2";
}

getHost() : string
{
	fd := sys->open( "/dev/sysname", Sys->OREAD );
	if( fd != nil ){
		buf := array [128] of byte;
		n := sys->read( fd, buf, len buf );
		if( n > 0 )
			return string buf[0:n];
	}
	return "beast2";
}
