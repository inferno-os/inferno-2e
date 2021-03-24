###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1998 Lucent Technologies
###
###

implement Keyboard;

include "sys.m";
        sys: Sys;

include "draw.m";
        draw: Draw;

include "tk.m";
        tk: Tk;

include "wmlib.m";
        wmlib: Wmlib;

include "key.m";

#Icon path
ICPATH: con "keykb/";

#Font
FONT: con "/fonts/lucidasans/latin1.7.font";
SPECFONT: con "/fonts/lucidasans/latin1.6.font";

# Dimension constants
KBDWIDTH: con 360;
KBDHEIGHT: con 137;
KEYWIDE: con "20";
KEYHIGH: con "22";
KEYSPACE: con 4;
KEYBORDER: con 1;
KEYGAP: con KEYSPACE - (2 * KEYBORDER);
ENDGAP: con 2 - KEYBORDER;

# Row size constants (cumulative)
ROW1: con 14;
ROW2: con 29;
ROW3: con 43;
ROW4: con 57;
NKEYS: con 69;

#Special key number constants
F1KEY,  F2KEY, F3KEY, F4KEY, F5KEY, F6KEY, F7KEY, F8KEY, F9KEY, F10KEY,
F11KEY, F12KEY, DELKEY, HOMKEY, TABKEY : con (iota + 0);
TOGKEY, PONKEY, MSTAKEY, PGUPKEY, CAPSLOCKKEY : con (iota + 25);
PERKEY, RETURNKEY, PGDOWKEY, LSHIFTKEY : con (iota + 40);
QUESKEY, DOTKEY, COKEY, CHAKEY, RSHIFTKEY, UPKEY, FINKEY, SCAKEY, SCDKEY, TARRKEY,
CTRLKEY, ALTKEY, SPACEKEY, ALTGKEY : con (iota + 50);
DIRKEY, SUPPKEY, LEFTKEY, DOWNKEY, RIGHTKEY : con (iota + 64);

#Tog keys,
#tog : int;
toga: con 15;
toge: con 17;
togi: con 22;
togo: con 23;
togu: con 21;
dots: con 16r00A8;
roc: con 16r005E;

#Special key code constants
METAKEY: con -11;
CAPSLOCK: con -1 ;
SHIFT: con -2;
CTRL: con -3;
ALT: con -4;
META: con -5;
MAGIC_PREFIX: con 256;
ARROW_PREFIX: con 57362;
SHIFTAB: con 16re209;
ALTGR: con -6;
TOG: con -7;
TOGSHIF: con -8;
SHIFTOG: con -9;
SHIFTOGSHIF: con -10;

stogs: int;
stog: int;
altgr_active: int;
shif: int;
toged: int;

#Special key width constants
F1SIZE: con 27; 
F2SIZE, F3SIZE, F4SIZE, F5SIZE, F6SIZE, F7SIZE, F8SIZE, F9SIZE : con 20;
F1XSIZE, F2XSIZE, F3XSIZE, QUESIZE, DOTSIZE : con 19;
COKSIZE, CHASIZE: con 19;
DELSIZE: con 30;
TABSIZE: con 15;
PONSIZE: con 21;
TOGKEYSIZE: con 22;
CAPSLOCKSIZE: con 21;
RETURNSIZE: con 35;
LSHIFTSIZE: con 32;
RSHIFTSIZE: con 31;
#ESCSIZE: con 21;
CTRLSIZE: con 18;
METASIZE: con 34;
ALTSIZE: con 18;
HOMSIZE, PGUPSIZE, PGDOWSIZE, MSTASIZE, UPSIZE, DOWNSIZE, LEFTSIZE, RIGHTSIZE,FINSIZE: con 14; 
PERSIZE: con 18;
SCASIZE: con 17;
SCDSIZE: con 17;
TARRSIZE: con 16; 
SPACESIZE: con 93;
DIRSIZE: con 13;
ALTGSIZE: con 31;
#ENTERSIZE: con 31;
SUPPSIZE: con 31; 

#circumflex, and diaeresis key values
cA: con 16r00C2;
dA: con 16r00C4;
ca: con 16r00E2;
da: con 16r00E4;

cE: con 16r00CA;
dE: con 16r00CB;
ce: con 16r00EA;
de: con 16r00EB;

cI: con 16r00CE;
dI: con 16r00CF;
ci: con 16r00EE;
di: con 16r00EF;

cO: con 16r00D4;
dO: con 16r00D6;
cok: con 16r00F4;
dok: con 16r00F6;
 
cU: con 16r00DB;
dU: con 16r00DC;
cu: con 16r00FB;
du: con 16r00FC;

cndot: con 16r00E7;
ponde: con 16r00A3;
ostar: con 16r00A4;
charp: con 16r00A7;

asla: con 16r00E0;
esla1: con 16r00E8;
esla2: con 16r00E9;
usla: con 16r00F9;
mu: con 16r00B5;
sqtw: con 16r00B2;
upso: con 16r00B0;
ap: con 16r00E0;
#Arrow key code constants
HOME: con ARROW_PREFIX - 2;
END: con ARROW_PREFIX - 1;

UP: con ARROW_PREFIX;
DOWN: con ARROW_PREFIX + 1;
LEFT: con ARROW_PREFIX + 2;
RIGHT: con ARROW_PREFIX + 3;

PGUP: con ARROW_PREFIX + 4;
PGDW: con ARROW_PREFIX + 5;

SCREENA: con 57443;
SCREENM: con 57444;

TGED: con -10;

direction:= array[] of {"up", "down", "left", "right"};
row_dimensions:= array[] of {0, ROW1, ROW2, ROW3, ROW4, NKEYS};

special_keys:= array[] of {
  (F1KEY, F1SIZE), (F2KEY, F2SIZE), (F3KEY, F3SIZE), (F4KEY, F4SIZE),
  (F5KEY, F5SIZE), (F6KEY, F6SIZE), (F7KEY, F7SIZE), (F8KEY, F8SIZE),
  (F9KEY, F9SIZE), (F10KEY, F1XSIZE), (F11KEY, F2XSIZE), (F12KEY, F3XSIZE),
  (DELKEY, DELSIZE), (PONKEY, PONSIZE), (MSTAKEY, MSTASIZE), (PERKEY, PERSIZE),
  (QUESKEY, QUESIZE), (COKEY, COKSIZE), (DOTKEY, DOTSIZE), (TARRKEY, TARRSIZE),
  (TABKEY, TABSIZE), (PGUPKEY, PGUPSIZE), (PGDOWKEY, PGDOWSIZE), 
  (UPKEY, UPSIZE), (LEFTKEY, LEFTSIZE), (DOWNKEY, DOWNSIZE), 
  (RIGHTKEY, RIGHTSIZE), (HOMKEY, HOMSIZE),
  (CAPSLOCKKEY, CAPSLOCKSIZE),
  (RETURNKEY, RETURNSIZE),
  (LSHIFTKEY, LSHIFTSIZE),
  (RSHIFTKEY, RSHIFTSIZE),
  (FINKEY, FINSIZE), 
  (SCAKEY, SCASIZE),
  (SCDKEY, SCDSIZE), 
  (CTRLKEY, CTRLSIZE),
  (METAKEY, METASIZE),
  (ALTKEY, ALTSIZE), (TARRKEY, TARRSIZE), (DIRKEY, DIRSIZE),
  (SPACEKEY, SPACESIZE),
  (ALTGKEY, ALTGSIZE),
  (SUPPKEY, SUPPSIZE),
};

 # Alted value
 altv := array[] of { sqtw, '~', '#', '{', '[', '|', '`', '\\', '^', '@', ']', '}', };

keys:= array[] of {
  # Unshifted
    "1\n& ²", "2\né ~", "3\n\" #", "4\n\' \\{", "5\n( [", "6\n- |", "7\nè `", "8\n_ \\\\", "9\nç ^", "0\nà @",  "°\n) ]", "+\n= )", "<--", "H",
    "->", "a", "z", "e", "r", "t", "y", "u", "i", "o", "p", "..", "£\n$ ¤", "µ\n*", "Pgup",
    "Cap\nLoc", "q", "s", "d", "f", "g", "h", "j", "k", "l", "m", "%\nù", "Entrée", "Pgdow",
    "Shift", "w", "x", "c", "v", "b", "n", "?\n,", ".\n;", "/\n:", "§\n!", "Shift", "^", "Fin",
    "Sc\n+", "Sc\n-", ">\n<", "Ctl", "Alt", "", "Alt Gr", "@", "Suppr", "<-", "v", "->",
  # Shifted
    "1\n& ²", "2\né ~", "3\n\" #", "4\n\' \\{", "5\n( [", "6\n- |", "7\nè `", "8\n_ \\\\", "9\nç ^", "0\nà @",  "°\n) ]", "+\n= )", "<--", "H",
    "->", "A", "Z", "E", "R", "T", "Y", "U", "I", "O", "P", "..", "£\n$ ¤", "µ\n*", "Pgup",
    "Cap\nLoc", "Q", "S", "D", "F", "G", "H", "J", "K", "L", "M", "%\nù", "Entrée", "Pgdow",
    "Shift", "W", "X", "C", "V", "B", "N", "?\n,", ".\n;", "/\n:", "§\n!", "Shift", "^", "Fin",
    "Sc\n+", "Sc\n-", ">\n<", "Ctl", "Alt", "", "Alt Gr", "@", "Suppr", "<-", "v", "->",

};

#Why we let SCREENM has the same value as DEL?
keyvals:= array[] of {
  # Unshifted
    '&', esla2, '"', '\'', '(', '-', esla1, '_', cndot, ap, ')', '=', '\b', HOME,
    '\t', 'a', 'z', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', TOG, '$', '*', PGUP,
    CAPSLOCK, 'q', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm', usla, '\n', PGDW,
    SHIFT, 'w', 'x', 'c', 'v', 'b', 'n', ',', ';', ':', '!',  SHIFT, UP,END,
    SCREENA, SCREENM, '<', CTRL, ALT, 32, ALTGR, '@', SCREENM, LEFT, DOWN, RIGHT,
  # Shifted
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', upso, '+', '\b', HOME,
    '\t', 'A', 'Z', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', TOG, ponde, mu, PGUP,
    CAPSLOCK, 'Q', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M', '%', '\n', PGDW,
    SHIFT, 'W', 'X', 'C', 'V', 'B', 'N', '?', '.', '/', charp, SHIFT, UP, END,
    SCREENA, SCREENM, '>', CTRL,  ALT, 32, ALTGR, '@', SCREENM, LEFT, DOWN, RIGHT,
};

rowlayout := array[] of {
  "frame .f1",
  "frame .f2",
  "frame .f3",
  "frame .f4",
  "frame .f5",
  "frame .dummy0 -height " + string (ENDGAP),
  "frame .dummy1 -height " + string KEYGAP,
  "frame .dummy2 -height " + string KEYGAP,
  "frame .dummy3 -height " + string KEYGAP,
  "frame .dummy4 -height " + string KEYGAP,
  "frame .dummy5 -height " + string (ENDGAP + 1),
};

# Move key flags
move_key_enabled := 0;
meta_active := 0;

# Create keyboard widget, spawn keystroke handler
initialize(t: ref Tk->Toplevel, ctxt : ref Draw->Context, dot: string): chan of string
{
  dummy := chan of string;
  return chaninit(t, ctxt, dot, dummy);
}

chaninit(t: ref Tk->Toplevel, ctxt : ref Draw->Context, dot: string, rc: chan of string): chan of string
{
  sys = load Sys Sys->PATH;
  draw = load Draw Draw->PATH;
  tk = load Tk Tk->PATH;
  wmlib = load Wmlib Wmlib->PATH;

  wmlib->init();

  tk->cmd(t, "frame " + dot + " -bd 2 -relief raised -width " + string KBDWIDTH 
          + " -height " + string KBDHEIGHT);
  wmlib->tkcmds(t, rowlayout);

  i: int;
  for(i = 0; i < NKEYS; i++) {
    tk->cmd(t, "button .b" + string i + " -font " + FONT + " -width " + KEYWIDE
	    + " -height " + KEYHIGH + " -bd " + string KEYBORDER);

    tk->cmd(t, ".b" + string i + " configure -text {" + keys[i]
            + "} -command 'send keypress " + string keyvals[i]);
  }

  keynum, keysize: int;
  for(i = 0; i < len special_keys; i++) {
    (keynum, keysize) = special_keys[i];
    tk->cmd(t, ".b" + string keynum + " configure -font " + SPECFONT + " -width " + string keysize);
  }
  
  
  tk->cmd(t, "image create bitmap TOG -file " + ICPATH + "tog.bit -maskfile " + ICPATH + "tog.bit");
  
  tk->cmd(t, "image create bitmap Home -file " + ICPATH + "hom.bit -maskfile " + ICPATH + "hom.bit");
  tk->cmd(t, "image create bitmap Pgup -file " + ICPATH + "pgup.bit -maskfile " + ICPATH + "pgup.bit");
  tk->cmd(t, "image create bitmap Pgdow -file " + ICPATH + "pgdw.bit -maskfile " + ICPATH + "pgdw.bit");
  tk->cmd(t, "image create bitmap Shift -file " + ICPATH + "shift.bit -maskfile " + ICPATH + "shift.bit"); 
  #tk->cmd(t, "image create bitmap Capslock_on -file " + ICPATH + "capson.bit -maskfile " + ICPATH + "capson.bit");
  #tk->cmd(t, "image create bitmap Capslock_off -file " + ICPATH + "capsoff.bit -maskfile " + ICPATH + "capsoff.bit");
  tk->cmd(t, "image create bitmap Left_arrow -file " + ICPATH + "larrow.bit -maskfile " + ICPATH + "larrow.bit");
  tk->cmd(t, "image create bitmap Right_arrow -file " + ICPATH + "rarrow.bit -maskfile " + ICPATH + "rarrow.bit");
  tk->cmd(t, "image create bitmap Down_arrow -file " + ICPATH + "darrow.bit -maskfile " + ICPATH + "darrow.bit");
  tk->cmd(t, "image create bitmap Up_arrow -file " + ICPATH + "uarrow.bit -maskfile " + ICPATH + "uarrow.bit");
  


  
   tk->cmd(t, ".b" + string TOGKEY + " configure -image TOG");
  
  tk->cmd(t, ".b" + string HOMKEY + " configure -image Home");
  tk->cmd(t, ".b" + string PGUPKEY + " configure -image Pgup");
  tk->cmd(t, ".b" + string PGDOWKEY + " configure -image Pgdow");
  tk->cmd(t, ".b" + string LSHIFTKEY + " configure -image Shift");
  tk->cmd(t, ".b" + string RSHIFTKEY + " configure -image Shift"); 
  #tk->cmd(t, ".b" + string CAPSLOCKKEY + " configure -image Capslock_off");
  tk->cmd(t, ".b" + string LEFTKEY + " configure -image Left_arrow");
  tk->cmd(t, ".b" + string RIGHTKEY + " configure -image Right_arrow");
  tk->cmd(t, ".b" + string DOWNKEY + " configure -image Down_arrow");
  tk->cmd(t, ".b" + string UPKEY + " configure -image Up_arrow");

  rowstart, rowend: int;
  for(j:=1; j < len row_dimensions; j++) {
    rowstart = row_dimensions[j-1];
    rowend = row_dimensions[j];
    for(i=rowstart; i<rowend; i++) {
      if (i == rowstart) {
        tk->cmd(t, "frame .f" + string j + ".dummy -width " + string KEYGAP);
        tk->cmd(t, "pack .f" + string j + ".dummy -side left");
      }
      tk->cmd(t, "pack .b" + string i + " -in .f" + string j + " -side left");
      if (i == rowend-1)
        tk->cmd(t, "frame .f" + string j + ".dummy" + string i + " -width " + string KEYGAP);
      else
        tk->cmd(t, "frame .f" + string j + ".dummy" + string i + " -width " + string KEYGAP);
      tk->cmd(t, "pack .f" + string j + ".dummy" + string i + " -side left");
    }
  }

  tk->cmd(t, "pack .dummy0 .f1 .dummy1 .f2 .dummy2 .f3 .dummy3 .f4 .dummy4 .f5 .dummy5 -in " + dot);
  tk->cmd(t,"update");

  key := chan of string;
  spawn handle_keyclicks(t, ctxt, key, rc);
  return key;
}


# Process key clicks and hand keycodes off to Tk
handle_keyclicks(t: ref Tk->Toplevel, ctxt : ref Draw->Context, sc, rc: chan of string)
{
    keycode : int;
    keypress := chan of string;
    tk->namechan(t, keypress, "keypress");

    caps_locked := 0;
    shifted := 0;
    ctrl_active := 0;
     alt_active := 0;
#############
  altf := 0;
  tog := 0;
  shiftog := 0;
 # stogs := 0;
################
    for (done := 0;done < 1;) alt {
	k := <-keypress =>
	    (n, cmdstr) := sys->tokenize(k, " \t\n");
        keycode = int hd cmdstr;
        case keycode {
	    CAPSLOCK => {
	        altgr_active = 0;
		tog = 0;	
                redisplay_keyboard(t,  tog, caps_locked ^= 1, caps_locked);
		shifted = 0;
		ctrl_active = 0;
		alt_active = 0;
	    }
	    SHIFT => {
	          if(stog)
	             stogs = 1;
	         altgr_active = 0; 
		  redisplay_keyboard(t, tog,  (shifted ^= 1) ^ caps_locked, caps_locked);
	          stogs = 0;
	          stog = 0; 
            	  tog = 0; 
		}
	    CTRL => {
		ctrl_active ^= 1;
	        tog =0;	
              	altgr_active = 0; 
		if (shifted) {
		    redisplay_keyboard(t, tog,  caps_locked, caps_locked);
		    shifted = 0;
		}
		alt_active = 0;
	    }
	    
	     ALT => {
		altgr_active = 0;	
		alt_active ^= 1;
		if (shifted) {
		    redisplay_keyboard(t, tog, caps_locked, caps_locked);
		    shifted = 0;
		}
	    }
	    ALTGR => {
	         tog = 0;
	         stog = 0;	
                 altgr_active ^= 1;
	         
		 redisplay_keyboard(t, tog,  caps_locked, caps_locked);
		    
		 shifted = 0;
		
		  ctrl_active = 0;
	    }

###################
	   
       	    TOG  =>
	        shift := 0;	
	        altgr_active = 0; 
	 	if(shifted) {
                  #redisplay_keyboard(t, tog, caps_locked, caps_locked);  
	          shifted = 0; 
	          stog = 1;
		  shift = 1;	
		}	
		tog ^= 1;
		if(!tog) {
		  shifted = 0;
		  stogs = 0;
		  stog = 0;
		}	
                togkbd(t,  tog, shift, caps_locked); 
	       #sys->print("tog is%d", tog);  
	    TGED => {
	       tog = 0;
	       stog = 0;
	       shifted = 0;
	       altgr_active = 0; 
        	redisplay_keyboard(t, tog, caps_locked, caps_locked);
	       
	     }
######################## 
	    * => {
		if (ctrl_active) {
		    keycode = ctrl_char(keycode);
		    ctrl_active = 0;
		#} else if (alt_active) {
		  
		  #  keycode += MAGIC_PREFIX;
		   # alt_active = 0;
		} 
		if (meta_active && UP <= keycode && keycode <= RIGHT) {
		    spawn send_move_msg(direction[keycode - ARROW_PREFIX], sc);
		} else 
		    tk->keyboard(ctxt.screen, keycode);
		
		tog = 0;
		stog = 0;
		stogs = 0;  
		altgr_active = 0;	
		redisplay_keyboard(t, tog,  caps_locked, caps_locked);
		shifted = 0;
	        
            }
	}
	s := <-rc =>
	  if (s == "kill")
	    done = 1;
    }
}

send_move_msg(dir: string, ch: chan of string)
{
  ch <-= dir;
}


# Redisplay keyboard to reflect current state (shifted or unshifted)
redisplay_keyboard(t: ref Tk->Toplevel, tog,  shifted, caps_locked: int)
{
  base, n: int;
#  sys->print("shift %d", shifted);
  if (shifted)
    base = NKEYS;
  else
    base = 0;
   

  for(i:=0; i<NKEYS; i++) {
    n = base + i;
    
    if (tog) {
       
       if(base) { 
        if (stogs) { 
           case i { 
          toga =>
              tk->cmd(t, ".b" + string i + " configure -text {Ä} -command 'send keypress " + string dA);
            # -command 'send keypress " + string dA);
          toge =>
               tk->cmd(t, ".b" + string i + " configure -text {Ë} -command 'send keypress " + string dE);
          togi =>
               tk->cmd(t, ".b" + string i + " configure -text {Ï} -command 'send keypress " + string dI);
          togo =>
               tk->cmd(t, ".b" + string i + " configure -text {Ö} -command 'send keypress " + string dO); 
          togu =>
               tk->cmd(t, ".b" + string i + " configure -text {Ü} -command 'send keypress " + string dU); 
          12 or 13 or 14 or 25 or 28 or 29 or 41 or 42 or 43 or 54 or 55 or 56 or 66 or 67 or 68 =>
               tk->cmd(t, ".b" + string i + " configure -text {" + keys[n]
             + "} -command 'send keypress " + string keyvals[n]);
           SPACEKEY =>
		tk->cmd(t, ".b" + string i + " configure -text {¨} -command 'send keypress " + string dots); 
 
	   * => 
          
              tk->cmd(t, ".b" + string i + " configure -text {" + 
                + "} -command 'send keypress " + string TGED);  
         }  
          
        } else {  
          case i { 
          toga =>
              tk->cmd(t, ".b" + string i + " configure -text {Â} -command 'send keypress " + string cA);
          toge =>
               tk->cmd(t, ".b" + string i + " configure -text {Ê} -command 'send keypress " + string cE);
          togi =>
               tk->cmd(t, ".b" + string i + " configure -text {Î} -command 'send keypress " + string cI);
          togo =>
               tk->cmd(t, ".b" + string i + " configure -text {Ô} -command 'send keypress " + string cO); 
          togu =>
               tk->cmd(t, ".b" + string i + " configure -text {Û} -command 'send keypress " + string cU); 
          12 or 13 or 14 or 25 or 28 or 29 or 41 or 42 or 43 or 54 or 55 or 56 or 66 or 67 or 68 =>
               tk->cmd(t, ".b" + string i + " configure -text {" + keys[n]
             + "} -command 'send keypress " + string keyvals[n]);
          SPACEKEY =>
	       tk->cmd(t, ".b" + string i + " configure -text {^} -command 'send keypress " + string roc); 
	   * => 
              
              tk->cmd(t, ".b" + string i + " configure -text {" + 
                + "} -command 'send keypress " + string TGED);    
          } 
         }   
        } else if (stog) {
          case i { 
          toga =>
              tk->cmd(t, ".b" + string i + " configure -text {ä} -command 'send keypress " + string da);
          toge =>
               tk->cmd(t, ".b" + string i + " configure -text {ë} -command 'send keypress " + string de);
          togi =>
               tk->cmd(t, ".b" + string i + " configure -text {ï} -command 'send keypress " + string di);
          togo =>
               tk->cmd(t, ".b" + string i + " configure -text {ö} -command 'send keypress " + string dok); 
          togu =>
               tk->cmd(t, ".b" + string i + " configure -text {ü} -command 'send keypress " + string du); 
          12 or 13 or 14 or 25 or 28 or 29 or 41 or 42 or 43 or 54 or 55 or 56 or 66 or 67 or 68 =>
               tk->cmd(t, ".b" + string i + " configure -text {" + keys[n]
             + "} -command 'send keypress " + string keyvals[n]);
           SPACEKEY =>
		tk->cmd(t, ".b" + string i + " configure -text {¨} -command 'send keypress " + string dots); 
 
          * => 
              tk->cmd(t, ".b" + string i + " configure -text {" + 
                + "} -command 'send keypress " + string TGED);    
          }    
        } else {  
          case i { 
          toga =>
              tk->cmd(t, ".b" + string i + " configure -text {â}  -command 'send keypress " + string ca);
          toge =>
               tk->cmd(t, ".b" + string i + " configure -text {ê} -command 'send keypress " + string ce);
          togi =>
               tk->cmd(t, ".b" + string i + " configure -text {î} -command 'send keypress " + string ci);
          togo =>
               tk->cmd(t, ".b" + string i + " configure -text {ô} -command 'send keypress " + string cok); 
          togu =>
               tk->cmd(t, ".b" + string i + " configure -text {û} -command 'send keypress " + string cu); 
          12 or 13 or 14 or 25 or 28 or 29 or 41 or 42 or 43 or 54 or 55 or 56 or 66 or 67 or 68 =>
               tk->cmd(t, ".b" + string i + " configure -text {" + keys[n]
             + "} -command 'send keypress " + string keyvals[n]);
           SPACEKEY =>
		tk->cmd(t, ".b" + string i + " configure -text {^} -command 'send keypress " + string roc); 
 
          * => 
              tk->cmd(t, ".b" + string i + " configure -text {" + 
                + "} -command 'send keypress " + string TGED);    
          }    
	}
      }
      else {
         
        tk->cmd(t, ".b" + string i + " configure -text {" + keys[n]
             + "} -command 'send keypress " + string keyvals[n]);
             
       if(altgr_active) {
      
         if(i< 12) 
            tk->cmd(t, ".b" + string i + " configure -text {" + keys[i]
               + "} -command 'send keypress " + string altv[i]);
             
         if (i==26)
            tk->cmd(t, ".b" + string i + " configure -text {" + keys[i]
             + "} -command 'send keypress " + string ostar);  
        }
      }
  }
   
  #if (caps_locked)
  #  tk->cmd(t, ".b" + string CAPSLOCKKEY + " configure -image Capslock_on");
  #else
  #  tk->cmd(t, ".b" + string CAPSLOCKKEY + " configure -image Capslock_off");
  tk->cmd(t, "update");
}

# after shift tog shift, we'll display upper diaeresis
stogs_display(t: ref Tk->Toplevel) {
   tk->cmd(t, ".b" + string toga + "  configure -text A -command 'send keypress " + string dA);  
     	   
   tk->cmd(t, ".b" + string togu + "  configure -text U -command 'send keypress " + string dU);   
   tk->cmd(t, ".b" + string toge + "  configure -text E -command 'send keypress " + string dE);  
   tk->cmd(t, ".b" + string togi + "  configure -text I -command 'send keypress " + string dI);  
   tk->cmd(t, ".b" + string togo + "  configure -text O -command 'send keypress " + string dO);  
   tk->cmd(t, "update");
}

# Tog key functions
togkbd(t: ref Tk->Toplevel,  tog,  shifted, caps_locked: int)
{
    base, n: int;

  if (shifted)
    base = NKEYS;
  else
    base = 0;
   

  for(i:=0; i<NKEYS; i++) {
    n = base + i;
    
    if (tog) {
       
       if(base) { 
        
          case i { 
          toga =>
              tk->cmd(t, ".b" + string i + " configure -text {ä} -command 'send keypress " + string da);
          toge =>
               tk->cmd(t, ".b" + string i + " configure -text {ë} -command 'send keypress " + string de);
          togi =>
               tk->cmd(t, ".b" + string i + " configure -text {ï} -command 'send keypress " + string di);
          togo =>
               tk->cmd(t, ".b" + string i + " configure -text {ö} -command 'send keypress " + string dok); 
          togu =>
               tk->cmd(t, ".b" + string i + " configure -text {ü} -command 'send keypress " + string du); 
          12 or 13 or 14 or 25 or 28 or 29 or 41 or 42 or 43 or 54 or 55 or 56 or 66 or 67 or 68 =>
               tk->cmd(t, ".b" + string i + " configure -text {" + keys[n]
             + "} -command 'send keypress " + string keyvals[n]);
           SPACEKEY =>
	        tk->cmd(t, ".b" + string i + " configure -text {¨} -command 'send keypress " + string dots); 
	   * => 
             
              tk->cmd(t, ".b" + string i + " configure -text {" + 
                + "} -command 'send keypress " + string TGED);    
           } 
             
        
         } else {  
          case i { 
          toga =>
              tk->cmd(t, ".b" + string i + " configure -text {â} -command 'send keypress " + string ca);
          toge =>
               tk->cmd(t, ".b" + string i + " configure -text {ê} -command 'send keypress " + string ce);
          togi =>
               tk->cmd(t, ".b" + string i + " configure -text {î} -command 'send keypress " + string ci);
          togo =>
               tk->cmd(t, ".b" + string i + " configure -text {ô} -command 'send keypress " + string cok); 
          togu =>
               tk->cmd(t, ".b" + string i + " configure -text {û} -command 'send keypress " + string cu); 
          12 or 13 or 14 or 25 or 28 or 29 or 41 or 42 or 43 or 54 or 55 or 56 or  66 or 67 or 68 =>
               tk->cmd(t, ".b" + string i + " configure -text {" + keys[n]
             + "} -command 'send keypress " + string keyvals[n]);
         	
           SPACEKEY =>
	        tk->cmd(t, ".b" + string i + " configure -text {^} -command 'send keypress " + string roc);  
	   * => 
              tk->cmd(t, ".b" + string i + " configure -text {" + 
                + "} -command 'send keypress " + string TGED);    
          }    
	}
      }
      else {
          
        
        
        tk->cmd(t, ".b" + string i + " configure -text {" + keys[n]
             + "} -command 'send keypress " + string keyvals[n]);
       
      }
  
     }
     tk->cmd(t, "update");
}
# Map characters to control characters
ctrl_char(keycode: int): int
{
  case keycode {
    '@' to '_' =>
      return keycode - 64;
    'a' to 'z' =>
      return keycode - 96;
    * =>
      return 0;
  }
}

enable_move_key(t: ref Tk->Toplevel)
{
  if (!move_key_enabled) {
    tk->cmd(t, ".b" + string METAKEY + " configure -text {Move} -image Move_off");
    # move_key_enabled = 1;			# Don't enable Move key (it's not needed anymore)
    meta_active = 0;
  }
}

disable_move_key(t: ref Tk->Toplevel)
{
  if (move_key_enabled) {
    tk->cmd(t, ".b" + string METAKEY + " configure -text { } -image None");
    move_key_enabled = 0;
    meta_active = 0;
  }
}
