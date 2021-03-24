implement Memory;

#
# 8 MB memory simulator, this runs on systems with greater than 8 Mb of memory
# but only lets the applications see the system has having 8 Mb of memory. This 
# requires changing the maximum memory and maximum available memory to within 
# the 8 Mb limit.

include "sys.m";
include "draw.m";
include "bufio.m";
include "string.m";

Context: import Draw;

sys: Sys;
str: String;
stderr: ref Sys->FD;
mem: ref Sys->FD;

AVAILMEM: con 8;
MEGABYTE: con 1024*1024;
AVAILMEG: con AVAILMEM * MEGABYTE;
maxmem:= array[3] of int;


Memory: module
{
	PATH: con "dis/lib/memory";
	init:	fn(nil: ref Context, nil: list of string);
};

init(nil: ref Context, nil: list of string)
{
	rbuf:= array[sys->ATOMICIO] of byte;

	if((sys = load Sys Sys->PATH) == nil)
	    return;
	stderr = sys->fildes(2);

	mem = sys->open("/dev/memory", sys->OREAD);
	sys->bind("#s", "/dev", Sys->MBEFORE);
	file := sys->file2chan("/dev", "memory");
	if(file == nil) {
		sys->fprint(stderr, "memory: failed to make file: %r\n");
		return;
	}

	if((str = load String String->PATH) == nil) {
		sys->fprint(stderr, "memory: failed to load string: %r\n");
		return;
	}
	# Compute the percentage of memory used by the three segments
	# Physical memory = max_main + max_heap + max_image + kernel
	# Assumption is the physical memory is a power of two, so the
	# size of the kernel can be dynamically determined, and the
	# percentages of memory can than be computed, with kernel memory
	# coming from the main memory pool.

	# First read the read /dev/memory to determine the actual memory pool sizes

	n := sys->read(mem, rbuf, sys->ATOMICIO);
	if (n <= 0) {
		sys->fprint(stderr, "memory: no data from real /dev/memory: %r\n");
		return;
	}
	(lines, line_list) := sys->tokenize(string rbuf, "\n");
	if (lines < 3) {
		sys->fprint(stderr, "memory: incorrect number of lines in /dev/memory: %r\n");
		return;
	}
	total_memory := 0;
	for (i := 0; i < 3; i++) {
		
		lbuf := hd line_list;
		line_list = tl line_list;
		(num, mlist) := sys->tokenize(lbuf, " \n");
		if (num < 8) {
			sys->fprint(stderr, "memory: not enough tokens in /dev/memory line: %r\n");
			return;
		}
		memuse := hd mlist; mlist = tl mlist;
		(poolmem, nil) := str->toint(hd mlist, 10); mlist = tl mlist;
		maxmem[i] = poolmem;
		hiwater := hd mlist; mlist = tl mlist;
		alloc := hd mlist; mlist = tl mlist;
		free := hd mlist; mlist = tl mlist;
		exten := hd mlist; mlist = tl mlist;
		maxavail := hd mlist; mlist = tl mlist;
		pname := hd mlist;
		total_memory += maxmem[i];
	}

	# guess at physical memory is total of memory pools. However, the
	# kernel is not included in the pools.
	# So, guess physical memory is the nearest power of 2 larger than
	# the total_memory.
	pool_megs := total_memory / MEGABYTE;	# estimate of phys_memory
	pow2 := 1;
	while (pool_megs > pow2) {
		pow2 <<= 1;
	}
	physical_memory := pow2 * MEGABYTE;


	kernel_size := physical_memory - total_memory;

	main_percent := ((maxmem[0] + kernel_size) * 100) / physical_memory;
	heap_percent := (maxmem[1] * 100) / physical_memory;
	image_percent := (maxmem[2] * 100) / physical_memory;


	# Recompute max memory for each pool based upon the available
	# memory desired and the percentages of the live system.

	maxmem[0] = ((AVAILMEG - kernel_size) * main_percent) / 100;
	maxmem[1] = (AVAILMEG * heap_percent) / 100;
	maxmem[2] = (AVAILMEG * image_percent) / 100;
	


	# Reset memory to beginning
	sys->seek(mem, 0, sys->SEEKSTART);

	spawn memory(file);
}


memory(file: ref sys->FileIO)
{
  off, nbytes, fid : int;
  rc : Sys->Rread;
  wc : Sys->Rwrite;
  buf : array of byte;
  sent_memory : int;

  # Monitor for rlist queue
  reqch = chan of int;
  freech = chan of int;	

  spawn memory_mon();
  for (;;) {
    alt {
      (off, buf, fid, wc) = <-file.write =>
	if(wc == nil)
	  break;
	buf = nil;
	continue;
      (off, nbytes, fid, rc) = <-file.read =>
#	sys->fprint(stderr, "offset: %d nbytes: %d rc: %x\n", off, nbytes, rc);
	if (off == 0) {
		sent_memory = 0;
	}
	if (rc != nil) {
	    if (!sent_memory) {
	  	send_memory(sent_memory, rc);
		sent_memory = 1;
	    } else {
		rc <- = (nil, nil);
		sent_memory = 0;
	    }
	}
	continue;
    }
  }
}

reqch, freech : chan of int;

memory_mon()
{
    while(1) {

	# Wait for request
	<- reqch;

	# Wait for free
	<- freech;
    }
}

send_memory(line_ind: int, rc: chan of (array of byte, string))
{
	rbuf := array[sys->ATOMICIO] of byte;
	res_str: string;
	rstr : string;
	
	if (line_ind == 0) {
		sys->seek(mem, 0, sys->SEEKSTART);
	}
	n := sys->read(mem, rbuf, sys->ATOMICIO);
	if (n <= 0) {
		rc <-= (nil, "no data from memory");
		return;
	}
	(lines, line_list) := sys->tokenize(string rbuf, "\n");
	if (lines < 3) {
		rc <-= (nil, "incorrect number of lines");
		return;
	}
	for (i := 0; i < 3; i++) {
		lbuf := hd line_list;
		line_list = tl line_list;
		(num, mlist) := sys->tokenize(lbuf, " \n");
		if (num < 8) {
			rc <-= (nil, "not enough tokens");
			return;
		}
		(memuse, nil) := str->toint(hd mlist, 10); mlist = tl mlist;
#		memuse := hd mlist; mlist = tl mlist;
		mlist = tl mlist;
#		maxmemsize := hd mlist; mlist = tl mlist;
		hiwater := hd mlist; mlist = tl mlist;
		alloc := hd mlist; mlist = tl mlist;
		free := hd mlist; mlist = tl mlist;
		exten := hd mlist; mlist = tl mlist;
		(maxavail, nil) := str->toint(hd mlist, 10); mlist = tl mlist;
		pname := hd mlist;
		smallavail := maxmem[i] - memuse;
##		smallavail := maxmem[line_ind] - memuse;
		if (smallavail < 0) {
			maxavail = 0;
		} else {
			if (smallavail < maxavail) {
				maxavail = smallavail;
			}
		}
		rstr = sys->sprint("%11.d%12.d%12.12s%12.12s%12.12s%12.12s%12.d %s\n",
				memuse, maxmem[i], hiwater, alloc, free, exten,
				maxavail, pname);
#		rstr = sys->sprint("%11.d%12.d%12.12s%12.12s%12.12s%12.12s%12.d %s\n",
#				memuse, maxmem[line_ind], hiwater, alloc, free, exten,
#				maxavail, pname);
#		rstr = sys->sprint("%12.12s%12.12s%12.12s%12.12s%12.12s%12.12s%12.d %s\n",
#				memuse, maxmemsize, hiwater, alloc, free, exten,
#				maxavail, pname);
#		sys->fprint(stderr, "memory rstr: '%s'\n", rstr);
#		sys->fprint(stderr, "memory res: '%s'\n", res_str);
		res_str += rstr;

	}
	rc <-= (array of byte res_str, nil);
}
