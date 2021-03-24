implement Workdir;

include "sys.m";

Workdir: module
{
	init:	fn(): string;
};

sys: Sys;

init(): string
{
	sys = load Sys Sys->PATH;

	c := chan of string;
	spawn getwd(c);
	return <-c;
}

getwd(c: chan of string)
{
	ok: int;
	d: sys->Dir;
	path: list of string;

	sys->pctl(sys->FORKNS, nil);

	qidp := -1;
	for(;;) {
		(ok, d) = sys->stat(".");
		if(ok < 0) {
			c <-= "";
			exit;
		}
		# are we at the root or are we crossing a ns boundary?
		if( (d.qid.path == qidp) && (d.name == hd path) )
			break;

		path = d.name :: path;
		if(sys->chdir("..") < 0) {
			c <-= "";
			exit;
		}
		qidp = d.qid.path;
	}
	s := "";
	path = tl path;
	while(path != nil) {
		s += "/"+hd path;
		path = tl path;
	}
	if(len s == 0) {
		s = "/";
	}
	else
	if(len s > 2 && s[1] == '#')
		s = s[1:];
	c <-= s;
}
