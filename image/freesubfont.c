#include <lib9.h>
#include <image.h>

void
freesubfont(Subfont *f)
{
	if(f == 0)
		return;
	uninstallsubfont(f);
	free(f->info);	/* note: f->info must have been malloc'ed! */
	freeimage(f->bits);
	free(f);
}
