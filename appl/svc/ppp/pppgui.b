###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1998 Lucent Technologies
###
### Originally Written by N. W. Knauft
### Adapted by E. V. Hensbergen (ericvh@lucent.com)
###

implement PPPGUI;

include "sys.m";
        sys: Sys;

include "draw.m";
        draw: Draw;

include "tk.m";
        tk: Tk;
include "lock.m";
include "modem.m";
include "script.m";
include "pppclient.m";
	ppp: PPPClient;
include "pppgui.m";

#Screen constants
BBG: con "#C0C0C0";             # Background color for button
PBG: con "#808080";             # Background color for progress bar
LTGRN: con "#00FF80";           # Color for progress bar
BARW: con 216;			# Progress bar width
BARH: con " 9";			# Progress bar height
INCR: con 30;			# Progress bar increment size
N_INCR: con 7;			# Number of increments in progress bar width
BSIZE: con 25;			# Icon button size
ISIZE: con BSIZE + 4;		# Icon window size
DIALQUANTA : con 1000;
ICONQUANTA : con 5000;

#Globals
pppquanta := DIALQUANTA;

#Font
FONT: con "/fonts/lucidasans/latin1.7.font";

#Messages
stat_msgs := array[] of {
	"Initializing Modem",
	"Dialing Service Provider",
	"Logging Into Network",
	"Executing Login Script",
	"Script Execution Complete",
	"Logging Into Network",
	"Verifying Password",
	"Connected",
	"",
};

config_win := array[] of {
  "frame .f",
  "frame .fprog",

  "canvas .cprog -bg "+PBG+" -bd 2 -width "+string BARW+" -height "+BARH+" -relief ridge",  
  "pack .cprog -in .fprog -pady 6",

  "label .stat -text {Initializing connection...} -width 164 -font "+FONT,
  "pack .stat -in .f -side left -fill y -anchor w",

  "button .done -text Cancel -width 80 -command {send cmd cancel} -bg "+BBG+" -font "+FONT,
  "pack .fprog -side bottom -expand 1 -fill x",
  "pack .done -side right -padx 1 -pady 1 -fill y -anchor e",
  "pack .f -side left -expand 1 -padx 5 -pady 3 -fill both -anchor w",

  "pack propogate . no",
  ". configure -bd 2 -relief raised -width "+string WIDTH,
  "update",
};

config_icon := array[] of {
  "button .btn -text X -width "+string BSIZE+" -height "+string BSIZE+" -command {send tsk open} -bg "+BBG,
  "pack .btn",

  "pack propogate . no",
  ". configure -bd 0",
  ". unmap",
  "update",
};


# Create internet connect window, spawn event handler
init(ctxt: ref Draw->Context, stat: chan of int, pppmod: PPPClient, 
		 args: list of string): chan of int
{
  sys = load Sys Sys->PATH;
  draw = load Draw Draw->PATH;
  tk = load Tk Tk->PATH;

  if (sys == nil)
    return nil;

  if ((draw == nil) ||
      (tk == nil)) {
    sys->print("Iconwin: Error loading a system file\n");
    return nil;
  }

  ppp = pppmod;		# set the global

  tkargs := "";

  if (args != nil) {
    tkargs = hd args;
    args = tl args;
  } else {
	tkargs="-x 340 -y 4";
  }
    
  t := tk->toplevel(ctxt.screen, tkargs);

  for( i := 0; i < len config_win; i++)
      tk->cmd(t, config_win[i]);

  itkargs := "";
  if (args != nil) {
    itkargs = hd args;
    args = tl args;
  }

  if (itkargs == "") {
    x := int tk->cmd(t, ". cget x");
    y := int tk->cmd(t, ". cget y");
    x += WIDTH - ISIZE;
    itkargs = "-x "+string x+" -y "+string y;
  }

  ticon := tk->toplevel(ctxt.screen, itkargs);

  for( i = 0; i < len config_icon; i++)
      tk->cmd(ticon, config_icon[i]);

  tk->cmd(ticon, "image create bitmap Network -file network.bit -maskfile network.bit");
  tk->cmd(ticon, ".btn configure -image Network");

  chn := chan of int;
  spawn handle_events(t, ticon, stat, chn);
  return chn;
}

# hack timer - there has got to be a better way to do this
ppp_timer(sync: chan of int, stat: chan of int)
{
    while(1) {
        sys->sleep(pppquanta);
            alt {
	    <-sync =>
		return;
	    stat <-= -1 =>
		continue;
	}
    }
}

send(cmd : chan of string, msg : string)
{
  cmd <-= msg;
}

# Process events and pass disconnect cmd to calling app
handle_events(t, ticon: ref Tk->Toplevel, stat, chn : chan of int)
{
    cmd := chan of string;
    tk->namechan(t, cmd, "cmd");

    tsk := chan of string;
    tk->namechan(ticon, tsk, "tsk");

    connected := 0;
    winmapped := 1;
    timecount := 0;
    xmin := 0;
    x := 0;

    pppquanta = DIALQUANTA;
    sync_chan := chan of int;
    spawn ppp_timer(sync_chan, stat);

    iocmd := sys->file2chan("/chan", "pppgui");
    if (iocmd == nil) sys->print("fail: pppgui: file2chan: /chan/pppgui\n");
    else for(done := 0;done < 1;) alt {
		# remote io control
		(off, data, fid, wc) := <-iocmd.write => {
		  if (wc == nil) break;
		  spawn send(cmd, string data[0:len data]);
		  wc <-= (len data, nil);
		}
		(nil, nbytes, fid, rc) := <-iocmd.read =>
		  if (rc != nil) rc <-= (nil, "not readable");

		press := <-cmd =>
		    case press {
		        "cancel" or "disconnect" =>
			    tk->cmd(t, ".stat configure -text 'Disconnecting...");
			    tk->cmd(t, "update");
 	   			ppp->reset();
				if (!connected)
					chn <-= 666;
			    done = 1;
			* => ;
		    }

		prs := <-tsk =>
		    case prs {
		        "open" =>
			    tk->cmd(ticon, ". unmap; update");
			    tk->cmd(t, ". map; raise .; update");
			    winmapped = 1;
			    timecount = 0;
			* => ;
		    }

		s := <-stat =>
			if (s == -1) {	# just an update event
			    if ((!connected) && (winmapped)) {	# increment status bar
			        if (x < (xmin+INCR)) {
			            x+=1;
			            tk->cmd(t, ".cprog create rectangle 0 0 "+string x + BARH+" -fill "+LTGRN);
			        }
			    }
			    if (winmapped)
			    	tk->cmd(t, "raise .; update");	
			    else
				tk->cmd(ticon, "raise .; update");
			    continue;
			}
		 	if (s == ppp->s_Error) {
			    tk->cmd(t, ".stat configure -text '"+ppp->lasterror);
				if (!winmapped) {
		  		  	tk->cmd(ticon, ". unmap; update");
		 		   	tk->cmd(t, ". map; raise .");
			    }
			    tk->cmd(t, "update");
				sys->sleep(3000);	
				ppp->reset();
				if (!connected)
					chn <-= 0;			# Failure	 		
			    done = 1;
				continue;
			}		
		
			if (s == 0)
			    tk->cmd(t,".cprog create rectangle 0 0 "+string BARW + BARH+" -fill "+PBG);
			
			x = xmin = s * INCR;
			if (xmin > BARW)
			   xmin = BARW;
			tk->cmd(t, ".cprog create rectangle 0 0 "+string xmin + BARH+" -fill "+LTGRN);
			tk->cmd(t, "raise .; update");
			tk->cmd(t, ".stat configure -text '"+stat_msgs[s]);

			if ((s == ppp->s_SuccessPPP) || (s == ppp->s_Done)) {
				if (!connected)
					chn <-= 1;
				connected = 1;
				pppquanta = ICONQUANTA;

				# find and display connection speed
				speed := "?";
				fd := sys->open("/dev/modemstat", sys->OREAD);
				if (fd != nil) {
					buf := array [1024] of byte;
					n := sys->read(fd, buf, len buf);
					if (n > 1) {
						(nflds, flds) := sys->tokenize(string buf[0:n], " \t\r\n");
						while (flds != nil) {
							if (hd flds == "rcvrate") {
								speed = hd tl flds;
								break;
							}
							flds = tl flds;
						}
					}
				}
				sys->print("speed=<%s>\n", speed);
				tk->cmd(t, ".stat configure -text {"+stat_msgs[s]+" "+speed+" bps}");

				tk->cmd(t, ".done configure -text Disconnect -command 'send cmd disconnect");
				tk->cmd(t, "update");
				sys->sleep(2000);	
				tk->cmd(t, ". unmap; pack forget .fprog; update");
				winmapped = 0;
				tk->cmd(ticon, ". map; raise .; update");
			}

			tk->cmd(t, "update");
	    };
	sync_chan <-= 1;
}

pretty(s : string) : string
{
  more := "";
  (nil, ls) := sys->tokenize(s, " \t");
  if (tl ls == nil)
    more = " to ISP";
  return capitalize(s)+more;
}

capitalize(s : string) : string
{
  if (s == nil)
    return s;
  c := s[0];
  if ('a' <= c && c <= 'z')
    c += 'A' - 'a';
  s[0] = c;
  return s;
}

