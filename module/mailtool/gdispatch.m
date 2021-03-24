
# Generic association list dispatch signature
#  uses Alists as basic methodology
# GDispatch extended to allow passing in of the graphics context ;-)

GDispatch: module
{
	# no path?

	# init message returns nil on success, else error.
	init: fn(): string;

        # dispatch method, returns (nil, diagnostic) only on error.
	dispatch: fn( Input: ref AsscList->Alist ): 
			ref AsscList->Alist;

        store_the_graphics_context_please: fn(ctx: ref Draw->Context);

	#PATHs
	MMGRPATH: con "/dis/mailtool/mail-mgr.dis";
};
