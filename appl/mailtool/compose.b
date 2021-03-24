###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1997 Lucent Technologies
###
### Written by N. W. Knauft
###

implement Compose;

include "sys.m";
	sys: Sys;
	
include "mailtool_gui.m";
        gui: Mailtool_GUI;

include "draw.m";
	draw: Draw;
	Context, Rect : import draw;
##########3
include "regex.m";
         regex: Regex;
#########3
include "tk.m";
	tk: Tk;
	Toplevel: import tk;

include "mailtool_code.m";
 cbu:Mailtool_code;

include "wmlib.m";
	wmlib: Wmlib;

include "string.m";
	S: String;
	splitl : import S;

include "mailtool_tk.m";
   gui_tk: Mailtool_Tk;

include "assclist.m";
	asl: AsscList;

include "gdispatch.m";
	mmgr : GDispatch;

include	"compose.m";

include "key.m";
	keyboard : Keyboard;

CTX, CTY, CTZ, ESC : con (iota + 24);    
DEL: con 57444; 
HOME, END, UPP, DOW, LEF, RIG, PGDOW, PGUP : con (iota + 57360);
SCREENUP : con 57443;
SCREENDM : con 57444;
PINTS : con 57449;
NUMLOC : con 57471;
CALLSE : con 57446;
KF1, KF2, KF3, KF4, KF5, KF6, KF7, KF8, KF9, KF10, KF11, KF12, KF13 : con (iota
+ 57409);

focusWidget     : string;

MAXWIDTH: con 480;
MAXHEIGHT: con 400;

### Constant regular expression patterns

ADDRESS: con "<[^>]+>";
rxADDRESS: Regex->Re;

#labels in GUI
Cc, Mailto, Subject, MailCC, Copy, Continue, MailW, Cancelled, Deliver, Adcc, Cut, 
Paste, SelectA, Quote, Message2, Edit, Cancel,  Keyboard1, 

# Help messages for toolbar buttons
keyboard_help, deliver_help,  cc_help, attach_help, cancel_help,
quote_help :  import gui;
contentype, mimeversion : string;
#-background #646464

pgrp: int;

#SDP#
umsg := array[] of {
    "frame .f -borderwidth 2 -relief flat -padx 3 -pady 3",
    "frame .fbrdr",
    "pack .fbrdr -in .f",
    "frame .flabel",
    "frame .fbut",
    "pack .flabel -in .fbrdr",
    "pack .fbut -in .fbrdr -pady 5",
    "label .lbl",
    "pack .lbl -side left -pady 10 -in .flabel",
    "pack .f",
    "update",
};
#SDP#

copylist, addattach,compose: int;
messageOrig : string;
cmptop,tmsg : ref Tk->Toplevel;
tktop : list of ref Tk->Toplevel;
usermessage : chan of string;
ctxt : ref Draw->Context;
keytop : ref tk->Toplevel;
keys1m : chan of string;
keybdchan : chan of string;

kbdchan : chan of string;

g : string;
keyboard_on : int;			# Is keyboard showing?
kbdinit : int;

initialize(ctx: ref Draw->Context, args: list of string, 
	   msgtype: int, msg: ref Message, chn: chan of string,
	   mailMgr : GDispatch)
{
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;
	mmgr = mailMgr;
	gui = load Mailtool_GUI Mailtool_GUI->PATH;
	gui->init();	
	asl = load AsscList AsscList->PATH;
	ctxt = ctx;

	pgrp = sys->pctl(sys->NEWPGRP,nil);
	gui_tk=load Mailtool_Tk Mailtool_Tk->PATH;
	gui_tk->init();

	cbu=load Mailtool_code Mailtool_code->PATH;
        cbu->init();
	
	wmlib->init();
	regex = load Regex Regex->PATH;


#       sys->print("msgtype====%d\n", msgtype);

	rxADDRESS = regex->compile(ADDRESS, 0);
	mailto : string; 	# address to be pasted into Mailto
	shannon:= 0;		# flag for shannon

	# Parse args
	al : list of string;
	a : string;
	for (al = args; al != nil; al = tl al) {
		a = hd al;
		if ( (len a) > 1) {
			if (a[0:2] == "-m") {
				
				mailto = a[2:];
			} else if (a[0:2] == "-s") {
				shannon = 1;
			}
		}
	}

	menubut: chan of string;
	usermessage = chan of string;
	kbdchan = chan of string;

	focuchan := chan of string;
        tna:= gui->MAIN_WHERE+" -font "+gui->COMPOSE_WIN_FONT;
        
	(cmptop, menubut) = wmlib->titlebar(ctxt.screen, tna,
          	     gui->COMPOSE_TITLE, Wmlib->Appl);
	tk->namechan(cmptop, focuchan, "focuchan");
	initscreen();
	#wmlib->tkcmds(cmptop, composescreen);
	tktop = cmptop::nil; 
        tkcmd(cmptop, ". configure -width "+string gui->screenX+" -height "+string gui->screenY+" ;update");

	if (mailto != "") {
		tkcmd(cmptop," .hdr.e.mt insert end {"+mailto+"}");
		tkcmd(cmptop," focus .hdr.e.sb");	
	}

	if (msgtype != NEW)
	    include_text(cmptop, msgtype, msg);

	tk->cmd(cmptop, "update");
	
        (cmd, keys,  key4s) := tkchn(cmptop);
	for(done := 0;done < 1;) alt {
	  menu := <-menubut =>
	    (n, cmdstr) := sys->tokenize(menu, " \t\n");
	    case hd cmdstr {
	      "exit" =>
		        cancel(ctxt, cmptop, msgtype, chn);
	      "help" =>
		        help(ctxt, cmptop);
	      * =>
			wmlib->titlectl(cmptop, menu);
	    }
	     s := <-focuchan =>
               (n, cmstr) := sys->tokenize(s, " \t\n");
                case hd cmstr {
                "focus" =>
		   if((tl cmstr != nil) &&( hd tl cmstr != focusWidget)) {
                   	tk->cmd(cmptop, focusWidget + " selection clear");
                   	tk->cmd(cmptop, "update");
		   }
                   if (tl cmstr != nil)
                      focusWidget = hd tl cmstr;
                      #sys->print("HONG focuswidget is: %s\n", focusWidget);
                      case focusWidget {
		      ".body.t" =>
		          tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 1 -state normal");
	                  tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 2 -state normal");
	                  tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 3 -state normal");
	                  tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 0 -state normal");
	                * =>  
		           tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 1 -state disabled");
	                   tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 2 -state disabled");
	                   tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 3 -state disabled");
	                   tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 0 -state disabled");
	                }
                   }

	    charpress := <-keys =>
	         killkeyboardbugs(cmptop, focusWidget, charpress);
	         
	     
	    charpress := <-key4s =>
	         tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 1 -state normal");
	         tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 2 -state normal");
	         tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 3 -state normal");
	         tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 0 -state normal");
	         typechar4(focusWidget, charpress);
	  
	  message := <-usermessage =>
	    (n, cmdstr) := sys->tokenize(message, " ");
	    ttemp := hd tktop;
	    tk->cmd(tmsg,"destroy .;update");
	    tmsg = nil;
	    tktop = ttemp::nil;

	    case hd cmdstr {
	      "discard" =>
		if (chn != nil) {
		    chn <-= "cancel";
		    #if(keys1m != nil)
			keyboard_on =0;
			kbdinit = 0;	    
		#  keys1m <-= "quit";
		    fd := sys->open("#p/" + string pgrp + "/ctl", sys->OWRITE);
		    if(fd != nil) 	    
		       sys->fprint(fd, "killgrp");	
		   tk->cmd(cmptop, "destroy . ;update"); 
		    exit;
		}
	     "continue" =>
                if (chn != nil) {
                    chn <-= "done";
                    #if(keys1m != nil)
               		keyboard_on = 0;
			kbdinit = 0;     
		#   keys1m <-= "quit";
                    fd := sys->open("#p/" + string pgrp + "/ctl", sys->OWRITE);     
		    if(fd != nil) 
		       sys->fprint(fd, "killgrp");	
		   	tk->cmd(cmptop, "destroy . ;update"); 
			exit;
                }
	    }
	  press := <-cmd =>
	    if(tmsg == nil) {
	    (n, cmdstr) := sys->tokenize(press, " \t\n");
	    case hd cmdstr {
		"deliver" =>
			deliver(cmptop, chn);
		"addcc" =>
			addcc(cmptop);
		"quote" =>
		        tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 0 -state normal");
	                tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 2 -state normal");
	                tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 3 -state normal");
	                tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 1 -state normal");
	    
			quote(cmptop, msg);
		"keyboard" =>
			spawn do_keyboard();
			#keyboard_on = 1 - keyboard_on;
			
		"attach" =>
			attach();
		"save" =>
			save();
		"pastefile" =>
			pastefile();
		"cancel" =>
			cancel(ctxt, cmptop, msgtype, chn);
		"cut" =>
			cut(cmptop, 1);
		"copy" =>
			copy(cmptop);
		"paste" =>
			paste(cmptop);
		"alltext" =>
			tk->cmd(cmptop, ".body.t tag remove sel 1.0 end");
			tk->cmd(cmptop, ".body.t tag add sel 1.0 end; update");
		"addsender" =>
			addsender();
		"addall" =>
			addall();
		"openaddress" =>
			openaddress();
		}
	  }
	}
}

##########333
#init screen
initscreen () {

   wmlib->tkcmds(cmptop, gui_tk->composcreen);
}
##############
set_mailto(cmptop: ref Tk->Toplevel, mailto: string)
{
	tkcmd(cmptop," .hdr.e.mt delete 0 end");
	tkcmd(cmptop," .hdr.e.mt insert end {"+mailto+"}");
	tkcmd(cmptop," .body.t delete 1.0 end");
	tkcmd(cmptop,"  focus .hdr.e.sb");
}

################ugly, if keyboard is ok, the following code should be deleted.
tkchn (t: ref Tk->Toplevel): (chan of string,  chan of string, chan of string)
{ 
      cmd   := chan of string;
      keys := chan of string;
      key4s := chan of string;
      
      tk->namechan(t, cmd, "cmd");
      tk->namechan(t, keys, "keys");
      tk->namechan(t, key4s, "key4s");
      
      return (cmd, keys,  key4s);
}

killkeyboardbugs( t: ref tk->Toplevel, focu: string, s: string)
{
	 char :=s[1];
        if (char == '\\')
             char = s[2];
         update := "; " + focu + " see insert;";
             
         case char {
         * =>
           u := focu + " insert insert " + s;
           tkcmd(t, u + update);
           tkcmd(t, "update"); 
         ESC or CTX or CTY or CTZ or HOME or END or PGUP or PGDOW  or UPP or  
	 DOW or SCREENUP or PINTS or NUMLOC or CALLSE or KF1 or
	 KF2 or KF3 or KF4 or KF5 or KF6 or KF7 or KF8 or KF9 or KF10 or
	 KF11 or KF12 or KF13 =>
	   tkcmd(t, "update");
	LEF => 
           goleft(t, focu);
         RIG  =>
           goright(t, focu);
	 DEL =>
	#	sys->print("itcomes here. \n");
	   dodel(t, focu);
       }
 }

typechar4( focu: string, c: string)
{    
       char :=c[1];
       if (char == '\\')
         char = c[2];
       update := "; " + focu + " see insert;";
       case char {
         * =>
           u := focu + " insert insert " + c;
           tkcmd(cmptop, u + update);
           tkcmd(cmptop, "update"); 
         ESC or CTX or CTY or CTZ or HOME or END  or
         SCREENUP or SCREENDM or  PINTS or NUMLOC or CALLSE or KF1 or
         KF2 or KF3 or KF4 or KF5 or KF6 or KF7 or KF8 or KF9 or KF10 or
         KF11 or KF12 or KF13 =>
           tkcmd(cmptop, "update");

         
       }
}
goleft(t: ref tk->Toplevel, widgetname: string) 
{
	curindex :=tk->cmd(t, widgetname + " index insert");
	
	index := (int curindex) - 1;
	tk->cmd(cmptop, widgetname + " xview scroll -1 unit");
	tk->cmd(cmptop, widgetname + " icursor " + string index);
	tk->cmd(cmptop, "update");
}
goright(t: ref tk->Toplevel, widgetname: string) 
{
	curindex :=tk->cmd(t, widgetname + " index insert");
	index := (int curindex) + 1;
	tk->cmd(cmptop, widgetname + " xview scroll 1 unit");
	tk->cmd(cmptop, widgetname + " icursor " + string index);
	tk->cmd(cmptop, "update");
}	
dodel(t: ref tk->Toplevel, widgetname: string)
{
        curindex := tk->cmd(t, widgetname + " index insert");
        index := (int curindex) +1;
        tk->cmd(cmptop, widgetname + " delete "+ curindex+" "+string index);
        tk->cmd(cmptop, "update");
}
	
	
include_text(t: ref Toplevel, msgtype: int, msg: ref Message)
{
  S = load String String->PATH;
  rematch : array of (int, int);
  addres : string;
  subject_line := mail_to_line := CC_line := "";
  quote_body := 0;
  mimeversion = msg.mimeversion;

  if (msg != nil) {
    if (msgtype==LITERAL) {
       subject_line = msg.subject;
       quote_body = 0;
       mail_to_line = msg.mailto;
       CC_line = msg.cc;
      
    }
    if ((msgtype==REPLY)||(msgtype==REPLYALL)) {
       if ( len msg.subject < 4 || (msg.subject[:3] != "RE:" &&
    				    msg.subject[:3] != "Re:" &&
    				    msg.subject[:3] != "re:") ) # Never mind rE
	   subject_line = "Re: ";

       subject_line += msg.subject;
       index := tk->cmd(t, ".mbar.msg.menu index {Add Cc...}");
       tk->cmd(t, ".mbar.msg.menu insert 0 command -label {Quote Orig.} " +
    				"-underline 0 -command {send cmd quote}");
       tk->cmd(t, "pack .bbar.quote -side left -fill y");
       addres = msg.mailto;
     
       rematch = regex->execute(rxADDRESS, addres);
       if (rematch !=nil)
          (addbeg, addend) := rematch[0];
       else
           (addbeg, addend) = (-1, -1);
       if (addbeg >= 0 && addend >=0)
           addres = addres[addbeg+1:addend-1];
       mail_to_line = msg.mailto;
    }
    if (msgtype==REPLYALL) 
    
       CC_line = msg.cc;
       

    if (msgtype==FORWARD) {
       tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 1 -state disabled");
       tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 2 -state disabled");
       tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 3 -state disabled");
       tk->cmd(cmptop, ".mbar.edit.menu entryconfigure 0 -state disabled");
       contentype = msg.contentype; 
       subject_line = "Fwd: "+msg.subject;
       quote1(t,msg);
    }
    if (subject_line != "")
      tk->cmd(t, ".hdr.e.sb insert end {" + subject_line +"}");

    if (mail_to_line!="")
      tk->cmd(t, ".hdr.e.mt insert end {"+addres+"}");
    if (CC_line!="") {
        cc_line := "";
    (num, adlist) := sys->tokenize(CC_line,", \n");
	while (adlist != nil) {
	    thisad := hd adlist;

	    rematch = regex->execute(rxADDRESS, thisad);
	    if (rematch != nil)
		(addbeg, addend) := rematch[0];
	    else
		(addbeg, addend) = (-1,-1);
	    
	    if (addbeg >= 0 && addend >= 0)
	       
		cc_line = cc_line + " " + thisad[addbeg+1:addend-1];
#	    else {
#	        if (len thisad > 1) 
#		cc_line = cc_line + " " + thisad;
#		
#		}
	    	    
	    adlist = tl adlist;
	}
	tk->cmd(t, "pack .hdr.l.cc -after .hdr.l.sb -fill y -expand 1");
	tk->cmd(t, "pack .hdr.e.cc -after .hdr.e.sb -fill x -expand 1");
	tk->cmd(t, "bind .hdr.e.sb <Key-\n> {focus .hdr.e.cc}");
	tk->cmd(t, "bind .hdr.e.sb <Control-i> {focus .hdr.e.cc}");	
	tk->cmd(t, ".hdr.e.cc insert end {"+ cc_line+"}");
	if(len cc_line >=124) {
            s := "CC list is too long, some of the recipients\n"
                  +"will be truncated. Please check!";
            #msgbox(s, "OK", "", "ok", "");
            compose_notice(s);
        }
	copylist = 1;
    }
    if (quote_body)
        quote(t, msg);
  }
  if ((msgtype==REPLY)||(msgtype==REPLYALL))
    tk->cmd(t, "focus .body.t");

  messageOrig = tk->cmd(t, ".body.t get 1.0 end");
}

deliver(cmptop: ref Toplevel, chn: chan of string)
{
    Alist : import asl;

    ### Save message to Outbox file
    request := Alist.frompairs
	((("PUTTO", tkcmd(cmptop, ".hdr.e.mt get")) ::
	  ("PUTCC", tkcmd(cmptop, ".hdr.e.cc get")) ::
	  ("PUTSUBJECT", tkcmd(cmptop, ".hdr.e.sb get")) ::
	  ("CONTENTYPE", contentype) ::
	  ("MIMEVERSION", mimeversion) ::
	  ("PUTBODY" , tkcmd(cmptop, ".body.t get 1.0 end")) ::
	  ("PUTMESSAGE" , "") ::
	  nil
	  ));
    test1 := request.getitems("PUTTO");
    if ((test1 == nil) || ( (hd test1) == "" )) {
       #SDP#
       #msgbox("No recipient specified","OK","","ok","");
       s:="No recipient specified";
       compose_notice(s);
       #SDP#
       return;
    }
    result := mmgr->dispatch(request);
    if (do_errors(cmptop, result) ==0) {
        if (chn != nil)
             chn <-= "done";
        #if (keys1m != nil)   
       		keyboard_on =0; 
		kbdinit = 0;	
	#    keys1m <-= "quit";
        fd := sys->open("#p/" + string pgrp + "/ctl", sys->OWRITE);
	if(fd != nil) 	    
	     sys->fprint(fd, "killgrp");	
       	tk->cmd(cmptop, "destroy . ;update"); 
	exit;
    }

}

get_toplevel(): list of ref Tk->Toplevel
{
	return tktop;
}
addcc(t: ref Toplevel)
{
  if (copylist == 1) {
    tk->cmd(t, "pack forget .hdr.l.cc");
    tk->cmd(t, "pack forget .hdr.e.cc ");
    tk->cmd(t, "bind .hdr.e.sb <Key-\n> {focus .body.t}");
    tk->cmd(t, "bind .hdr.e.sb <Control-i> {focus .body.t}"); 
    tk->cmd(t, "update");
    copylist = 0;
  } else {
    tk->cmd(t, "pack .hdr.l.cc -after .hdr.l.sb -fill y -expand 1");
    tk->cmd(t, "pack .hdr.e.cc -after .hdr.e.sb -fill x -expand 1");
    tk->cmd(t, "bind .hdr.e.sb <Key-\n> {focus .hdr.e.cc}");
    tk->cmd(t, "bind .hdr.e.sb <Control-i> {focus .hdr.e.cc}"); 
    tk->cmd(t, "update");
    copylist = 1;
  }
}



quote(t: ref Toplevel, msg: ref Message)
{
    start := tk->cmd(t, ".body.t index insert");
    (line, nil) := splitl(start, ".");
    tk->cmd(t, ".body.t mark set insert {insert lineend}");
    tk->cmd(t, ".body.t insert insert {\n" + msg.mailto + " wrote:\n\n}");

    tk->cmd(t, ".body.t insert insert {" + msg.text + "\n\n}");
    #tk->cmd(t, ".body.t insert insert {" + "fuck nato" + "\n\n}");

#	text_byte:=array of byte msg.text;
#       text_len:=len text_byte;  
#       tk->cmd(t, ".body.t insert insert {" + trimCRLF(string cbu->cbuf_ubuf(text_byte,text_len)) + "\n\n}");



    iline := int line + 3;
    current := tk->cmd(t, ".body.t index insert-1l");
    (line, nil) = splitl(current, ".");

    for (i := iline; i < int line; i++)
	tk->cmd(t, ".body.t insert "+string i+".0 {> }");
    tk->cmd(t, ".body.t tag add fwdtag " + string iline + ".0 " + current);
    tk->cmd(t, ".body.t mark set insert " + start + ";update");
}

quote1(t: ref Toplevel, msg: ref Message)
{
    start := tk->cmd(t, ".body.t index insert");
    (line, nil) := splitl(start, ".");
    tk->cmd(t, ".body.t mark set insert {insert lineend}");
   # tk->cmd(t, ".body.t insert insert {\n" + msg.mailto + " wrote:\n\n}");
    tk->cmd(t, ".body.t insert insert {" + msg.text + "\n\n}");
    iline := int line + 3;
    current := tk->cmd(t, ".body.t index insert-1l");
    (line, nil) = splitl(current, ".");

    for (i := iline; i < int line; i++)
        tk->cmd(t, ".body.t insert "+string i);
    tk->cmd(t, ".body.t tag add fwdtag " + string iline + ".0 " + current);
    tk->cmd(t, ".body.t mark set insert " + start + ";update");
}


#keytop : ref tk->Toplevel;
do_keyboard()
{
    #keys1m1 := keys1m; #chan of string;	

	if (keyboard_on) {
	   keys1m <-= "exit";
	   keyboard_on = 0;
	} else {
	  if (kbdinit == 0) {
                kbdinit = 1;
                keyboard_on =1;	
		g = wmlib->geom(keytop);
		keyboard = load Keyboard Keyboard->PATH;	
		(keytop, keys1m) = wmlib->titlebar(ctxt.screen, g, gui->keyboard_title, 0);	
		#keytop = tk->toplevel(ctxt.screen, "-x 278 -y 314");
		keybdchan = keyboard->initialize(keytop, ctxt, ".keyframe");
		tk->cmd(keytop, "pack .Wm_t   -fill x");	
		tk->cmd(keytop, "pack .keyframe -fill x; update");
		tk->cmd(keytop, "pack propagate . 0");
		
		#keyboard->enable_move_key(keytop);
	    } else {
               keyboard_on = 1;
               tk->cmd(keytop, ". map;raise .; update");
               tk->cmd(keytop, "pack .Wm_t   -fill x");	
	       tk->cmd(keytop, "pack .keyframe -fill x; update");
	       tk->cmd(keytop, "pack propagate . 0");
        }
       
	for(;;) {
		#     tk->cmd(keytop, "update");
		     alt {
			s := <-keys1m =>
			  if(s[0] == 'e') {
			        tk->cmd(keytop, ". unmap; update");
		
				keyboard_on  = 0;
				return;
	
	                        
			}
			#if (s[0] == 'q') {
			#  keyboard_on = 0;
			#  kbdinit = 0;	 
			#   tk->cmd(keytop, "destroy . ;update");
			#   exit;	
		      	# } 
			 wmlib->titlectl(keytop, s);
			 
		    }
		}		
	}
		
	
	

}

attach()
{
  ### Pop up panel to get filename(s) or URL(s)
  ### Pack attachment entry field with attachment name(s)
}

save()
{
  ### Get filename, then save message to file
}

pastefile()
{
  ### Get filename, then read text from file, and paste into text
}

cancel(ctxt : ref Draw->Context, cmptop : ref Toplevel, msgtype : int, chn: chan of string)
{
    dummy := ctxt;
    # Get any text entered in body and check for changes
    newbody := tk->cmd(cmptop, ".body.t get 1.0 end");
    if (msgtype == NEW && newbody == "") {
    	if (chn != nil) {
       	   chn <-= "cancel";
       	   #if (keys1m != nil)
       	     keyboard_on = 0;
	     kbdinit = 0;	   
	#  keys1m <-= "quit";
       	   fd := sys->open("#p/" + string pgrp + "/ctl", sys->OWRITE);
	   if(fd != nil) 	    
		sys->fprint(fd, "killgrp");	
	   tk->cmd(cmptop, "destroy . ;update"); 
	   exit;
	}
    } else if (newbody == messageOrig) {
    	if (chn != nil) {
       	   chn <-= "cancel";
       	   #if (keys1m != nil)  
       	     keyboard_on =0;
	     kbdinit = 0; 
		  # keys1m <-= "quit";
       	   fd := sys->open("#p/" + string pgrp + "/ctl", sys->OWRITE);
	   if(fd != nil) 	    
		    sys->fprint(fd, "killgrp");	
	   tk->cmd(cmptop, "destroy , ;update"); 
	    exit;		
	}
    }

    # Changes detected -- verify that the user really wants to cancel
    #SDP#
    msgbox(gui->msgbox_ques,gui->msgbox_ndiscard,
		gui->msgbox_discard,"donotdiscard","discard");
    #SDP#
}

addsender()
{
  ### Add sender address to Address Book file
}

addall()
{
  ### Add sender and cc addresses to Address Book file
}

openaddress()
{
  ### Display address panel with contents of address file in it
}

serveropts()
{
  ### Pop up panel for setting server options
}

useropts()
{
  ### Pop up panel for setting user options
}

help(ctxt: ref Draw->Context, t: ref Tk->Toplevel)
{
    dummy := (ctxt, t);
  ### Pop up Help window
}

cut(t: ref Tk->Toplevel, snarfit: int)
{
  if(tk->cmd(t, ".body.t tag ranges sel") == "")
    return;
  if(snarfit)
    wmlib->snarfput(tk->cmd(t, ".body.t get sel.first sel.last"));
  tk->cmd(t, ".body.t delete sel.first sel.last");
  tk->cmd(t, "update");
}

copy(t: ref Tk->Toplevel)
{
  if (tk->cmd(t, ".body.t tag ranges sel") == "")
    return;
  wmlib->snarfput(tk->cmd(t, ".body.t get sel.first sel.last"));
}

paste(t: ref Tk->Toplevel)
{
  snarf := wmlib->snarfget();
  if(snarf == "")
    return;
  cut(t, 0);
  tk->cmd(t, ".body.t insert insert '"+snarf);
  tk->cmd(t, "update");
}

#SDP#
# See if error or warning returned and put up dialog box
do_errors(cmptop : ref Toplevel, result : ref AsscList->Alist) :int {
    Alist : import asl;
    dummy := cmptop;
    error := 0;

    errormsg := result.getitems("ERROR");
    if (errormsg != nil) {
	s := sys->sprint("Mail ERROR\n%s", hd errormsg);
	#msgbox(s,"OK","","ok","");
       compose_notice(s);
	error = 1;
    }

    warnmsg := result.getitems("WARNING");
    if (warnmsg != nil) {
	s := sys->sprint("Mail WARNING\n%s", hd warnmsg);
	msgbox(s,gui->msgbox_cancel,gui->msgbox_continue,gui->msgbox_cancel,gui->msgbox_continue);
	error = 1;
    }
    return error;
}
#SDP#

# Execute a tk->cmd and catch errors
tkcmd(t: ref Toplevel, s: string): string
{
	res := tk->cmd(t, s);
#	if(wmlib->is_err(res)) {
#	    sys->print("mygui: tk error executing '%s': %s\n", s, res);
#	}
	return res;
}

#SDP#
msgbox(msg:string, but1txt,but2txt,button1,button2: string)
{

        tmsg = tk->toplevel(ctxt.screen, gui->msgbox_font);
#                 " -font /fonts/lucidasans/unicode.7.font"
#                 + "  -x 150 -y 200 -bd 2 -relief raised");
        ttemp := hd tktop;
        tktop = ttemp::tmsg::nil;
        tk->namechan(tmsg,usermessage,"usermessage");
        wmlib->tkcmds(tmsg, umsg);
        if(but1txt != "")
	{
		tk->cmd(tmsg,"button .but1 -text {"+ but1txt +"} -command {send usermessage "+button1+"}");
		tk->cmd(tmsg,"pack .but1 -side left -padx 5 -in .fbut");
	}
        if(but2txt != "")
	{
		tk->cmd(tmsg,"button .but2 -text {"+ but2txt +"} -command {send usermessage "+button2+"}");
		tk->cmd(tmsg,"pack .but2 -in .fbut");
	}
        tk->cmd(tmsg,".lbl configure -text {" + msg +"}");
        tk->cmd(tmsg,"update");
        return;
}
#SDP#

compose_notice_cmd := array[] of {
        "frame .f",
        "button .b -text OK -command {send cmd done}",
        "focus .f",
        "bind .f <Key-\n> {send cmd done}",
        "pack .f.m -side left -expand 1 -padx 10 -pady 10",
        "pack .f .b -padx 10 -pady 10",
        "update; cursor -default"
};

compose_notice(msg: string)
{
      t := tk->toplevel(ctxt.screen, 
		"-x 70 -y 70 -borderwidth 2 -relief raised");
        cmd := chan of string;
        tk->namechan(t, cmd, "cmd");
        tk->cmd(t, "label .f.m -text {"+msg+"}");
        for(i := 0; i < len compose_notice_cmd; i++)
                tk->cmd(t, compose_notice_cmd[i]);
        <-cmd;
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

