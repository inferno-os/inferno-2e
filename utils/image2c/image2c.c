#include <lib9.h>
#include	<libg.h>

main(int argc, char *argv[])
{
	Bitmap *b;
	int i, j, fd, width;
	uchar buf[2048];

	binit(0, 0, 0);
	fd = open(argv[1], OREAD);
	if(fd < 0){
		fprint(2, "can't open font\n");
		exits("foo");
	}
	b = rdbitmapfile(fd);
	if(b==0){
		fprint(2, "can't read file\n");
		exits("foo");
	}
	width = (b->r.max.x+(31>>b->ldepth))/(32>>b->ldepth)
			- (b->r.min.x)/(32>>b->ldepth);
	print(	"#include <u.h>\n"
		"#include <libc.h>\n"
		"#include <memimage.h>\n");
	print(	"static ulong bits0[] ={\n");
	for(i=b->r.min.y; i<b->r.max.y; i++){
		rdbitmap(b, i, i+1, buf);
		for(j=0; j<4*width; j+=4){
			print("	0x%.8lux,%c",
				(buf[j]<<24)|(buf[j+1]<<16)|(buf[j+2]<<8)|(buf[j+3]<<0),
				(((j/4)&3)==3)? '\n' : ' ');
		}
	}
	print(	"};\n"
		"static Memimage strike0 = {\n"
		"	{%d, %d, %d, %d},\n"
		"	{%d, %d, %d, %d},\n"
		"	%d,\n"
		"	0,\n"
		"	bits0,\n"
		"	0,\n"
		"	%d,\n"
		"	0,\n"
		"};\n",
		b->r.min.x, b->r.min.y, b->r.max.x, b->r.max.y,
		b->r.min.x, b->r.min.y, b->r.max.x, b->r.max.y,
		b->ldepth, width);
}
