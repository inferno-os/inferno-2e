#include	"cc.h"

void*
mysbrk(ulong size)
{
	return sbrk(size);
}

int
mycreat(char *n, int p)
{

	return create(n, 1, p);
}

int
mywait(int *s)
{
	int p;
	Waitmsg status;

	p = wait(&status);
	*s = 0;
	if(status.msg[0])
		*s = 1;
	return p;
}

int
mydup(int f1, int f2)
{
	return dup(f1,f2);
}

int
mypipe(int *fd)
{
	return pipe(fd);
}

int
systemtype(int sys)
{

	return sys&Plan9;
}

int
pathchar(void)
{
	return '/';
}

char*
mygetwd(char *path, int len)
{
	return getwd(path, len);
}

int
myexec(char *path, char *argv[])
{
	return exec(path, argv);
}

/*
 * fake mallocs
 */
void*
malloc(long n)
{
	return alloc(n);
}

void*
calloc(long m, long n)
{
	return alloc(m*n);
}

void*
realloc(void*, long)
{
	fprint(2, "realloc called\n");
	abort();
	return 0;
}

void
free(void*)
{
}

int
myfork(void)
{
	return fork();
}
