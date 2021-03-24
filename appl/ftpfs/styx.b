implement Styx;

#
#	Unpack Styx transaction messages read from FD.
#

include "sys.m";
include "draw.m";
include "styx.m";

sys:	Sys;

debug:	con 0;

badsize(want, found: int)
{
	reply->errorstatus = sys->sprint("bad length: mesg %d, expected %d found %d",
					int reply->buffer[0], want, found);
}

#
#	Extract integers, LSB first.
#

getb(x: int) : int
{
	return int reply->buffer[x];
}

get2(x: int) : int
{
	return (int reply->buffer[x]) | (int reply->buffer[x + 1] << 8);
}

get4(x: int) : int
{
	return (int reply->buffer[x]) | (int reply->buffer[x + 1] << 8) |
		(int reply->buffer[x + 2] << 16) | (int reply->buffer[x + 3] << 24);
}

get8(x: int) : big
{
	return (big reply->buffer[x]) | (big reply->buffer[x + 1] << 8) |
		(big reply->buffer[x + 2] << 16) | (big reply->buffer[x + 3] << 24) |
		(big reply->buffer[x + 4] << 32) | (big reply->buffer[x + 5] << 40) |
		(big reply->buffer[x + 6] << 48) | (big reply->buffer[x + 7] << 56);
}

#
#	Slice of buffer as byte array.
#

getn(x: int, n: int) : array of byte
{
	return reply->buffer[x:x+n];
}

#
#	Debug trace.
#

trace(b: byte)
{
	case int b {
	int Tnop =>
		sys->print("Tnop\n");
	int Tflush =>
		sys->print("Tflush\n");
	int Tclone =>
		sys->print("Tclone\n");
	int Twalk =>
		sys->print("Twalk\n");
	int Topen =>
		sys->print("Topen\n");
	int Tcreate =>
		sys->print("Tcreate\n");
	int Tread =>
		sys->print("Tread\n");
	int Twrite =>
		sys->print("Twrite\n");
	int Tclunk =>
		sys->print("Tclunk\n");
	int Tremove =>
		sys->print("Tremove\n");
	int Tstat =>
		sys->print("Tstat\n");
	int Twstat =>
		sys->print("Twstat\n");
	int Tattach =>
		sys->print("Tattach\n");
	}
}

#
#	Styx server.  Load reply module.  Initialize server.
#	Loop, reading and cracking transactions.
#

serve(a: string, s: StyxServer) : string
{
	if (sys == nil)
		sys = load Sys Sys->PATH;

	reply = load StyxReply StyxReply->PATH;

	if (reply == nil)
		return sys->sprint("could not load %s: %r", StyxReply->PATH);

	server = s;
	m := server->init(a, reply);

	if (m != nil)
		return m;

	if (reply->buffer == nil)
		reply->buffer = array[MAXMSG] of byte;

	reply->errorstatus = nil;

	do {
		n := sys->read(reply->fd, reply->buffer, MAXMSG);

		if (n == 0)
			return nil;

		if (n < 0)
			return sys->sprint("read error: %r");

		if (debug)
			trace(reply->buffer[0]);

		case int reply->buffer[0] {
		int Tnop =>
			if (n != 3) {
				badsize(3, n);
				break;
			}
			server->nopT(get2(1));
		int Tflush =>
			if (n != 5) {
				badsize(5, n);
				break;
			}
			server->flushT(get2(1), get2(3));
		int Tclone =>
			if (n != 7) {
				badsize(7, n);
				break;
			}
			server->cloneT(get2(1), get2(3), get2(5));
		int Twalk =>
			if (n != 5 + Sys->NAMELEN) {
				badsize(5 + Sys->NAMELEN, n);
				break;
			}
			server->walkT(get2(1), get2(3), getn(5, Sys->NAMELEN));
		int Topen =>
			if (n != 6) {
				badsize(6, n);
				break;
			}
			server->openT(get2(1), get2(3), getb(5));
		int Tcreate =>
			if (n != 10 + Sys->NAMELEN) {
				badsize(10 + Sys->NAMELEN, n);
				break;
			}
			server->createT(get2(1), get2(3), getn(5, Sys->NAMELEN),
					get4(5 + Sys->NAMELEN), getb(9 + Sys->NAMELEN));
		int Tread =>
			if (n != 15) {
				badsize(15, n);
				break;
			}
			server->readT(get2(1), get2(3), get8(5), get2(13));
		int Twrite =>
			if (n < 16) {
				reply->errorstatus = sys->sprint("short Twrite: %d", n);
				break;
			}
			c := get2(13);
			if (c < 0 || c > Sys->ATOMICIO) {
				reply->errorstatus = sys->sprint("bad write count: %d", c);
				break;
			}
			if (n != 16 + c) {
				badsize(16 + c, n);
				break;
			}
			server->writeT(get2(1), get2(3), get8(5), c, getn(16, c));
		int Tclunk =>
			if (n != 5) {
				badsize(5, n);
				break;
			}
			server->clunkT(get2(1), get2(3));
		int Tremove =>
			if (n != 5) {
				badsize(5, n);
				break;
			}
			server->removeT(get2(1), get2(3));
		int Tstat =>
			if (n != 5) {
				badsize(5, n);
				break;
			}
			server->statT(get2(1), get2(3));
		int Twstat =>
			if (n != 5 + STATSZ) {
				badsize(5 + STATSZ, n);
				break;
			}
			server->wstatT(get2(1), get2(3), getn(5, STATSZ));
		int Tattach =>
			if (n != 5 + 2 * Sys->NAMELEN) {
				badsize(5 + 2 * Sys->NAMELEN, n);
				break;
			}
			server->attachT(get2(1), get2(3),
					getn(5, Sys->NAMELEN), getn(5 + Sys->NAMELEN, Sys->NAMELEN));
		* =>
			return sys->sprint("bad type: %d", int reply->buffer[0]);
		}
	}
	while (reply->errorstatus == nil);

	return reply->errorstatus;
}
