Log: module
{
   ###############################################################
   #
   #   log.m         Logging utility functions.
   #
   #   Copyright 1998, Lucent Technologies. All rights reserved.
   #
   ###############################################################

   PATH:    con "/dis/snmp/log.dis";
   dlevel:  int;   #global debugging level

   ### logging types
   NOLOGGING, ERROR, WARNING, STATUS,  #log levels 0,1,2,3 respectively
   DEBUG, IN, OUT: con iota;           #can have log levels 4 and up
   TIMER: con -1;                      #timer envoked if loglevel < 0
   START: con -2;
   STOP:  con -3;

   init:  fn (filename:string): int;
   err:   fn (filename:string, funcname:string, message:string);
   warn:  fn (filename:string, funcname:string, message:string);
   stat:  fn (filename:string, funcname:string, message:string);
   debug: fn (filename:string, funcname:string, level:int, message:string);
   timer: fn (filename:string, funcname:string, message:string);
   start: fn (filename:string, funcname:string, message:string);
   stop:  fn (filename:string, funcname:string, message:string);
   in:    fn (filename:string, funcname:string, level:int);
   out:   fn (filename:string, funcname:string, level:int);
};
