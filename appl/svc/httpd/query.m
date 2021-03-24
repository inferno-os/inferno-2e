Que: module{
	PATH:  		con	"/dis/svc/httpd/query.dis";	

	# list of tag=val pairs from a search string 
	Query: adt{
		tag : string;
		val : string;	
	};

	init: fn();
	parsequery: fn(search : string): list of Query;
	
};


