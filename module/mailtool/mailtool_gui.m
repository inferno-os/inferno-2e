Mailtool_GUI: module
{
    PATH: con "/dis/mailtool/mailtool_gui.dis";

    init: fn();

    
#   main mailtool window parameters

        MAIN_FONT:string;
        MAILTOOL_TITLE: string;
        MAIN_STATUS_NEWMAIL_FONT,
        MAIN_STATUS_HELP_FONT: string;
	MAIN_LISTBOX_FONT:string;
	MAIN_START_POINT: string;
	MAIN_WHERE: string;
        MESSAGE_TITLE: string;

        MAIN_TITLE:string;
        screenX,screenY: int;
        status_indicator:string;
        dialog_msg_main1,dialog_msg_main2,
        dialog_msg_main3: string;


	BUTTON_OK,
	REG_TITLE,
	REG_FONT,
	REG_POPSERVER,
	REG_SMTPSERVER,
	REG_USERLOGIN,
	REG_PASSWORD,
	REG_EMAILADDRESS,
	REG_SAVE,
	REG_OK,
	REG_NOCHANGE,
	SVR_OUTGOING,
	SVR_FONT,
	SVR_SMTPSERVER,
	SVR_INCOMING,
	SVR_POPSERVER,
	USR_OUTGOING,
	USR_EMAILADDRESS,
	USR_INCOMING,
	USR_USERLOGIN,
	USR_FONT,
        USR_PASSWORD,
	USR_SAVE,
	USR_CANCEL,
	USR_OK,

	DIALOG_MSG_FONT,
	DIALOG_MSG_OK,

        FOLDER_OK, 
       FOLDER_CANCEL: string;

       SERVER_OPTIONS,
       COMPOSE_TITLE,
       COMPOSE_MAIN_FONT, 
       COMPOSE_WIN_FONT,
       COMPOSE_MAILTO_FONT,
	COMPOSE_SUBJECT_FONT,
	 COMPOSE_MAILCC_FONT,
	COMPOSE_HELP_FONT,

	MESSAGE_FONT,
	MESSAGE_HELP_FONT,

        IMPATH,
#        REPLY,
#        REPLYALL,
#        FORWARD,


       USER_OPTIONS_TITLE,


	
	no_mail_alert,  # Max length 20,
	new_mail,	# max length 15,
	no_new_mail,	# max length 19,
	please_connect,	# max length 25,
	working,	# max length 20,

      # Help messages for toolbar buttons
	file_help,	# 40,
	connect_help,	# 40,

	new_help,	# 40,
	savesel_help,	# 40,
	delsel_help,	# 40,
	prev_help,	# 40,
	next_help,	# 40,
	reply_help,	# 40,
	all_help,	# 40,
	fwd_help,	# 40,
	head_help,	# 40,
	save_help,	# 40,
	del_help,	# 40,

      # Action status messages
	signing_on,	# 40,
	getting_hdrs,	# 40,
	getting_mail,	# 40,
	contacting_server,	# 40,
	messages_sent,	# 40,
	send_error,	# 40,
	server_change,	# 40,

      # widgets messages
	fi,	# 9,
	headers1,	# 11,
	options,	# 11,
	#address,
	OK,	# 5,
	Check_mail,	#14,
	Exit,	# 11,
	Next,	# 11,
	Previous,	#14,
	Composedot,	#14,
	Composem,	# 14,
	Savedot,	# 14,
	Save,	# 12,
	Delete,	# 12,
	Serverdot,	#12,
	Userdot,	# 12,
	Inbox,	#11,
	New,	#12 ,
	Newdot,	# 12,
	Close,	# 12,
	Message2, #14,
	Edit,	# 12,
	Show_Head,	#25 ,
	Copy,	# 11,
	Select_all,	# 14,
	Reply_to_Sdot,	# 25,
	Reply_to_Adot,	# 20,
	Forwarddot,	# 20,
	Prev,	# 11,
	Re,	#11,
	All,	# 11,
	Fwd,	# 11,
	Show,	# 11,
	POp,	# 20,
	SMtP,	#20,
	User_L,	# 20,
	Password,	#20,
	Email_add,	#20,
	Cancel,	#11,
	mailmess1,	#45,
	mailmess2,	#45,
	Keyboard1,	#15,
	Sendin,	#20,
	Please_c,	#30,
	folder1,	#11,
	Total,	#20,
	error1,	#30,
	error2,	#30,
	warn1,	#10,
	warn2,	#20,
	warn3,	#30,
	warn4,	#30,
	Message1,	#15,
	Mail,	#11,
	Warn5,	#40,
	To,	#11,
	From,	#11,
	Subject,	#15,
	nosubject,	#15,

      #messages in compose Window
      # Help messages for toolbar buttons
	keyboard_help,	#50,
	deliver_help,	#50,
	cc_help,	#50, 
	attach_help,	#50,
	cancel_help,	#50,
	quote_help,	#50,

	Deliver,	#14,
	Adcc,	#14,

	Cut,	#14,
	Paste,	#14,
	SelectA,	#14,
	Quote,	#14,
	Cc,	#14,
	Mailto,	#14,
	#Subject,
	MailCC,	#14,
	Continue,	#14,
	MailW,	#14,
	Operation,	#14,
	Cancelled,	#14,
	Ope_in_prog,    #30 

	msgbox_font,
	msgbox_ques,
	msgbox_ndiscard,
	msgbox_discard,
	msgbox_cancel,
	msgbox_continue,

	keyboard_title,
	QUOTE_FONT:string;

	OPERATION_PROGRESS,
	OPERATION_CANCEL,
	OPERATION_CANCELLED,
	OPERATION_FONT: string;

	FOLDER_FONT: string;

};
