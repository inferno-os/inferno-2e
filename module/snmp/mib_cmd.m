Mib_Cmd : module 
{
	init : fn(g : ref Snmpd->Global);
	do_req: fn(req :Snmpd->mibreq,mt:int) : (int,Snmpd->mibreq);
	timeout: fn(secs:int);
	list_mods: fn():string;
	NODATA,RANGE,NLIST: con iota;
	NOACCESS,READWRITE,READONLY,WRITEONLY,READCREATE: con iota;
};
