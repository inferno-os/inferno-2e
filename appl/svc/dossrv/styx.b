implement Styx;

include "sys.m";
	sys: Sys;

include "styx.m";

msgname := array [Tmax] of{
	Tnop =>		"Tnop",
	Rnop =>		"Rnop",
	Terror =>	"Terror",
	Rerror =>	"Rerror",
	Tflush =>	"Tflush",
	Rflush =>	"Rflush",
	Tclone =>	"Tclone",
	Rclone =>	"Rclone",
	Twalk =>	"Twalk",
	Rwalk =>	"Rwalk",
	Topen =>	"Topen",
	Ropen =>	"Ropen",
	Tcreate =>	"Tcreate",
	Rcreate =>	"Rcreate",
	Tread =>	"Tread",
	Rread =>	"Rread",
	Twrite =>	"Twrite",
	Rwrite =>	"Rwrite",
	Tclunk =>	"Tclunk",
	Rclunk =>	"Rclunk",
	Tremove =>	"Tremove",
	Rremove =>	"Rremove",
	Tstat =>	"Tstat",
	Rstat =>	"Rstat",
	Twstat =>	"Twstat",
	Rwstat =>	"Rwstat",
	Tattach =>	"Tattach",
	Rattach =>	"Rattach",
};

msglen := array[Tmax] of
{
 	3, 		# Tnop
 	3, 		# Rnop
 	0, 		# Terror
 	67, 		# Rerror
 	5, 		# Tflush
 	3, 		# Rflush
 	7, 		# Tclone
 	5, 		# Rclone
 	33, 		# Twalk
 	13, 		# Rwalk
 	6, 		# Topen
 	13, 		# Ropen
 	38, 		# Tcreate
 	13, 		# Rcreate
 	15, 		# Tread
 	8,		# Rread header only; excludes data 
 	16,		# Twrite header only; excludes data 
 	7, 		# Rwrite
 	5, 		# Tclunk
 	5, 		# Rclunk
 	5, 		# Tremove
 	5, 		# Rremove
 	5, 		# Tstat
 	121, 		# Rstat
 	121, 		# Twstat
 	5, 		# Rwstat
	0,		# Tsession
	0,		# Rsession
 	5+2*NAMELEN, 	# Tattach
 	13, 		# Rattach
};


convD2M(f : ref Sys->Dir) : array of byte
{
	if(sys == nil)
		sys = load Sys Sys->PATH;

	rval := array[DIRLEN] of byte;
	n :=0;	
	for(i:=0;i<len f.name && i<NAMELEN;i++)
		rval[n++]=byte f.name[i];
	for(;i<NAMELEN;i++)
		rval[n++]= byte 0;
	for (i=0;i<len f.uid && i<NAMELEN;i++)
		rval[n++]=byte f.uid[i];
	for(;i<NAMELEN;i++)
		rval[n++]= byte 0;
	for (i=0;i<len f.gid && i<NAMELEN;i++)
		rval[n++]=byte f.gid[i];
	for(;i<NAMELEN;i++)
		rval[n++]= byte 0;

#	if(len f.name > NAMELEN)
#		sys->print("Warning truncating [%s] to [%s]\n",
#			f.name, f.name[:NAMELEN]);
#	if(len f.uid > NAMELEN)
#		sys->print("Warning truncating [%s] to [%s]\n",
#			f.uid, f.uid[:NAMELEN]);
#	if(len f.gid > NAMELEN)
#		sys->print("Warning truncating [%s] to [%s]\n",
#			f.gid, f.gid[:NAMELEN]);

	rval[n++] = byte f.qid.path;
	rval[n++] = byte (f.qid.path >> 8);
	rval[n++] = byte (f.qid.path >> 16);
	rval[n++] = byte (f.qid.path >> 24);
	
	rval[n++] = byte f.qid.vers;
	rval[n++] = byte (f.qid.vers >> 8);
	rval[n++] = byte (f.qid.vers >> 16);
	rval[n++] = byte (f.qid.vers >> 24);

	rval[n++] = byte f.mode;
	rval[n++] = byte (f.mode >> 8);
	rval[n++] = byte (f.mode >> 16);
	rval[n++] = byte (f.mode >> 24);

	rval[n++] = byte f.atime;
	rval[n++] = byte (f.atime >> 8);
	rval[n++] = byte (f.atime >> 16);
	rval[n++] = byte (f.atime >> 24);

	rval[n++] = byte f.mtime;
	rval[n++] = byte (f.mtime >> 8);
	rval[n++] = byte (f.mtime >> 16);
	rval[n++] = byte (f.mtime >> 24);

	rval[n++] = byte f.length;
	rval[n++] = byte (f.length >> 8);
	rval[n++] = byte (f.length >> 16);
	rval[n++] = byte (f.length >> 24);
	rval[n++] = byte 0;
	rval[n++] = byte 0;
	rval[n++] = byte 0;
	rval[n++] = byte 0;

	rval[n++] = byte f.dtype;
	rval[n++] = byte (f.dtype >> 8);

	rval[n++] = byte f.dev;
	rval[n++] = byte (f.dev >> 8);

	return rval;
}

convM2D(f : array of byte) : ref Sys->Dir
{
	retval := ref Sys->Dir;

	retval.name = string f[0:28];
	retval.uid =  string f[28:56];
	retval.gid =  string f[56:84];

	retval.qid.path =	(int f[84]<<0)|
				(int f[85]<<8)|
				(int f[86]<<16)|
				(int f[87]<<24);
	retval.qid.vers =	(int f[88]<<0)|
				(int f[89]<<8)|
				(int f[90]<<16)|
				(int f[91]<<24);

	retval.mode =	(int f[92]<<0)|
			(int f[93]<<8)|
			(int f[94]<<16)|
			(int f[95]<<24);
	retval.atime =	(int f[96]<<0)|
			(int f[97]<<8)|
			(int f[98]<<16)|
			(int f[99]<<24);
	retval.mtime =	(int f[100]<<0)|
			(int f[101]<<8)|
			(int f[102]<<16)|
			(int f[103]<<24);
	retval.length =	(int f[104]<<0)|
			(int f[105]<<8)|
			(int f[106]<<16)|
			(int f[107]<<24);
	retval.dtype =	(int f[112]<<0)|
			(int f[113]<<8);
	retval.dev =	(int f[114]<<0)|
			(int f[115]<<8);
	return retval;
}

ConvM2S(f : array of byte) : (int,Smsg)
{
	retval : Smsg;
	i:=0;

	if(len f < 3)
		return (0, retval);

	retval.Mtype = int f[i++];
	retval.Tag =	(int f[i+0]<<0)|
			(int f[i+1]<<8);
	i += 2;

	if(len f < msglen[retval.Mtype])
		return (0,retval);

	case retval.Mtype {
	* =>
		return (-1,retval);
	Tnop =>
		break;
	Tflush =>
		retval.oldtag =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
	Tattach =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;

		for(j := 0; j < NAMELEN; j++) {
			if (f[i] == byte 0)
				break;
			retval.uname[len retval.uname]= int f[i++];
		}

		for(; j < NAMELEN; j++)
			i++;

		for(j = 0; j < NAMELEN; j++) {
			if(f[i]== byte 0)
				break;
			retval.aname[len retval.aname]= int f[i++];
		}

		for(;j<NAMELEN;j++)
			i++;
	Tclone =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		retval.newfid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
	Twalk =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		for(j := 0; j < NAMELEN; j++) {
			if(f[i] == byte 0)
				break;
			retval.name[len retval.name]= int f[i++];
		}

		for(; j < NAMELEN; j++)
			i++;
	Topen =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		retval.mode= int f[i++];
	Tcreate =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		for(j := 0; j < NAMELEN; j++){
			if(f[i] == byte 0)
				break;
			retval.name[len retval.name]= int f[i++];
		}

		for(; j < NAMELEN; j++)
			i++;
		retval.perm =	(int f[i+0]<<0)|
				(int f[i+1]<<8)|
				(int f[i+2]<<16)|
				(int f[i+3]<<24);
		i += 4;
		retval.mode= int f[i++];
	Tread =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		retval.offset =	(int f[i+0]<<0)|
				(int f[i+1]<<8)|
				(int f[i+2]<<16)|
				(int f[i+3]<<24);
		i += 8;
		retval.count =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
	Twrite =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		retval.offset = (int f[i+0]<<0)|
				(int f[i+1]<<8)|
				(int f[i+2]<<16)|
				(int f[i+3]<<24);
		i += 8;
		retval.count =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		if(len f < msglen[retval.Mtype]+retval.count)
			return (0,retval);

		i++;	# pad(1) 
		retval.data = f[i:i+retval.count]; 
		i += retval.count;
	Tclunk or Tremove or Tstat=>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
	Twstat =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		retval.stat = array[DIRLEN] of byte;
		retval.stat[0:] = f[i:];
		i += DIRLEN;
	Rnop or
	Rflush =>
		break;
	Rerror =>
		for(j := 0; j < ERRLEN; j++){
			if(f[i] == byte 0) {
				i++;
				continue;
			}
			retval.ename[len retval.ename]= int f[i++];
		}
	Rclone or
	Rclunk or
	Rremove or
	Rwstat=>
		retval.fid= (int f[i+0]<<0)|
			    (int f[i+1]<<8);
		i += 2;
	Rattach or
	Rwalk or
	Ropen or
	Rcreate=>
		retval.fid = 	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		retval.qid.path =(int f[i+0]<<0)|
				 (int f[i+1]<<8)|
				 (int f[i+2]<<16)|
				 (int f[i+3]<<24);
		i += 4;
		retval.qid.vers =(int f[i+0]<<0)|
				 (int f[i+1]<<8)|
				 (int f[i+2]<<16)|
				 (int f[i+3]<<24);
		i += 4;
	Rread =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		retval.count =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		if(len f < msglen[retval.Mtype]+retval.count)
			return (0, retval);

		i++;			# pad(1) 
		retval.data = f[i:i+retval.count];
		i += retval.count;
	Rwrite =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		retval.count =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
	Rstat =>
		retval.fid =	(int f[i+0]<<0)|
				(int f[i+1]<<8);
		i += 2;
		retval.stat = array[DIRLEN] of byte;
		retval.stat[0:] = f[i:];
		i += DIRLEN;
	}
	return (i,retval);
}	


Smsg.ConvS2M(s : self Smsg) : array of byte
{
	buf := array[130+8192] of byte; # max size of a styx message +2
	i :=0;

	buf[i++]= byte s.Mtype;
	buf[i++]= byte s.Tag;
	buf[i++]= byte (s.Tag>>8);

	case s.Mtype {
	* => 
		return nil;
	Tnop =>
		break;
	Tflush =>
		buf[i++]= byte s.oldtag;
		buf[i++]= byte (s.oldtag>>8);
	Tattach =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		for(j:=0;j<len s.uname;j++)
			buf[i+j]=byte s.uname[j];
		while(j<NAMELEN){
			buf[i+j]= byte 0;
			j++;
		}
		i+=j; 
		for(j=0;j<len s.aname;j++)
			buf[i+j]=byte s.aname[j];
		while(j<NAMELEN){
			buf[i+j]= byte 0;
			j++;
		}
		i+=j; 
	Tclone =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		buf[i++]= byte s.newfid;
		buf[i++]= byte (s.newfid>>8);
	Twalk =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		for(j:=0;j<len s.name;j++)
			buf[i+j]=byte s.name[j];
		while(j<NAMELEN){
			buf[i+j]= byte 0;
			j++;
		}
		i+=j; 
	Topen =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		buf[i++]= byte s.mode;
	Tcreate =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		for(j:=0;j<len s.name;j++)
			buf[i+j]=byte s.name[j];
		while(j<NAMELEN){
			buf[i+j]= byte 0;
			j++;
		}
		i+=j; 
		buf[i++]= byte s.perm;
		buf[i++]= byte (s.perm>>8);
		buf[i++]=byte (s.perm>>16);
		buf[i++]=byte (s.perm>>24);
		buf[i++]= byte s.mode;
	Tread =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		buf[i++]= byte s.offset;
		buf[i++]= byte (s.offset>>8);
		buf[i++]=byte (s.offset>>16);
		buf[i++]=byte (s.offset>>24);
		buf[i++]=byte 0;
		buf[i++]=byte 0;
		buf[i++]=byte 0;
		buf[i++]=byte 0;
		buf[i++]= byte s.count;
		buf[i++]= byte (s.count>>8);
	Twrite =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		buf[i++]= byte s.offset;
		buf[i++]= byte (s.offset>>8);
		buf[i++]=byte (s.offset>>16);
		buf[i++]=byte (s.offset>>24);
		buf[i++]=byte 0;
		buf[i++]=byte 0;
		buf[i++]=byte 0;
		buf[i++]=byte 0;
		buf[i++]= byte s.count;
		buf[i++]= byte (s.count>>8);
		i++;	# pad(1) 
		for(j:=0;j<s.count;j++)
			buf[i++]=s.data[j];
	Tclunk or Tremove or Tstat =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
	Twstat =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		for(j:=0;j<len s.stat;j++)
			buf[i+j]=byte s.stat[j];
		while(j<DIRLEN){
			buf[i+j]= byte 0;
			j++;
		}
		i+=j; 
	Rnop or Rflush =>
		;
	Rerror =>
		for(j:=0;j<len s.ename;j++)
			buf[i++]=byte s.ename[j];
		for(;j<ERRLEN;j++)
			buf[i++]=byte 0;
	Rattach or Rwalk or Ropen or Rcreate =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		buf[i++]= byte s.qid.path;
		buf[i++]= byte (s.qid.path>>8);
		buf[i++]=byte (s.qid.path>>16);
		buf[i++]=byte (s.qid.path>>24);
		buf[i++]= byte s.qid.vers;
		buf[i++]= byte (s.qid.vers>>8);
		buf[i++]=byte (s.qid.vers>>16);
		buf[i++]=byte (s.qid.vers>>24);
	Rclone or Rclunk or Rremove or Rwstat =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
	Rread =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		buf[i++]= byte s.count;
		buf[i++]= byte (s.count>>8);
		buf[i++]= byte 0;	# pad(1) 
		for(j:=0;j<s.count;j++)
			buf[i++]=s.data[j];
	Rwrite =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		buf[i++]= byte s.count;
		buf[i++]= byte (s.count>>8);
	Rstat =>
		buf[i++]= byte s.fid;
		buf[i++]= byte (s.fid>>8);
		for(j:=0;j<len s.stat;j++)
			buf[i++]=s.stat[j];
		for(;j<DIRLEN;j++)
			buf[i++]= byte 0;
	}
	return buf[0:i];
}	
	
Smsg.print(s : self Smsg) : string
{
	if(sys == nil)
		sys = load Sys Sys->PATH;

	retval :=sys->sprint("Mtype : %s\n",msgname[s.Mtype]); 
	retval +=sys->sprint("Tag : %d\n",s.Tag); 
	retval +=sys->sprint("fid : %d\n",s.fid);
	case s.Mtype {
	Tflush => 
		retval +=sys->sprint("oldtag : %d\n",s.oldtag); 
	Rattach or Rwalk or Ropen or Rcreate =>		 
		retval +=sys->sprint("qid.path : 16r%ux\n",s.qid.path); 	 
		retval +=sys->sprint("qid.vers : 16r%ux\n",s.qid.vers); 
	Tattach =>	
		retval +=sys->sprint("uname : %s\n",s.uname); 	
		retval +=sys->sprint("aname : %s\n",s.aname); 	
	Rerror =>
		retval +=sys->sprint("ename : %s\n",s.ename); 
	Tcreate =>		
		retval +=sys->sprint("perm : %d\n",s.perm); 
		retval +=sys->sprint("name : %s\n",s.name);
		retval +=sys->sprint("mode : %d\n",s.mode);		 
	Tclone => 
		retval +=sys->sprint("newfid : %d\n",s.newfid);
	Twalk => 		
		retval +=sys->sprint("name : %s\n",s.name); 
	Topen =>		
		retval +=sys->sprint("mode : %d\n",s.mode); 
	Tread or Twrite =>		 
		retval +=sys->sprint("offset : %d\n",s.offset);
		retval +=sys->sprint("count : %d\n",s.count); 
		if (s.Mtype == Twrite)
			retval +=sys->sprint("data len : %d\n",len s.data);	 
	Rread => 	 
		retval +=sys->sprint("count : %d\n",s.count); 		 
		retval +=sys->sprint("data len : %d\n",len s.data);	
	Twstat or Rstat =>
		retval +=sys->sprint("stat len : %d\n",len s.stat); 
	}
	return retval;
}

