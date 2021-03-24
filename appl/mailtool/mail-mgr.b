###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1997 Lucent Technologies
###
###

### This module implements the mail management functionality.  All interface
### is performed through the "dispatch" call, which allows an object-oriented
### flavor to the interactions.
###

### NOTE: message retrieval is arbitrarily limited to MAXMESSAGE bytes
###  to prevent overloading here.

implement GDispatch;

include "assclist.m";
	asl: AsscList;

include "dispatch.m";
        pop3: Dispatch;
	smtp: Dispatch;

include "sys.m";
	sys: Sys;

include "bufio.m";
	bufio: Bufio;

include "regex.m";
        regex: Regex;

include "daytime.m";
	daytime: Daytime;
	Tm: import daytime;

include "draw.m";
	draw: Draw;

include "tk.m";
	tk: Tk;

include "wmlib.m";
	wmlib: Wmlib;

include "string.m";
	S: String;

include "gdispatch.m";


include "mailtool_gui.m";
     gui: Mailtool_GUI;


include "mailtool_tk.m";
    gui_tk: Mailtool_Tk;

# stuff for storing the graphics context... (arg...)
ctx: ref Draw->Context;
store_the_graphics_context_please(the_ctx: ref Draw->Context)
{
   ctx = the_ctx;
}

inputbuffer: Bufio->Iobuf;
cbuf: ref Bufio->Iobuf;

srv: Sys->Connection;

debug: int;
counter : string;
MAXMESSAGE: con "20000";

ERROR: con "ERROR";
WARNING: con "WARNING";
PUTCONTYPE: con "CONTENTYPE";
PUTVERSION: con "MIMEVERSION";
SETHWSIZE: con "SETHWSIZE";
GETSTATE: con "GETSTATE";
SELECTBOX: con "SELECTBOX";
CFVARNAMES: con "CFVARNAMES";
CFVARVALS: con "CFVARVALS";
POPSERVER: con "POPSERVER";
SMTPSERVER: con "SMTPSERVER";
USERID: con "USERID";
PASSWORD: con "PASSWORD";
EMAILADDR: con "EMAILADDR";
FETCHHDRS: con "FETCHHDRS";
LASTHDR: con "LASTHDR";
PARSEHDR: con "PARSEHDR";
MSGCOUNT: con "MSGCOUNT";
CHECKOUTBOX: con "CHECKOUTBOX";
LDCONFIG: con "LDCONFIG";
SVCONFIG: con "SVCONFIG";
GETBODY: con "GETBODY";
PUTCC: con "PUTCC"; 
PUTTO: con "PUTTO";
PUTSUBJECT: con "PUTSUBJECT";
PUTBODY: con "PUTBODY";
PUTMESSAGE: con "PUTMESSAGE";
SEND: con "SEND";
DELETE: con "DELETE";

DEFLIMIT: con "32767";
newmail : int;
beg : int;
en :int;
### Constant regular expression patterns
MSGHDR: con "\n\n";
rxMSGHDR : Regex->Re;
ADDRESS: con "<[^>]+>";
rxADDRESS: Regex->Re;
rxUSID: Regex->Re;
###
### Module State variables
###

HWSize := 20;                      # Header window size
HWStart := 0;                      # First header number
HWEnd := 0;                        # Last header number
CurrentBox := "";                  # Current mailbox
InBoxName := "InBox";              # Name of the inbox
OutBoxName := "OutBox";            # Name of the outbox
DelBoxName : string;               # No trash box
SentBoxName : string;              # No sent box
ConfigVariables : ref AsscList->Alist;  # Extra configuration variables
POPServer := "";                   # POP-3 server name
SMTPServer := "";                  # SMTP server name
UserID := "";                      # Name of the user
Password := "";                    # Password
EmailAddr := "";                   # Return address
ExplicitConn := "TRUE";            # Explicit server connection request required
PermStore := "FALSE";              # No persistent storage available
LastMessage := 0;                  # Last known message is 0
outmsg_to := "";                   # Addressees of outgoing message
outmsg_cc := "";                   # CC addressees of outgoing message
outmsg_subject := "";              # Subject of outgoing message
outmsg_body := "";                 # Body of outgoing message
lastmsg :=0; 
contentype := "";
mimeversion := "";

###

###
### Mail manager handles the outbox
###

Outmessage : adt {
    cc : string;
    addr : string;
    subject : string;
    contentype : string;
    mimeversion : string; 
    header : string;
    body : string;
};

Outbox : list of ref Outmessage;

MONTHS := array [] of {"Jan" , "Feb" , "Mar" , "Apr" , "May" , "Jun" ,
		       "Jul" , "Aug" , "Sep" , "Oct" , "Nov" , "Dec"};

init(): string
{
#
# Load basics
#
    sys = load Sys Sys->PATH;
    if (sys == nil) {
       return "mailmgr.init: could not load Sys";
    }
    regex = load Regex Regex->PATH;
    if (regex == nil) {
       return "mailmgr.init: could not load Regex";
    }
    daytime = load Daytime Daytime->PATH;
    if (daytime == nil) {
       return "mailmgr.init: could not load Daytime";
    }
    asl = load AsscList AsscList->PATH;
    if (asl == nil) {
       return "mailmgr.init: could not load AsscList";
    }
    bufio = load Bufio Bufio->PATH;
    if (bufio == nil) {
       return "mailmgr.init: could not load Bufio";
    }

   gui = load Mailtool_GUI Mailtool_GUI->PATH;
   gui->init();


   gui_tk = load Mailtool_Tk Mailtool_Tk->PATH;
   gui_tk->init();
#
# Load and init pop3
#
    pop3 = load Dispatch Dispatch->POP3PATH;
    if (pop3 == nil) {
       return "mailmgr.init: could not load pop3";
    }
    test := pop3->init();
    if (test!=nil) {
	sys->print("pop.init failed: %s", test);
    }
    Alist: import asl;
    dummy := pop3->dispatch( 
              Alist.frompairs( ( ("LIMIT", MAXMESSAGE) :: nil ) ) );
    # presume success.

#
# Load and init smtp
#
    smtp = load Dispatch Dispatch->SMTPPATH;
    test = smtp->init();
    if (test!=nil) {
	sys->print("smtp.init failed: %s", test);
    }

    # Extra configuration variables
    ConfigVariables = ref AsscList->Alist(nil, nil, 0);

    # Advanced options
    debug = 0; # for now

    # Compile regular expressions
    # Note: the second argument to compile does not appear in the documentation.
    #       Examination of regex.b leads us to suspect that when set to 1 it
    #       changes the behavior of paren matching to match only corresponding
    #       pairs.  Set to 0 to get previously documented behavior. JEK

    rxMSGHDR = regex->compile(MSGHDR,0);
    rxADDRESS = regex->compile(ADDRESS,0);
    
    return nil;
}

## Dispatch input fields in order of use:
##
## All functions called via dispatch return a results Alist.  If the
## function called returned an error, the function name will appear
## in the Alist, otherwise only return values specified below will
## appear.
##
## "GETSTATE" -- return the value of a state variable
##      Response contains the pair (VarName . value)
##
## State variables:
## 
##   Variable     Description                                    Type
##   ---------------------------------------------------------------------------
##   ExplicitConn Manager only connects to POP-3 or SMTP server  stringTF
##                on explicit CONNECT request.
##   PermStore    Mail manager can handle permanent              stringTF
##                storage requests (e.g. folder out).
##   LastMessage  Highest numbered message known                 int
##   HWSize       Number of headers to retain in memory (if      int
##                negative, get all headers.
##   HWStart      Lowest header number of current header window  int
##   HWEnd        Highest header number of current header window int
##   CurrentBox   Name of current mailbox                        string
##   InBoxName    Name of inbox                                  string
##   OutBoxName   Name of outbox                                 string
##   DelBoxName   Name of deleted messages box                   string
##   SentBoxName  Name of sent messages box                      string
##   BoxNames     List of mailboxes supported                    stringList
##   ConfigVars   List of configuration state vars (other than   stringList
##                POPServer, SMTPServer, UserID, Password,
##                EmailAddr)
##   POPServer    Name of the POP3 server to use                 string
##   SMTPServer   Name of the SMTP server to use                 string
##   UserID       Username of the user                           string
##   Password     Password of the user                           string
##   EmailAddr    Return address                                 string
##
##
## "MSGCOUNT" -- Return count of messages on the POP3 server.
##               (or current mbox).
##                 Return ("MSGCOUNT" , count)
##
## "CHECKOUTBOX" -- Check the outbox for unsent messages.
##                 Return ("EMPTY", 0) of ("FULL", count)
##
## "FETCHHDRS" -- Get HWSize headers from the server, ENDING with the header
##                number specified.  Returns headers as the list "TOP", plus
##                (FETCHHDRS , "<start-hdr-number>")
##
## "PARSEHDR" -- Parse a single header into fields.  Returns keywords (less
##               colons) and their associated values in the form:
##            (("PARSEHDR" . error) +
##               ("From" . "")("To" . "")("Cc" . "")("Date" . "")
##               ("Time" . "")("Message-ID" . "")("Subject" . "")
##               ... ("Etc" . ""))
##               *** ONLY 1 PER DISPATCH. ***
## 
## "PARSEBODY" -- Parse the body of the message for html information.  Returns
##            (("HTMLStart" . "offset")(HTMLEnd . "offset")
##             ("HTMLStart" . "")(HTMLEnd . "") ...)
##
## "GETBODY" -- Returns the message body for message number in current box 
##              ("Body" . body)
##
## "DELETE" -- Delete message number
##
## "SEND" -- Send messages in outbox (and delete)
##
## "EXPUNGE" -- Remove deleted messages in trash (if supported)
##
## "LDCONFIG" -- Loads the saved configuration information into the mail manager.
##               Returns error if unsupported.
##
## "SVCONFIG" -- Saves configuration information to persistent storage.  Returns
##               error if unsupported.
##
## "POPSERVER" -- Change value of the POP-3 server name
## "SMTPSERVER" -- Change value of the SMTP server name
## "USERID" -- Change value of the user ID
## "PASSWORD" -- Change value of the user password
## "EMAILADDR" -- Change value of the return email address
##
## "PUTTO" -- Send addresses for outgoing mail
## "PUTCC" -- CC addresses for outgoing mail
## "PUTSUBJECT" -- Subject for outgoing mail
## "PUTBODY" -- Body of outgoing mail
## "PUTMESSAGE" -- Add message to the outbox.  Returns ("PUTMSG" , "<msgnumber>")
##
##
##
##

dispatch( Input: ref AsscList->Alist ): ref AsscList->Alist
{
  Alist : import asl;
  result := ref Alist(nil, nil, 0);
  directives := Input.getitems("DEBUG");
  if (directives!=nil) {
      test := hd directives;
      debug = 0;
      if (test=="ON") {
	  debug = 1;
      }
  }

  # Process the Alist
  # First: all direct set values

  directives = Input.getitems(SETHWSIZE);
  if (directives!=nil) {
      HWSize = int (hd directives);
      result.setitem(SETHWSIZE,hd directives);
  }

  directives = Input.getitems(SELECTBOX);
  if (directives!=nil) {
      newbox := hd directives;

      # Only inbox and outbox supported right now
      if (newbox == "Inbox" || newbox == "Outbox") {
	  CurrentBox = newbox;
	  result.setitem(SELECTBOX, CurrentBox);
      }
      else {
	  result.setitem(ERROR, SELECTBOX+": Invalid Mailbox"+CurrentBox);
      }
  }

  # Additional configuration variables are sent as the lists
  # CFVARNAMES and CFVARVALS, and stored in the local Alist ConfigVariables.

  directives = Input.getitems(CFVARNAMES);
  if (directives!=nil) {
      ###
      ### Split out as separate routine
      ###
      CfVarNames := directives;
      directives = Input.getitems(CFVARVALS);
      if (directives!=nil) {
	  CfVarVals := directives;
	  if (len CfVarNames != len CfVarVals) {
	      result.setitem(ERROR, CFVARNAMES+": Name and value list length mismatch");
	  }
	  else
	  {
	      vname := hd CfVarNames;
	      CfVarNames = tl CfVarNames;
	      vval := hd CfVarVals;
	      CfVarVals = tl CfVarVals;
	      while(vname != nil) {
		  ConfigVariables.setitem(vname, vval);
		  if (CfVarNames != nil) {
		      vname = hd CfVarNames;
		      CfVarNames = tl CfVarNames;
		      vval = hd CfVarVals;
		      CfVarVals = tl CfVarVals;
		  }
		  else {
		      vname = nil;
		  }
	      }
	  }
      }
      else {
	  result.setitem(ERROR, CFVARNAMES+": Missing value list");
      }
  }

  directives = Input.getitems(POPSERVER);
  if (directives!=nil) {
      POPServer = hd directives;
      trace("POPServer = "+POPServer);
  }

  directives = Input.getitems(SMTPSERVER);
  if (directives!=nil) {
      SMTPServer = hd directives;
      trace("SMTPServer = "+SMTPServer);
  }

  directives = Input.getitems(USERID);
  if (directives!=nil) {
      UserID = hd directives;
      trace("Userid = "+UserID);
  }
  
  directives = Input.getitems(PASSWORD);
  if (directives!=nil) {
      Password = hd directives;
#      trace("Password = "+Password);
  }
  
  directives = Input.getitems(EMAILADDR);
  if (directives!=nil) {
      EmailAddr = hd directives;
      trace("EmailAddr = "+EmailAddr);
  }
  
  # Return the value of a local state variable

  directives = Input.getitems(GETSTATE);
  if (directives!=nil) {
      ###
      ###  Split out as separate routine
      ###
      trace("GetState");
      statevar := hd directives;
      directives = tl directives;
    VarLoop:
      while (statevar != nil) {
	  case statevar {
	      "ExplicitConn" =>
		  result.setitem("ExplicitConn", ExplicitConn);
	      "PermStore" =>
		  result.setitem("PermStore", PermStore);
	      "LastMessage" =>
		  result.setitem("LastMessage", string LastMessage);
	      "HWSize" =>
		  result.setitem("HWSize", string HWSize);
	      "HWStart" =>
		  result.setitem("HWStart", string HWStart);
	      "HWEnd" =>
		  result.setitem("HWEnd", string HWEnd);
	      "CurrentBox" =>
		  result.setitem("CurrentBox", CurrentBox);
	      "InBoxName" =>
		  result.setitem("InBoxName", InBoxName);
	      "OutBoxName" =>
		  result.setitem("OutBoxName", OutBoxName);
	      "DelBoxName" =>
		  if (DelBoxName != "") result.setitem("DelBoxName", DelBoxName);
	      "SentBoxName" =>
		  if (SentBoxName != "") result.setitem("SentBoxName", SentBoxName);
	      "BoxNames" =>
		  result.setitem("BoxNames", InBoxName);
		  result.setitem("BoxNames", OutBoxName);
	          if (DelBoxName != "") result.setitem("BoxNames", DelBoxName);
		  if (SentBoxName != "") result.setitem("BoxNames", SentBoxName);
	      "ConfigVars" =>
		  ConfigVariables.first();
	          (vname,whocares) := ConfigVariables.thiskey();
	          while ( vname != nil) {
		      result.setitem("ConfigVars", vname);
		      ConfigVariables.next();
		      (vname,whocares) = ConfigVariables.thiskey();
		  }
	      "POPServer" =>
		  result.setitem("POPServer", POPServer );
	      "SMTPServer" =>
		  result.setitem("SMTPServer", SMTPServer);
	      "UserID" =>
		  result.setitem("UserID", UserID);
	      "Password" =>
		  result.setitem("Password", Password);
	      "EmailAddr" =>
		  result.setitem("EmailAddr", EmailAddr);
	      * =>
		  ###
		  ### Need to try and retrieve a config variable before fail [?]
		  ### (or maybe just put explicit retrieve command)
		  ###
		  result.setitem(ERROR, GETSTATE+": Invalid state variable "+statevar);
	          break VarLoop;
	  }
	  if (directives != nil) {
	      statevar = hd directives;
	      directives = tl directives;
	  }
	  else {
	      statevar = nil;
	  }
      }
  }

  directives = Input.getitems(MSGCOUNT);
  if (directives!=nil) {
      result.augment(blocking_op("msg_count", "", ""));
  }

  directives = Input.getitems(CHECKOUTBOX);
  #sys->print("checking outbox\n");
  if (directives!=nil) {
     lenoutbox := len Outbox;
     if (lenoutbox>0) {
       #sys->print("full\n");
       result.setitem("FULL", (string lenoutbox));
     } else {
       #sys->print("empty\n");
       result.setitem("EMPTY", "0");
     }
  }

  directives = Input.getitems(FETCHHDRS);
  if (directives!=nil) {
      result.augment(blocking_op("fetch_headers", (hd directives), ""));
      #fetch_headers(hd directives)
  }

  directives = Input.getitems(PARSEHDR);
  if (directives!=nil) {
      result.augment(parse_header(hd directives));
  }

  directives = Input.getitems(LDCONFIG);
  if (directives!=nil) {
     #sys->print("getting cfg file\n");
     user_cfg_filename := get_user_cfg_filename();
     #sys->print("getting cfg info\n");
     cfg := read_cfg_file(user_cfg_filename);
     required := ("USERID"::"PASSWORD"::"POPSERVER"::"SMTPSERVER"::"EMAILADDR"::nil);
     cfgproblem := 0;
     for (;;) {
         if (required==nil) { break; }
         field:= hd required;
         required= tl required;
         test:= cfg.getitems(field);
         if (test == nil) { cfgproblem = 1; }
         #else { sys->print("got %s: %s", field, (hd test)); }
     }
     result.augment(cfg);
     if (cfgproblem) {
        result.setitem("ERROR", "Invalid config file: <"+user_cfg_filename +">");
     }
  }

  directives = Input.getitems(SVCONFIG);
  if (directives!=nil) {
      #result.augment(Alist.frompairs((("ERROR" , "Cannot save configuration") :: nil)));
      out_file := get_user_cfg_filename();
      success := save_cfg(out_file, Input);
      if (success != nil) {
         result.setitem("ERROR", success);
      }
  }

  directives = Input.getitems(GETBODY);
  if (directives!=nil) {
      result.augment( blocking_op("get_body", (hd directives), "") );
      #get_body(hd directives)
  }

  directives = Input.getitems(PUTTO);
  if (directives!=nil) {
      outmsg_to = hd directives;
      if (len outmsg_to == 124)
        result.setitem("WARNING","Mail To list is too long, some of the recipients will be removed.\n");	
  }

  directives = Input.getitems(PUTCC);
  if (directives!=nil) {
      outmsg_cc = hd directives;
      if (len outmsg_cc == 124)
        result.setitem("WARNING","CC list is too long, some of the recipients will be removed.\n");
  }

  directives = Input.getitems(PUTSUBJECT);
  if (directives!=nil) {
      outmsg_subject = hd directives;
  }
  
  directives = Input.getitems(PUTCONTYPE);
  if (directives != nil) {
     contentype = hd directives;
  }

  directives = Input.getitems(PUTVERSION);
  if (directives != nil) {
     mimeversion = hd directives;
  }
  directives = Input.getitems(PUTBODY);
  if (directives!=nil) {
      outmsg_body = hd directives;
  }

  directives = Input.getitems(PUTMESSAGE);
  if (directives!=nil) {
      result.augment(add_outmsg());
  }

  directives = Input.getitems(SEND);
  if (directives!=nil) {
      result.augment(blocking_op("send_msgs", "", ""));
	if(newmail&&(en<20)) {
	   sys->sleep(12000);
	   newmail = 0;
	} 
	#send_msgs();
  }

  directives = Input.getitems(DELETE);
  if (directives!=nil) {
      result.augment(blocking_op("delete_msg", CurrentBox, (hd directives)));
      #delete_msg(CurrentBox, hd directives)
  }

  directives = Input.getitems("");
  if (directives!=nil) {
  }

  return result;
}

save_cfg(filename: string, data: ref AsscList->Alist): string
{
   Alist: import asl;
   #dump_alist("cfg out", data);


   f := sys->create(filename, sys->OWRITE, 8r600);
   if (f == nil) {
      return "could not create/rewrite config file: "+filename;
   }

   bytes := data.marshal();
   length := len bytes;
   test := sys->write(f, bytes, length);
   if (test!=length) {
      return "could not write config file: "+filename;
   }
   return nil;
}

read_cfg_file(filename: string): ref AsscList->Alist
{
   Alist: import asl;
   ### temporary!!!!
   #return Alist.frompairs( (("USERID","arw") :: ("PASSWORD","blah") ::
   #      ("SMTPSERVER","dante") :: ("POPSERVER", "dante") :: ("EMAILADDR","arw") :: nil));
   result := Alist.frompairs(nil);
   marshalled := get_small_file_contents(filename);
   if (marshalled == nil) {
      return result; # could not open file, or file empty: return no data
   }
   (unmarshalled, err) := Alist.unmarshal(marshalled, 0);
   #dump_alist("unmarshalled", unmarshalled);
   if (err != nil) {
      result.setitem("ERROR", err);
   } else {
      result.augment(unmarshalled);
   }
   #dump_alist("cfg in", result);
   #sys->print(" end of configuration\n");
   return result;
}

dump_alist(header:string, result: ref AsscList->Alist)
{
   Alist: import asl;
   result.first();
   for (;;) {
       ((a,b), i) := result.thispair();
       if (!i) { break; }
       result.next();
       sys->print(" %s: (%s) : (%s)\n", header, a, b);
   }
}


get_user_cfg_filename(): string
{
   ##### temporary!
   #return "/usr/arw/mail.cfg";
   username := string get_small_file_contents("/dev/user");
   return "/usr/"+username+"/mail.cfg";
}

get_small_file_contents(filename: string): array of byte
{
   max := 4000;
   f := sys->open(filename, sys->OREAD);
   if (f == nil) { 
      #sys->print("open failed %r %s", filename);
      return nil; 
   }
   buffer:= array[max] of byte;
   got := sys->read(f, buffer, max);
   if (got == max) { return nil; }
   return buffer[:got];
}

trace(s: string)
{
    if (debug) {
       sys->print("%s\n", s);
    }
}

# do a blocking operation with popup...
#   launch a process which spawns a new process group
#   pops up a "cancel screen" and launches a subprocess
#   which blocks.

blocking_op(which_op: string, arg1: string, arg2: string): ref AsscList->Alist
{
   tunnel:= chan of ref AsscList->Alist;
   spawn blocking_process(which_op, arg1, arg2, tunnel);
   alt{
      result := <-tunnel => { return result; }
   }
}


#foldertab := array[] of {
#	"frame .brder -bg black",
#	"frame .buts",
	#"label .buts.lbl -text {Operation in Progress} -font /fonts/lucidasans/latin1.7.font",
#	"label .buts.lbl -text {"+gui->OPERATION_PROGRESS+"} -font "+gui->OPERATION_FONT,
#	"button .buts.cancel -text {Cancel} -fg red -command 'send chn cancel",
#	"pack .buts.lbl -expand 1 -side left -fill x -padx 4 -pady 4",
#	"pack .buts.cancel -expand 1 -side left -fill x -padx 4 -pady 4",

#	"pack .Wm_t -side top -fill x",
#	"pack .brder -side bottom -fill x",
#	"pack .buts -in .brder -side bottom -fill x -padx 2 -pady 2",
#	"pack propagate . 0",
#+++++	". configure -bd 1 -relief raised",
#	"update"
#};


# spawn a new process group, pop a cancel screen, and launch
# a blocking op
#
blocking_process(which_op:string, arg1:string, arg2:string, 
      tunnel: chan of ref AsscList->Alist)
{
   Alist: import asl;
   #oops := Alist.frompairs( ( ("ERROR", "Operation cancelled!") :: nil ) );
   oops := Alist.frompairs( ( ("ERROR", gui->OPERATION_CANCELLED) :: nil ) );
   mytunnel := chan of ref AsscList->Alist;
   # feather in the testk2.b stuff here.
#
# Load basics
#
   sys = load Sys Sys->PATH;
   draw = load Draw Draw->PATH;
   tk = load Tk Tk->PATH;
   S = load String String->PATH;
   wmlib = load Wmlib Wmlib->PATH;
   wmlib->init();

   tna := "";
   chn := chan of string;

   # make a new process group!

   t : ref Tk->Toplevel;
   #t = tk->toplevel(ctx.screen, "-borderwidth 2 -relief ridge -x 220 -y 200" + " -font /fonts/lucidasans/latin1.7.font");
   t = tk->toplevel(ctx.screen, "-borderwidth 2 -relief ridge -x 220 -y 200 -font "+gui->OPERATION_FONT);
   tk->namechan(t, chn, "chn");
   wmlib->tkcmds(t, gui_tk->foldertab);
   spawn the_real_blocking_process(which_op, arg1, arg2, mytunnel);
   doubleclickflag := 0;

#
    tk->cmd(t, "update");
    alt{
      s := <-chn => {
	case s{
	"cancel" =>
	  finish();
          tunnel <-= oops;
          return;
        }
      }
      result := <-mytunnel => {
          tunnel <-= result;
          finish(); 
          return;
          }
      }
}

the_real_blocking_process(which_op:string, arg1:string, arg2:string, 
      tunnel: chan of ref AsscList->Alist)
{
    Alist: import asl;
    case which_op {
      "msg_count" => {
           tunnel <-= msg_count();
           return;
         }
      "fetch_headers" => {
           tunnel <-= fetch_headers(arg1);
           return;
         }
      "get_body" => {
           tunnel <-= get_body(arg1);
           return;
         }
      "send_msgs" => {
           tunnel <-= send_msgs();
           return;
         }
      "delete_msg" => {
           tunnel <-= delete_msg(arg1, arg2);
           return;
         }
    }
    tunnel <-=
      Alist.frompairs( ( ("ERROR", "unknown blocking opcode") :: nil ) );
}

finish()
{
  Alist: import asl;
  dummy := pop3->dispatch( 
              Alist.frompairs( ( ("CHOKE", "DARNIT") :: nil ) ) );
  dummy = smtp->dispatch( 
              Alist.frompairs( ( ("CHOKE", "DARNIT") :: nil ) ) );
  dummy = dummy; # dummy!
	return;
}


# Get the count of messages on the server
msg_count() : ref AsscList->Alist
{
    Alist : import asl;
    if (CurrentBox == "Inbox") {
	if (POPServer=="" || UserID=="")
            return Alist.frompairs(("ERROR","POP Server, User missing") :: nil);
	else
	    return pop3_msg_count();
    }
    else        # Outbox
	return (Alist.frompairs((MSGCOUNT, string len Outbox)::nil));
}

    
# Get (at most) nheaders headers from the server, ending with a number
# If endstr is 0, get the last nheaders headers
fetch_headers(endstr: string) : ref AsscList->Alist
{
    Alist : import asl;
    begmsg : int;
    endmsg := int endstr;
    header : list of string;
    result := ref Alist(nil, nil, 0);
    #car : string;
    if(newmail&&(en<20)) {
	sys->sleep(12000);
	newmail = 0;
    } 
    hdcount := 0;
    if (lastmsg < endmsg)
	lastmsg = endmsg;
    if (CurrentBox == "Inbox") {
	# First sign on and STAT to see how many messages are on the server
	request1 :=Alist.frompairs(nil);	
	doconnect := pop3->dostat1();
        #sys->print("con %d", doconnect);	
	if(!doconnect) 
	  request1 = Alist.frompairs
	    (
	     ("LIMIT", MAXMESSAGE) ::
	     ("SERVER", POPServer) ::
	     ("USER", UserID) ::
	     ("PASS", Password)::
	     ("DOCONNECT", "") ::
	     ("STAT", "") :: 
	     nil
	     );
	   else  
	     request1 = Alist.frompairs
	    (
	     ("LIMIT", MAXMESSAGE) ::
	     ("SERVER", POPServer) ::
	     ("USER", UserID) ::
	     ("PASS", Password)::
	     #("DOCONNECT", "") ::
	     ("STAT", "") :: 
	     nil
	     );
	  
	    
		
	  result1 := pop3->dispatch(request1);
	
	# Check for error return
	if (result1.getitems("ERROR") != nil) return result1;
	statret := hd result1.getitems("STAT");
	(dummy, count) := sys->tokenize(statret, " ");
	hdcount = int hd count;
	if (endmsg == 0) endmsg = hdcount;
	if (endmsg > hdcount) endmsg = hdcount;
	if (endmsg < HWSize && hdcount >= HWSize) endmsg = HWSize;
	
	# Loop, requesting headers in reverse order (so they will be in the
	#  correct order when pulled from the alist)
	begmsg = (endmsg - HWSize) + 1;
	if (begmsg < 1) begmsg = 1;
	en = endmsg;	
	for (i:=endmsg; i>=begmsg; i--) {
	    request1 = Alist.frompairs
		(("TOP" , string i) ::
		 nil
		 );
	    result2 := pop3->dispatch(request1);

	    # Check for error return
	    if (result2.getitems("ERROR") != nil) return result2;

	    # Put the header on the results alist
	    header = result2.getitems("TOP");
	   ###########
            if (header != nil)
           ############ 
            result.setitem("TOP" , hd header);
	}
	# Disconnect from server
        if(endmsg >= lastmsg-20) {	
	    request1 = Alist.frompairs
	        (("QUIT", "") ::
	         nil
	         );
	     result1 = pop3->dispatch(request1);
	}
	result.setitem(FETCHHDRS , string begmsg);
	result.setitem(LASTHDR, string hdcount);
	return result;
    }
    else {    #Outbox
	obox := ob_reverse(Outbox);
	while (obox != nil) {
	    omsg := hd obox;
	    result.setitem("TOP" , omsg.header);
	    obox = tl obox;
	}

	result.setitem(FETCHHDRS , "1");
	result.setitem(LASTHDR, string hdcount);
	return result;
    }
}

#newheader : ref AsscList->Alist;

# Break a header up into its component fields.  The start of each
#  field is identifiable as a CR-nonwhitespace pair
parse_header(header : string) : ref AsscList->Alist
{
    Alist : import asl;
    result := ref Alist(nil, nil, 0);

    # Pull the message number off
    fieldstart := 0;
    (tagend, datastart, fieldend, nextstart) := find_break(header, fieldstart);
    result.setitem("MSGNUM" , header[fieldstart:tagend]);

    fieldstart = datastart;
    (tagend, datastart, fieldend, nextstart) = find_break(header, datastart);
    while (tagend != fieldend) {
	result.setitem(header[fieldstart:tagend], header[datastart:fieldend]);
	fieldstart = nextstart;
	(tagend, datastart, fieldend, nextstart) = find_break(header, fieldstart);
    }
    #newheader = result;
    return result;
}

CR : con 13;
LF : con 10;
TAB : con 9;
SP : con 32;
COLON : con 58;

find_break(header : string, start : int) : (int, int, int, int)
{
    nextstart : int;
    tagend := 0;
    datastart := 0;
    fieldend := 0;
    headlen := len header;

    if (start >= headlen-1) return (0, 0, 0, 0);

    # Find the end of the tag field
    for (i:=start; i<(headlen-1); i++) {
	if (header[i] == COLON) {
#	    sys->print("tag: %d %d %d %d\n", i, header[i], header[i+1], header[i+2]);
	    tagend = i;
	    datastart = i+1;
	    if (datastart >= headlen) return (0, 0, 0, 0);
	    break;
	}
    }
    if (datastart == 0) return (0, 0, 0, 0);
    
    for ( ;datastart<(headlen-2) && header[datastart]==SP; datastart++)
	;

    for (i=datastart; i<(headlen-2); i++) {
# 	sys->print("data: %d %d %d %d\n", i, header[i], header[i+1], header[i+2]);
	if (header[i] == CR &&
	    header[i+1] == LF &&
	    header[i+2] != SP &&
	    header[i+2] != TAB) {
	    fieldend = i;
	    nextstart = i+2;
	    return (tagend, datastart, fieldend, nextstart);
	}
    }

    return (tagend, datastart, headlen, headlen);
}
	
    
get_body(msg : string) : ref AsscList->Alist
{
    Alist : import asl;
    rematch : array of (int,int);

    if (CurrentBox == "Inbox") {
	# Request the full message from the POP3 server and sign off
     	request1 := Alist.frompairs(("init", MAXMESSAGE) :: nil); 
	doconnect := pop3->dostat1();
      if(!doconnect)  
	 request1 = Alist.frompairs
	    (
	     ("LIMIT", MAXMESSAGE) ::
	     ("SERVER", POPServer) ::
	     ("USER", UserID) ::
	     ("PASS", Password)::
	     ("DOCONNECT", "") ::
	     ("RETR", msg) ::
	     nil
	     );
       else 
         request1 = Alist.frompairs
	    (
	     ("LIMIT", MAXMESSAGE) ::
	     ("SERVER", POPServer) ::
	     ("USER", UserID) ::
	     ("PASS", Password)::
	    # ("DOCONNECT", "") ::
	     ("RETR", msg) ::
	     nil
	     );
       
	result1 := pop3->dispatch(request1);
	
	# Check for error return
	if (result1.getitems("ERROR") != nil) return result1;
	
	### Remove header and return only the message body
	body := hd result1.getitems("RETR");
#	(hdrbeg, hdrend) := regex->execute(rxMSGHDR, body);
	rematch = regex->execute(rxMSGHDR, body);
	if (rematch != nil)
	    (hdrbeg, hdrend) := rematch[0];
	else
	    (hdrbeg, hdrend) = (-1,-1);
	body = body[hdrend:];
      #  sys->print("body %s", body);	
	return Alist.frompairs((("GETBODY" , body)::nil));
    }
    else {    # Outbox
	outbody := nth_outmsg((int msg)-1, Outbox);
	if(outbody != nil){
                return Alist.frompairs((("GETBODY" , outbody.body)::nil));
        }
        else 
		return Alist.frompairs((("ERROR" , "mail has been sent!")::nil));
   }
}
# Return the nth message in the outbox
nth_outmsg(n : int , obox : list of ref Outmessage) : ref Outmessage
{
    if (n > (len obox - 1)) return nil;
    for (i:=0; i<n; i++) obox = tl obox;
    return hd obox;
}

# Reverse outbox list
ob_reverse (box : list of ref Outmessage) : list of ref Outmessage
{
    revbox : list of ref Outmessage;
    revbox = nil;
    
    while (box != nil) {
	revbox = hd box :: revbox;
	box = tl box;
    }

    return revbox;
}

# Add an outgoing message to the Outbox
add_outmsg () : ref AsscList->Alist
{
    Alist : import asl;
    CRLF : con "\n";
    
    now := daytime->local(daytime->now());
    date := string now.mday + "-" + MONTHS[now.mon] + "-" + string now.year;
        
    newmessage := ref Outmessage;
    newmessage.addr = outmsg_to;
    newmessage.cc = outmsg_cc;
    newmessage.subject = outmsg_subject;
    newmessage.contentype = contentype;
    newmessage.mimeversion = mimeversion; 
    newmessage.body = outmsg_body;

    msgnum := len Outbox + 1;

    ### Create a formatted header
    newmessage.header = string msgnum + ":" + 
	                "From: " + newmessage.addr + CRLF +
			"Subject: " + newmessage.subject + CRLF +
		        "Content-Type: " +newmessage.contentype + CRLF +
			"Mime-Version: " + newmessage.mimeversion + CRLF +	
                        "Date: " + date + CRLF +
			"Trailer: " + CRLF;

    Outbox = ob_reverse(Outbox);
    Outbox = newmessage :: Outbox;
    Outbox = ob_reverse(Outbox);

    return (Alist.frompairs(nil));
}    

# Send contents of Outbox and return message count
send_msgs() : ref AsscList->Alist
{
    Alist : import asl;

#    if (Outbox == nil) return Alist.frompairs((("WARNING" , "nothing to send")::nil));

    if ( Outbox != nil ) {
	result := deliver_msgs();
	if (result.getitems("ERROR") != nil) return result;
    }
    return (Alist.frompairs((MSGCOUNT, counter) :: nil)); 
    # Pick up new stat from POP3 server
    #return pop3_msg_count();
}

# Send contents of Outbox
deliver_msgs() : ref AsscList->Alist
{
    Alist : import asl;
    rematch : array of (int,int);

    ### Sign on to the server
    request := Alist.frompairs
	    (
	     ("SERVER", SMTPServer) ::
	     ("DOCONNECT", "") ::
	     nil
	     );
    result := smtp->dispatch(request);
    if (result.getitems("ERROR") != nil) return result;

    while (Outbox != nil) {
        box := Outbox;
        msg := hd box;
        while (box != nil) {
           msg = hd box;
           box = tl box;
        }
	### Build the send request
	request = Alist.frompairs
		(
		 ("FROM" , EmailAddr) ::
		 ("SUBJECT", msg.subject) ::
		 ("CONTENT-TYPE", msg.contentype) ::
		 ("MIME-VERSION", msg.mimeversion) :: 
	         ("BODY", msg.body) ::
		 ("SEND", "") ::
		 ("CLEAR", "") ::
		 ("QUIT", "") ::  
		 nil
		 );
	rxUSID = regex->compile((UserID),0);
	### Do the To addressees
	 (num, adlist) := sys->tokenize(msg.addr,", \n");
        while (adlist != nil) {
            thisad := hd adlist;
            rematch = regex->execute(rxUSID, thisad);
            if (rematch != nil)
		newmail = 1; 
            rematch = regex->execute(rxADDRESS, thisad);
            if (rematch != nil)
               (addbeg, addend) := rematch[0];
            else
                request.setitem("TO" , thisad);
            adlist = tl adlist;
        }


	### Do the CC addressees
	(num, adlist) = sys->tokenize(msg.cc,", \n");
	while (adlist != nil) {
	    thisad := hd adlist;
	    rematch = regex->execute(rxUSID, thisad);
	    if(rematch != nil)
		newmail = 1;
	    rematch = regex->execute(rxADDRESS, thisad);
	    if (rematch != nil)
		(addbeg, addend) := rematch[0];
	    else
		(addbeg, addend) = (-1,-1);
	    
	    if (addbeg >= 0 && addend >= 0)
		request.setitem("CC" , thisad[addbeg+1:addend-1]);
	    else
		request.setitem("CC" , thisad);	    
	    adlist = tl adlist;
	}

	### Fire it off!
	result = smtp->dispatch(request);

	# Check for error return
	if (result.getitems("ERROR") != nil) return result;
	Outbox = tl Outbox;
    }                       #end if 

    # Disconnect from server
#    request = Alist.frompairs
#	    (("QUIT", "") ::
#	     nil
#	     );
#    result = smtp->dispatch(request);
    return result;
}

# Get message count from server
pop3_msg_count() : ref AsscList->Alist
{
    Alist : import asl;
    request := Alist.frompairs(nil); 
    doconnect := pop3->dostat1();
    if(doconnect){ 
          request = Alist.frompairs
	    (
	    ("QUIT" , "") ::
	     nil
	     );
           result1 :=pop3->dispatch(request); 
      }
      request = Alist.frompairs
	(
#	 ("DEBUG" , "ON") ::
	 ("LIMIT", MAXMESSAGE) ::
	 ("SERVER", POPServer) ::
	 ("USER", UserID) ::
	 ("PASS", Password)::
	 ("DOCONNECT", "") ::
	 ("STAT", "") ::
	 nil
	 );
       
    result := pop3->dispatch(request);
	
    # Check for error return
    if (result.getitems("ERROR") != nil) return result;
	
    statret := hd result.getitems("STAT");
    (dummy, count) := sys->tokenize(statret, " ");
    counter = hd count;	
    return (Alist.frompairs((MSGCOUNT, counter) :: nil));
}

# Delete a message from inbox or outbox
delete_msg(mailbox : string, msg : string) : ref AsscList->Alist
{
    Alist : import asl;
    newbox : list of ref Outmessage;
	
    if (mailbox == "Inbox") {
	# Delete from POP-3 server
	request1 := Alist.frompairs(nil);
        doconnect := pop3->dostat1();
        if(doconnect)

	  request1 = Alist.frompairs
	    (
	     ("LIMIT", MAXMESSAGE) ::
	     ("SERVER", POPServer) ::
	     ("USER", UserID) ::
	     ("PASS", Password)::
	    # ("DOCONNECT", "") ::
	     ("DELE", msg) ::
	     ("QUIT", "") :: 
		 nil
	     );
	  else
	    request1 = Alist.frompairs
	    (
	     ("LIMIT", MAXMESSAGE) ::
	     ("SERVER", POPServer) ::
	     ("USER", UserID) ::
	     ("PASS", Password)::
	     ("DOCONNECT", "") ::
	     ("DELE", msg) ::
	     ("QUIT","") :: 
		nil
	     );
	    
	result1 := pop3->dispatch(request1);

	# Check for error return
	if (result1.getitems("ERROR") != nil) return result1;

	success := result1.getitems("DELE");
	if (success != nil) return Alist.frompairs((nil));
	else                return Alist.frompairs((("WARNING" , "Delete failed")::nil));
    }
    else {    # Outbox
	msgnum := int msg;
	obox := Outbox;
	newbox = nil;
	i := 1;

	while (obox != nil) {
	    if (i != msgnum) newbox = hd obox :: newbox;
	    i++;
	    obox = tl obox;
	}

	Outbox = ob_reverse(newbox);
	return Alist.frompairs((nil));
    }
}

