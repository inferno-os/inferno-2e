CCI: module
{
	PATH:	con "/dis/charon/cci.dis";

	# Common Client Interface, for external control of Charon

	init: fn(smod: String, emod: Events, umod: Url);
	view: fn(url, ctype: string, data: array of byte);
};
