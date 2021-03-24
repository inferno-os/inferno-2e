Kill: module
{
	PATH:	con "/dis/kill.dis";

	# Command Line Interface
	init:		fn(ctxt: ref Draw->Context, argv: list of string);

	# Kill a thread based on a string representation of its PID
	#	msg is sent to stderr
	killpid:		fn(pid: string, msg: array of byte);

	# Kill a thread based on its module name
	#	msg is sent to stderr
	killmod:		fn(mod: string, msg: array of byte);
};
