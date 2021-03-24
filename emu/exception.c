#include "dat.h"
#include "fns.h"
#include "error.h"
#include "interp.h"
#include "isa.h"
#include "runt.h"
#include "kernel.h"

Inst ieclr = { IECLR, SRC(AXXX)|DST(AXXX) };

static void
restorepc(Exception *e, Exception *l)
{
	while(l != nil) {
		if(l->R.FP == e->R.FP)
			return;
		l = l->link;
	}
	*e->lrp = e->lr;
}

void
Sys_raise(void *a)
{
	char *e;
	F_Sys_raise *f;

	f = a;
	e = string2c(f->s);
	if(strlen(e) >= ERRLEN)
		e = "rescue message size";

	error(e);
}

void
Sys_rescue(void *a)
{
	Prog *p;
	Modlink *m;
	Frame *cf, *bf, *fr;
	Stkext *sx;
	F_Sys_rescue *f;
	Exception *e, *l;
	Sys_Exception *le;

	f = a;
	p = currun();
	cf = a;
	*f->ret = Sys_HANDLER;

	e = malloc(sizeof(Exception));
	if(e == nil)
		error("no memory");

	e->pattern = strdup(string2c(f->s));
	e->rptr = f->ret;
	memmove(&e->R, &R, sizeof(e->R));
	e->eptr = f->e;
	if(e->eptr != H) {
		le = f->e;
		D2H(le)->ref++;
		/* Populate these now because we could be
		 * reporting no memory
		 */
		destroy(le->mod);
		le->mod = newstring(sizeof(m->m->name));
		destroy(le->name);
		le->name = newstring(ERRLEN);
	}
	fr = (Frame*)R.FP;
	if(fr->t != nil)
		e->R.SP = R.FP;
	else {
		sx = SEXTYPE(fr);
		e->R.SP = sx->reg.SP;
		e->R.TS = sx->reg.TS;
		e->R.EX = sx->reg.EX;
	}
	e->R.PC = cf->lr;
	e->R.FP = cf->fp;
	if(cf->mr != nil)
		e->R.M = cf->mr;
	for(l = p->exsp; l != nil; l = l->link)
		if(l->R.FP == e->R.FP)
			break;

	bf = (Frame*)cf->fp;
	if(l == nil) {
		e->lrp = &bf->lr;
		e->lr = bf->lr;
		m = bf->mr;
		if(m == nil)
			m = cf->mr;
		if(m == nil)
			m = R.M;
		bf->lr = m->m->eclr;
	}
	else {
		e->lrp = l->lrp;
		e->lr = l->lr;
	}

	e->link = p->exsp;
	p->exsp = e;
}

void
Sys_rescued(void *a)
{
	Prog *p;
	F_Sys_rescued *f;
	char ebuf[ERRLEN];
	Exception *eh, *e, **l;

	f = a;
	p = currun();
	if(p->exhdlr == nil)
		return;

	switch(f->flag) {
	case Sys_ACTIVE:
		p->exhdlr = nil;
		return;
	case Sys_EXIT:
		error(up->env->error);
	case Sys_ONCE:
	case Sys_RAISE:
		eh = p->exhdlr;
		p->exhdlr = nil;
		l = &p->exsp;
		for(e = *l; e != nil; e = e->link) {
			if(e == eh) {
				*l = e->link;
				restorepc(e, p->exsp);
				expfree(e);
				return;
			}
			l = &e->link;
		}
		if(f->flag == Sys_ONCE)
			return;
		if(f->s != H)
			strncpy(ebuf, string2c(f->s), ERRLEN-1);
		else
			memmove(ebuf, up->env->error, ERRLEN);
		error(ebuf);
	}
}

void
Sys_unrescue(void *a)
{
	Prog *p;
	Exception *e, *n;
	Frame *cf;

	cf = a;
	p = currun();
	p->exhdlr = nil;
	e = p->exsp;
	if(e == nil || e->R.FP != cf->fp)
		return;
	n = e->link;
	if(n == nil || n->R.FP != e->R.FP)
		*e->lrp = e->lr;
	p->exsp = n;
	expfree(e);
}

static int
ematch(char *pat, char *exp)
{
	int l;

	if(strcmp(pat, exp) == 0)
		return 1;

	l = strlen(pat);
	if(l == 0)
		return 0;
	if(pat[l-1] == '*') {
		if(l == 1)
			return 1;
		if(strncmp(pat, exp, l-1) == 0)
			return 1;
	}
	return 0;
}

static void
popex(uchar *f)
{
	Prog *p;
	Exception *e;

	p = currun();
	while(p->exsp != nil) {
		e = p->exsp;
		if(e->R.FP != f)
			break;
		p->exsp = e->link;
		*e->lrp = e->lr;
		expfree(e);
	}
}

static void
unwind(REG *RF, REG *RT)
{
	Frame *f;
	Modlink *m;

	while(RF->FP != RT->FP) {
		popex(RF->FP);
		f = (Frame*)RF->FP;
		RF->FP = f->fp;
		RF->SP = (uchar*)f;
		RF->PC = f->lr;
		m = f->mr;
		if(f->t == nil)
			unextend(f);
		else if (f->t->np)
			freeptrs(f, f->t);
		if(m != nil) {
			destroy(RF->M);
			RF->M = m;
			RF->MP = m->MP;
		}
	}
}

int
activated(void *exfp, REG *reg)
{
	Type *t;
	Frame *f;
	Stkext *sx;
	uchar *fp, *sp, *ex;

	sp = reg->SP;
	ex = reg->EX;
	while(ex != nil) {
		sx = (Stkext*)ex;
		fp = sx->reg.tos.fu;
		while(fp != sp) {
			if(fp == exfp)
				return 1;
			f = (Frame*)fp;
			t = f->t;
			if(t == nil)
				t = sx->reg.TR;
			fp += t->size;
		}
		ex = sx->reg.EX;
		sp = sx->reg.SP;
	}
	return 0;
}

Inst*
unlinkex(void)
{
	Prog *p;
	Inst *i;
	Exception *e;

	p = currun();
	i = nil;
	while(p->exsp != nil) {
		e = p->exsp;
		if(activated(e->R.FP, &R))
			break;

		p->exsp = e->link;
		if(e->lr != nil)
			i = e->lr;

		expfree(e);
	}
	if(i == nil)
		error("bad unlinkex");
	return i;
}

static void
setstr(String *s, char *p, int len)
{
	if(s == H)
		return;
	if(s->len < 0 || s->max < len)
		return;

	strncpy(s->Sascii, p, len-1);
	s->len = strlen(p);
}

int
handler(char *estr)
{
	Prog *p;
	Exception *e;
	Sys_Exception *l;
	char name[NAMELEN];

	p = currun();
	if(p == nil || p->exhdlr != nil)
		return 0;

	for(e = p->exsp; e != nil; e = e->link)
		if(ematch(e->pattern, estr) && activated(e->R.FP, &R))
			break;

	if(e == nil)
		return 0;

	l = e->eptr;
	if(l != H) {
		l->pc = modstatus(&R, name, sizeof(name));
		setstr(l->mod, name, sizeof(name));
		setstr(l->name, estr, ERRLEN);
	}
	unwind(&R, &e->R);
	memmove(&p->R, &e->R, sizeof(R));
	*e->rptr = Sys_EXCEPTION;
	p->exhdlr = e;
	p->kill = nil;
	return 1;
}

void
closeexp(Prog *p)
{
	Exception *e;

	while(p->exsp != nil) {
		e = p->exsp;
		p->exsp = e->link;
		expfree(e);
	}
}

void
expfree(Exception *e)
{
	Sys_Exception *l;

	free(e->pattern);
	l = e->eptr;
	if(l != H)
		destroy(l);
	free(e);
}
