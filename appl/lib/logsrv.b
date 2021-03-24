implement LogSrv;


include "sys.m";
include "srv.m";
include "draw.m";
include "string.m";
include "daytime.m";
include "logsrv.m";
include "eventCodes.m";

FD, FileIO: import Sys;
Context: import Draw;

sys: Sys;
str: String;
daytime: Daytime;
journal: LogJournal;
ms: LogMeasure;

stderr,stdlog : ref FD;


LogSrv: module
{
 init:	fn(nil: ref Context, argv: list of string);
};


line: adt
{
 ln: array of byte;
};

logBuf: array of ref line;

Nmax: con 1000;
current: int;
wrap: int;
tail: int;
bufSiz: int;
journalFlag: int;
msFlag: int;
newFileName : string;

Reply: adt
{
 fid:	int;
 startIdx: int;
 curIdx: int;
 wrapState: int;
};

Wait: adt
{
 fid: int;
 rcWait: Sys->Rread;
};

rlist: list of ref Reply;
waitList: list of ref Wait;

init(nil: ref Context, argv: list of string)
{
  if((sys = load Sys Sys->PATH) == nil)
    return;
  stderr = sys->fildes(2);
  str = load String String->PATH;
  if (str == nil)
    {
      sys->fprint(stderr, "LogSrv: FATAL: cannot load module %s: %r\n", String->PATH);
      exit;
    }

  daytime = load Daytime Daytime->PATH;
  if (daytime == nil)
    {
      sys->fprint(stderr, "LogSrv: FATAL: cannot load module %s: %r\n", Daytime->PATH);
      exit;
    }

# set defaults
  tail = 0;
  bufSiz = Nmax;
#  forceLogName = nil;
  logDir : string;
  logFileName : string;
  logCtlName : string;
  file, ctl : ref Sys->FileIO;
  journalFlag = 0;
  msFlag = 0;
 
# perhaps override defaults
  commandArgs(argv);

  sys->fprint(stderr, "LogSrv size=%d  tail=%d\n\n", bufSiz, tail);

  logDir = Events->logdirName;
  logFileName = Events->logName;
  logCtlName = Events->logctlName;

  if (newFileName != nil)
    (logDir, logFileName, logCtlName) = parsePath(newFileName);

  sys->bind("#s", logDir, sys->MBEFORE);
  file = sys->file2chan(logDir, logFileName);
  ctl = sys->file2chan(logDir, logCtlName);

  if(file == nil) {
    sys->fprint(stderr, "LogSrv: failed to make file: %s/%s %r\n", logDir, logFileName);
    return;
  }
  if(ctl == nil) {
    sys->fprint(stderr, "LogSrv: failed to make file: %s/%s%r\n", logDir, logCtlName);
    return;
  }

 
  spawn logsrvr(file, ctl);

  sys->fprint(stderr, "LogSrv: logging on %s/%s\n", logDir, logFileName);
}



logsrvr(file: ref FileIO, ctl: ref FileIO)
{
  data : array of byte;
  off, nbytes, fid : int;
  rc : Sys->Rread;
  wc : Sys->Rwrite;

  start(bufSiz);
  idx : int;
  for(;;)
    {
      alt {
	(off, nbytes, fid, rc) = <-file.read =>
	  if(rc == nil)
	    {
	      cleanfid(fid);
	      continue;
	    }
	idx = findFid(fid);
#sys->fprint(stderr, "\n    read %d idx %d  current %d   wrap %d\n", fid, idx, current, wrap);

	if (idx >= 0)
	  {
	    rc <-= ((*logBuf[idx]).ln, nil);
	  }
	else
	  {
	    errMsg : string;
	    case idx 
	      {
		-1 =>  errMsg = nil;

		-2 =>  errMsg = "Clobbered";
		-3 =>  errMsg = "File Empty";
		*  =>  errMsg = "Unknown ERROR";
	      }
	    if(idx == -1 && tail == 1)
	      {
		addWait(fid, rc);
	      }
	    else
	      {
		rc <-= (nil, errMsg);
		cleanfid(fid);
	      }
	  }


	(off, data, fid, wc) = <-file.write =>
	  if(wc == nil)
	    continue;
	wc <-= putMsg(data);
	data = nil;


	(off, data, fid, wc) = <-ctl.write =>
	  if(wc == nil)
	    break;
	wc <-= control(data);
	data = nil;
      }
    }
}



# returns index of line to write back
# -1 at end of file
# -2 for overrun -- where writer clobbered the reader
# -3 virgin buffer -- never written.
findFid(fid: int): int
{
  xlist := rlist;
  curReply : ref Reply;
  start, idx: int;

  while(xlist != nil)
    {
      curReply = hd xlist;
      if((*curReply).fid == fid)
	{
	  idx = (*curReply).curIdx;
	  idx++;
	  idx %= bufSiz;
	  (*curReply).curIdx = idx;
	  if ((*curReply).curIdx == current) return -1;
	  else return (*curReply).curIdx;
	}
      else xlist = tl xlist;
    }
  
# NOT FOUND -- hence first read by this fid
  if (current == 0 && wrap == 0) return -3;

  headstart : int;
  headstart = bufSiz / 10;
  if(wrap == 1)
    {
      start = (current + headstart) % bufSiz;
    }
  else
    {
      if ((current + headstart) > bufSiz)
	start = (current + headstart) % bufSiz;
      else start = 0;
    }

  rlist = ref Reply(fid, start, start, wrap) :: rlist;
  return start;
}


inWait(fid: int): (int, Sys->Rread)
{
  xlist := waitList;
  waiter : ref Wait;

  while(xlist != nil)
    {
      waiter = hd xlist;
      if (fid == (*waiter).fid) return (1, (*waiter).rcWait);
      xlist = tl xlist;
    }
  return (0, nil);
}
  
addWait(fid: int, rcw: Sys->Rread)
{
  (found, nil) := inWait(fid);
  if(found == 0)
    {
      waitList = ref Wait(fid, rcw) :: waitList;
    }
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




control(data: array of byte): (int, string)
{
  size : int;
  strdata : string;

  strdata = string data[0: (len data -1)];

  if (numeric(strdata))
    {
      size = int strdata;

      if (size > 0)
	{
	  oldSiz := bufSiz;
	  resize(size);
	  return (0, sys->sprint("LogSrv: resizing from %d to %d\n", oldSiz, size));
	}
      else 
	{
	  return(0, sys->sprint("Logsrv: Zero or negative length %d not allowed\n", size));
	}
    }


  case strdata
    {
      "journal" =>
	oldj := journalFlag;
      journalFlag = 1;
      journal = load LogJournal LogJournal->PATH;
      if (journal == nil)
	{
	  sys->fprint(stderr, "LogSrv: cannot load module %s: %r\n", LogJournal->PATH);
	  journalFlag = 0;
	}
      else journal->journalInit();
      return (0,sys->sprint("LogSrv: change mode from journal=%d to journal=1\n", oldj) );


      "nojournal" =>
	oldj := journalFlag;
      journalFlag = 0;
      journal = nil;
      return (0,sys->sprint("LogSrv: change mode from journal=%d to journal=0\n", oldj) );

      "measure" =>
	oldms := msFlag;
      msFlag = 1;
      ms = load LogMeasure LogMeasure->PATH;
      if (ms == nil)
	{
	  sys->fprint(stderr, "LogSrv: cannot load module %s: %r\n", LogMeasure->PATH);
	  msFlag = 0;
	}
      else ms->measureInit();
      return (0,sys->sprint("LogSrv: change mode from measure=%d to measure=1\n", oldms) );


      "nomeasure" =>
	oldms := msFlag;
      msFlag = 0;
      ms = nil;
      return (0,sys->sprint("LogSrv: change mode from measure=%d to measure=0\n", oldms) );


      "tail" =>
	oldtail := tail;
      tail = 1;
      return (0,sys->sprint("LogSrv: change mode from tail=%d to tail=1\n", oldtail) );


      "notail" =>
	oldtail := tail;
      tail = 0;
# trigger the waiters
      waiter : ref Wait;
      while(waitList != nil)
	{
	  waiter = hd waitList;
	  (*waiter).rcWait <-= (nil, nil);
	  waitList = tl waitList;
	}
      return (0, sys->sprint("LogSrv: change mode from tail=%d to tail=0\n", oldtail));


      "status" =>
	return(0, sys->sprint("LogSrv: siz=%d bufSiz= %d current=%d wrap=%d readers=%d waiters=%d journal=%d measure=%d\n",
			      len logBuf, bufSiz, current, wrap, len rlist, len waitList, journalFlag, msFlag));

      "dump" =>
	dump();
      return(0, sys->sprint("LogSrv: siz=%d bufSiz= %d current=%d wrap=%d readers=%d waiters=%d\n",
			    len logBuf, bufSiz, current, wrap, len rlist, len waitList));

	
      * =>
	return(0, "Logsrv: Unrecognized control command\n");
    }
  return(0, nil);
}

resize(size: int)
{
#sys->fprint(stderr, "Resize from %d to %d\n", bufSiz, size);
  tmpAry : array of ref line;

  case relative(size, bufSiz)
    {
      1 =>  # increase size
	tmpAry = array[size] of ref line;
    shift: int;
      if(wrap == 0)
	{
	  tmpAry[0:] = logBuf[0: (current -1)];
	}
      else # wrap == 1
	{
	  shift = bufSiz - current;
	  tmpAry[0: ] = logBuf[current: bufSiz];
	  tmpAry[shift: ] = logBuf[0: current];
	  wrap = 0;
	  adjustIncrease(shift);
	  current += shift;
	}
      bufSiz = size;
      logBuf = tmpAry;
#sys->fprint(stderr, "Resize complete -- increase\n");

      0 =>
# do nothing -- its a no-op
	;
      -1 => # shrink size
	tmpAry = array[size] of ref line;
      shift : int;
      if(current == size)
	{
	  tmpAry[0:] = logBuf[0: current];
	}
      else
	{
	  if(current > size)
	    {
	      shift = (current - size) + 1;
	      if(wrap == 1) 
		{
		  tmpAry[0:] = logBuf[shift: (current + 1)];
		}
	      else
		{
		  tmpAry[0:] = logBuf[(shift - 1): current];
		  wrap = 1;
		}
	      current = size - 1;
	      adjustDecrease(shift, size);
	    }
	  else
	    {
	      shift = size - current;
	      tmpAry[0:] = logBuf[0: current];
	      tmpAry[current:] = logBuf[ (bufSiz - shift) : bufSiz];
	      adjustDecrease(0, size);
	    }
	}
      bufSiz = size;
      logBuf = tmpAry;
#sys->fprint(stderr, "Resize complete -- decrease\n");
    }
#dump();
}



adjustDecrease(shift, newSiz: int)
{
  xlist := rlist;
  curReply : ref Reply;
  idx : int;

  while(xlist != nil)
    {
      curReply = hd xlist;
      idx = (*curReply).curIdx;
      if(shift == 0 && idx >= newSiz)
	{
	  idx = 0;
	}
      else
	{
	  idx = idx - shift;
	  if (idx < 0) idx = 0;
	}
      (*curReply).curIdx = idx;
      xlist = tl xlist;
    }
}

adjustIncrease(shift: int)
{
  xlist := rlist;
  curReply : ref Reply;
  idx : int;

  while(xlist != nil)
    {
      curReply = hd xlist;
      idx = (*curReply).curIdx;
      if(idx < current)
	{
	  (*curReply).curIdx = idx + shift;
	}
      else
	{
	  (*curReply).curIdx = idx - current;
	}

      xlist = tl xlist;
    }
}


relative(a, b: int): int
{
  if(a>b) return 1;
  if(a<b) return -1;
  return 0;
}

numeric(a: string): int
{
  i, c: int;

  for(i = 0; i < len a; i++) {
    c = a[i];
    if(i == 0 && c == '-')
      {
	;
      }
    else
      {
	if(c < '0' || c > '9')
	  return 0;
      }
  }
  return 1;
}


start(n:int)
{
  logBuf = array[n] of ref line;
  current = 0;
  wrap = 0;
}


putMsg(msg: array of byte): (int, string)
{
  timeAry := array of byte daytime->time();

# may want to check the module handle and reopen if necessary -- gives a way to bind a new module.
  if (journalFlag == 1)   journal->journalRecord(string msg, string timeAry);
  if (msFlag == 1) ms->measureRecord(string msg);

  tlen := len timeAry;
  mlen := len msg;
  pad := array of byte " :  ";
  padlen := len pad;
  tmpAry := array[(tlen + mlen) + padlen] of byte;

  tmpAry[0:] = timeAry[0:];
  tmpAry[tlen:] = pad[0:];
  tmpAry[(tlen+padlen):] = msg[0:];

  logBuf[current] = ref line(tmpAry);
  current++;
  current %= bufSiz;
  if(current == 0)
    {
      wrap = 1;
    }

# trigger the waiters
  waiter : ref Wait;
  while(waitList != nil)
    {
      waiter = hd waitList;
      (*waiter).rcWait <-= (tmpAry, nil);
      waitList = tl waitList;
    }

  return (len msg, nil);
}


dump()
{
 i:int;
 sys->fprint(stderr, "DUMP\n==============\n current=%d  bufSiz=%d wrap=%d\n", current, bufSiz, wrap);
 if(wrap == 0) # never filled yet
   {
     for(i=0;i<current;i++)
       {
	 if(isNil(logBuf[i], i) == 0) sys->fprint(stderr, "%d  %s\n", i, string (*logBuf[i]).ln);
       }
   }
 else # filled at least once
   {
     if(current==0) # wrap point -- its full
       {
	 for(i=0;i<bufSiz;i++)
	   {
	     if(isNil(logBuf[i], i) == 0) sys->fprint(stderr, "%d  %s\n", i, string (*logBuf[i]).ln);
	   }
       }
     else
       {
	 for(i = current;i<bufSiz;i++)
	   {
	     if(isNil(logBuf[i], i) == 0) sys->fprint(stderr, "%d  %s\n", i, string (*logBuf[i]).ln);
	   }

	 for(i=0;i<current;i++)
	   {
	     if(isNil(logBuf[i], i) == 0) sys->fprint(stderr, "%d  %s\n", i, string (*logBuf[i]).ln);
	   }
       }
   }

 sys->fprint(stderr, "\n\n");
}

isNil(lineRef: ref line, n:int): int
{ 
  if(lineRef == nil)
    {
      sys->fprint(stderr, "%d is NIL\n", n);
      return 1;
    }
  else return 0;
}

commandArgs(argv: list of string)
{
  if (argv != nil)
    argv = tl argv;

  while(argv != nil)
    {
      arg := hd argv;
      if (numeric(arg) && (int arg) > 0) 
	{
	  bufSiz = int arg;
	}
      if (arg == "log") {
	argv = tl argv;
	if (argv != nil)
	  newFileName = hd argv;
      }

      if (arg == "tail") tail = 1;
      if (arg == "notail") tail = 0;
      if (arg == "journal")
	{
	  journalFlag = 1;
	  journal = load LogJournal LogJournal->PATH;
	  if (journal == nil)
	    {
	      sys->fprint(stderr, "LogSrv: Journaling Disabled %s: %r\n", LogJournal->PATH);
	      journalFlag = 0;
	    }
	  else journal->journalInit();
	}
      if (arg == "nojournal")
	{
	  journalFlag = 0;
	  journal = nil;
	}
      if (arg == "measure")
	{
	  msFlag = 1;
	  ms = load LogMeasure LogMeasure->PATH;
	  if (ms == nil)
	    {
	      sys->fprint(stderr, "LogSrv: Measurements Disabled %s: %r\n", LogMeasure->PATH);
	      msFlag = 0;
	    }
	  else ms->measureInit();
	}
      if (arg == "nomeasure")
	{
	  msFlag = 0;
	  ms = nil;
	}

      argv = tl argv;
    }
}

parsePath(path : string) : (string, string, string)
{
  logdir := Events->logdirName;
  log := Events->logName;

  (n, pathlst) := sys->tokenize(path, "/");
  if (pathlst != nil) {
    logdir = nil;
    for (; (tl pathlst) != nil; pathlst = tl pathlst)
      logdir = logdir+"/"+(hd pathlst);

    log = hd pathlst;
  }

  return(logdir, log, Events->logctlName);
}

