implement WmFtpfs;

include "sys.m";
include "draw.m";
include "tk.m";
include "wmlib.m";
include "styx.m";

sys:		Sys;
print:		import sys;
tk:		Tk;
wmlib:		Wmlib;
scr:		ref Draw->Screen;

dialog := array[] of {
	"frame .h",
	"label .h.l -text {Hostname:} -anchor w",
	"entry .h.e",
	"pack .h.l -side left",
	"pack .h.e -side right",
	"frame .m",
	"label .m.l -text {Mountpoint:} -anchor w",
	"entry .m.e",
	"pack .m.l -side left",
	"pack .m.e -side right",
	"frame .u",
	"label .u.l -text {User:} -anchor w",
	"entry .u.e",
	"pack .u.l -side left",
	"pack .u.e -side right",
	"frame .p",
	"label .p.l -text {Password:} -anchor w",
	"entry .p.e -show •",
	"pack .p.l -side left",
	"pack .p.e -side right",
	"frame .f -borderwidth 2 -relief raised",
	"pack .h .m .u .p -fill x -in .f",
	"pack .Wm_t -fill x",
	"pack .f -fill x",
	"bind .h.e <Key-\n> {send cmd okh}",
	"bind .m.e <Key-\n> {send cmd okm}",
	"bind .u.e <Key-\n> {send cmd oku}",
	"bind .p.e <Key-\n> {send cmd okp}",
	"focus .h.e",
	"update"
};

WmFtpfs: module
{
	init: fn(nil: ref Draw->Context, argv: list of string);
};

Ftpfs: module
{
	init: fn(nil: ref Draw->Context, argv: list of string);
};

init(ctxt: ref Draw->Context, argv: list of string)
{
	sys = load Sys Sys->PATH;
	tk = load Tk Tk->PATH;
	if (tk == nil) {
		loaderr(Tk->PATH);
		return;
	}
	wmlib = load Wmlib Wmlib->PATH;
	if (wmlib == nil) {
		loaderr(Wmlib->PATH);
		return;
	}
	scr = ctxt.screen;
	styx := load Styx Styx->PATH;
	if (styx == nil) {
		loaderr(Styx->PATH);
		return;
	}
	ftpfs := load StyxServer StyxServer->FTPFSPATH;
	if (ftpfs == nil) {
		loaderr(StyxServer->FTPFSPATH);
		return;
	}
	ftpfs->ctxt = ctxt;
	if (tl argv == nil)
		ftpfs->geometry = nil;
	else
		ftpfs->geometry = hd tl argv;

	wmlib->init();
	(top, titlectl) := wmlib->titlebar(ctxt.screen, ftpfs->geometry, "Ftpfs", Wmlib->Hide);
	wmlib->tkcmds(top, dialog);
	cmd := chan of string;
	tk->namechan(top, cmd, "cmd");
	for(;;) {
		tk->cmd(top, "update");
		alt {
		menu := <- titlectl =>
			if (menu[0] == 'e')
				return;
			wmlib->titlectl(top, menu);
		resp := <- cmd =>
			hostname := tk->cmd(top, ".h.e get");
			mountpoint := tk->cmd(top, ".m.e get");
			user := tk->cmd(top, ".u.e get");
			password := tk->cmd(top, ".p.e get");
			if (hostname == "") {
				tk->cmd(top, "focus .h.e");
				continue;
			}
			if (mountpoint == "") {
				tk->cmd(top, "focus .m.e");
				continue;
			}
			if (user == "") {
				tk->cmd(top, "focus .u.e");
				continue;
			}
			if (resp == "okp" || password != "") {
				connect(hostname, mountpoint, user, password, styx, ftpfs);
				tk->cmd(top, "focus .h.e");
			}
			else
				tk->cmd(top, "focus .p.e");
		}
	}
}

loaderr(s: string)
{
	sys->print("could not load %s: %r\n", s);
}

doconnect(h, m, u, p: string, s: Styx, f: StyxServer)
{
	r := s->serve(h + "\n" + m + "\n" + u + "\n" + p, f);
	if (r != nil)
		notice(r);
}

connect(h, m, u, p: string, s: Styx, f: StyxServer)
{
	spawn doconnect(h, m, u, p, s, f);
	exit;
}

notecmd := array[] of {
	"frame .f",
	"label .f.l -bitmap error -foreground red",
	"button .b -text Continue -command {send cmd done}",
	"focus .f",
	"bind .f <Key-\n> {send cmd done}",
	"pack .f.l .f.m -side left -expand 1 -padx 10 -pady 10",
	"pack .f .b -padx 10 -pady 10",
	"update; cursor -default"
};

notice(s: string)
{
	t := tk->toplevel(scr, "-x 70 -y 70 -borderwidth 2 -relief raised");
	cmd := chan of string;
	tk->namechan(t, cmd, "cmd");
	tk->cmd(t, "label .f.m -text {" + s + "}");
	for(i := 0; i < len notecmd; i++)
		tk->cmd(t, notecmd[i]);
	<- cmd;
}
