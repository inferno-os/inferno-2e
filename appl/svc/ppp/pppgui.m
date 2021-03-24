###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1998 Lucent Technologies
###
### Originally Written by N. W. Knauft
### Adapted by E. V. Hensbergen
###

PPPGUI: module
{
        PATH:	con "/dis/svc/ppp/pppgui.dis";

	# Dimension constant for ISP Connect window
	WIDTH: con 264;
	HEIGHT: con 58;

        init:	fn(ctxt: ref Draw->Context, stat: chan of int,
			ppp: PPPClient, args: list of string): chan of int;
};

