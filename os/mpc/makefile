!include ..\..\mkconfig.nmk
!include ..\..\$(SYSHOST)\mkhost
!include ..\..\$(OBJDIR)\makefile

dotodir=.

CC_LOCAL=-I..\port -I..\..\interp

CONF=pc
CONFLIST=pc

INIT=init
IR=ir

# Run ntprep to produce these files.
!include devlist	# For DEVCONF and DEVPORT
!include linklist	# For LINK
!include misclist	# For MISC
!include iplist		# For IP
!include liblist	# For LIBS

OBJ= $(LCLOBJ_S) $(LCLOBJ_C) $(PORTOBJ_C) $(IPOBJ_C)

LCLOBJ_S=\
	l.$O\

LCLOBJ_C=\
	clock.$O\
	dma.$O\
	fault386.$O\
	main.$O\
	mmu.$O\
	screen.$O\
	trap.$O\
	$(DEVCONF)\
	$(MISC) $(LINK)\
	kbd.$O\

PORTOBJ_C=\
	alarm.$O\
	alloc.$O\
	xalloc.$O\
	chan.$O\
	dbg.$O\
	dev.$O\
	exportfs.$O\
	fault.$O\
	dis.$O\
	discall.$O\
	latin1.$O\
	ns.$O\
	pgrp.$O\
	print.$O\
	proc.$O\
	qio.$O\
	qlock.$O\
	styx.$O\
	sysfile.$O\
	inferno.$O\
	dial.$O\
	taslock.$O\
	utils.$O\
	$(DEVPORT)\

IPOBJ_C=\
	$(IP)

b$(CONF):	$(OBJ) $(CONF).c $(LIBS) daten size
	$(CC) -o $(CONF).$O $(CFLAGS) -DKERNDATE=0 $(CONF).c
	$(LD) -o $@ -T0x80100020 -l $(OBJ) $(CONF).$O $(LIBS)
	.\size $@

install: b$(CONF)
	cp b$(CONF) ..\..\$(OBJDIR)\bin\tv$(CONF)

uninstall:
	$(DEL) $(DELFLAGS) ..\..\$(OBJDIR)\bin\tv$(CONF)

!include ..\port\portmakefile

%.$O:	io.h
clock.$O devether.$O fault386.$O main.$O trap.$O: \
				..\..\$(OBJDIR)\include\ureg.h

osinit.root.c:	..\init\$(INIT).dis data2c
		.\data2c osinit ..\init\$(INIT).dis osinit.root.c 

..\init\$(INIT).dis:	..\init\$(INIT).b
		cd ..\init
		$(MAKE)
		cd ..\pc

ir.root.c:	..\init\$(IR).dis data2c
		cp ../init/$(IR).dis ir.out
		.\data2c ir ..\init\$(IR).dis ir.root.c 

..\init\$(IR).dis:
		cd ..\init
		$(MAKE) $(IR).dis
		cd ..\pc

phone.root.c:	..\..\icons\phone.bit data2c
		.\data2c phone ..\..\icons\phone.bit phone.root.c 

lucent.root.c:	..\..\icons\lucent.bit data2c
		.\data2c lucent ..\..\icons\lucent.bit lucent.root.c 

screen.$O: lucent.root.c
