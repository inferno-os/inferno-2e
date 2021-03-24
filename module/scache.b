# list of string to string caching utility
# originally added to lib/service by Roy Maddux 5/98.
# obc 5/99.
implement Scache;

Mod : con "scache";

include "sys.m";
stderr : ref Sys->FD;
sys : Sys;

include "lock.m";
lock: Lock;


include "daytime.m";
daytime : Daytime;

include "scache.m";

enabled := 1;
initp(owner : string) : int
{
	if (sys != nil) return enabled;
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	if(lock == nil) {
		lock = load Lock Lock->PATH;
		if(lock == nil) {
			sys->fprint(stderr, "%s: failed load %s %r\n", owner, Lock->PATH);
			enabled = 0;
		}
		else
		  addrcachesema = lock->init();
	}
	if(daytime == nil) {
		daytime = load Daytime Daytime->PATH;
		if(daytime == nil) {
			sys->fprint(stderr, "%s: failed load %s %r\n", owner, Daytime->PATH);
			enabled = 0;
		}
	}
	initcache(0, 0, 0);
	return enabled;
}

addrCache: adt {
	key: string;
	value: list of string;
	time_last_used: int;
};

addrcachesize: int;
max_addrcache_uses: int;
max_age: int;
addrcache: array of addrCache;
# Semaphore: import lock;
addrcachesema : ref Lock->Semaphore;

initcache(age, size, maxsize : int) : int
{
	if (sys == nil) initp(Mod);
	if (enabled) {
	  if (age <= 0) max_age = CACHE_AGE;
	  if (size <= 0) addrcachesize = CACHE_SIZE;
	  if (maxsize <= 0 || maxsize < size) max_addrcache_uses = 2*addrcachesize;
	  addrcache = array[addrcachesize] of addrCache;
	  if (age != 0) sys->print(Mod+": enabled (%d sec., %d, %d)\n", max_age, addrcachesize, max_addrcache_uses);
	}
	return enabled;
}

getcache(owner, mach: string): list of string
{
	ii, age: int;
	retval : list of string = nil;
	if (!initp(owner)) return nil;
	lock->addrcachesema.obtain();
	for(ii=0; ii<addrcachesize; ii++) {
		if(mach == addrcache[ii].key) {
			t_now : int = daytime->now();
			age = t_now - addrcache[ii].time_last_used;
			#sys->print("  found %s in cache, entry %d, age %d\n",mach,ii,age);
			if(age > max_age) {
				#sys->print("  too old, invalidate entry %d\n",ii);
				addrcache[ii].key = nil;
				addrcache[ii].time_last_used = 0; # start of epoch
				# do not use after invalidating
			} else {
				#sys->print(" using cache entry %d for %s\n",ii,mach);
				addrcache[ii].time_last_used = t_now;
				retval = addrcache[ii].value;
			}
			break;
		}
	}
	lock->addrcachesema.release();
	#if(retval == nil) sys->print(" not using cache\n");
	return retval;
}

setcache(host: string, addrs: list of string)
{
	#sys->print(" save_in_cache <%s>\n",host);
	t_now : int = daytime->now();
	ii, age, oldest_entry: int;
	oldest_age : int = -1;
	lock->addrcachesema.obtain();
	for(ii=0; ii<addrcachesize; ii++) {
		age = t_now - addrcache[ii].time_last_used;
		# sys->print("entry %d age %d\n",ii,age);
		# sys->print("%s %s\n",tmp1,tmp2);
		if(age > oldest_age) {
			oldest_age = age;
			oldest_entry = ii;
		}
	}
	# sys->print("oldest_entry %d oldest_age %d\n",oldest_entry,oldest_age);
	addrcache[oldest_entry].key = host;
	addrcache[oldest_entry].value = addrs;
	addrcache[oldest_entry].time_last_used = t_now;
	lock->addrcachesema.release();
}
