Snmp_Init : module 
{
	PATH:		 con "/dis/snmp/snmp_init.dis";
	init: fn(og:ref Snmpd->Global, argv:list of string) : int;
};
