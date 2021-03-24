#
# File: smtp.b
#
# This file contains the implementation of the SMTP module.
# This implementation uses the smtp protocol (RFC 821)
# to access an email server.
#
#
 
 
implement SMTP;
 
include "sys.m";
sys: Sys;
FD, Connection: import sys;
 
include "smtp.m";

connect : Connection;
isOpen : int;
 
 
# some useful constants.
ERR:     con 0;
OK:      con 1;
CR:     con 13;
LF:     con 10;
CRLF :  con "\r\n";
SMTP_OK: con 250;
SMTP_STARTMAIL: con 354;
SMTP_SERVICEREADY: con 220;

DELIMETER : con "\n";
 
 
#
# FUNCTION:	init()
#
# PURPOSE:	initialize the SMTP module
#
# 
init()
{
	# Load the sys module and set the connection status
	# to not open.
	sys = load Sys Sys->PATH;
	isOpen = 0;
}
 
 
#
# FUNCTION:	open()
#
# PURPOSE:	open the tcp connection to the smtp server
#
#		Implements HELO smtp command
#
#
open(ipaddr : string): (int, string)
{
	success : int;
	resp : string;
	cmd : string;
 
	# Is the connection to the smtp server open?
	if (isOpen)
		return (0, "Connection is already open.");
 
	# dial the IP Network Services.
	(success, connect) = sys->dial (ipaddr, nil);
	if (success <= 0)
		return (0, "Failed when dialing address specified.");
 
	# Send the SMTP greeting (HELO command)
	cmd = "HELO" + CRLF;
	success = sendcommand (connect.dfd, cmd);
	if (!success)
		return (0, "Failed when sending command to server.");

	(success, resp) = readresponse(connect.dfd);
	if (success != SMTP_SERVICEREADY) {
		return (0, resp);
	}
	(success, resp) = readresponse(connect.dfd);
	if (success == SMTP_SERVICEREADY) 
		(success, resp) = readresponse(connect.dfd);
	if (success != SMTP_OK) 
		return (0, resp);
 
	# Total success!
	isOpen = 1;
	return (1, nil);
}
 
#
# FUNCTION:	reset()
#
# PURPOSE:	reset the connection - aborts the current transaction
#
#		Implements RSET smtp command
#
#
reset(): (int, string)
{
	success : int;
	resp : string;
	cmd : string;
 
	# Is the connection to the email server open?
	if (!isOpen)
		return (0, "Connection is not open.");
 
	# Issue the RSET command.
	cmd = "RSET" + CRLF;
	success = sendcommand(connect.dfd, cmd);
	if (!success)
		return (0, "Failed when sending command to server.");
 
	# Get the response for the RSET command.
	(success, resp) = readresponse(connect.dfd);
	if (!success)
		return (0, resp);
 
	# Total success!
	return (1, nil);
}
 
 
#
# FUNCTION:	sendmail()
#
# PURPOSE:	send mail to one user
#
#
sendmail (fromwho, towho, subject, msg: string): (int, string)
{
	cmd : string;
	success : int;
	resp : string;

	# Is the connection to the smtp server open?
	if (!isOpen)
		return (0, "Connection is not open.");

	cmd = "MAIL FROM:" + fromwho + CRLF;
	success = sendcommand (connect.dfd, cmd);
	if (!success)
		return (0, "Failed when sending command to server.");
	(success, resp) = readresponse(connect.dfd);
	if (success != SMTP_OK) {
		cmd = "QUIT" + CRLF;
		success = sendcommand (connect.dfd, cmd);
		return (0, resp);
	}

	cmd = "RCPT TO:<" + towho + ">" + CRLF;
	success = sendcommand (connect.dfd, cmd);
	if (!success)
		return (0, "Failed when sending command to server.");
	(success, resp) = readresponse(connect.dfd);
	if (success != SMTP_OK) {
		cmd = "QUIT" + CRLF;
		success = sendcommand (connect.dfd, cmd);
		return (0, resp);
	}

	cmd = "DATA" + CRLF;
	success = sendcommand (connect.dfd, cmd);
	if (!success)
		return (0, "Failed when sending command to server.");
	(success, resp) = readresponse(connect.dfd);
	if (success != SMTP_STARTMAIL)  {
		cmd = "QUIT" + CRLF;
		success = sendcommand (connect.dfd, cmd);
		return (0, resp);
	}

	if (subject != "")
		cmd = "Subject: " + subject + CRLF + msg + CRLF;
	else
		cmd = msg + CRLF;
	success = sendcommand (connect.dfd, cmd);
	if (!success)
		return (0, "Failed when sending command to server.");
	cmd = "." + CRLF;	
	success = sendcommand (connect.dfd, cmd);      
	if (!success)  
		return (0, "Failed when sending command to server.");  
	(success, resp) = readresponse(connect.dfd);   
	if (success != SMTP_OK) {       
		cmd = "QUIT" + CRLF;   
		success = sendcommand (connect.dfd, cmd);      
		return (0, resp);      
	}      

	return (1, nil);
}


 
#
# FUNCTION:	close()
#
# PURPOSE:	close the connection 
#
#		Implements QUIT smtp command
#
#
close(): (int, string)
{
	success : int;
	resp : string;
	cmd : string;
 
	# Is the connection to the email server open?
	if (!isOpen)
		return (1, "Connection is not open.");
 
	# Issue the QUIT command.
	cmd = "QUIT" + CRLF;
	success = sendcommand(connect.dfd, cmd);
	if (!success)
		return (0, "Failed when sending command to server.");
 
	# Get the response for the QUIT command.
	(success, resp) = readresponse(connect.dfd);
	if (!success)
		return (0, resp);
 
	# Total success!
	isOpen = 0;
	return (1, nil);
}
 
 
#
# FUNCTION:	readresponse() 
#
# PURPOSE:	read smtp response from the server
#
#
readresponse(io: ref FD): (int, string)
{
	# Read a line (up to " \n") from the io file.
	(success, line) := readline(io);
	if (!success)
		return (0, "Could not read from server");

	#
	# Examine the response string for the response number
	# and description
	#
	if (len line >= 3) 
		(n, text) := sys->tokenize (line, " ");
	num := int hd text;
	case num {
		220 =>	if (len line >= 5)
		                return (SMTP_SERVICEREADY, line);
		        else
		                return (SMTP_SERVICEREADY, nil);
		250 =>	if (len line >= 5)
				return (SMTP_OK, line);
			else
				return (SMTP_OK, nil);
		354 =>	if (len line >= 5)
		                return (SMTP_STARTMAIL, line);
		        else
		                return (SMTP_STARTMAIL, nil);
		221 or
		421 or
		450 or
		451 or
		452 or
		500 or
		550 or
		554 =>	if (len line >= 5)
		                return (num, line);
		        else
		                return (num, nil);
		*   =>	str := "Unknown response from server: "+string num;
			return (ERR, str);
	}
}
 
 
#
# FUNCTION:	readline()
#
# PURPOSE:	utility function
#
#		NOTE: This function is WAY too slow reading just one
#	       	character at a time - Should be redesigned, but be
#	       	careful - it is used it a lot of places.  It is ok in
#	       	its current form, but for some uses must design a new
#		reading function.
#
#
readline(io: ref FD): (int, string)
{
	r : int;
	line : string;
	buf := array[1] of byte;
 
	#
	# Read up to the CRLF
	#
	line = "";
	for(;;) {
		r = sys->read(io, buf, len buf);
		if(r <= 0)
		        return (ERR, nil);
 
		line += string buf[0:r];
		if ((len line >= 2) &&
		    (line[len line-2] == CR) &&
		    (line[len line-1] == LF))
		        break;
	}

	buf = nil;
	# replace CRLF with " \n" 
	line = line[0:len line-2] + " \n";
	return (1, line);
}
 
 
#
# FUNCTION:	sendcommand()
#
# PURPOSE:	send the smtp command
#
# 
sendcommand(io: ref FD, cmd : string): int
{
	bytes := sys->write(io, array of byte(cmd), len cmd);
	return (bytes == len cmd);
}
 
