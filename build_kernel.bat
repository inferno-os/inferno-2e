@echo on
REM Nt inferno build script
set PATH=E:\USR\INF2.3\NT\386\BIN;%PATH%
rem call D:\MSDEV\BIN\vcvars32 x86
rem
rem move /Y C:\inf2.3\mkconfig.nmk C:\inf2.3\org.mkconfig.nmk
rem copy /Y C:\inf2.3\mkconfig.nmk C:\inf2.3\mkconfig.nmk

cd E:
cd \usr\inf2.3
set HOME=E:
set ROOT=E:\usr\inf2.3
rem echo running mk kernelnuke
rem mk SYSHOST=Nt SYSTARG=Nt OBJTYPE=386 ROOT=E:\usr\inf2.3 kernelnuke > E:\usr\log\inf2.3\kernelnuke.out

rem echo running mk uninstall
rem mk SYSHOST=Nt SYSTARG=Nt OBJTYPE=386 uninstall > E:\usr\log\inf2.3\uninstall.out

echo running mk kernelnuke
mk SYSHOST=Nt SYSTARG=Nt OBJTYPE=386 kernelnuke > E:\usr\log\inf2.3\kernelcleandist.out

rem echo running mk kerneluninstall
rem mk SYSHOST=Nt SYSTARG=Nt OBJTYPE=386 kerneluninstall > E:\usr\log\inf2.3\kerneluninstall.out

echo running mk kernelinstall
mk SYSHOST=Nt SYSTARG=Nt OBJTYPE=386 ROOT=E:\usr\inf2.3 kernelinstall > E:\usr\log\inf2.3\kernelinstall.out
rem mk kernelinstall

rem echo running mk kernelinstall
rem set SYSHOST=Nt
rem set SYSTARG=Inferno
rem set OBJTYPE=386
rem mk kernelinstall > E:\usr\log\inf2.3\kernelinstall.out

rem echo running mk kernelclean
rem mk SYSHOST=Nt SYSTARG=Nt OBJTYPE=386 ROOT=E:\usr\inf2.3 clean > E:\usr\log\inf2.3\clean.out
rem mk kernelclean > E:\usr\log\inf2.3\kernelclean.out

echo on
rem E:\mksnt\tar cf ../from_nt.tar Nt
rem

