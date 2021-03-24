Snmp_Msg : module 
{
	PATH:		 con "/dis/snmp/snmp_msg.dis";

	HEADER:  con "ALARM";
	TIMEOUT: con (byte 1);
	DEBUG:   con (byte 2);
	LMODS:   con (byte 3);
	EXIT:    con (byte 4);
	RESET:   con (byte 5);
	TIMER:   con (byte 6);

	init: fn(nil:ref Draw->Context, argv:list of string);
};
