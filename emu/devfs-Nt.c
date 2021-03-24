#include	<windows.h>
#include	<winbase.h>
#undef	Sleep
#include	"dat.h"
#include	"fns.h"
#include	"error.h"
#include	"styx.h"
#include	<lm.h>

enum
{
	MAX_SID		= sizeof(SID) + SID_MAX_SUB_AUTHORITIES*sizeof(DWORD),
	ACL_ROCK	= sizeof(ACL) + 20*(sizeof(ACCESS_ALLOWED_ACE)+MAX_SID),
	SD_ROCK		= SECURITY_DESCRIPTOR_MIN_LENGTH + MAX_SID + ACL_ROCK,
	MAXCOMP		= 128,
};

typedef struct	User	User;
typedef struct  Gmem	Gmem;
typedef	struct	Stat	Stat;

/*
 * info about a user or group
 * there are two ways to specify a user:
 *	by sid, a unique identifier
 *	by user and domain names
 * this structure is used to convert between the two,
 * as well as figure out which groups a users belongs to.
 * the user information never gets thrown away,
 * but the group information gets refreshed with each setid.
 */
struct User
{
	QLock	lk;		/* locks the gotgroup and group fields */
	SID	*sid;
	Rune	*name;
	Rune	*dom;
	int	type;		/* the type of sid, ie SidTypeUser, SidTypeAlias, ... */
	int	gotgroup;	/* tried to add group */
	Gmem	*group;		/* global and local groups to which this user or group belongs. */
	User	*next;
};

struct Gmem
{
	User	*user;
	Gmem	*next;
};

/*
 * intermediate stat information
 */
struct Stat
{
	User	*owner;
	User	*group;
	ulong	mode;
};

/*
 * some "well-known" sids
 */
static	SID	*creatorowner;
static	SID	*creatorgroup;
static	SID	*everyone;
static	SID	*ntignore;
static	SID	*ntroot;	/* user who is supposed to run emu as a server */

/*
 * all users we ever see end up in this table
 * users are never deleted, but we should update
 * group information for users sometime
 */
static struct
{
	QLock	lk;
	User	*u;
}users;

/*
 * conversion from inferno permission modes to nt access masks
 * is this good enough?  this is what nt sets, except for NOMODE
 */
#define	NOMODE	(READ_CONTROL|FILE_READ_EA|FILE_READ_ATTRIBUTES)
#define	RMODE	(READ_CONTROL|SYNCHRONIZE\
		|FILE_READ_DATA|FILE_READ_EA|FILE_READ_ATTRIBUTES)
#define	XMODE	(READ_CONTROL|SYNCHRONIZE\
		|FILE_EXECUTE|FILE_READ_ATTRIBUTES)
#define	WMODE	(DELETE|READ_CONTROL|SYNCHRONIZE|WRITE_DAC|WRITE_OWNER\
		|FILE_WRITE_DATA|FILE_APPEND_DATA|FILE_WRITE_EA\
		|FILE_DELETE_CHILD|FILE_WRITE_ATTRIBUTES)

static	int
modetomask[] =
{
	NOMODE,
	XMODE,
	WMODE,
	WMODE|XMODE,
	RMODE,
	RMODE|XMODE,
	RMODE|WMODE,
	RMODE|WMODE|XMODE,
};

extern	DWORD	PlatformId;
	char    rootdir[MAXROOT] = "\\users\\inferno";
	Rune	rootname[NAMELEN] = L"root";
static	Qid	rootqid;
static	User	*fsnone;
static	User	*fsuser;
static	Rune	*ntsrv;
static	int	usesec;
static	int	checksec;
static	int	isserver;
static	uchar	isntfrog[256];

/*
 * these lan manager functions are not supplied
 * on windows95, so we have to load the dll by hand
 */
static struct {
	NET_API_STATUS (NET_API_FUNCTION *UserGetLocalGroups)(
		LPWSTR servername,
		LPWSTR username,
		DWORD level,
		DWORD flags,
		LPBYTE *bufptr,
		DWORD prefmaxlen,
		LPDWORD entriesread,
		LPDWORD totalentries);
	NET_API_STATUS (NET_API_FUNCTION *UserGetGroups)(
		LPWSTR servername,
		LPWSTR username,
		DWORD level,
		LPBYTE *bufptr,
		DWORD prefmaxlen,
		LPDWORD entriesread,
		LPDWORD totalentries);
	NET_API_STATUS (NET_API_FUNCTION *GetAnyDCName)(
		LPCWSTR ServerName,
		LPCWSTR DomainName,
		LPBYTE *Buffer);
	NET_API_STATUS (NET_API_FUNCTION *ApiBufferFree)(LPVOID Buffer);
} net;

extern	int		nth2fd(HANDLE);
extern	HANDLE		ntfd2h(int);
static	int		fsisroot(Chan*);
static	int		okelem(char*, int);
static	int		fsexist(char*, Qid*);
static	char*		fspath(Path*, char*, char*);
static	long		fsdirread(Chan*, uchar*, int, ulong);
static	ulong		fsqidpath(char*);
static	int		fsomode(int);
static	void		fsdirset(char*, WIN32_FIND_DATA*, char*, int);
static	void		fssettime(char*, long, long);
static	long		unixtime(FILETIME);
static	FILETIME	wintime(ulong);
static	void		secinit(void);
static	int		secstat(Dir*, char*);
static	void		seccheck(char*, ulong);
static	int		sechasperm(char*, ulong);
static	SECURITY_DESCRIPTOR* secsd(char*, char[SD_ROCK]);
static	int		secsdhasperm(SECURITY_DESCRIPTOR*, ulong);
static	int		secsdstat(SECURITY_DESCRIPTOR*, Stat*);
static	SECURITY_DESCRIPTOR* secmksd(char[SD_ROCK], Stat*, int);
static	SID		*dupsid(SID*);
static	int		ismembersid(Rune*, User*, SID*);
static	int		ismember(User*, User*);
static	User		*sidtouser(Rune*, SID*);
static	User		*domnametouser(Rune*, Rune*, Rune*);
static	User		*nametouser(Rune*, Rune*);
static	User		*unametouser(Rune*, char*);
static	void		addgroups(User*, int);
static	User		*mkuser(SID*, int, Rune*, Rune*);
static	Rune		*domsrv(Rune *, Rune[MAX_PATH]);
static	Rune		*filesrv(char*);
static	int		fsacls(char*);
static	User		*secuser(void);

	int		runeslen(Rune*);
	Rune*		runesdup(Rune*);
	Rune*		utftorunes(Rune*, char*, int);
	char*		runestoutf(char*, Rune*, int);
	int		runescmp(Rune*, Rune*);

/*
 * this gets called to set up the environment when we switch users
 */
void
setid(char *name)
{
	User *u;

	strncpy(up->env->user, name, NAMELEN-1);
	up->env->user[NAMELEN-1] = '\0';

	if(!usesec)
		return;

	u = unametouser(ntsrv, up->env->user);
	if(u == nil)
		u = fsnone;
	else {
		qlock(&u->lk);
		addgroups(u, 1);
		qunlock(&u->lk);
	}
	if(u == nil)
		panic("setid: user nil\n");

	up->env->ui = u;
}

void
fsinit(void)
{
	int n, isvol;
	ulong attr;
	char *p, *last, tmp[MAXROOT];

	isntfrog['/'] = 1;
	isntfrog['\\'] = 1;
	isntfrog[':'] = 1;
	isntfrog['*'] = 1;
	isntfrog['?'] = 1;
	isntfrog['"'] = 1;
	isntfrog['<'] = 1;
	isntfrog['>'] = 1;

	/*
	 * vet the root
	 */
	strcpy(tmp, rootdir);
	for(p = tmp; *p; p++) {
		if(*p == '/')
			*p = '\\';
		if(*p < 32 || (isntfrog[*p] && *p != '\\' && *p != ':'))
			panic("illegal root path");
	}

	if(tmp[0] != 0 && tmp[1] == ':') {
		if(tmp[2] == 0) {
			tmp[2] = '\\';
			tmp[3] = 0;
		}
		else if(tmp[2] != '\\') {
			/* don't allow c:foo - only c:\foo */
			panic("illegal root path");
		}
	}
	rootdir[0] = '\0';
	n = GetFullPathName(tmp, MAXROOT, rootdir, &last);
	if(n >= MAXROOT || n == 0)
		panic("illegal root path");

	/* get rid of trailling \ */
	while(rootdir[n-1] == '\\') {
		if(n <= 2) {
			panic("illegal root path");
		}
		rootdir[--n] = '\0';
	}

	isvol = 0;
	if(rootdir[1] == ':' && rootdir[2] == '\0')
		isvol = 1;
	else if(rootdir[0] == '\\' && rootdir[1] == '\\') {
		p = strchr(&rootdir[2], '\\');
		if(p == nil)
			panic("inferno root can't be a server");
		isvol = strchr(p+1, '\\') == nil;
	}

	if(strchr(rootdir, '\\') == nil)
		strcat(rootdir, "\\.");
	attr = GetFileAttributes(rootdir);
	if(attr == 0xFFFFFFFF)
		panic("root path '%s' does not exist", rootdir);
	rootqid.path = fsqidpath(rootdir);
	if(attr & FILE_ATTRIBUTE_DIRECTORY)
		rootqid.path |= CHDIR;
	rootdir[n] = '\0';

	rootqid.vers = time(0);

	/*
	 * set up for nt file security checking
	 */
	ntsrv = filesrv(rootdir);
	usesec = PlatformId == VER_PLATFORM_WIN32_NT;
	if(usesec && !fsacls(rootdir))
		usesec = 0;
	if(usesec)
		secinit();
	checksec = usesec && isserver;
}

Chan*
fsattach(void *spec)
{
	Chan *c;

	c = devattach('U', spec);
	c->qid = rootqid;
	c->dev = 0;

	return c;
}

Chan*
fsclone(Chan *c, Chan *nc)
{
	return devclone(c, nc);
}

int
fswalk(Chan *c, char *name)
{
	Path *op;
	char path[MAX_PATH], *p;

	isdir(c);
	if(!okelem(name, 0))
		return 0;

	p = fspath(c->path, name, path);
	if(checksec) {
		*p = '\0';
		if(!sechasperm(path, XMODE))
			return 0;
		*p = '\\';
	}

	if(strcmp(name, "..") == 0) {
		op = c->path;
		if(op->parent == nil)
			return 1;
		if(op->parent->parent == nil)
			c->qid = rootqid;
		else {
			fspath(op->parent, 0, path);
			if(!fsexist(path, &c->qid))
				return 0;
		}
		c->path = ptenter(&syspt, op, name);
		decref(&op->r);
		return 1;
	}

	if(!fsexist(path, &c->qid))
		return 0;

	op = c->path;
	c->path = ptenter(&syspt, op, name);
	decref(&op->r);

	return 1;
}

Chan*
fsopen(Chan *c, int mode)
{
	HANDLE h;
	int m, isdir, aflag, cflag;
	char path[MAX_PATH], ebuf[ERRLEN];

	isdir = c->qid.path & CHDIR;
	if(isdir && mode != OREAD)
		error(Eperm);

	fspath(c->path, 0, path);

	if(checksec) {
		switch(mode & (OTRUNC|3)) {
		case OREAD:
			seccheck(path, RMODE);
			break;
		case OWRITE:
		case OWRITE|OTRUNC:
			seccheck(path, WMODE);
			break;
		case ORDWR:
		case ORDWR|OTRUNC:
		case OREAD|OTRUNC:
			seccheck(path, RMODE|WMODE);
			break;
		case OEXEC:
			seccheck(path, XMODE);
			break;
		default:
			error(Ebadarg);
		}
	}

	c->mode = openmode(mode);
	if(isdir)
		c->u.uif.fd = nth2fd(INVALID_HANDLE_VALUE);
	else {
		m = fsomode(mode & 3);
		cflag = OPEN_EXISTING;
		if(mode & OTRUNC)
			cflag = TRUNCATE_EXISTING;
		aflag = FILE_FLAG_RANDOM_ACCESS;
		if(mode & ORCLOSE)
			aflag |= FILE_FLAG_DELETE_ON_CLOSE;

		h = CreateFile(path, m, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, cflag, aflag, 0);
		if(h == INVALID_HANDLE_VALUE) {
			oserrstr(ebuf);
			error(ebuf);
		}
		c->u.uif.fd = nth2fd(h);
	}

	c->offset = 0;
	c->u.uif.offset = 0;
	c->flag |= COPEN;
	return c;
}

void
fscreate(Chan *c, char *name, int mode, ulong perm)
{
	Stat st;
	Path *op;
	HANDLE h;
	int m, aflag;
	SECURITY_ATTRIBUTES sa;
	SECURITY_DESCRIPTOR *sd;
	BY_HANDLE_FILE_INFORMATION hi;
	char *p, path[MAX_PATH], ebuf[ERRLEN], sdrock[SD_ROCK];

	if(!okelem(name, 1))
		error(Efilename);

	m = fsomode(mode & 3);
	p = fspath(c->path, name, path);

	sd = nil;
	if(usesec) {
		*p = '\0';
		sd = secsd(path, sdrock);
		*p = '\\';
		if(sd == nil){
			oserrstr(ebuf);
			error(ebuf);
		}
		if(checksec && !secsdhasperm(sd, WMODE)
		|| !secsdstat(sd, &st)){
			if(sd != (void*)sdrock)
				free(sd);
			error(Eperm);
		}
		if(sd != (void*)sdrock)
			free(sd);
		if(perm & CHDIR)
			st.mode = (perm & ~0777) | (st.mode & perm & 0777);
		else
			st.mode = (perm & ~0666) | (st.mode & perm & 0666);
		st.owner = up->env->ui;
		if(!isserver)
			st.owner = fsuser;
		sd = secmksd(sdrock, &st, perm & CHDIR);
		if(sd == nil){
			oserrstr(ebuf);
			error(ebuf);
		}
	}
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = sd;
	sa.bInheritHandle = 0;

	if(perm & CHDIR) {
		if(mode != OREAD)
			error(Eperm);
		if(!CreateDirectory(path, &sa) || !fsexist(path, &c->qid)) {
			oserrstr(ebuf);
			error(ebuf);
		}
		c->u.uif.fd = nth2fd(INVALID_HANDLE_VALUE);
	}
	else {
		aflag = 0;
		if(mode & ORCLOSE)
			aflag = FILE_FLAG_DELETE_ON_CLOSE;
		h = CreateFile(path, m, FILE_SHARE_READ|FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, aflag, 0);
		if(h == INVALID_HANDLE_VALUE) {
			oserrstr(ebuf);
			error(ebuf);
		}
		c->u.uif.fd = nth2fd(h);
		c->qid.path = fsqidpath(path);
		c->qid.vers = 0;
		if(GetFileInformationByHandle(h, &hi))
			c->qid.vers = unixtime(hi.ftLastWriteTime);
	}

	c->mode = openmode(mode);
	c->offset = 0;
	c->u.uif.offset = 0;
	c->flag |= COPEN;
	op = c->path;
	c->path = ptenter(&syspt, op, name);
	decref(&op->r);
}

void
fsclose(Chan *c)
{
	HANDLE h;

	if((c->flag & COPEN) == 0)
		return;

	h = ntfd2h(c->u.uif.fd);
	if(h == INVALID_HANDLE_VALUE)
		return;
	if(c->qid.path & CHDIR)
		FindClose(h);
	else
		CloseHandle(h);
}

long
fsread(Chan *c, void *va, long n, ulong offset)
{
	DWORD n2;
	HANDLE h;
	char ebuf[ERRLEN];

	qlock(&c->u.uif.oq);
	if(waserror()){
		qunlock(&c->u.uif.oq);
		nexterror();
	}
	if(c->qid.path & CHDIR) {
		n2 = fsdirread(c, va, n, offset);
	}
	else {
		h = ntfd2h(c->u.uif.fd);
		if(c->u.uif.offset != offset){
			if(SetFilePointer(h, offset, NULL, FILE_BEGIN) != offset){
				oserrstr(ebuf);
				error(ebuf);
			}
		}
		if(!ReadFile(h, va, n, &n2, NULL)) {
			oserrstr(ebuf);
			error(ebuf);
		}
		c->u.uif.offset += n2;
	}
	qunlock(&c->u.uif.oq);
	poperror();
	return n2;
}

long
fswrite(Chan *c, void *va, long n, ulong offset)
{
	DWORD n2;
	HANDLE h;
	char ebuf[ERRLEN];

	qlock(&c->u.uif.oq);
	if(waserror()){
		qunlock(&c->u.uif.oq);
		nexterror();
	}
	h = ntfd2h(c->u.uif.fd);
	if(c->u.uif.offset != offset){
		if(SetFilePointer(h, offset, NULL, FILE_BEGIN) != offset){
			oserrstr(ebuf);
			error(ebuf);
		}
	}
	if(!WriteFile(h, va, n, &n2, NULL)) {
		oserrstr(ebuf);
		error(ebuf);
	}
	c->u.uif.offset += n2;
	qunlock(&c->u.uif.oq);
	poperror();
	return n2;
}

void
fsstat(Chan *c, char *buf)
{
	HANDLE h;
	WIN32_FIND_DATA data;
	char path[MAX_PATH], ebuf[ERRLEN];

	/*
	 * have to fake up a data for volumes like
	 * c: and \\server\share since you can't FindFirstFile them
	 */
	if(fsisroot(c)){
		strcpy(path, rootdir);
		if(strchr(path, '\\') == nil)
			strcat(path, "\\.");
		data.dwFileAttributes = GetFileAttributes(path);
		if(data.dwFileAttributes == 0xffffffff){
			oserrstr(ebuf);
			error(ebuf);
		}
		data.ftCreationTime =
		data.ftLastAccessTime =
		data.ftLastWriteTime = wintime(time(0));
		data.nFileSizeHigh = 0;
		data.nFileSizeLow = 0;
		strcpy(data.cFileName, ".");
	}else{
		fspath(c->path, 0, path);
		h = FindFirstFile(path, &data);
		if(h == INVALID_HANDLE_VALUE) {
			oserrstr(ebuf);
			error(ebuf);
		}
		FindClose(h);
	}

	fsdirset(buf, &data, path, c->dev);
}

void
fswstat(Chan *c, char *buf)
{
	int wsd;
	Dir dir;
	Stat st;
	Path *ph;
	HANDLE h;
	ulong attr;
	User *ou, *gu;
	WIN32_FIND_DATA data;
	SECURITY_DESCRIPTOR *sd;
	char *last, sdrock[SD_ROCK], path[MAX_PATH], newpath[MAX_PATH], ebuf[ERRLEN];

	convM2D(buf, &dir);
	last = fspath(c->path, 0, path);

	if(fsisroot(c)){
		data.ftLastAccessTime = wintime(dir.atime);
		data.ftLastWriteTime = wintime(dir.mtime);
		strcpy(data.cFileName, ".");
	}else{
		h = FindFirstFile(path, &data);
		if(h == INVALID_HANDLE_VALUE) {
			oserrstr(ebuf);
			error(ebuf);
		}
		FindClose(h);
	}

	wsd = 0;
	ou = nil;
	gu = nil;
	if(usesec) {
		if(checksec && up->env->ui == fsnone)
			error(Eperm);

		/*
		 * find new owner and group
		 */
		ou = unametouser(ntsrv, dir.uid);
		if(ou == nil){
			oserrstr(ebuf);
			error(ebuf);
		}
		gu = unametouser(ntsrv, dir.gid);
		if(gu == nil){
			if(strcmp(dir.gid, "unknown") != 0
			&& strcmp(dir.gid, "deleted") != 0){
				oserrstr(ebuf);
				error(ebuf);
			}
			gu = ou;
		}

		/*
		 * find old stat info
		 */
		sd = secsd(path, sdrock);
		if(sd == nil || !secsdstat(sd, &st)){
			if(sd != nil && sd != (void*)sdrock)
				free(sd);
			oserrstr(ebuf);
			error(ebuf);
		}
		if(sd != (void*)sdrock)
			free(sd);

		/*
		 * permission rules:
		 * if none, can't do anything
		 * chown => no way
		 * chgrp => current owner or group, and in new group
		 * mode/time => owner or in either group
		 * rename => write in parent
		 */
		if(checksec && st.owner != ou)
			error(Eperm);

		if(st.group != gu){
			if(checksec
			&&(!ismember(up->env->ui, ou) && !ismember(up->env->ui, gu)
			|| !ismember(up->env->ui, st.group)))
				error(Eperm);
			wsd = 1;
		}

		if(unixtime(data.ftLastAccessTime) != dir.atime
		|| unixtime(data.ftLastWriteTime) != dir.mtime
		|| st.mode != dir.mode){
			if(checksec
			&& !ismember(up->env->ui, ou)
			&& !ismember(up->env->ui, gu)
			&& !ismember(up->env->ui, st.group))
				error(Eperm);
			if(st.mode != dir.mode)
				wsd = 1;
		}
	}
	if(strcmp(dir.name, data.cFileName) != 0){
		if(!okelem(dir.name, 1))
			error(Efilename);
		ph = ptenter(&syspt, c->path->parent, dir.name);
		fspath(ph, 0, newpath);
		decref(&ph->r);
		if(GetFileAttributes(newpath) != 0xffffffff)
			error("file already exists");
		if(fsisroot(c))
			error(Eperm);
		if(checksec){
			*last = '\0';
			seccheck(path, WMODE);
			*last = '\\';
		}
	}

	if(unixtime(data.ftLastAccessTime) != dir.atime
	|| unixtime(data.ftLastWriteTime) != dir.mtime)
		fssettime(path, dir.atime, dir.mtime);

	attr = data.dwFileAttributes;
	if(dir.mode & 0222)
		attr &= ~FILE_ATTRIBUTE_READONLY;
	else
		attr |= FILE_ATTRIBUTE_READONLY;
	if(!fsisroot(c)
	&& attr != data.dwFileAttributes
	&& (attr & FILE_ATTRIBUTE_READONLY))
		SetFileAttributes(path, attr);

	if(usesec && wsd){
		st.owner = ou;
		st.group = gu;
		st.mode = dir.mode;
		sd = secmksd(sdrock, &st, data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		if(sd == nil || !SetFileSecurity(path, DACL_SECURITY_INFORMATION, sd)){
			oserrstr(ebuf);
			error(ebuf);
		}
	}

	if(!fsisroot(c)
	&& attr != data.dwFileAttributes
	&& !(attr & FILE_ATTRIBUTE_READONLY))
		SetFileAttributes(path, attr);

	/* do last so path is valid throughout */
	if(strcmp(dir.name, data.cFileName) != 0) {
		ph = ptenter(&syspt, c->path->parent, dir.name);
		fspath(ph, 0, newpath);
		/*
		 * Cannot move file if a file has it opened
		 * At least this user has the file opened, so close the file.
		 */
		CloseHandle(ntfd2h(c->u.uif.fd));
		if(!MoveFile(path, newpath)) {
			decref(&ph->r);
			oserrstr(ebuf);
			error(ebuf);
		} else {
			int	aflag;
			SECURITY_ATTRIBUTES sa;
			
			/* The move succeeded, so open new file to maintain handle */
			sa.nLength = sizeof(sa);
			sa.lpSecurityDescriptor = sd;
			sa.bInheritHandle = 0;
			if(c->mode & ORCLOSE)
				aflag = FILE_FLAG_DELETE_ON_CLOSE;
			h = CreateFile(newpath, fsomode(c->mode & 0x3), FILE_SHARE_READ|FILE_SHARE_WRITE, &sa, OPEN_EXISTING, aflag, 0);
			if(h == INVALID_HANDLE_VALUE) {
				decref(&ph->r);
				oserrstr(ebuf);
				error(ebuf);
			}
			c->u.uif.fd = nth2fd(h);
		}
		decref(&c->path->r);
		c->path = ph;
	}
}

void
fsremove(Chan *c)
{
	int n;
	char *p, path[MAX_PATH], ebuf[ERRLEN];

	if(fsisroot(c))
		error(Eperm);
	p = fspath(c->path, 0, path);
	if(checksec){
		*p = '\0';
		seccheck(path, WMODE);
		*p = '\\';
	}
	if(c->qid.path & CHDIR)
		n = RemoveDirectory(path);
	else
		n = DeleteFile(path);
	if(!n) {
		oserrstr(ebuf);
		error(ebuf);
	}
}

/*
 * check elem for illegal characters /\:*?"<>
 * ... and relatives are also disallowed,
 * since they specify grandparents, which we
 * are not prepared to handle
 */
static int
okelem(char *elem, int nodots)
{
	int c, dots;

	dots = 0;
	while((c = *(uchar*)elem) != 0){
		if(isntfrog[c])
			return 0;
		if(c == '.')
			dots++;
		else
			dots = -NAMELEN;
		elem++;
	}
	if(nodots)
		return dots <= 0;
	return dots <= 2;
}

static int
fsisroot(Chan *c)
{
	return c->path->parent == nil;
}

static char*
fspath(Path *p, char *ext, char *path)
{
	int i, n, last;
	char *comp[MAXCOMP];

	strcpy(path, rootdir);
	if(p == 0) {
                if(ext) {
			strcat(path, "\\");
			strcat(path, ext);
		}
		return path;
	}
	i = strlen(rootdir);

	n = 0;
	if(ext)
		comp[n++] = ext;
	while(p->parent) {
		comp[n++] = p->elem;
		p = p->parent;
	}

	if(n == 0)
		return nil;

	last = i;
	while(n) {
		last = i;
                path[i++] = '\\';
		strcpy(path+i, comp[--n]);
		i += strlen(comp[n]);
	}
	path[i] = '\0';
	return &path[last];
}

static int
fsdirbadentry(WIN32_FIND_DATA *data)
{
	char *s;

	s = data->cFileName;
	if(s[0] == 0)
		return 1;
	if(s[0] == '.' && (s[1] == 0 || s[1] == '.' && s[2] == 0))
		return 1;

	return 0;
}

static int
fsdirnext(HANDLE h, WIN32_FIND_DATA *data)
{
	for(;;) {
		if(!FindNextFile(h, data))
			return 0;
		if(!fsdirbadentry(data))
			break;
	}

	return 1;
}

static long
fsdirread(Chan *c, uchar *va, int n, ulong offset)
{
	int i;
	HANDLE h;
	ulong cur;
	char path[MAX_PATH], *p;
	WIN32_FIND_DATA data;

	n = (n/DIRLEN)*DIRLEN;
	if(n == 0)
		return 0;
	cur = c->u.uif.offset;
	h = ntfd2h(c->u.uif.fd);
	p = fspath(c->path, "*.*", path);
	p++;
	if(offset != cur || cur == 0) {
		cur = 0;
		FindClose(h);
		h = FindFirstFile(path, &data);
		c->u.uif.fd = nth2fd(h);
		c->u.uif.offset = 0;
		if(h == INVALID_HANDLE_VALUE)
			return 0;
		if(!fsdirbadentry(&data))
			cur += DIRLEN;
		for(; cur <= offset; cur += DIRLEN){
			if(!fsdirnext(h, &data)) {
				c->u.uif.offset = cur;
				return 0;
			}
		}
	} else if(!fsdirnext(h, &data))
		return 0;

	i = 0;
	do {
		strncpy(p, data.cFileName, &path[MAX_PATH]-p);
		path[MAX_PATH-1] = '\0';
		fsdirset(va+i, &data, path, c->dev);
		i += DIRLEN;
		c->u.uif.offset += DIRLEN;
	} while(i < n && fsdirnext(h, &data));

	return i;
}

static ulong
fsqidpath(char *p)
{
	ulong h;

	h = 0;
	while(*p != '\0')
		h = h * 19 ^ *p++;
	return h & ~CHDIR;
}

/*
 * there are other ways to figure out
 * the attributes and times for a file.
 * perhaps they are faster
 */
static int
fsexist(char *p, Qid *q)
{
	HANDLE h;
	WIN32_FIND_DATA data;

	h = FindFirstFile(p, &data);
	if(h == INVALID_HANDLE_VALUE)
		return 0;
	FindClose(h);

	q->path = fsqidpath(p);

	if(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		q->path |= CHDIR;

	q->vers = unixtime(data.ftLastWriteTime);

	return 1;
}

static void
fsdirset(char *edir, WIN32_FIND_DATA *data, char *path, int dev)
{
	Dir dir;
	char ebuf[ERRLEN];

	strncpy(dir.name, data->cFileName, NAMELEN);
	dir.name[NAMELEN-1] = 0;
	dir.qid.path = fsqidpath(path);
	dir.atime = unixtime(data->ftLastAccessTime);
	dir.mtime = unixtime(data->ftLastWriteTime);
	dir.qid.vers = dir.mtime;
	dir.length = data->nFileSizeLow;
	dir.type = 'U';
	dir.dev = dev;

	if(!usesec){
		/* no NT security so make something up */
		strncpy(dir.uid, "Everyone", NAMELEN);
		strncpy(dir.gid, "Everyone", NAMELEN);
		dir.mode = 0777;
	}else if(!secstat(&dir, path)){
		oserrstr(ebuf);
		error(ebuf);
	}

	if(data->dwFileAttributes & FILE_ATTRIBUTE_READONLY)
		dir.mode &= ~0222;
	if(data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
		dir.qid.path |= CHDIR;
		dir.mode |= CHDIR;
	}

	convD2M(&dir, edir);
}

static void
fssettime(char *path, long at, long mt)
{
	HANDLE h;
	FILETIME atime, mtime;
	char ebuf[ERRLEN];

	h = CreateFile(path, GENERIC_WRITE,
		0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if(h == INVALID_HANDLE_VALUE)
		return;
	mtime = wintime(mt);
	atime = wintime(at);
	if(!SetFileTime(h, 0, &atime, &mtime)) {
		oserrstr(ebuf);
		CloseHandle(h);
		error(ebuf);
	}
	CloseHandle(h);
}

static int
fsomode(int m)
{
	switch(m & 0x3) {
	case OREAD:
	case OEXEC:
		return GENERIC_READ;
	case OWRITE:
		return GENERIC_WRITE;
	case ORDWR:
		return GENERIC_READ|GENERIC_WRITE;
	}
	error(Ebadarg);
	return 0;
}

static long
unixtime(FILETIME ft)
{
	vlong t;

	t = (vlong)ft.dwLowDateTime + ((vlong)ft.dwHighDateTime<<32);
	t -= (vlong)10000000*134774*24*60*60;

	return (long)(t/10000000);
}

static FILETIME
wintime(ulong t)
{
	FILETIME ft;
	vlong vt;

	vt = (vlong)t*10000000+(vlong)10000000*134774*24*60*60;

	ft.dwLowDateTime = vt;
	ft.dwHighDateTime = vt>>32;

	return ft;
}

/*
 * the sec routines manage file permissions for nt.
 * nt files have an associated security descriptor,
 * which has in it an owner, a group,
 * and a discretionary acces control list, or acl,
 * which specifies the permissions for the file.
 *
 * the strategy for mapping between inferno owner,
 * group, other, and mode and nt file security is:
 *
 *	inferno owner == nt file owner
 *	inferno other == nt Everyone
 *	inferno group == first non-owner,
 *			non-Everyone user given in the acl,
 *			or the owner if there is no such user.
 * we examine the entire acl when check for permissions,
 * but only report a subset.
 *
 * when we write an acl, we also give all permissions to
 * the special user root, who is supposed to run emu in sever mode.
 */
static void
secinit(void)
{
	HMODULE lib;
	HANDLE token;
	TOKEN_PRIVILEGES *priv;
	char privrock[sizeof(TOKEN_PRIVILEGES) + 1*sizeof(LUID_AND_ATTRIBUTES)];
	SID_IDENTIFIER_AUTHORITY id = SECURITY_CREATOR_SID_AUTHORITY;
	SID_IDENTIFIER_AUTHORITY wid = SECURITY_WORLD_SID_AUTHORITY;
	SID_IDENTIFIER_AUTHORITY ntid = SECURITY_NT_AUTHORITY;

	lib = LoadLibrary("netapi32");
	if(lib == 0) {
		usesec = 0;
		return;
	}

	net.UserGetGroups = (void*)GetProcAddress(lib, "NetUserGetGroups");
	if(net.UserGetGroups == 0)
		panic("bad netapi32 library");
	net.UserGetLocalGroups = (void*)GetProcAddress(lib, "NetUserGetLocalGroups");
	if(net.UserGetLocalGroups == 0)
		panic("bad netapi32 library");
	net.GetAnyDCName = (void*)GetProcAddress(lib, "NetGetAnyDCName");
	if(net.GetAnyDCName == 0)
		panic("bad netapi32 library");
	net.ApiBufferFree = (void*)GetProcAddress(lib, "NetApiBufferFree");
	if(net.ApiBufferFree == 0)
		panic("bad netapi32 library");

	if(!AllocateAndInitializeSid(&id, 1,
		SECURITY_CREATOR_OWNER_RID,
		1, 2, 3, 4, 5, 6, 7, &creatorowner)
	|| !AllocateAndInitializeSid(&id, 1,
		SECURITY_CREATOR_GROUP_RID,
		1, 2, 3, 4, 5, 6, 7, &creatorgroup)
	|| !AllocateAndInitializeSid(&wid, 1,
		SECURITY_WORLD_RID,
		1, 2, 3, 4, 5, 6, 7, &everyone)
	|| !AllocateAndInitializeSid(&ntid, 1,
		0,
		1, 2, 3, 4, 5, 6, 7, &ntignore))
		panic("can't initialize well-known sids");

	fsnone = sidtouser(ntsrv, everyone);
	if(fsnone == nil)
		panic("can't make none user");

	/*
	 * see if we are running as the emu server user
	 * if so, set up SE_RESTORE_NAME privilege,
	 * which allows setting the owner field in a security descriptor.
	 * other interesting privileges are SE_TAKE_OWNERSHIP_NAME,
	 * which enables changing the ownership of a file to yourself
	 * regardless of the permissions on the file, SE_BACKUP_NAME,
	 * which enables reading any files regardless of permission,
	 * and SE_CHANGE_NOTIFY_NAME, which enables walking through
	 * directories without X permission.
	 * SE_RESTORE_NAME and SE_BACKUP_NAME together allow writing
	 * and reading any file data, regardless of permission,
	 * if the file is opened with FILE_BACKUP_SEMANTICS.
	 */
	isserver = 0;
	fsuser = secuser();
	if(fsuser == nil)
		fsuser = fsnone;
	else if(runescmp(fsuser->name, rootname) == 0
	     && OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token)){
		priv = (TOKEN_PRIVILEGES*)privrock;
		priv->PrivilegeCount = 1;
		priv->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if(LookupPrivilegeValue(NULL, SE_RESTORE_NAME, &priv->Privileges[0].Luid)
		&& AdjustTokenPrivileges(token, 0, priv, 0, NULL, NULL))
			isserver = 1;
		CloseHandle(token);
	}
}

/*
 * get the User for the executing process
 */
static User*
secuser(void)
{
	DWORD need;
	HANDLE token;
	TOKEN_USER *tu;
	char turock[sizeof(TOKEN_USER) + MAX_SID];

	if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
		return nil;

	tu = (TOKEN_USER*)turock;
	if(!GetTokenInformation(token, TokenUser, tu, sizeof(turock), &need)){
		CloseHandle(token);
		return nil;
	}
	CloseHandle(token);
	return sidtouser(nil, tu->User.Sid);
}

static int
secstat(Dir *dir, char *file)
{
	int ok;
	Stat st;
	char sdrock[SD_ROCK];
	SECURITY_DESCRIPTOR *sd;

	sd = secsd(file, sdrock);
	if(sd == nil){
		if(GetLastError() == ERROR_ACCESS_DENIED){
			strncpy(dir->uid, "unknown", NAMELEN);
			strncpy(dir->gid, "unknown", NAMELEN);
			dir->mode = 0;
			return 1;
		}
		return 0;
	}
	ok = secsdstat(sd, &st);
	if(sd != (void*)sdrock)
		free(sd);
	if(ok){
		dir->mode = st.mode;
		runestoutf(dir->uid, st.owner->name, nelem(dir->uid));
		runestoutf(dir->gid, st.group->name, nelem(dir->gid));
	}
	return ok;
}

/*
 * verify that u had access to file
 */
static void
seccheck(char *file, ulong access)
{
	if(!sechasperm(file, access))
		error(Eperm);
}

static int
sechasperm(char *file, ulong access)
{
	int ok;
	char sdrock[SD_ROCK];
	SECURITY_DESCRIPTOR *sd;

	/*
	 * only really needs dacl info
	 */
	sd = secsd(file, sdrock);
	if(sd == nil)
		return 0;
	ok = secsdhasperm(sd, access);
	if(sd != (void*)sdrock)
		free(sd);
	return ok;
}

static SECURITY_DESCRIPTOR*
secsd(char *file, char sdrock[SD_ROCK])
{
	DWORD need;
	SECURITY_DESCRIPTOR *sd;
	char *path, pathrock[6];

	path = file;
	if(path[0] != '\0' && path[1] == ':' && path[2] == '\0'){
		path = pathrock;
		strcpy(path, "?:\\.");
		path[0] = file[0];
	}
	sd = (SECURITY_DESCRIPTOR*)sdrock;
	need = 0;
	if(GetFileSecurity(path, OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION, sd, SD_ROCK, &need))
		return sd;
	 if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		return nil;
	sd = malloc(need);
	if(sd == nil)
		error(Enomem);
	if(GetFileSecurity(path, OWNER_SECURITY_INFORMATION|DACL_SECURITY_INFORMATION, sd, need, &need))
		return sd;

	free(sd);
	return nil;
}

static int
secsdstat(SECURITY_DESCRIPTOR *sd, Stat *st)
{
	ACL *acl;
	BOOL hasacl, b;
	ACE_HEADER *aceh;
	User *owner, *group;
	SID *sid, *osid, *gsid;
	ACCESS_ALLOWED_ACE *ace;
	int i, allow, deny, *p, m;
	ACL_SIZE_INFORMATION size;

	st->mode = 0;

	osid = nil;
	gsid = nil;
	if(!GetSecurityDescriptorOwner(sd, &osid, &b)
	|| !GetSecurityDescriptorDacl(sd, &hasacl, &acl, &b))
		return -1;

	if(acl == 0)
		size.AceCount = 0;
	else if(!GetAclInformation(acl, &size, sizeof(size), AclSizeInformation))
		return -1;

	/*
	 * first pass through acl finds group
	 */
	for(i = 0; i < size.AceCount; i++){
		if(!GetAce(acl, i, &aceh))
			continue;
		if(aceh->AceFlags & INHERIT_ONLY_ACE)
			continue;

		if(aceh->AceType != ACCESS_ALLOWED_ACE_TYPE
		&& aceh->AceType != ACCESS_DENIED_ACE_TYPE)
			continue;

		ace = (ACCESS_ALLOWED_ACE*)aceh;
		sid = (SID*)&ace->SidStart;
		if(EqualSid(sid, creatorowner) || EqualSid(sid, creatorgroup))
			continue;

		if(EqualSid(sid, everyone))
			;
		else if(EqualSid(sid, osid))
			;
		else if(EqualPrefixSid(sid, ntignore))
			continue;		/* boring nt accounts */
		else{
			gsid = sid;
			break;
		}
	}
	if(gsid == nil)
		gsid = osid;

	owner = sidtouser(ntsrv, osid);
	group = sidtouser(ntsrv, gsid);
	if(owner == 0 || group == 0)
		return -1;

	/* no acl means full access */
	allow = 0;
	if(acl == 0)
		allow = 0777;
	deny = 0;
	for(i = 0; i < size.AceCount; i++){
		if(!GetAce(acl, i, &aceh))
			continue;
		if(aceh->AceFlags & INHERIT_ONLY_ACE)
			continue;

		if(aceh->AceType == ACCESS_ALLOWED_ACE_TYPE)
			p = &allow;
		else if(aceh->AceType == ACCESS_DENIED_ACE_TYPE)
			p = &deny;
		else
			continue;

		ace = (ACCESS_ALLOWED_ACE*)aceh;
		sid = (SID*)&ace->SidStart;
		if(EqualSid(sid, creatorowner) || EqualSid(sid, creatorgroup))
			continue;

		m = 0;
		if(ace->Mask & FILE_EXECUTE)
			m |= 1;
		if(ace->Mask & FILE_WRITE_DATA)
			m |= 2;
		if(ace->Mask & FILE_READ_DATA)
			m |= 4;

		if(ismembersid(ntsrv, owner, sid))
			*p |= (m << 6) & ~(allow|deny) & 0700;
		if(ismembersid(ntsrv, group, sid))
			*p |= (m << 3) & ~(allow|deny) & 0070;
		if(EqualSid(everyone, sid))
			*p |= m & ~(allow|deny) & 0007;
	}

	st->mode = allow & ~deny;
	st->owner = owner;
	st->group = group;
	return 1;
}

static int
secsdhasperm(SECURITY_DESCRIPTOR *sd, ulong access)
{
	User *u;
	ACL *acl;
	BOOL hasacl, b;
	ACE_HEADER *aceh;
	SID *sid, *osid, *gsid;
	int i, allow, deny, *p, m;
	ACCESS_ALLOWED_ACE *ace;
	ACL_SIZE_INFORMATION size;

	u = up->env->ui;
	allow = 0;
	deny = 0;
	osid = nil;
	gsid = nil;
	if(!GetSecurityDescriptorDacl(sd, &hasacl, &acl, &b))
		return 0;

	/* no acl means full access */
	if(acl == 0)
		return 1;
	if(!GetAclInformation(acl, &size, sizeof(size), AclSizeInformation))
		return 0;
	for(i = 0; i < size.AceCount; i++){
		if(!GetAce(acl, i, &aceh))
			continue;
		if(aceh->AceFlags & INHERIT_ONLY_ACE)
			continue;

		if(aceh->AceType == ACCESS_ALLOWED_ACE_TYPE)
			p = &allow;
		else if(aceh->AceType == ACCESS_DENIED_ACE_TYPE)
			p = &deny;
		else
			continue;

		ace = (ACCESS_ALLOWED_ACE*)aceh;
		sid = (SID*)&ace->SidStart;
		if(EqualSid(sid, creatorowner) || EqualSid(sid, creatorgroup))
			continue;

		m = ace->Mask;

		if(ismembersid(ntsrv, u, sid))
			*p |= m & ~(allow|deny);
	}

	allow &= ~deny;

	return (allow & access) == access;
}

static SECURITY_DESCRIPTOR*
secmksd(char *sdrock, Stat *st, int isdir)
{
	int m;
	ACL *dacl;
	ulong mode;
	ACE_HEADER *aceh;
	SECURITY_DESCRIPTOR *sd;

	sd = (SECURITY_DESCRIPTOR*)sdrock;
	dacl = (ACL*)(&sd[SECURITY_DESCRIPTOR_MIN_LENGTH]);
	if(!InitializeAcl(dacl, ACL_ROCK, ACL_REVISION2))
		return nil;

	mode = st->mode;
	if(st->owner == st->group){
		mode |= (mode >> 3) & 0070;
		mode |= (mode << 3) & 0700;
	}

	m = modetomask[(mode>>6) & 7];
	if(!AddAccessAllowedAce(dacl, ACL_REVISION2, m, st->owner->sid))
		return nil;

	if(isdir && !AddAccessAllowedAce(dacl, ACL_REVISION2, m, creatorowner))
		return nil;

	m = modetomask[(mode>>3) & 7];
	if(!AddAccessAllowedAce(dacl, ACL_REVISION2, m, st->group->sid))
		return nil;

	m = modetomask[(mode>>0) & 7];
	if(!AddAccessAllowedAce(dacl, ACL_REVISION2, m, everyone))
		return nil;

	if(isdir){
		/* hack to add inherit flags */
		if(!GetAce(dacl, 1, &aceh))
			return nil;
		aceh->AceFlags |= OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE;
		if(!GetAce(dacl, 2, &aceh))
			return nil;
		aceh->AceFlags |= OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE;
		if(!GetAce(dacl, 3, &aceh))
			return nil;
		aceh->AceFlags |= OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE;
	}

	/*
	 * allow server user to access any file
	 */
	if(isserver){
		if(!AddAccessAllowedAce(dacl, ACL_REVISION2, RMODE|WMODE|XMODE, fsuser->sid))
			return nil;
		if(isdir){
			if(!GetAce(dacl, 4, &aceh))
				return nil;
			aceh->AceFlags |= OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE;
		}
	}

	if(!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION))
		return nil;
	if(!SetSecurityDescriptorDacl(sd, 1, dacl, 0))
		return nil;
	if(isserver && !SetSecurityDescriptorOwner(sd, st->owner->sid, 0))
		return nil;
	return sd;
}

/*
 * the user manipulation routines
 * just make it easier to deal with user identities
 */
static User*
sidtouser(Rune *srv, SID *s)
{
	SID_NAME_USE type;
	Rune aname[100], dname[100];
	DWORD naname, ndname;
	User *u;

	qlock(&users.lk);
	for(u = users.u; u != 0; u = u->next)
		if(EqualSid(s, u->sid))
			break;
	qunlock(&users.lk);

	if(u != 0)
		return u;

	naname = sizeof(aname);
	ndname = sizeof(dname);

	if(!LookupAccountSidW(srv, s, aname, &naname, dname, &ndname, &type))
		return nil;
	return mkuser(s, type, aname, dname);
}

static User*
domnametouser(Rune *srv, Rune *name, Rune *dom)
{
	User *u;

	qlock(&users.lk);
	for(u = users.u; u != 0; u = u->next)
		if(runescmp(name, u->name) == 0 && runescmp(dom, u->dom) == 0)
			break;
	qunlock(&users.lk);
	if(u == 0)
		u = nametouser(srv, name);
	return u;
}

static User*
nametouser(Rune *srv, Rune *name)
{
	char sidrock[MAX_SID];
	SID *sid;
	SID_NAME_USE type;
	Rune dom[MAX_PATH];
	DWORD nsid, ndom;

	sid = (SID*)sidrock;
	nsid = sizeof(sidrock);
	ndom = sizeof(dom);
	if(!LookupAccountNameW(srv, name, sid, &nsid, dom, &ndom, &type))
		return nil;

	return mkuser(sid, type, name, dom);
}

/*
 * this mapping could be cached
 */
static User*
unametouser(Rune *srv, char *name)
{
	Rune rname[MAX_PATH];

	utftorunes(rname, name, MAX_PATH);
	return nametouser(srv, rname);
}

/*
 * make a user structure and add it to the global cache.
 */
static User*
mkuser(SID *sid, int type, Rune *name, Rune *dom)
{
	User *u;

	qlock(&users.lk);
	for(u = users.u; u != 0; u = u->next){
		if(EqualSid(sid, u->sid)){
			qunlock(&users.lk);
			return u;
		}
	}

	switch(type) {
	default:
		break;
	case SidTypeDeletedAccount:
		name = L"deleted";
		break;
	case SidTypeInvalid:
		name = L"invalid";
		break;
	case SidTypeUnknown:
		name = L"unknown";
		break;
	}

	u = malloc(sizeof(User));
	if(u == nil){
		qunlock(&users.lk);
		return 0;
	}
	u->next = nil;
	u->group = nil;
	u->sid = dupsid(sid);
	u->type = type;
	u->name = nil;
	if(name != nil)
		u->name = runesdup(name);
	u->dom = nil;
	if(dom != nil)
		u->dom = runesdup(dom);

	u->next = users.u;
	users.u = u;

	qunlock(&users.lk);
	return u;
}

/*
 * check if u is a member of gsid,
 * which might be a group.
 */
static int
ismembersid(Rune *srv, User *u, SID *gsid)
{
	User *g;

	if(EqualSid(u->sid, gsid))
		return 1;

	g = sidtouser(srv, gsid);
	if(g == 0)
		return 0;
	return ismember(u, g);
}

static int
ismember(User *u, User *g)
{
	Gmem *grps;

	if(EqualSid(u->sid, g->sid))
		return 1;

	if(EqualSid(g->sid, everyone))
		return 1;

	qlock(&u->lk);
	addgroups(u, 0);
	for(grps = u->group; grps != 0; grps = grps->next){
		if(EqualSid(grps->user->sid, g->sid)){
			qunlock(&u->lk);
			return 1;
		}
	}
	qunlock(&u->lk);
	return 0;
}

/*
 * find out what groups a user belongs to.
 * if force, throw out the old info and do it again.
 *
 * note that a global group is also know as a group,
 * and a local group is also know as an alias.
 * global groups can only contain users.
 * local groups can contain global groups or users.
 * this code finds all global groups to which a user belongs,
 * and all the local groups to which the user or a global group
 * containing the user belongs.
 */
static void
addgroups(User *u, int force)
{
	LOCALGROUP_USERS_INFO_0 *loc;
	GROUP_USERS_INFO_0 *grp;
	DWORD i, n, rem;
	User *gu;
	Gmem *g, *next;
	Rune *srv, srvrock[MAX_PATH];

	if(force){
		u->gotgroup = 0;
		for(g = u->group; g != nil; g = next){
			next = g->next;
			free(g);
		}
		u->group = nil;
	}
	if(u->gotgroup)
		return;
	u->gotgroup = 1;

	rem = 1;
	n = 0;
	srv = domsrv(u->dom, srvrock);
	while(rem != n){
		i = net.UserGetGroups(srv, u->name, 0,
			(BYTE**)&grp, 1024, &n, &rem);
		if(i != NERR_Success && i != ERROR_MORE_DATA)
			break;
		for(i = 0; i < n; i++){
			gu = domnametouser(srv, grp[i].grui0_name, u->dom);
			if(gu == 0)
				continue;
			g = malloc(sizeof(Gmem));
			if(g == nil)
				error(Enomem);
			g->user = gu;
			g->next = u->group;
			u->group = g;
		}
		net.ApiBufferFree(grp);
	}

	rem = 1;
	n = 0;
	while(rem != n){
		i = net.UserGetLocalGroups(srv, u->name, 0, LG_INCLUDE_INDIRECT,
			(BYTE**)&loc, 1024, &n, &rem);
		if(i != NERR_Success && i != ERROR_MORE_DATA)
			break;
		for(i = 0; i < n; i++){
			gu = domnametouser(srv, loc[i].lgrui0_name, u->dom);
			if(gu == NULL)
				continue;
			g = malloc(sizeof(Gmem));
			if(g == nil)
				error(Enomem);
			g->user = gu;
			g->next = u->group;
			u->group = g;
		}
		net.ApiBufferFree(loc);
	}
}

static SID*
dupsid(SID *sid)
{
	SID *nsid;
	int n;

	n = GetLengthSid(sid);
	nsid = malloc(n);
	if(nsid == nil || !CopySid(n, nsid, sid))
		panic("can't copy sid");
	return nsid;
}

/*
 * return the name of the server machine for file
 */
static Rune*
filesrv(char *file)
{
	int n;
	Rune *srv;
	char *p, vol[3], uni[MAX_PATH];

	/* assume file is a fully qualified name - X: or \\server */
	if(file[1] == ':') {
		vol[0] = file[0];
		vol[1] = file[1];
		vol[2] = 0;
		if(GetDriveType(vol) != DRIVE_REMOTE)
			return 0;
		n = sizeof(uni);
		if(WNetGetUniversalName(vol, UNIVERSAL_NAME_INFO_LEVEL, uni, &n) != NO_ERROR)
			return nil;
		file = ((UNIVERSAL_NAME_INFO*)uni)->lpUniversalName;
	}
	file += 2;
	p = strchr(file, '\\');
	if(p == 0)
		n = strlen(file);
	else
		n = p - file;
	if(n >= MAX_PATH)
		n = MAX_PATH-1;

	memmove(uni, file, n);
	uni[n] = '\0';

	srv = malloc((n + 1) * sizeof(Rune));
	if(srv == nil)
		panic("filesrv: no memory");
	utftorunes(srv, uni, n+1);
	return srv;
}

/*
 * does the file system support acls?
 */
static int
fsacls(char *file)
{
	char *p;
	DWORD flags;
	char path[MAX_PATH];

	/* assume file is a fully qualified name - X: or \\server */
	if(file[1] == ':') {
		path[0] = file[0];
		path[1] = file[1];
		path[2] = '\\';
		path[3] = 0;
	} else {
		strcpy(path, file);
		p = strchr(path+2, '\\');
		if(p == 0)
			return 0;
		p = strchr(p+1, '\\');
		if(p == 0)
			strcat(path, "\\");
		else
			p[1] = 0;
	}
	if(!GetVolumeInformation(path, NULL, 0, NULL, NULL, &flags, NULL, 0))
		return 0;

	return flags & FS_PERSISTENT_ACLS;
}

/*
 * given a domain, find out the server to ask about its users.
 * we just ask the local machine to do the translation,
 * so it might fail sometimes.  in those cases, we don't
 * trust the domain anyway, and vice versa, so it's not
 * clear what benifit we would gain by getting the answer "right".
 */
static Rune*
domsrv(Rune *dom, Rune srv[MAX_PATH])
{
	Rune *psrv;
	int n, r;

	if(dom[0] == 0)
		return nil;

	r = net.GetAnyDCName(NULL, dom, (LPBYTE*)&psrv);
	if(r == NERR_Success) {
		n = runeslen(psrv);
		if(n >= MAX_PATH)
			n = MAX_PATH-1;
		memmove(srv, psrv, n*sizeof(Rune));
		srv[n] = 0;
		net.ApiBufferFree(psrv);
		return srv;
	}

	return nil;
}

Rune*
runesdup(Rune *r)
{
	int n;
	Rune *s;

	n = runeslen(r) + 1;
	s = malloc(n * sizeof(Rune));
	if(s == nil)
		error(Enomem);
	memmove(s, r, n * sizeof(Rune));
	return s;
}

int
runeslen(Rune *r)
{
	int n;

	n = 0;
	while(*r++ != '\0')
		n++;
	return n;
}

char*
runestoutf(char *p, Rune *r, int nc)
{
	char *op, *ep;
	int n, c;

	op = p;
	ep = p + nc;
	while(c = *r++) {
		n = 1;
		if(c >= Runeself)
			n = runelen(c);
		if(p + n >= ep)
			break;
		if(c < Runeself)
			*p++ = c;
		else
			p += runetochar(p, r-1);
	}
	*p = '\0';
	return op;
}

Rune*
utftorunes(Rune *r, char *p, int nc)
{
	Rune *or, *er;

	or = r;
	er = r + nc;
	while(*p != '\0' && r + 1 < er)
		p += chartorune(r++, p);
	*r = '\0';
	return or;
}

int
runescmp(Rune *s1, Rune *s2)
{
	Rune r1, r2;

	for(;;) {
		r1 = *s1++;
		r2 = *s2++;
		if(r1 != r2) {
			if(r1 > r2)
				return 1;
			return -1;
		}
		if(r1 == 0)
			return 0;
	}
}

Dev fsdevtab = {
	'U',
	"fs",

	fsinit,
	fsattach,
	fsclone,
	fswalk,
	fsstat,
	fsopen,
	fscreate,
	fsclose,
	fsread,
	devbread,
	fswrite,
	devbwrite,
	fsremove,
	fswstat
};
