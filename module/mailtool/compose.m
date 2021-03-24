Compose: module
{
	PATH:	con "/dis/mailtool/compose.dis";

	NEW, REPLY, REPLYALL, FORWARD, LITERAL: con iota;

	Message: adt
	{
		mailto: string;
		subject: string;
		cc: string;
		attach: string;	
		text: string;
		mimeversion: string;
		contentype: string;

		### Add any datatypes needed to hold attachment info
	};

        initialize:	fn(ctxt: ref Draw->Context, args: list of string, msgtype: int, msg: ref Message, chn: chan of string, mmgr : GDispatch);
	get_toplevel: fn(): list of ref Tk->Toplevel;
	set_mailto: fn(top: ref Tk->Toplevel, mailto: string);
};
