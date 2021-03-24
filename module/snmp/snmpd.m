Snmpd : module 
{
	PATH : con "/dis/snmp/snmpd.dis";

	Global : adt {
		utils   : Snmp_Utils;
		log     : Log;
		local_addr : string;
		sgc 	: array of int;		# snmp group counters
		maxpacketsize : int;
		version    : int;
		port    : string;
		logfile : string;
		configfile : string;
		configlist : list of string;
		trap    : TrapInfo;
		hostlist : array of HostInfo;
		alarm : int;
		timeout : int;
		getMess : array of string;
		backout : int;
		s_time 	: int; 			# cold start time
		ws_time : int;			# warm start time
		opsys : int;
		storage : list of Snmp_Utils->leafdata;
	};

	mibreq : adt {
		name : ref ASN1->Oid;
		val  : ref ASN1->Elem;
	};

	TrapInfo : adt {
		mode : int;
		port : string;
		community : string;
		portBytes : array of byte;
		Enterprise : ref ASN1->Oid;
		ClientAddress : array of byte;
		ManagerAddress : string;
		ManagerAddressBytes : array of byte;
	};

	HostInfo : adt {
		ip : string;
		community : string;
		permission : string;
	};

	### Configuration file keys
	ENTERPRISE:     con "Enterprise";
	CLIENTADDR:     con "ClientAddress";
	TRAPINFO:       con "Trap";
	OPSYS:		con "OperatingSystem";
	SYSCONTACT:     con "Contact";
	SYSDESCR:       con "Description";
	SYSLOCATION:    con "Location";
	SYSOBJECTID:    con "ObjectID";
	SYSSERVICES:    con "Services";
	AUTHENTRAPS:    con "AuthenticationTraps";
	HOSTLIST:       con "Community";
	SUBAGENT:       con "MibEntryPoint";

	### debug levels for snmp
	REQ_P: con 4;        #path of a request
	REQ_H: con 5;        #highlights of a request
	REQ_A: con 6;        #all aspects of a request
	INTR: con 7;         #interesting data used in close debugging
	OBSC: con 8;         #obscure data needed for close debugging
	LOWEST: con 9;       #happens very often - too many messages

	### miscellaneous constants
	MAXSUBAGENTS: con 100;
	UDP_OFFSET: con 6;
	DCOMM: con "public";
	Dlogfile: con "/services/snmp/log.snmp";
	Dconfigfile: con "/services/snmp/snmpd.conf";
	Dtimeout: con 300;
	NATIVE,EMULATED: con iota;
	Read: con "read";
	Write: con "write";
	Dip: con "0.0.0.0";

	### snmp group counter index values
	snmpInPkts, snmpOutPkts, snmpInBadVersions,
	snmpInBadCommunityNames, snmpInBadCommunityUses,
	snmpInASNParseErrs, snmpInTooBigs,
	snmpInNoSuchNames, snmpInBadValues, snmpInReadOnlys,
	snmpInGenErrs, snmpInTotalReqVars, snmpInTotalSetVars,
	snmpInGetRequests, snmpInGetNexts, snmpInSetRequests,
	snmpInGetResponses, snmpInTraps, snmpOutTooBigs,
	snmpOutNoSuchNames, snmpOutBadValues,
	snmpOutGenErrs, snmpOutGetRequests, snmpOutGetNexts,
	snmpOutSetRequests, snmpOutGetResponses,
	snmpOutTraps, snmpEnableAuthenTraps, snmpdummylast : con iota;

	### error strings for return values
	TOOBIG: con "Value is too big for intended type.";
	NOSUCHNAME: con "No such variable.";
	BADVALUE: con "Value is of the wrong type.";
	READONLY: con "Cannot modify read-only variable.";
	GENERR: con "General error encountered.";


	GET,GETN,RESP,SET,TRAP	: con iota;

	noError, tooBig, noSuchName, 
	badValue, readOnly, genErr : con iota;

	coldStart, warmStart, linkDown, linkUp, 
	authFailure, egpNeighLoss, entSpec : con iota;	

	NEXTGROUP 	: con 10;
	PduType 	: con 16ra0;

	IpAddress	: con 0;
 	Counter32	: con 1;
	Gauge32		: con 8; # should be 2 but ASN1->INTEGER is in the way
	ActualGauge32	: con 2;
 	TimeTicks	: con 3;
 	Opaque		: con 9; # should be 4 but ASN1->OCTET_STRING is in the way
 	ActualOpaque	: con 4;
	NsapAddress	: con 5;
	Counter64	: con 6;
	UInteger32	: con 7;

	init: fn(ctxt: ref Draw->Context, argv: list of string);
};
