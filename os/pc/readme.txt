








	      Inferno Network Operating System
	     Generic x86 Board Support Package

		    Eric Van Hensbergen
		     ericvh@lucent.com

		 Inferno Native Kernel Team
	 Lucent Technologies Bell-Labs Innovations
		      666 Mountain Ave
		   Murray Hill, NJ 07974


			  ABSTRACT


	  Inferno(tm)  is  a  network  operating system
     which is a product of the Inferno Business Unit of
     Lucent  Technologies and was developed in coopera-
     tion with the Computing Science Research Center of
     Bell Labs.

	  This	document describes the requirements for
     the generic x86 board support package and	details
     instructions for building the boot loader, kernel,
     and BSP distribution.  It also  describes	how  to
     install  the  built  boot loader and kernel onto a
     target and details how to	setup  a  network  file
     server to serve applications to the target.



1.  Requirements

     1.1.  Supported Target Hardware

	  1.1.1.  CPU

	       1.1.1.1.	 386

	       1.1.1.2.	 486

	       1.1.1.3.	 Pentium

	       1.1.1.4.	 Pentium/II

	  1.1.2.  Physical Drives

	       1.1.2.1.	 Standard 31/4 1.44 MB Floppy Drive

	       1.1.2.2.	 ATA Hard Drive (IDE)

	  1.1.3.  VGA
	       NOTE: If you do not have one of these two cards,
			it is VERY likely your system will not work
			and will probably reboot the system when you
			try to load the kernel.

	       1.1.3.1.	  Cirrus  Logic	 CLGD542X-based	 VGA
	       Cards

	       1.1.3.2.	 S3-based VGA Cards

	  1.1.4.  Ethernet

	       1.1.4.1.	 3-Com Etherlink III (3C509,  3C579,
	       3C592, 3C590, 3C595, 3C900, 3C905)

	       1.1.4.2.	 Intel 82557 PCI (Intel EtherExpress
	       PRO/100B)

	       1.1.4.3.	 Digital Semiconductor DECchip 21140
	       PCI (DE-500-X)

	  1.1.5.  Audio

	       1.1.5.1.	  SoundBlaster	16  Compatible (non-
	       plug & play)

     1.2.  Supported Boot Environment

     The generic x86 BSP's configured environment is network
     based, that is to say that the system expects to access
     the application file system from a file server using an
     ethernet network.	The system may also be configured to
     boot it's kernel from the network.	 In order  for	this
     to	 work correctly, you must have a network file server
     running a hosted version of Inferno 2.3.

2.  Building

     2.1.  Building the Kernel

	     cd <builddir>/os/pc
	     mk install acid

     2.2.  Building the Boot Loader

	  Note: The boot loader must be built after the kernel
	       cd <builddir>/os/boot/pc
	       mk install

     2.3.  Building a BSP distribution from a source distri-
     bution

	cd <builddir>/os/boot/pc
	mk clean
     	cd <builddir>/os/pc
     	chmod u+x Dist.a
     	iar vu Dist.a bootp.8 compress.8 devip.8 gre.8 icmp.8 \
	  	il.8 ip.8 ipaux.8 ipextproto.8 media.8 netlog.8 \
	   	pktifc.8 ppp.8 tcp.8 udp.8
	chmod u+w mkfile
     	remove $LIBNAME dependancy from mkfile
     	mk clean
     	mk <builddir>/utils
     	mk clean
	Make a tar archive using <builddir>/os/pc/x86ddklist as
	 a file list.

3.  Installation and Boot

     3.1.  Installing the Boot Loader

     The Generic x86 BSP boot-loader is a DOS  uttility	 (in
     other  words, you must have DOS or Windows installed on
     the machine and a mechanism for copying the boot loader
     (and  any necessary configuration files) to the target.
     You must then edit the boot-loader's configuration file
     (inferno.ini)  to	configure  any	devices	 and/or boot
     options as discussed in the boot-loader's man-page.


     3.2.  Setting Up a Boot Host

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
	  may  usually	by looking at the card, by reviewing
	  system logs for  unknown  bootp  requests,  or  by
	  putting  the	bootp  application into "snoop" mode
	  (-s).

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

	  You can start bootp  in  two	ways,  manually,  or
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

     After building your kernel, boot-loader, and setting up
     your  file	 server	 boot  the target into DOS, edit the
     inferno.ini file as appropriate.	In  most  cases	 the
     only entry necessary in your inferno.ini will be a des-
     ignation of your hardware's ethernet controller:

	  ether0=type=21140

     Once your inferno.ini is configured, start l.com:

	  C> l


Appendix: Man-Pages for inferno.ini & l.com
-------------------------------------------



L.COM(8)	      System Administration		 L.COM(8)



NAME
     l.com - PC bootstrap program

SYNOPSIS
     (Under MS-DOS)
     [ drive :][ path ]l [ bootfile ]

DESCRIPTION
     l.com  is	an MS-DOS program that loads and starts a program
     in Inferno boot format  (-H3  option  to  8l  (see	 2l(1))).
     l.com  loads  the bootfile at the entry address specified by
     the header, usually 0x80100020.  After loading,  control  is
     passed to the entry location.

     l.com  can	 be loaded  by booting MS-DOS and typing l in the
     appropriate directory.

     The bootfile can be specified in one of 3 ways, in order  of
     precedence:
     	command-line argument
     	configuration file option
     	default, based on available devices

     The  format  of  the  bootfile name is device!unit!file.  If
     !file is omitted, the default for the particular  device  is
     used.  Supported devices are

     fd	  An MS-DOS floppy disk.  The bootfile is the contents of
	  the MS-DOS file.  The default file is ipc.

     e	  Ethernet.  File is [host:]pathname.

     h	  Hard (IDE) disk partition.  The bootfile  is	the  con-
	  tents of the partition given by file.	 The default par-
	  tition is boot.

     hd	  Hard (IDE) disk MS-DOS partition.  As for fd.

     When l.com starts, it relocates itself to address 0x80000 in
     the standard PC memory and switches to 32-bit mode.  It then
     double maps the first 16Mb of  physical  memory  to  virtual
     addresses 0 and 0x80000000.  Physical memory between 0x80000
     and 0xA0000 and from 0x200000 upwards is used as program and
     data  space.   Next, in order to find configuration informa-
     tion, l.com searches all units on devices fd, hd, and sd, in
     that  order,  for	an  MS-DOS  file system containing a file
     called inferno.ini (see inferno.ini(8)).  If one  is  found,
     searching stops and the file is read into memory at physical
     address 0x400 where it can be  found  later  by  any  loaded
     bootfile.	Some options in inferno.ini are used by l.com:

     console
     baud Specifies  the  console  device  and baud rate if not a
	  display.

     etherX
	  (where X is a number) Ethernet interfaces. These can be
	  used	to load the bootfile over a network.  Probing for
	  Ethernet interfaces is too prone to error.

     bootfile=device!unit!file
	  Specifies the bootfile.  This option is overridden by a
	  command-line argument.

     bootfile=auto
	  Default.

     bootfile=local
	  Like auto, but do not attempt to load over the network.

     bootfile=manual
	  After determining which devices are available for load-
	  ing from, enter prompt mode.

     When  the	search for inferno.ini is done, l.com proceeds to
     determine which bootfile to load.	If there was no	 command-
     line  argument  or	 bootfile option, l.com chooses a default
     from the following prioritized device list: fd e h hd

     l.com then attempts to load the bootfile  unless  the  boot-
     file=manual  option  was given, in which case prompt mode is
     entered immediately.  If the default  device  is  fd,  l.com
     will  prompt  the	user for input before proceeding with the
     default bootfile load after 5 seconds; this prompt is  omit-
     ted if a command-line argument or bootfile option was given.

     l.com prints the list of  available  device!unit  pairs  and
     enters  prompt mode on encountering any error or if directed
     to do so by a bootfile=manual option.  In prompt  mode,  the
     user  is  required to type a device!unit!file in response to
     the prompt.

     A control-P character typed  at  any  time	 on  the  console
     causes l.com to perform a hardware reset.

FILES
     [drive:] [ path ]l.com
     MS-DOS filesystem:\inferno.ini

SOURCE
     os/boot/pc

SEE ALSO
     inferno.ini(8)

BUGS
     Much  of  the work done by l.com is duplicated by the loaded
     kernel.

     The BIOS data area at physical address 0x400 should  not  be
     overwritten, and more use made of the information therein.

     If	 l.com	detects	 an installed MS-DOS Extended Memory Man-
     ager, it attempts to de-install it, but the  technique  used
     may  not  always  work.   It  is  safer  not  to install the
     Extended Memory Manager before running l.com.

INFERNO.INI(8)	      System Administration	   INFERNO.INI(8)



NAME
     inferno.ini - configuration file for PC's

SYNOPSIS
     none

DESCRIPTION
     When  booting  Inferno  on a PC, the DOS program l.com first
     reads a DOS file containing configuration	information  from
     the  boot	disk.  This file, inferno.ini, looks like a shell
     script containing lines of the form

	  name=value

     each of which defines a kernel or device parameter.

     For devices, the generic format of value is

	  type=TYPE [port=N]  [irq=N]  [mem=N]	[size=N]  [dma=N]
     [ea=N]

     specifying	 the  controller  type,	 the base I/O port of the
     interface,	 its  interrupt	 level,	 the  physical	 starting
     address  of  any  mapped memory, the length in bytes of that
     memory, the DMA channel, and for Ethernets	 an  override  of
     the physical network address.  Not all elements are relevant
     to all devices; the relevant values and their  defaults  are
     defined below in the description of each device.

     The  file	is  used by l.com and the kernel to configure the
     hardware available.  The  following  sections  describe  how
     variables are used.

  etherX=value
     This  defines  an Ethernet interface.  X, a unique monotoni-
     cally increasing number beginning at 0, identifies an Ether-
     net  card to be probed at system boot.  Probing stops when a
     card is found or there is no line for etherX+1.

     Some cards are software configurable and do not require  all
     options.	 Unspecified   options	default	 to  the  factory
     defaults.

     Known types are

     elnk3
     3C509
	  As found in the 3COM Etherlink III and  Fast	EtherLink
	  adapters.

     21140
	  The Digital Semiconductor DECchip 21140 PCI Fast Ether-
	  net  LAN  Controller	as  found  on  the  Digital  Fast
	  EtherWORKS PCI 10/100 adapter (DE-500-X)

     82557
	  The Intel 82557 Fast Ethernet PCI Bus LAN Controller as
	  found on the Intel EtherExpress PRO/100B.

  serialX=value
     This defines add on serial ports and cards.  Multiple  cards
     can share the same IRQ.  Unfortunately, many PC's allow only
     the built in UARTs on the COM1 and COM2 IRQ's  (3	&  4)  so
     beware.

     Known types are

     mp008
	  The  TTC  8 serial line card.	 The mem parameter is the
	  port number of the interrupt polling port.  Size is the
	  number of UARTs, default 8.  Port is the port number of
	  the first UART.

     generic
	  Any set of 16450 compatible serial lines with	 consecu-
	  tive	port  addresses.   Size	 is  the number of UARTs,
	  default 1.  Port is the port number of the first  UART.

  mouseport=value
     This specifies where the mouse is attached.  Value can be

     ps2  the  PS2 mouse/keyboard port.	 The BIOS setup procedure
	  should be used to configure the machine  appropriately.

     0	  for COM1

     1	  for COM2

  console=value
  baud=value
     These  are	 used to specify the console device.  The default
     console value is cga.  Values of 0 or 1 specify COM1 or COM2
     respectively,  in	which case baud is used to initialize the
     port.

  bootfile=value
     This is used to direct the actions of l.com(8).

  audioX=value
     This defines a sound interface.

     Known types are

     sb16 Sound Blaster 16.

     The DMA channel may be any of 5, 6, or 7.	The defaults  are
	  port=0x220 irq=7 dma=5

EXAMPLES
     A representative inferno.ini:


	  % cat /n/c:/ingrtno.ini
	  ether0=type=3C509
	  mouseport=ps2
	  modemport=1
	  serial0=type=generic port=0x3E8 irq=5
	  %

     Minimum  CONFIG.SYS  and AUTOEXEC.BAT files to use COM2 as a
     console:


	  % cat /n/c:/config.sys
	  SHELL=COMMAND.COM COM2 /P
	  % cat /n/c:/autoexec.bat
	  @ECHO OFF
	  PROMPT $p$g
	  PATH C:\DOS;C:\BIN
	  mode com2:96,n,8,1,p
	  SET TEMP=C:\TMP
	  %

SEE ALSO
     l.com(8),

BUGS
     Being able to set the console device to other than a display
     is	 marginally  useful  on file servers; MS-DOS and the pro-
     grams which run under it are so tightly bound to the display
     that  it  is  necessary  to  have	a display if any setup or
     reconfiguration programs need to be run.	Also,  the  delay
     before any messages appear at boot time is disconcerting, as
     any error messages from the BIOS are lost.

     This idea is at best an interesting  experiment  that  needs
     another iteration.

