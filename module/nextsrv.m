Nextsrv: module
{
	init : fn(ctxt: ref Context, args: list of string);
	# This function returns once nextsrv is alive (connected)
	# if nextsrv is already active (connected) must return immediately
	srv : fn() : int;
};
