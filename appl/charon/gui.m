Gui: module {
	PATH: con "/dis/charon/gui.dis";
	GUIWMPATH: con "/dis/charon/guiwm.dis";
	GUITBPATH: con "/dis/charon/guitb.dis";

	# Built-in icon identifiers
	IClogo, ICback, ICfwd, ICreload, ICstop, IChist, ICbmark, ICedit,
		ICconf, IChelp, IChome, ICcopy, ICsslon, ICssloff, ICup,
		ICdown, ICplus, ICminus, ICkeybd, ICexit: con iota;

	display : ref Draw->Display;
	mainwin : ref Draw->Image;
	ctlwin : ref Draw->Image;
	progwin : ref Draw->Image;
	popupwin : ref Draw->Image;

	init: fn(ctxt: ref Draw->Context, rmain, rctl, rprot: Draw->Rect, cu: CharonUtils) :
		(Draw->Rect, Draw->Rect, Draw->Rect);
	tbinit: fn(ctxt: ref Draw->Context, wc, rc: chan of string, rmain, rctl, rprot: Draw->Rect, cu: CharonUtils) :
		(Draw->Rect, Draw->Rect, Draw->Rect);
	makepopup : fn(width, height: int) : ref Draw->Image;
	geticon: fn(icon: int) : (ref Draw->Image, ref Draw->Image);
	skeyboard: fn(ctxt: ref Draw->Context);
	snarfput: fn(s: string);
	grabmouse: fn(grab : int);
};
