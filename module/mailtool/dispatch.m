
# Generic association list dispatch signature
#  uses Alists as basic methodology

Dispatch: module
{
	# no path?

	# init message returns nil on success, else error.
	init: fn(): string;

        # dispatch method, returns (nil, diagnostic) only on error.
	dispatch: fn( Input: ref AsscList->Alist ): 
			ref AsscList->Alist;
	dostat1: fn(): int;
	# PATHs
	SMTPPATH: con "/dis/mailtool/smtp.dis";
	POP3PATH: con "/dis/mailtool/pop3.dis";
};
