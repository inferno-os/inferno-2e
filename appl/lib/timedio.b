implement TimedIO;

include "timedio.m";
include "sys.m";
	sys : Sys;
include "draw.m";
include "kill.m";
	kill: Kill;

init()
{
	sys = load Sys Sys->PATH;

	if (sys == nil) 
		exit;

	kill = load Kill Kill->PATH;
	if (kill == nil) {
		sys->fprint(sys->fildes(2),"timedio: Could not load kill file\n");
		exit;
	}
}

reader( fd: ref sys->FD, buf: array of byte, nbytes:int, pidchan: chan  of int, response: chan of int) 
{
	# Return PID down pidchan
	pidchan <- = sys->pctl(0, nil);
	
	# blocking read 
	response <- = sys->read(fd, buf, nbytes );
}

writer( fd: ref sys->FD, buf: array of byte, nbytes:int, pidchan: chan  of int, response: chan of int) 
{
	# Return PID down pidchan
	pidchan <- = sys->pctl(0, nil);
	
	# blocking read 
	response <- = sys->write(fd, buf, nbytes );
}

sleeper(  timeout: int, pidchan: chan of int, response: chan of int ) 
{
	# Return PID down pidchan 	
	pidchan <- = sys->pctl(0,nil);

	# sleep for timeout
	sys->sleep( timeout );

	# return response 0
	response <- = TIMEOUT;
}

	#
	# procedure:	read
	# arguments:	fd - reference to an open file descriptor
	#			buf - receive buffer to be filled in by read
	#			nbytes - number of bytes expected
	#			timeout - timeout in milliseconds
	# returns:		Number of bytes read
	#			or 0 on EOF
	#			or -1 on Timeout
	#



read( fd: ref Sys->FD, buf: array of byte, nbytes:int, timeout: int ) : int
{
reader_pid_chan:= chan of int;
sleeper_pid_chan:= chan of int;
response:= chan of int;

	# Initialize if necessary
	if (sys == nil) {
		sys = load Sys Sys->PATH;
		if (sys == nil) 
			exit;	
	}
	if (kill == nil) {
		kill = load Kill Kill->PATH;
		if (kill == nil) {
			sys->fprint(sys->fildes(2),"timedio: Could not load kill module\n");
			exit;
		}
	}
	# Spawn off reader 
	spawn reader( fd, buf, nbytes, reader_pid_chan, response );

	# Get PID on return channel
	reader_pid := <- reader_pid_chan;
	 
	# Spawn off sleeper
	spawn sleeper( timeout, sleeper_pid_chan, response );

	# Get PID on return channel
	sleeper_pid := <- sleeper_pid_chan;

	# Block for response
	resp := <- response ;

	if ( resp == TIMEOUT) {
		kill->killpid( string(reader_pid), nil);
		return TIMEOUT;
	} else {
		kill->killpid( string(sleeper_pid), nil);
		return resp;
	}
}

#
# procedure:	write
# arguments:	fd - reference to an open file descriptor
#				buf - buffer to be written
#				nbytes - number of bytes to write
#				timeout - timeout in milliseconds
#


write( fd: ref Sys->FD, buf: array of byte, nbytes:int, timeout: int ) : int
{
writer_pid_chan:= chan of int;
sleeper_pid_chan:= chan of int;
response:= chan of int;

	# Initialize if necessary
	if (sys == nil) {
		sys = load Sys Sys->PATH;
		if (sys == nil) 
			exit;	
	}
	if (kill == nil) {
		kill = load Kill Kill->PATH;
		if (kill == nil) {
			sys->fprint(sys->fildes(2),"timedio: Could not load kill module\n");
			exit;
		}
	}
	# Spawn off sleeper
	spawn sleeper(timeout,sleeper_pid_chan, response );
	
	# Get PID on return channel
	sleeper_pid := <- sleeper_pid_chan;


	# Spawn off writer
	spawn writer( fd, buf, nbytes, writer_pid_chan, response );

	# Get PID on return channel
	writer_pid := <- writer_pid_chan;

	# Block for response
	resp := <- response ;

	if ( resp == TIMEOUT) {
		kill->killpid( string(writer_pid), nil );
		return TIMEOUT;
	} else {
		kill->killpid( string(sleeper_pid), nil);
		return resp;
	}
}