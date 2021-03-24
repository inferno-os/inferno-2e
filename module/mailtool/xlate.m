Xlate: module
{
	PATH:	con "/dis/mailtool/xlate.dis";

	init : fn(argv : list of string);

	substitute : fn(instring : string, regexp : Regex->Re, newstring : string) : string;
};
