###
### This data and information is not to be used as the basis of manufacture,
### or be reproduced or copied, or be distributed to another party, in whole
### or in part, without the prior written consent of Lucent Technologies.
###
### (C) Copyright 1997 Lucent Technologies
###
### Written by J. Keane

###
###
### Perform string substitution
###

implement Xlate;

include "sys.m";
	sys: Sys;

include "regex.m";
        regex: Regex;
	compile, execute, Re : import regex;

include "xlate.m";

init (argv: list of string)
{
    sys = load Sys Sys->PATH;
    if (sys == nil) exit;
    
    regex = load Regex Regex->PATH;
    if (regex == nil) {
	sys->print("Xlate ERROR: could not load regex.\n");
	exit;
    }
}

#
# Substitute(InString, Regexp, NewString) : string
#
# Returns the string resulting from the replacement of every instance of Regexp
# in Instring with NewString.  Regexp must be a compiled regular expression.
#
substitute(instring : string, regexp : Re, newstring : string) : string
{
    lastmark := 0;
    outstring := "";

    rematch := execute(regexp, instring);
    if (rematch != nil) 
	(begmatch, endmatch) := rematch[0];
    else
	(begmatch, endmatch) = (-1,-1);
    
    while (begmatch != -1) {
	begmatch += lastmark;
	endmatch += lastmark;
	if (begmatch > lastmark) outstring = outstring + instring[lastmark:begmatch];
	outstring = outstring + newstring;
	lastmark = endmatch;
	rematch = execute(regexp, instring[lastmark:]);
	if (rematch != nil)
	    (begmatch, endmatch) = rematch[0];
	else
	    (begmatch, endmatch) = (-1,-1);
    }

    if (lastmark < len instring) outstring = outstring + instring[lastmark:];
    return outstring;
}
	
