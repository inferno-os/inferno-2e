#    Last change:  I     Nov. 9, 1998
###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1997 Lucent Technologies
###
### Written by Hongfan Yu & Hongyan Zhao
## 
implement MailI18N; 

include "sys.m";
include "maili18ngui.m";

sys: Sys;

# mail box alert messages
init () {
	sys = load Sys Sys->PATH;

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




}

name_language(name: string) : string {

	
	return nil;
}
