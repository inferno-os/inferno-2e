#include "lib9.h"
#include "isa.h"
#include "interp.h"
#include "kernel.h"

Module*
readmod(char *path, Module *m, int sync)
{
	Dir d;
	int fd, n;
	uchar *code;
	Module *ans;

	if(path[0] == '$')
		return m;

	ans = nil;
	code = nil;

	if(sync)
		release();

	fd = kopen(path, OREAD);
	if(fd < 0)
		goto done;

	if(kdirfstat(fd, &d) < 0)
		goto done;

	if(m != nil) {
		if(d.mtime == m->mtime && d.qid.path == m->qid.path && d.qid.vers == m->qid.vers) {
			ans = m;
			goto done;
		}
	}

	code = mallocz(d.length, 0);
	if(code == nil)
		goto done;

	n = kread(fd, code, d.length);
	if(n != d.length) {
		free(code);
		code = nil;
	}
done:
	if(fd >= 0)
		kclose(fd);
	if(sync)
		acquire();
	if(m != nil && ans == nil)
		unload(m);
	if(code != nil) {
		ans = parsemod(path, code, d.mtime, d.qid);
		free(code);
	}
	return ans;
}
