implement Out_input;

include "sys.m";
	sys: Sys;
	Dir: import sys;

include "draw.m";
	draw: Draw;
	Screen, Display, Image: import draw;

include "keyIn.m";
include "tk.m";
	tk: Tk;

include "wmlib.m";
	wmlib: Wmlib;
include "bufio.m";
        bufio: Bufio;
        Iobuf: import bufio;

include "out_input.m";

UNICODE,ENGLISH,PINYIN: con iota;
NO_CHAR:	con -1;
ENTER:	con 16r0a;
SPACE:	con 16r20;
BACKSPACE:	con 16r08;
TITLE:	con "Input platform Demo 1.1";

MAX_IN:	con 7;
MAX_OUT:    con 10;
MAX_RESULT: con 512;
result_buf:=array[MAX_RESULT] of {* => int 0};
result_index:=0;
result_len : int;
#backspace_flag:=0;

tc:			ref tk->Toplevel;
input_buf :=	array[MAX_IN] of {* => int 0}; #store unicode
search_buf :=	array[MAX_OUT+1] of {* => int 0};		# Unicode value, maxium 10
keyIO: ref Sys->FileIO;   #for transfer keyvalue back to application
key_buf := array[MAX_OUT*3] of {* => byte 0};
key_count := 0;
key_req:=0;

Rdreq: adt
{
        off:    int;
        nbytes: int;
        fid:    int;
        rc:     chan of (array of byte, string);
};
rdreq: Rdreq;

search_str:		string;
input_num:		int;
search_num:		int;

keyChan: chan of byte;
stderr:		ref sys->FD;

index: adt {
        yin:    string;
        code:   string;
};
CH_table := array[450] of index;

Prog: adt
{
	pid:	int;
	size:	int;
	state:	string;
	mod:	string;
};

pform := array[] of {

	"frame .f1",
	"button .f1.l1 -text { } -width 100 -anchor w",
	"menubutton .f1.bmeth -relief raised -text {English} -menu .f1.bmeth.menu",
	"menu .f1.bmeth.menu",
	".f1.bmeth.menu add command -label Unicode  -command {send cmd Unicode}",
	".f1.bmeth.menu add command -label English  -command {send cmd English}",
	".f1.bmeth.menu add command -label Pinyin -command {send cmd Pinyin}",
	"button .f1.b0 -text {   } -command {send selection 0}",
	"button .f1.b1 -text {   } -command {send selection 1}",
	"button .f1.b2 -text {   } -command {send selection 2}",
	"button .f1.b3 -text {   } -command {send selection 3}",
	"button .f1.b4 -text {   } -command {send selection 4}",
	"button .f1.b5 -text {   } -command {send selection 5}",
	"button .f1.b6 -text {   } -command {send selection 6}",
	"button .f1.b7 -text {   } -command {send selection 7}",
	"button .f1.b8 -text {   } -command {send selection 8}",
	"button .f1.b9 -text {   } -command {send selection 9}",
	"button .f1.b10 -text {<} -command {send selection left}",
	"button .f1.b11 -text {>} -command {send selection right}",
	"pack .f1.b10 .f1.b0 .f1.b1 .f1.b2 .f1.b3 .f1.b4 .f1.b5 .f1.b6 .f1.b7 .f1.b8 .f1.b9 .f1.b11 .f1.bmeth .f1.l1 -side left",
	"pack .f1 -side top -fill x",
};

init(ctxt: ref Draw->Context, argv: list of string)
{
	sys  = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk   = load Tk Tk->PATH;
	wmlib = load Wmlib Wmlib->PATH;
 	wmlib->init();


	stderr = sys->fildes( 2 );

	sys->bind("#s","/chan",sys->MBEFORE);
        keyIO=sys->file2chan("/chan","keyIO");
	if(keyIO == nil) {
		sys->print("error to bind keyIO\n");
                return;
        }
	kchan := chan of int;
	dfd:=sys->open("/dev/keyboard",sys->OREAD);
	if(dfd==nil) return;
	spawn getkey( kchan,dfd);
	spawn mainloop(ctxt,kchan);
}

mainloop(ctxt:ref Draw->Context,kchan: chan of int)
{
	help_txt: list of string = list of { "help.txt", "help.txt" };

	cmd := chan of string; 
	tc=tk->toplevel(ctxt.screen,"");
	#tc_chan:chan of string;
	#( tc, tc_chan ) = wmlib->titlebar( ctxt.screen, "", "Help", Wmlib->Appl);
	tk->namechan(tc, cmd, "cmd" );
	tkcmds(tc, pform);
	tk->cmd(tc, "update");

      buf := array[10] of byte;
	input := ENGLISH;
	uchar: int;
	input_num =0;
	search_num =0;

	keyChan=chan of byte;  #key in 

	selection:=chan of string;
	tk->namechan(tc,selection,"selection");

	killChild:=chan of string;

	out_input_set( "English" );
	pinyin_install();

	for(;;){
		alt{
			rdreq=<-keyIO.read=>
				if(rdreq.rc == nil) break;
			if(key_count>0) {
				rdreq.rc<-=(key_buf[0:key_count],nil); #array of byte,string
				key_req=0;
				key_count=0;
			}
			else key_req=1;
			
			s:=<-selection=>
				if((s=="right")||(s=="left"))
					handle_page(s);
				#{
				#sys->print("result_len:%d,index:%d\n",result_len,result_index);
				#	if((result_buf[0]!=0)&&(result_len>(result_index+1)*10))
				#	{
				#		result_index++;	
				#		if(result_len>((result_index+1)*10))
				#			dest:=10;
				#		else dest=result_len-result_index*10;
				#		for(i:=0;i<dest;i++)
				#			{
				#			search_buf[i]=result_buf[i+result_index*10];
				#			}
				#		search_num=i;
				#		search_buf[i]=0;
				#		redisplay();	
				#	}
				#}
				#else if(s=="left")
				#{
				#	if(result_index>0)
				#	{
				#	result_index--;
				#	for(i:=0;i<10;i++)
				#		{
				#		search_buf[i]=result_buf[i+result_index*10];	
				#		}
				#	search_num=10;
				#	redisplay();
				#	}
				#}
				else{
				if(s[0]-'0'>=search_num) break;
				uchar=search_buf[s[0]-'0'];
				#sys->char2byte(uchar,buf,0);
				key_count+=sys->char2byte(uchar,key_buf,key_count);
				#sys->write(wpfd,buf,4);
				if(key_req) {rdreq.rc<-=(key_buf[0:key_count],nil);
					     key_req=0;
						key_count=0;
						};
				buf_clear();
				}
			key :=<- kchan =>
				if(input==ENGLISH || key==ENTER || ( key== BACKSPACE && input_num==0 )){
					key_count+=sys->char2byte( key, key_buf, key_count );
					#sys->write(wpfd, buf, 4 );
					if(key_req) {rdreq.rc<-=(key_buf[0:key_count],nil);
						     key_req=0;
							key_count=0;
							uchar=NO_CHAR;
						};
					break;
				}
					
				if( input == UNICODE )
					uchar = unicode_input( key );
				if (input ==PINYIN)
					uchar = pinyin_input(key);
				
				if( uchar != NO_CHAR ){
					key_count+=sys->char2byte( uchar, key_buf, key_count );
					#sys->write(wpfd, buf, 4 );
					if(key_req) {rdreq.rc<-=(key_buf[0:key_count],nil);
						     key_req=0;
							key_count=0;
						};

					buf_clear();
				}
		
			s := <- cmd =>
		  	  (n, l) := sys->tokenize(s, " \t");
		  	  case hd l {
				"English" =>
					input = ENGLISH;
					out_input_set("English");
				"Unicode" =>
					input = UNICODE;
					out_input_set("Unicode" );	
				"Pinyin" =>
					input=PINYIN;
					out_input_set("Pinyin");
				"Help" =>
					spawn help( ctxt,help_txt );
				"exit" =>
					input=ENGLISH;
					out_input_set("English");
					exit; #kill itself
			  }

		}
	}
}

getkey( kchan: chan of int ,dfd:ref Sys->FD)
{
	#mypid := sys->pctl( 0, nil );
	#pidchan <-= mypid;

	#dfd:=sys->open("/dev/keyboard",sys->OREAD);
	#if(dfd==nil) return;
	i:=0;
	buf := array[10] of byte;
	for(;;){
		n:=sys->read(dfd,buf[i:],len buf-i);
		if(n<1) break;
		i+=n;
		while(i>0 && (nutf:=sys->utfbytes(buf,i))>0){
			s:=string buf[0:nutf];
			for(j:=0;j<len s;j++)
				kchan<-=int s[j];
			buf[0:]=buf[nutf:i];
			i-=nutf;
		}
		} #for
		#sys->read(rpfd, buf, 6);
		#s := string buf[0:6];
		#kchan <-= int s;
	#}
}

tkcmds(top: ref Tk->Toplevel, a: array of string)
{
	n := len a;
	for(i := 0; i < n; i++)
		v := tk->cmd(top, a[i]);
}

out_input_set(  input: string )
{
	buf_clear();

	notice( input );
}

notice( input: string )
{
	tk->cmd(tc,".f1.bmeth configure -text {"+input+"}");
	tk->cmd( tc, "update");
}


#########################
##    Unicode Input    ##
#########################

UNI_keychars: con "0123456789abcdef ";

unicode_input( key: int ): int
{
	uchar : int;

	if( !oneof(UNI_keychars, key) && key != BACKSPACE)
		return key;

	if( key == SPACE )
		uchar = unicode_space();

	if( key == BACKSPACE )
		uchar = buf_clear();
	
	if( key != SPACE && key != BACKSPACE )
		uchar = unicode_in( key );

	return uchar;
	
}

PIN_keychars : con " 0123456789abcdefghijklmnopqrstuvwxyz.,";
pinyin_input( key: int ): int
{
	uchar : int;

	if( !oneof(PIN_keychars, key) && key != BACKSPACE)
		return key;

	if((key==16r2e) && (search_num!=0))
		{
		handle_page("right");
		return NO_CHAR;
		}
	else if ((key==16r2c) && (search_num!=0))
		{
		handle_page("left");
		return NO_CHAR;
		}
	if ((key==16r2e) || (key==16r2c))
		return key;
	if(key==SPACE && search_num==0)
		return key;
	if(( key == BACKSPACE ) && (input_num==0))
			return key;
	else if(key==BACKSPACE)
		{
		search_buf[0]=0;  #recontruct output
		result_index=0;
		}
		
	else if (input_num==MAX_IN)
		return NO_CHAR;
	
        if( key<= '9' && key>= '0'){
                if( search_num == 0 ||(key -'0') >= search_num )
                        return NO_CHAR;
                return search_buf[key- '0'];
        }

	if(key==SPACE) return search_buf[0];

	uchar=pinyin_in( key );

	return uchar;
	
}

pinyin_in(key: int):int
{
	if(key==BACKSPACE)
		input_num=input_num-1;
	else{
		input_buf[input_num]=key;
		input_num++;
		}
	inbuf:=array[10] of byte;
	for(k:=0;k<input_num;k++)
		inbuf[k]=byte input_buf[k];

	input_str:=string inbuf[0:input_num];
	for(i:=0;i<450;i++)
		if(input_str==CH_table[i].yin)
			break;

	if(i!=450){
		result_len=len CH_table[i].code;
		j:int;
		if(result_len>10)
			{
			for(j=0;j<result_len;j++)
				result_buf[j]=CH_table[i].code[j];
			for(j=0;j<10;j++)
				search_buf[j]=result_buf[j];
			}
		else{
			result_buf[0]=0;
			for(j=0;j<result_len;j++)
				search_buf[j]=CH_table[i].code[j];	

		}
		search_num=j;
		search_buf[j]=0;
	}
	else  search_num=0;
	redisplay();
	return NO_CHAR;
}

unicode_space(): int
{
	if( input_num < 4 )
		ret := NO_CHAR;

	if( input_num == 4 ){
		#buf_clear();
		ret = search_buf[0];
	}
	
	return ret;	
}

unicode_in( key: int ): int
{
	buf := array[10] of byte;
	buf2 := array[10] of int;

	if( input_num == 4 )
		return NO_CHAR;

	input_num += 1;
	input_buf[input_num-1] =  key;
	input_buf[input_num] =  0;;

	if( input_num == 4 ){
		search_num = 1;
		for( i:=0; i<4; i++ ){
			t := int input_buf[i];
			if( t>='0' && t<='9' )
				buf2[i] = t - '0';
			else buf2[i] = t - 'a' + 10;
		}
		
		uchar := buf2[0]* 16r1000 + buf2[1]*16r100 +
			buf2[2]*16r10 + buf2[3];
		n := sys->char2byte( uchar, buf, 0 );
		search_str = string buf[0:n];
		search_buf[0] = uchar;
	}

	redisplay();
	return NO_CHAR;
}

## public functions ##
## buf_clear()
## redisplay()

buf_clear(): int
{
	input_num = 0;
	search_num =0;
	search_buf[0]=0;
	search_str = " ";
	result_index=0;
	redisplay();
	return NO_CHAR;
}

redisplay()
{
	num:=len input_buf;
	utfBuf:=array[3*2*num] of {* => byte 0};
	j:=0;
	skip: int;

	for(i:=0;i<input_num;i++)
	{
	if(input_buf[i]==0) break;
	skip=sys->char2byte(input_buf[i],utfBuf,j);

	j=j+skip;
	}
	
	if(j==0) tk->cmd(tc,".f1.l1 configure -text { }");
	else
	tk->cmd( tc, ".f1.l1 configure -text {"+ string utfBuf[0:j] +"}" );
	tk->cmd(tc,"update");

	num=len search_buf;
	resultUtf:=array[3*2*num] of {* => byte 0};
	j=0;

	#for(i=0;i<num;i++)
	for(i=0;i<search_num;i++)
	{
	if(search_buf[i]==0) break;
		resultUtf[j]=byte (i+'0');
	j++;
	skip=sys->char2byte(search_buf[i],resultUtf,j);

	oneByteBuf:=array[1] of byte;
	oneByteBuf[0]=byte(i+'0');
	tk->cmd(tc,".f1.b"+string oneByteBuf+" configure -text {"+string oneByteBuf+string resultUtf[j:j+skip]+"}"); # -command {send selection "+string oneByteBuf+"}");

	j=j+skip;
	}
	#for(j=i;j<10;j++)
	for(j=search_num;j<10;j++)
		tk->cmd(tc,".f1.b"+string j+" configure -text {   }");

	tk->cmd(tc,"update");
}

oneof(s: string, c: int): int
{
	for(i:=0; i<len s; i++)
		if(s[i] == c)
			return 1;
	return 0;
}


key_in(keyChan:chan of byte, key: byte)
{
	keyChan<-=key;		
}

#########################
#    for help  modue    #
#########################

ed: ref Tk->Toplevel;
screen: ref Screen;

ed_config := array[] of {
	"frame .m -relief raised -bd 2",
	"frame .b",
	"label .m.filename",
	"text .b.t -width 12c -height 7c " + 
		"-state disabled -yscrollcommand {.b.s set}",
	"scrollbar .b.s -command {.b.t yview}",
	"pack .b.s -fill y -side left",
	"pack .b.t -fill both -expand 1",
	"pack .Wm_t -fill x",
	"pack .b -fill both -expand 1",
	"focus .b.t",
	"pack propagate . 0",
	"update",
};

curfile := "";
snarf := "";
searchfor := "";
path := ".";

help(ctxt: ref Draw->Context, argv: list of string)
{

	tkargs := "";
	argv = tl argv;
	if(argv != nil) {
		tkargs = hd argv;
		argv = tl argv;
	}
	screen = ctxt.screen;
	wmctl := chan of string;

	( ed, wmctl ) = wmlib->titlebar( screen, "", "Help", Wmlib->Appl);

	c := chan of string;
	tk->namechan(ed, c, "c");
	drag := chan of string;
	tk->namechan(ed, drag, "Wm_drag");
	tkcmds(ed, ed_config);

	e := tk->cmd(ed, "variable lasterror");
	if(e != "") {
		return;
	}

	loadtfile( tkargs );	
	tk->cmd(ed, "update");

cmdloop: for(;;) {
		alt {
		s := <-wmctl =>
			if(s == "exit")
				break cmdloop;
			wmlib->titlectl(ed, s);
		}
		tk->cmd(ed, "update");
		e = tk->cmd(ed, "variable lasterror");
		if(e != "") {
			break cmdloop;
		}
	}

	tk->cmd(ed, "destroy ." );
}

loadtfile(path: string): string
{
	fd := sys->open(path, sys->OREAD);
	if(fd == nil)
		return "Can't open "+path+", the error was:\n"+sys->sprint("%r");
	(ok, d) := sys->fstat(fd);
	if(ok < 0)
		return "Can't stat "+path+", the error was:\n"+sys->sprint("%r");
	if(d.mode & sys->CHDIR)
		return path+" is a directory";

	tk->cmd(ed, "cursor -bitmap cursor.wait");
	BLEN: con 8192;
	buf := array[BLEN+Sys->UTFmax] of byte;
	inset := 0;
	for(;;) {
		n := sys->read(fd, buf[inset:], BLEN);
		if(n <= 0)
			break;
		n += inset;
		nutf := sys->utfbytes(buf, n);
		s := string buf[0:nutf];
		# move any partial rune to beginning of buffer
		inset = n-nutf;
		buf[0:] = buf[nutf:n];
		tk->cmd(ed, ".b.t insert end '" + s);
	}
	curfile = path;
	newcurfile();
	tk->cmd(ed, "cursor -default");
	return "";
}

newcurfile()
{
	tk->cmd(ed, ".m.filename configure -text '" + curfile);
}

pinyin_install()
{
	bufio=load Bufio Bufio->PATH;
	iofd := bufio->open( "/dis/lib/Upinyin.dat", bufio->OREAD );
	if(iofd==nil)
		{sys->print("Upinyin.dat:%r\n");
		exit;
		}

        #iofd := sys->open( "/dis/lib/Upinyin.dat", sys->OREAD );
        k := -1;
        for( i:=0;;i++ ){
                t := zgets( iofd );
		#t:=iofd.gets(16r0a);
                if( t==nil ) break;

                for( j:=0; j<len t -1; j++ )
                        if( t[j]<= 'z' && t[j+1] > 'z' ) break;

                yin :=  t[0:j+1];
                code :=  t[j+1:];
                CH_table[i].yin = yin;
                CH_table[i].code = code[0:len code-1];
        }
	iofd.close();
}

zgets( iofd: ref Iobuf ):string 
{
        s:string; 

        for( i:=0;;i++){
                c := iofd.getc();
                if ( c<0 ) return nil;
                s[i] = c;
                if ( c == 16r0a ) break;
        }

        return s;
}

handle_page(s:string)
{

	if(s=="right")
        {
	sys->print("result_len:%d,index:%d\n",result_len,result_index);
	if((result_buf[0]!=0)&&(result_len>(result_index+1)*10))
		{
		result_index++;
		if(result_len>((result_index+1)*10))
			dest:=10;
		else dest=result_len-result_index*10;
		for(i:=0;i<dest;i++)
		{
		search_buf[i]=result_buf[i+result_index*10];
		}
		search_num=i;
		search_buf[i]=0;
		redisplay();
		}
	}
	else if(s=="left")
	{
	if(result_index>0)
		{
		result_index--;
		for(i:=0;i<10;i++)
		{
		search_buf[i]=result_buf[i+result_index*10];
		}
		search_num=10;
		redisplay();
		}
	}

}
