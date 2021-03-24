#
# File: smtp.m
#
# This file contains the declaration of the SMTP module.
# The SMTP module provides smtp protocol independent access
# to an email server.
#
 
SMTP : module
{
        #
        # The init function initializes the SMTP module.
        # It must be called before any other function in the
        # module.
        #
        init: fn();
 
        #
        # The open function opens a connection with the email
        # server.  The function requires the email server's
        # tcp/ip address, a username and a password to make the
        # connection to the email server.  It returns a tuple
        # (int, string).  The int indicates success or failure
        # (0 = failure, 1 = success). If the function fails,
        # the int returned is 0, the string returned will indicate
        # why the function failed. It should be called after the
        # init function and before any other function in the module.
        #
        open: fn(ipaddr : string) : (int, string);

	#
        # The reset function unmarks all messages that have been
        # marked deleted during this session. It returns a tuple
        # (int, string).  The int indicates success or failure
        # (0 = failure, 1 = success). If the function fails,
        # the int returned is 0, the string returned will indicate
        # why the function failed.
        #
        reset: fn() : (int, string);

	#
	# The sendmail command sends a specified messages to one 
	# reciepient specified in the towho variable.
	# Return a tuple (int, string). The int indicates success or
	# failure (0 = failure, 1 = success). If the functio fails,
	# the int returned is 0, the string returned will contain
	# the message indicating the reason for the failure.
	#
	sendmail: fn(fromwho: string, 
		     towho: string, 
		     subject: string,
		     msg: string) : (int, string);


        #
        # The close function closes a connection with the email
        # server. It returns a tuple (int, string).  The int
        # indicates success or failure (0 = failure, 1 = success).
        # If the function fails, the int returned is 0, the string
        # returned will indicate why the function failed.
        #
        close: fn() : (int, string);
};
