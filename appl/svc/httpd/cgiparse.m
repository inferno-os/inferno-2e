CgiData : adt {
    method : string;
    version : string;
    uri : string;
    search : string;
    tmstamp : string;
    host : string;
    remote : string;
    referer : string;
    httphd : string;
    header : list of (string, string);
    form : list of (string, string);
};

CgiParse : module
{
    PATH : con "/dis/svc/httpd/cgiparse.dis";
    cgipar : fn( g : ref Private_info, argv : list of string ) : ref CgiData;
    getBase : fn() : string;
    getHost : fn() : string;
};

