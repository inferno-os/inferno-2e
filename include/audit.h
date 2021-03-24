void		(*auditalloc)(int, ulong, void *);
void		(*auditfree)(int, ulong, void *);
void		(*auditmemloc)(char *, void *);
#define AUDITALLOC(pool,pc,addr) {if(auditalloc)auditalloc(pool,pc,addr);}
#define AUDITFREE(pool,pc,addr)	 {if(auditfree)auditfree(pool,pc,addr);}

int		isvalid_pc(ulong);
int		isvalid_va(void *);
