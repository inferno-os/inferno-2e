#    Last change:  I     9 Apr 1998    4:17 pm
###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1997 Lucent Technologies
###
### Written by N. W. Knauft
###

### LOOK FOR HARD CODED PATHS!!!

implement Mail;

include "sys.m";
	sys: Sys;

include "draw.m";
	draw: Draw;
	Context, Font, Rect: import draw;

include "mailtool_gui.m";
	gui: Mailtool_GUI;


include "tk.m";
	tk: Tk;
	Toplevel: import tk;

include "wmlib.m";
	wmlib: Wmlib;

include "assclist.m";
	asl: AsscList;
	Alist : import asl;

include "gdispatch.m";
	mmgr : GDispatch;

include "mailtool.m";

include "mailtool_tk.m";
      gui_tk: Mailtool_Tk;

include "compose.m";
	compose: Compose;
	Message: import compose;        
	NEW, REPLY, REPLYALL, FORWARD, LITERAL: import compose;

include "mail-interface.m";
	extras: GUI_extras;
	Header, fetchheaders, loadheaders, sendcfig : import extras;
	loadcfig, savecfig, short_header, fetch_body : import extras;
	remove_crs, get_msgargs, do_connect, delete_msg: import extras;
	renumber_headers: import extras;
        NOCHANGE : import extras;
	SENDERROR, SENDCOMPLETE, NEWMAIL, LESSMAIL : import extras;

#include "ctlDbSh.m";

#####
include "daytime.m";

include "string.m";
   Str: String;
   splitstrr: import Str;
########
#fi : import i18n;
MAXWIDTH: con 480;
MAXHEIGHT: con 400;
focusWidget 	: string;
CTX, CTY, CTZ, ESC : con (iota + 24);
DEL: con 57444;
HOME, END, UPP, DOW, LEF, RIG, PGDOW, PGUP : con (iota + 57360);
SCREENUP : con 57443;
#SCREENDM : con 57444;
PINTS : con 57449;
NUMLOC : con 57471;
CALLSE : con 57446;
KF1, KF2, KF3, KF4, KF5, KF6, KF7, KF8, KF9, KF10, KF11, KF12, KF13 : con (iota
+ 57409);

#######################3
#GUI labels
fi,  New, Save, Check_mail, Exit, Next, Previous, Close, Delete, OK, Composedot,
Composem, Savedot, Serverdot, Userdot, Inbox, Newdot, Show_Head, Copy, Select_all, Reply_to_Sdot,
Reply_to_Adot, Forwarddot, Prev, Re, All, Fwd, Show, POp, SMtP, User_L,
Password, Email_add, Cancel, mailmess1, mailmess2,  Sendin, Please_c, folder1,
Message2, headers1, options, Edit,

#error message
error1, error2, warn1, warn2, warn3, warn4, Warn5,  
###############3333
# mail box alert messages
please_connect, no_mail_alert,
working,

# Help messages for toolbar buttons
file_help, connect_help, new_help, savesel_help, delsel_help, prev_help,
next_help, reply_help, all_help, fwd_help, head_help, save_help, del_help,

# Action status messages
signing_on, getting_hdrs, getting_mail, contacting_server, messages_sent,
send_error, server_change : import gui;

curselect := array[] of {
    ".mbar.msg.menu entryconfigure 0 -state normal",
    ".mbar.msg.menu entryconfigure 1 -state normal",
    #".mbar.address.menu entryconfigure 0 -state normal",
    #".mbar.address.menu entryconfigure 1 -state normal",
    #".bbar.msg.save configure -state normal",
    ".bbar.msg.delete configure -state normal",
};

noselect := array[] of {
    ".mbar.msg.menu entryconfigure 0 -state disabled",
    ".mbar.msg.menu entryconfigure 1 -state disabled",
    #".mbar.address.menu entryconfigure 0 -state disabled",
    #".mbar.address.menu entryconfigure 1 -state disabled",
    #".bbar.msg.save configure -state disabled",
    ".bbar.msg.delete configure -state disabled",
};

msgcurselect := array[] of {
    ".mbar.msg.menu entryconfigure 4 -state normal",
    ".mbar.msg.menu entryconfigure 5 -state normal",
    ".mbar.edit.menu entryconfigure 0 -state normal",
    ".mbar.edit.menu entryconfigure 1 -state normal",
    ".mbar.compose.menu entryconfigure 1 -state normal",
    ".mbar.compose.menu entryconfigure 2 -state normal",
    ".mbar.compose.menu entryconfigure 3 -state normal",
    #".mbar.address.menu entryconfigure 0 -state normal",
    #".mbar.address.menu entryconfigure 1 -state normal",
    ".bbar.compose.reply configure -state normal",
    ".bbar.compose.replyall configure -state normal",
    ".bbar.compose.forward configure -state normal",
    ".bbar.msg.header configure -state normal",
    ".bbar.msg.save configure -state normal",
    ".bbar.msg.delete configure -state normal",
};

msgnoselect := array[] of {
    ".mbar.msg.menu entryconfigure 4 -state disabled",
    ".mbar.msg.menu entryconfigure 5 -state disabled",
    ".mbar.edit.menu entryconfigure 0 -state disabled",
    ".mbar.edit.menu entryconfigure 1 -state disabled",
    ".mbar.compose.menu entryconfigure 1 -state disabled",
    ".mbar.compose.menu entryconfigure 2 -state disabled",
    ".mbar.compose.menu entryconfigure 3 -state disabled",
    #".mbar.address.menu entryconfigure 0 -state disabled",
    #".mbar.address.menu entryconfigure 1 -state disabled",
    ".bbar.compose.reply configure -state disabled",
    ".bbar.compose.replyall configure -state disabled",
    ".bbar.compose.forward configure -state disabled",
    ".bbar.msg.header configure -state disabled",
    ".bbar.msg.save configure -state disabled",
    ".bbar.msg.delete configure -state disabled",
};

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
    "button .but1",
    "button .but2",
    "pack .lbl -side left -pady 10 -in .flabel",
    "pack .but1 .but2 -side left -padx 5 -in .fbut",
    "pack .f",
    "update",
};
#SDP#








# Globals
tktop : ref Toplevel;	# Root window for the application
tkmsg: ref Toplevel;	# Mail message window
cmptop: list of ref Toplevel; 	# Compose root window
tpopup : ref Toplevel;  #SDP# Popup window
tmsg : ref Toplevel; #SDP# Message Window

msgmenubut, msg, cmp, popup, usermsg : chan of string; #SDP#
cmesg, nmesg, lastmsg: int;
currentfolder: string;
headers : list of ref Header;
firstheader,lastheader : int;           # Number of the first and last message
showLongHeaders := 0;                   # Display all header info
cacheBody := "";                        # Hold onto last displayed message body;
direction := 0;
#########
mailcounter: int = 0;
###########
pop, smtp, uid, passwd, mailaddr : string;
mailcheck: int = 30;
user := "";
pgrp: int;

PREVBUTTON, NEXTBUTTON, CHECKBUTTON, NEWBUTTON, DELETEBUTTON, CLOSEBUTTON : con (iota + 0);

listfontWidth, listfontHeight : int;
listboxWidth, listboxHeight : int;      # Dimensions in characters

LastMsgHdr := 0;		        # Current value of last message
HeaderWindow := 20;		        # Number of headers to fetch

#
# Toolbar/Program Manager
# Channels & database
#

RdProgMgr, WrProgMgr: chan of string;
WrProgMgr = nil;
RdProgMgr = nil;

#tdb: Tdb;
#Dbh: import tdb;
#ispDb: ref Dbh;

jumpcompose := 0;
debug: con 0;

#########################################################################
#
# Mailtool init
#
 toolbarinit(ctxt: ref Draw->Context, argv:list of string, Ch1, Ch2: chan of string){}

init(ctxt: ref Draw->Context, argv: list of string)
{
	# Load the basics
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
        if (draw==nil) { sys->print("draw not loaded\n"); }
	tk = load Tk Tk->PATH;
        if (tk==nil) { sys->print("tk not loaded\n"); }
	wmlib = load Wmlib Wmlib->PATH;
        if (wmlib==nil) { sys->print("wmlib not loaded\n"); }

	pgrp = sys->pctl(sys->NEWPGRP, nil);
	wmlib->init();

	# Load Asslist
	asl = load AsscList AsscList->PATH;
	
	gui = load Mailtool_GUI Mailtool_GUI->PATH;
	gui->init();

	gui_tk = load Mailtool_Tk Mailtool_Tk->PATH;
	gui_tk->init();

	
	# Load and Init gdipatch 
        mmgr = load GDispatch GDispatch->MMGRPATH;
        if (mmgr==nil) { sys->print("mmgr not loaded\n"); }
	mmgr->init();
        mmgr->store_the_graphics_context_please(ctxt);

	# Load and init the gui stuff
	extras = load GUI_extras GUI_extras->PATH;
        if (extras==nil) { sys->print("extras not loaded\n"); }
	extras->init(ctxt);


        # Parse args
        al : list of string;
        a : string;
        for (al = argv; al != nil; al = tl al) {
                a = hd al;
                if ( (len a) > 1) {
                        if (a[0:2] == "-m") {
                                mto := a[2:];
				argv = mto :: argv;
				jumpcompose = 1;
                        }
                }
        }

	
	tkargs := argv;
	
#
# Put up main mailtool screen 
#
	ttop : ref Tk->Toplevel;
	menubut: chan of string;

        #tna := " -font /fonts/lucidasans/latin1.7.font " +
        tna := gui->MAIN_WHERE+" -font "+gui->MAIN_FONT;
	(ttop, menubut) = 
        	wmlib->titlebar(ctxt.screen, tna, gui->MAILTOOL_TITLE, Wmlib->Appl);
       	tktop = ttop;
	tkcmd(ttop, "pack .Wm_t -side top -fill x;" +
    		    ".Wm_t.title configure -text {"+gui->MAIN_TITLE+"}"+gui->MAIN_START_POINT);
	initmain();
        tkcmd(ttop, ". configure -width "+string gui->screenX+" -height "+string gui->screenY+";update");

	cmd := chan of string;
	entychan := chan of string;
	focuchan :=chan of string;
	
	tk->namechan(tktop, cmd, "cmd");
        
	msgmenubut = chan of string;
	msg = chan of string;
	cmp = chan of string;
	popup = chan of string;
	usermsg = chan of string;

	# Capture configure (resizing and move) events
	tkcmd(tktop, "bind . <Configure> {send cmd config}");

	# Determine character size for the listbox
	listfont := Font.open(ctxt.display, "/fonts/lucidasans/typelatin1.7.font");
	listfontWidth = listfont.width(" ");
	listfontHeight = listfont.height;

	# Get the original size for the listbox in chars
	listboxWidth = int (int tkcmd(tktop, ".listing.p cget -actwidth") / listfontWidth);
	listboxHeight = int (int tkcmd(tktop, ".listing.p cget -actheight") / listfontHeight);


#	loadmessages(ctxt, tktop, "Inbox");
#	updateselectbuttons(tktop);

	### Start timer process to periodically check for new mail


	# Set defaults for connection/number of messages
	nmesg = 0;
	cmesg = -1;
        justconnected := 0;

	# Setup dummy channel
	if (RdProgMgr == nil) {
		RdProgMgr = chan of string;
	}

	if (jumpcompose) {
		if (compose == nil)
		  compose = load Compose Compose->PATH;
		spawn compose->initialize(ctxt, tkargs, NEW, nil, cmp, mmgr);
	}
	#############
	set_server_parameters();
	spawn connect(ctxt, tktop);
	justconnected = 1;
	##########
#
# Main event loop
#
	for(;;) alt {
	  menu := <-menubut =>
	    (n, cmdstr) := sys->tokenize(menu, " \t\n");
	    case hd cmdstr {
	      "exit" =>
			spawn finish();
	      "task" =>
		        spawn wmlib->titlectl(tktop, menu);
	      "help" =>
		        help(ctxt, tktop);
	      * =>
			wmlib->titlectl(tktop, menu);
	    }

          #SDP#

          message := <-usermsg =>
          (n, cmdstr) := sys->tokenize(message, " ");
          tk->cmd(tmsg,"destroy .;update");
          tmsg = nil;

          case hd cmdstr {
                "overwriteoutbox" =>
                        spawn compose->initialize(ctxt, tkargs, NEW, nil, cmp, mmgr);
                "owriteoutbox" =>
                        header := nth_header(cmesg, headers);
                        mmsg := get_msgargs(header,  mmgr);
                        mmsg.text = cacheBody;
                        if (tl cmdstr != nil) mtype := hd(tl cmdstr);
                        spawn compose->initialize(ctxt, tkargs, int mtype, mmsg
, cmp, mmgr);
                "discard" =>
                        finish();
          }
          #SDP# 
          ##############
        ###############

	# Listen for Toolbar/Program Manager commands
	#
	     pmgr_cmd := <-RdProgMgr =>
		(n, cmdstr) := sys->tokenize(pmgr_cmd, " \t\n");
		case hd cmdstr {
			"raise" =>
				tk->cmd(tktop, "raise .; update");
				if (WrProgMgr != nil) WrProgMgr <-= "raised";
			#### Check for args for app to app
			"destroy" =>
				finish();
		}

	  press := <-cmd =>
	    if((tpopup == nil) && (tmsg == nil)) {
	    (n, cmdstr) := sys->tokenize(press, " \t\n");
            if (justconnected) {
               justconnected = 0;
            }
            newmail_alert(tktop, working);
            if (cmdstr!=nil) {
              this_cmd := hd cmdstr;
	      case this_cmd {
                "exitfile" =>
                        finish();
		"connect" =>
			disablmenu(tktop);
		        spawn connect(ctxt, tktop);
                        justconnected = 1;
		"newfile" =>
			newfile(tktop);
		"openfile" =>
		#	openfile(ctxt, tktop);
		"deletefile" =>
			spawn deletefile(ctxt, tktop);
		"emptytrash" =>
			emptytrash();
		"select" =>
			 select(ctxt, tktop);
		"nextmsg" =>
			tkcmd(tktop, ".mbar.msg.menu entryconfigure 0 -state disabled; update");
			 nextmsg(ctxt, tktop);
		"prevmsg" =>
			tkcmd(tktop, ".mbar.msg.menu entryconfigure 1 -state disabled");
			 prevmsg(ctxt, tktop);
		"prevhdrs" =>
			disablmenu(tktop);
			prevhdrs(ctxt, tktop);
		"nexthdrs" =>
			disablmenu(tktop);
			nexthdrs(ctxt, tktop);
		"newmsg" =>
                       	disablnew(); 
			newmail_alert(tktop, please_connect);
			if (compose == nil)
			  compose= load Compose Compose->PATH;
			spawn compose->initialize(ctxt, tkargs, NEW, nil, cmp, mmgr);
		"savemsg" =>
			savemsg(ctxt, tktop, tktop);
		"deletemsg" =>
			tk->cmd(tktop, ".bbar.msg.delete configure -state disabled");
			spawn deletemsg(ctxt, tktop);
		"addsender" =>
			addsender();
		"addall" =>
			addall();
		"openaddress" =>
			openaddress();
		"serveropts" =>
			serveropts(ctxt, tktop);
		"useropts" =>
			useropts(ctxt, tktop);
		"saveopts" =>
	      	#	savecfig(mmgr, uid, passwd, pop, smtp, mailaddr);
			do_user_reg(ctxt);		
		### Need to save mailcheck interval, too
                  #      set_server_parameters();
		"config" =>
		    resize_main(ctxt, tktop);
#		    sys->print("Config command: %s\n", press);
                }
	  }
          if (checkoutbox()) {
             newmail_alert(tktop, please_connect); # outbox alert
          } else {
             if (!justconnected == 1) {
                newmail_alert(tktop, no_mail_alert);
             }
          }
	}
	  msgmenu := <-msgmenubut =>
	    (n, cmdstr) := sys->tokenize(msgmenu, " \t\n");
	    case hd cmdstr {
	      "exit" =>
# The close button (under Message) should do the same thing as exit
		        tk->cmd(tkmsg, "destroy .; update");
	                tkmsg = nil;
	                cacheBody = "";
			tk->cmd(tktop, "focus .listing.p");
	      "task" =>
		        spawn wmlib->titlectl(tkmsg, msgmenu);
	      "help" =>
		        help(ctxt, tkmsg);
	      * =>
			wmlib->titlectl(tkmsg, msgmenu);
	    }

	  msgpress := <-msg =>
	#	sys->print("msgpress  *********= %s\n", msgpress);
	    (n, cmdstr) := sys->tokenize(msgpress, " \t\n");
            if (cmdstr!=nil) {
             this_cmd := hd cmdstr;
	     case this_cmd {
		"closemsg" =>
		        tk->cmd(tkmsg, "destroy .; update");
	                tkmsg = nil;
	                cacheBody = "";
			tk->cmd(tktop, "focus .listing.p");
		"nextmsg" =>
			spawn nextmsg(ctxt, tktop);
		"prevmsg" =>
			spawn prevmsg(ctxt, tktop);
		"show_header" =>
			spawn show_header();
		"savemsg" =>
			savemsg(ctxt, tktop, tkmsg);
		"deletemsg" =>
			tk->cmd(tkmsg, ".mbar.msg.menu entryconfigure 5 -state disabled");
                        tk->cmd(tkmsg, ".bbar.msg.delete configure -state disabled");
			spawn deletemsg(ctxt, tktop);
		"copy" =>
			copy(tkmsg);
		"alltext" =>
			tk->cmd(tkmsg, ".body.t tag remove sel 1.0 end");
			tk->cmd(tkmsg, ".body.t tag add sel 1.0 end; update");
		"newmsg" =>
			disablnew();	
			if (compose == nil)
			  compose = load Compose Compose->PATH;
			spawn compose->initialize(ctxt, tkargs, NEW, nil, cmp, mmgr);
		"compose" =>
			disablnew();	
			if (compose == nil)
			  compose = load Compose Compose->PATH;
			spawn compose_msg(ctxt, tkargs, int hd tl cmdstr);
		"addsender" =>
			addsender();
		"addall" =>
			addall();
		"openaddress" =>
			openaddress();
               }
	  }

	 composed := <-cmp =>
	    (n, cmdstr) := sys->tokenize(composed, " \t\n");
	    case hd cmdstr {
		"done" => {
			enablnew();	
			# Deliver mail -- if possible
			if (cmptop == nil)
				cmptop = compose->get_toplevel();
			if (cmptop == nil)
				sys->print("bull\n");
			tkcmd(hd cmptop,"destroy . ;update");
			cmptop = nil;

			if (tkmsg == nil) {
				tkcmd(tktop,"raise . ;update");
				tk->cmd(tktop, "focus .listing.p");
			}else {
				tkcmd(tkmsg,"raise . ;update");
			}

			if (currentfolder=="Outbox") {
				loadmessages(ctxt, tktop, "Outbox");
			}
			if(tkmsg == nil) {
				newmail_alert(tktop, "Sending message");
				spawn sendmail(ctxt, tktop);
			}
			else {
				 newmail_alert(tkmsg, "Sending message");
				spawn sendmail(ctxt, tkmsg);
			}
	# Old version just told user to connect
	#		newmail_alert(tktop, please_connect);
		}
		"cancel" => {
			# put back top, no delivery needed
			enablnew();	
			if (cmptop == nil)
				cmptop = compose->get_toplevel();
			tkcmd(hd cmptop,"destroy . ;update");
			cmptop = nil;

			if (tkmsg == nil) {
				tkcmd(tktop,"raise . ;update");
				tk->cmd(tktop, "focus .listing.p");
			}else {
				tkcmd(tkmsg,"raise . ;update");
			}

			if (currentfolder=="Outbox") {
				loadmessages(ctxt, tktop, "Outbox");
			}
		}
	  }
	}
}
#fi : import i18n;
#init the main scre
initmain () {
          wmlib->tkcmds(tktop, gui_tk->initscreen);
          
}

#init message screen.
initmsgscreen () {
 
     wmlib->tkcmds(tkmsg, gui_tk->msgscreen);
}

# disable message and headers menu
disablmenu(t: ref Toplevel) {
	tkcmd(t, ".mbar.file.menu entryconfigure 0 -state disabled");
        tkcmd(t, ".bbar.msg.connect configure -state disabled");
	tkcmd(t, ".mbar.hdr.menu entryconfigure 0 -state disabled");
        tkcmd(t, ".mbar.hdr.menu entryconfigure 1 -state disabled");
        tkcmd(t, ".mbar.msg.menu entryconfigure 0 -state disabled");
        tkcmd(t, ".mbar.msg.menu entryconfigure 1 -state disabled");
}
################
disablnew() {
	tkcmd(tktop, ".bbar.msg.compose configure -state disabled");
	tkcmd(tktop, ".mbar.msg.menu entryconfigure 3 -state disabled;update");
	tkcmd(tkmsg, ".bbar.compose.new configure -state disabled");
	tkcmd(tkmsg, ".bbar.compose.reply configure -state disabled");
	tkcmd(tkmsg, ".bbar.compose.replyall configure -state disabled");
	tkcmd(tkmsg, ".bbar.compose.forward configure -state disabled");
	tkcmd(tkmsg, ".mbar.compose.menu entryconfigure 0 -state disabled");
	tkcmd(tkmsg, ".mbar.compose.menu entryconfigure 1 -state disabled");
	tkcmd(tkmsg, ".mbar.compose.menu entryconfigure 2 -state disabled");
	tkcmd(tkmsg, ".mbar.compose.menu entryconfigure 3 -state disabled;update"); 
}
enablnew() {
	tkcmd(tktop, ".bbar.msg.compose configure -state normal");
        tkcmd(tktop, ".mbar.msg.menu entryconfigure 3 -state normal;update");
        tkcmd(tkmsg, ".bbar.compose.new configure -state normal");
        tkcmd(tkmsg, ".bbar.compose.reply configure -state normal");
        tkcmd(tkmsg, ".bbar.compose.replyall configure -state normal");
        tkcmd(tkmsg, ".bbar.compose.forward configure -state normal");
        tkcmd(tkmsg, ".mbar.compose.menu entryconfigure 0 -state normal");
        tkcmd(tkmsg, ".mbar.compose.menu entryconfigure 1 -state normal");
        tkcmd(tkmsg, ".mbar.compose.menu entryconfigure 2 -state normal");
        tkcmd(tkmsg, ".mbar.compose.menu entryconfigure 3 -state normal;update");

}
#################333
goleft(t: ref tk->Toplevel, widgetname: string) 
{
	curindex :=tk->cmd(t, widgetname + " index insert");
	
	index := (int curindex) - 1;
	#sys->print("the KEY index is: %d\n", index);
	#sys->print("the widget name  is: %s\n", widgetname);
	tk->cmd(t, widgetname + " xview scroll -1 unit");
	tk->cmd(t, widgetname + " icursor " + string index);
	tk->cmd(t, "update");
}
goright(t: ref tk->Toplevel, widgetname: string) 
{
	curindex :=tk->cmd(t, widgetname + " index insert");
	index := (int curindex) + 1;
	#sys->print("the KEY--R index is: %d\n", index);
	#sys->print("the widget--R name  is: %s\n", widgetname);
	tk->cmd(t, widgetname + " xview scroll 1 unit");
	tk->cmd(t, widgetname + " icursor " + string index);
	tk->cmd(t, "update");
}	
dodel(t: ref tk->Toplevel, entryName: string)
{
        curindex := tk->cmd(t, entryName + " index insert");
        index := (int curindex) +1;
        tk->cmd(t, entryName + " delete "+ curindex+" "+string index);
        tk->cmd(t, "update");
}

##################3

# return 1 if outbox is full, else 0
checkoutbox(): int
{
   Alist: import asl;
   outboxtest := mmgr->dispatch( Alist.frompairs(
      (("CHECKOUTBOX", "") :: nil) ));
   if (outboxtest.getitems("FULL") != nil) {
      return 1;
   }
   return 0;
}


set_server_parameters()
{

#   sys->print("set_server_parameters: popserver=%s, mailaddr=%s\n",pop,mailaddr);
   # set the global server parameters in mmgr
   Alist: import asl;
   request := Alist.frompairs( (
	  ("USERID", uid) ::
	  ("PASSWORD", passwd) ::
	  ("POPSERVER" , pop) ::
	  ("SMTPSERVER" , smtp) ::
	  ("EMAILADDR" , mailaddr) ::
          nil ) );
   result := mmgr->dispatch(request);
   # ignore errors
}

#
# Read from isp.dat file 
# the info needed to connect to ISP
#
#read_isp() : int
#{

#}

get_user_info(ctxt: ref Draw->Context) :
    (string, string, string, string, string)
{
    ### Get default user/server info from file.  If not found, display registration panel

    (good, luid, lpasswd, lpop, lsmtp, lmailaddr) := loadcfig(mmgr);



    if (good == 1) { return (lpop, lsmtp, luid, lpasswd, lmailaddr); }
    else {
      return do_user_reg(ctxt);
    }
}

do_user_reg(ctxt: ref Draw->Context): (string, string, string, string, string)
{
  t := tk->toplevel(ctxt.screen, gui->COMPOSE_MAIN_FONT);
  #t := tk->toplevel(ctxt.screen, " -font /fonts/lucidasans/latin1.7.font -border width 1 -relief raised");
  reg := chan of string;
  tk->namechan(t, reg, "reg");


  entychan := chan of string;
  tk->namechan(t, entychan, "entychan");
  focuchan := chan of string;
  tk->namechan(t, focuchan, "focuchan");
  
  wmlib->tkcmds(t, gui_tk->regpanel);
  if (WrProgMgr == nil) {
	tkcmd(t,". configure -width 420 -height 300");
  } else {
	tkcmd(t,". configure -width 640 -height 440");
  }
  

  ### Load current values into edit boxes
  tkcmd(t, ".f.e.pop insert 0 {" + pop + "}");
  tkcmd(t, ".f.e.smtp insert 0 {" + smtp + "}");
  tkcmd(t, ".f.e.user insert 0  {" + uid + "}");
  tkcmd(t, ".f.e.secret insert 0 {" + passwd + "}");
  tkcmd(t, ".f.e.addr insert 0 {" + mailaddr + "}");
 # pop1, smtp1, uid1, passwd1, mailaddr1: string;


#
# Registration event loop
#
  for(;;) {
    tk->cmd(t, "update");
    alt{
         ###################
      s := <-focuchan =>
             (n, cmstr) := sys->tokenize(s, " \t\n");
             case hd cmstr {
             "focus" =>
                   tk->cmd(t, focusWidget + " selection clear");
                   tk->cmd(t, "update");
                   if (tl cmstr != nil)
                      focusWidget = hd tl cmstr;
                      #sys->print("HONG focuswidget is: %s\n", focusWidget);
             }
          s :=<-entychan =>
            killkeyboardbugs( t, focusWidget, s);
             
              
      ######################
	s := <-reg => {
          if ((s=="save")||(s=="ok")) {
	    ### Do error checking of entered values
  	    pop = tk->cmd(t, ".f.e.pop get");
	    smtp = tk->cmd(t, ".f.e.smtp get");
	    uid = tk->cmd(t, ".f.e.user get");
	    passwd = tk->cmd(t, ".f.e.secret get");
	    mailaddr = tk->cmd(t, ".f.e.addr get");
          
	   }             


	  case s {
          "quit" =>
	    return ("", "", "", "", "");
	  "save" =>
	      ### Save values to user/server options file
	      savecfig(mmgr, uid, passwd, pop, smtp, mailaddr);
	  "ok" =>

	    tk->cmd(t, "destroy .");
	    
	    return (pop, smtp, uid, passwd, mailaddr);
	}
      }
    }
  }
}

loadmessages(ctxt: ref Draw->Context, t: ref Toplevel, folder: string)
{
  err := "";
  currentfolder = folder;

  tmp_headers: list of ref Header;
  tmp_firstheader, tmp_lastheader : int;

  ### Indicate action on status line
  tkcmd(t, ".stat.helpline configure -text {" + getting_hdrs + "}; update");
  
  ### Read mail file, create header list, get number of messages in file,
  if (uid=="" || pop=="" || passwd=="") {
     # warn the user
    	do_user_reg(ctxt); 
	newmail_alert(t, "please configure servers");
  tkcmd(t, ".stat.helpline configure -text {get headers failed}; update");
     return;
  } else {
     (tmp_firstheader, tmp_lastheader, tmp_headers, err) = fetchheaders(mmgr, currentfolder, 0);
  }
  if(err != "")
  {
        dialog_msg(ctxt,err);
        return;
  }
  firstheader = tmp_firstheader;
  lastheader = tmp_lastheader;
  headers = tmp_headers;

  LastMsgHdr = lastheader;

  ### Load headers into listbox
  (nmesg, cmesg, err) = loadheaders(t, mmgr, nmesg, headers, listboxWidth);
  if(err != "")
  {
        dialog_msg(ctxt,err);
        return;
  }
  if (currentfolder == "Inbox") lastmsg = nmesg;

  ### Update file button text with filename
  tk->cmd(t, ".bbar.file configure -text {" + folder + "...}; update");

  #Update Headers menu buttons, this is a new staff
  updatehdr(t, firstheader, LastMsgHdr);

  ### Indicate action on status line
  tkcmd(t, ".stat.helpline configure -text {}");
  updatemsg(ctxt, t);
  if (checkoutbox()) {
     newmail_alert(t, please_connect); # outbox alert
  }
}

prevhdrs(ctxt: ref Draw->Context, t: ref Toplevel)
{
    err := "";
    if( lastheader > HeaderWindow )   # This is a new staff
        LastMsgHdr -= HeaderWindow;
    if(LastMsgHdr < HeaderWindow )
        LastMsgHdr =  HeaderWindow;   # This is a new staff
#    if (LastMsgHdr < 1) LastMsgHdr = 1;
  
    ### Indicate action on status line
    tkcmd(t, ".stat.helpline configure -text {" + getting_hdrs + "}; update");
  
    ### Read mail file, create header list, get number of messages in file,
    if (uid=="" || pop=="" || passwd=="") {
	# warn the user
	newmail_alert(t, "please configure servers");
	tkcmd(t, ".stat.helpline configure -text {get headers failed}; update");
	return;
    } else {
	(firstheader, lastheader, headers, err) = fetchheaders(mmgr, currentfolder, LastMsgHdr);
    }
    if(err != "")
    {
        dialog_msg(ctxt,err);
	tkcmd(t, ".mbar.file.menu entryconfigure 0 -state normal");
    	tkcmd(t, ".bbar.msg.connect configure -state normal");
	tkcmd(t, ".mbar.hdr.menu entryconfigure 0 -state normal");
        tkcmd(t, ".mbar.hdr.menu entryconfigure 1 -state normal");
        return;
    }
    LastMsgHdr = (firstheader + HeaderWindow)  - 1;
    if ( LastMsgHdr > lastheader )    # This is new staff
        LastMsgHdr = lastheader;      # This is new staff

    ### Load headers into listbox
    (nmesg, cmesg, err) = loadheaders(t, mmgr, nmesg, headers, listboxWidth);
    if(err != "")
    {
        dialog_msg(ctxt,err);
        return;
    }
    if (currentfolder == "Inbox") lastmsg = nmesg;

    ### Indicate action on status line
    tkcmd(t, ".stat.helpline configure -text {}");

    # Update header menu
    updatehdr(t, firstheader, LastMsgHdr); #This is a new staff
    tkcmd(t, ".mbar.file.menu entryconfigure 0 -state normal");
    tkcmd(t, ".bbar.msg.connect configure -state normal");

    updatemsg(ctxt, t);
    if (checkoutbox()) {
	newmail_alert(t, please_connect); # outbox alert
    }
}

nexthdrs(ctxt: ref Draw->Context, t: ref Toplevel)
{
    err := "";
    LastMsgHdr += HeaderWindow;
    if (LastMsgHdr > lastheader) LastMsgHdr = lastheader;
  
    ### Indicate action on status line
    tkcmd(t, ".stat.helpline configure -text {" + getting_hdrs + "}; update");
  
    ### Read mail file, create header list, get number of messages in file,
    if (uid=="" || pop=="" || passwd=="") {
	# warn the user
	newmail_alert(t, "please configure servers");
	tkcmd(t, ".stat.helpline configure -text {get headers failed}; update");
	return;
    } else {
	(firstheader, lastheader, headers, err) = fetchheaders(mmgr, currentfolder, LastMsgHdr);
    }
    if(err != "")
    {
        dialog_msg(ctxt,err);
	tkcmd(t, ".mbar.file.menu entryconfigure 0 -state normal");
        tkcmd(t, ".bbar.msg.connect configure -state normal");
        tkcmd(t, ".mbar.hdr.menu entryconfigure 0 -state normal");
        tkcmd(t, ".mbar.hdr.menu entryconfigure 1 -state normal");
        return;
    }
    LastMsgHdr = (firstheader + HeaderWindow)  - 1;
    if( LastMsgHdr > lastheader )   # This is a new staff
        LastMsgHdr = lastheader;    # This is a new staff

    ### Load headers into listbox
    (nmesg, cmesg, err) = loadheaders(t, mmgr, nmesg, headers, listboxWidth);
    if(err != "")
    {
        dialog_msg(ctxt,err);
        return;
    }
    if (currentfolder == "Inbox") lastmsg = nmesg;

    ### Indicate action on status line
    tkcmd(t, ".stat.helpline configure -text {}");

    # Update header menu
    updatehdr(t, firstheader, LastMsgHdr); # This is a new staff
    tkcmd(t, ".mbar.file.menu entryconfigure 0 -state normal");
    tkcmd(t, ".bbar.msg.connect configure -state normal");

    updatemsg(ctxt, t);
    if (checkoutbox()) {
	newmail_alert(t, please_connect); # outbox alert
    }
}

updatehdr(t : ref Toplevel, first, last : int)
{
    state := "normal";
    if (first <= 1)	state = "disabled";
    tk->cmd(t, ".mbar.hdr.menu entryconfigure 0 -state "+state);
    state = "normal";
    if (last >= lastheader)	state = "disabled";
    tk->cmd(t, ".mbar.hdr.menu entryconfigure 1 -state "+state);
  
    ### Update message count label text
    nmesgs := "";
    if (currentfolder == "Inbox")
	nmesgs = sys->sprint("Total messages: %d", lastheader);
    tk->cmd(tktop, ".bbar.msgcount configure -text '" + nmesgs);
}


### This function should be called with flag = 1
### whenever the automated query detects new mail on the server.
###   flag = 2 for "no new mail" message.
###   flag = 3 for "outbox full" message.
newmail_alert(t: ref Toplevel, message: string)
{
  tk->cmd(t, ".stat.newmail configure -text {" + message + "}; update");
}

connect(ctxt: ref Draw->Context, t: ref Toplevel)
{
#
# Sign on user
# Set server parameters (& connect?)
#
  if (pop == "" || smtp == "" || uid == "" || passwd == "" || mailaddr == ""){
	tkcmd(tktop, ".stat.helpline configure -text " +
	      	     " {" + signing_on + "}; update");
	(pop, smtp, uid, passwd, mailaddr) = get_user_info(ctxt);
       	set_server_parameters();
	tkcmd(tktop, ".stat.helpline configure -text {}; update");
  }

    tkcmd(t, ".bbar.msg.compose configure -state normal");
    loadmessages(ctxt, t, "Inbox");

    tkcmd(t, ".mbar.msg.menu entryconfigure 0 -state disabled");
    tkcmd(t, ".mbar.msg.menu entryconfigure 1 -state disabled");
    #tkcmd(t, ".mbar.msg.menu entryconfigure 3 -state normal;update");

    tkcmd(t, ".mbar.hdr.menu entryconfigure 0 -state normal");
    tkcmd(t, ".mbar.hdr.menu entryconfigure 1 -state normal");
	
    tkcmd(t, ".mbar.file.menu entryconfigure 0 -state normal"); 
    tkcmd(t, ".bbar.msg.connect configure -state normal");	
    
    tkcmd(t, ".stat.helpline configure -text {}; update");
    tkcmd(t, ".stat.newmail configure -text {}; update");
 

}
############
sendmail(ctxt: ref Draw->Context, t: ref Toplevel)
{

    tkcmd(t, ".stat.helpline configure -text {" + contacting_server + "}; update");

    (newmail,err) := do_connect(ctxt,  mmgr);
    if (newmail & SENDCOMPLETE) 
	tkcmd(t, ".stat.helpline configure -text {" + messages_sent + "}; update");
    else if (newmail & SENDERROR) {
	tkcmd(t, ".stat.helpline configure -text {" + send_error + "}; update");
	#clear sending message
	tkcmd(t, ".stat.newmail configure -text {}; update");
	dialog_msg(ctxt,err);
	return;
    }
    tkcmd(t, ".bbar.msg.compose configure -state normal");
    #tkcmd(t, ".mbar.msg.menu entryconfigure 0 -state normal");
    tkcmd(t, ".mbar.hdr.menu entryconfigure 0 -state normal");
    #tkcmd(t, ".mbar.msg.menu entryconfigure 1 -state normal");
    tkcmd(t, ".mbar.hdr.menu entryconfigure 1 -state normal");
    #tkcmd(t, ".mbar.msg.menu entryconfigure 3 -state normal;update");
    tkcmd(t, ".stat.helpline configure -text {}; update");
    tkcmd(t, ".stat.newmail configure -text {}; update");	


}
#############
newfile(t: ref Toplevel)
{
  newfile := wmlib->getstring(t, "Filename");

  ### Create file in mailbox directory

  tk->cmd(t, ".stat.helpline configure -text {"+newfile+" folder created}; update");
}

openfile(ctxt: ref Draw->Context, t: ref Toplevel)
{
  folder := selectfile(ctxt, t);
  if (folder != "")
    loadmessages(ctxt, t, folder);
}

selectfile(ctxt: ref Draw->Context, parent: ref Toplevel): string
{
  folder: string;

#
# Setup mail header screen
#
  t: ref Tk->Toplevel;
  titlebut: chan of string;

  if (WrProgMgr == nil) {
  	tna := wmlib->geom(parent)+" -font /fonts/lucidasans/latin1.7.font " + 
		"-borderwidth 1 -relief raised";
	(t, titlebut) = wmlib->titlebar(ctxt.screen, tna, "Folders", Wmlib->Appl);
  } else {
 	 t = tk->toplevel(ctxt.screen, " -borderwidth 1 -relief raised" +
		" -font /fonts/lucidasans/latin1.7.font ");
	titlebut = chan of string;
	tk->namechan(t, titlebut, "titlebut");
 }

  chn := chan of string;
  tk->namechan(t, chn, "chn");

  wmlib->tkcmds(t, gui_tk->main_foldertab);
  tk->cmd(t, "grab set .");
  addfilenames(t);
  doubleclickflag := 0;
#
# Mail message event loop
#
  for(;;){
    tk->cmd(t, "update");
    alt{
      s := <-titlebut =>
	if(s[0] == 'e')
	  return "";
        wmlib->titlectl(t, s);
      s := <-chn =>
	case s{
	"doubleclick" =>
	  doubleclickflag = 1;
	"buttonup" =>
	  if (doubleclickflag == 1) {
	    folder = getselection(t);
	    if (folder != "") {
	      return folder;
	    } else
	      doubleclickflag = 0;
	  } else {
	      if (cmptop == nil)
                   cmptop = compose->get_toplevel();
              if (cmptop == nil)
                    sys->print("bull\n");
              tkcmd(hd cmptop,"destroy . ;update");
              cmptop = nil;		    
       	     	compose = nil; 
		sel := tk->cmd(t, ".names.p curselection");
	      if(sel == "")
	        tk->cmd(t, ".buts.ok configure -state disabled; update");
	       else
	         tk->cmd(t, ".buts.ok configure -state normal; update");
	  }
	"cancel" =>
	  return "";
	"ok" =>
	  return getselection(t);
      }
    }
  }
}

addfilenames(t: ref Tk->Toplevel)
{
### Add folder names (and sizes?) from mailbox directory, with 4 default boxes first, 
### followed by user defined folders in alphabetical order

  tk->cmd(t, ".names.p insert end Inbox");
  tk->cmd(t, ".names.p insert end Outbox");
  #tk->cmd(t, ".names.p insert end Sent");
  #tk->cmd(t, ".names.p insert end Deleted");
}

getselection(t: ref Tk->Toplevel): string
{
  sel := tk->cmd(t, ".names.p curselection");
  if(sel == "")
    return "";
  name := tk->cmd(t, ".names.p get "+sel);
  tk->cmd(t, "destroy .");
  return name;
}

deletefile(ctxt: ref Draw->Context, t: ref Toplevel)
{
  if (nmesg > 0) {
    s := sys->sprint("The %s file contains\n%d messages that will be lost\nif this file is deleted.", currentfolder, nmesg);
    if (wmlib->dialog(t, "error -fg red", "mail",
          s, 0, "Cancel" :: "Delete" :: nil) == 0)
      return;
  }

  ### Delete file

  tk->cmd(t, ".stat.helpline configure -text {"+currentfolder+" folder deleted}; update");
  loadmessages(ctxt, t, "Inbox");
}

emptytrash()
{
  ### Delete contents of Deleted file
}

select(ctxt: ref Draw->Context, t: ref Toplevel)
{
  showLongHeaders = 0;
  direction = 0;
  sel := tk->cmd(t, ".listing.p curselection");
  if( sel != "" && int sel  == 0)
    tk->cmd(t, ".listing.p selection clear " + sel);
  if(sel == "") {
    cmesg = -1;
    updateselectbuttons(t);
  }
  else if (int sel  == cmesg)
    updatemsg(ctxt,t);
  else{
    cmesg = int sel;
    updateselectbuttons(t);
  }
}

nextmsg(ctxt: ref Draw->Context, t: ref Toplevel)
{
  showLongHeaders = 0;
  direction = 1;
  if (cmesg == -1)
    cmesg = 0;
  else
    cmesg++;
#    tk->cmd(t, ".listing.p selection clear " + string cmesg++);
  updatemsg(ctxt, t);
}

prevmsg(ctxt: ref Draw->Context, t: ref Toplevel)
{
  showLongHeaders = 0;
  direction = -1;
  if (cmesg == -1)
    cmesg = nmesg - 1;
  else
    cmesg--;
#    tk->cmd(t, ".listing.p selection clear " + string cmesg--);
  updatemsg(ctxt, t);
}

updatemsg(ctxt: ref Draw->Context, t: ref Toplevel)
{
  updateselectbuttons(t);
  if (cmesg == -1 && tkmsg == nil)
    return;

  tmesg := string (cmesg);
  tk->cmd(t, ".listing.p selection clear 0 end");
  tk->cmd(t, ".listing.p selection set " +tmesg);

  if (tkmsg == nil)
    initmsgwin(ctxt);
  else {
    if (cmesg == -1){
      tk->cmd(tkmsg, "destroy .; update");
      tkmsg = nil;
    }
    else {
      tk->cmd(tkmsg, ". map; raise .");
      tk->cmd(tkmsg, ".bbar.go.prev configure -state disabled");
      tk->cmd(tkmsg, ".bbar.go.next configure -state disabled");
      tk->cmd(tkmsg, ".mbar.msg.menu entryconfigure 0 -state disabled");
      tk->cmd(tkmsg, ".mbar.msg.menu entryconfigure 1 -state disabled");
      displaymessage(cmesg);
    }
  }

  ### update message window title
  if( tkmsg != nil)
    updatemsgbuttons(tkmsg);
}

initmsgwin(ctxt: ref Draw->Context)
{
  rect := ctxt.screen.image.r;
  win_width := rect.dx();
  win_height := rect.dy();
  if (win_width > MAXWIDTH)
    win_width = MAXWIDTH;
  if (win_height > MAXHEIGHT)
    win_height = MAXHEIGHT;
  width := string win_width;
  height := string win_height;

#
# Put up message screen
#

  #tna := "-font /fonts/lucidasans/latin1.7.font";
  tna := gui->MAIN_WHERE+" -font "+gui->MESSAGE_FONT;
  (tkmsg, msgmenubut) = wmlib->titlebar(ctxt.screen, tna, "Mail", Wmlib->Appl);
  tkcmd(tkmsg, "pack .Wm_t -side top -fill x;" +
	     ".Wm_t.title configure -text {"+gui->MESSAGE_TITLE+"}");
  initmsgscreen(); 
  tkcmd(tkmsg, " . configure -relief raised -width "+string gui->screenX+" -height "+string gui->screenY + ";update");
  msg = chan of string;
  tk->namechan(tkmsg, msg, "msg");

  displaymessage(cmesg);
  tk->cmd(tkmsg, ".bbar.go.prev configure -state normal");
  tk->cmd(tkmsg, ".bbar.go.prev configure -state normal");
  tk->cmd(tkmsg, ".mbar.msg.menu entryconfigure 0 -state normal");
  tk->cmd(tkmsg, ".mbar.msg.menu entryconfigure 1 -state normal");
}

nth_header(n : int , hlist : list of ref Header) : ref Header
{
    if (n > (len hlist - 1)) return nil;
    for (i:=0; i<n; i++) hlist = tl hlist;
    return hd hlist;
}

displaymessage(cmesg : int)
{
    hstring : string;
    err := "";

    header := nth_header(cmesg, headers);
    if (header != nil) {
    	if (!showLongHeaders ) (hstring,err) = short_header(header, mmgr);
    	else                   hstring = header.fullHeader;

    	### Indicate action on status line
    	tkcmd(tktop, ".stat.helpline configure -text {" + gui->status_indicator + "}; update");

    	msgnum := realmsgnum(cmesg);
    	cacheBody = fetch_body(msgnum,  mmgr);
   	# cacheBody = fetch_body(cmesg+1,  mmgr);

    	tkcmd(tktop, ".stat.helpline configure -text {}; update");

    	redisplaymessage(hstring, cacheBody);
    }
}

realmsgnum(cmesg: int) : int
{
    msgnum : int;
    selstr := tk->cmd(tktop, ".listing.p get " + string (cmesg) );
#    selstr = selstr[0:3];
    (n, l) := sys->tokenize(selstr, " ");
    if (l != nil) {
        mnum := hd l;
        msgnum = int mnum;
    }
    return msgnum;
}

redisplaymessage(header : string, body : string)
{
    tkcmd(tkmsg, ".body.t delete 1.0 end");
    tkcmd(tkmsg, ".body.t insert end {" + header + body + "}");

    ### To do:
    ### Mark any URLs with a tag and display in blue underlined text
    ### Bind mouse clicks on URLs to bring up browser or compose window
    ### as appropriate

    tkcmd(tkmsg, "update");
}

updateselectbuttons(t: ref Toplevel)
{
  if (cmesg == 0 || nmesg == 0)
    tk->cmd(t, ".mbar.msg.menu entryconfigure 1 -state disabled");
  else
    tk->cmd(t, ".mbar.msg.menu entryconfigure 1 -state normal");

  if (cmesg == nmesg - 1)
    tk->cmd(t, ".mbar.msg.menu entryconfigure 0 -state disabled");
  else
    tk->cmd(t, ".mbar.msg.menu entryconfigure 0 -state normal");

  if (cmesg == -1)
    wmlib->tkcmds(t, noselect);
  else{
    wmlib->tkcmds(t, curselect);
    tmesg := string (cmesg);
    tk->cmd(t, ".listing.p selection set" + tmesg);
    tk->cmd(t, ".listing.p see " + tmesg);
  }
  tk->cmd(t, "update");  
}

updatemsgbuttons(t: ref Toplevel)
{
  if (cmesg == 0 || nmesg == 0) {
    tk->cmd(t, ".mbar.msg.menu entryconfigure 1 -state disabled");
    tk->cmd(t, ".bbar.go.prev configure -state disabled");
  } else {
    tk->cmd(t, ".mbar.msg.menu entryconfigure 1 -state normal");
    tk->cmd(t, ".bbar.go.prev configure -state normal");
  }

  if (cmesg == nmesg - 1) {
    tk->cmd(t, ".mbar.msg.menu entryconfigure 0 -state disabled");
    tk->cmd(t, ".bbar.go.next configure -state disabled");
  } else {
    tk->cmd(t, ".mbar.msg.menu entryconfigure 0 -state normal");
    tk->cmd(t, ".bbar.go.next configure -state normal");
  }

  if (cmesg == -1)
    wmlib->tkcmds(t, msgnoselect);
  else
    wmlib->tkcmds(t, msgcurselect);
  tk->cmd(t, "update");    
}

show_header()
{
    showLongHeaders = !showLongHeaders;
    header := nth_header(cmesg, headers);
    if (!showLongHeaders ) (hstring,err) := short_header(header, mmgr);
    else                   hstring = remove_crs(header.fullHeader);

    redisplaymessage(hstring, cacheBody);
}

savemsg(ctxt: ref Draw->Context, t, parent: ref Toplevel)
{
    
    dummy := (t, parent);

    daytime := load Daytime Daytime->PATH;
    if ( daytime == nil ) {
      dialog_msg(ctxt, gui->dialog_msg_main1);
      return;
    }
    timeint := daytime->now();
    str := string timeint;
    filename := "mailID:" + str;
    mailfd := sys->create(filename, sys->OWRITE, 8r600);
    if (mailfd == nil ) {
       dialog_msg(ctxt, gui->dialog_msg_main2);
       return;
     }
    # Get the needed fields for the note
    who : string;
    me : string;
    subject : string;
    header := nth_header(cmesg, headers);
    request := Alist.frompairs
	((("PARSEHDR", header.fullHeader) :: nil));
    result := mmgr->dispatch(request);
    errormsg := result.getitems("ERROR");
    if (errormsg != nil) {
        dialog_msg(ctxt, gui->dialog_msg_main3+hd errormsg);
	return;
    }
    tolist := result.getitems("To");
    if ( tolist == nil ) me = uid;
    else		 me = hd tolist;
    me = "address:=" + me +"\n";
    senderlist := result.getitems("From");
    if (senderlist == nil) who = "unknown";
    else                   who = hd senderlist;
    who = "Mail From: " + who + "\n\n";
    subjlist := result.getitems("Subject");
    if (subjlist == nil) subject = "(No subject)";
    else                 subject = hd subjlist;
    subject = "Subject: " + subject + "\n\n";
    buff1 := array of byte who;
    n1 := len buff1;
    buff2 := array of byte subject;
    n2 := len buff2;
    buff3 := array of byte me;
    n3 := len buff3;
    buff4 := array of byte cacheBody;
    n4 := len buff4;
    
    
   
    i3 := sys->write(mailfd, buff3, n3);
    i1 := sys->write(mailfd, buff1, n1);
    i2 := sys->write(mailfd, buff2, n2);
    i4 := sys->write(mailfd, buff4, n4);
    
    if ( (i1 != n1) | (i2 != n2) | (i3 != n3) | (i4 != n4)) {
       sys->print("save mail not complete");
    } 
    
   
}

deletemsg(ctxt: ref Draw->Context, t: ref Toplevel)
{
    err := "";
    dummy : int;
    
    # Move message to trash
   # headers = delete_msg(ctxt, mmgr, cmesg+1, headers);
    msgnum := realmsgnum(cmesg);
    headers = delete_msg(ctxt,  mmgr, cmesg+1, msgnum, headers);

    # Renumber headers and refresh the listbox
    renumber_headers(headers, firstheader);
    (nmesg, dummy, err) = loadheaders(t, mmgr, nmesg, headers, listboxWidth);
    if(err != "")
    {
        dialog_msg(ctxt,err);
        return;
    }

    if (cmesg == nmesg)
	cmesg--;
    if (tkmsg != nil)
    	updatemsg(ctxt, t);
}

compose_msg(ctxt: ref Draw->Context, tkargs: list of string, msgtype: int)
{

    ### Get header elements and message text to pass to Compose application
    header := nth_header(cmesg, headers);
    mmsg := get_msgargs(header,  mmgr);
    mmsg.text = cacheBody;
    spawn compose->initialize(ctxt, tkargs, msgtype, mmsg, cmp, mmgr);
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

serveropts(ctxt: ref Draw->Context, parent: ref Tk->Toplevel)
{
  tna := wmlib->geom(parent)+
   gui->COMPOSE_MAIN_FONT;
#" -font /fonts/lucidasans/latin1.7.font -borderwidth 1 -relief raised";
  svr := chan of string;

  (t, titlebut) := wmlib->titlebar(ctxt.screen, tna, gui->SERVER_OPTIONS, 0);
  tk->namechan(t, svr, "svr");
  ###############
  entychan := chan of string;
  tk->namechan(t, entychan, "entychan");
  focuchan := chan of string;
  tk->namechan(t, focuchan, "focuchan");
  ###################
  wmlib->tkcmds(t, gui_tk->svrpanel);
  tk->cmd(t, "grab set .");
  tk->cmd(t, ".smtp_entry insert end " + smtp);
  tk->cmd(t, ".pop_entry insert end " + pop);

  if (mailcheck == -1)
    tk->cmd(t, ".never invoke");
  else {
    tk->cmd(t, ".interval invoke");
    tk->cmd(t, ".ck_entry insert end " + string mailcheck);
  }

#
# Server options event loop
#
  for(;;) {
    tk->cmd(t, "update");
    alt{
      ####################
      s := <-focuchan =>
             (n, cmstr) := sys->tokenize(s, " \t\n");
             case hd cmstr {
             "focus" =>
                   tk->cmd(t, focusWidget + " selection clear");
                   tk->cmd(t, "update");
                   if (tl cmstr != nil)
                      focusWidget = hd tl cmstr;
                      #sys->print("HONG focuswidget is: %s\n", focusWidget);
             }
          s :=<-entychan =>
            killkeyboardbugs( t, focusWidget, s);
             
      ##################33
      s := <-titlebut =>
	if(s[0] == 'e')
	  return;
        wmlib->titlectl(t, s);
      s := <-svr =>
	case s {
	"cancel" =>
	  return;
	"ok" =>
	  smtp = tk->cmd(t, ".smtp_entry get");
	  pop = tk->cmd(t, ".pop_entry get");
	  #mailcheck = int tk->cmd(t, "variable mailcheck");
	  #if (mailcheck != -1){
	  #  interval := tk->cmd(t, ".ck_entry get");
	  #  if (interval == nil)
	  #    mailcheck = 0;
	  #  else
	  #    mailcheck = int interval;
	  #}
          set_server_parameters();
	  tk->cmd(t, "destroy .");
	  return;
        }
    }
  }
}

useropts(ctxt: ref Draw->Context, parent: ref Tk->Toplevel)
{
  tna := wmlib->geom(parent)+
   " -font /fonts/lucidasans/latin1.7.font -borderwidth 1 -relief raised";
  usr := chan of string;

  (t, titlebut) := wmlib->titlebar(ctxt.screen, tna, gui->USER_OPTIONS_TITLE, 0);
  ##################
  entychan := chan of string;
  tk->namechan(t, entychan, "entychan");
  focuchan := chan of string;
  tk->namechan(t, focuchan, "focuchan");
   
  ###########################
  tk->namechan(t, usr, "usr");
  wmlib->tkcmds(t, gui_tk->usrpanel);
  tk->cmd(t, "grab set .");
  tk->cmd(t, ".addr_entry insert end " + mailaddr);
  tk->cmd(t, ".uid_entry insert end " + uid);
  tk->cmd(t, ".pwd_entry insert end " + passwd);

#
# User options event loop
#
  for(;;) {
    tk->cmd(t, "update");
    alt{
      ###################
      s := <-focuchan =>
             (n, cmstr) := sys->tokenize(s, " \t\n");
             case hd cmstr {
             "focus" =>
                   tk->cmd(t, focusWidget + " selection clear");
                   tk->cmd(t, "update");
                   if (tl cmstr != nil)
                      focusWidget = hd tl cmstr;
                     # sys->print("HONG focuswidget is: %s\n", focusWidget);
             }
          s :=<-entychan =>
            killkeyboardbugs( t, focusWidget, s);
             
              
      ######################
      s := <-titlebut =>
	if(s[0] == 'e')
	  return;
        wmlib->titlectl(t, s);
      s := <-usr =>
	case s {
	"cancel" =>
	  return;
	"ok" =>
	  mailaddr = tk->cmd(t, ".addr_entry get");
	  uid = tk->cmd(t, ".uid_entry get");
	  passwd = tk->cmd(t, ".pwd_entry get");
	  tk->cmd(t, "destroy .");
          set_server_parameters();
	  return;
        }
    }
  }
}

#################3
killkeyboardbugs( t: ref tk->Toplevel, focu: string, s: string)
{
   #sys->print("the string hong is: %s\n", s);
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
         DOW or SCREENUP or  PINTS or NUMLOC or CALLSE or KF1 or
         KF2 or KF3 or KF4 or KF5 or KF6 or KF7 or KF8 or KF9 or KF10 or
         KF11 or KF12 or KF13 =>
           tkcmd(t, "update");
         LEF =>
           goleft(t, focu);
         RIG  =>
           goright(t, focu);
	 DEL =>
	   dodel(t, focu);
       }
 }
#####################
help(ctxt: ref Draw->Context, t: ref Tk->Toplevel)
{
    ctxt=ctxt;
    t=t;
    ### Pop up Help window
}

copy(t: ref Tk->Toplevel)
{
  if (tk->cmd(t, ".body.t tag ranges sel") == "")
    return;
  wmlib->snarfput(tk->cmd(t, ".body.t get sel.first sel.last"));
}

# Execute a tk->cmd and catch errors
tkcmd(t: ref Toplevel, s: string): string
{
	res := tk->cmd(t, s);
	if((len string res > 0) && string res[0] == "!") {
	    sys->print("mygui: tk error executing '%s': %s\n", s, res);
	}
	return res;
}

resize_main( ctxt : ref Draw->Context, tktop : ref Toplevel)
{
    err := "";

    # Get the new size for the listbox
    newWidth := int (int tkcmd(tktop, ".listing.p cget -actwidth") / listfontWidth);
    newHeight := int (int tkcmd(tktop, ".listing.p cget -actheight") / listfontHeight);

    if (newWidth != listboxWidth) {
	# Reload the listbox
	(nmesg, cmesg, err) = loadheaders(tktop, mmgr, nmesg, headers, newWidth);
	updatemsg(ctxt, tktop);
    }
    if(err != "")
    {
        dialog_msg(ctxt,err);
        return;
    }

    listboxWidth = newWidth;
    listboxHeight = newHeight;
}

finish()
{
	# Place destroy on the toolbar channel
	if (WrProgMgr != nil)
		WrProgMgr <-= "destroy";	

	fd := sys->open("#p/" + string pgrp + "/ctl", sys->OWRITE);
	if(fd != nil) {
		sys->fprint(fd, "killgrp");
	}
	exit;
}
#mailtool_notecmd := array[] of {
#        "frame .f",
#        "button .b -text "+gui->DIALOG_MSG_OK+" -command {send popup OK}",
#        "focus .f",
#        "bind .f <Key-\n> {send popup OK}",
#        "pack .f.m -side left -expand 1 -padx 10 -pady 10",
#        "pack .f .b -padx 10 -pady 10",
#        "update; cursor -default"
#n};


dialog_msg(ctxt :ref Draw->Context ,msg : string)
{
   t:= tk->toplevel(ctxt.screen, 
	#	"-x 70 -y 70 -borderwidth 2 -relief raised");
                 #" -font /fonts/lucida/unicode.7.font"
#shenxinguo
                 " -font "+gui->DIALOG_MSG_FONT
                 + "  -x 150 -y 200 -bd 2 -relief raised");
        popup = chan of string;
        tk->namechan(t, popup, "popup");
        tk->cmd(t, "label .f.m -text {"+msg+"}");
        for(i := 0; i < len gui_tk->mailtool_notecmd; i++)
                tk->cmd(t, gui_tk->mailtool_notecmd[i]);

#        tpopup = tk->toplevel(ctxt.screen,
#                 " -font /fonts/lucida/unicode.7.font"
#                 + "  -x 150 -y 200 -bd 2 -relief raised");
#        popup = chan of string;
#        tk->namechan(tpopup,popup,"popup");
#        wmlib->tkcmds(tpopup, popupmsg);
#        tk->cmd(tpopup,".lbl configure -text {" + msg +"}");
#
#        tk->cmd(tpopup,"focus .ok;update");
#        tk->cmd(tpopup, "update");
          alt {
            s := <-popup =>
             (n, char) := sys->tokenize(s, " ");
              case hd char {
              "OK" =>
                 tk->cmd(tpopup, "destroy .;update");
                  tpopup = nil; 
            } 
         }    
       return;
}

msgbox(ctxt: ref Draw->Context, msg:string, but1txt,but2txt,button1,button2: string)
{

        tmsg = tk->toplevel(ctxt.screen,
                 " -font /fonts/lucida/unicode.7.font"
                 + "  -x 150 -y 200 -bd 2 -relief raised");
        usermsg = chan of string;
        tk->namechan(tmsg,usermsg,"usermsg");
        wmlib->tkcmds(tmsg, umsg);
        tk->cmd(tmsg,".but1 configure -text {" + but1txt +"} -command {send usermsg "+button1+"}");
        tk->cmd(tmsg,".but2 configure -text {" + but2txt +"} -command {send usermsg "+button2+"}");
        tk->cmd(tmsg,".lbl configure -text {" + msg +"}");
        tk->cmd(tmsg,"update");

        return;
}
#SDP#
