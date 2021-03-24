implement Que;

include "sys.m";
	sys : Sys;

include "bufio.m";
Iobuf : import Bufio;

include "query.m";

include "content.m";

include "cache.m";

include "httpd.m";

include "parser.m";
	parse : Parser;

include "string.m";
	str : String;

init()
{
	sys = load Sys Sys->PATH;
	parse = load Parser Parser->PATH;
	if (parse==nil){
		sys->print("Parse load: %r\n");
		exit;
	}
	str = load String String->PATH;
	if (str==nil){
		sys->print("String load: %r\n");
		exit;
	}
}

# parse a search string of the form
# tag=val&tag1=val1...
 

parsequery(search : string): list of Query{
	q : list of Query;
	tag, val : string;
	if (str->in('?',search))
		(nil,search) = str->splitr(search,"?");
	q = nil;
	while(search!=nil){
		(tag,search) = str->splitl(search,"=");
		search=search[1:];
		(val,search) = str->splitl(search,"&");
		if (search!=nil)
			search=search[1:];
		q = mkquery(tag, val) :: q;
	}
	return q;
}


mkquery(tag,val : string): Query {
	q : Query;
	q.tag = parse->urlunesc(tag);
	q.val = parse->urlunesc(val);
	return q;
}

