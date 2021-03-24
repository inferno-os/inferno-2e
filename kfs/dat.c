#include	"all.h"

Uid*	uid;
char*	uidspace;
short*	gidspace;
File*	files;
Wpath*	wpaths;
Lock	wpathlock;
Lock	newfplock;
RWlock	mainlock;
Tlock	*tlocks;
Fsconf	fsconf;
Cons	fscons;
FChan	*chan;
int	RBUFSIZE;
int	BUFSIZE;
int	DIRPERBUF;
int	INDPERBUF;
int	INDPERBUF2;
int	FEPERBUF;

Filsys	filesys[MAXFILSYS];

Device 	devnone = {Devnone, 0, 0, 0};

Devcall devcall[MAXDEV] = {
			{0}, 	
		/*
		 [Devnone]	{0},
		*/
			{
			wreninit,
			wrenream,
			wrencheck,
			wrensuper,
			wrenroot,
			wrensize,
			wrenread,
			wrenwrite}
		/*
		 [Devwren]	{
			wreninit,
			wrenream,
			wrencheck,
			wrensuper,
			wrenroot,
			wrensize,
			wrenread,
			wrenwrite}
		*/
};

char*	tagnames[MAXTAG] =
{
			"Tnone",
			"Tsuper",
			"Tdir",
			"Tind1",
			"Tind2",
			"Tfile",
			"Tfree",
			"Tbuck",
			"Tvirgo",
			"Tcache"
	/*
	[Tbuck]		"Tbuck",
	[Tdir]		"Tdir",
	[Tfile]		"Tfile",
	[Tfree]		"Tfree",
	[Tind1]		"Tind1",
	[Tind2]		"Tind2",
	[Tnone]		"Tnone",
	[Tsuper]	"Tsuper",
	[Tvirgo]	"Tvirgo",
	[Tcache]	"Tcache"
	*/
};

char	*fserrstr[MAXERR]=
{
	"none",
	"attach -- bad specifier",
	"unknown fid",
	"bad character in directory name",
	"read/write -- on non open fid",
	"read/write -- count too big",
	"phase error -- directory entry not allocated",
	"phase error -- qid does not match",
	"no authentication",
	"access permission denied",
/* Next entry used to be	"directory entry not found", */
	"file does not exist",		/* must be this or sh.b gets worried */
	"open/create -- unknown mode",
	"walk -- in a non-directory",
	"create -- in a non-directory",
	"phase error -- cannot happen",
	"create -- file exists",
	"create -- . and .. illegal names",
	"remove -- directory not empty",
	"attach -- privileged user",
	"wstat -- not owner",
	"wstat -- not in group",
	"create/wstat -- bad character in file name",
	"walk -- too many (system wide)",
	"file system read only",
	"file system full",
	"read/write -- offset negative",
	"open/create -- file is locked",
	"close/read/write -- lock is broken"
};

#ifdef	NEVER
char	*fserrstr[MAXERR] =
{
	[Ebadspc]	"attach -- bad specifier",
	[Efid]		"unknown fid",
	[Echar]		"bad character in directory name",
	[Eopen]		"read/write -- on non open fid",
	[Ecount]	"read/write -- count too big",
	[Ealloc]	"phase error -- directory entry not allocated",
	[Eqid]		"phase error -- qid does not match",
	[Eauth]		"no authentication",
	[Eaccess]	"access permission denied",
/* Next entry used to be	[Eentry]	"directory entry not found", */
	[Eentry]	"file does not exist",	
	[Emode]		"open/create -- unknown mode",
	[Edir1]		"walk -- in a non-directory",
	[Edir2]		"create -- in a non-directory",
	[Ephase]	"phase error -- cannot happen",
	[Eexist]	"create -- file exists",
	[Edot]		"create -- . and .. illegal names",
	[Eempty]	"remove -- directory not empty",
	[Ebadu]		"attach -- privileged user",
	[Enotu]		"wstat -- not owner",
	[Enotg]		"wstat -- not in group",
	[Ename]		"create/wstat -- bad character in file name",
	[Ewalk]		"walk -- too many (system wide)",
	[Eronly]	"file system read only",
	[Efull]		"file system full",
	[Eoffset]	"read/write -- offset negative",
	[Elocked]	"open/create -- file is locked",
	[Ebroken]	"close/read/write -- lock is broken"
};
#endif
