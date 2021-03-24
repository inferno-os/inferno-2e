Scache : module
{
	PATH : con "/dis/lib/scache.dis";

	# 15 minutes
	CACHE_AGE : con 15*60;
	CACHE_SIZE : con 10;

	# Required initialization
	initp : fn(owner : string) : int;

	# Optional change the default settings
	initcache : fn(age, size, maxsize : int) : int;

	# Operations on cache
	setcache : fn(host: string, addrs: list of string);
	getcache : fn(owner, mach: string): list of string;
};
