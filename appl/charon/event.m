Events: module {
	PATH: con "/dis/charon/event.dis";
	Event: adt {
		pick {
			Ekey =>
				keychar: int;		# Unicode char for pressed key
			Emouse =>
				p: Draw->Point;	# coords of pointer
				mtype: int;		# Mmove, etc.
			Emove =>
				p: Draw->Point;	# new top-left of moved window
			Ereshape =>
				r: Draw->Rect;		# new window place and size
			Ehelp =>
				dummy: int;	
			Eexpose =>
				dummy: int;
			Ehide =>
				dummy: int;
			Elower =>
				dummy: int;
			Eraise =>
				url: string;
			Equit =>
				dummy: int;
			Estop =>
				dummy: int;
			Ealert =>
				msg: string;		# message to show
				sync: chan of int;	# wait for answer...
			Econfirm =>
				msg: string;
				sync: chan of int;
			Eprompt =>
				msg: string;
				inputdflt: string;	# input field default
				sync: chan of (int, string);
			Eform =>
				frameid: int;		# which frame is form in
				formid: int;		# which form in the frame
				ftype: int;			# EFsubmit or EFreset
			Eformfield =>
				frameid: int;		# which frame is form in
				formid: int;		# which form in the frame
				fieldid: int;		# which formfield in the form
				fftype: int;		# EFFblur, EFFfocus, etc.
			Ego =>
				url: string;			# where to go
				target: string;		# frame to replace
				delta: int;		# History.go(delta)
				gtype: int;
			Esubmit =>
				subkind: int;		# CU->HGet or CU->HPost
				action: ref Url->ParsedUrl;
				target: string;
		}

		tostring: fn(e: self ref Event) : string;
	};

	# Events sent to scripting engines
	ScriptEvent: adt {
		kind: int;
		frameid: int;
		formid: int;
		fieldid: int;
		anchorid: int;
		imageid: int;
		x: int;
		y: int;
		which: int;
		reply: chan of int;	# onreset/onsubmit reply channel
	};

	# ScriptEvent kinds
	SEonclick, SEonmouseover, SEonmouseout, SEonblur, SEonfocus,
		SEonchange, SEonload, SEtimeout, SEonabort, SEonerror,
		SEonreset, SEonselect, SEonsubmit, SEonunload : con iota;

	# some special keychars (use Unicode Private Area)
	Kup, Kdown, Khome, Kleft, Kright, Kend, Kaup, Kadown : con (iota + 16rF000);

	# Mouse event subtypes
	Mmove, Mlbuttondown, Mlbuttonup, Mldrag, Mldrop,
		Mmbuttondown, Mmbuttonup, Mmdrag,
		Mrbuttondown, Mrbuttonup, Mrdrag : con iota;

	# Form event subtypes
	EFsubmit, EFreset : con iota;

	# FormField event subtypes
	EFFblur, EFFfocus, EFFclick, EFFselect, EFFnone : con iota;

	# Go event subtypes
	EGnormal, EGreplace, EGreload, EGforward, EGback, EGhome, EGbookmarks, EGdelta, EGlocation: con iota;

	init: fn();
	evchan: chan of ref Event;
};
