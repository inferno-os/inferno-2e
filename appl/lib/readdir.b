implement Readdir;

Mod : con "readdir";

include "sys.m";
sys: Sys;
stderr : ref Sys->FD;
Dir: import sys;

include "readdir.m";
include "hash.m";
hl : Hash;
HashTable, HashVal: import hl;

DLEN : con 200;
init(path: string, sortkey: int): (array of ref Dir, int)
{
	sys = load Sys Sys->PATH;
	stderr = sys->fildes(2);
	fd := sys->open(path, sys->OREAD);
	if(fd == nil)
		return (nil, -1);
	
	d := array[DLEN] of Dir;
	a := array[DLEN] of ref Dir;
	n := 0;

	h : ref HashTable;
	if (sortkey & COMPACT)
	  h = mkhash(DLEN);

	for(;;){
		nr := sys->dirread(fd, d);
		if(nr < 0)
			return (nil, -1);
		if(nr == 0)
			break;
		if (len a < n + nr) {
			# expand a
			na := array[2 * len a] of ref Dir;
			na[0:] = a[0:n];
			a = na;
		}
		n = addref(sortkey, d, a, n, nr, h);
	}
	a = a[0:n];

	sortkey &= ~(COMPACT | DIR | FILE);
	if((sortkey & ~DESCENDING) == NONE)
		return (a, n);
	return sortdir(a, sortkey);	
}

#
#addref(nil : int, d : array of Dir, a : array of ref Dir, n, nr : int, nil : ref HashTable) : int
#{
#	for(i := 0; i < nr; i++) a[n++] = ref d[i];
#	return n;
#}
#

sortdir(a: array of ref Dir, key: int): (array of ref Dir, int)
{
	key &= ~(COMPACT | DIR | FILE);
	m: int;
	n := len a;
	for(m = n; m > 1; ) {
		if(m < 5)
			m = 1;
		else
			m = (5*m-1)/11;
		for(i := n-m-1; i >= 0; i--) {
			tmp := a[i];
			for(j := i+m; j <= n-1 && greater(tmp, a[j], key); j += m)
				a[j-m] = a[j];
			a[j-m] = tmp;
		}
	}

	return (a, n);
}

greater(x, y: ref Dir, sortkey: int): int
{
	case (sortkey) {
	NAME => return(x.name > y.name);
	ATIME => return(x.atime < y.atime);
	MTIME => return(x.mtime < y.mtime);
	SIZE => return(x.length > y.length);
	NAME|DESCENDING => return(x.name < y.name);
	ATIME|DESCENDING => return(x.atime > y.atime);
	MTIME|DESCENDING => return(x.mtime > y.mtime);
	SIZE|DESCENDING => return(x.length < y.length);
	}
	return 0;
}

# compact (remove dupplicates) for namespace dirs - obc

mkhash(size : int) : ref HashTable
{
  if (hl == nil)
    hl = load Hash Hash->PATH;
  if (hl == nil)
    sys->fprint(stderr, "Warning: "+Mod+" COMPACT using array: load %s %r\n", Hash->PATH);
  else
    return hl->new(DLEN);
  return nil;
}

addref(sortkey : int, d : array of Dir, a : array of ref Dir, n, nr : int, h : ref HashTable) : int
{
  dirp := sortkey & DIR;
  filp := sortkey & FILE;
  comp := sortkey & COMPACT;
  fnd := 0;
  for(i := 0; i < nr; i++) {
    # done upfront - in namespace: file can shadow dir!
    if (comp && h == nil) fnd = findref(d[i].name, a, n);
    else if (comp) fnd =  findh(d[i].name, h);
    if (!(dirp || filp) ||
	dirp && (d[i].mode & sys->CHDIR) ||
	filp && !(d[i].mode & sys->CHDIR))
      if (!fnd)
	a[n++] = ref d[i];
  }
  return n;
}

findref(e : string, a : array of ref Dir, n : int) : int
{
  for(j := 0; j < n; j++)
    if (e == a[j].name)
      return 1;
  return 0;
}

findh(e : string, h : ref HashTable) : int
{
  if (h != nil) {
    if ((hv := h.find(e)) != nil)
      return 1;
    h.insert(e, HashVal(0, 0.0, e));
  }
  return 0;
}
