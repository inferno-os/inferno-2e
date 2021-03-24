###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1997 Lucent Technologies
###
### Written by N. W. Knauft
###

KeyIn: module
{
        PATH:   con "/dis/lib/keyIn.dis";
ICPATH: con "/dis/wm/mailtool/shannon/";

#Font
FONT: con "/fonts/lucidasans/latin1.7.font";
#SPECFONT: con "/fonts/lucidasans/unicode.6.font";
SPECFONT: con "/fonts/lucidasans/unicode.6.font";
UNICODEFONT : con "/fonts/pelm/unicode.9.font";

# Dimension constants
KBDWIDTH: con 360;
KBDHEIGHT: con 120;
KEYSIZE: con "19";
KEYSPACE: con 5;
KEYBORDER: con 1;
KEYGAP: con KEYSPACE - (2 * KEYBORDER);
ENDGAP: con 2 - KEYBORDER;


TILDEKEY : con 0;
KEY1 : con 1;
KEY2 : con 2;
KEY3 : con 3;
KEY4 : con 4;
KEY5 : con 5;
KEY6 : con 6;
KEY7 : con 7;
KEY8 : con 8;
KEY9 : con 9;
KEY0 : con 10;
MINUSKEY : con 11;
PLUSKEY : con 12;
DELKEY : con 13;

TABKEY : con 14;
QKEY : con 15;
WKEY : con 16;
EKEY : con 17;
RKEY : con 18;
TKEY : con 19;
YKEY : con 20;
UKEY : con 21;
IKEY : con 22;
OKEY : con 23;
PKEY : con 24;
LBRACKETKEY : con 25;
RBRACKETKEY : con 26;
BACKSLASHKEY : con 27;

CAPSLOCKKEY : con 28;
AKEY : con 29;
SKEY : con 30;
DKEY : con 31;
FKEY : con 32;
GKEY : con 33;
HKEY : con 34;
JKEY : con 35;
KKEY : con 36;
LKEY : con 37;
SEMICOLONKEY : con 38;
QUOTEKEY: con 39;
RETURNKEY: con 40;

LSHIFTKEY : con 41;
ZKEY : con 42;
XKEY : con 43;
CKEY : con 44;
VKEY : con 45;
BKEY : con 46;
NKEY : con 47;
MKEY : con 48;
COMMAKEY : con 49;
PERIODKEY : con 50;
SLASHKEY : con 51;
RSHIFTKEY: con 52;

ESCKEY : con 53;
CTRLKEY : con 54;
METAKEY : con 55;
ALTKEY : con 56;
SPACEKEY : con 57;
ENTERKEY : con 58;
LEFTKEY : con 59;
RIGHTKEY : con 60;
DOWNKEY : con 61;
UPKEY : con 62;


#Special key code constants
CAPSLOCK: con -1 ;
SHIFT: con -2;
CTRL: con -3;
ALT: con -4;
META: con -5;
MAGIC_PREFIX: con 256;
ARROW_PREFIX: con 512;

#Special key width constants
DELSIZE: con 44;
TABSIZE: con 32;
BACKSLASHSIZE: con 31;
CAPSLOCKSIZE: con 44;
RETURNSIZE: con 43;
LSHIFTSIZE: con 56;
RSHIFTSIZE: con 55;
ESCSIZE: con 21;
CTRLSIZE: con 23;
METASIZE: con 38;
ALTSIZE: con 22;
SPACESIZE: con 100;
ENTERSIZE: con 31;

#Arrow key code constants
LEFT: con ARROW_PREFIX + 1;
RIGHT: con ARROW_PREFIX + 2;
UP: con ARROW_PREFIX + 3;
DOWN: con ARROW_PREFIX + 4;

YINGSHU :con 0;
ZHUYING : con 1;
CHANGJEI :con 2;
NEIMA : con 3;
FUHAO: con 4;
SHOUXIE : con 5;

ROW1: con 14;
ROW2: con 28;
ROW3: con 41;
ROW4: con 53;
NKEYS: con 63;



        initialize:     fn(t: ref Tk->Toplevel, ctxt : ref Draw->Context,dot: string,inputMethod: byte): chan of string;
	redisplay_keyboard:  fn(t:ref Tk->Toplevel,shifted:int,caps_locked:int);
        key_in :fn(keyChan:chan of byte,key:byte);
	
        enable_move_key:        fn(t: ref Tk->Toplevel);
        disable_move_key:       fn(t: ref Tk->Toplevel);

};
