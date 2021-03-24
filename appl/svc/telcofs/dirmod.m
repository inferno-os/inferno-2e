
# put these here until we agree changes to styx.m
Dirmod: module
{
	PATH:	con "/dis/svc/telcofs/dirmod.dis";

	convD2M:	fn(nil: ref Sys->Dir, nil: array of byte): int;
	convM2D:	fn(nil: array of byte, nil: ref Sys->Dir): int;
};
