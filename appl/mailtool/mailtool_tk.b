implement Mailtool_Tk;

include "mailtool_tk.m";

include "sys.m";
include "draw.m";
include "tk.m";
include "gdispatch.m";
include "assclist.m";

include "mailtool_gui.m";
gui: Mailtool_GUI;

include "compose.m";
    compose:Compose;

   NEW,REPLY, REPLYALL,FORWARD,LITERAL:import compose;

init()
{

if (gui==nil) {

   gui = load Mailtool_GUI Mailtool_GUI->PATH;
   gui->init();

}


popupmsg = array[] of {
    "frame .f -borderwidth 2 -relief flat -padx 3 -pady 3",
    "frame .fbrdr",
    "pack .fbrdr -in .f",
    "frame .flabel",
    "frame .fbut",
    "pack .flabel -in .fbrdr",
    "pack .fbut -in .fbrdr -pady 5",
    "label .lbl",
    "button .ok -text {"+ gui->BUTTON_OK +"} -command {send popup OK}",
    "pack .lbl -side left -pady 10 -in .flabel",
    "pack .ok -side left -padx 5 -in .fbut",
    "pack .f",
    "focus .f",
    "update; cursor -default",
};


# registration panel
#regpanel = array[] of {
#	"frame .all",
#	"frame .i",
#        "frame .f",
#        "frame .b",
#	"frame .l",
#	"frame .e",
#	"label .i.instruct  -text {"+gui->REG_TITLE + "} -font " + #gui->REG_FONT,
#	"pack .i.instruct -fill x",
#	"label .l.pop -text {"+gui->REG_POPSERVER+ "}  -anchor e",
#	"label .l.smtp -text {"+gui->REG_SMTPSERVER+"}  -anchor e",
#	"label .l.user -text {"+gui->REG_USERLOGIN+"}  -anchor e",
#	"label .l.secret -text {"+gui->REG_PASSWORD+"}  -anchor e",
#	"label .l.addr -text {"+gui->REG_EMAILADDRESS+"}  -anchor e#",
#	"pack .l.pop .l.smtp .l.user .l.secret .l.addr  -fill both #-pady 4",
#	"entry .e.pop",
#	"entry .e.smtp",
#	"entry .e.user",
#	"entry .e.secret -show *",
#	"entry .e.addr",
#	"pack .e.pop .e.smtp .e.user .e.secret .e.addr -fill both",
#        "pack .e.pop .e.smtp .e.user .e.secret .e.addr -fill both"#,
#        
#        "bind .e.pop <FocusIn> {send focuchan focus %W}",
#        "bind .e.pop <Key> {send entychan  {%A}}",
#	"bind .e.pop <Key-\n> {focus .e.smtp}",
#	"bind .e.pop <Control-i> {focus .e.smtp}",
#	"bind .e.smtp <FocusIn> {send focuchan focus %W}",
#	"bind .e.smtp <Key> {send entychan  {%A}}",
#	"bind .e.smtp <Key-\n> {focus .e.user}",
#	"bind .e.smtp <Control-i> {focus .e.user}",#
#
#	"bind .e.user <FocusIn> {send focuchan focus %W}",
#	"bind .e.user <Key> {send entychan  {%A}}",
#	"bind .e.user <Key-\n> {focus .e.secret}",
#	"bind .e.user <Control-i> {focus .e.secret}",
#	"bind .e.secret <FocusIn> {send focuchan focus %W}",
#	"bind .e.secret <Key> {send entychan  {%A}}",
#	"bind .e.secret <Key-\n> {focus .e.addr}",
#	"bind .e.secret <Control-i> {focus .e.addr}",
#	"bind .e.addr <FocusIn> {send focuchan focus %W}",
#	"bind .e.addr <Key> {send entychan  {%A}}",
#	"bind .e.addr <Key-\n> {focus .e.pop}",
#	"bind .e.addr <Control-i> {focus .f.e.pop}",
#	#"pack .e.pop .e.smtp .e.user .e.secret .e.addr -fill both"#,
#       
      
#	"pack .l .e -side left -in .f",
#	"frame .b.default -relief sunken -bd 1 ",
#	"button .b.quit -text {"+gui->REG_NOCHANGE+"} -width 20w  -#command {send reg quit}",
#	"button .b.save -text {"+gui->REG_SAVE+"} -width 10w  -comm#and {send reg save}",
#	"button .b.ok -text {"+gui->REG_OK+"} -width 10w  -command #{send reg ok}",
#	
#	"pack .b.default.ok .b.save .b.quit -side right -padx 8 -an#chor e",
#	"pack .b.default -side right -padx 8 -pady 8 -ahchor e",
#	"pack .b.quit -side right -padx 8 -anchor e",
#	"pack .b.save -side right -padx 8",
#	"pack .b.ok -in .b.default -side right -padx 4 -pady 4",
#	"pack .b.default -side right -padx 8 -pady 8 -anchor e",
#	"pack .Wm_t -side top -fill x ",
#	"pack .all",
#	"pack .i -fill x -padx 6 -pady 26 -in .all -side top",
#	"pack .f -fill both -padx 6 -pady 6 -in .all -side top",
#	"pack .b -fill x -padx 6  -in .all -side top",
#	"pack propagate . no ",
#	"focus .e.pop",
#	". configure -bd 1 -relief raised",
#	". configure -bd 1 -relief raised -width 320 -height 240",
#	"update",
#};


# server options panel
svrpanel = array[] of {
        "frame .o -bd 1 -relief groove",
        "frame .i -bd 1 -relief groove",
        "frame .b -bd 1 -relief groove",
	"label .o.hdr -text {"+gui->SVR_OUTGOING+"} -font "+gui->SVR_FONT,
	"label .smtp_lbl -text {"+gui->SVR_SMTPSERVER+"}",
	"entry .smtp_entry -width 22w",
	"pack .o.hdr -pady 2 -anchor w",
	"pack .smtp_lbl -in .o -side left -pady 2 -anchor e",
	"pack .smtp_entry -in .o -side left -fill x -padx 4 -pady 2 -expand 1",
	"bind .smtp_entry <FocusIn> {send focuchan focus %W}",
	"bind .smtp_entry <Key> {send entychan  {%A}}",
	"bind .smtp_entry <Key-\n> {focus .pop_entry}",
	"bind .smtp_entry <Control-i> {focus .pop_entry}",
	"frame .i.txt",
	"label .i.hdr -text {"+gui->SVR_INCOMING+"} -font "+ gui->SVR_FONT,
	"label .pop_lbl -text {"+gui->SVR_POPSERVER+"}",
	"entry .pop_entry -width 22w",
	"pack .i.hdr -pady 2 -anchor w",
	"pack .i.txt -fill x",
	"pack .pop_lbl -in .i.txt -side left -pady 2 -anchor e",
	"pack .pop_entry -in .i.txt -side left -fill x -padx 4 -pady 2 -expand 1",
	##############
	#"bind .smtp_entry <FocusIn> {send focuchan focus %W}",
	#"bind .smtp_entry <Key> {send entychan  {%A}}",
	"bind .pop_entry <FocusIn> {send focuchan focus %W}",
	"bind .pop_entry <Key> {send entychan  {%A}}",
	"bind .pop_entry <Key-\n> {focus .smtp_entry}",
	"bind .pop_entry <Control-i> {focus .smtp_entry}",
	################
	
	
	
	"frame .b.default -relief sunken -bd 1",
	"button .b.cancel -text {Cancel} -width 8w -command {send svr cancel}",
	"button .b.ok -text {OK} -width 8w -command {send svr ok}",
	"pack .b.default -side right -padx 8 -pady 4 -anchor e",
	"pack .b.cancel -side right -padx 8 -anchor e",
	"pack .b.ok -in .b.default -side right -padx 4 -pady 4",
#	"pack .Wm_t -side top -fill x",
	"pack .o .i .b -fill x",
	". configure -bd 1 -relief raised -width 640 -height 420",
};


# user options panel
usrpanel = array[] of {
        "frame .o -bd 1 -relief groove",
        "frame .i -bd 1 -relief groove",
        "frame .b -bd 1 -relief groove",
	"label .o.hdr -text {"+gui->USR_OUTGOING+"} -font "+gui->USR_FONT,
	"label .addr_lbl -text {"+gui->USR_EMAILADDRESS+"}",
	"entry .addr_entry -width 25w",
	"pack .o.hdr -pady 2 -anchor w",
	"pack .addr_entry -in .o -side right -anchor e -padx 4 -pady 2 -expand 1",
	"pack .addr_lbl -in .o -pady 2 -side right -anchor e",
	"bind .addr_entry <FocusIn> {send focuchan focus %W}",
        "bind .addr_entry <Key> {send entychan  {%A}}",
	"bind .addr_entry <Key-\n> {focus .uid_entry}",
	"bind .addr_entry <Control-i> {focus .uid_entry}",
	"frame .i.txt",
	"frame .i.pwd",
	"label .i.hdr -text {"+gui->USR_INCOMING+"} -font "+gui->USR_FONT,
	"label .uid_lbl -text {"+gui->USR_USERLOGIN+"}",
	"entry .uid_entry -width 25w",
	"pack .i.hdr -pady 2 -anchor w",
	"pack .i.txt .i.pwd -anchor e",
	"pack .uid_entry -in .i.txt -side right -anchor e -padx 4 -pady 2 -expand 1",
	"pack .uid_lbl -in .i.txt -pady 2 -side right -anchor e",
	"bind .uid_entry <FocusIn> {send focuchan focus %W}",
        "bind .uid_entry <Key> {send entychan  {%A}}",
	"bind .uid_entry <Key-\n> {focus .pwd_entry}",
	"bind .uid_entry <Control-i> {focus .pwd_entry}",
	"label .pwd_lbl -text {"+gui->USR_PASSWORD+"}",
	"entry .pwd_entry -show * -width 25w",
	"pack .pwd_entry -in .i.pwd -side right -anchor e -padx 4 -pady 2 -expand 1",
	"pack .pwd_lbl -in .i.pwd -pady 2 -side right -anchor e",
	#"bind .pwd_entry <Key-\n> {focus .addr_entry}",
       #############
       #"bind .addr_entry <FocusIn> {send focuchan focus %W}",
        #"bind .addr_entry <Key> {send entychan  {%A}}",
        #"bind .uid_entry <FocusIn> {send focuchan focus %W}",
        #"bind .uid_entry <Key> {send entychan  {%A}}",
        "bind .pwd_entry <FocusIn> {send focuchan focus %W}",
        "bind .pwd_entry <Key> {send entychan  {%A}}",
        "bind .pwd_entry <Key-\n> {focus .addr_entry}",
	"bind .pwd_entry <Control-i> {focus .addr_entry}",
       ###########3333
	"frame .b.default -relief sunken -bd 1",
	#"frame .b.default1 -re	
	"button .b.save -text {"+gui->USR_SAVE+"} -width 8w  -command {send cmd saveopts}",
	"button .b.cancel -text {"+gui->USR_CANCEL+" -width 8w -command {send usr cancel}",
	"button .b.ok -text {"+gui->USR_OK+"} -width 8w -command {send usr ok}",
	"pack .b.default -side left -padx 8 -pady 4 -anchor e",
	"pack .b.cancel -side right -padx 8 -anchor e",
	"pack .b.ok -in .b.default -side right -padx 4 -pady 4",
	"pack .Wm_t -side top -fill x",
	"pack .o .i .b -fill x",
	". configure -bd 1 -relief raised -width 320 -height 240",
};


# folder listing dialog box
main_foldertab = array[] of {
	"frame .names",
	"scrollbar .names.s -command '.names.p yview",
	"listbox .names.p -width 20w -yscrollcommand '.names.s set",
	"bind .names.p <Double-Button-1> 'send chn doubleclick",
	"bind .names.p <ButtonRelease-1> 'send chn buttonup",
	"pack .names.s -side right -fill y",
	"pack .names.p -fill both -expand 1",

	"frame .buts",
	"button .buts.cancel -text {"+gui->FOLDER_CANCEL+"} -command 'send chn cancel",
	"button .buts.ok -text {"+gui->FOLDER_OK+"} -state disabled -command 'send chn ok",
	"pack .buts.cancel .buts.ok -expand 1 -side left -fill x -padx 4 -pady 4",

	"pack .Wm_t -side top -fill x",
	"pack .buts -side bottom -fill x",
	"pack .names -fill both -expand 1",
	"pack propagate . 0",
	"focus .names.p",
	". configure -bd 1 -relief raised -width 150 -height 200",
	"update"
};



  composcreen = array[] of {
    "frame .bar ",
    "frame .mbar -bd 2 ",
    "frame .bbar ",
    "frame .hdr ",
    "frame .body ",
    "frame .stat ",

    # menu bar
    "menubutton .mbar.msg -text {" + gui->Message2 + "} -underline 0 -menu .mbar.msg.menu",
    "menubutton .mbar.edit -text {" + gui->Edit + "} -underline 0 -menu .mbar.edit.menu",
    "pack .mbar.msg .mbar.edit -side left",

    # message menu 
    "menu .mbar.msg.menu ",
    ".mbar.msg.menu add command -label {"+ gui->Deliver + "} -underline 0 -command {send cmd deliver}",
    ".mbar.msg.menu add command -label {"+ gui->Adcc +"} -underline 4 -command {send cmd addcc}",
    ".mbar.msg.menu add separator",
    ".mbar.msg.menu add command -label {"+ gui->Keyboard1  +"} -underline 0 -command {send cmd keyboard}",
    ".mbar.msg.menu add separator",
    ".mbar.msg.menu add command -label {" + gui->Cancel +"} -underline 4 -command {send cmd cancel}",

    # edit menu
    "menu .mbar.edit.menu ",
    ".mbar.edit.menu add command -label {" + gui->Cut + "} -underline 2 -state disabled -command {send cmd cut}",
    ".mbar.edit.menu add command -label {" + gui->Copy +"} -underline 0 -state disabled -command {send cmd copy}",
    ".mbar.edit.menu add command -label {" + gui->Paste +"} -underline 0 -state disabled -command {send cmd paste}",
    ".mbar.edit.menu add command -label {" + gui->SelectA +"} -underline 7  -state disabled -command {send cmd alltext}",

    # address menu
    #"menu .mbar.address.menu",
    #".mbar.address.menu add command -label {Add Recipient(s)} -underline 4 -command {send cmd addsender}",
    #".mbar.address.menu add command -label {Add All} -underline 4 -command {send cmd addall}",
    #".mbar.address.menu add command -label {Open...} -underline 0 -command {send cmd openaddress}",

    # toolbar buttons
    "button .bbar.quote -text {" + gui->Quote +"} -bd 2 -command {send cmd quote}",
    "button .bbar.kbrd -text {" + gui->Keyboard1 +"} -bd 2 -command {send cmd keyboard}",
    "button .bbar.deliver -text {" + gui->Deliver +"} -bd 2 -command {send cmd deliver}",
    "button .bbar.cc -text {" + gui->Cc +"} -bd 2 -command {send cmd addcc}",
    "button .bbar.cancel -text {" + gui->Cancel +"} -bd 2 -command {send cmd cancel}",
    "label .bbar.empty -text {} -width 30",
    "pack .bbar.empty .bbar.cancel .bbar.cc .bbar.deliver .bbar.kbrd -side right -fill y",
    "bind .bbar.quote <Enter> +{.stat.helpline configure -text {"+gui->quote_help+"}; update}",
    "bind .bbar.kbrd <Enter> +{.stat.helpline configure -text {"+gui->keyboard_help+"}; update}",
    "bind .bbar.deliver <Enter> +{.stat.helpline configure -text {"+gui->deliver_help+"}; update}",
    "bind .bbar.cc <Enter> +{.stat.helpline configure -text {"+gui->cc_help+"}; update}",
    "bind .bbar.attach <Enter> +{.stat.helpline configure -text {"+gui->attach_help+"}; update}",
    "bind .bbar.cancel <Enter> +{.stat.helpline configure -text {"+gui->cancel_help+"}; update}",

    "bind .bbar.quote <Leave> +{.stat.helpline configure -text {}; update}",
    "bind .bbar.kbrd <Leave> +{.stat.helpline configure -text {}; update}",
    "bind .bbar.deliver <Leave> +{.stat.helpline configure -text {}; update}",
    "bind .bbar.cc <Leave> +{.stat.helpline configure -text {}; update}",
    "bind .bbar.cancel <Leave> +{.stat.helpline configure -text {}; update}",
 

    # header entry
    "frame .hdr.l ",
    "frame .hdr.e ",
    "label .hdr.l.mt -text {" + gui->Mailto +"} -font "+gui->COMPOSE_MAILTO_FONT,
    "label .hdr.l.sb -text {" + gui->Subject +"} -font "+gui->COMPOSE_SUBJECT_FONT,
    "label .hdr.l.cc -text {"+ gui->MailCC + "} -font "+gui->COMPOSE_MAILCC_FONT,
    "pack .hdr.l.mt .hdr.l.sb -fill y -expand 1",
    "entry .hdr.e.mt -background white -height 20",
    "entry .hdr.e.sb -background white -height 20",
    "entry .hdr.e.cc -background white -height 20",
    "entry .hdr.e.att -height 20",
    "pack .hdr.e.mt .hdr.e.sb -fill x -expand 1",
    "pack .hdr.l -side left -fill y",
    "pack .hdr.e -side left -fill x -expand 1",
    
    "bind .hdr.e.mt <FocusIn> {send focuchan focus %W}",
    "bind .hdr.e.mt <Key> {send keys {%A}}",
    "bind .hdr.e.mt <Key-\n> {focus .hdr.e.sb}",
    "bind .hdr.e.mt <Control-i> {focus .hdr.e.sb}",
    "bind .hdr.e.sb <FocusIn> {send focuchan focus %W}",
    "bind .hdr.e.sb <Key> {send keys {%A}}",
    "bind .hdr.e.sb <Key-\n> {focus .body.t}",
    "bind .hdr.e.sb <Control-i> {focus .body.t}",
    "bind .hdr.e.cc <FocusIn> {send focuchan focus %W}",
    "bind .hdr.e.cc <Key> {send keys {%A}}",
    "bind .hdr.e.cc <Key-\n> {focus .body.t}",
    "bind .hdr.e.cc <Control-i> {focus .body.t}",
    "bind .hdr.e.att <Key-\n> {focus .body.t}",
    "bind .hdr.e.att <Control-i> {focus .body.t}",

    # message text
    "scrollbar .body.scroll -command {.body.t yview} ",
    "scrollbar .body.scroll2 -orient horizontal -command {.body.t xview} ",
    "text .body.t -background white  -tabs {1c} -wrap word -yscrollcommand {.body.scroll set} -xscrollcommand {.body.scroll2 set}",
    #"text .body.t -background white  -tabs {1c} -wrap word -yscrollcommand {.body.scroll set}",
    "pack .body.scroll -side right -fill y",
    "pack .body.scroll2 -side bottom -fill x",
    "pack .body.t -side left -expand 1 -fill both",
    "bind .body.t <FocusIn> {send focuchan focus %W}",
    "bind .body.t <Key> {send key4s {%A}}",
    
	# status line
    "label .stat.helpline -text {} -font "+gui->COMPOSE_HELP_FONT+" -height 1h -fg black",
    "pack .stat.helpline -side left",

    # toplevel
    "pack propagate . no ",
    "pack .bbar -in .bar -side right -pady 2 -padx 6",
    "pack .mbar -in .bar -side left -pady 2 -fill both -expand 1",
    "pack .bar -side top -fill x",
    "pack .hdr -side top -fill x -anchor w -padx 5",
    "pack .stat -side bottom -fill x",
    "pack .body -fill both -expand yes -padx 5 -pady 5",
    #"pack propagate . no",
    "focus .hdr.e.mt",
    
	# tag for quoted messages
    ".body.t tag configure fwdtag -fg #006000 " +
    				"-font "+gui->QUOTE_FONT,
};


   initscreen = array[] of {
      "frame .mbar -bd 2 ",
      "frame .bbar ",
      "frame .bbar.msg ",
      "frame .listing ",
      "frame .stat ",
   
      # menu bar
           
       "menubutton .mbar.file -text  {"+ gui->fi+"}   -underline 0 -menu .mbar.file.menu",
       "menubutton .mbar.msg -text {" + gui->Message2 +"}  -underline 0 -menu .mbar.msg.menu",
       "menubutton .mbar.hdr -text {"+ gui->headers1 +"} -underline 0 -menu .mbar.hdr.menu",
    #"menubutton .mbar.address -text Addresses -underline 0 -state disabled -menu .mbar.address.menu",
        "menubutton .mbar.option -text {" + gui->options +"} -underline 0 -menu .mbar.option.menu",
       
       
        "pack  .mbar.file .mbar.msg .mbar.hdr .mbar.option -side left",
    #"pack .mbar.file .mbar.msg .mbar.address .mbar.option -side left",

    # file menu
        "menu .mbar.file.menu ",
        ".mbar.file.menu add command -label {" + gui->Check_mail +"} -underline 0 -state disabled -command {send cmd connect}",
        ".mbar.file.menu add command -label {"+ gui->Exit +"} -underline 0 -command {send cmd exitfile}",
    # message menu
        "menu .mbar.msg.menu ",
        ".mbar.msg.menu add command -label {"+ gui->Next +"} -underline 0 -state disabled -command {send cmd nextmsg}",  
        ".mbar.msg.menu add command -label {"+ gui->Previous +"} -underline 0 -state disabled -command {send cmd prevmsg}",
        ".mbar.msg.menu add separator",
        ".mbar.msg.menu add command -label {" + gui->Composedot +"} -underline 0 -command {send cmd newmsg}",
    #".mbar.msg.menu add command -label {" + Savedot +"} -underline 0 -command {send cmd savemsg}",
    #".mbar.msg.menu add command -label {" + Delete +"} -underline 0 -command {send cmd deletemsg}",

    # headers menu
         "menu .mbar.hdr.menu ",
         ".mbar.hdr.menu add command -label {"+ gui->Previous +"} -underline 0 -state disabled -command {send cmd prevhdrs}",
         ".mbar.hdr.menu add command -label {"+ gui->Next +"} -underline 0 -state disabled -command {send cmd nexthdrs}",

    #option menu
         "menu .mbar.option.menu ",
         #".mbar.option.menu add command -label {"+ gui->Serverdot +"} -underline 0 -command {send cmd serveropts}",
         #".mbar.option.menu add command -label {User...} -underline 0 -command {send cmd useropts}",
         #".mbar.option.menu add separator",
         ".mbar.option.menu add command -label {" + gui->options  +"} -underline 0 -command {send cmd saveopts}",
	  # "image create bitmap mail -file " + IMPATH + "email.bit -maskfile + IMPATH + "email.bit",
           "image create bitmap delet -file " + gui->IMPATH + "trash.bit -maskfile " + gui->IMPATH + "trash.bit",

           "image create bitmap new -file " + gui->IMPATH + "new.bit -maskfile " + gui->IMPATH + "new.bit",
           "image create bitmap check -file " + gui->IMPATH + "mail_in.bit -maskfile " + gui->IMPATH + "mail_in.bit",
           "image create bitmap bpre -file " + gui->IMPATH + "down.bit -maskfile " + gui->IMPATH + "down.bit",
           "image create bitmap bnext -file " + gui->IMPATH + "up.bit -maskfile " + gui->IMPATH + "up.bit",
           "image create bitmap clos -file " + gui->IMPATH + "stop.bit -maskfile " + gui->IMPATH + "stop.bit", 
    # toolbar buttons
         "button .bbar.msg.file -text {      }",
          "button .bbar.msg.pre -text {Prev} -bd 0  -command {send cmd prevhdrs}",
           "button .bbar.msg.next -text {Next} -bd  0  -command {send cmd nexthdrs}",
        
         "button .bbar.msg.connect -text {"+ gui->Check_mail+"} -bd 2  -state disabled -command {send cmd connect}",
         "button .bbar.msg.compose -text {"+ gui->New +"} -bd 2  -command {send cmd newmsg}",
   # "button .bbar.msg.save -text {{"+Save+"} -bd 2  -command {send cmd savemsg} -state disabled",
         "button .bbar.msg.delete -text {delete} -bd 2  -command {send cmd deletemsg} -state disabled",
         "button .bbar.msg.close -text {"+ gui->Close +"} -bd 2  -command {send cmd exitfile}",
###########

        # "image create bitmap delet -file " + IMPATH + "delete.bit -maskfile " + IMPATH + "delete.bit",
         ".bbar.msg.delete configure -image delet",
          ".bbar.msg.compose configure -image new",
           ".bbar.msg.connect configure -image check",
           ".bbar.msg.pre configure -image bnext",
           ".bbar.msg.next configure -image bpre",
           ".bbar.msg.close configure -image clos",          
 
         "pack  .bbar.msg.pre .bbar.msg.next .bbar.msg.connect .bbar.msg.compose   .bbar.msg.delete .bbar.msg.close -side left -fill y",
   # "pack .bbar.msg.connect .bbar.msg.sendmail .bbar.msg.compose .bbar.msg.delete -side left -fill y",
##########
    #"pack .bbar.msg.connect .bbar.msg.compose .bbar.msg.save .bbar.msg.delete -side left -fill y",

         "pack .bbar.msg -side left -fill y -padx 6 -pady 2",
        # "pack .bbar.file -side left -fill both -expand yes",

        # "bind .bbar.file <Enter> +{.stat.helpline configure -text {"+gui->file_help+"}; update}",
         "bind .bbar.msg.connect <Enter> +{.stat.helpline configure -text {"+gui->connect_help+"}; update}",
         "bind .bbar.msg.compose <Enter> +{.stat.helpline configure -text {"+gui->new_help+"}; update}",
         "bind .bbar.msg.save <Enter> +{.stat.helpline configure -text {"+gui->savesel_help+"}; update}",
         "bind .bbar.msg.delete <Enter> +{.stat.helpline configure -text {"+gui->delsel_help+"}; update}",

         "bind .bbar.file <Leave> +{.stat.helpline configure -text {}; update}",
         "bind .bbar.msg.connect <Leave> +{.stat.helpline configure -text {}; update}",
        "bind .bbar.msg.compose <Leave> +{.stat.helpline configure -text {}; update}",
         "bind .bbar.msg.save <Leave> +{.stat.helpline configure -text {}; update}",
         "bind .bbar.msg.delete <Leave> +{.stat.helpline configure -text {}; update}",
 
    # message headers
         "scrollbar .listing.s -command {.listing.p yview}",
         "scrollbar .listing.h -orient horizontal -command {.listing.p xview}",
         "listbox .listing.p -width 90w -font "+gui->MAIN_LISTBOX_FONT+" -xscrollcommand {.listing.h set} -yscrollcommand {.listing.s set}",
         "bind .listing.p <ButtonRelease-1> + {send cmd select}",
         "bind .listing.p <ButtonPress-1> {%W tkListbButton1P %y}",
         "pack .listing.s -side right -fill y",
         "pack .listing.h -side bottom -fill x",
         "pack .listing.p -fill both -expand 1",

    # status line
         "label .stat.newmail -text {} -font "+gui->MAIN_STATUS_NEWMAIL_FONT,
#/fonts/lucidasans/latin1.6.font -fg red -height 1h",
         "label .stat.helpline -text {ready} "+gui->MAIN_STATUS_HELP_FONT,
#-font /fonts/lucidasans/latin1.6.font -fg black -height 1h",
         "pack .stat.newmail -side right",
         "pack .stat.helpline -side left",

# toplevel
         "pack propagate . no ",
         "pack .mbar  -side top -fill x",
         "pack .bbar -side top -fill x  -pady 2",
         "pack .stat   -side bottom -fill x",
         "pack .listing  -side top -fill both -expand yes -padx 5 -pady 5",
         
         #"pack propagate . no ",
         "focus .listing.p",

          "update",
          };


 msgscreen = array[] of {
    "frame .mbar -bd 2 ",
    "frame .bbar ",
    "frame .bbar.go ",
    "frame .bbar.compose ",
    "frame .bbar.msg ",
    "frame .body ",
    "frame .stat ",
    
     "image create bitmap delet -file " + gui->IMPATH + "trash.bit -maskfile " + gui->IMPATH + "trash.bit",

     "image create bitmap new -file " + gui->IMPATH + "new.bit -maskfile " + gui->IMPATH + "new.bit",
     "image create bitmap save -file " + gui->IMPATH + "cd.bit -maskfile " + gui->IMPATH + "cd.bit",
     "image create bitmap pre -file small_color_left.bit -maskfile small_color_left.bit",
     "image create bitmap next -file small_color_right.bit -maskfile small_color_right.bit",
     "image create bitmap clos -file " + gui->IMPATH + "stop.bit -maskfile " + gui->IMPATH + "stop.bit", 
     "image create bitmap forward -file " + gui->IMPATH + "forward.bit -maskfile " + gui->IMPATH + "forward.bit",

     "image create bitmap show -file " + gui->IMPATH + "show.bit -maskfile " + gui->IMPATH + "show.bit",
     "image create bitmap reply -file " + gui->IMPATH + "reply.bit -maskfile " + gui->IMPATH + "reply.bit",
     "image create bitmap replyall -file " + gui->IMPATH + "replyall.bit -maskfile " + gui->IMPATH + "replyall.bit",
          


    # menu bar
    "menubutton .mbar.msg -text {"+ gui->Message2 +"} -underline 0 -menu .mbar.msg.menu",
    "menubutton .mbar.edit -text {" + gui->Edit + "} -underline 0 -menu .mbar.edit.menu",
    "menubutton .mbar.compose -text {" + gui->Composem +"} -underline 0 -menu .mbar.compose.menu",
    #"menubutton .mbar.address -text Addresses -underline 0 -menu .mbar.address.menu",
    #"pack .mbar.msg .mbar.edit .mbar.compose .mbar.address -side left",
    "pack .mbar.msg .mbar.edit .mbar.compose -side left",

    # message menu
    "menu .mbar.msg.menu ",
    ".mbar.msg.menu add command -label {" + gui->Next +"} -underline 0 -state disabled -command {send msg nextmsg}",
    ".mbar.msg.menu add command -label {"+ gui->Previous +"} -underline 0 -state disabled -command {send msg prevmsg}",
    ".mbar.msg.menu add separator",
    ".mbar.msg.menu add command -label {" +gui->Show_Head +"} -underline 12 -command {send msg show_header}",
    ".mbar.msg.menu add command -label {" + gui->Savedot +"} -underline 0 -command {send msg savemsg}",
    ".mbar.msg.menu add command -label {" + gui->Delete +"} -underline 0 -command {send msg deletemsg}",
    ".mbar.msg.menu add separator",
    ".mbar.msg.menu add command -label {" + gui->Close +"} -underline 0 -command {send msg closemsg}",

    # edit menu
    "menu .mbar.edit.menu ",
    ".mbar.edit.menu add command -label {" + gui->Copy +"} -underline 0 -command {send msg copy}",
    ".mbar.edit.menu add command -label {" +gui->Select_all +"} -underline 7 -command {send msg alltext}",

    # compose menu
    "menu .mbar.compose.menu ",
    ".mbar.compose.menu add command -label {"+ gui->Newdot +"} -underline 0 -command {send msg newmsg}",
    ".mbar.compose.menu add command -label {" + gui->Reply_to_Sdot + "} -underline 0 -command {send msg compose "+string REPLY+"}",
    ".mbar.compose.menu add command -label {" + gui->Reply_to_Adot +"} -underline 9 -command {send msg compose "+string REPLYALL+"}",
    ".mbar.compose.menu add command -label {" + gui->Forwarddot +"} -underline 0 -command {send msg compose "+string FORWARD+"}",

    # address menu
    #"menu .mbar.address.menu",
    #".mbar.address.menu add command -label {Add Sender} -underline 4 -command {send msg addsender}",
    #".mbar.address.menu add command -label {Add All} -underline 4 -command {send msg addall}",
    #".mbar.address.menu add command -label {Open...} -underline 0 -command {send msg openaddress}",

    #toolbar buttons
    "button .bbar.go.prev  -text {" + gui->Prev +"} -bd 2 -state disabled -command {send msg prevmsg}",
    "button .bbar.go.next  -text {" + gui->Next + "} -bd 2 -state disabled -command {send msg nextmsg}",
    "button .bbar.compose.new -text {" + gui->New +"} -bd 2 -command {send msg newmsg}",
    "button .bbar.compose.reply -text {" + gui->Re +"} -bd 2 -command {send msg compose "+string REPLY+"}",
    "button .bbar.compose.replyall -text {" + gui->All + "} -bd 2 -command {send msg compose "+string REPLYALL+"}",
    "button .bbar.compose.forward -text {" + gui->Fwd +"} -bd 2 -command {send msg compose "+string FORWARD+"}",
    "button .bbar.msg.header -text {" + gui->Show +"} -bd 2 -command {send msg show_header}",
    "button .bbar.msg.save -text {"+ gui->Save +"} -bd 2 -command {send msg savemsg}",
    "button .bbar.msg.delete -text {"+ gui->Delete +"} -bd 2 -command {send msg deletemsg}",
   ##############
    "button .bbar.msg.close -text {"+ gui->Close +"} -bd 2  -command {send msg closemsg}", 
   ############
   
    ".bbar.msg.delete configure -image delet",
    ".bbar.compose.new configure -image new",
    ".bbar.compose.reply configure -image reply",
    ".bbar.go.prev configure -image pre",
    ".bbar.go.next configure -image next",
    ".bbar.msg.close configure -image clos",      
    ".bbar.compose.replyall configure -image replyall",
    ".bbar.compose.forward configure -image forward",
    ".bbar.msg.header configure -image show",
    ".bbar.msg.save configure -image save",
        
   
    "pack .bbar.go.prev .bbar.go.next -side left -fill y",
    "pack .bbar.compose.new .bbar.compose.reply .bbar.compose.replyall .bbar.compose.forward -side left -fill y",
    "pack .bbar.msg.header .bbar.msg.save .bbar.msg.delete .bbar.msg.close -side left -fill y",
#    "pack .bbar.msg.header .bbar.msg.delete .bbar.msg.close -side left -fill y",
    "pack .bbar.go -side left -padx 6 -fill y",
    "pack .bbar.compose -side left -padx 6 -fill y",
    "pack .bbar.msg -side left -padx 6 -fill y",

    "bind .bbar.go.prev <Enter> +{.stat.helpline configure -text {"+gui->prev_help+"}; update}",
    "bind .bbar.go.next <Enter> +{.stat.helpline configure -text {"+gui->next_help+"}; update}",
    "bind .bbar.compose.new <Enter> +{.stat.helpline configure -text {"+gui->new_help+"}; update}",
    "bind .bbar.compose.reply <Enter> +{.stat.helpline configure -text {"+gui->reply_help+"}; update}",
    "bind .bbar.compose.replyall <Enter> +{.stat.helpline configure -text {"+gui->all_help+"}; update}",
    "bind .bbar.compose.forward <Enter> +{.stat.helpline configure -text {"+gui->fwd_help+"}; update}",
    "bind .bbar.msg.header <Enter> +{.stat.helpline configure -text {"+gui->head_help+"}; update}",
    "bind .bbar.msg.save <Enter> +{.stat.helpline configure -text {"+gui->save_help+"}; update}",
    "bind .bbar.msg.delete <Enter> +{.stat.helpline configure -text {"+gui->del_help+"}; update}",

    "bind .bbar.go.prev <Leave> +{.stat.helpline configure -text {}; update}",
    "bind .bbar.go.next <Leave> +{.stat.helpline configure -text {}; update}",
   "bind .bbar.compose.new <Leave> +{.stat.helpline configure -text {}; update}",
    "bind .bbar.compose.reply <Leave> +{.stat.helpline configure -text {}; update}",
   "bind .bbar.compose.replyall <Leave> +{.stat.helpline configure -text {}; update}",
    "bind .bbar.compose.forward <Leave> +{.stat.helpline configure -text {}; update}",
    "bind .bbar.msg.header <Leave> +{.stat.helpline configure -text {}; update}",
    "bind .bbar.msg.save <Leave> +{.stat.helpline configure -text {}; update}",
    "bind .bbar.msg.delete <Leave> +{.stat.helpline configure -text {}; update}",

    # message text
    "scrollbar .body.scroll -command {.body.t yview}",
    "text .body.t -state disabled -yscrollcommand {.body.scroll set}",
    #"text .body.t -state disabled -wrap word -yscrollcommand {.body.scroll set}",
    "pack .body.scroll -side right -fill y",
    "pack .body.t -side left -expand 1 -fill both",

    # status line
    "label .stat.newmail -text {} -font "+gui->MESSAGE_HELP_FONT+" -fg red -height 1h",
    "label .stat.helpline -text {} -font "+gui->MESSAGE_HELP_FONT+" -fg black -height 1h",
    "pack .stat.newmail -side right",
   "pack .stat.helpline -side left",

    # toplevel
    "pack propagate . no ",
    "pack .mbar -side top -fill x",
    "pack .bbar -side top -fill x -pady 2",
    "pack .stat -side bottom -fill x",
    "pack .body -expand 1 -fill both -padx 5 -pady 5",
    #"pack propagate . no ",

     "focus .body.t",
     };

foldertab = array[] of {
	"frame .brder -bg black",
	"frame .buts",
	#"label .buts.lbl -text {Operation in Progress} -font /fonts/lucidasans/latin1.7.font",
	"label .buts.lbl -text {"+gui->OPERATION_PROGRESS+"} -font "+gui->OPERATION_FONT,
	"button .buts.cancel -text {"+gui->OPERATION_CANCEL+"} -fg red -command 'send chn cancel",
	"pack .buts.lbl -expand 1 -side left -fill x -padx 4 -pady 4",
	"pack .buts.cancel -expand 1 -side left -fill x -padx 4 -pady 4",

	"pack .Wm_t -side top -fill x",
	"pack .brder -side bottom -fill x",
	"pack .buts -in .brder -side bottom -fill x -padx 2 -pady 2",
	"pack propagate . 0",

	"pack propagate . 0",
#+++++	". configure -bd 1 -relief raised",
	"update"
};

mailtool_notecmd = array[] of {
        "frame .f",
        "button .b -text "+gui->DIALOG_MSG_OK+" -command {send popup OK}",
        "focus .f",
        "bind .f <Key-\n> {send popup OK}",
        "pack .f.m -side left -expand 1 -padx 10 -pady 10",
        "pack .f .b -padx 10 -pady 10",
        "update; cursor -default"
};

regpanel = array[] of {
	"frame .i ",
        "frame .f",
        "frame .b",
	"frame .f.l",
	"frame .f.e",
	"label .i.instruct  -text {"+gui->REG_TITLE + "} -font " + gui->REG_FONT,
	"pack .i.instruct -fill x",
	"label .f.l.pop -text {"+gui->REG_POPSERVER+ "}  -anchor e",
	"label .f.l.smtp -text {"+gui->REG_SMTPSERVER+"}  -anchor e",
	"label .f.l.user -text {"+gui->REG_USERLOGIN+"}  -anchor e",
	"label .f.l.secret -text {"+gui->REG_PASSWORD+"}  -anchor e",
	"label .f.l.addr -text {"+gui->REG_EMAILADDRESS+"}  -anchor e",
	"pack .f.l.pop .f.l.smtp .f.l.user .f.l.secret .f.l.addr  -fill both -pady 4",
	"entry .f.e.pop",
	"entry .f.e.smtp",
	"entry .f.e.user",
	"entry .f.e.secret -show *",
	"entry .f.e.addr",
	"pack .f.e.pop .f.e.smtp .f.e.user .f.e.secret .f.e.addr -fill both",
        #"pack .e.pop .e.smtp .e.user .e.secret .e.addr -fill both",
        
        "bind .f.e.pop <FocusIn> {send focuchan focus %W}",
        "bind .f.e.pop <Key> {send entychan  {%A}}",
	"bind .f.e.pop <Key-\n> {focus .f.e.smtp}",
	"bind .f.e.pop <Control-i> {focus .f.e.smtp}",
	"bind .f.e.smtp <FocusIn> {send focuchan focus %W}",
	"bind .f.e.smtp <Key> {send entychan  {%A}}",
	"bind .f.e.smtp <Key-\n> {focus .f.e.user}",
	"bind .f.e.smtp <Control-i> {focus .f.e.user}",

	"bind .f.e.user <FocusIn> {send focuchan focus %W}",
	"bind .f.e.user <Key> {send entychan  {%A}}",
	"bind .f.e.user <Key-\n> {focus .f.e.secret}",
	"bind .f.e.user <Control-i> {focus .f.e.secret}",
	"bind .f.e.secret <FocusIn> {send focuchan focus %W}",
	"bind .f.e.secret <Key> {send entychan  {%A}}",
	"bind .f.e.secret <Key-\n> {focus .f.e.addr}",
	"bind .f.e.secret <Control-i> {focus .f.e.addr}",
	"bind .f.e.addr <FocusIn> {send focuchan focus %W}",
	"bind .f.e.addr <Key> {send entychan  {%A}}",
	"bind .f.e.addr <Key-\n> {focus .f.e.pop}",
	"bind .f.e.addr <Control-i> {focus .f.e.pop}",
	#"pack .e.pop .e.smtp .e.user .e.secret .e.addr -fill both",
       
      
	"pack .f.l .f.e -side left -in .f",
	"frame .b.default -relief sunken -bd 1 ",
	"button .b.quit -text {"+gui->REG_NOCHANGE+"} -width 20w  -command {send reg quit}",
	"button .b.save -text {"+gui->REG_SAVE+"} -width 10w  -command {send reg save}",
	"button .b.default.ok -text {"+gui->REG_OK+"} -width 10w  -command {send reg ok}",
#	"pack .b.default.ok .b.save .b.quit -side right -padx 8 -anchor e",
	"pack .b.quit -side right -padx 8 -anchor e",
	"pack .b.save -side right -padx 8",
	"pack .b.default.ok -in .b.default -side right -padx 4 -pady 4",
	"pack .b.default -side right -padx 8 -pady 8 -anchor e",
#	"pack .Wm_t -side top -fill x ",
	#"pack .all",
	"pack .i -fill x -padx 6 -pady 26 -side top",
	"pack .f -fill both -padx 6 -pady 6 -side top",
	"pack .b -fill x -padx 6  -side top",
	"pack propagate . no ",
	"focus .e.pop",
	". configure -bd 1 -relief raised",
#	". configure -bd 1 -relief raised -width 320 -height 240",
	"update",
};


}
