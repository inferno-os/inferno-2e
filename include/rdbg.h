/* Remote kernel debug protocol */
enum
{
	Terr='0',
	Rerr,
	Tmget,
	Rmget,
	Tmput,
	Rmput,

	Tspid,
	Rspid,
	Tproc,
	Rproc,
	Tstatus,
	Rstatus,
	Trnote,
	Rrnote,

	Tstartstop,
	Rstartstop,
	Twaitstop,
	Rwaitstop,
	Tstart,
	Rstart,
	Tstop,
	Rstop,
	Tkill,
	Rkill,

	Tcondbreak,
	Rcondbreak,

	RDBMSGLEN = 10	/* tag byte, 9 data bytes */
};
