#
# 	Module:		Shannon
#	Author:		Eric Van Hensbergen (ericvh@lucent.com)
#	Purpose:	Initial DIS thread
#				Setup the namespace and initial configuration
# ed - obc

implement Shannon;

#
# Included Modules
#

include "sys.m";
	sys:	Sys;
	stderr : ref Sys->FD;

include "draw.m";

include "timedio.m";
	timedio : TimedIO;

include "tk.m";

include "bufio.m";
	bufio:	Bufio;
	Iobuf: import bufio;

include "string.m";
	str:	String;

#
# Constants
#

KFS_READONLY:	con "ronly";
KFS_READWRITE:	con "";

#
# The following are for the purposes of remote debug setup
#
debug:	con 9;						# debug level
use_local: con 1;

# Factory and other fs load procedures
# Defaults can be overridden by boot-time environment variables

Serialize := 0;                 # allow program flash serialization/configuration
Factory := 1;			# built for factory load
Java := 0;			# built to load Java from PCMCIA disk
Makedisk := 0;			# built for developer upload
Atadata := 0;			# mount ata disk as 'data'
Sfdata := 1;			# mount serial flash as 'data'

logfile:con "#c/null";				# debug log file
wmpath:	con "/dis/wm/wmcp.dis";		# initial application (usually a wm)
startpt:con "/usr/inferno";			# directory to start initial DIS code in
host:	con "shannon";

bubblemsg44 :con "\n\n\n\tThere is a problem with\n\tyour IS 2630 that could\n\tnot be corrected.\n\tPlease call customer\n\tservice (see Owner's\n\tManual) for additional\n\thelp.\n";

#
# My module definition
#

Shannon: module
{
	init:	fn();
};

#
# A module definition to use when loading others
#

Sh: module
{
	init:	fn(ctxt: ref Draw->Context, argv: list of string);
};

# 
# Main Routine
#

k1p := 0;			# Kernel 1 flag
nrp := 0;			# Netready flag
nk1p := 0;			# Netready Kernel 1 flag

init()
{
	# 
	# Load the necessary modules
	#

	sys = load Sys Sys->PATH;
	timedio = load TimedIO TimedIO->PATH;
	if (debug > 0)
		stderr = sys->fildes(2);
	else 
		stderr = sys->open(logfile,Sys->OWRITE);

	startrand();

	#
	# Test environment for overrides of various file system actions
	#
	Makedisk = getenv(Makedisk, "makedisk");
	if (Makedisk)
	{
		# If you were expecting to make an image
		#  why would you first try to load it from the card?
		Factory = 0;
		# Fatal to mount a card and write over it
		Atadata = 0;
		Java = 0;
	}
	else
	{
		Java = getenv(Java, "java");
		if (Java)
		{
			# Must insert java card
			# Following options wouldn't make sense
			Atadata = 0;
			Factory = 0;
		}
		else
			Atadata = getenv(Atadata, "atadata");
		if (Atadata)
		{
			# Must insert ata/fs card
			# Following options wouldn't make sense
			Factory = 0;
			Sfdata = 0;
		}
	}
	if (!Atadata)
		Sfdata = getenv(Sfdata, "sfdata");
	#
	# Can always override "Factory".  If the disk is present
	#  the system will stop after performing the procedure.
	#
	Factory = getenv(Factory, "factory");
	if (Factory)
		flashman_load();

	kern := sysenv("#c/sysenv","bootfile");
	if (sys->open("/tmp", Sys->OREAD) != nil) {
	    sys->print("Netready kernel detected...\n");
	    nrp = 1;
	}
	if (kern == "F!kern1") {
	  k1p = 1;
	  if (nrp) 
	    nk1p = 1;
        }

	namespace_init();		# setup initial namespace
	setsysname();			# setup system name
	if(use_local) {
	  mountfs();
	  if (nk1p) {
	    dobind("/data", "/usr/inferno/config", sys->MAFTER);
	    dobind("/data", "/usr/inferno/charon", sys->MAFTER);
	  }
	  else {
	    if (Atadata)
	    {
	      flashman_data();	# ATA disk replaces serial flash
	      bindsfs();
	    }
	    else if (
	        Sfdata
		&&
		have_file("#F/dataflash")
	    )
	    {
	      if (readysflash())
	      {
		if (!condclearsfs())
			mountsfs();
		bindsfs();
	      }
	    }
	    if (Java) {
	      flashman_java();
	      bindjava();
	    }
	  }
	}

	namespace_local();	# bind the local names

	# kernel security type:
	#       i - international
	#       u - usa domestic
	ksv := kattr("security");
	sys->print("kernel security is [%s]\n", ksv);
	if(ksv != "u") # international or unknown
		dobind("/dis/lib/sslhs_intl.dis", "/dis/lib/sslhs.dis", sys->MREPL);

        ## jrc - new stuff for factory serialization
        Serialize = getenv(Serialize, "serialize");
        if (Serialize)
        {
                Open_Serial_Link();
        }                


        if (!nk1p)
	  bindreadimg();

        ## Speakerphone - absolutely required
	Spkr_Enable ();

	if (Makedisk)
		flashman_save();

	start();
}

# Why not use sysenv(,) below? - obc

# Get attribute value of a kernel build environment
# which has a format:
#       attr = val
# Any comment must start with '#' at a new line
Venv : con "/version.env";
kattr(attr: string): string
{
	file := Venv;
        if(bufio == nil)
                bufio = load Bufio Bufio->PATH;

        val := "";
parse:
        for(;;) {
                fd := sys->open(file, sys->OREAD);
                if(fd == nil) {
                        sys->print("can't open %s\n", file);
                        break parse;
                }
                bio := bufio->fopen(fd, bufio->OREAD);
                if(bio == nil) {
                        sys->print("open bufio failed\n");
                        break parse;
                }
                while((s := bufio->bio.gets('\n')) != nil) {
                        if(s[0] == '#') # comment
                                continue;
                        (n, sl) := sys->tokenize(s, " =\t\r\n");
                        if(n == 0) # blank line
                                continue;
                        if(hd sl == attr && n >= 2) {
                                val = hd tl sl;
                                break parse;
                        }
                }
        }
        return val;
}

mountfs()
{
  sys->print("Mounting application file system - readonly\n");
  if( !mountkfs("#W/flash0fs", "fs", KFS_READONLY) ) {
    if( !reamkfs("#W/flash0fs", "fs" ) ) {
      if (!nk1p)
	hangreboot();
    }
  }
  else {
    if (nrp) {
      if (sysenv("#c/sysenv", "fsdebug") != nil) {
	dobind("/", "/tmp", sys->MAFTER);
	dobind("#Kfs", "/", sys->MBEFORE);
      }
      else if (sys->open("#Kfs/tmp", Sys->OREAD) != nil) {
	sys->print("Netready FS detected...\n");
	if (nk1p)
	  dobind("#Kfs", "/", sys->MAFTER);
	else
	  dobind("#Kfs", "/", sys->MBEFORE);
      }
      else {
	if (nk1p)
	  sys->print("Warning: file system  is not for netready kernel.\n");
	else {
	  sys->print("Error: unexpected file system for netready kernel.\n");
	  sys->sleep(10000);
	}
	dobind("#Kfs", "/", sys->MAFTER);
      }
    }
    else
      dobind("#Kfs", "/", sys->MBEFORE);
  }
}

#
# Turn console printing on or off (restore)
#
consprint(on : int)
{
	fd := sys->open("#c/sysctl", sys->ORDWR);
	if (fd == nil)
		sys->fprint(stderr, "*** Can't open #c/sysctl: %r\n");
	else
	{
		cmd := array of byte "console restore";
		if (on)
			cmd = array of byte "console print";
		sys->write(fd, cmd, len cmd);
	}
}

#
# Bind java card file system
#
bindjava()
{
  dobind("#Kjava/dis", "/dis", sys->MAFTER);
  dobind("#Kjava/usr", "/usr", sys->MAFTER);
}

#
# Bind kernel-based image conversion utilities
#
bindreadimg()
{
	readimg := sysenv("#c/sysenv", "readimg");
	if ((readimg == nil) || (readimg == "kernel")) {
		sys->print("binding kernel readimg library...\n");
		sys->bind("/dis/wm/browser/readimg.dis", "/dis/wm/browser/readjpg.dis", Sys->MREPL);
		sys->bind("/dis/wm/browser/readimg.dis", "/dis/wm/browser/readgif.dis", Sys->MREPL);
	}
}

bindsfs()
{
    ### data flash
    dobind("#Ksfs", "/data", (sys->MAFTER | sys->MCREATE));
    dobind("/data", "/usr/inferno/config", sys->MREPL);
    dobind("/data", "/usr/inferno/charon", sys->MREPL);
}

condclearsfs() : int
{
  if (sysenv("#c/sysenv", "clearsfs") != nil) {
    consprint(1);
    sys->fprint(stderr, "Clearing serial flash...\n");
    if(!reamkfs("#F/dataflash", "sfs"))
      sys->fprint(stderr, "Failed to clear serial flash.\n");
    consprint(0);
    return 1;
  }
  return 0;
}

mountsfs()
{
  sys->print("Mounting serial data flash file system -readwrite\n");
  if (!mountkfs("#F/dataflash", "sfs", KFS_READWRITE)) {
    if (!reamkfs("#F/dataflash", "sfs")) {
      if(!nrp)
	hangreboot();
    }
  }
  if (kfs_cons("cfs sfs"))
    kfs_cons("flashwrite");
}

#
# Really shell for nfailsafe testing
#
nrshell()
{
	shenv := sysenv("#c/sysenv", "shell");
	if (shenv == nil)
		return;
	sh := load Sh shenv;
	if (sh == nil) {
		sys->fprint(stderr, "could not load %s: %r\n", shenv);
		return;
	}
	if (nrp)
	  sys->print("Starting shell from netready kernel...\n");
	else
	  sys->print("Warning: Starting shell from incomplete kernel...\n");
	if (!nk1p)
	  dobind(dialuipath, osdcpath, Sys->MREPL);
	sys->chdir(startpt);
	sh->init(nil, shenv :: nil);
}

#
# Start the initial application
#

### netready access for nfailsafe kernel only
# obc
mwmpath : con "/dis/wm/mwm.dis";
osdcpath : con "/dis/wm/sdc/sdc.dis";
tbarpath : con "/dis/wm/toolbar.dis";
dialuipath: con "/dis/wm/sdc/dialui.dis";

start()
{
  nrshell();
  if (!nrp || sysenv("#c/sysenv", "reboot") != nil)	# check hangreboot
    hangreboot();
  if (nk1p) {
    sys->print("Starting netready dialer...\n");
    nsd := load Sh dialuipath;
    if (nsd == nil) {
      sys->print("Looking for Dialui in: /tmp"+dialuipath+"\n");
      nsd = load Sh "/tmp"+dialuipath;
    }
    if (nsd != nil) {
      if ((waitime := sysenv("#c/sysenv", "wait")) != nil) {
	sys->print("Waiting %d seconds so you can read this...\n", int waitime);
	sys->sleep(int waitime * 1000);
      }
      nsd->init(nil, dialuipath :: nil);
      sys->print("Netready session finishes\n");
      dobind("#Kfs", "/", sys->MBEFORE);
      sys->sleep(5000);
    }
  }
  else {
    # Toolbar sideffect kill mwm mouse - need to use wmcp for now...
    # mwm := load Sh mwmpath;
    mwm := load Sh wmpath;
    if (mwm == nil)
      sys->print("Error: Mwm not found.\n");
    else {
      dobind(dialuipath, osdcpath, Sys->MREPL);
      mwm->init(nil, mwmpath :: tbarpath :: nil);
    }
  }
}

namespace_init()
{
	dobind("#t", "/net", sys->MREPL);	# serial line
	dobind("#I", "/net", sys->MAFTER);	# IP
	dobind("#c", "/dev", sys->MAFTER);	# console
	dobind("#T", "/dev", sys->MBEFORE); 	# Touchscreen/Contrast
	dobind("#p", "/prog", sys->MREPL);	# prog device
	dobind("#s", "/chan", sys->MREPL);	# chan device
}

#
# Setup local namespace after network mounts
#

namespace_local()
{
	dobind("#t", "/net", sys->MREPL);	# serial line
	dobind("#I", "/net", sys->MAFTER);	# IP
	dobind("#p", "/prog", sys->MREPL);	# prog device
	dobind("#c", "/dev", sys->MREPL);	# console
	dobind("#d", "/dev", sys->MBEFORE); 	# draw device
	dobind("#T", "/dev", sys->MBEFORE); 	# Touchscreen/Contrast
	dobind("#s", "/chan", sys->MREPL);	# chan device
}

#
# Bind wrapper which reports errors & spins
#
dobind(f, t: string, flags: int)
{
	if (sys->bind(f, t, flags) < 0) {
                consprint(1);
		sys->fprint(stderr, "bind(%s, %s, %d) failed: %r\n", f, t, flags);
		if(!nrp)
		  hangreboot();
		consprint(0);
	}
}

#
# Set our system name
#
setsysname()
{
	if (debug)
		sys->fprint(stderr, "host = %s\n", host);
	f := sys->open("/dev/sysname", sys->OWRITE);
	if (f == nil) {
		sys->fprint(stderr, "open /dev/sysname failed: %r\n");
		hangreboot();
	}
	b := array of byte host;
	if (sys->write(f, b, len b) < 0) {
		sys->fprint(stderr, "write /dev/sysname failed: %r\n");
		hangreboot();
	}
}

# Perform command on kfscons.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

kfs_cons(cmd : string) : int
{
	return kfs_cmd("cons", cmd);
}

# Perform command on kfsctl.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

kfs_ctl(cmd : string) : int
{
	return kfs_cmd("ctl", cmd);
}

# Send command to kfs.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

kfs_cmd(file : string, cmd : string) : int
{
	fd := sys->open("#Kcons/kfs" + file, sys->OWRITE);
	if (fd == nil) {
		sys->fprint(stderr, "could not open #Kcons/kfs%s: %r\n", file);
		return 0;
	}
	b := array of byte cmd;
	if (sys->write(fd, b, len b) < 0) {
		sys->fprint(stderr, "#Kcons/kfs%s: %r\n", file);
		return 0;
	}
	return 1;
}

# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

reamkfs(devname, fsname: string) :int
{
	return kfs_ctl("ream " + fsname + " " + devname);
}


# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

mountkfs(devname, fsname, ronly : string) :int
{
	return kfs_ctl("filsys " + fsname + " " + devname + " " + ronly);
}

getenv( val : int, var : string ) : int
{
	sval := sysenv("#c/sysenv", var);
	if (sval == nil)
		return(val);
	return(int sval);
}

#
# Reboot or else hang
#
hangreboot()
{
	# reboot by writing reboot to sysctl
	sysfd := sys->open("#c/sysctl", Sys->OWRITE);
	if (sysfd != nil) {
	  sys->fprint(stderr, "Restarting the device in five seconds...\n");
	  sys->sleep(5000);
	  sys->fprint(sysfd, "reboot");
	  sys->sleep(5000);
	}
	else {
	  # reboot wont work so hang
	  sys->fprint(stderr, "Halting the device in five seconds...\n");
	  sys->sleep(5000);
	  exit;
	}
}

cidTest()
{
	readResult : int;
	bbb := array[64] of byte;

	#attach the cid driver to dev
	channel := sys->bind( "#S", "/dev", sys->MBEFORE );

	#check the attach
	if( channel <= 0 )sys->print("\nbind fail");


	#open the file
	dsp2mpFD := sys->open( "/dev/dsp2mp", sys->ORDWR );
	if( nil == dsp2mpFD )sys->print("\nopen failed");

	for(;;)
	{
		#get any messages
		readResult = sys->read( dsp2mpFD, bbb, 64 );

		#data arrived
		if( readResult != 0 )
		{
			sys->print("\nl<-m ");
			i := 0;
			while( i < readResult )
			{
				sys->print("%c", (int bbb[i]) );
				++i;
			}
		}
	}
}

#
# Run the serial flash startup scan
# If ok, mark it ready
#

readysflash() : int
{
	if (have_file("#F/stat"))
	{
		if (!Factory)
		{
			consprint(1);
			err := runshcmd(1, "sf init");
			if (err != nil)
			{
				sys->fprint(stderr, "Can't init serial flash:\n -- %s\n", err);
				sys->fprint(stderr, "Trying to scan ...\n");
				sflash_cmd("scan");	# Try locally
			}
			consprint(0);
		}
		else
		{
			sflash_cmd("scan");
		}
		if (!sflash_ok())
		{
			if (nk1p)
				return 0;
			hangreboot();
		}
		sflash_cmd("ready");

		err := runshcmd(0, "sfmon");
		if (err != nil)
			sys->fprint(stderr, "Can't monitor SF errors: %s\n", err);
	}
	return 1;
}

sflash_cmd(cmd : string)
{
	fd := sys->open("#F/ctl", sys->ORDWR);
	sys->fprint(fd, "%s", cmd);
	sys->stream(fd, sys->fildes(1), 128);
}

sflash_ok() : int
{
	v := sysenv("#F/stat", "badScan");
	if ((v != nil) && (int v < 1000))
		return(1);
	sys->fprint(stderr, "Bad serial flash\n");
	consprint(1);
	sys->fprint(stderr, bubblemsg44);
	consprint(0);
	while(1)
		sys->sleep(1000);
	return(0);
}

flashman_load()
{
	if (have_disk())
	{
		flashman(1, "autoload");
		hangreboot();
	}
}

flashman_data()
{
	flashman(0, "automount");
}

flashman_save()
{
	if (
		flashman(0, "makedisk filsys")
		&&
		have_disk()
	)
		sys->sleep(10 * 1000);	# leave some time to reboot
}

flashman_java()
{
	flashman(0, "javamount");
}

Cantload: con "%s module not found: %r";

flashman(try_hd : int, cmd : string) : int
{
        consprint(1);
	errstr := runshcmd(try_hd, "flashman " + cmd);
	ret := 0;
	if (errstr != nil)
	{
		sys->fprint(stderr, "Can't scan for system disk: %s\n", errstr);
		sys->sleep(10000);
	}
	else
		ret = 1;
	consprint(0);
	return ret;
}


## Enable speakerphone - required to set speakerphone data

Spkr_Enable ()

{

   spawn spkph_update_task();

}


spkph_update_task ()

{

   BLOCK_SIZE       : con  20;                # Bytes in a block
   PARAM_BYTES_SIZE : con   5 * BLOCK_SIZE;   # Five blocks (0 - 4) of parameter data 
   STATE_BYTES_SIZE : con  20 * BLOCK_SIZE;   # Twenty blocks (0 - 19) of state data

   ALL_BYTES_SIZE   : con PARAM_BYTES_SIZE + STATE_BYTES_SIZE;

   spkr_dev_FD:   ref Sys->FD;     # Device level file, i.e., channel
   spkr_data_FD:  ref Sys->FD;     # Limbo level "file", for reading and writing

   spkr_data := array[ALL_BYTES_SIZE] of byte;
   
   readResult   : int;
   writeResult  : int;
   seekResult   : int;
   channel      : int;
   i            : int;
   invalid_file : int = 1;

   #attach the driver to dev
   channel = sys->bind( "#S", "/dev", sys->MBEFORE );

   #check the attach
   if ( channel <= 0 )  sys->print ("\n DSP driver bind fail");


   #open the channel/file
   spkr_dev_FD = sys->open( "/dev/speakperm", sys->ORDWR );
   if ( nil == spkr_dev_FD )  sys->print ("\n DSP speakperm open failed");

   #
   # CRD2115 - Handle an Inferno problem that "cannot happen".  Note that
   #           investigation of this MR shows that negative values can be
   #           returned by the call to read ( a completly undocumented
   #           feature).  
   # sure: read returns < 0 upon error, 0 if nothing is read.

   for(iq := 0; invalid_file && iq < 6; iq++)
   {
      # Now open the limbo file
      spkr_data_FD = sys->open( "/data/spkrdata.cfg", sys->ORDWR );
      if ( spkr_data_FD == nil )
      {
         #
         # File does not exist, go create it
         #

         sys->print  ( "\n  spkrdata.cfg - Open failed, creating the file" );

         # Set file permissions to be consistent with other files in this directory
         spkr_data_FD = sys->create ( "/data/spkrdata.cfg", sys->ORDWR, 8r666 );

         # Fill in default parameter data

         i = 0;

         # Params - Block 0
         
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r01;
         spkr_data[i++] = byte 16r03;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16rE6;
         spkr_data[i++] = byte 16rFF;
         spkr_data[i++] = byte 16r08;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r99;
         spkr_data[i++] = byte 16r32;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r40;
         spkr_data[i++] = byte 16rC5;
         spkr_data[i++] = byte 16r0C;
         spkr_data[i++] = byte 16r01;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r96;
         spkr_data[i++] = byte 16r1C;
         spkr_data[i++] = byte 16r01;
         spkr_data[i++] = byte 16r00;

         # Params - Block 1

         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r40;
         spkr_data[i++] = byte 16r04;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r40;
         spkr_data[i++] = byte 16r02;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r01;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r02;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16rB0;
         spkr_data[i++] = byte 16rFF;
         spkr_data[i++] = byte 16r70;
         spkr_data[i++] = byte 16rFF;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r40;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r40;

         # Params - Block 2

         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r40;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r40;
         spkr_data[i++] = byte 16rEC;
         spkr_data[i++] = byte 16r31;
         spkr_data[i++] = byte 16rFC;
         spkr_data[i++] = byte 16rFF;
         spkr_data[i++] = byte 16rFB;
         spkr_data[i++] = byte 16rFF;
         spkr_data[i++] = byte 16r01;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r01;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r5E;
         spkr_data[i++] = byte 16r01;
         spkr_data[i++] = byte 16rE6;
         spkr_data[i++] = byte 16rFF;
         spkr_data[i++] = byte 16r04;
         spkr_data[i++] = byte 16r00;

         # Params - Block 3

         spkr_data[i++] = byte 16r06;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r08;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r0A;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r10;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r12;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r14;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r16;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r1C;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r1E;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r20;
         spkr_data[i++] = byte 16r00;
 
         # Params - Block 4

         spkr_data[i++] = byte 16r22;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;
         spkr_data[i++] = byte 16r00;

         # Default the state bytes to all zeroes

         for ( i = PARAM_BYTES_SIZE ; i < ALL_BYTES_SIZE; i++ )
         {
            spkr_data[i] = byte 0;
         }
         if ( nil == spkr_data_FD ) {
		sys->print ("\n spkrdata.cfg - create failed %d", iq);
		sys->sleep(2000);
		if (nk1p)
			break;
		else
			continue;
	 }
         writeResult = sys->write( spkr_data_FD, spkr_data, ALL_BYTES_SIZE );
      }

      # File has been opened, now go to the beginning and get the data

      # Reset the file offset to start of file
      seekResult  = sys->seek( spkr_data_FD, 0, sys->SEEKSTART );
      sys->print ( "\n Seek Result, before extract from data file = %d ", seekResult );
      # Extract the data from the file
      readResult  = sys->read( spkr_data_FD, spkr_data, ALL_BYTES_SIZE );
      sys->print ( "\n Read Result, after extract from data file = %d ", readResult );
      if ( readResult <= 0 )
      {
         # Delete the file and recreate it!
         sys->print ( "\n spkrdata file is corrupted !!!  Deleting/recreating . . . " );
         sys->remove ("/data/spkrdata.cfg");
      }
      else
      {
         ## File is OK - force loop exit 
         invalid_file = 0;    
      }

   } ## END while ( invalid_file )


   ## Write Limbo speaker data to the device channel "file"
   writeResult = sys->write( spkr_dev_FD, spkr_data, ALL_BYTES_SIZE );
   sys->print ( "\n Write Result, after extract from data file = %d ", writeResult );

   ## Wait for any more updates from device driver
   for ( ; ; )
   {
      ## Get any messages
      readResult = sys->read( spkr_dev_FD, spkr_data, ALL_BYTES_SIZE );

      ## Data arrived
      if ( readResult != 0 && spkr_data_FD != nil)
      {
         ## Reset the file offset to start of file
         seekResult  = sys->seek( spkr_data_FD, 0, sys->SEEKSTART );
         ## Write the data to the file
         writeResult = sys->write( spkr_data_FD, spkr_data, ALL_BYTES_SIZE );
      }
   }
}

sysenv(filename,paramname : string) : string {
    buf : array of byte;

    fd       := sys->open(filename, sys->OREAD);
    if (fd == nil)
	return(nil);
    buf       = array[4096] of byte;
    nb       := sys->read(fd,buf,4096);
    sbuf     := string buf;
    (nfl,fl) := sys->tokenize(sbuf,"\n");
    while (fl != nil) {
	pair     := hd fl;
	(npl,pl) := sys->tokenize(pair,"=");
	if (npl > 1) {
	    if ((hd pl) == paramname)
		return((hd (tl pl)));
	}
	fl = tl fl;
    }
    return(nil);
}

Vname:	con "fd0";		# "unit" name for partition-formatted disk
HDvol:	con "#H/"+Vname;	# partition-formatted volume name
HD0:	con "#H/hd0";		# ordinary or empty disk
Appfs:	con "fs";		# std partition for application files
HD0fs:	con HD0 + Appfs;	# std partition for disk file system
HDvolfs: con HDvol + Appfs;	# alt partition for disk file system
Mnthd:	con "hd";		# kfs mount point for disk

#
# Globals
#

findmodule(try_hd : int, modname : string) : ( Sh, string )
{
	loadm := load Sh modname;
	if (loadm != nil)
		return ( loadm, "" );	# Found it on available filesys

	if (try_hd)
	{
		if (have_disk())
		{
			# See if the disk has one of the acceptable unit names.

			if (!can_mount(HD0fs) && !can_mount(HDvolfs))
			{
				return ( nil, "can't mount any file system from disk" );
			}
			if (!have_file("#K" + Mnthd + "/dis"))
			{
				return ( nil, "No /dis on disk" );
			}

			dobind("#K" + Mnthd + "/dis", "/dis", sys->MAFTER);
			loadm = load Sh modname;
			if (loadm != nil)
			{
				sys->fprint(stderr, "Notice: loaded %s from #K%s\n", modname, Mnthd);

				if (!have_file("#K" + Mnthd + "/dis/lib"))
					sys->fprint(stderr, "No /dis/lib on disk\n");
				else
					dobind("#K" + Mnthd + "/dis/lib", "/dis/lib", sys->MAFTER);
				return ( loadm, "" );
			}
		}
		else
		{
			# In failsafe, the local file system is mounted elsewhere
			loadm = load Sh "#Kfs" + modname;
			if (loadm != nil)
				return ( loadm, "" );	# Found it on alternate filesys
		}
	}
	return (nil, sys->sprint(Cantload, modname) );
}


runshcmd(try_hd : int, cmd : string)  : string
{
	(n, toks) := sys->tokenize(cmd, " \t");
	if (n <= 0)
		return "No cmd name";

	(mload, errstr) := findmodule(try_hd, "/dis/" + hd toks + ".dis");
	
	if (mload != nil)
	{
		mload->init(nil, toks);
		return nil;
	}
	return errstr;
}

can_mount(vol : string) : int
{
	if (!have_file(vol))
		return 0;
	if (have_file("#K" + Mnthd))
	{
		sys->fprint(stderr, "Notice(%s?): Using previously mounted #K%s\n", vol, Mnthd);
	}
	else if (mountkfs(vol, Mnthd, "ronly"))
	{
		sys->fprint(stderr, "Notice: Mounting %s to #K%s\n", vol, Mnthd);
	}
	else
		return 0;
	return 1;
}

# See if a disk card is plugged in.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

have_disk() : int
{
	return have_pcmdev("SunDisk");
}

# See if a card is plugged in containing "devstr" in the version string.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

have_pcmdev(devstr : string) : int
{
	if (bufio == nil)
		bufio = load Bufio Bufio->PATH;
	if (bufio == nil)
	{
		sys->fprint(stderr, Cantload + "\n", Bufio->PATH);
		return 0;
	}
	if (str == nil)
		str = load String String->PATH;
	if (str == nil)
	{
		sys->fprint(stderr, Cantload + "\n", String->PATH);
		return 0;
	}
	pcm := "#y/pcm0ctl" :: "#y/pcm1ctl" :: nil;
	while (pcm != nil)
	{
		if (have_pcmslot(hd pcm, devstr))
		{
			return 1;
		}
		pcm = tl pcm;
	}
	return 0;
}

# Read the control file of the pcm driver to see if a device is plugged in.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

have_pcmslot(f, devstr : string) : int
{
	pcmio := bufio->open(f, bufio->OREAD);
	if (pcmio == nil)
		return 0;
	for (;;)
	{
		line := pcmio.gets('\n');
		if (len line <= 0)
			return 0;

		( n, toks ) := sys->tokenize(line, "= ");
		if ((n < 2) || (hd toks != "verstr"))
			continue;

		# Found a version string.  Something is plugged in.

		(k, dump) := str->toint(hd tl toks, 10);
		buf := array[k] of byte;
		k = pcmio.read(buf, len buf);
		if (k <= 0)
		{
			sys->fprint(stderr, "Trouble reading %s: %r\n", f);
			return 0;
		}
		ver := string buf;
		( b, e ) := str->splitstrl(ver, devstr);
		if (len e > 0)
			return 1;
		return 0;
	}
}

# See if the file exists.
# Return value:  0 is "false" or "not OK", non-0 is "true" or "OK".

have_file(name : string) : int
{
	ret : int;

	ret = (sys->open(name, sys->OREAD) != nil);
	return(ret);
}

# Start the random number generator, so that it has some random numbers
# ready when we need them later on.
startrand()
{
	c:= array[1] of byte;
	fd := sys->open("#c/random", sys->OREAD);
	if (fd != nil)
		sys->read(fd, c, len c);
}




### =======================================================================
###
### New routines added for flash locking/unlocking
###
### =======================================================================


rsp_ACK   : con 16r06;    ## ASCII table hex value - ACK 
rsp_NACK  : con 16r15;    ## ASCII table hex value - NACK
rsp_READY : con 16r52;    ## ASCII table hex value - 'R'



###
### This routine will read the serial number data from program flash and
### display it.
###

Read_Flash ( config_buff: array of byte )

{  ### BEGIN Read_Flash

   cnt : int;
###   display_buff := array[64] of byte;

   if ( len config_buff != 1024 )
   {
      sys->print ( " \n  FATAL ERROR - Invalid size of config buffer " );
      return;
   }


   flash_FD := sys->open ( "#W/flash0config", Sys->OREAD );
   if ( flash_FD == nil )
   {
      sys->print ( " \n FATAL ERROR - cannot open program flash for reading " );
      return;
   }

   ### All is well, go read the data from flash
   cnt = sys->read ( flash_FD, config_buff, len config_buff );
   if ( cnt > 0 )
   {
###      display_buff[0:len display_buff] = config_buff[0:len display_buff];
###      sys->print ( "Config Data = [%s]\n", string display_buff[0:len display_buff] );
###      sys->print ( "Config Data = [%s]\n", string config_buff[0:40] );
   }
   else
   {
      sys->print ( "ERROR : Flash_Read reads %d bytes : %r\n", cnt );
   }

   ### Return memory to buffer pool
   flash_FD = nil;    

} ### END Read_Flash




###
### This routine will write the configuration data to flash
###
### The return value is number of errors that occurred, so a value of
### 0 means no errors occurred.
###


Write_Flash  ( config_buff: array of byte ) : int


{ ### BEGIN Write_Flash

num_errors := 0;  


   if ( len config_buff != 1024 )
   {
      sys->print ( " \n  FATAL ERROR - Invalid size of config buffer " );
      return num_errors + 1;
   }

   flash_FD := sys->open ( "#W/flash0config", Sys->OWRITE );
   if ( flash_FD == nil )
   {
      sys->print ( " \n FATAL ERROR - cannot open program flash for writing " );
      return num_errors + 1;
   }

   write_result := sys->write ( flash_FD, config_buff, len config_buff );
   if ( write_result != len config_buff )
   {
      sys->print ( " \n Write of config data failed ");
      return num_errors + 1;
   }

   ### Return memory to buffer pool
   flash_FD = nil;    

   return num_errors;

} ### END Write_Flash



##
## This routine will set the program flash permissions based on the 
## input parameter.  
##


Set_Flash_Permissions ( mode : int )


{ ## BEGIN Set_Flash_Permissions

partition_name : string;

   ## NOTE - Due to the organization of the AMD flash device, we must
   ##        protect/unprotect sboot, config, and kern1 in unison.

   for ( i := 0; i < 3; i++ )
   {
      case i
      {
         0 => partition_name = "#W/flash0config";
         1 => partition_name = "#W/flash0sboot";
         2 => partition_name = "#W/flash0kern1";
      }

      ## Get the current attributes
      ##(sys_stat_result, flash_attributes) := sys->stat ( "#W/flash0config" );
      (sys_stat_result, flash_attributes) := sys->stat ( partition_name );

      if ( sys_stat_result != 0 )
      {
         sys->print ( " \n FATAL ERROR - cannot access %s attributes", partition_name );
         return;
      }
      else ## ( Got the attributes OK )
      {
         ## Change file permissions as specified by input parameter
         flash_attributes.mode = mode;

         ## NOTE - writing attributes to flash, even the same exact settings,
         ##        will fail if 12 Volts is not applied !!!
 
         ## Write the new attributes
         ##sys_wstat_result := sys->wstat ( "#W/flash0config", flash_attributes );
         sys_wstat_result := sys->wstat ( partition_name, flash_attributes );
         if ( sys_wstat_result != 0 )
         {
            sys->print ( " \n FATAL ERROR - cannot write %s attributes", partition_name );
            return;
         }
         else ## ( wrote attributes OK )
         {
            sys->print ( " %s permissions have been updated to %d ", partition_name, mode );
         }
      }
   } 


} ## END Set_Flash_Permissions



###
### This routine will unlock the program flash by changing the file
### permissions to allow writing of configuration data to the flash.
###


UnLock_Flash (  )

{ ### BEGIN UnLock_Flash


   ### Get write access to Flash
   Set_Flash_Permissions ( 8r666 );


} ### END UnLock_Flash



###
### This routine will lock the program flash by changing the file
### permissions to allow readonly of configuration data in the flash.
###


Lock_Flash (  )

{ ### BEGIN Lock_Flash

      ## Set readonly permissions
      Set_Flash_Permissions ( 8r444 );

} ### END Lock_Flash



###
### This routine will send a single byte response over the serial port
###


Send_Rsp  ( data_port : ref Sys->FD,
            response  : int          )

{ ### BEGIN Send_Rsp

   rsp_as_array := array[1] of byte;
   rsp_as_array[0] = byte response;

   write_result := sys->write ( data_port, rsp_as_array, len rsp_as_array );
   if ( write_result != len rsp_as_array )
   {
      sys->print ( " \n Write of response failed ");
   }

} ### END Send_Rsp




###
### This routine will send a message to the factory PC telling it to begin
### Serial Number Download.  
###

Send_Ready_Msg  ( data_port : ref Sys->FD )


{ ### BEGIN Send_Ready_Msg

   ready_msg := array of byte "serialnumber";
   write_result := sys->write ( data_port, ready_msg, len ready_msg );
   if ( write_result != len ready_msg )
   {
      sys->print ( " \n Write of ready response failed ");
   }


} ### END Send_Ready_Msg

flash_flush()
{
 	# sync the program flash
 	fd := sys->open("#W/flash0ctl", sys->OWRITE);
 	if (fd == nil)
 		return;		# pray for no corruption (or maybe for it!)
 	sbuf := array of byte "sync";
 	sys->write(fd, sbuf, len sbuf);
}

p9_unsetenv(s: string)
{
	fd := sys->open("#W/flash0plan9.ini", sys->OREAD);
	if (fd == nil)
		return;
	buf := array[4096] of byte;
	n := sys->read(fd, buf, len buf);
	for(i := 0; i < n - (len s + 1); i++) {
		j := i+len s;
		if((i == 0 || buf[i-1] == byte '\n') && buf[j] == byte '=')
			if(string buf[i:i+len s] == s) {
				while(j < n && buf[j++] != byte '\n')
					;
				while(i < n) {
					if(j < n)
						buf[i++] = buf[j++];
					else
						buf[i++] = byte 0;
				}
			}
	}
	fd = sys->open("#W/flash0plan9.ini", sys->OWRITE);
	if(fd == nil)
		return;
	sys->write(fd, buf, n);
}

###
### This routine will change the value of the plan9.ini parameter "serialize"
### so that the serialize process will not execute again after it has been
### successfully performed.
###
### NOTE - this code was copied (and slightly modified) from the toolbar.b
###        file, from the routine "kernelworks"
###

unserialize (  )

{ ## BEGIN unserialize

	p9_unsetenv("serialize");
	p9_unsetenv("nofailsafe");
	flash_flush();
 
} ## END unserialize


###
### This routine will clean up any factory testing which may have populated
### caller id files, etc., and also remove the file which determines when
### to go through the day 1 scenario.  NOTE - just deleting the day 1 file
### is not enough, because after this file gets recreated, any data which
### was previously stored, is now available to the user.  
###

Restore_Day1 ( )
{
    ## Don't care what the response is, just delete the files

    ## NOTE - Does the remove function support wildcards??

    ## Day one file
    sys->remove( "/usr/inferno/config/configdb.dat" );

    ## Caller ID files
    sys->remove( "/usr/inferno/config/newcall.dat" );
    sys->remove( "/usr/inferno/config/oldcall.dat" );

    ## Directory files
    sys->remove( "/usr/inferno/config/folders.dat" );
    sys->remove( "/usr/inferno/config/entries.dat" );

    ## Telephone files
    sys->remove( "/usr/inferno/config/dialbuf" );
    sys->remove( "/usr/inferno/config/feature.dat" );
    sys->remove( "/usr/inferno/config/spkrdata.cfg" );

    ## Notepad files
    sys->remove( "/usr/inferno/config/notepad.dat" );

    ## Calendar files
    sys->remove( "/usr/inferno/config/cal.dat" );

    ## Browser files
    sys->remove( "/usr/inferno/config/browser.cfg" );
    sys->remove( "/usr/inferno/charon/config" );
    sys->remove( "/usr/inferno/charon/bookmarks.html" );
    sys->remove( "/usr/inferno/charon/history.html" );

    ## Miscellaneous files
    sys->remove( "/usr/inferno/config/toolbar.cfg" );
    sys->remove( "/usr/inferno/config/configIsp.dat" );
    sys->remove( "/usr/inferno/config/dns" );
    sys->remove( "/usr/inferno/config/fcntnr" );

}



##
## This routine will wait for a command input from the factory PC
## to begin a download process.
##
## NOTE - this can NOT run as a spawned process, because we do NOT
##        want to continue on and start up toolbar until this
##        step is complete.
##


Wait_for_PC_cmd ( data_port : ref Sys->FD )


{ ### BEGIN Wait_for_PC_cmd


cmd_buffer       := array [1] of byte;
byte_count       : int;
waiting_for_data := 1; 

config_data    := array [1024] of byte;
write_result   : int;
i              : int;
wcount         : int;
errors         := 0;

        Send_Ready_Msg ( data_port );

        consprint(1);  ## Turn On Console printing
        sys->print ( "\n \n \n Waiting for Serial Number Configuration . . . " );
        sys->print ( "\n \n \n Apply 12V Now !!! " );
        sys->print ( "\n \n \n Timeout in 10 seconds . . . " );
        consprint(0);  ## Turn OFF Console printing

   ## new code to handle a timeout
   pid_chan         := chan of int;
   user_action_chan := chan of ( string, byte );
   user_action      : string;
   first_byte       : byte;
   bypass_mode      := 0;    ## special processing for 1st byte received

   ##sys->print ( " \n Spawning Timer process " );
   spawn Wait_For_Input_Timer ( pid_chan, user_action_chan );
   timer_pid :=  <- pid_chan;

   ##sys->print ( " \n Spawning Data process " );
   spawn Wait_For_Input_Data  ( pid_chan, user_action_chan, data_port );
   data_pid  :=  <- pid_chan;

   ##sys->print ( " \n Spawning DSP process " );
   spawn Wait_For_DSP_Keypress  ( pid_chan, user_action_chan );
   dsp_pid  :=  <- pid_chan;

   ##sys->print ( " \n Waiting for user action  " );

   ( user_action, first_byte ) = <- user_action_chan;

   ##sys->print ( " \n Got user Action %s %x ", user_action, int first_byte );

   case user_action
   {
      "data"  =>
         ## Kill of the timer and dsp processes, and fall through to handler
         ##sys->print ( " \n Received Data, about to process " );
         Kill_the_Bastard ( timer_pid );
         Kill_the_Bastard ( dsp_pid );
         bypass_mode = 1;


      "timeout" =>
         ## Kill off the read and dsp processes and get out
         ##sys->print ( " \n Received Timer, about to abort " );
         Kill_the_Bastard ( data_pid );
         Kill_the_Bastard ( dsp_pid );
         return;


      "keypress" =>
         ## Kill off the read and timer processes and get out
         ##sys->print ( " \n Received DSP keypress, about to abort " );
         Kill_the_Bastard ( data_pid );
         Kill_the_Bastard ( timer_pid );
         return;
                
   }
       

   ## If we got here , no timeout occurred, so process data and check for
   ## additional incoming commands from PC

        consprint(1);  ## Turn On Console printing
        sys->print ( "\n \n \n RECEIVED DATA, processing command(s) . . ." );
        consprint(0);  ## Turn OFF Console printing

   while ( waiting_for_data )
   {
      if ( bypass_mode )
      {
         bypass_mode   = 0;
         byte_count    = 1;
         cmd_buffer[0] = first_byte;
      }
      else
      {
         byte_count = sys->read ( data_port, cmd_buffer, len cmd_buffer );
      }

      ## DEBUG only
      ##sys->print ( "\n Just got data, byte_count = %d, length cmd_buffer = %d ", byte_count, len cmd_buffer );


      ## Data has arrived
      if ( byte_count != 0 )
      {
         case int cmd_buffer[0]
         {
            'r' or 'R' =>  ## Read configuration block
               
               ## sys->print ( "\n Received READ command, sending data " );
               Send_Rsp ( data_port, rsp_ACK );

               ## Get the current data
               Read_Flash ( config_data );

               ## Send it out
               write_result = sys->write ( data_port, config_data, len config_data );
               if ( write_result != len config_data )
                  sys->print ( " \n Write of config data failed ");


            'd' or 'D' =>  ## Restore Day 1 - out of box settings

               ## sys->print ( "\n Received DAY 1 command " );
               Send_Rsp ( data_port, rsp_ACK );

               Restore_Day1 ();

               ## Disable any furthur serialization
               ## Do this here, not on the terminate, so that the factory
               ## can perform several iterations of this serial process,
               ## before finally clearing out the set.
               unserialize();



            'l' or 'L' =>  ## Lock Flash

               ## sys->print ( "\n Received LOCK command " );
               Send_Rsp ( data_port, rsp_ACK );
               Lock_Flash ();


            't' or 'T' =>  ## Terminate Link

               ## sys->print ( "\n Received TERMINATE command " );
               Send_Rsp ( data_port, rsp_ACK );

               ## Do NOT unserialize here, since we may want to do this
               ## again in the factory test sequence.  

               ## Force loop exit
               waiting_for_data = 0;


            'u' or 'U' =>  ## UnLock Flash

               ## sys->print ( "\n Received UNLOCK command " );
               Send_Rsp ( data_port, rsp_ACK );
               UnLock_Flash ();


            'w' or 'W' =>  ## Write Configuration Block

               ## sys->print ( "\n Received WRITE command " );
               Send_Rsp ( data_port, rsp_ACK );

               ## Accept the next 1K of data as configuration data
               for ( i = 0; i < len config_data; i++ )
               {
                  wcount = sys->read ( data_port, cmd_buffer, len cmd_buffer );
                  config_data[i] = cmd_buffer[0] ;
                  Send_Rsp ( data_port, rsp_ACK );
               }

               ## Put the accumulated data into Flash
               errors = Write_Flash ( config_data );
               if (errors == 0)
               {
                  Send_Rsp ( data_port, rsp_ACK );
               }
               else 
               {
                  Send_Rsp ( data_port, rsp_NACK );
               }


            * => ## DEFAULT
               sys->print ( "\n What is Factory PC command %x " , int cmd_buffer[0] );
               Send_Rsp ( data_port, rsp_NACK );
         }
      }
   }

} ### End Wait_for_PC_cmd




##
## This routine will establish a serial link with the factory PC
##


Open_Serial_Link (  )


{ ## BEGIN Open_Serial_Link

write_result : int;

   ### Set up the serial port
   control_FD := sys->open ( "/net/eia0ctl", Sys->ORDWR);

   if ( control_FD == nil )
   {
      sys->print ( " \n FATAL ERROR - cannot open serial port - control " );
      return;
   }
   else ## ( control_FD != nil )
   {
      ## Set up baud rate
      baud_rate := array of byte "B38400";
      write_result = sys->write ( control_FD, baud_rate, len baud_rate );
      if ( write_result != len baud_rate )
      {
         sys->print ( " \n Serial port - control - FAILED - cannot set 38400 baud, default is 9600 " );
      }
      else
      {
         sys->print ( " \n Serial port - control - Baud changed to 38400 " );
      }

      ## Set data size select to 7 bit data
      data_size := array of byte "L7";
      write_result = sys->write ( control_FD, data_size, len data_size );
      if ( write_result != len data_size )
      {
         sys->print ( " \n Serial port - control - FAILED - cannot set data size " );
      }
      else
      {
         sys->print ( " \n Serial port - control - Data Size set to 7 bits" );
      }

      ## Set Parity to none
      parity := array of byte "Pn";
      write_result = sys->write ( control_FD, parity, len parity );
      if ( write_result != len parity )
      {
         sys->print ( " \n Serial port - control - FAILED - cannot set parity " );
      }
      else
      {
         sys->print ( " \n Serial port - control - Parity set to none " );
      }
   }


   data_FD := sys->open ( "/net/eia0", Sys->ORDWR);
   if ( data_FD == nil )
   {
      sys->print ( " \n FATAL ERROR - cannot open serial port - data " );
      return;
   }
   else ### ( data_FD != nil )
   {
      ##sys->print ( " \n Serial port - data - is alive " );
      ##spawn Wait_for_PC_cmd ( data_FD );
      Wait_for_PC_cmd ( data_FD );
   }


} ## END Open_Serial_Link



##
## This routine will wait a fixed amount of time, and then timeout.
##

Wait_For_Input_Timer ( prog_id_chan : chan of int ,
                       input_chan   : chan of (string, byte)  )

{

TIME_10_SEC : con 10000;   ## time is in milliseconds

   prog_id_chan <- = sys->pctl(0,nil);   ## Tell parent process my ID

   sys->sleep ( TIME_10_SEC );

   ##sys->print ( " \n In Timer Process - just timed out, sending timeout msg " );
   input_chan <- = ( "timeout", byte 0);     ## Tell parent we timed out
                                             ## NOTE - byte is don't care.

   return;
}


##
## This routine will wait for any data input on the serial port
##

Wait_For_Input_Data (  prog_id_chan : chan of int,
                       input_chan   : chan of ( string, byte ),
                       data_port    : ref Sys->FD )
{

cmd_buffer       := array [1] of byte;

   prog_id_chan <- = sys->pctl(0,nil);   ## Tell parent process my ID

   ### Check for incoming commands from PC
   byte_count := sys->read ( data_port, cmd_buffer, len cmd_buffer );

   ##sys->print ( " \n In Data Process - sending data " );
   input_chan <- = ( "data", cmd_buffer[0] );     ## Tell parent we got data
   
   return;
}



##
## This routine will wait for a keypress on the DSP keypad
##

Wait_For_DSP_Keypress (  prog_id_chan : chan of int,
                         input_chan   : chan of ( string, byte ) )

{

DSP_KEYPRESS         : con 'K';
waiting_for_keypress : int = 1;
cmd_buffer           := array [64] of byte;

   prog_id_chan <- = sys->pctl(0,nil);   ## Tell parent process my ID

   #attach the dsp driver to dev
   channel := sys->bind( "#S", "/dev", sys->MBEFORE );
   if ( channel <= 0 )
   {
      sys->print ("\n DSP bind fail");
      return;
   }

   #open the dsp read channel
   dsp_read_FD := sys->open( "/dev/dsp2mp", sys->ORDWR );
   if ( dsp_read_FD == nil )
   {
      sys->print ("\n DSP Read open failed");
      return;
   }

   ## NOTE - since DSP sends many unsolicited responses, stay in a loop
   ##        until we get a keypress.  Do not assume that any response
   ##        from the DSP is a keypress by a user.

   ## Check for incoming commands from DSP
   while ( waiting_for_keypress )
   {
      byte_count := sys->read ( dsp_read_FD, cmd_buffer, len cmd_buffer );
      if ( byte_count != 0 )
      {
         if ( cmd_buffer[0] == byte DSP_KEYPRESS )
         {
            ## Tell parent we got data
            input_chan <- = ( "keypress", cmd_buffer[1] );
            waiting_for_keypress = 0;
         }
      }
   }

   return;
}




##
## This routine will kill off a [bastard] child process
##

Kill_the_Bastard ( pid: int )

{

   ##sys->print ( " \n Trying to kill process %d ", pid );

   ## Open the Process control file
   pid_FD := sys->open ( "/prog/" + string pid + "/ctl", sys->OWRITE );
   if ( pid_FD == nil )
   {
      sys->print ( "\n ERROR - Process doesn't exist to kill " );
      return;
   }

   kill_msg := array of byte "kill";
   write_result := sys->write ( pid_FD, kill_msg, len kill_msg );
   if ( write_result != len kill_msg )
   {
      sys->print ( " \n ERROR - Write failed trying to kill process ");
   }

}

