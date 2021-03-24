<mkconfig

# Directories common to all architectures.
# Build in order:
#	- critical libraries used by the limbo compiler
#	- the limbo compiler (used to build some subsequent libraries)
#	- the remaining libraries
#	- commands
#	- utilities (unix and windows only)

DIRS=\
	lib9\
	libbio\
	crypt\
	math\
	limbo\
	interp\
	keyring\
	image\
	prefab\
	tk\
	memimage\
	memlayer\
	asm\
	kfs\
	emu\
	appl\

KERNEL_DIRS=\
	os/sa1100\
	os/pc\
	os/mpc\
	os/js\
	os/boot/mpc\
	os/boot/sa1100\
	os/init\

JAVA_DIRS=\
	java

foo:QV:
	echo mk all, clean, install, installall, nuke, or uninstall

all:V:		all-$HOSTMODEL
clean:V:	clean-$HOSTMODEL
install:V:	install-$HOSTMODEL
installall:V:	installall-$HOSTMODEL
kernel:V:	kernel/all-$HOSTMODEL
kernelall:V:	kernel/all-$HOSTMODEL
kernelclean:V:	kernel/clean-$HOSTMODEL
kernelinstall:V:	kernel/installall-$HOSTMODEL
kernelnuke:V:	kernel/nuke-$HOSTMODEL
kerneluninstall:V:	kernel/uninstall-$HOSTMODEL
nuke:V:		nuke-$HOSTMODEL
uninstall:V:	uninstall-$HOSTMODEL
java:V: java/all-$HOSTMODEL
javainstall:V: java/install-$HOSTMODEL
javaclean:V: java/clean-$HOSTMODEL
javanuke:V: java/nuke-$HOSTMODEL

cleandist:V: clean
	rm -f $ROOT/$OBJDIR/lib/lib*.a

&-Posix:QV:
	for j in $DIRS libregexp utils tools
	do
		echo "(cd $j; mk $MKFLAGS $stem)"
		(cd $j; mk $MKFLAGS $stem)
	done

&-Nt:QV:
	for (j in $DIRS libregexp utils tools)
	{
		echo '@{builtin cd' $j '; mk $MKFLAGS $stem}'
		@{builtin cd $j; mk.exe $MKFLAGS $stem}
	}

&-Inferno:QV:
	for (j in $DIRS libregexp utils)
	{
		echo '@{builtin cd' $j '; mk $MKFLAGS $stem}'
		@{builtin cd $j; mk $MKFLAGS $stem}
	}

kernel/&-Posix:QV:
	for j in $KERNEL_DIRS
	do
		echo "(cd $j; mk $MKFLAGS $stem)"
		(cd $j; mk $MKFLAGS $stem)
	done

kernel/&-Nt:QV:
	for (j in $KERNEL_DIRS)
	{
		echo '@{builtin cd' $j '; mk $MKFLAGS $stem}'
		@{builtin cd $j; mk $MKFLAGS $stem}
	}

kernel/&-Inferno:QV:
	for (j in $KERNEL_DIRS)
	{
		echo '@{builtin cd' $j '; mk $MKFLAGS $stem}'
		@{builtin cd $j; mk $MKFLAGS $stem}
	}

java/&-Posix:QV:
	for j in $JAVA_DIRS
	do
		echo "(cd $j; mk $MKFLAGS $stem)"
		(cd $j; mk $MKFLAGS $stem)
	done

java/&-Inferno java/&-Nt:QV:
	for (j in $JAVA_DIRS)
	{
		echo '@{builtin cd' $j '; mk $MKFLAGS $stem}'
		@{builtin cd $j; mk $MKFLAGS $stem}
	}

# Convenience targets

Hp-% hp-%:V:
	mk 'SYSHOST=Hp' 'OBJTYPE=s800' $stem

Inferno-% inferno-% Inferno-386-% inferno-386-%:V:
	mk 'SYSHOST=Inferno' 'OBJTYPE=386' $stem

Inferno-arm-% inferno-arm-%:V:
	mk 'SYSHOST=Inferno' 'OBJTYPE=arm' $stem

Plan9-% plan9-%:V:
	mk 'SYSHOST=Plan9' 'OBJTYPE=386' $stem

Irix-% irix-%:V:
	mk 'SYSHOST=Irix' 'OBJTYPE=mips' $stem

Linux-% linux-%:V:
	mk 'SYSHOST=Linux' 'OBJTYPE=386' $stem

NetBSD-% netbsd-%:V:
	mk 'SYSHOST=NetBSD' 'OBJTYPE=386' $stem

Nt-% nt-% Win95-% win95-%:V:
	mk 'SYSHOST=Nt' 'OBJTYPE=386' $stem

Solaris-% solaris-%:V:
	mk 'SYSHOST=Solaris' 'OBJTYPE=sparc' $stem
