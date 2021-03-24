# Events are sent to the logSrv at /services/logs/log. An event is just a message, but to be 
# a little more useful for postprocessing, it can be formatted according to the following rules.
# Fields are TAB separated.
# The first field (to be considered for further processing) should be an eventCode defined below
# in string form. So, I'll define them as numbers but want them written as strings (e.g.,
#
# sys->fprint(stdlog, "%d \t stuff\n", EVENT_NUM);
#
# The fields should be something like:
# 
# Code(above) \t Who \t What \t Why .....
# 
# each code or range of codes can agree on their own fields and order.
#
# Downstream the logSrv can look at the codes and post process.
#
# NOTE: eventCode 0 (zero) is reserved for non-coded messages. If the first field casts to 0
# (zero), we assume its just info and not subject to any post processing. Its in the log
# as long as it stays there, but won't get passed to any journaling or filtering modules.
#

Events : module
{
# Ranges
  eventNull :                     con 0;
  infoRange : con 98;
  initRange : con 99;
  faultRange : con 99;
  countRange : con 13;

  logdirName : con "/services/logs";
  logName : con "log";
  logctlName : con "logctl";

# Informational Events -- add yours
  eventInfo : con 1;
  eventServiceInvocation :        con eventInfo + 1;


  eventInfoMax :                  con eventInfo + infoRange;

# Initialization Events
  eventInit :                     con eventInfoMax + 1;



  eventInitMax :                  con eventInit + initRange;


# Fault Events
  eventFault :                     con eventInitMax + 1;



  eventFaultMax :                  con eventFault + faultRange;


# Counter Events
  eventCount :                     con eventFaultMax + 1;
  DirConnect : con eventCount;
  DirQuery : con eventCount +1;
  DirLogin : con eventCount +2;
  DirLoginUserNotFound : con eventCount +3;
  DirLoginAccessDenied : con eventCount +4;
  DirAgentConnectFail : con eventCount +5;
  DirAgentBrokenConnection : con eventCount +6;
  DirBadParameter : con eventCount +7;
  DownloadAttempt : con eventCount +8;
  DownloadIncomplete : con eventCount +9;
  NotifyAttempt : con eventCount +10;
  Registration : con eventCount +11;
  HourlyMeasurements : con eventCount +12;

# set countRange to N+1
  eventCountMax :                  con eventCount + countRange;
};

