# pop3 functionality

implement Dispatch;

include "assclist.m";
	asl: AsscList;

include "dispatch.m";

include "sys.m";
	sys: Sys;

include "bufio.m";
	bufio: Bufio;

include "mailtool_code.m";
   cbu:Mailtool_code;

inputbuffer: Bufio->Iobuf;

user, pass, server, port
  : string;

srv: Sys->Connection;

cbuf: ref Bufio->Iobuf;

connected, signed_on, limit, used: int;
stat1 := 1;
debug: int;

ERROR: con "ERROR";
WARNING: con "WARNING";

init(): string
{
    sys = load Sys Sys->PATH;
    if (sys == nil) {
       return "pop3.init: could not load Sys";
    }
    asl = load AsscList AsscList->PATH;
    if (asl == nil) {
       return "pop3.init: could not load AsscList";
    }
    bufio = load Bufio Bufio->PATH;
    if (bufio == nil) {
       return "pop3.init: could not load Bufio";
    }
    cbu = load Mailtool_code Mailtool_code->PATH;
    if (cbu == nil) {
       return "pop3.init: could not load Mailtool_code";
    }
    cbu->init();
    user = pass = server = nil;
    port = "110";
    connected = used = signed_on = 0;
    debug = 0; # for now
    limit = -1; # default to unlimited.
    return nil;
}

# dispatch input fields in order of use:
#    "SERVER" -- name of server (only 1)
#       appears in response only if connect failed with err.
#    "PORT" -- port number (only needed if not 110)
#       appears in response only if connect failed with err.
#    "USER" -- name of user (only 1)
#       appears in response only if not recognized, with err.
#    "DOCONNECT" -- establish connection (value ignored).
#       appears in response only if connect failed with err.
#    "STAT" -- do a stat (nil for number of bytes)
#       echoed to response with "#messages #octets"
#    "LIMIT" -- byte limit for retrieval OF EACH MESSAGE (1)
#       returned in response if limit reached (negative limit=unlimited)
#    "TOPALL" -- get tops for all messages
#    "TOP" -- number of message to retrieve headers for.
#       response will include "TOP" --> "#msg: " + header text.
#    "RETRALL" -- retrieve all messages.
#    "RETR" -- number of message to retrieve (multiple)
#       response will include "RETR" --> "#msg: " +msg text w/header.
#    "DELETEALL" -- delete all messages
#    "DELE" -- number of message to delete (delete)
#       response will echo "DELE" --> #msg for successful deletes.
#    "QUIT" -- close connection, (value ignored).

# error returns:
#    

dispatch( Input: ref AsscList->Alist ): ref AsscList->Alist
{
    Alist: import asl;
    result := ref Alist(nil, nil, 0);
    directives:= Input.getitems("DEBUG");
    if (directives!=nil) {
       test:= hd directives;
       debug = 0;
       if (test=="ON") {
          debug = 1;
       }
    }
#    debug = 1; #### FOR TESTING!
    directives = Input.getitems("SERVER");
    if (directives!=nil) {
       server = hd directives;
       trace("server = "+server);
    }
    directives = Input.getitems("PORT");
    if (directives!=nil) {
       port = hd directives;
       trace("port = "+port);
    }
    directives = Input.getitems("USER");
    if (directives!=nil) {
       user = hd directives;
       trace("user = " + user);
    }
    directives = Input.getitems("PASS");
    if (directives!=nil) {
       pass = hd directives;
       trace("pass = " + pass);
    }
    directives = Input.getitems("LIMIT");
    if (directives!=nil) {
       limit = int (hd directives);
       trace("limit = " + (string limit));
    }
    directives = Input.getitems("DOCONNECT");
    if (directives!=nil) {
       trace("now connecting...");
      	#if(!(connected && stat1)) {
      	 #  stat1 =1; 
	   doconnect(result);
	#}
    }
    # none of the rest makes any sense unless we're connected now.
    if (!(signed_on && connected)) {
       connected = 0; 
	result.setitem(WARNING, "connection failed.");
       return result;
    }
    directives = Input.getitems("STAT");
    if (directives!=nil) {
       trace("now stating...");
       dostat(result);
    }
    directives = Input.getitems("LIST");
    if (directives!=nil) {
       trace("now listing...");
       dolist(result);
    }
    directives = Input.getitems("TOPALL");
    if (directives!=nil) {
       trace("doing topall...");
       dotopall(result);
    }
    directives = Input.getitems("TOP");
    if (directives!=nil) {
       trace("doing tops...");
       dotops( directives, result );
    }
    directives = Input.getitems("RETRALL");
    if (directives!=nil) {
       trace("doing retrall..");
       doretrall(result);
    }
    directives = Input.getitems("RETR");
    if (directives!=nil) {
       trace("doretrs...");
       doretrs(directives, result);
    }
    # check for errors, only execute deletes if no errors
    errs := result.getitems(ERROR);
    directives = Input.getitems("DELETEALL");
    if ((directives!=nil)) {
       if (errs == nil) {
          trace("dodeletall...");
          dodeleteall(result);
       } else {
          trace("deleteall ignored");
          result.setitem(WARNING, "DELETEALL ignored after errors");
       }
    }
    directives = Input.getitems("DELE");
    if (directives!=nil) {
       if (errs == nil) {
          trace("dodeletes...");
          dodeletes(directives, result);
       } else {
          trace("deletes ignored");
          result.setitem(WARNING, "DELETE ignored after errors");
       }
    }
    directives = Input.getitems("QUIT");
    if (directives!=nil) {
       trace("quitting...");
       doquit();
    }
    return result;
}

trace(s: string)
{
    if (debug) {
       sys->print("%s\n", s);
    }
}

doconnect(result: ref AsscList->Alist)
{
    fopen: import bufio;
    Alist: import asl;
    abort := 0;
    signed_on = 0;
    if (user==nil) {
       result.setitem(ERROR, "user required\n");
       abort = 1;
    }
    if (pass==nil) {
       result.setitem(ERROR, "password required\n");
       abort = 1;
    }
    if (server==nil) {
       result.setitem(ERROR, "server required\n");
       abort = 1;
    }
    if (abort) {
       return;
    }
    connected = 0;
    trace("connecting:");
    cstring := "tcp!" + server + "!" + port;
    trace(cstring);
    ok: int;
    (ok, srv) = sys->dial(cstring, nil);
    if (ok<0) {
       trace("connection failed");
       diagnostic := sys->sprint("Connection failed.\n");
       result.setitem(ERROR, diagnostic);
       return;
    }
    cbuf = fopen(srv.dfd, Bufio->ORDWR);
    if (cbuf==nil) {
       trace("fopen failed");
       result.setitem(ERROR, sys->sprint("fopen failed.\n"));
       return;
    }
    connected = 1;
    greeting := getoneline();
    if (greeting==nil) {
       result.setitem(ERROR, "no server greeting\n");
       return;
    }
    (success, message) := onelinecommand("USER "+user);
    if (!success) {
       result.setitem(ERROR, "user "+user+" not recognized ("+
                             message+")\n");
       return;
    }
    (success, message) = onelinecommand("PASS "+pass);
    if (!success) {
       result.setitem(ERROR, "The mail server responded:\n "+message+"\n");
       return;
    }
    signed_on = 1;
}

# One line POP command w/o newline. 
#   returns (success, response)
#
onelinecommand(cmd: string): (int, string)
{
    Iobuf: import bufio;
    if (!connected) {
       return (0, "Not connected");
    }
    cmd += "\r\n";
    trace("sending "+cmd);
    bcmd := (array of byte cmd);
    l := len bcmd;
    cbuf.write(bcmd, l);
    cbuf.flush();
    response:= getoneline();
    success:= 0;
    if ( ((len response)>1) && (response[0:1]=="+") ) {
       success=1;
    }
    response = trimCRLF(response);
    return (success, response);
}

getoneline(): string
{
    Iobuf: import bufio;
    if (!connected) {
       return nil;
    }
    response:= cbuf.gett("\n");
    trace("got "+response);
    return response;
}

trimCRLF(response: string): string
{
    if ( ((len response)>1) && (response[(len response)-1:] == "\n")) {
        response = response[: (len response)-1];
        if ( ((len response)>1) && (response[(len response)-1:] == "\r")) {
              response = response[: (len response)-1];
        }
    }
    return response;
}

dostat1(): int
{
   
    if (!connected) {
       return 0;
    }
    (success, response) := onelinecommand("STAT");
    if (success && ( (len response)>5) ) {
        #sys->print("respone %s", response);
       return 1;
    }
   else
        stat1 = 0;
        return 0;
}

dostat(result: ref AsscList->Alist)
{
    Alist: import asl;
    if (!signed_on) {
       stat1 = 0;
       result.setitem(ERROR, "cannot STAT: not connected\n");
       return;
    }
    (success, response) := onelinecommand("STAT");
    if (success && ( (len response)>5) ) {
       stat1 = 1;
       result.setitem("STAT", response[4:]);
       return;
    }
    stat1 = 0;
    result.setitem(ERROR, "STAT failed\n");
}

dolist(result: ref AsscList->Alist)
{
    Alist: import asl;
    if (!signed_on) {
       result.setitem(ERROR, "cannot LIST: not connected\n");
    }
    (success, response) := onelinecommand("LIST");
    if (!success) {
       result.setitem(ERROR, "LIST failed (" + response + ")\n");
       return;
    }
    # get the list lines "LIST" : "number nbytes"
    for (;;) {
        response = getoneline();
        response = trimCRLF(response);
        if (len(response) < 3) {
           if ((len response)>0 && (response[0:1] == ".")) {
              return;
           }
           result.setitem(ERROR, "invalid LIST line ("+response+")\n");
           return;
        }
        result.setitem("LIST", response);
    }
}

# return list of string of message numbers and nil
# or return nil and an error message.
getmsgnumbers(): (list of string, string)
{
   Alist: import asl;
   tempassc := ref Alist(nil,nil,0);
   dolist(tempassc);
   results := tempassc.getitems(ERROR);
   if (results != nil) {
      return (nil, hd results);
   }
   results = tempassc.getitems("LIST");
   out: list of string;
   out = nil;
   for (;;) {
       if (results==nil) { break; }
       this:= hd results;
       results = tl results;
       (dummy, thistok) := sys->tokenize(this, " ");
       out = (hd thistok) :: out;
   }
   return (out, nil);
}

dotopall(result: ref AsscList->Alist)
{
    Alist: import asl;
    (msgs, err):= getmsgnumbers();
    if (err!=nil) {
       result.setitem(ERROR, "can't retrieve message numbers ("+
                             err+")\n");
       return;
    }
    dotops(msgs, result);
}

dotops(msgs: list of string, result: ref AsscList->Alist)
{
    Alist: import asl;
    Iobuf: import bufio;
    msg : string;
    #success: int;
    for (;;) {
        if (msgs == nil) {
           return;
        }
        msg = hd msgs;
        msgs = tl msgs;
        (success, response) := onelinecommand("TOP "+msg+" 1");
        if (!success) {
          result.setitem(ERROR, "could not retrieve message top "+msg+
                                " ("+response+")\n");
          continue;
          # keep trying.
       }
       octets := 512; # just geussing.
       trace("top msg "+msg+" octets="+(string octets));
       body := getbody(octets, result);
       body_len:=len body;
       result.setitem("TOP", msg+":"+ trimCRLF(string cbu->cbuf_ubuf(body,body_len)));          
    }
}

doretrall(result: ref AsscList->Alist)
{
    Alist: import asl;
    (msgs, err):= getmsgnumbers();
    if (err!=nil) {
       result.setitem(ERROR, "can't retrieve message numbers ("+
                             err+")\n");
       return;
    }
    doretrs(msgs, result);
}

doretrs(msgs: list of string, result: ref AsscList->Alist)
{
   Alist: import asl;
   Iobuf: import bufio;
   msg : string;
   octets: int;
   #resplist: list of string;
   for (;;) {
       if (msgs == nil) {
          return;
       }
       msg = hd msgs;
       msgs = tl msgs;
       (success, response) := onelinecommand("RETR "+msg);
       if (!success) {
          result.setitem(ERROR, "could not retrieve message "+msg+
                                " ("+response+")\n");
          continue;
          # keep trying.
       }
       #(success, resplist) = sys->tokenize(response, " ");
       #if ((len resplist)<2) {
       #   result.setitem(ERROR, "bad RETR response fmt: "+response+"\n");
       #   return; # catastrophy.
       #}
       octets = 1024; #int (hd (tl resplist));
       body := getbody(octets, result);
       body_len:=len body;
       result.setitem("RETR", msg+":"+ trimCRLF(string cbu->cbuf_ubuf(body,body_len)));          
       #result.setitem("RETR", msg+":"+ body);
   }
}

#getbody(octets: int, result: ref AsscList->Alist): string
getbody(octets: int, result: ref AsscList->Alist): array of byte 

{
       Iobuf: import bufio;
       Alist: import asl;
       text := array[octets] of byte;
       hack := array[1] of byte;
       #success = cbuf.read(text, octets);
       limitreached := here:= count:= thisint:= 0;
       before:= last:= this:= "x"; # dummy
       byt: byte;
       for (;;) {
           # get a byte of the message.
           #if (count>=octets) { break; }
           thisint = cbuf.getb();
           if (thisint<0) {
              result.setitem(ERROR, "invalid message termination\n");
           }
           byt = byte thisint;
           # expand the buffer if needed.
           if (here>=(len text)) {
              ntext := array[ (len text)+512 ] of byte;
              ntext[0:] = text;
              text = ntext;
           }
           # store byt, only if limit not exceeded.
           if ((limit<=0) || (here<limit)) {
              text[here] = byt;
           } else {
             if (!limitreached) {
                limitreached = 1;
                trace("limit reached at "+ (string here) + "\n");
             }
           }
           # look for termination
           hack[0] = byt;
           (before, last, this) = 
             (last, this, string hack);
           #sys->print("%s", this);
           if ( ( (before=="\n") && (last == ".") &&
                  ( (this=="\r") || (this=="\n") ) )
                # || ( (limit>0) && (count>limit) ) 
              ) {
              trace("done at (" + before + last + this + ")");
              break;
           }
           # ignore second . in \n.. (as per rfc)
           if ( (before=="\n") && (last == ".") &&
                (this == ".") ) {
              continue; # clobber text[here] entry in next iteration.
           }
           # always count eoln as two bytes.
           if ((this=="\n") && (last!="\r")) { count++; }
           #sys->print("%d %s %d\n", here, before+last+this, octets);
           if ((limit<=0) || (here<=limit)) {
              here++;
           }
           count++;
       }
       if (this == "\r") {
          # nuke the newline
          byt = byte cbuf.getb();
       }

      return text[:here-2];

       # slice out the terminator and crlf
 
      # body := trimCRLF(string text[:here-2]);
      # return body;
}


dodeleteall(result: ref AsscList->Alist)
{
    Alist: import asl;
    (msgs, err):= getmsgnumbers();
    if (err!=nil) {
       result.setitem(ERROR, "can't retrieve message numbers ("+
                             err+")\n");
       return;
    }
    dodeletes(msgs, result);
}

dodeletes(msgs: list of string, result: ref AsscList->Alist)
{
    Alist: import asl;
    for (;;) {
        if (msgs==nil) {
           trace("done deleting");
           break;
        }
        msg := hd msgs;
        msgs = tl msgs;
        (success, response) := onelinecommand("DELE "+msg);
        if (!success) {
           result.setitem(ERROR, "could not delete message "+msg+
                                " ("+response+")\n");
           return; # give up.
        }
        result.setitem("DELE", msg);
    }
}

doquit()
{
   Iobuf: import bufio;
   (success, response) := onelinecommand("QUIT");
   cbuf.close();
   connected = signed_on = 0;
}

