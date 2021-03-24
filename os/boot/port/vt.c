/*  This file was taken from the ShagOS software distribution,
 *  and used for Inferno with the author's permission.
 *
 *  Complete, unaltered, original distributions of this and related code
 *  may be obtained from:   http://www.csh.rit.edu/~shaggy/software.html
 *  or:	ftp://ftp.csh.rit.edu/pub/csh/shaggy
 */

/* this file should be included from the code needing vt100/102/200 support,
   with the following macros defined before inclusion:
	VT_PUTCHAR(vt,x,y,ch)
	VT_SCROLL_UP(vt,x1,y1,x2,y2,n)
	VT_SCROLL_DOWN(vt,x1,x2,y2,n)
	VT_SCROLL_LEFT(vt,x1,x2,y2,n)
	VT_SCROLL_RIGHT(vt,x1,x2,y2,n)
	VT_CLEAR(vt,x1,y1,x2,y2)
	VT_SET_COLOR(vt)
	VT_SET_CURSOR(vt,x,y)
	VT_BEEP(vt)
	VT_TYPE(vt,b,n) function for simulated typing (for returning status)
	VT_WID		width in characters
	VT_HGT		height in characters
	VT_X		cursor X
	VT_Y		cursor Y
	VT_DX		delta X (in standard-sized characters)
	VT_DY		delta Y (in standard-sized characters)
	VT_NLCR		should evaluate to 1 if newlines imply carriage returns
	VTPARAM		implicit first parameter(s)
	VTPARAM_C	implicit first parameter(s) and a comma (if needed)
	VTARG		implicit first argument(s)
	VTARG_C		implicit first argument(s) and a comma (if needed)
*/

static int vt_call_csi(VTPARAM_C char ch);
static int vt_call_ncsi(VTPARAM_C char ch);
static int vt_param(VTPARAM_C int n, int def, int min, int max);


static void
vt_save_state(VTPARAM)
{
	VTS.save_x = VT_X;
	VTS.save_y = VT_Y;
	VTS.save_attr = VTS.attr;
	VTS.save_fg = VTS.fg;
	VTS.save_bg = VTS.bg;
	VTS.save_mode = VTS.mode;
	VTS.save_qmode = VTS.qmode;
}

static void
vt_restore_state(VTPARAM)
{
	VT_X = VTS.save_x;
	VT_Y = VTS.save_y;
	VTS.attr = VTS.save_attr;
	VTS.fg = VTS.save_fg;
	VTS.bg = VTS.save_bg;
	VTS.mode = VTS.save_mode;
	VTS.qmode = VTS.save_qmode;
	VT_SET_COLOR(VTARG);
}



/* expects VT_WID, VT_HGT and implementation
 * variables to be initialized first: */

static void
vt_init(VTPARAM)
{
	VTS.fg = 7;
	VTS.bg = 0;
	VTS.attr = 0;
	VTS.mode = 0;
	VTS.qmode = (1<<7);
	VTS.y1 = 0;
	VTS.y2 = VT_HGT-1;
	VT_X = 0;
	VT_Y = 0;
	VTS.esc = 0;
	VTS.pcount = 0;
	vt_save_state(VTARG);
	VT_SET_COLOR(VTARG);
}


static void
vt_checkscroll(VTPARAM_C char *bp, int len)
{
	int n;
	if (VT_Y == VTS.y2+1 || VT_Y >= VT_HGT) {
		n = 1;
		while(len-- > 0 && n < (VTS.y2-VTS.y1)) {
			int c = *bp++;
			if(c == '\033' || c > 126 || c < 0)
				break;
			if(c == '\n')
				n++;
		}
              	VT_Y = VTS.y2-n+1;
		VT_SCROLL_UP(VTARG,0,VTS.y1,VT_WID-1,VTS.y2,n);
       	} else if (VT_Y == VTS.y1-1) {
		VT_Y = VTS.y1;
		VT_SCROLL_DOWN(VTARG,0,VTS.y1,VT_WID-1,VTS.y2,1);
	} else if (VT_Y < 0)
		VT_Y = 0;
}

static void
vt_write(VTPARAM_C char *buf, int len)
{
	char ch;
	int check_scroll;
	int n;

        while(len--) {
	    check_scroll = 0;
            ch = *buf++;
	    switch(VTS.esc) {
	    case 1:
		if(ch == '[') {
			VTS.type = ch;
			VTS.esc++;
			VTS.value = 0;
			VTS.pcount = 0;
			VTS.ptype = 1;
			for(n=0; n<VT_MAXPARAM; n++)
				VTS.param[n] = 0;
		} else {
			check_scroll = vt_call_ncsi(VTARG_C ch);
			VTS.esc = 0;
		}	
		break;	
	    case 2:
		if(ch >= '0' && ch <= '9') 
			VTS.value=(VTS.value)*10+(ch-'0');
		else if(ch == '?')
			VTS.ptype = -1;
		else {
			VTS.param[VTS.pcount++] = VTS.value*VTS.ptype;
			if(ch == ';') {
				if(VTS.pcount >= VT_MAXPARAM)
					VTS.pcount = VT_MAXPARAM-1;
				VTS.value = 0;
			} else {
				check_scroll = vt_call_csi(VTARG_C ch);
				VTS.esc = 0;
			}
		}
		break;	
	    default:
		switch(ch) {
                case '\n':
                        VT_Y += VT_DY;
			check_scroll = 1;
			if(!VT_NLCR)
                        	break;
                case '\r':
                        VT_X = 0;
                        break;
                case '\b':
                        if (VT_X > 0)
                                VT_X -= VT_DX;
                        break;
                case '\t':
			n = (VT_X & ~7)+8;
			if(VTS.mode & 0x10)
				VT_SCROLL_RIGHT(VTARG, VT_X,VT_Y,
				  VT_WID-1,VT_Y, n - VT_X);
                        VT_X = n;
			if(VT_X > VT_WID) {
				VT_X = 0; 
				VT_Y++;
				check_scroll = 1;
			}
			break;
		case '\007':
			VT_BEEP(VTARG);
			break;
		case '\013':
			VT_X = 0;
			VT_Y = VTS.y1;
			break;
		case '\014':
			VT_CLEAR(VTARG,0,VTS.y1,VT_WID-1,VTS.y2);
			break;
		case '\033':
			VTS.esc++;
			break;
		case '\205':
			VT_X = 0;
		case '\204':
			VT_Y++;
			check_scroll = 1;
			break;
		case '\210':	/* XXX/ADD -- set a tabstop */
			break;	
		case '\215':
			VT_Y--;
			check_scroll = 1;
			break;
		case '\216':	/* XXX/ADD -- map G2 into GL for next char only */
		case '\217':	/* XXX/ADD -- map G3 into GL for next char */
		case '\220':	/* XXX/ADD -- device control string */
		case '\221':	/* XXX/ADD -- start of string - ignored */
		case '\222':	/* XXX/ADD -- device attribute request */
			break;
		case '\223':
			VTS.esc = 2;
			VTS.type = '[';
			VTS.esc++;
			VTS.value = 0;
			VTS.pcount = 0;
			VTS.ptype = 1;
			for(n=0; n<VT_MAXPARAM; n++)
				VTS.param[n] = 0;
                default:
			if(VTS.mode & 0x10)
				VT_SCROLL_RIGHT(VTARG,VT_X,VT_Y,
				  VT_WID-1,VT_Y,1);	
			if(ch<32 || ch >126)
				break;
			if(VTS.qmode & 0x8000) {
				if(VT_X >= VT_WID-1 && (VTS.qmode & 0x80)) {
					VT_X = 0;
					VT_Y += VT_DY;
					vt_checkscroll(VTARG_C buf, len);
				}
				VTS.qmode &= ~0x8000;
			}
			VT_PUTCHAR(VTARG,VT_X,VT_Y,ch);
                        if((VT_X += VT_DX) >= VT_WID) {
				VT_X = VT_WID-1; 
				VTS.qmode |= 0x8000;
                        }
                }
	    }
	    if(check_scroll)
		vt_checkscroll(VTARG_C buf, len); 
	    if(VT_X < 0)
		VT_X = 0;
	    else if(VT_X >= VT_WID)
		VT_X = VT_WID-1;
	    if(VT_Y < 0)
		VT_Y = 0;
	    else if(VT_Y >= VT_HGT)
		VT_Y = VT_HGT-1;
	}
	VT_SET_CURSOR(VTARG,VT_X, VT_Y);
}




static int vt_call_csi(VTPARAM_C char ch)
{
	register int i, n;
	switch(ch) {
	case 'A':
		VT_Y -= vt_param(VTARG_C 1,1,1,VT_HGT);
		break;
	case 'B':
		VT_Y += vt_param(VTARG_C 1,1,1,VT_HGT);
		break;
	case 'C':
		VT_X += vt_param(VTARG_C 1,1,1,VT_WID);
		break;
	case 'D':
		VT_X -= vt_param(VTARG_C 1,1,1,VT_WID);
		break;
	case 'f':
	case 'H':
		VT_Y = vt_param(VTARG_C 0,1,1,VT_HGT)-1;
		VT_X = vt_param(VTARG_C 1,1,1,VT_WID)-1;
		break;
	case 'J':
		switch(VTS.param[0]) {
		case 0: VT_CLEAR(VTARG,VT_X,VT_Y,VT_WID-1,VT_Y);
			VT_CLEAR(VTARG,0,VT_Y+1,VT_WID-1,VTS.y2); break;
		case 1: VT_CLEAR(VTARG,0,0,VT_WID-1,VT_Y-1); 
			VT_CLEAR(VTARG,0,VT_Y,VT_X,VT_Y); break;
		case 2: VT_CLEAR(VTARG,0,VTS.y1,VT_WID-1,VTS.y2);	break;
		}
		break;
	case 'K':
		switch(VTS.param[0]) {
		case 0: VT_CLEAR(VTARG,VT_X,VT_Y,VT_WID-1,VT_Y); break;
		case 1: VT_CLEAR(VTARG,0,VT_Y,VT_X,VT_Y); break;
		case 2: VT_CLEAR(VTARG,0,VT_Y,VT_WID-1,VT_Y); break;
		}
		break;
	case 'L':
		n = vt_param(VTARG_C 0,1,1,VT_HGT);
		VT_SCROLL_DOWN(VTARG,0,VT_Y,VT_WID-1,VTS.y2,n);	
		break;
	case 'M':
		n = vt_param(VTARG_C 0,1,1,VT_HGT);
		VT_SCROLL_UP(VTARG,0,VT_Y,VT_WID-1,VTS.y2,n);	
		break;
	case '@':
		n = vt_param(VTARG_C 0,1,1,VT_WID-1-VT_X);
		VT_SCROLL_RIGHT(VTARG,VT_X,VT_Y,VT_WID-1,VT_Y,n);	
		break;
	case 'P':
		n = vt_param(VTARG_C 0,1,1,VT_WID-1-VT_X);
		VT_SCROLL_LEFT(VTARG,VT_X,VT_Y,VT_WID-1,VT_Y,n);	
		break;
	case 'X':
		n = vt_param(VTARG_C 0,1,1,VT_WID-1-VT_X);
		VT_CLEAR(VTARG,VT_X,VT_Y,VT_X+n-1,VT_Y);
		break;
	case 'm':
		if(VTS.pcount == 0)
			VTS.pcount++;
		for(i=0; i<VTS.pcount; i++) {
			n = VTS.param[i];
			if(!n) {
				VTS.attr = 0; 
				VTS.fg = 7;
				VTS.bg = 0;
			} else if (n < 16)
				VTS.attr |= (1<<n);
			else if (n < 28)
				VTS.attr &= ~(1<<(n-20));
			else if (n < 38)
				VTS.fg = n-30;
			else if (n < 48)
				VTS.bg = n-40;
			else if (n < 58)
				VTS.fg = n-50+8;
			else if (n < 68)
				VTS.bg = n-60+8;
		}
		VT_SET_COLOR(VTARG);
		break;
	case 'c':
		if(VT_WID >= 132)
			VT_TYPE(VTARG,"\033[?61;1;6c", 10);
		else
			VT_TYPE(VTARG,"\033[?61;6c", 8);
		break;
	case 'n': 
		n = vt_param(VTARG_C 0,0,0,9);
		switch(n) {
		case 5:
			VT_TYPE(VTARG,"\033[0n", 4);
		case 6:
			{ char buf[20];
			  int n = sprintf(buf, "\033[%d;%dR",VT_Y+1,VT_X+1);
			  VT_TYPE(VTARG,buf, n);
			}
			break;
		default:
			break;
		}
		break; 
	case 'r':
		VTS.y1 = vt_param(VTARG_C 0,1,1,VT_HGT)-1;
		VTS.y2 = vt_param(VTARG_C 1,VT_HGT,1,VT_HGT)-1;
		break;
	case 's':
		vt_save_state(VTARG);
		break;
	case 'u':
		vt_restore_state(VTARG);
		break;
	case 'h':
		for(i=0; i<VTS.pcount; i++) {
			n = VTS.param[i];
			if(n >= 0)
				VTS.mode |= (1<<n);
			else
				VTS.qmode |= (1<<(-n));
		}
		break;
	case 'l':
		for(i=0; i<VTS.pcount; i++) {
			n = VTS.param[i];
			if(n >= 0)
				VTS.mode &= ~(1<<n);
			else
				VTS.qmode &= ~(1<<(-n));
		}
		break;
	}
	return 0;
}

static int vt_call_ncsi(VTPARAM_C char ch)
{
	switch(ch) {
	case 'E':
		VT_X = 0;
	case '9':
	case 'D':
		VT_Y++;
		return 1;
	case 'H':	/* XXX/ADD -- horizontal tab set */
		break;
	case '6':
	case 'M':
		VT_Y--;
		return 1;
	case '7':
		vt_save_state(VTARG_C);
		break;
	case '8':
		vt_restore_state(VTARG_C);
		break;
	case '=':
		break;
	case '>':
		break;
	case '#':
		break;
	case '(':
		break;
	case ')':
		break;
	}
	return 0;
}


static int vt_param(VTPARAM_C int n, int def, int min, int max)
{
	register int param;
	param = VTS.param[n];
	if(param == 0)
		param = def;
	if(param < min)
		param = min;
	if(param > max)
		param = max;
	return param;
}

