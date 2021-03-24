###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1997 Lucent Technologies
###
### Written by J. Keane
###

### This module implements routines that interface the GUI to the mail
### manager.  These should ultimately be included in the GUI module, but
### are separate for the moment for clarity.

implement GUI_extras;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Context, Rect: import draw;

include "tk.m";
	tk: Tk;
	Toplevel: import tk;

include "wmlib.m";
	wmlib: Wmlib;

include "string.m";
	str: String;

include "assclist.m";
	asl: AsscList;

include "dispatch.m";
include "gdispatch.m";

include "compose.m";
	compose: Compose;
	Message: import compose;        
include "mailtool_gui.m";
     gui:Mailtool_GUI;

include "regex.m";
        regex: Regex;
	compile, execute, Re : import regex;

include "xlate.m";
	xlate : Xlate;
	substitute : import xlate;

include "mail-interface.m";

## Regular Expressions
CRRE, lcurlyRE, rcurlyRE : Re;

tkmsg: ref Toplevel;
context : ref Draw->Context;
alphaMonths : ref AsscList->Alist;
lastServerMessage := 0;                # Count of messages on server

init(ctxt: ref Draw->Context)
{
    Alist : import asl;
    
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;
#	tklib = load Tklib Tklib->PATH;
	str = load String String->PATH;
        regex = load Regex Regex->PATH;
        xlate = load Xlate Xlate->PATH;
        gui=load Mailtool_GUI Mailtool_GUI->PATH; 
	gui->init();

	context = ctxt;
#	tklib->init(ctxt);
	wmlib->init();
        xlate->init(nil);

	asl = load AsscList AsscList->PATH;

        alphaMonths = Alist.frompairs (("JAN" , "1") ::
				       ("FEB" , "2") ::
				       ("MAR" , "3") ::
				       ("APR" , "4") ::
				       ("MAY" , "5") ::
				       ("JUN" , "6") ::
				       ("JUL" , "7") ::
				       ("AUG" , "8") ::
				       ("SEP" , "9") ::
				       ("OCT" , "10") ::
				       ("NOV" , "11") ::
				       ("DEC" , "12") ::
				       nil);
    # Compile regular expressions
    CRRE = compile("",0);
    lcurlyRE = compile("{",0);
    rcurlyRE = compile("}",0);
}

#SDP#
# See if error or warning returned and put up dialog box
#do_errors(tktop : ref Toplevel, result : ref AsscList->Alist) : int {
#    Alist : import asl;
#    error := 0; 
#
#    errormsg := result.getitems("ERROR");
#    if (errormsg != nil) {
#	s := sys->sprint("Mail ERROR: %s", hd errormsg);
#	wmlib->dialog(tktop,  "error -fg red", "mail",
#          s, 0, "OK" :: nil);
#	sys->print("Mail: ERROR %s\n", hd errormsg);
#	error = 1;
#    }
#
#    warnmsg := result.getitems("WARNING");
#    if (warnmsg != nil) {
#	s := sys->sprint("Mail WARNING: %s", hd warnmsg);
#	wmlib->dialog(tktop,  "error -fg red", "mail",
#                 s, 0, "OK" :: nil);
#	sys->print("Mail: WARNING %s\n", hd warnmsg);
#    }
#
#    return error;
#}
#SDP#


# If possible, load configuration information from permanent storage
# Returns: good, user, passwd, pop_server, smtp_server, email_address
loadcfig(mmgr : GDispatch) :
         (int, string, string, string, string, string)
{
    Alist : import asl;

    request := Alist.frompairs
	(
	 (
	  ("LDCONFIG", "") ::
	  nil
	  )
	 );
    
    result := mmgr->dispatch(request);
    if (result.getitems("USERID") != nil) {
	user := hd result.getitems("USERID");
	passwd := hd result.getitems("PASSWORD");
	pop_server := hd result.getitems("POPSERVER");
	smtp_server := hd result.getitems("SMTPSERVER");
	email_address := hd result.getitems("EMAILADDR");
	return (1,user,passwd,pop_server,smtp_server,email_address);
    }
    else return (0,"", "", "", "", "");
}

# If possible, save configuration information to permanent storage
savecfig(mmgr : GDispatch,
	 user, passwd, pop_server, smtp_server, email_address : string)
{
    Alist : import asl;


    request := Alist.frompairs
	(
	 (
	  ("SVCONFIG", "") ::
	  ("USERID" , user) ::
	  ("PASSWORD" , passwd) ::
	  ("POPSERVER" , pop_server) ::
	  ("SMTPSERVER", smtp_server) ::
	  ("EMAILADDR" , email_address) ::
	  nil
	  )
	 );
    result := mmgr->dispatch(request);
    #SDP#do_errors(tktop, result);
    errors := result.getitems("ERROR");
    if (errors == nil) {
       #SDP#
       #wmlib->dialog(tktop,  "error -fg white", "mail",
       #     "Saved!", 0, "OK" :: nil);
       #SDP#
    }
}

# Send configuration information to mail manager
#
sendcfig(mmgr : GDispatch, headerWindow : int,
	 user : string, passwd : string, pop_server : string,
	 smtp_server : string, email_address : string) : int
{
    Alist : import asl;
    lastmsg : int;
    ### Do an initialization and return the number of messages in the file

    ### Initialization and request message count
    request := Alist.frompairs
	(
	 (
	  ("SETHWSIZE", string headerWindow) ::
	  ("USERID", user) ::
	  ("PASSWORD", passwd) ::
	  ("POPSERVER" , pop_server) ::
	  ("SMTPSERVER" , smtp_server) ::
	  ("EMAILADDR" , email_address) ::
	  ("SELECTBOX", "Inbox") ::
	  ("MSGCOUNT" , "") ::
	  nil
	  )
	 );
    result := mmgr->dispatch(request);
    #SDP#
    errormsg := result.getitems("ERROR");
    if (errormsg != nil) return 0;
    #if (do_errors(tktop, result) != 0) return 0;
    #SDP#
    else                        lastmsg = int (hd result.getitems("MSGCOUNT"));
    lastServerMessage = lastmsg;

    if (lastmsg == 0) {
	#SDP#do_errors(tktop, Alist.frompairs(("WARNING" , "No mail on server")::nil));
	return 0;
    }

    return lastmsg;
}		  


# Get the headers from the mail server
# Returns message number of the first header and the list of headers
fetchheaders(mmgr : GDispatch,
	     filename: string, lastmsg : int) : (int, int, list of ref Header,string)
{
    Alist : import asl;
    headerList : list of ref Header;

    # Pick up the headers from the server.  Headers will be returned
    # in reverse message number order.
    request := Alist.frompairs
	(
	 (
	  ("SELECTBOX", filename) ::
	  ("FETCHHDRS", string lastmsg) ::
	  nil
	  )
	 );
    result := mmgr->dispatch(request);
    #SDP#
    errormsg := result.getitems("ERROR");
    if (errormsg != nil) return (0,0,nil,""+hd errormsg); 
    #if (do_errors(tktop, result) != 0) return (0,0,nil);
    #SDP#
    headers := result.getitems("TOP");

    ### Build the local header list (reverses header order)
    headerList = nil;
    while (headers != nil) {
	header := hd headers;
	headerList = ref Header(0, header, "") :: headerList;
	headers = tl headers;
    }

#    dump_alist("result",result);

    firstheader := int hd result.getitems("FETCHHDRS");
    lastheader := int hd result.getitems("LASTHDR");
    lastServerMessage = lastheader;
    renumber_headers(headerList, firstheader);

    return (firstheader, lastheader, headerList,"");
}

### Renumber headers after a change in the list (fetch, delete)
renumber_headers(headerList : list of ref Header, start : int)
{
    car : ref Header;
    i := start;
    while (headerList != nil) {
	car = hd headerList;
	car.msgnum = i;
	i++;
	headerList = tl headerList;
    }
}
    
# Load headers into the listbox for selection
loadheaders(tktop : ref Toplevel, mmgr : GDispatch,
	    nmesg : int, hdrlst : list of ref Header, listboxWidth : int) : (int, int,string)
{
    foo,saveerr : string;
    err := "";
    ### Remove current messages from listbox
#    tk->cmd(tktop, ".listing.p delete 0 "+string (nmesg-1));
    tk->cmd(tktop, ".listing.p delete 0 end");
    cmesg := -1;
    nmesg = len hdrlst;

    # Parse the headers into items suitable for entry into the listbox
    for ( ;hdrlst != nil; hdrlst = tl hdrlst) {
#        if (hdrlst==nil) {break;
         #}
	(foo,err) = format_header(mmgr,
			     (hd hdrlst).fullHeader, (hd hdrlst).msgnum,
			     listboxWidth, ".");
	if (err != "")
		saveerr = err;
#	sys->print("Formatted header: %s\n", foo);
#	tk->cmd(tktop, ".listing.p insert end { }");
	tk->cmd(tktop, ".listing.p insert end {" + foo + "}");
	(hd hdrlst).dispHeader = foo;
	#if ((hd (tl hdrlst)) == nil)
	 #  tk->cmd(tktop, ".listing.p insert end { }"); 
#	hdrlst = tl hdrlst;
    }
#    if (hdrlst == nil)
#        tk->cmd(tktop, ".listing.p insert end { }");
    tk->cmd(tktop, "update");
    return(nmesg, cmesg, saveerr);
}

### This routine is not as smart as it ought to be about sizing.  Ideally,
### it should adjust all of the fields proportionally as the display width
### of the list box increases, up to a maximum, with the subject field
### being allowed to fill.
###
### Do we need to provide more robustness on field testing, or can
### we be sure that formats will be as assumed here?

format_header (mmgr : GDispatch,
	       header : string, number : int,
	       listboxWidth : int, status : string) : (string,string)
{
    Alist : import asl;
    hdrstring : string;
    date, subj, sender : string;
    
    minwidth : con 32;
    fill : con "                                                                                                                                                                                                       ";
    
    # First, parse the header into fields
    request := Alist.frompairs
	((("PARSEHDR", header) :: nil));
    result := mmgr->dispatch(request);
    #SDP#
    errormsg := result.getitems("ERROR");
    if (errormsg != nil) return ("","Mail Error\n"+hd errormsg);
    #if (do_errors(tktop, result) != 0) return ("");
    #SDP#

    # Extract date "DDD, dd mmm yyyy" (simplistic parse for now)
    datelist := result.getitems("Date");
    if (datelist == nil) date = "--/--";
    else                 date = get_mmyy(hd datelist);

    # Extract as much of subject as possible
    subjlist := result.getitems("Subject");
    if (subjlist == nil) subj = "(No subject)";
    else                 subj = hd subjlist;

    # Stick on some of the sender ID at the end
    senderlist := result.getitems("From");
    if (senderlist == nil) sender = " ";
    else                   sender = hd senderlist;

    # Add status information on the end
    msgstat := result.getitems("Status");
    if (msgstat != nil) {
	case hd msgstat {
	    "" or "U" or " U"=>	status = "N";
	    "RO" or " RO" =>	status = "R";
	    "O" or " O" =>	status = "U";
	}
    }
    #if (listboxWidth >= minwidth) {
#	subjlen := listboxWidth - (12 + 18 + 3);
	hdrstring = sys->sprint("%4d %s %-5.5s %-30.30s   %-50.50s ", number,status, date, sender, subj);
  
#    }
#    else {
#	subjlen :=  minwidth - (12 + 9 + 2);
#	hdrstring = sys->sprint("%4d %-5.5s %-*.*s %-8.8s %s", number, date,
#				subjlen, subjlen, subj, sender, status);
#    
#    }

    return (hdrstring,"");
}


## Extract numeric month and year from the date field
get_mmyy (indate : string) : string
{
    Alist : import asl;
    day, month : string;
    ## For now, assume date will have "dd mmm" somewhere in the string

    indate = str->toupper(indate);
    (count, tokens) := sys->tokenize(indate, " ,-/");

    # Try to pick the day and month out of the tokens
    tknlst := tokens;
    month = "";
    dd := "--";
    while (tknlst != nil) {
	car := hd tknlst;
	mm := alphaMonths.getitems(car);
	if (mm != nil) {
	    month = hd mm;
	    day = dd;
	    break;}
	else dd = car;
	tknlst = tl tknlst;
    }
    if (month != "") return month + "/" + day;
    else           return "--/--";
}

short_header(header : ref Header,  mmgr : GDispatch) : (string,string)
{
    Alist : import asl;
    date := "";
    subject := "";
    from := "";

    # First, parse the header into fields
    request := Alist.frompairs
	((("PARSEHDR", header.fullHeader) :: nil));

    result := mmgr->dispatch(request);
    #SDP#
    errormsg := result.getitems("ERROR");
    if (errormsg != nil) return ("","Mail Error\n"+hd errormsg);
    #if (do_errors(tktop, result) != 0) return "";
    #SDP#

    ### Now extract the date, subject, and from fields
    field := result.getitems("From");
    if (field != nil) from = hd field;
    field = result.getitems("Date");
    if (field != nil) date = hd field;
    field = result.getitems("Subject");
    if (field != nil) subject = hd field;

    return ("From: " + from + "\n" +
	   "Date: " + date + "\n" +
	   "Subject: " + subject + "\n\n","");
}

# Strip ^M characters and quote {}s for Tk
remove_crs (str : string) : string
{
    outstring := substitute(str, CRRE, "");
    outstring = substitute(outstring, lcurlyRE, "\\{");
    outstring = substitute(outstring, rcurlyRE, "\\}");
    return outstring;
}

fetch_body(cmesg : int, mmgr : GDispatch) : string
{
    Alist : import asl;

    # Get the body of the message
    request := Alist.frompairs
	((("GETBODY", string cmesg) :: nil));

    result := mmgr->dispatch(request);
    errormsg := result.getitems("ERROR");
    if (errormsg != nil) return "";
    #SDP#if (do_errors(tktop, result) != 0) return "";
    else	                       body := hd result.getitems("GETBODY");

    return remove_crs(body);
}

get_msgargs (header : ref Header,  mmgr : GDispatch) : ref Message
{
    #sys->print("get_msgargs\n");
    Alist : import asl;
    cc := "";
    retn := "";
    subject := "";
    contentype := "";
    mimeversion := ""; 
    msg := ref Message("", "", "", "", "", "", "");

    # First, parse the header into fields
    request := Alist.frompairs
	((("PARSEHDR", header.fullHeader) :: nil));

    result := mmgr->dispatch(request);
    errormsg := result.getitems("ERROR");
    if (errormsg != nil) return msg;
    #if (do_errors(tktop, result) != 0) return msg;

    ### Now extract Return-path, CC
    field := result.getitems("Return-Path");
    if (field != nil) retn = hd field;
    else {
	field = result.getitems("From");
	if (field != nil) retn = hd field;
    }

    field = result.getitems("CC");
    if (field != nil) cc = hd field;
    field = result.getitems("Cc");
    if (field != nil) cc = hd field;

    ### add all from "To" to cc list also
    field = result.getitems("To");
    if (field != nil) {
       others := hd field;
       if (cc!="") {
          cc = cc + ", " + others;
       } else {
          cc = others;
       }
    }

    field = result.getitems("Subject");
    if (field != nil) subject = hd field;
    
    field = result.getitems("Content-Type");
    if (field != nil) contentype = hd field;
    
    field = result.getitems("Mime-Version");
    if (field != nil) mimeversion = hd field;
    
   # sys->print("retn: (%s) cc: (%s) subject: (%s)", retn, cc, subject);
    msg.mailto = retn;
    msg.cc = cc;
    msg.subject = subject;
    msg.contentype = contentype;
    msg.mimeversion = mimeversion;
    return msg;
}

do_connect (ctxt : ref Draw->Context, mmgr : GDispatch) : (int,string)
{
    Alist : import asl;

    ctxt=ctxt;

    # Send it!
    request := Alist.frompairs
	((("SEND", "") :: nil));

    result := mmgr->dispatch(request);
    #SDP#
    errormsg := result.getitems("ERROR");
    if (errormsg != nil)
    {
        if(hd errormsg == gui->OPERATION_CANCELLED)
                return (SENDERROR,"Sending mail is cancelled!");
        else
                return (SENDERROR,"Mail Error\n"+hd errormsg);
    }

    return (SENDCOMPLETE,"");
}

# Returns updated list of formatted headers 
delete_msg (ctxt : ref Draw->Context, mmgr : GDispatch,
	    cmesg, msgnum : int, headers : list of ref Header) : list of ref Header
{
    Alist : import asl;
    ctxt=ctxt;

    # Send it!
    request := Alist.frompairs
	((("DELETE", string msgnum) :: nil));

    result := mmgr->dispatch(request);
    errormsg := result.getitems("ERROR");
    if (errormsg != nil) return headers;
    #SDP#if (do_errors(tktop, result) != 0) return headers;

    # Remove it from the list of headers given
    hlist := headers;
    newlist : list of ref Header;
    newlist = nil;
    
    i := 1;
    while (hlist != nil) {
	if (i != cmesg) newlist = hd hlist :: newlist;
	i++;
	hlist = tl hlist;
    }

    return hl_reverse(newlist);
}

# Reverse header list
hl_reverse (headers : list of ref Header) : list of ref Header
{
    revlist : list of ref Header;
    revlist = nil;
    
    while (headers != nil) {
	revlist = hd headers :: revlist;
	headers = tl headers;
    }

    return revlist;
}

# dump_alist(header:string, result: ref AsscList->Alist)
# {
#    Alist: import asl;
#    result.first();
#    for (;;) {
#        ((a,b), i) := result.thispair();
#        if (!i) { break; }
#        result.next();
#        sys->print(" %s: (%s) : (%s)\n", header, a, b);
#    }
# }

