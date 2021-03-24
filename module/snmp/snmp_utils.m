Snmp_Utils: module
{
	PATH:	con "/dis/snmp/snmp_utils.dis";

	modinfo: adt {
		index : array of int;
		name  : string;
		fname : string;
		dir   : string;
		table : int;
		mp    : Mib_Cmd;
		lastaccess : int;
	};

	leafinfo: adt {
		typ : int;
		access : int;
		constraint : int;
		data : array of string;
	};

	leafdata: adt {
		Int : int;
		Octets : array of byte;
		ObjId : array of int;
	};

	init: fn(og : ref Snmpd->Global);
	parse: fn(packet : array of byte) : (list of Snmpd->mibreq,int,int,string);
	pack: fn(mlist : list of Snmpd->mibreq,hdr: array of byte,comm:string,rqid,errflg,erridx: int) : array of byte;
	trap: fn(mlist : list of Snmpd->mibreq,trapnum : int,spec_code: int,ts : int);
	int2octets: fn(val : int) : array of byte;
	octets2int: fn(val : array of byte) : int;
	string2oid: fn(val : string) : array of int;
	oid2string: fn(val : array of int) : string;
	string2ip:  fn(val : string) : array of byte;
	ip2string:  fn(val : array of byte) : string;
	makeVal:    fn(addr: array of int, val: ref ASN1->Elem) : Snmpd->mibreq;
	makeGauge:  fn(val:array of byte) : ref ASN1->Elem;
	makeC32:    fn(val:array of byte) : ref ASN1->Elem;
	makeTime:   fn(val:array of byte) : ref ASN1->Elem;
	makeOpaque: fn(val:array of byte) : ref ASN1->Elem;
	makeStr:    fn(val:string) : ref ASN1->Elem;
	makeIp:     fn(val:array of byte) : ref ASN1->Elem;
	makeOid:    fn(val:array of int) : ref ASN1->Elem;
	makeInt:    fn(val:int) : ref ASN1->Elem;
	convert:    fn(which:int, vali:int, vals:array of byte, vala:array of int) : ref ASN1->Elem;
	oidcmp:     fn(oid1:array of int, oid2: array of int): int;
	chk_oid_path: fn(roid:array of int, coid: array of int, mode: int, elt: array of modinfo): int;
	chk_oid_path2: fn(roid:array of int, coid: array of int, mode: int, elt: array of array of int): int;
	read_config: fn(file: string) : list of string;
	get_config: fn(token:string,configlist:list of string,sep:string): (int,list of string,list of string);
	validate_community: fn(comm:string, action:int, ip:string): int;
	print_packet: fn(inout:int, packet:array of byte);
	sort_oids: fn(a:array of array of int): array of array of int;

	Error			: con -1;
};
