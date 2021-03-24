implement SSL;

include "sys.m";
include "keyring.m";
include "draw.m";
include "security.m";

sys: Sys;

connect(fd: ref Sys->FD): (string, ref Sys->Connection)
{
	c := ref Sys->Connection;

	sys = load Sys Sys->PATH;

	c.dir = "/n/ssl";
	c.cfd = sys->open(c.dir + "/clone", Sys->ORDWR);
	if(c.cfd == nil)
		return (sys->sprint("Cannot open clone: %r"), nil);

	buf := array[128] of byte;
	if((n := sys->read(c.cfd, buf, len buf)) < 0)
		return (sys->sprint("Cannot read ctl: %r"), nil);

	c.dir += "/" + string buf[0:n];
	
	c.dfd = sys->open(c.dir + "/data", Sys->ORDWR);
	if(c.dfd == nil)
		return (sys->sprint("Cannot open data: %r"), nil);
	
	c.cfd = sys->open(c.dir + "/ctl", Sys->ORDWR);
	if(c.cfd == nil)
		return (sys->sprint("Cannot open ctl: %r"), nil);

	if(sys->fprint(c.cfd, "fd %d", fd.fd) < 0)
		return (sys->sprint("Cannot push fd: %r"), nil);

	return (nil, c);
}

secret(c: ref Sys->Connection, secretin, secretout: array of byte): string
{
	fd: ref Sys->FD;

	sys = load Sys Sys->PATH;

	if(secretin != nil){
		fd = sys->open(c.dir + "/secretin", Sys->ORDWR);
		if(fd == nil)
			return sys->sprint("Cannot open %s: %r", c.dir + "/secretin");
		if(sys->write(fd, secretin, len secretin) < 0)
			return sys->sprint("Cannot write %s: %r", c.dir + "/secretin");
	}

	if(secretout != nil){
		fd = sys->open(c.dir + "/secretout", Sys->ORDWR);
		if(fd == nil)
			return sys->sprint("Cannot open %s: %r", c.dir + "/secretout");
		if(sys->write(fd, secretout, len secretout) < 0)
			return sys->sprint("Cannot open %s: %r", c.dir + "/secretout");
	}
	return nil;
}
