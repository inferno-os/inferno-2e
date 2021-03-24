###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1997 Lucent Technologies
###
### Written by N. W. Knauft
###

implement KeyIn;

include "sys.m";
        sys: Sys;

include "draw.m";
        draw: Draw;

include "tk.m";
        tk: Tk;

include "wmlib.m";
        wmlib: Wmlib;

include "string.m";
        str: String;

include "keyIn.m";

direction:= array[] of {"left", "right", "up", "down"};
row_dimensions:= array[] of {0, ROW1, ROW2, ROW3, ROW4, NKEYS};


special_keys:= array[] of {
  (DELKEY, DELSIZE),
  (TABKEY, TABSIZE),
  (BACKSLASHKEY, BACKSLASHSIZE),
  (CAPSLOCKKEY, CAPSLOCKSIZE),
  (RETURNKEY, RETURNSIZE),
  (LSHIFTKEY, LSHIFTSIZE),
  (RSHIFTKEY, RSHIFTSIZE),
  (ESCKEY, ESCSIZE),
  (CTRLKEY, CTRLSIZE),
  (METAKEY, METASIZE),
  (ALTKEY, ALTSIZE),
  (SPACEKEY, SPACESIZE),
  (ENTERKEY, ENTERSIZE),
};

f_key:= array[] of{
	"F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"};

keys:= array[] of {
  # Unshifted
    "`", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=", "Delete",
    "Tab", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]", "\\\\",
    "CapLoc", "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "\'", "Return",
    "Shift", "z", "x", "c", "v", "b", "n", "m", ",", ".", "/", "Shift",
    "Esc", "Ctrl", " ", "Alt", " ", "Enter", "<-", "->", "v", "^",
  # Shifted
    "~", "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+", "Delete",
    "Tab", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\\{", "\\}", "|",
    "CapLoc", "A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\"", "Return",
    "Shift", "Z", "X", "C", "V", "B", "N", "M", "<", ">", "?", "Shift",
    "Esc", "Ctrl", " ", "Alt", " ", "Enter", "<-", "->", "v", "^",
};

keyvals:= array[] of {
  # Unshifted
    '`', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\\',
    CAPSLOCK, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '\n',
    SHIFT, 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', SHIFT,
    27, CTRL, META, ALT, 32, '\n', LEFT, RIGHT, DOWN, UP,
  # Shifted
    '~', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '|',
    CAPSLOCK, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '\n',
    SHIFT, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', SHIFT,
    27, CTRL, META, ALT, 32, '\n', LEFT, RIGHT, DOWN, UP,
};


rowlayout := array[] of {
  "frame .f0",
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
  "frame .dummy5 -height " + string KEYGAP,
  "frame .dummy6 -height " + string (ENDGAP + 1),
};


# Move key flags
move_key_enabled := 0;
meta_active := 0;

# Create keyboard widget, spawn keystroke handler
initialize(t: ref Tk->Toplevel, ctxt : ref Draw->Context, dot: string,inputMethod:byte): chan of string
{
  sys = load Sys Sys->PATH;
  draw = load Draw Draw->PATH;
  tk = load Tk Tk->PATH;
  wmlib = load Wmlib Wmlib->PATH;
  str = load String String->PATH;

  wmlib->init();

  screenX:=ctxt.screen.image.r.max.x;
  screenY:=ctxt.screen.image.r.max.y;
  screenX-=360;
  screenY=screenY-180-30; # 30 for leave space for toolbar which is on sword

  tk->cmd(t,". configure -x "+string screenX+" -y "+string screenY);
  tk->cmd(t, "frame " + dot + " -bd 2 -relief raised -width " + string KBDWIDTH 
          + " -height " + string KBDHEIGHT);
  wmlib->tkcmds(t, rowlayout);

  i: int;



  
  for(i =0; i<12;i++)
    {    
    tk->cmd(t,"button .fk"+string i +" -text {"+f_key[i]+ "} -font "+SPECFONT+" -width "+KEYSIZE+" -height "+KEYSIZE+" -bd "+string KEYBORDER);
    tk->cmd(t,"pack .fk"+string i+" -in .f0 -side left");
    tk->cmd(t,"frame .f0.dummy_fk"+string i+" -width "+string KEYGAP);
    tk->cmd(t,"pack .f0.dummy_fk"+string i+" -side left");
    }

   
  tk->cmd(t,"button .f0.e_dummy -text { } -width "+KEYSIZE+" -height "+KEYSIZE+" -bd "+string KEYBORDER);
  tk->cmd(t,"pack .f0.e_dummy -side left");
  tk->cmd(t,"frame .f0.dummy_end -width "+string KEYGAP);
  tk->cmd(t,"pack .f0.dummy_end -side left");
 
  tk->cmd(t,"button .f0.last_b -text { } -width "+string DELSIZE+" -height "+KEYSIZE+" -bd "+string KEYBORDER);
  tk->cmd(t,"pack .f0.last_b -side left");

  for(i = 0; i < NKEYS; i++)
    tk->cmd(t, "button .b" + string i + " -text {" + keys[i] + "} -font " + FONT + " -width " + KEYSIZE + " -height " + KEYSIZE + " -bd " + string KEYBORDER + " -command 'send keypress " + string i);

  tk->cmd(t,".b0 configure -text "+" "); #this key no use

  keynum, keysize: int;
  for(i = 0; i < len special_keys; i++) {
    (keynum, keysize) = special_keys[i];
    tk->cmd(t, ".b" + string keynum + " configure -font " + SPECFONT + " -width " + string keysize);
  }

  rowstart, rowend: int;
  for(j:=1; j < len row_dimensions; j++) {
    rowstart = row_dimensions[j-1];
    rowend = row_dimensions[j];
    for(i=rowstart; i<rowend; i++) {
      if (i == rowstart) {
        tk->cmd(t, "frame .f" + string j + ".dummy -width " + string ENDGAP);
        
        tk->cmd(t, "pack .f" + string j + ".dummy -side left");
      }
      tk->cmd(t, "pack .b" + string i + " -in .f" + string j + " -side left");
      if (i == rowend-1)
        tk->cmd(t, "frame .f" + string j + ".dummy" + string i + " -width " + string ENDGAP);
      else
        tk->cmd(t, "frame .f" + string j + ".dummy" + string i + " -width " + string KEYGAP);
        
      tk->cmd(t, "pack .f" + string j + ".dummy" + string i + " -side left");
      
    }
  }

  tk->cmd(t, "pack .f0 .dummy0 .f1 .dummy1 .f2 .dummy2 .f3 .dummy3 .f4 .dummy4 .f5 .dummy5 -in " + dot);
  tk->cmd(t,"update");
  tk->cmd(t,"pack "+dot+" -fill both");
  tk->cmd(t,"update");

  case (int inputMethod){
	
	YINGSHU => fillYingshu(t);
	#ZHUYING  => fillZhuying(t);
	#CHANGJEI  => fillChangjei(t);
	NEIMA  => fillNeima(t);
	FUHAO => fillFuhao(t);
	SHOUXIE => fillShouxie(t);

	}
  key := chan of string;
  return key;
}

fillYingshu(t: ref Tk->Toplevel)

{

}

fillZhuying(t: ref Tk->Toplevel)
{
	tk->cmd(t,".b"+string QKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("624b"));
	tk->cmd(t,".b"+string KEY1+" configure -font "+UNICODEFONT+" -text '"+uni_s("3105"));
	tk->cmd(t,".b"+string KEY2+" configure -font "+UNICODEFONT+" -text '"+uni_s("3109"));
	tk->cmd(t,".b"+string KEY3+" configure -font "+UNICODEFONT+" -text '"+uni_s("3103"));
	tk->cmd(t,".b"+string KEY4+" configure -font "+UNICODEFONT+" -text '"+uni_s("3104"));
	tk->cmd(t,".b"+string KEY5+" configure -font "+UNICODEFONT+" -text '"+uni_s("3113"));
	tk->cmd(t,".b"+string KEY6+" configure -font "+UNICODEFONT+" -text '"+uni_s("3102"));
	tk->cmd(t,".b"+string KEY7+" configure -font "+UNICODEFONT+" -text '"+uni_s("3101"));
	tk->cmd(t,".b"+string KEY8+" configure -font "+UNICODEFONT+" -text '"+uni_s("311a"));
	tk->cmd(t,".b"+string KEY9+" configure -font "+UNICODEFONT+" -text '"+uni_s("311e"));
	tk->cmd(t,".b"+string KEY0+" configure -font "+UNICODEFONT+" -text '"+uni_s("3122"));
	tk->cmd(t,".b"+string MINUSKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3126"));

	tk->cmd(t,".b"+string QKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3106"));
	tk->cmd(t,".b"+string WKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("310a"));
	tk->cmd(t,".b"+string EKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("310d"));
	tk->cmd(t,".b"+string RKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3110"));
	tk->cmd(t,".b"+string TKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3114"));
	tk->cmd(t,".b"+string YKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3117"));
	tk->cmd(t,".b"+string UKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3127"));
	tk->cmd(t,".b"+string IKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("311b"));
	tk->cmd(t,".b"+string OKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("311f"));
	tk->cmd(t,".b"+string PKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3123"));

	tk->cmd(t,".b"+string AKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3107"));
	tk->cmd(t,".b"+string SKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("310b"));
	tk->cmd(t,".b"+string DKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("310e"));
	tk->cmd(t,".b"+string FKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3111"));
	tk->cmd(t,".b"+string GKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3115"));
	tk->cmd(t,".b"+string HKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3118"));
	tk->cmd(t,".b"+string JKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3128"));
	tk->cmd(t,".b"+string KKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("311c"));
	tk->cmd(t,".b"+string LKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3120"));
	tk->cmd(t,".b"+string SEMICOLONKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3124"));
	
	tk->cmd(t,".b"+string ZKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3108"));
	tk->cmd(t,".b"+string XKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("310c"));
	tk->cmd(t,".b"+string CKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("310f"));
	tk->cmd(t,".b"+string VKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3112"));
	tk->cmd(t,".b"+string BKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3116"));
	tk->cmd(t,".b"+string NKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3119"));
	tk->cmd(t,".b"+string MKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3129"));
	tk->cmd(t,".b"+string COMMAKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("311d"));
	tk->cmd(t,".b"+string PERIODKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3121"));
	tk->cmd(t,".b"+string SLASHKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3125"));
	tk->cmd(t,"update");
	
}



fillChangjei(t : ref Tk->Toplevel)
{
	tk->cmd(t,".b"+string QKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("624b"));
	tk->cmd(t,".b"+string WKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("7530"));
	tk->cmd(t,".b"+string EKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("6c34"));
	tk->cmd(t,".b"+string RKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("53e3"));
	tk->cmd(t,".b"+string TKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("5eff"));
	tk->cmd(t,".b"+string YKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("535c"));
	tk->cmd(t,".b"+string UKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("5c71"));
	tk->cmd(t,".b"+string IKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("6208"));
	tk->cmd(t,".b"+string OKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("4eba"));
	tk->cmd(t,".b"+string PKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("5fc3"));

	tk->cmd(t,".b"+string AKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("65e5"));
	tk->cmd(t,".b"+string SKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("5c38"));
	tk->cmd(t,".b"+string DKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("6728"));
	tk->cmd(t,".b"+string FKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("706b"));
	tk->cmd(t,".b"+string GKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("571f"));
	tk->cmd(t,".b"+string HKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("7af9"));
	tk->cmd(t,".b"+string JKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("5341"));
	tk->cmd(t,".b"+string KKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("5927"));
	tk->cmd(t,".b"+string LKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("4e2d"));
	
	tk->cmd(t,".b"+string ZKEY+" configure -text '"+"z");
	tk->cmd(t,".b"+string XKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("96e3"));
	tk->cmd(t,".b"+string CKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("91d1"));
	tk->cmd(t,".b"+string VKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("5973"));
	tk->cmd(t,".b"+string BKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("6708"));
	tk->cmd(t,".b"+string NKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("5f13"));
	tk->cmd(t,".b"+string MKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("4e00"));
}


fillNeima(t: ref Tk->Toplevel)

{
	#tk->cmd(t,".b"+string QKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string WKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string RKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string TKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string YKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string UKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string IKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string OKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string PKEY+" configure -text '"+ " ");

	#tk->cmd(t,".b"+string SKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string GKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string HKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string JKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string KKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string LKEY+" configure -text '"+ " ");
	
	#tk->cmd(t,".b"+string ZKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string XKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string VKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string NKEY+" configure -text '"+ " ");
	#tk->cmd(t,".b"+string MKEY+" configure -text '"+ " ");

}



fillFuhao(t: ref Tk->Toplevel)

{
	tk->cmd(t,".b"+string KEY1+" configure -font "+UNICODEFONT+" -text '"+uni_s("250c"));
	tk->cmd(t,".b"+string KEY2+" configure -font "+UNICODEFONT+" -text '"+uni_s("252c"));
	tk->cmd(t,".b"+string KEY3+" configure -font "+UNICODEFONT+" -text '"+uni_s("2510"));
	tk->cmd(t,".b"+string KEY4+" configure -font "+UNICODEFONT+" -text '"+uni_s("02b9"));
	tk->cmd(t,".b"+string KEY5+" configure -font "+UNICODEFONT+" -text '"+uni_s("02ba"));
	tk->cmd(t,".b"+string KEY6+" configure -font "+UNICODEFONT+" -text '"+uni_s("2018"));
	tk->cmd(t,".b"+string KEY7+" configure -font "+UNICODEFONT+" -text '"+uni_s("2019"));
	tk->cmd(t,".b"+string KEY8+" configure -font "+UNICODEFONT+" -text '"+uni_s("201c"));
	tk->cmd(t,".b"+string KEY9+" configure -font "+UNICODEFONT+" -text '"+uni_s("201d"));
	tk->cmd(t,".b"+string KEY0+" configure -font "+UNICODEFONT+" -text '"+uni_s("300e"));
	tk->cmd(t,".b"+string MINUSKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("300f"));
	tk->cmd(t,".b"+string PLUSKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("300c"));
	tk->cmd(t,".b"+string DELKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("300d"));
	

	tk->cmd(t,".b"+string QKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("251c"));
	tk->cmd(t,".b"+string WKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("253c"));
	tk->cmd(t,".b"+string EKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("2524"));
	tk->cmd(t,".b"+string RKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("203b"));
	tk->cmd(t,".b"+string TKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3008"));
	tk->cmd(t,".b"+string YKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3009"));
	tk->cmd(t,".b"+string UKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("300a"));
	tk->cmd(t,".b"+string IKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("300b"));
	tk->cmd(t,".b"+string OKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3010"));
	tk->cmd(t,".b"+string PKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3011"));
	tk->cmd(t,".b"+string LBRACKETKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("02c2"));
	tk->cmd(t,".b"+string RBRACKETKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("02c3"));
	tk->cmd(t,".b"+string BACKSLASHKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("02c4"));
	
	tk->cmd(t,".b"+string AKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("2514"));
	tk->cmd(t,".b"+string SKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("2534"));
	tk->cmd(t,".b"+string DKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("2518"));
	tk->cmd(t,".b"+string FKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("25cb"));
	tk->cmd(t,".b"+string GKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("25cf"));
	tk->cmd(t,".b"+string HKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("2191"));
	tk->cmd(t,".b"+string JKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("2193"));
	tk->cmd(t,".b"+string KKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("ff01"));
	tk->cmd(t,".b"+string LKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("ff1a"));
	tk->cmd(t,".b"+string SEMICOLONKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("ff1b"));
	tk->cmd(t,".b"+string QUOTEKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3001"));

	tk->cmd(t,".b"+string ZKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("2500"));
	tk->cmd(t,".b"+string XKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("2502"));
	tk->cmd(t,".b"+string CKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("25ce"));
	tk->cmd(t,".b"+string VKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("00a7"));
	tk->cmd(t,".b"+string BKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("2190"));
	tk->cmd(t,".b"+string NKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("2192"));
	tk->cmd(t,".b"+string MKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("3002"));
	tk->cmd(t,".b"+string COMMAKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("ff0c"));
	tk->cmd(t,".b"+string PERIODKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("ff0e"));
	tk->cmd(t,".b"+string SLASHKEY+" configure -font "+UNICODEFONT+" -text '"+uni_s("ff1f"));
	
}



fillShouxie(t: ref Tk->Toplevel)

{


}



uni_s(s:string):string

{

  (m,q):=str->toint(s,16);

  return sys->sprint("%c\n",m);

}


# Process key clicks and hand keycodes off to Tk

handle_keyclicks(t: ref Tk->Toplevel, ctxt : ref Draw->Context, ch: chan of string)
{
    keycode : int;
    keypress := chan of string;
    tk->namechan(t, keypress, "keypress");

  caps_locked := 0;
  shifted := 0;
  ctrl_active := 0;
  alt_active := 0;

    for (;;) alt {
	k := <-keypress =>
	    (n, cmdstr) := sys->tokenize(k, " \t\n");
        keycode = int hd cmdstr;
        case keycode {
	    CAPSLOCK => {
		redisplay_keyboard(t, caps_locked ^= 1, caps_locked);
		shifted = 0;
		ctrl_active = 0;
		alt_active = 0;
	    }
	    SHIFT => {
		redisplay_keyboard(t, (shifted ^= 1) ^ caps_locked, caps_locked);
	    }
	    CTRL => {
		ctrl_active ^= 1;
		if (shifted) {
		    redisplay_keyboard(t, caps_locked, caps_locked);
		    shifted = 0;
		}
		alt_active = 0;
	    }
	    ALT => {
		alt_active ^= 1;
		if (shifted) {
		    redisplay_keyboard(t, caps_locked, caps_locked);
		    shifted = 0;
		}
		ctrl_active = 0;
	    }
	    META => {
		if (move_key_enabled) {
  		  if (meta_active ^= 1)
    		    tk->cmd(t, ".b" + string METAKEY + " configure -image Move_on");
  		  else
    		    tk->cmd(t, ".b" + string METAKEY + " configure -image Move_off");
		}
		redisplay_keyboard(t, caps_locked, caps_locked);
		shifted = 0;
		ctrl_active = 0;
		alt_active = 0;
	    }
	    * => {
		if (ctrl_active) {
		    keycode = ctrl_char(keycode);
		    ctrl_active = 0;
		} else if (alt_active) {
		    keycode += MAGIC_PREFIX; #value: 256
		    alt_active = 0;
		}
		if (meta_active && LEFT <= keycode && keycode <= DOWN) {#LEFT:513,DOWN:516
		    spawn send_move_msg(direction[keycode - ARROW_PREFIX - 1], ch);#ARROW_PREFIX:512
		} else
		{
		    #---------------------------------
		    # this is another method:
		    # ch <-= hd cmdstr; 
		    #---------------------------------
		    tk->keyboard(ctxt.screen, keycode);
		}
		if (shifted) {
		    redisplay_keyboard(t, caps_locked, caps_locked);
		    shifted = 0;
		}
	    }
	}
    }
}

send_move_msg(dir: string, ch: chan of string)
{
  ch <-= dir;
}


# Redisplay keyboard to reflect current state (shifted or unshifted)
redisplay_keyboard(t: ref Tk->Toplevel, shifted, caps_locked: int)
{
  base, n: int;

  if (shifted)
    base = NKEYS; #NKEYS:63
  else
    base = 0;

  for(i:=0; i<NKEYS; i++) {
    n = base + i;
    tk->cmd(t, ".b" + string i + " configure -text {" + keys[n]
            + "} -command 'send keypress " + string i);
  }

  tk->cmd(t,".b0 configure -text { }"); #this key no use 
  if (caps_locked)
    tk->cmd(t, ".b" + string CAPSLOCKKEY + " configure -image Capslock_on");
  else
    tk->cmd(t, ".b" + string CAPSLOCKKEY + " configure -image Capslock_off");
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
    move_key_enabled = 1;
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
key_in( keyChan:chan of byte,key: byte)
{
        keyChan<-=key;
}

