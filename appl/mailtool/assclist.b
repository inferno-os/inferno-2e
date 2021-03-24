# simple functions for manipulating association lists

implement AsscList;

include "assclist.m";

Alist.setitem(s: self ref Alist, n, v: string)
{ 
   curs := s.the_list;
   s.cursor = nil;  # no walking!
   unique := s.unique;
   for (;;) {
       if (curs == nil) {
          s.the_list = ref item(n, v) :: s.the_list;
          return;
       }
       headcurs := hd curs;
       (ck, cv) := (headcurs.key, headcurs.value);
       if (ck == n) {
          if (cv == v) { return; }
          if (unique) {
             (hd curs).value = v;
             return;
          }
       }
       curs  = tl curs;
   }
}

Alist.delitem(s: self ref Alist, n: string): int
{ 
    # reset walking cursor, for safety
    result: int;
    s.cursor = nil;
    (s.the_list, result) = remove_matching_items(s.the_list, n);
    return result;
}

remove_matching_items(l: list of ref item, n: string): (list of ref item, int)
{
    if (l == nil) { return (nil, 0); }
    k := (hd l).key;
    if (k == n) {
       (remainder, nil) := remove_matching_items(tl l, n);
       return (remainder, 1);
    }
    (others, ind) := remove_matching_items(tl l, n);
    return ( (hd l :: others), ind );
}

Alist.getitem(s: self ref Alist, n: string): (string, int)
{
    return first_match(s.the_list, n);
}

first_match(l: list of ref item, n: string): (string, int)
{
    if (l == nil) { return (nil, 0); }
    h := hd l;
    if (h.key == n) {
	return (h.value, 1);
    }
    return first_match(tl l, n);
}

Alist.member(s: self ref Alist, n: string, v: string): int
{
  return pair_match(s.the_list, n, v);
}

pair_match(l: list of ref item, n, v: string): int
{
  if (l==nil) {
    return 0;
  }
  h := hd l;
  if ((h.key == n) && (h.value == v)) {
    return 1;
  }
  return pair_match(tl l, n, v);
}

Alist.getitems(s: self ref Alist, n: string): list of string
{ 
    return all_matches(s.the_list, n);
}

all_matches(l: list of ref item, n: string): list of string
{
    if (l == nil) { return nil; }
    h := hd l;
    if (h.key == n) {
	return h.value :: all_matches(tl l, n);
    }
    return all_matches(tl l, n);
}

Alist.copy(s: self ref Alist): ref Alist
{
    result := ref Alist(nil, nil, s.unique);
    s.first();
    k,v: string;
    test: int;
    for (;;) {
        ((k,v), test) = s.thispair();
	if (test) {
	   result.setitem(k,v);
	} else {
	   break;
	}
	s.next();
    }
    return result;
}

Alist.project(s: self ref Alist, X: list of string): ref Alist
{
    result := ref Alist(nil, nil, s.unique);
    s.first();
    k,v,x1: string;
    test: int;
    curs: list of string;
    for (;;) {
        ((k,v), test) = s.thispair();
        if (test) {
	  curs = X;
	  for (;;) {
	      if (curs==nil) {
                 break;
              } else {
		 x1 = hd curs;
                 curs = tl curs;
                 if (x1==k) {
                    result.setitem(k,v);
                 }
              }
          }
        } else {
          break;
        }
    }
    return result;
}

Alist.thiskey(s: self ref Alist): (string, int)
{ 
    if (s.cursor == nil) { return (nil, 0); }
    return ((hd s.cursor).key, 1);
}

Alist.thisval(s: self ref Alist): (string, int)
{ 
    if (s.cursor == nil) { return (nil, 0); }
    return ((hd s.cursor).value, 1);
}

Alist.thispair(s: self ref Alist): ((string, string), int)
{ 
    if (s.cursor == nil) { return ((nil, nil), 0); }
    h := hd s.cursor;
    return ((h.key, h.value), 1);
}

Alist.next(s: self ref Alist): int
{ 
    if (s.cursor == nil) { return 0; }
    s.cursor = tl s.cursor;
    return 1;
}

Alist.first(s: self ref Alist)
{ 
    s.cursor = s.the_list;
}

Alist.addpairs(s: self ref Alist, d: list of (string, string))
{ 
    a,b: string;
    for (;;) {
	if (d == nil) { break; }
	(a, b) = hd d;
        s.setitem(a,b);
        d = tl d;
    }
}

Alist.frompairs(d: list of (string, string)): ref Alist
{ 
   result := ref Alist(nil, nil, 0);
   result.addpairs(d);
   return result;
}

Alist.augment(s: self ref Alist, other: ref Alist)
{ 
    a, b: string;
    ind: int;
    other.first();
    for (;;) {
	((a, b), ind) = other.thispair();
	if (ind) {
	    s.setitem(a,b);
	} else {
	    break;
	}
	ind = other.next();
    }
}

Alist.union(a: self ref Alist, b: ref Alist): ref Alist
{
   result := ref Alist(nil, nil, 0);
   result.unique = a.unique && b.unique;
   result.augment(a);
   result.augment(b);
   return result;
}

Alist.length(s: self ref Alist): int
{
    return (len s.the_list);
}

Alist.intersect(a: self ref Alist, b: ref Alist): ref Alist
{ 
   result := ref Alist(nil, nil, 0);
   result.unique = (a.unique || b.unique);
   # brute force
   bigger, little: ref Alist;
   if (a.length() < b.length()) {
       (little, bigger) = (a,b);
   } else {
       (bigger, little) = (a,b);
   }
   bigger.first();
   test: int;
   k, v: string;
   for (;;) {
       ((k,v), test) = bigger.thispair();
       if (test) {
	   if (little.member(k,v)) {
	       result.setitem(k,v);
	   }
       } else {
	   break;
       }
       bigger.next();
   }
   return result;
}

Alist.diff(a: self ref Alist, b: ref Alist): ref Alist
{ 
   result := ref Alist(nil, nil, 0);
   result.unique = a.unique;
   # brute force
   a.first();
   test: int;
   k, v: string;
   for (;;) {
       ((k,v), test) = a.thispair();
       if (test) {
	   if (!b.member(k,v)) {
	       result.setitem(k,v);
	   }
       } else {
	   break;
       }
       a.next();
   }
   return result;
}

Alist.subset(a: self ref Alist, b: ref Alist): int
{
    d := a.diff(b);
    return (d.length() == 0);
}

Alist.equal(a: self ref Alist, b: ref Alist): int
{
    return (a.subset(b) && b.subset(a));
} 

Alist.compatible(a: self ref Alist, b: ref Alist): int
{
    test := ref Alist(nil, nil, 0);
    test.unique = 1;
    test.augment(a);
    test.augment(b);
    return (a.subset(test) && b.subset(test));
}

# format a string to an array of bytes with a /0/0 end
# marker.  Any /0 in the string is translated to /0/1
#
# Note /0/n (n>1) explicitly reserved as an out of band markers.
#
quotestringtobyte(s: string): array of byte
{
    a := array of byte s;
    countzeros:= ind:= 0;
    l := len a;
    for (;;) {
        if (ind >= l) { break; }
        if (a[ind] == (byte 0)) { countzeros++; }
        ind++;
    }
    outlen := l + countzeros + 2;
    result := array[outlen] of byte;
    ind = 0;
    outind := 0;
    test: byte;
    for (;;) {
        if (ind >= l) { break; }
        test = result[outind] = a[ind];
        if (test == (byte 0)) {
           outind++;
           result[outind] = byte 1;
        }
        ind++;
        outind++;
    }
    # assert outind==outlen-2
    result[outlen-1] = result[outlen-2] = byte 0;
    return result;
}

# return string unformatted from array of bytes (inverse of
# fmtstringtobyte).  return int as end of string in array,
# or 0 upon failure.
#
unquotestringfrombyte(b: array of byte, start: int): (string, int)
{
    countzeros := outlen := 0;
    maxind := len b;
    inindex := start;
    # determine # of zeros, check fmt.
    for (;;) {
        if (inindex >= maxind) {
           return (nil, 0); # off end of array
        }
        if (b[inindex] == (byte 0)) {
           inindex++;
           if (b[inindex] == (byte 0)) {
              break; # end marker
           } else {
             if (b[inindex] == (byte 1)) {
               countzeros++; # zero byte
             } else {
               return (nil, 0); # invalid /0/n sequence
             }
          }
        }
        inindex++;
        outlen++;
    }
    resultbytes := array[outlen] of byte;
    # lastin := inindex
    inindex = start;
    outindex := 0;
    for (;;) {
        if (inindex >= maxind) {
           return (nil, 0); # never happens
        }
        if (b[inindex] == (byte 0)) {
           inindex++;
           if (b[inindex] == (byte 0)) {
              inindex++;
              break; # done
           } else {
             if (b[inindex] == (byte 1)) {
               resultbytes[outindex] = byte 0; # quoted 0 byte
             } else {
               return (nil, 0); # never happens
             }
          }
        } else {
          resultbytes[outindex] = b[inindex]; # usual case
        }
        inindex++;
        outindex++;
    }
    # assert lastin == inindex && outindex == outlen
    return ( (string resultbytes), inindex );
}

### Marshalling.  Uniqueness not currently marshalled.

# marshal an alist to array of byte of format
#   formattedkey | formattedval | .. | error
# using string marshalling defined by quotestringtobyte,
# and error of form /0/2.
#
Alist.marshal(a: self ref Alist): array of byte
{
    final := array[] of {byte 0, byte 2};
    totallen := 2;
    thebytes := (final :: nil);
    a.first();
    test: int;
    k,v: string;
    for (;;) {
        ((k,v), test) = a.thispair();
        if (test) {
          kb := quotestringtobyte(k);
          vb := quotestringtobyte(v);
          thebytes = (kb :: vb
			:: thebytes );
          totallen = totallen + (len kb) + (len vb);
        } else {
          break;
        }
        a.next();
    }
    result := array[totallen] of byte;
    here:= there:= 0;
    somebytes: array of byte;
    # pop off the bytes, smack them into the result
    allbytes := thebytes; # for debug.
    for (;;) {
        if (thebytes == nil) {
           break;
        }
        somebytes := hd thebytes;
        thebytes = tl thebytes;
        there = here + (len somebytes);
        result[here :] = somebytes;
        here = there;
    }
    # assert here = totallen
    return result;
}

# unmarshal from marshalled format,
# return  (ref Alist, nil)
# or return (nil, msg) on format error
#
Alist.unmarshal(b: array of byte, start: int):
    (ref Alist, string)
{
    maxlen := len b;
    if (start >= maxlen) {
       return (nil, "Alist.unmarshal: start past array end");
    }
    k, v: string;
    test: int;
    place:= start;
    result:= ref Alist(nil, nil, 0);
    diag := ""; # for debug.
    for (;;) {
        (k, test) = unquotestringfrombyte(b, place);
        if (test == 0) {
           break;  # apparent end of structure
        }
        place = test;
        (v, test) = unquotestringfrombyte(b, place);
        if (test == 0) {
           return (nil, "Alist.unmarshal: no value for "+k+diag);
        }
        place = test;
        result.setitem(k, v);
    }
    if ( place+2 > maxlen ) {
       return (nil, "termination marker past array end"); # bad termination
    }
    # check final bytes
    if ( (b[place] != (byte 0)) || (b[place+1] != (byte 2)) ) {
        return (nil, "termination missing");
    }
    return (result, nil);
}

