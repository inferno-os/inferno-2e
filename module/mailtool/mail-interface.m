GUI_extras: module
{
PATH : con "/dis/mailtool/mail-interface.dis";
#  PATH : con "mail-interface.dis";

  init : fn(ctxt: ref Draw->Context);

  #do_errors : fn(tktop : ref Toplevel, result : ref AsscList->Alist) : int;

  sendcfig : fn(mmgr : GDispatch, headerWindow : int,
		user : string, passwd : string, pop_server : string,
		smtp_server : string, email_address : string) : int;

  loadcfig : fn(mmgr : GDispatch) :
          (int, string, string, string, string, string);

  savecfig : fn(mmgr : GDispatch,
		user, passwd, pop_server, smtp_server, email_address : string) ;

  fetchheaders : fn(mmgr : GDispatch,
		  filename: string, lastmsg : int) :
		      (int, int, list of ref Header, string);

  renumber_headers : fn(headerList : list of ref Header, start : int);

  loadheaders : fn(tktop : ref Toplevel, mmgr : GDispatch,
		   nmesg : int, hdrlst : list of ref Header, listboxWidth : int) : (int, int, string);

  format_header : fn(mmgr : GDispatch,
		     header : string, number : int,
		     listboxWidth : int, status : string) : (string,string);

  short_header : fn(header : ref Header,  mmgr : GDispatch) : (string,string);

  fetch_body : fn(cmesg : int, mmgr : GDispatch) : string;

  get_msgargs : fn(header : ref Header,  mmgr : GDispatch) : ref Message;

  remove_crs : fn(str : string) : string;

  do_connect : fn (ctxt : ref Draw->Context, mmgr : GDispatch) : (int, string);
  NOCHANGE : con 0;
  SENDERROR, SENDCOMPLETE, NEWMAIL, LESSMAIL : con (1<<iota);
  
  delete_msg : fn(ctxt : ref Draw->Context,  mmgr : GDispatch,
		  cmesg, msgnum : int, headers : list of ref Header) : list of ref Header;

  Header : adt {
      msgnum : int;
      fullHeader : string;
      dispHeader : string;
  };
};
