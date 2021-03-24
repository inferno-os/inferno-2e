.include "../../mkconfig.nmk"
.include "../../$(SYSHOST)/makehost"
.include "../../$(OBJDIR)/Makefile"

TARG=mk
LIBS=\
	-lregexp\
	-lbio\
	-l9\

OFILES=\
	arc.$O\
	bufblock.$O\
	dir.$O\
	graph.$O\
	job.$O\
	lex.$O\
	main.$O\
	match.$O\
	mk.$O\
	os-$(SYSHOST).$O\
	parse.$O\
	recipe.$O\
	rule.$O\
	run.$O\
	shprint.$O\
	symtab.$O\
	xt_ar.$O\
	xt_driver.$O\
	xt_file.$O\
	var.$O\
	varsub.$O\
	word.$O\

HFILES=\
	ar.h\
	fns.h\
	mk.h\

.include "../../$(SYSTARG)/makeone"

CC_LOCAL=	-I.

package:
	$(TRUE)
