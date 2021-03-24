Ipsrv: module
{
	PATH:		con	"/dis/lib/ipsrv.dis";

	#
	# Required init
	#
	init: fn(nil : ref Draw->Context, args : list of string);
	#
	# Optional args include:
	# module	-- optional first argument: pathname
	# -m "dnsport"	-- set mode to dial dns port (default does not)
	# -s srvfile	-- change the /services.txt default
	# -d dnsfile	-- change the /services/cs/db default
	# -i iterations	-- change the 10 default
	# This option enables cachine (not used by default):
	# -c cache_parameters -- parameters to Scache->setcache();
	# -c '0 0 0'	-- setcache using default parameters
	#

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
