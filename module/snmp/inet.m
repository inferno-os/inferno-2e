Inet : module 
{
	PATH:		con "/dis/snmp/inet.dis";

	init: fn(g : ref Snmpd->Global) : int;
	parsereq: fn(mt : int,mlist:list of Snmpd->mibreq) : (int,int,list of Snmpd->mibreq);
	timeout: fn(secs:int);
	list_mods: fn();
};
