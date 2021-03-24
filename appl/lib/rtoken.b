# read token from fd
# faster than bufio (utf, no seek while readtoken on fd)
# mthread safe without need for close
# obc
implement Rtoken;

Mod : con "rtoken";

include "sys.m";
sys : Sys;
stderr : ref Sys->FD;

include "draw.m";

include "rtoken.m";

init(nil : ref Draw->Context, args : list of string)
{
  d := "\r\n";
  id := id();
  if (args == nil)	# init only
    return;
  fd := sys->fildes(0);
  if (args != nil)
    args = tl args;
  opt : string;
  if (args != nil && hd args != nil) {
    opt = hd args;
    case opt[0] {
      '-' => opt = opt[1:]; id.seteot(NOEOT);
      '+' => opt = opt[1:]; id.seteot(WITHEOT);
    }
  }
  if (opt != nil)
    id.setbuflen(int opt);
  cat1(fd, d, id);
}

# read/write delimited tokens
cat1(stdin : ref Sys->FD, delim : string, id : ref Id)
{
  while((t := readtoken(stdin, delim, id)) != nil)
    print1(t, id);
  if (id.n < 0)
    sys->fprint(stderr, Mod+": error reading stdin: %r\n");
}

print1(t : string, id : ref Id)
{
  if (id.eot)
    sys->print("%s", t);
  else
    sys->print("%s\n", t);
}

id() : ref Id
{
  if (sys == nil) {
    sys = load Sys Sys->PATH;
    stderr = sys->fildes(2);
  }
  return ref Id(nil, nil, sys->ATOMICIO, WITHEOT, 1);
}

Id.seteot(id : self ref Id, eot : int)
{
  id.eot = eot;
}

Id.setbuflen(id : self ref Id, blen : int)
{
  if (blen <= 0 || blen > sys->ATOMICIO)
    blen = sys->ATOMICIO;
  if (blen < 128)
    sys->fprint(stderr, "Warning: "+Mod+" (efficiency) small buffer length set to %d\n", blen);
  id.BLEN = blen;
}

readtoken(stdin : ref Sys->FD, delim : string, id : ref Id) : string
{
  r : string;
  n : int;
  for (;;) {
    if (id.l == nil && id.n > 0)
      (id.n, id.l, id.buf) = readtokens(stdin, delim, id.buf, id.BLEN);
    if (id.l == nil)
      return nil;
    (r, id.l) = (hd id.l, tl id.l);
    if (id.eot == WITHEOT)
      return r;
    else {
      if (r != nil && find(r[len r -1], delim))
        r = r[0:len r -1];
      if (r != nil) return r;
    }
  }
  return r;
}

readtokens(stdin : ref Sys->FD, delim : string, buf : array of byte, BL : int) : (int, list of string, array of byte)
{
  off := len buf;
  tmp := array[BL + off] of byte;
  tmp[0:] = buf;
  buf = tmp;
  hds : string;
  n : int;
  while((n = sys->read(stdin, buf[off:], BL)) > 0) {
    n += off;
    off = 0;
    m := sys->utfbytes(buf, n);
    sbuf := string buf[0:m];
    eol := drpos(delim, sbuf);
    if (eol >= 0 && ++eol < m) {
      m = eol;
      sbuf = sbuf[0:m];
    }
    if (m < n) {
      buf[0:] = buf[m:n];
      off = n - m;
    }
    (ep, l) := tokenize1(hds+sbuf, delim);
    # sys->print("tokenizend -> %d\n", ep);
    if (ep) {
      return (n, l, buf[0:off]);
    }
    else
      hds = hds + sbuf;
  }
  r : list of string;
  if (off > 0)
    r = hds + string buf[0:off] :: nil;
  return (n, r, nil);
}

# tokenize on one char delimiter
# preserve first delimiter found at end of each token
tokenize1(s, d : string) : (int, list of string)
{
  l : list of string;
  a := len s;
  tok := 0;
  cnt := 0;

  for(i := a -1; i >= 0; i--)
    if (find(s[i], d)) {
      tok = 1;
      if (i+1 < a) {
	cnt++;
	l = s[i+1:a] :: l;
	a = i+1;
      }
    }
  if (!tok)
    return (0, s :: nil);
  if (a) {
    cnt++;
    l = s[0:a] :: l;
  }
  return (cnt, l);
}

# first delimiter position
dpos(d, s : string) : int
{
  for(i := 0; i < len s; i++)
    if (find(s[i], d))
      return i;
  return -1;
}

# last delimiter position (from end)
drpos(d, s : string) : int
{
  for(i := len s -1; i >= 0; i--)
    if (find(s[i], d))
      return i;
  return -1;
}

find(e : int, s : string) : int
{
  for(i := 0; i < len s; i++)
    if (s[i] == e)
      return 1;
  return 0;
}

reverse(l : list of string) : list of string
{
  r : list of string;
  for(; l != nil; l = tl l)
    r = hd l :: r;
  return r;
}
