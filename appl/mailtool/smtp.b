# smtp functionality

implement Dispatch;

include "assclist.m";
	asl: AsscList;

include "dispatch.m";

include "sys.m";
	sys: Sys;

include "bufio.m";
	bufio: Bufio;

include "mailtool_code.m";
   cbu: Mailtool_code;

include "string.m";
	str: String;

inputbuffer: Bufio->Iobuf;

server, port
  : string;

send_to, cc, hdr: list of string;
contentype, mimeversion, from, subject: string;
body: string;
send_buffer: array of byte;

srv: Sys->Connection;

cbuf: ref Bufio->Iobuf;

connected, signed_on: int;

debug: int;

ERROR: con "ERROR";
WARNING: con "WARNING";

init(): string
{
    sys = load Sys Sys->PATH;
    if (sys == nil) {
       return "smtp.init: could not load Sys";
    }
    asl = load AsscList AsscList->PATH;
    if (asl == nil) {
       return "smtp.init: could not load AsscList";
    }
    bufio = load Bufio Bufio->PATH;
    if (bufio == nil) {
       return "smtp.init: could not load Bufio";
    }
    str = load String String->PATH;
    if (bufio == nil) {
       return "smtp.init: could not load String";
    }

    cbu = load Mailtool_code Mailtool_code->PATH;
    if (cbu == nil) {
       return "smtp.init: could not load Mailtool_code";
    }
    cbu->init();

    server = nil;
    port = "25";
    connected = signed_on = 0;
    clear_msg_state();
    debug = 0; # for now
    send_buffer = nil;
    return nil;
}

clear_msg_state()
{
    send_to = cc = hdr = nil;
    from = subject = body = "";
}

# dispatch input fields in order of use:
#
# server connection related:
#    "SERVER" -- name of server (only 1)
#       appears in response only if connect failed with err.
#    "PORT" -- port number (only needed if not 25)
#       appears in response only if connect failed with err.
#    "DOCONNECT" -- establish connection (value ignored).
#       appears in response only if connect failed with err.
#    "QUIT" -- close connection, (value ignored).
#
# message related.  Each message requires its own dispatch call.
#    "FROM" -- address/name of sender (only 1)
#       appears in response only if not recognized, with err.
#    "TO" -- recipient (multiples allowed, 1 required)
#    "CC" -- Carbon copy recipient
#    "SUBJECT" -- subject text (optional, max 1)
#    "HDR" -- additional header lines, not checked for validity.
#      NOTE: FROM, TO, AND CC HEADERS ARE ADDED HERE, MAYBE THEY
#        SHOULD BE ADDED ELSEWHERE. (?)
#      any ending \r\n will be stripped.
#    "BODY" -- mail (text) body.  just one.
#      as per rfc the body will be broken into lines on \n or \r\n
#      and any leading . will be replaced with .. to prevent
#      protocol confusion.  Lines will always be terminated with
#      \r\n.  The body should not include rfc822 headers.
#    "SEND" -- send the message.
#    "CLEAR" -- forget all message related state (after send).
#
# each message sent is prefixed by a reset and a reestablishment
# of all send parameters.
#    

# error returns:

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
#    debug = 1; ### TESTING!
      
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
    directives = Input.getitems("DOCONNECT");
    if (directives!=nil) {
       trace("now connecting...");
       doconnect(result);
    }
    directives = Input.getitems("FROM");
    if (directives!=nil) {
       from = hd directives;
    }
    directives = Input.getitems("TO");
    send_to = string_list_add(directives, send_to);
    directives = Input.getitems("CC");
    cc = string_list_add(directives, cc);
    directives = Input.getitems("SUBJECT");
    if (directives!=nil) {
       subject = hd directives;
    }
    directives = Input.getitems("CONTENT-TYPE");
    if (directives!=nil) {
	contentype = hd directives;
    } 
    directives = Input.getitems("MIME-VERSION");

    if (directives!=nil)
       mimeversion = hd directives;
    else mimeversion = "1.0"; 
    directives = Input.getitems("HDR");
    hdr = string_list_add(directives, hdr);
    directives = Input.getitems("BODY");
    if (directives!=nil) {
       body = hd directives;
    }
    # none of the rest makes any sense unless we're connected now.
    if (!connected) {
       result.setitem(WARNING, "no connection: check server info");
       return result;
    }
    directives = Input.getitems("SEND");
    if (directives!=nil) {
       trace("do send...");
       do_send(result);
    }
    directives = Input.getitems("QUIT");
    if (directives!=nil) {
       trace("quitting...");
       doquit();
    }
    directives = Input.getitems("CLEAR");
    if (directives!=nil) {
       clear_msg_state();
    }
    directives = Input.getitems("CHOKE");
    if (directives!=nil) {
       choke();
    }
    return result;
}

string_list_add(a, b: list of string): list of string
{
    if (a == nil) {
       return b;
    }
    return string_list_add( (tl a), (hd a) :: b );
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
       diagnostic := sys->sprint("Connection failed.\n%r\n");
       result.setitem(ERROR, diagnostic);
       return;
    }
    cbuf = fopen(srv.dfd, Bufio->ORDWR);
    if (cbuf==nil) {
       trace("fopen failed");
       result.setitem(ERROR, sys->sprint("fopen failed.\n%r\n"));
       return;
    }
    connected = 1;
    (success, greeting) := getresponse();
    if (!success) {
       result.setitem(ERROR, "Server greeting failed\n");
       return;
    }
}

# One line SMTP command w/o newline. 
#   returns (success, response)
#
onelinecommand(cmd: string): (int, string)
{
    Iobuf: import bufio;
    (test, error) := sendline(cmd);
    if (!test) {
       return (0, error);
    }
    return getresponse();
}

dostat1(): int
{
return 0;
}

sendline(cmd: string): (int, string)
{
    

    Iobuf: import bufio;
    if (!connected) {
       return (0, "Not connected");
    }
    cmd += "\r\n";
    trace("sending "+cmd);
    bcmd := (array of byte cmd);
    l := len bcmd;

   (nindex, ncmd):=cbu->convertUTFtoGB(l,bcmd);

    #cbuf.write(bcmd, l);

    cbuf.write(ncmd, nindex);
    cbuf.flush();
    return (1, "");
}


getresponse(): (int, string)
{
    response:= getoneline();
    success:= 0;
    if ((len response)>1) {
       fpos:= response[0:1];
       if ((fpos == "1") || (fpos == "2") || (fpos == "3")) {
          success = 1; # positive reply
       }
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

do_send(result: ref AsscList->Alist)
{
    Alist: import asl;
    splitl: import str;
    if (!connected) {
       result.setitem(ERROR, "Not connected\n");
       return;
    }
    (success, response):= onelinecommand("RSET");
    if (!success) {
       result.setitem(ERROR, "Could not RSET (" +response+ ")\n");
       return;
    }
    if (from == "") {
       result.setitem(ERROR, "No sender specified\n");
       return;
    }
    # do HELO domain is present
    (username, domain) := splitl(from, "@");
    if ( (len domain) > 1 ) {
       trace("domain: " + domain);
       domain = domain[1:];
       (success, response) = onelinecommand("HELO "+domain);
       if (!success) {
          result.setitem(ERROR, "HELO failed ("+response+")\n");
          return;
       }
    }
    (success, response) = onelinecommand("MAIL FROM:<"+from+">");
    if (!success) {
        result.setitem(ERROR,
                       "Sender not accepted ("+from+") "+response+"\n");
        return;
    }
    if (send_to == nil) {
       result.setitem(ERROR, "No recipient specified\n");
       return;
    }
    allrcpt:= string_list_add(send_to, cc);
    rcpt: string;
    abort:= 0;
    for (;;) {
        if (allrcpt==nil) {
           break;
        }
        rcpt = hd allrcpt;
        allrcpt = tl allrcpt;
        (success, response) = onelinecommand("RCPT TO:<"+rcpt+">");
        if (!success) {
           result.setitem(ERROR,
                          "Recipient not accepted ("+rcpt+") "+response+"\n");
           abort = 1;
        }
    }
    if (abort) {
       result.setitem(ERROR, "Send aborted: please check recipient addresses.\n");
       #return;
    }
    (success, response) = onelinecommand("DATA");
    if (!success) {
       trace("DATA command problem " + response);
       result.setitem(ERROR, "DATA command not accepted ("+response+")\n");
       return;
    }
    success = send_headers(result);
    if (!success) { 
       trace("problem sending headers");
       return; 
    }
    (success, response) = sendline("");
    if (!success) {
       trace("problem sending separator");
       result.setitem(ERROR, "error sending header/body separation: "+response+"\n");
       return;
    }
    success = send_quoted_body(result);
    if (!success) { 
       trace("problem sending body");
       return;
    }
    (success, response) = onelinecommand(".");
    if (!success) {
       trace("problem with message termination");
       result.setitem(ERROR,
                      "Termination of message body failed.\n");
       return;
    }
}

send_headers(result: ref AsscList->Alist): int
{
    splitl: import str;
    Alist: import asl;
    Iobuf: import bufio;
    (test, msg):= sendline("From: <" + from +">");
    if (!test) {
       result.setitem(ERROR, "error sending from line\n");
       return 0;
    }
    test = send_recipients("To", send_to, result);
    if (!test) { 
       result.setitem(ERROR, "Please check TO addresses.\n");
       return 0;
     }
    test = send_recipients("Cc", cc, result);
    if (!test) { 
       result.setitem(ERROR, "Please check CC addresses.\n");
       return 0; 
    }
    # XXX this should be generalized somewhere for multiline...
    # (not necessarily here, for now assume subject is formatted right).
    (test, msg) = sendline("Subject: " + subject);
    if (!test) {
       result.setitem(ERROR, "subject not accepted.\n");
       return 0;
    }
    (test, msg) = sendline("Content-Type: " + contentype);
    if (!test) {
       result.setitem(ERROR, "no content tpye. \n");
       return 0;
    }
    (test, msg) = sendline("Mime-Version: " + mimeversion);
     if (!test) {
        result.setitem(ERROR, "no version numeber. \n");
        return 0;
     } 
    # XXX also, multiline should be handled/preprocessed somewhere.
    headers := hdr;
    for (;;) {
        if (headers == nil) { break; }
        thisheader := hd headers;
        headers = tl headers;
        (test, msg) = sendline(thisheader);
        if (!test) {
           result.setitem(ERROR, "error sending header: "+thisheader+"\n");
           return 0;
        }
    }
    return 1;
}

send_recipients(prefix: string, slist: list of string,
                result: ref AsscList->Alist): int
{
    Alist: import asl;
    if (slist == nil) { return 1; }
    rcpt:= hd slist;
    slist = tl slist;
    sep:= ",";
    if (slist == nil) {
       sep = "";
    }
    (test, message):= sendline(prefix + ": <" + rcpt + ">" + sep);
    if (!test) {
       result.setitem(ERROR, 
         "error sending "+prefix+" header: "+rcpt+ " ("+message+")\n");
       return 0;
    }
    for (;;) {
        if (slist == nil) { return 1; }
        rcpt = hd slist;
        slist = tl slist; 
        if (slist == nil) {
            sep = "";
        }
        (test, message) = sendline("\t<" + rcpt + ">"+sep);
        if (!test) {
            result.setitem(ERROR, "error sending "+prefix+" header: "+rcpt+"\n");
            return 0;
        }
    }
    return 1;
}

send_quoted_body(result: ref AsscList->Alist): int
{
    # handle . lines correctly.
    Alist: import asl;
    splitl: import str;
    text := body;
    #trace("now sending body: "+text);
    line: string;
    for (;;) {
        if (text == "") { break; }
        #trace("text len = " + (string (len text)) + ":" + text);
        (line, nil) = splitl(text, "\n");
        ln := len line;
        if ( (len text) > ln ) {
           line = text[: ln+1 ];
        }
        #trace("line = ("+line+")\n");
        text = text[(len line):];
        line = trimCRLF(line);
        if ( ((len line) > 0) && (line[:1] == ".") ) {
           line = "." + line;
        }
        (test, message) := sendline(line);
        if (!test) { return 0; }
    }
    return 1;
}

doquit()
{
   Iobuf: import bufio;
   (success, response) := sendline("QUIT");
   # ignore response.
   cbuf.close();
   cbuf = nil;
   connected = signed_on = 0;
}

choke()
{
    cbuf = nil;
}
