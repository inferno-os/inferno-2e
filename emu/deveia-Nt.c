/*
 *  Win95/NT serial driver
 */

#include	<windows.h>
#undef	Sleep
#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	"styx.h"
#include	<stdio.h>
#include	<lm.H>
#include	<direct.h>

// local fcts
static void openport(int);
static void wrctl(int, char*);
static long rdstat(int, void*, long, ulong );

enum
{
	Devchar = 't',

	Ndataqid = 1,
	Nctlqid,
	Nstatqid,
	Nqid = 3,		/* number of QIDs */

	CTLS=	023,
	CTLQ=	021,

	Maxctl = 128,
	Maxfield = 32,

	// in/out buffer sizes for comm port (NT requires an even number)
	// set it to x* the max styx message rounded up to the
	// nearest 4 byte value
	CommBufSize = ((((MAXFDATA+MAXMSG)*2)+3) & ~3)
};

/*
 *  Macros to manage QIDs
 */
#define NETTYPE(x)	((x)&0x0F)
#define NETID(x)	(((x)&~CHDIR)>>4)
#define NETQID(i,t)	(((i)<<4)|(t))

static Dirtab *eiadir;
static int ndir;

static char Devname[] = "eia";

typedef struct Eia Eia;
struct Eia {
	Ref	        r;
	HANDLE      hCom;          //handle to open port
	int		    restore;       //flag to restore prev. states 
	DCB	        dcb;           //win32 device control block used for restore
	int         id;            //index to host port name in sysdev
};

// the same timeouts are used for all ports
// currently there is no Inferno interface to
// change the timeouts.
COMMTIMEOUTS  timeouts;  
                   
// std win32 serial port names are COM1..COM4
// however there can be more and they can be
// named anything. we should be more flexible
// pehaps adding a ctl command to allow you to
// access any win32 comm port
static char* sysdev[] = {
	"COM1:",
	"COM2:",
	"COM3:",
	"COM4:",
	NULL
};
    
static Eia *eia;

struct OptTable {
	char   *str;
	DWORD  flag;
};

#define BAD ((DWORD)-1)

// valid bit-per-byte sizes
static struct OptTable size[] = {
	{"5",	5},
	{"6",	6},
	{"7",	7},
	{"8",	8},
	{NULL,  BAD}
};

// valid stop bits
static struct OptTable stopbits[] = {
	{"1",    ONESTOPBIT},
	{"1.5",  ONE5STOPBITS},
	{"2",    TWOSTOPBITS},
	{NULL,   BAD}
};

// valid parity settings
static struct OptTable parity[] = {
	{"o",    ODDPARITY},
	{"e",    EVENPARITY},
	{"s",    SPACEPARITY},
	{"m",    MARKPARITY},
	{"n",    NOPARITY},
	{NULL,   NOPARITY}
};


static char *
ftos(struct OptTable *tbl, DWORD flag)
{
	while(tbl->str && tbl->flag != flag)
		tbl++;
	if(tbl->str == 0)
		return "unknown";
	return tbl->str;
}

static DWORD
stof(struct OptTable *tbl, char *str)
{
	while(tbl->str && strcmp(tbl->str, str) != 0)
		tbl++;
	return tbl->flag;
}


void
eiainit(void)
{
	int     i,x;
	byte    ports;   //bitmask of active host ports
	int     nports;  //number of active host ports
	int     max;     //number of highest port
	Dirtab *dp;

	// setup the timeouts; choose carefully
	timeouts.ReadIntervalTimeout         = 2;
	timeouts.ReadTotalTimeoutMultiplier  = 0;
	timeouts.ReadTotalTimeoutConstant    = 200;
	timeouts.WriteTotalTimeoutMultiplier = 0;
	timeouts.WriteTotalTimeoutConstant   = 400;

	// check to see which ports exist by trying to open them
	// keep results in a bitmask
	ports = nports = max = 0;
	for (i=0; (sysdev[i] != NULL) && (i<8); i++) {
		HANDLE hCom = CreateFile(sysdev[i], 0, 0, NULL,	/* no security attrs */
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (hCom != INVALID_HANDLE_VALUE) {
			ports |= 1<<i;   
			CloseHandle(hCom);
			nports++;
			max = i;
		}
	}

	if (nports == 0)
		return;  //no ports

	// allocate directory table and eia structure
	// for each active port.
	ndir = Nqid*nports;
	dp = eiadir = calloc(ndir, sizeof(Dirtab));
	if(dp == 0)
		panic("eiainit");
	eia = calloc(nports, sizeof(Eia));
	if(eia == 0)
	{
		free(dp);
		panic("eiainit");
	}

	// fill in the directory table and initialize
	// the eia strucutre.  skip inactive ports.
	x = 0;  //inferno id#
	for(i = 0; i <= max; i++) {
		if ( (ports & (1<<i)) == 0)
			continue;  //port 'i' is not active
		sprint(dp->name, "%s%d", Devname, i);
		dp->qid.path = NETQID(x, Ndataqid);
		dp->perm = 0660;
		dp++;
		sprint(dp->name, "%s%dctl", Devname, i);
		dp->qid.path = NETQID(x, Nctlqid);
		dp->perm = 0660;
		dp++;
		sprint(dp->name, "%s%dstat", Devname, i);
		dp->qid.path = NETQID(x, Nstatqid);
		dp->perm = 0660;
		dp++;
		// init the eia structure
		eia[x].restore = 0;
		eia[x].id      = i;
		x++; //next inferno id#
	}
}

Chan*
eiaattach(void *spec)
{
	if (!eiadir)
		error(Enodev);

	return devattach(Devchar, (char *)spec);
}

int
eiawalk(Chan *c, char *name)
{
	return devwalk(c, name, eiadir, ndir, devgen);
}

void
eiastat(Chan *c, char *db)
{
	devstat(c, db, eiadir, ndir, devgen);
}

static void
resxtra(int port, struct termios *ts);	/* non-POSIX extensions */

Chan*
eiaopen(Chan *c, int mode)
{
	int port = NETID(c->qid.path);

	c = devopen(c, mode, eiadir, ndir, devgen);

	switch(NETTYPE(c->qid.path)) {
		case Nctlqid:
		case Ndataqid:
		case Nstatqid:
			if(incref(&eia[port].r) != 1)
				break;
			if (waserror()) {
				decref(&eia[port].r);
				nexterror();
			}
			openport(port);
			poperror();

			break;
	}
	return c;
}

void
eiaclose(Chan *c)
{
	int port = NETID(c->qid.path);

	if((c->flag & COPEN) == 0)
		return;

	switch(NETTYPE(c->qid.path)) {
	case Nctlqid:
	case Ndataqid:
	case Nstatqid:
		if(decref(&eia[port].r) == 0) {
			osenter();
			CloseHandle(eia[port].hCom);
			osleave();
		}
		break;
	}

}

long
eiaread(Chan *c, void *buf, long n, ulong offset)
{
	DWORD cnt;
	char err[ERRLEN];
	int port = NETID(c->qid.path);
	BOOL good;

	if(c->qid.path & CHDIR)
		return devdirread(c, buf, n, eiadir, ndir, devgen);

	switch(NETTYPE(c->qid.path)) {
		case Ndataqid:
			cnt  = 0;
			// if ReadFile timeouts and cnt==0 then just re-read
			// this will give osleave() a chance to detect an
			// interruption (i.e. killprog)
			while (cnt==0) {
	  			osenter(); 
				good = ReadFile(eia[port].hCom, buf, n, &cnt, NULL);
				SleepEx(0,FALSE);  //allow another thread access to port
				osleave();
				if (! good) {
					oserrstr(err);
					error(err);
				}
			}
			return cnt;
		case Nctlqid:
				return readnum(offset, buf, n, eia[port].id, NUMSIZE);
		case Nstatqid:
			return rdstat(port, buf, n, offset);
	}

	return 0;
}

long
eiawrite(Chan *c, void *buf, long n, ulong offset)
{
	DWORD cnt;
	char err[ERRLEN];
	char cmd[Maxctl];
	int port = NETID(c->qid.path);
	BOOL good;
	uchar *data;

	if(c->qid.path & CHDIR)
		error(Eperm);

	switch(NETTYPE(c->qid.path)) {
	case Ndataqid:
		cnt = 0;
		data = (uchar*)buf;
		// if WriteFile times out (i.e. return true; cnt<n) then
		// allow osleave() to check for an interrupt else try
		// to send the unsent data.
		while (n>0) {
	  		osenter(); 
			good = WriteFile(eia[port].hCom, data, n, &cnt, NULL);
			osleave(); 
			if (! good) {
				oserrstr(err);
				error(err);
			}
			data += cnt;
			n -= cnt;
		}
		return (data-(uchar*)buf);
	case Nctlqid:
		if(n >= sizeof(cmd))
			n = sizeof(cmd)-1;
		memmove(cmd, buf, n);
		cmd[n] = 0;
		wrctl(port, cmd);
		return n;
	}
	return 0;
}

void
eiawstat(Chan *c, char *dp)
{
	Dir d;
	int i;

	if(strcmp(up->env->user, eve) != 0)
		error(Eperm);
	if(CHDIR & c->qid.path)
		error(Eperm);

	convM2D(dp, &d);
	i = Nqid*NETID(c->qid.path)+NETTYPE(c->qid.path)-Ndataqid;
	eiadir[i].perm = d.mode&0666;
}

Dev eiadevtab = {
        Devchar,
        Devname,

        eiainit,
        eiaattach,
        devclone,
        eiawalk,
        eiastat,
        eiaopen,
        devcreate,
        eiaclose,
        eiaread,
        devbread,
        eiawrite,
        devbwrite,
        devremove,
        eiawstat
};


//
// local functions
//

/*
 * open the indicated comm port and then set 
 * the default settings for the port.
 */
static void
openport(int port)
{
	char ebuf[ERRLEN];
	Eia* p = &eia[port];

	// open the port
	p->hCom = CreateFile(sysdev[p->id], 
		GENERIC_READ|GENERIC_WRITE,     //open underlying port for rd/wr
		0,	                            //comm port can't be shared
		NULL,	                        //no security attrs
		OPEN_EXISTING,                  //a must for comm port
		FILE_ATTRIBUTE_NORMAL,          //nonoverlapped io
		NULL);                          //another must for comm port

	if(p->hCom == INVALID_HANDLE_VALUE) {
		oserrstr(ebuf);
		error(ebuf);
	}		

	// setup in/out buffers (NT requires an even number)
	if (! SetupComm(p->hCom, CommBufSize, CommBufSize)) {
		oserrstr(ebuf);
		CloseHandle(p->hCom);
		error(ebuf);
	}

	// either use existing settings or set defaults
	if ( ! p->restore) {
		// set default settings
		if (!GetCommState(p->hCom, &(p->dcb))) {
			oserrstr(ebuf);
			CloseHandle(p->hCom);
			error(ebuf);
		}
		p->dcb.BaudRate      = 9600;
		p->dcb.ByteSize      = 8;
		p->dcb.fParity       = 0;
		p->dcb.Parity        = NOPARITY;
		p->dcb.StopBits      = ONESTOPBIT;
		p->dcb.fInX          = 0;  //default to xoff
		p->dcb.fOutX         = 0;  
		p->dcb.fAbortOnError = 1; //read/write abort on err
	}

	if(!SetCommState(p->hCom, &p->dcb)) {
		oserrstr(ebuf);
		CloseHandle(p->hCom);
		error(ebuf);
	}		

	// set timeouts
	if (! SetCommTimeouts(p->hCom, &timeouts)) {
		oserrstr(ebuf);
		CloseHandle(p->hCom);
		error(ebuf);
	}		
}




/*
 * Obtain status information on the com port.
 */
static long
rdstat(int port, void *buf, long n, ulong offset)
{
	HANDLE   hCom = eia[port].hCom;
	char     err[ERRLEN];
	char     str[Maxctl];
	char     *s;
	DCB      dcb;
	DWORD    modemstatus;
	DWORD    porterr;
	COMSTAT  portstat;
	int      frame;
	int      overrun;

	// valid line control ids
	static enum {
		L_CTS, L_DSR, L_RING, L_DCD, L_DTR, L_RTS, L_MAX
	};

	// line control strings (should match above id's)
	static char* lines[] = {
		"cts", "dsr", "ring", "dcd", "dtr",	"rts", NULL
	};

	int status[L_MAX];

	// get any error conditions; also clears error flag
	// and enables io
	if (!ClearCommError(hCom, &porterr, &portstat)) {
		oserrstr(err);
		error(err);
	}

	// get comm port state
	if (! GetCommState(hCom, &dcb)) {
		oserrstr(err);
		error(err);
	}
	// get modem line information
	if (! GetCommModemStatus(hCom, &modemstatus)) {
		oserrstr(err);
		error(err);
	}

	// now set our local flags
	status[L_CTS]  = MS_CTS_ON & modemstatus;
	status[L_DSR]  = MS_DSR_ON & modemstatus;
	status[L_RING] = MS_RING_ON & modemstatus;
	status[L_DCD]  = MS_RLSD_ON & modemstatus;
	status[L_DTR]  = FALSE;  //?? cand this work: dcb.fDtrControl;
	status[L_RTS]  = FALSE;  //??   dcb.fRtsControl;
	frame          = porterr & CE_FRAME;
	overrun        = porterr & CE_OVERRUN;

	s = str;
	s += sprint(s, "opens %d ferr %d oerr %d baud %d", 
		    eia[port].r.ref-1, 
			frame, 
			overrun,
		    dcb.BaudRate);

	// add line settings
	{
		int i;
		for(i=0; i < L_MAX; i++) 
			if (status[i])
				s += sprint(s, " %s", lines[i]);
		sprint(s, "\n");
	}
	return readstr(offset, buf, n, str);
}

//
// write on ctl file. modify the settings for
// the underlying port.
//
static void
wrctl(int port, char *cmd)
{
	DCB    dcb;
	char   err[ERRLEN];
	int    nf, n;
	char   *field[Maxfield];
	HANDLE hCom = eia[port].hCom;
	DWORD  flag;
	BOOL   rslt;

	// get the current settings for the port
	if (!GetCommState(hCom, &dcb)) {
Error:
		oserrstr(err);
		error(err);
	}

	nf = parsefields(cmd, field, Maxfield, " \t\n"); 
	if(nf > 1)
	        strcat(field[0], field[1]);
	n  = atoi(field[0]+1);
	if(nf > 0)
	        switch(*field[0]) {
		case 'B':
		case 'b':	// set the baud rate
			if(n < 110)
				error(Ebadarg);
			dcb.BaudRate = n;
			break;
		case 'L':
		case 'l':	// set bits per byte 
		    flag = stof(size, field[0]+1);
			if(flag == BAD)
				error(Ebadarg);
			dcb.ByteSize = (BYTE)flag;
			break;
		case 'S':
		case 's':	// set stop bits -- valid: 1 or 2 (win32 allows 1.5??)
			flag = stof(stopbits, field[0]+1);
			if (flag==BAD)
				error(Ebadarg);
			dcb.StopBits = flag;
			break;
		case 'P':
		case 'p':	// set parity -- even or odd
			flag = stof(parity, field[0]+1);
			if (flag==BAD)
				error(Ebadarg);
			dcb.Parity = (BYTE)flag;
			break;
		case 'M':
		case 'm':	// set CTS
			dcb.fOutxCtsFlow    = (n!=0);
			break;
		case 'F':
		case 'f':	// flush any untransmitted data
			if (! PurgeComm(hCom, PURGE_TXCLEAR)) 
				goto Error;
			return;
			break;
		case 'H':
		case 'h':	//hangup 
			//in unix: cfsetospeed(&ts, B0);  //B0==hangup
			//?? don't know in win32? CloseHandle?
			
			//?? fall through and just do a break
		case 'K':
		case 'k':	//send a break
			// is tcsendbreak(fd, 0) in unix
			// the following is a close approximation
			if (!SetCommBreak(hCom)) goto Error;
			SleepEx((DWORD)300, FALSE);
			if (!ClearCommBreak(hCom)) goto Error;
			return;
			break;
		case 'X':
		case 'x':	// xon/xoff
			{
				DWORD opt;
				opt = n ? SETXON : SETXOFF;
				if (!EscapeCommFunction(hCom, opt)) {
					oserrstr(err);
					error(err);
				}
				return;
			}
		case 'D':
		case 'd':  // set DTR
			{
				DWORD opt;
				opt = n ? SETDTR : CLRDTR;
				if (!EscapeCommFunction(hCom, opt)) {
					oserrstr(err);
					error(err);
				}
				return;
			}
		case 'R':
		case 'r':	// set RTS
			{
				DWORD opt;
				opt = n ? SETRTS : CLRRTS;
				if (!EscapeCommFunction(hCom, opt)) {
					oserrstr(err);
					error(err);
				}
				return;
			}
		default:
		    error(Ebadarg);
		}

	// make the changes on the underlying port, but flush
	// outgoing chars down the port before
	// on unix is: tcsetattr(fd, TCSADRAIN, &ts);  
	osenter();
	rslt = FlushFileBuffers(hCom);
	if (rslt)
		rslt = SetCommState(hCom, &dcb);
	osleave();
	if(! rslt)
		goto Error;
	eia[port].restore  = 1;
	eia[port].dcb      = dcb;
}


