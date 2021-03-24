implement Events;

include "common.m";

sys: Sys;
url: Url;
	ParsedUrl: import url;

init()
{
	sys = load Sys Sys->PATH;
	url = load Url Url->PATH;
	evchan = chan of ref Event;
}

Event.tostring(ev: self ref Event) : string
{
	s := "?";
	pick e := ev {
		Ekey =>
			t : string;
			case e.keychar {
			' ' =>	 t = "<SP>";
			'\t' => t = "<TAB>";
			'\n' => t = "<NL>";
			'\r' => t = "<CR>";
			'\b' => t = "<BS>";
			16r7F => t = "<DEL>";
			Kup => t = "<UP>";
			Kdown => t = "<DOWN>";
			Khome => t = "<HOME>";
			Kleft => t = "<LEFT>";
			Kright => t = "<RIGHT>";
			Kend => t = "<END>";
			* => t = sys->sprint("%c", e.keychar);
			}
			s = sys->sprint("key %d = %s", e.keychar, t);
		Emouse =>
			t := "?";
			case e.mtype {
			Mmove => t = "move";
			Mlbuttondown => t = "lbuttondown";
			Mlbuttonup => t = "lbuttonup";
			Mldrag => t = "ldrag";
			Mmbuttondown => t = "mbuttondown";
			Mmbuttonup => t = "mbuttonup";
			Mmdrag => t = "mdrag";
			Mrbuttondown => t = "rbuttondown";
			Mrbuttonup => t = "rbuttonup";
			Mrdrag => t = "rdrag";
			}
			s = sys->sprint("mouse (%d,%d) %s", e.p.x, e.p.y, t);
		Emove =>
			s = sys->sprint("move (%d,%d)", e.p.x, e.p.y);
		Ereshape =>
			s = sys->sprint("reshape (%d,%d) (%d,%d)", e.r.min.x, e.r.min.y, e.r.max.x, e.r.max.y);
		Eexpose =>
			s = "expose";
		Ehide =>
			s = "hide";
		Elower =>
			s = "lower";
		Eraise =>
			s = "raise";
		Equit =>
			s = "quit";
		Ehelp =>
			s = "help";
		Estop =>
			s = "stop";
		Ealert =>
			s = "alert(" + e.msg + ")";
		Econfirm =>
			s = "confirm(" + e.msg + ")";
		Eprompt =>
			s = "prompt(" + e.msg + ", " + e.inputdflt + ")";
		Eform =>
			case e.ftype {
			EFsubmit => s = "form submit";
			EFreset => s = "form reset";
			}
		Eformfield =>
			case e.fftype {
			EFFblur => s = "formfield blur";
			EFFfocus => s = "formfield focus";
			EFFclick => s = "formfield click";
			EFFselect => s = "formfield select";
			}
		Ego =>
			s = "go(";
			case e.gtype {
			EGlocation or
			EGnormal or
			EGreplace => s += e.url;
			EGreload => s += "RELOAD";
			EGforward => s += "FORWARD";
			EGback => s += "BACK";
			EGhome => s += "HOME";
			EGbookmarks => s += "BOOKMARKS";
			EGdelta => s += "HISTORY[" + string e.delta + "]";
			}
			s += ", " + e.target + ")";
		Esubmit =>
			if(e.subkind == CharonUtils->HGet)
				s = "GET";
			else
				s = "POST";
			s = "submit(" + s;
			s += ", " + e.action.tostring();
			s += ", " + e.target + ")";
	}
	return s;
}

