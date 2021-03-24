implement Srv;

include "sys.m";
	sys: Sys;
	stderr: ref Sys->FD;

include "draw.m";
include "ipsrv.m";
is : Ipsrv;
include "srv.m";

 ###########################################################################
 #                                    reads                                #
 ###########################################################################

reads(str: string, off, nbytes: int): (array of byte, string)
{
	if(len str < 1 || nbytes < 1)
		return (nil, "nothing to do");
	bstr := array of byte str;
	if(off >= len bstr)
		return (nil, "offset exceeds string length");
	if(off + nbytes > len bstr)
		nbytes = len bstr - off;
	buf := bstr[off:off+nbytes];
	return (buf, nil);
}

 ###########################################################################
 #                                   services                              #
 ###########################################################################

srvfile: con "/services.txt";
dnsfile: con "/services/dns/db";
iterations: con 10;

# first call to iph2a initializes Srv module
# iph2a(nil)	-- enable caching if Scache module exists
# iph2a("!c")	-- (invalid host) disable caching
#

iph2a(host: string): list of string
{
	if (initsrv(host)) return is->iph2a(host);
	return nil;
}

ipa2h(addr: string): list of string
{
	if (initsrv(nil)) return is->ipa2h(addr);
	return nil;
}

ipn2p(net, service: string): string
{
	if (initsrv(nil)) return is->ipn2p(net, service);
	return nil;
}

NoCache : con "!c";
initsrv(host : string) : int
{
  if (sys == nil) {
    sys = load Sys Sys->PATH;
    stderr = sys->fildes(2);
    is = load Ipsrv Ipsrv->PATH;
    args : list of string;
    if (host != NoCache)
      args = "-c" :: "0 0 0" :: nil;	# Use default cache
    if (is != nil)
      is->init(nil, Ipsrv->PATH :: "-s" :: srvfile :: "-d" :: dnsfile :: "-i" :: string iterations :: args);
    else
      sys->fprint(stderr, "Srv: load %s %r\n", Ipsrv->PATH);
    return host != nil && host != NoCache && is != nil;
  }
  return is != nil;
}
