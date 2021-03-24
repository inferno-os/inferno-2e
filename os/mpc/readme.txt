




	      Inferno Network Operating System
	     MPC 823 FADS  Board Support Package



			  ABSTRACT


	  Inferno(tm)  is  a  network  operating system
     which is a product of the Inferno Business Unit of
     Lucent  Technologies and was developed in coopera-
     tion with the Computing Science Research Center of
     Bell Labs.

	  This	document describes the requirements for
     the MPC 823 FADS board support package and	details
     instructions for building the boot loader, kernel,
     and BSP distribution.  It also  describes	how  to
     install  the  built  boot loader and kernel onto a
     target and details how to	setup  a  network  file
     server to serve applications to the target.



1.  Requirements

     1.1.  Supported Target Hardware

		MPC 823 FADS Board, although can be ported to
	other MPC 8XX variants.

     1.2.  Supported Boot Environment

     The MPC 823 BSP's configured environment is network
     based, that is to say that the system expects to access
     the application file system from a file server using an
     ethernet network.	The system may also be configured to
     boot it's kernel from the network.	 In order  for	this
     to	 work correctly, you must have a network file server
     running a hosted version of Inferno 2.3.

2.  Building

     2.1.  Building the Kernel

	     cd <builddir>/os/mpc
	     mk install acid

     2.2.  Building the Boot Loader

	  Note: The boot loader must be built after the kernel
	       cd <builddir>/os/boot/mpc
	       mk f.mx

     2.3.  Building a Solaris BSP distribution from a source distri-
     bution

	cd <builddir>/os/boot/mpc
	mk clean
     	cd <builddir>/os/mpc
     	chmod u+x Dist.a
     	iar vu Dist.a bootp.q compress.q devip.q gre.q icmp.q \
	  	il.q ip.q ipaux.q ipextproto.q media.q netlog.q \
	   	pktifc.q ppp.q tcp.q udp.q \
		chk.q dat.q fcall.q fswren.q kcon.q sub.q console.q dentry.q \
		fs.q iobuf.q kfs.q uid.q
	chmod u+w mkfile
     	remove $LIBNAME dependency from <builddir>/os/mpc/mkfile
	cd <builddir>/os/boot/mpc
	chmod u+w mkfile
	remove libkern.a dependency from <builddir>/os/boot/mpc
     	mk clean
     	cd <builddir>/utils
     	mk clean
	Make a tar archive using <builddir>/os/mpc/mpcddkproto as
	 a file list.

     2.4   Building an NT BSP distribution from a source distribution

	cd <builddir>/os/boot/mpc
	mk clean
	cd <builddir>/os/mpc
	chmod 777 Dist.a
     	iar vu Dist.a bootp.q compress.q devip.q gre.q icmp.q \
	  	il.q ip.q ipaux.q ipextproto.q media.q netlog.q \
	   	pktifc.q ppp.q tcp.q udp.q \
		chk.q dat.q fcall.q fswren.q kcon.q sub.q console.q dentry.q \
		fs.q iobuf.q kfs.q uid.q
	chmod 666 mkfile
	remove $LIBNAME dependency from <builddir>/os/mpc/mkfile
	mk clean
	cd <builddir>/os/boot/mpc
	chmod 666 mkfile
	remove libkern.a dependency from <builddir>/os/boot/mpc
	mk clean
	cd <builddir>/utils
	mk clean

	The InstallShield files assume the <builddir> is c:\usr\inf2.3,
	it this is not correct than the files in the File Groups directory
	must be changed to point to the correct <builddir>. The MPC 823
	BSP InstallShield files are in the tar file.
	
		<builddir>\scripts\InstallShield\mpc823.tar

	The files should be extracted from the tar file into 
	C:\My Installations\MPC823 Directory. Using InstallShield 5.0 or
	later, open the file 'mpc 823 bsp.ipr'. Create the media using the
	button  Build -> Make Media, and than send to the media. 
	

3.  Installation and Boot

     3.1.  Installing the Boot Loader



     3.2.  Setting Up a Bootp File Server Host

	  3.2.1.  Setting Up Bootp

	  In order to boot the x86 target, you	must  create
	  an  entry  for  it  in  the  Bootp  database.	 The
	  default  location  for  this	database  is   /ser-
	  vices/bootp/db.   In order to create an entry, you
	  must first obtain the target's  hardware  ethernet
	  address,  IP	address,  subnet  mask	and  gateway
	  information.	You must have also decided on a file
	  server, authentication server, and boot file name.

	  Bootp database entries are colon-separated fields,
	  one line per machine in the following format:

<name>:<h/w addr>:<ip addr>:<boot-file>:<sub-net>:<gateway>:\
	<file server>:<auth server>:

	  Example:

nativepc:ha=080020880AE2:ip=135.3.60.180:bf=ipc:sm=255.255.255.0: \
	gw=135.3.60.150:fs=135.3.60.232:au=135.3.60.232:

	  <name>  designates the symbolic name of the system
	  you are booting.

	  <h/w addr> is the hardware ethernet address  which
	  by default is 00108bf12900 as defined in the file
	  os/boot/mpc/conf.c in the variable, defplan9ini.
	  This should be changed for each system.

	  <ip addr> is the IP address you wish to assign  to
	  the target.

	  <boot-file> is the path to the kernel (Relative to
	  tftpd) that you wish to boot.

	  <gateway> is the ip address of the target's  gate-
	  way.

	  <file	 server>  is  the ip address of the target's
	  file server (running styxd).

	  <auth server> is the ip address  of  the  target's
	  authentication server.

	  Make	sure to leave a blank line at the end of the
	  Bootp	 database.   If	 bootp	can't  process	 the
	  database  (due  to formating errors) it will print
	  error messages and may not start.  Try  using	 the
	  debug	 flags	(-d,  -D,  and -v) if you are having
	  trouble.

	  You can start bootp  in  two	ways on an Host running
	  the Inferno host command emu,  manually,  or
	  using	 lib/srv.   To	start bootp manually, simply
	  run lib/bootp from the shell:

	       inferno$ lib/bootp &

	  You may also start bootp  automatically  when	 you
	  start	  lib/srv   by	adding	an  entry  to  /ser-
	  vices/server/config:

	       10 M 67	 udp /dis/lib/bootp.dis

	  3.2.2.  Setting Up Tftpd

	  In order to start up tftpd (which  allows  you  to
	  boot	kernels across the network), simply create a
	  directory called /services/tftpd and copy  kernels
	  you  wish  to boot there.  Then either start tftpd
	  manually from the shell:

	       inferno$ lib/tftpd

	  or add another line to /services/server/config:

	       10 M 69	 udp /dis/lib/tftpd.dis

	NOTE: Since bootp & tftpd use ports in the "reserved"
	 	range under Unix, you may need to run emu as root
		depending on how your system is configured.

	  3.2.3.  Exporting the File System

	  In order to set up a simple file server for devel-
	  opment, simply run lib/srv from the shell:

	       inferno$ lib/srv

	  If  properly	configured, this will start up bootp
	  and tftpd for you as well.  For  more	 details  on
	  the components and functions of lib/srv along with
	  a  more  detailed  description  of  how  to  setup
	  authenticated	 file  service, please reference the
	  Inferno user's manuals.

     3.3.  Booting

	The MPC 823 boot program can load the kernel from either
	the network or from flash. When booting the kernel via the
	network the bootp server must be have the correct ethernet
	address in the ea field for the MPC board. The boot loader
	code is in the file os/boot/mpc/f.mx in S-record format.
	This file is loaded into the MPC 823 FADS board's flash memory
	using Motorola's MPC8BUG utility. The command to use is the
	loadf. Refer to your MPC8BUG documentation for information on
	how to connect your MPC 823 FADS board to the MPC8BUG command.

	Once the MPC 823 is reset the boot loader runs and examines the
	Dip Switch one(DS1) settings to determine:
		1) filesystem is in flash(off) or network file server(on).
		2) load kernel from network(on) or flash(off).
		3) oscillators is 4(on) or 5(off) Mhz. 

	The LSB of the DS1 switch is on the right. Once the kernel is loaded
	it accesses the file system as defined in DS1 on continues to boot.

	The Serial port of the MPC 823 FADS board is the console output
	during boot loader and the kernel operation. This port can be
	connected via a null modem cable to a serial port at 9600 baud,
	8 bits, no parity and no xon-xoff. A terminal emulator can be
	running to gather console output and send commands to the console,
	because the MPC 823 has no keyboard input.

	If the debug driver is included the serial port after the kernel
	starts the debug driver uses the serial port for diagnostics.
	So, console keyboard input is disabled.


     3.4.  Flash
	
	The MPC 823 board has only 2 MB of flash memory, which is divided
	into 256Kb sectors. The boot loader is loaded into the first sector,
	and the kernel is loaded into sectors two through four. If a
	filesystem is loaded in the flash memory it is stored in sector five
	through seven. The last sector is reserved for updating other flash
	segments. This scheme limits the size of the flash kernel and 
	filesystem to 727040 bytes, to allow for some reserved portions of
	the segments.

	Once the kernel is booted and running with a connection to a file
	server the following commands can be used to create a filesystem
	and load it onto the MPC's flash.

	# Zero out the filesystem
	$ zeros 1024 |dd -bs 1024 -of myfs.kfs -count 2048


	Use kfs(1) to create the contents of the filesystem in myfs.kfs.

	# clear flash and copy the contents of myfs.kfs to flash
	$ echo format > '#X/ftlctl'
	$ cp myfs.kfs '#X/ftldata'

	# tell kfs to access the file system
	$ echo filsys main '#X/ftldata' > '#Kcons/kfsctl'
	$ echo cons flashwrite > '#Kcons/kfsctl'

	# check filesystem than add to local namespace
	$ echo cons check v > '#Kcons/kfsctl'
	$ bind -c '#Kmain' /n/local


     3.4.  Configuration
	
	The configuration information is compiled into the boot loader
	in the variable, defplan9ini. This is an array of strings
	defining the ethernet hardware comfiguration SCC and ethernet
	address, the size of the lcd screen in pixels, the amount
	of memory available to the kernel, and the console output
	parameters. There is no current method of modifying this
	information via the flash. Although, the structure is in the
	boot loader to determine this.


     3.5.  Debugging with Acid

	In order to debug the kernel with the acid command, you must include
	the debug driver and powerbreak files into the kernel build. This
	is done in the dev section of the mpc kernel configuration file,
	os/mpc/mpc, as follows:

		dev
			dbg	powerbreak portbreak
			
	
	Rebuild the kernel and copy it to your bootp server. To have the
	most information available, you should also run the mk acid rule.
	After the system boots, the debugger is in control of the serial port,
	and sets the speed to 19200 baud. The serial port must be connected
	via a null modem cable to a working serial port on your host.
	An example command to run acid on the host is:


		acid -R <serial_port_name> -l impc.acid impc

	
