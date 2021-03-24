Script: module
{
	JSCRIPTPATH: con "/dis/charon/jscript.dis";

	defaultStatus: string;
	jevchan: chan of ref Events->ScriptEvent;

	init: fn(cu: CharonUtils);
	frametreechanged: fn(top: ref Layout->Frame);
	havenewdoc: fn(f: ref Layout->Frame);
	evalscript: fn(f: ref Layout->Frame, s: string, donebuild: int) : (string, string);
};
