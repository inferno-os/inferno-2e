!include ../../mkconfig.nmk
!include ../../$(SYSHOST)/mkhost

OBJ=\
	B.dis\
	auplay.dis\
	auhdr.dis\
	bind.dis\
	cat.dis\
	cd.dis\
	changelogin.dis\
#	changepass.dis\
	chmod.dis\
	cmp.dis\
	cook.dis\
	cp.dis\
	cpu.dis\
	date.dis\
	diff.dis\
	du.dis\
	echo.dis\
	fone.dis\
	getauthinfo.dis\
	gettar.dis\
	grep.dis\
	gunzip.dis\
	gzip.dis\
	host.dis\
	imount.dis\
#	init.dis\
#	irtest.dis\
	kill.dis\
	ls.dis\
	lstar.dis\
#	manufacture.dis\
	mathcalc.dis\
	metamorph.dis\
	mkdir.dis\
	mount.dis\
	mv.dis\
	netstat.dis\
	nsbuild.dis\
	os.dis\
	p.dis\
	ps.dis\
	puttar.dis\
	pwd.dis\
	randpass.dis\
	raw2iaf.dis\
	rcmd.dis\
	rm.dis\
	sh.dis\
	shutdown.dis\
	sleep.dis\
	stack.dis\
	tail.dis\
	tclsh.dis\
	time.dis\
	unicode.dis\
	unmount.dis\
	wav2iaf.dis\
	wc.dis\
	webgrab.dis\
	wish.dis\
	xd.dis\
	yacc.dis\

all:	$(OBJ)

auhdr.dis: auplay.dis
	copy auplay.dis auhdr.dis

MODULE= ..\..\module\sys.m\
	..\..\module\draw.m\
	..\..\module\string.m\
	..\..\module\prefab.m\
	..\..\module\ir.m\
	..\..\module\srv.m\
	..\..\module\tk.m\
	..\..\module\daytime.m\
	..\..\module\keyring.m\
	..\..\module\security.m\
	..\..\module\bufio.m\
	..\..\module\readdir.m\
	..\..\module\regex.m\
	..\..\module\newns.m\
	..\..\module\sh.m\
	..\..\module\workdir.m\
	..\..\module\filepat.m\
	..\..\module\webget.m\
	..\..\module\url.m\
	..\..\module\deflate.m\
	..\..\module\inflate.m\

$(OBJ):	$(MODULE)

install: $(OBJ)
	copy *.dis ..\..\dis

uninstall:
	@-for %%j in ( $(OBJ) ) \
	do $(DEL) $(DELFLAGS) ..\..\dis\%%j

package:	$(OBJ)\
		$(PREFIX_PKG)\dis
	copy $(OBJ) $(PREFIX_PKG)\dis

clean nuke:
	-$(DEL) $(DELFLAGS) *.dis 
	-$(DEL) $(DELFLAGS) *.sbl

.SUFFIXES:	.b .dis .s

.b.dis:	
	limbo -I../../module -gw $*.b

.b.s:	
	limbo -w -G -S -I../../module $*.b

$(PREFIX_PKG)\dis:
	mkdir $(PREFIX_PKG)\dis
