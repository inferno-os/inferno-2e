implement Mailtool_GUI;
include "mailtool_gui.m";

init()
{	

   # main mailtool windows parameters

        MAIN_FONT="/fonts/lucm/unicode.9.font -bd 2 -relief raised ";
        MAILTOOL_TITLE="邮件";
        MAIN_STATUS_NEWMAIL_FONT="/fonts/lucidasans/latin.6.font -fg red -height 1h";
        MAIN_STATUS_HELP_FONT="/fonts/lucidasans/latin.6.font -fg black -height 1h";
	MAIN_LISTBOX_FONT="/fonts/lucm/unicode.9.font";
	MAIN_WHERE=" -x 0 -y 28";
	MAIN_START_POINT=" -x 180 -y 0 ";
	MESSAGE_TITLE="邮件:";

        MAIN_TITLE="邮件:收信夹";

	DIALOG_MSG_FONT="/fonts/lucm/unicode.9.font";
	DIALOG_MSG_OK="确认";



        screenX=620;
        screenY=400;
        status_indicator="正在接收邮件";
        dialog_msg_main1="找不到TIME模块. \n数据可能已损坏.";
        dialog_msg_main2="文件创建错误!";
        dialog_msg_main3="邮件错误\n";

	BUTTON_OK="确认";	
	REG_TITLE="请输入邮件程序信息.";
	REG_FONT="/fonts/lucm/unicode.9.font";
	REG_POPSERVER="POP服务器:";
	REG_SMTPSERVER="SMTP服务器:";

	REG_USERLOGIN="用户注册名:";
	REG_PASSWORD="口令:";
	REG_EMAILADDRESS="邮件地址:";
	REG_SAVE="保存";
	REG_OK="确认";
	REG_NOCHANGE="不作修改/继续";
	SVR_OUTGOING="发出去的邮件";
	SVR_FONT="/fonts/lucidasans/boldlatin1.7.font";
	SVR_SMTPSERVER="SMTP 服务器:";
	SVR_INCOMING="出邮件";
	SVR_POPSERVER="POP 服务器S:";
	USR_OUTGOING="出邮件";
	USR_EMAILADDRESS="邮件地址";
	USR_INCOMING="入邮件";
	USR_USERLOGIN="用户注册名:";
	USR_FONT="/fonts/lucidasans/boldlatin1.7.font";
        USR_PASSWORD="口令:";
	USR_SAVE="保存";
	USR_CANCEL="取消";
	USR_OK="确认";
        FOLDER_OK="确认"; 
        FOLDER_CANCEL="取消";


        SERVER_OPTIONS="服务器选项"; 
        COMPOSE_TITLE="邮件";
        COMPOSE_MAIN_FONT="/fonts/lucm/unicode.9.font -borderwidth 1 -relief raised";
        COMPOSE_WIN_FONT="/fonts/lucm/unicode.9.font -bd 2 -relief raised ";

	COMPOSE_MAILTO_FONT="/fonts/lucm/unicode.9.font";
	COMPOSE_SUBJECT_FONT="/fonts/lucm/unicode.9.font";
	COMPOSE_MAILCC_FONT="/fonts/lucm/unicode.9.font";
	COMPOSE_HELP_FONT="/fonts/lucm/unicode.9.font";

	MESSAGE_FONT="/fonts/lucm/unicode.9.font";
	MESSAGE_HELP_FONT="/fonts/lucm/unicode.9.font";
        IMPATH="mail/";
#        REPLY="回复";
#        REPLYALL="回复所有人";
#        FORWARD="转发";
     

       USER_OPTIONS_TITLE="用户选择项";


# mail box alert messages
	no_mail_alert = "";
	new_mail = "新邮件";
	no_new_mail = "没有新邮件";
	please_connect = "连接并送邮件";
	working = "程序忙";

# Help messages for toolbar buttons
	file_help = "开文件夹";
	connect_help = "连接邮件服务器";
#########
##sendmail_help: con "连接并送邮件";
##########
	new_help = "编辑新邮件";
	savesel_help = "保存选中的邮件到文件夹";
	delsel_help = "删除选中的邮件";
	prev_help  = "显示前一邮件";
	next_help = "显示下一邮件";
	reply_help = "给发信人回信";
	all_help = "给所有人回信";
	fwd_help = "转发邮件";
	head_help = "显示邮件头信息";
	save_help = "保存邮件到文件夹";
	del_help = "删除邮件";

# Action status messages
	signing_on = "正在登记...";
	getting_hdrs = "获取邮件头...";
	getting_mail = "获取邮件...";
	contacting_server = "正在连接服务器...";
	messages_sent = "邮件发送成功.";
	send_error = "发送邮件时出错.";
	server_change = "服务器邮件状态改变.";

# widgets messages
	fi = "文件";
	OK = "确认";
        Check_mail="检查邮件";
	Exit = "退出";
	Next = "下一封";
	Previous = "前一封";
	Composedot = "编辑...";
	Composem = "处理";
	Savedot = "保存...";
	Save = "保存";
	Delete = "删除";
	Serverdot = "服务器...";
	Userdot = "用户...";
	Inbox = "收信夹...";
	New = "新建";
	Newdot = "新建...";
	Close = "关闭";
	headers1 = "邮件头";
	Message2 = "邮件";
	Edit = "编辑";
	Show_Head = "显示邮件头信息...";
	Copy = "复制";
	Select_all = "选择所有邮件";
	Reply_to_Sdot = "回复寄信人...";
	Reply_to_Adot = "回复所有人...";
	Forwarddot = "转发...";
	Prev = "前一封";
	Re = "回复:";
	All = "所有";
	Fwd = "转发";
	Show = "显示";
	POp = "POP服务器:";
	SMtP = "SMTP服务器:";
	User_L = "用户名:";
	Password = "口令:";
	Email_add = "邮件地址:";
	Cancel = "取消";
	mailmess1 = "为使修改后的设置信息\n";
	mailmess2 = "效,需要重新邮件程序.";
	Keyboard1 = "键盘";
	Sendin = "送邮件";
	Please_c = "请配置邮件服务器";
	folder1 = "收信夹";
	Total = "所有邮件:";
	error1 = "错误:ISP 数据空";
	error2 = "ISP 读失败";
	warn1 = "如";
	warn2 = "果该文件被删除,";
	warn3 = "那么它包含的邮件";
	warn4 = "将丢失.";
	#Message2 = "邮件:";
	Mail = "邮件";
	Warn5 = "找不到附注数据";
	To = "到";
	From = "从";
	#Subject = "标题";
	nosubject = "没有标题";
	#header = "邮件头";
	options = "选项";
	
#messages in compose Window
# Help messages for toolbar buttons
	keyboard_help = "显示/隐藏软键盘";
	deliver_help = "将邮件入信箱并关闭编辑窗口";
	cc_help = "增加附送者地址";
	attach_help = "增加附件";
	cancel_help = "取消邮件并关闭编辑窗口";
	quote_help = "附带正在回复的邮件";

	Deliver = "发送";
	Adcc = "增加 Cc..";

	Cut = "剪切";
	Paste = "粘贴";
	SelectA = "全选";
	Quote = "附着原文";
	Cc =  "附送";
	Mailto = "收信人:";
	Subject = "题目:";
	MailCC = "邮件附送:";
	Continue = "继续";
	MailW = "警告";
	Operation = "操作";
	
	Cancelled = "取消";
	Ope_in_prog = "程序正在运行";
	
	msgbox_font=" -font /fonts/lucm/unicode.9.font"
                 + "  -x 150 -y 200 -bd 2 -relief raised";
	msgbox_ques="当前编辑的邮件将丢失,真的要取消吗?";
	msgbox_ndiscard="不忽略";
	msgbox_discard="忽略";
	msgbox_cancel="取消";
	msgbox_continue="继续";
	keyboard_title="键盘";
	QUOTE_FONT="/fonts/lucm/unicode.9.font";
	OPERATION_PROGRESS="正在运行中";
	OPERATION_CANCEL="取消";
	OPERATION_CANCELLED="操作取消";	
	OPERATION_FONT="/fonts/lucm/unicode.9.font";

	FOLDER_FONT="/fonts/lucm/unicode.9.font";

}


