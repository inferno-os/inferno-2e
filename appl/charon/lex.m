Lex: module
{
	PATH: con "/dis/charon/lex.dis";

	# HTML 4.0 tags (blink, nobr)
	# sorted in lexical order; used as array indices
	Notfound, Comment,
	Ta, Tabbr, Tacronym, Taddress, Tapplet, Tarea, Tb,
		Tbase, Tbasefont, Tbdo, Tbig, Tblink, Tblockquote, Tbody,
		Tbq, Tbr, Tbutton, Tcaption, Tcenter, Tcite, Tcode, Tcol, Tcolgroup,
		Tdd, Tdel, Tdfn, Tdir, Tdiv, Tdl, Tdt, Tem,
		Tfieldset, Tfont, Tform, Tframe, Tframeset,
		Th1, Th2, Th3, Th4, Th5, Th6, Thead, Thr, Thtml, Ti, Tiframe, Timg,
		Tinput, Tins, Tisindex, Tkbd, Tlabel, Tlegend, Tli, Tlink, Tmap, Tmenu,
		Tmeta, Tnobr, Tnoframes, Tnoscript,
		Tobject, Tol, Toptgroup, Toption, Tp, Tparam, Tpre,
		Tq, Ts, Tsamp, Tscript, Tselect, Tsmall, Tspan, Tstrike, Tstrong,
		Tstyle, Tsub, Tsup, Ttable, Ttbody, Ttd, Ttextarea, Ttfoot, Tth,
		Tthead, Ttitle, Ttr, Ttt, Tu, Tul, Tvar,
		Numtags
			: con iota;
	RBRA : con Numtags;
	Data: con Numtags+RBRA;

	tagnames: array of string;

	# HTML 4.0 tag attributes
	# Keep sorted in lexical order
	Aabbr, Aaccept_charset, Aaccess_key, Aaction,
		Aalign, Aalink, Aalt, Aarchive, Aaxis,
		Abackground, Abgcolor, Aborder,
		Acellpadding, Acellspacing, Achar, Acharoff,
		Acharset, Achecked, Acite, Aclass, Aclassid, Aclear,
		Acode, Acodebase, Acodetype,
		Acolor, Acols, Acolspan, Acompact, Acontent, Acoords,
		Adata, Adatetime, Adeclare, Adefer, Adir, Adisabled,
		Aenctype, Aface, Afor, Aframe, Aframeborder,
		Aheaders, Aheight, Ahref, Ahreflang, Ahspace, Ahttp_equiv,
		Aid, Aismap, Alabel, Alang, Alink, Alongdesc, Alowsrc,
		Amarginheight, Amarginwidth, Amaxlength, Amedia, Amethod, Amultiple,
		Aname, Anohref, Anoresize, Anoshade, Anowrap, Aobject,
		Aonabort, Aonblur, Aonchange, Aonclick, Aondblclick,
		Aonerror, Aonfocus, Aonkeypress, Aonkeyup, Aonload,
		Aonmousedown, Aonmousemove, Aonmouseout, Aonmouseover,
		Aonmouseup, Aonreset, Aonselect, Aonsubmit, Aonunload,
		Aprofile, Aprompt, Areadonly, Arel, Arev, Arows, Arowspan, Arules,
		Ascheme, Ascope, Ascrolling, Aselected, Ashape, Asize,
		Aspan, Asrc, Astandby, Astart, Astyle, Asummary,
		Atabindex, Atarget, Atext, Atitle, Atype, Ausemap,
		Avalign, Avalue, Avaluetype, Aversion, Avlink, Avspace, Awidth,
		Numattrs
			: con iota;

	attrnames: array of string;

	Token: adt
	{
		tag:		int;
		text:		string;	# text in Data, attribute text in tag
		attr:		list of Attr;
		starti:	int;		# index into source buffer where token starts

		aval: fn(t: self ref Token, attid: int) : (int, string);
		tostring: fn(t: self ref Token) : string;
	};

	Attr: adt
	{
		attid:		int;
		value:	string;
	};

	# A source of HTML tokens.
	# After calling new with a ByteSource (which is past 'gethdr' stage),
	# call gettoks repeatedly until get nil.  Errors are signalled by exceptions.
	# Possible exceptions raised:
	#	EXInternal		(start, gettoks)
	#	exGeterror	(gettoks)
	#	exAbort		(gettoks)
	TokenSource: adt
	{
		i: int;						# index of next byte to use
		b: ref CharonUtils->ByteSource;
		chset: int;					# one of CU->US_Ascii, etc.
		mtype: int;				# CU->TextHtml or CU->TextPlain
		ihigh: int;					# high water mark of i

		new: fn(b: ref CharonUtils->ByteSource, chset, mtype: int) : ref TokenSource;
		gettoks: fn(ts: self ref TokenSource) : array of ref Token;
	};

	init: fn(cu: CharonUtils);
};
