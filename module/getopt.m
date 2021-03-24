# Copyright 1998, Lucent Technologies. All rights reserved.
Getopt: module {

  PATH: con "/dis/lib/getopt.dis";
  optleft: list of string;
  optarg: string;
  optopt: int;
  opterr: int;

  getopt: fn(argv: list of string, options: string): int;

};
