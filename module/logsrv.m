# Copyright 1997, Lucent Technologies. All rights reserved.
#
# API for logsrv.dis plug-ins to do journaling and filtering of log entries and to implement
# measurements off of the event stream
#
# An application should bind their own versions of these plug-ins before /dis/lib to override
# the default and null versions in /dis/lib
#

LogJournal: module
{
  PATH :	con	"/dis/lib/logJournal.dis";

  journalInit : fn();
  journalRecord : fn(event: string, timestamp : string);
};


LogMeasure: module
{
  PATH :	con	"/dis/lib/logMeasure.dis";

  measureInit : fn();
  measureRecord : fn(event : string);
};
