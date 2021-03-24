### Written by B.A. Westergren
### ed obc
implement ChSrv;

include "sys.m";
	sys: Sys;
        stderr : ref Sys->FD;

include "draw.m";
	Context: import Draw;

include "chprocess.m";
chproc : ChProcess;

Reply: adt
{
  fid   : int;
  reply : array of byte;
  size  : int;
  bsize : array of byte;
  rchan : chan of int;
};
rlist: list of ref Reply;

ChSrv: module
{
  init : fn(ctxt: ref Draw->Context, argv: list of string);
};

#
# init(nil, "chsrv" :: "/dis/process.dis" :: "/tmp/chan/chsrv" :: nil) 
# Called with the process algorithm path to be used and optional
# channel file path.
# Process algorithm interface is chprocess.m extra arguments are passed
# to process init function.
#
init(ctxt : ref Context, argv : list of string)
{
  sys = load Sys Sys->PATH;
  if (sys == nil)
    exit;

  stderr = sys->fildes(2);

  path : string;
  if (argv != nil)
    argv = tl argv;

  if (argv != nil) {
    path = hd argv;
    argv = tl argv;
  }

  file := "/chan/chsrv";
  if (argv != nil) {
    file = hd argv;
    argv = tl argv;
  }

  if (path == nil) {
    sys->fprint(stderr, "Error[ChSrv]: Process algorithm path must be specified\n");
    return;
  }
  
  spawn initchan(ctxt, path, file, argv);
}

fileofdir(file : string) : (string, string)
{
  dir := file;
  for (l := len file -1; l > 0; l--)
    if (file[l] == '/') {
      dir = file[0:l];
      file = file[l+1: len file];
      break;
    }
  #sys->print("file=%s, dir=%s\n", file, dir);
  return (file, dir);
}

reqch, freech : chan of int;

#
# Initialize the channel and loop forever handling read and write
# requests from the user.  This process will handle asynchronous 
# requests from multiple users.
#
initchan(ctxt : ref Context, path, file : string, argv : list of string)
{
  dpath := file;
  (file, dpath) = fileofdir(file);

  sys->bind("#s", dpath, sys->MBEFORE | sys->MCREATE);
  io := sys->file2chan(dpath, file);
  if(io == nil) {
    sys->fprint(stderr, "Error[ChSrv]: Opening %s: %r\n", dpath);
    return;
  }

  if (chproc == nil) {
    chproc = load ChProcess path;
    if (chproc == nil) {
      sys->fprint(stderr, "Error[ChSrv]: Cannot load %s\n", path);
      return;
    }
    # Allow for an init function called upon process loading
    chproc->init(ctxt, path :: argv);
  }

  # Monitor for rlist queue
  reqch = chan of int;
  freech = chan of int;	
  spawn cs_mon();

  for (;;) {
    alt {
      (off, buf, fid, wc) := <-io.write =>
	reqch <- = 1;
        cleanfid(fid);
	freech <- = 1;
	if(wc == nil)
	  break;
	spawn process(buf, fid, wc);
	buf = nil;

      (nil, i, fid, rc) := <-io.read =>
	if (rc != nil) {
	  spawn result(fid, i, rc);
	  continue;
	}
    }
  }
}

cs_mon()
{
    for (;;) {

	# Wait for request
	nil = <-reqch;

	# Wait for free
	nil = <-freech;
    }
}

#
# Process the write request. Create a channel to synchronize the read
# request.  Place the reply adt on the reply list with a valid fid and
# sync channel.  Then release the users write request.
#
process(data : array of byte, fid :int, wc : chan of (int, string))
{
  # Channel to sync on result completion
  rchan := chan of int;

  r := ref Reply;
  r.rchan = rchan;
  r.fid = fid;
  r.bsize = array[2] of byte;
  r.bsize[0] = byte 0;
  r.bsize[1] = byte 0;

  reqch <- = 1;
  rlist = r :: rlist;
  freech <- = 1;

  # Must create an entry before returning
  wc <-= (len data, nil);

  processdata(r, data);
}

#
# Try to load the users specific process algorithm. If one is not found
# then exit with error.  Call the user process function, synchronously
# when complete fill the reply field with the response.  Set the size
# of the reponse data and free the read request.
#
processdata(r : ref Reply, data : array of byte)
{
  # Process the input data
  r.reply = chproc->process(data);
  r.size = len r.reply;
  # used to send the size of the reply buffer
  (b1, b2) := twobytes(r.size);
  r.bsize[0] = b1;
  r.bsize[1] = b2;
  # Let the reader know the information is ready
  r.rchan <-= 0;
}

#
# Process for the reader function.  Looks in the reply list (rlist) to
# find a valid file id (fid).  If found and the reply value is nil then
# wait for the write process to complete.  When data is available or
# if it was available from a previous read then attempt to fill the
# users buffer.
#
result(fid : int, size : int, ch : chan of (array of byte, string))
{
  r := ref Reply;
  s : list of ref Reply;
  reply : array of byte;

  for(s = rlist; s != nil; s = tl s) {
    r = hd s;
    if(r.fid == fid) {
      # if this is the first read then fill reply with r.bsize
      if (r.bsize != nil) {
	<- r.rchan;
	reply = r.bsize;
	r.bsize = nil;
	break;
      }
      if (size > r.size) {
	reply = r.reply;
	r.size = 0;
	break;
      }
      else {
	reply = r.reply[0:size];
	r.reply = r.reply[size:];
	r.size -= size;
	break;
      }
    }
  }

  if (r.size <= 0) {
    reqch <- = 1;
    cleanfid(fid);
    freech <- = 1;
  }

  if (reply == nil)
    ch <-= (nil, "can't find result");
  else
    ch <-= (reply, nil);

  # BAW (build <= 27)
  #sys->sleep(3000);
}

cleanfid(fid: int)
{
  new : list of ref Reply;

  for(s := rlist; s != nil; s = tl s) {
    r := hd s;
    if(r.fid != fid)
      new = r :: new;
  }

  rlist = new;
}

twobytes(length : int) : (byte, byte)
{
  q1 := length >> 8;
  q2 := length - (q1 << 8);
  return (byte q1, byte q2);
}
