implement WmAuth;

include "sys.m";

include "draw.m";

include "keyring.m";

include "security.m";

WmAuth: module
{
	init:	fn(ctxt: ref Draw->Context, args: list of string);
};

init(ctxt: ref Draw->Context, argv: list of string)
{
	keyfile: string;

	kr := load Keyring Keyring->PATH;
	login := load Login Login->PATH;

	keyname := "default";

	argv = tl argv;
	if(argv != nil){
		keyname = hd argv;
		argv = tl argv;
	}
	if(argv != nil){
		keyfile = hd argv;
		argv = tl argv;
	}

	login->getauthinfo(ctxt, keyname, keyfile);
}
