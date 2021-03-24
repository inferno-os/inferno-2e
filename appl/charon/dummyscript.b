implement Script;

include "sys.m";
include "draw.m";
include "string.m";
include "url.m";
include "strinttab.m";
include "ctype.m";
include "event.m";
include "chutils.m";
include "lex.m";
include "script.m";
include "build.m";

CU: CharonUtils;
	ByteSource, CImage, ImageCache: import CU;

D: Draw;
	Point, Rect, Image: import D;

U: Url;
	ParsedUrl: import U;


init(nil: CharonUtils, nil: ref Build->Docinfo)
{
}

evalscript(nil: string): (string, string)
{
	return (nil, nil);
}

do_on(nil: ref Events->ScriptEvent)
{
}
