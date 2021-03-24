#include "u.h"
#include "lib.h"
#include "mem.h"
#include "fns.h"
#include "iostream.h"
#include "console.h"
#include "image.h"
#include "screen.h"
#include "bootparam.h"
#include "cmd.h"

#define SBOOT_VER_MAJ	0
#define SBOOT_VER_MIN	68


extern Vmode default_vmode;
extern Vctlr vgait;
extern int text_fg, text_bg, text_wid, text_hgt;
extern const char *conffile;

extern void touchtest(void);


#include "exec.h"


void
print_title(void)
{
	print("\n");
	print("sboot v%d.%d (%s)\n",
			SBOOT_VER_MAJ, SBOOT_VER_MIN, conffile);
	print(" - Inferno StrongARM Bootstrap Loader\n\n",
			SBOOT_VER_MAJ, SBOOT_VER_MIN);
}


int
main(int argc, char **argv)
{
	int i;
	int debug = (bootparam->flags & BP_FLAG_DEBUG != 0);
	int autoboot = (*(ulong*)(bootparam->flashbase+0x428) != 0);
	int touch = 0;

	memset(edata, 0, end-edata);	/* clear the BSS */
	mallocinit(bootparam->himem-64*_K_, (ulong)end);
	links();
	console_init();

	if(bootparam->bootparam_major == 0
			&& bootparam->bootparam_minor < 3) {
		print("WARNING: BootParam interface >= 0.3 is required\n");
		debug = 1;
	} else {
		if(debug) {
			text_fg = 0x00;		/* white */
			text_bg = 0xc9;		/* dark blue */
		} else {
			text_fg = 0x00;		/* white */
			text_bg = 0xff;		/* black */
			stdout = vidcon_ostream;
		}
		if(setscreen(&default_vmode, &vgait) < 0)
			print("error: %r\n");
		stdout = vidcon_ostream;
	}

		
	//if(autoboot) {
	//	startupbanner();
	//	delay(1000);
	//}  else
		print_bootparam();

	// these parameters ought to come from BootParam:
	envinit(0xc60, 40*sizeof(char*), 0x1200, 256*sizeof(char));

	// mmuctlregw(mmuctlregr() | CpCDcache | CpCwb | CpCIcache);
	// if(!debug)
	//	mmuenable(0x5075);

	if(debug || !autoboot) {
		text_wid = 2;
		print("debug mode\n");
		text_wid = text_hgt = 1;
		// if(debug)
		//	stdout = demcon_ostream;
		// else
		stdin = kbd_istream;
		bootparam->flags |= BP_FLAG_DEBUG;
		print_title();
		cmdlock = 1;
		// if(!debug)
		//	system(". F!plan9.ini");
		text_hgt = 2;
		print("\nUpgrading ebsit from /tftpboot/ebsitupgrade.flash\n");
		text_hgt = 1;
		if(system("c e!/tftpboot/ebsitupgrade.flash F!all") < 0)
			print("Error: %r\n");
		else {
			text_hgt = 2;
			print("Upgrade complete\n\n\n");
			text_hgt = 1;
			print("You should now change the /etc/bootptab file on the\n");
			print("server to point to a standard ebsit kernel instead of\n");
			print("this upgrade utility\n\n");
		}
		cmdlock = 0;
		cmd_interp();
	} else
		system("A");		/* autoboot */
	print("sboot exiting... ");
	return 0;
}


void
halt(void)
{
	while(1)
		;
}

