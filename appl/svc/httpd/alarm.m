Alm: module{
	PATH:  		con	"/dis/svc/httpd/alarm.dis";	

	Alarm : adt{
		alchan : chan of int;
		pid : int;
		stop: fn(a : self Alarm); 
		alarm: fn(time : int) : Alarm;
	};
	
};

