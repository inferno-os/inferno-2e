# Copyright 1997, Lucent Technologies. All rights reserved.
#
# Thread Dispatcher and Monitor functions
#

Service: adt
{
	deaths: int;	# number of restarts recorded so far
	recover: int;	# -1 is ignore death, n means restart n times before alarm; don't use 0
	ptype: string;	# S for Spawn, M for monolith, A for announce only
	port: string;
	pid: int;
	net: string;
	cmd: list of string;
};

Tmonitor: module
{
	PATH: con "/dis/lib/tmonitor.dis";

	tmonitorInit: fn(chatty: int, srvFileName: string, logFd: ref FD): (list of ref Service, string);
};
