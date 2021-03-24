#include <stdio.h>

void
main(int argc, char *argv[])
{
	long len;
	int c;
	FILE *fin, *fout;

	/*
	 * Command line can be just name (argv[1]), a name and an input
	 * file (argv[2]), or a name, an input file, and an output
	 * file (argv[3]).
	 */
	switch(argc){
	case 2:
		fin = stdin;
		fout = stdout;
		break;
	case 3:
	case 4:
		if((fin=fopen(argv[2], "r"))==NULL) {
			fprintf(stderr, "%s failed to open input file %s\n", argv[0], argv[2]);
			exit(2);
		}
		if(argc==3) {
			fout = stdout;
			break;
		}
		if((fout=fopen(argv[3], "w"))==NULL) {
			fprintf(stderr, "%s failed to open output file %s\n", argv[0], argv[3]);
			exit(3);
		}
		break;
	default:
		fprintf(stderr, "usage: %s name [infile] [outfile]\n", argv[0]);
		exit(1);
	}
	fprintf(fout, "unsigned char %scode[] = {\n", argv[1]);
	for(len=0; (c=fgetc(fin))!=EOF; len++) {
		if((len&7) == 0)
			fprintf(fout, "\t");
		fprintf(fout, "0x%x,", c);
		if((len&7) == 7)
			fprintf(fout, "\n");
	}
	if(len & 7){
		while(len & 7){
			fprintf(fout, "0x%x,", 0);
			len++;
		}
		fprintf(fout, "\n");
	}
	fprintf(fout, "};\n");
	fprintf(fout, "unsigned long %slen = %ld;\n", argv[1], len);
	fprintf(fout, "#define %ssize\t%ld\n", argv[1], len);
	exit(0);
}
