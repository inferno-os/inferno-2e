implement Dirmod;

include "sys.m";
	Dir, NAMELEN: import Sys;

include "draw.m";
include "styx.m";
include "dirmod.m";

# put these here until we agree changes to styx.m

convD2M(dir: ref Dir, a: array of byte): int
{
	O: con 3*Sys->NAMELEN;
	name2byte(a[0:], dir.name, Sys->NAMELEN);
	name2byte(a[Sys->NAMELEN:], dir.uid, Sys->NAMELEN);
	name2byte(a[2*Sys->NAMELEN:], dir.gid, Sys->NAMELEN);
	put4(a[O:], dir.qid.path);
	put4(a[O+4:], dir.qid.vers);
	put4(a[O+8:], dir.mode);
	put4(a[O+12:], dir.atime);
	put4(a[O+16:], dir.mtime);
	put4(a[O+20:], dir.length);
	put4(a[O+24:], 0);	# high-order word of length
	put2(a[O+28:], dir.dtype);
	put2(a[O+30:], dir.dev);
	return Styx->STATSZ;
}

put2(a: array of byte, v: int)
{
	a[0] = byte v;
	a[1] = byte (v >> 8);
}

put4(a: array of byte, v: int)
{
	a[0] = byte v;
	a[1] = byte (v >> 8);
	a[2] = byte (v >> 16);
	a[3] = byte (v >> 24);
}

name2byte(a: array of byte, s: string, n: int)
{
	b := array of byte s;
	l := len b;
	if(l > n)
		l = n;
	for(i := 0; i < l; i++)
		a[i] = b[i];
	while(i < n)
		a[i++] = byte 0;
}

convM2D(a: array of byte, dir: ref Dir): int
{
	O: con 3*Sys->NAMELEN;
	dir.name = byte2name(a[0:], Sys->NAMELEN);
	dir.uid = byte2name(a[Sys->NAMELEN:], Sys->NAMELEN);
	dir.gid = byte2name(a[2*Sys->NAMELEN:], Sys->NAMELEN);
	dir.qid.path = get4(a[O:]);
	dir.qid.vers = get4(a[O+4:]);
	dir.mode = get4(a[O+8:]);
	dir.atime = get4(a[O+12:]);
	dir.mtime = get4(a[O+16:]);
	# skip high order word of length
	dir.length = get4(a[O+24:]);
	dir.dtype = get2(a[O+28:]);
	dir.dev = get2(a[O+30:]);
	return Styx->STATSZ;
}

get2(a: array of byte): int
{
	return (int a[1]<<8)|int a[0];
}

get4(a: array of byte): int
{
	return (((((int a[3]<<8)|int a[2])<<8)|int a[1])<<8)|int a[0];
}

byte2name(a: array of byte, n: int): string
{
	for(i:=0; i<n; i++)
		if(a[i] == byte 0)
			break;
	return string a[0:i];
}
