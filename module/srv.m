Srv: module
{
	BUILTINPATH:	con	"$Srv";
	PATH:		con	"/dis/lib/service.dis";

	reads:	fn(str: string, off, nbytes: int): (array of byte, string);
	#
	# IP network database lookups
	#
	#	iph2a:	host name to ip addrs
	#	ipa2h:	ip addr to host aliases
	#	ipn2p:	service name to port
	#
	iph2a:	fn(host: string): list of string;
	ipa2h:	fn(addr: string): list of string;
	ipn2p:	fn(net, service: string): string;
};
