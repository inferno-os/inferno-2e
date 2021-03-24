MailI18N: module {
	PATH: con "/dis/mailtool/maili18n_english.dis";
	#FRENCHPATH: con "/dis/mailtool/maili18n_french.dis";
	#GERMANPATH: con "/dis/mailtool/maili18n_german.dis";
	#SPANISHPATH: con "/dis/mailtool/maili18n_spanish.dis";
	# Built-in icon identifiers
	
#	name_language: fn(name: string) : string;

	init: fn();
#	tbinit: fn(ctx: ref Draw->Context, wc, rc: chan of string, rmain, rctl, rprot: Rect, events: Events) : (Rect, Rect, Rect);
#	skeyboard: fn(ctxt: ref Draw->Context);
	
	# mail box alert messages
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
	Date,           #8,	
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

	Really_cancel_and_lose_message_being_composed,	#55,
	Do_not_discard,	#20,
	Discard_message,	#25,	
	PopServer,	#20
	SmtpServer,	#22
	UserLogin,	#20,
	EmailAddress,  #23,
	Please_Enter_Mail_information, #440,
	No_change_Continue : string; #25,

};
