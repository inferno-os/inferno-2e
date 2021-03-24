
Telco: module
{
	PATH:		con "/dis/svc/telcofs/telco.dis";
	init:	fn(ctxt: ref Context, argv: list of string);	# Command interface
	initr: fn(argv: list of string): string;			# Library interface
	shutdown:	fn();
};
