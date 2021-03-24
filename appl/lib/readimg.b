implement RImagefile;
#
# bind /dis/lib/readimg.dis /dis/lib/read{gif,jpeg}.dis
#

include "sys.m";
include "draw.m";
include "bufio.m";
include "imagefile.m";
include "readimage.m";
	readimage: Readimage;

init(nil: Bufio)
{
	if(readimage == nil)
		readimage = load Readimage Readimage->PATH;
}

read(fd: ref Bufio->Iobuf): (ref Rawimage, string)
{
	if(readimage == nil)
		return (nil, "$Readimage not loaded");

	(a, err) := readimage->readimagedata(fd, 0);
	if(a != nil)
		return (a[0], err);
	return (nil, err);
}

readmulti(fd: ref Bufio->Iobuf): (array of ref Rawimage, string)
{
	if(readimage == nil)
		return (nil, "$Readimage not loaded");

	return readimage->readimagedata(fd, 1);
}
