implement Mailtool_GUI;
include "mailtool_gui.m";

init()
{	

   # main mailtool windows parameters

        MAIN_FONT="/fonts/lucidasans/latin1.7.font -bd 2 -relief raised ";
        MAILTOOL_TITLE="Mail";
        MAIN_STATUS_NEWMAIL_FONT="/fonts/lucidasans/latin1.6.font -fg red -height 1h";
        MAIN_STATUS_HELP_FONT="/fonts/lucidasans/latin1.6.font -fg black -height 1h";
	MAIN_LISTBOX_FONT="/fonts/lucidasans/typelatin1.7.font";
	MAIN_WHERE=" -x 0 -y 0";
	MAIN_START_POINT=" -x 0 -y 0 ";
	MESSAGE_TITLE="Message:";

        MAIN_TITLE="Mail:Inbox";

	DIALOG_MSG_FONT="/fonts/lucida/unicode.7.font";
	DIALOG_MSG_OK="OK";



        screenX=620;
        screenY=400;
        status_indicator="getting_mail";
        dialog_msg_main1="Can't find time module. \nData corruption possible";
        dialog_msg_main2="file create error!";
        dialog_msg_main3="Mail error\n";

	BUTTON_OK="OK";	
	REG_TITLE="Please enter Mail information.";
	REG_FONT="/fonts/lucidasans/typelatin1.10.font";
	REG_POPSERVER="POP Server:";
	REG_SMTPSERVER="SMTP Server:";

	REG_USERLOGIN="User Login:";
	REG_PASSWORD="Password:";
	REG_EMAILADDRESS="Email Address:";
	REG_SAVE="Save";
	REG_OK="OK";
	REG_NOCHANGE="No Change/Continue";
	SVR_OUTGOING="Outgoing mail";
	SVR_FONT="/fonts/lucidasans/boldlatin1.7.font";
	SVR_SMTPSERVER="SMTP Server:";
	SVR_INCOMING="Incoming mail";
	SVR_POPSERVER="POP Server:";
	USR_OUTGOING="Outgoing mail";
	USR_EMAILADDRESS="Email address";
	USR_INCOMING="Incoming mail";
	USR_USERLOGIN="User Login:";
	USR_FONT="/fonts/lucidasans/boldlatin1.7.font";
        USR_PASSWORD="Password:";
	USR_SAVE="Save";
	USR_CANCEL="Cancel";
	USR_OK="OK";
        FOLDER_OK="OK"; 
        FOLDER_CANCEL="Cancel";


        SERVER_OPTIONS="Server Options"; 
        COMPOSE_TITLE="Mail";
        COMPOSE_MAIN_FONT="/fonts/lucidasans/latin1.7.font -bd 2 -relief raised ";
        COMPOSE_WIN_FONT="/fonts/lucidasans/latin1.7.font -bd 2 -relief raised ";

	COMPOSE_MAILTO_FONT="/fonts/lucidasans/typelatin1.7.font";
	COMPOSE_SUBJECT_FONT="/fonts/lucidasans/typelatin1.7.font";
	COMPOSE_MAILCC_FONT="/fonts/lucidasans/typelatin1.7.font";
	COMPOSE_HELP_FONT="/fonts/lucidasans/latin1.6.font -height 1h -fg black ";;

	MESSAGE_FONT="/fonts/lucidasans/latin1.7.font -bd 2 -relief raised ";
	MESSAGE_HELP_FONT="/fonts/lucidasans/latin1.6.font -fg red -height 1h ";
        IMPATH="mail/";
#        REPLY="回复";
#        REPLYALL="回复所有人";
#        FORWARD="转发";
     

       USER_OPTIONS_TITLE="User Options";


# mail box alert messages
	no_mail_alert = "";
	new_mail = "New Mail";
	no_new_mail = "no new mail";
	please_connect = "connect to deliver mail";
	working = "working";

# Help messages for toolbar buttons
	file_help = "Open a folder";
	connect_help = "Connect to mail server";
#########
##sendmail_help: con "Sending message";
##########
	new_help = "Compose new message";
	savesel_help = "Save selected message to folder";
	delsel_help = "Delete selected message";
	prev_help  = "Show previous message";
	next_help = "Show next message";
	reply_help = "Reply to sender";
	all_help = "Reply to sender and cc list";
	fwd_help = "Forward message";
	head_help = "Show header info";
	save_help = "Save message to folder";
	del_help = "Delete message";

# Action status messages
	signing_on = "Signing on...";
	getting_hdrs = "Getting headers...";
	getting_mail = "Getting message...";
	contacting_server = "Contacting server...";
	messages_sent = "Messages successfully sent.";
	send_error = "Error sending message.";
	server_change = "Server mail status changed.";

# widgets messages
	fi = "File";
	OK = "OK";
	Check_mail = "Check Mail";
	Exit = "Exit";
	Next = "Next";
	Previous = "Previous";
	Composedot = "Compose...";
	Composem = "Compose";
	Savedot = "Save...";
	Save = "Save";
	Delete = "Delete";
	Serverdot = "Server...";
	Userdot = "User...";
	Inbox = "Inbox...";
	New = "New";
	Newdot = "New...";
	Close = "Close";
	headers1 = "Headers";
	Message2 = "Message";
	Edit = "Edit";
	Show_Head = "Show Header Info...";
	Copy = "Copy";
	Select_all = "Select All";
	Reply_to_Sdot = "Reply to Sender...";
	Reply_to_Adot = "Reply to All...";
	Forwarddot = "Forward...";
	Prev = "Prev";
	Re = "Re:";
	All = "All";
	Fwd = "Fwd";
	Show = "Show";
	POp = "POP Server:";
	SMtP = "SMTP Server:";
	User_L = "User Login:";
	Password = "Passward:";
	Email_add = "Email Address:";
	Cancel = "Cancel";
	mailmess1 = "The Mailtool needs to be restarted for\n";
	mailmess2 = "the configuration changes to take effect.";
	Keyboard1 = "Keyboard";
	Sendin = "Sending Message";
	Please_c = "Please configure server";
	folder1 = "folder";
	Total = "Total messages:";
	error1 = "Error mail:ISP Db nil";
	error2 = "ISP read failed";
	warn1 = "The";
	warn2 = "file contains";
	warn3 = "messages that will be lost";
	warn4 = "if this file is deleted.";
	#Message2 = "Message:";
	Mail = "Mail";
	Warn5 = "Can't find notes database";
	To = "To";
	From = "From";
	#Subject = "Subject";
	nosubject = "No subject";
	#header = "Headers";
	options = "Options";
	
#messages in compose Window
# Help messages for toolbar buttons
	keyboard_help = "Display/hide soft keyboard on screen";
	deliver_help = "Deliver message to Outbox and close Compose window";
	cc_help = "Add carbon copy addresses";
	attach_help = "Add attachment(s)";
	cancel_help = "Cancel message and close Compose window";
	quote_help = "Include the message you are replying to";

	Deliver = "Deliver";
	Adcc = "Add Cc...";

	Cut = "Cut";
	Paste = "Paste";
	SelectA = "Select All";
	Quote = "Quote Orig.";
	Cc =  "Cc";
	Mailto = "Mail To:";
	Subject = "Subject:";
	MailCC = "Mail CC:";
	Continue = "Continue";
	MailW = "Mail WARNING";
	Operation = "Operation";
	
	Cancelled = "Cancelled";
	Ope_in_prog = "Operation in Progress";
	
	msgbox_font=" -font /fonts/lucm/unicode.9.font"
                 + "  -x 150 -y 200 -bd 2 -relief raised";
	msgbox_ques="Really cancel and lose the message being composed?";
	msgbox_ndiscard="Do not discard";
	msgbox_discard="Discard";
	msgbox_cancel="Cancel";
	msgbox_continue="Continue";
	keyboard_title="Keyboard";
	QUOTE_FONT="/fonts/lucm/unicode.9.font";
	OPERATION_PROGRESS="Operation in progress";
	OPERATION_CANCEL="Cancel";
	OPERATION_CANCELLED="Operation cancelled";	
	OPERATION_FONT="/fonts/lucidasans/latin1.7.font";

	FOLDER_FONT="/fonts/lucidasans/latin1.7.font";

}


