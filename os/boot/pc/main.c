#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"

Mach *m;

enum {					/* mode */
	Mauto		= 0x00,
	Mlocal		= 0x01,
	Manual		= 0x02,
	NMode		= 0x03,
};

typedef struct Mode Mode;

typedef struct Mode {
	char*	name;
	int	mode;
} Mode;

static Type types[Maxdev+1] = {
[Maxdev]	{	Tnone,
		0,
		0, 0, 0, 0,
		{ 0, },
	},
};

void
addboottype( Type *newtype )
{
static ntypes = 0;

	if(ntypes == Maxdev)
		panic("too many devices");
	memcpy( &types[ntypes], newtype, sizeof(Type));
	ntypes++;
}

static Medium media[Nmedia];
static Medium *curmedium = media;

static Mode modes[NMode+1] = {
	[Mauto]		{ "auto",   Mauto,  },
	[Mlocal]	{ "local",  Mlocal, },
	[Manual]	{ "manual", Manual, },
};

static char *inis[Nini+1];
static int nini = 0;
char **ini;

static int
parse(char *line, int *type, int *flag, int *dev, char *file)
{
	Type *tp;
	char buf[2*NAMELEN], *v[4], *p;
	int i;

	strcpy(buf, line);
	switch(getfields(buf, v, 4, '!')){

	case 3:
		break;

	case 2:
		v[2] = "";
		break;

	default:
		return 0;
	}

	*flag = 0;
	for(tp = types; tp->type != Tnone; tp++){
		for(i = 0; i < NName; i++){

			if(tp->name[i] == 0 || strcmp(v[0], tp->name[i]))
				continue;
			*type = tp->type;
			*flag |= 1<<i;

			if((*dev = strtoul(v[1], &p, 0)) == 0 && p == v[1])
				return 0;
		
			strcpy(file, v[2]);
		
			return 1;
		}
	}

	return 0;

}

static int
boot(Medium *mp, int flag, char *file)
{
	Dosfile df;
	char ixdos[128], *p;

	if(flag & Fbootp){
		sprint(BOOTLINE, "%s!%d", mp->type->name[Nbootp], mp->dev);
		return mp->type->boot(mp, file);
	}

	if(flag & Fdos){
		if(mp->type->setpart)
			(*mp->type->setpart)(mp->dev, "disk");
		if(mp->flag & Fini)
			dotini(mp);
		if(file == 0 || *file == 0){
			strcpy(ixdos, *ini);
			if(p = strrchr(ixdos, '/'))
				p++;
			else
				p = ixdos;
			strcpy(p, "9dos");
			if(dosstat(mp, ixdos, &df) <= 0)
				return -1;
		}
		else
			strcpy(ixdos, file);
		sprint(BOOTLINE, "%s!%d!%s", mp->type->name[Ndos], mp->dev, ixdos);
		return mp->type->boot(mp, ixdos);
	}

	return -1;
}

static Medium*
allocm(Type *tp)
{
	Medium **l;

	if(curmedium >= &media[Nmedia])
		return 0;

	for(l = &tp->media; *l; l = &(*l)->next)
		;
	*l = curmedium++;
	return *l;
}

Medium*
probe(int type, int flag, int dev)
{
	Type *tp;
	int dombr, i, start;
	Medium *mp;
	Dosfile df;
	Partition *pp;

	for(tp = types; tp->type != Tnone; tp++){
		if(type != Tany && type != tp->type)
			continue;

		if(flag != Fnone){
			for(mp = tp->media; mp; mp = mp->next){
				if((flag & mp->flag) && (dev == Dany || dev == mp->dev))
					return mp;
			}
		}

		if((tp->flag & Fprobe) == 0){
			tp->flag |= Fprobe;
			tp->mask = (*tp->init)();
		}

		for(i = 0; tp->mask; i++){
			if((tp->mask & (1<<i)) == 0)
				continue;
			tp->mask &= ~(1<<i);

			if((mp = allocm(tp)) == 0)
				continue;

			mp->dev = i;
			mp->flag = tp->flag;
			mp->seek = tp->seek;
			mp->read = tp->read;
			mp->type = tp;

			if(mp->flag & Fdos){
				start = 0;
				dombr = (mp->type->type != Tfloppy);
				if(mp->type->setpart){
					if(pp = (*mp->type->setpart)(i, "dos")){
						if(start = pp->start)
							dombr = 0;
					}
					(*tp->setpart)(i, "disk");
				}
				if(dosinit(mp, start, dombr) < 0)
					mp->flag &= ~(Fini|Fdos);
			}

			if(mp->flag & Fini){
				mp->flag &= ~Fini;
				for(ini = inis; *ini; ini++){
					if(dosstat(mp, *ini, &df) <= 0)
						continue;
					mp->flag |= Fini;
					break;
				}
			}

			if((flag & mp->flag) && (dev == Dany || dev == i))
				return mp;
		}
	}

	return 0;
}

void
main(void)
{
	Medium *mp;
	int dev, flag, i, mode, tried, type;
	char def[2*NAMELEN], file[2*NAMELEN], line[80], *p;
	Type *tp;

	i8042a20();
	m->pdb = (void*)CPU0PDB;
	mmuinit();
	trapinit();
	clockinit();
	alarminit();

	spllo();

	if((ulong)&end > (KZERO|(640*1024)))
		panic("i'm too big\n");

	links();

	/*
	 * If there were any arguments, MS-DOS leaves a character
	 * count followed by the arguments in the runtime header.
	 * Step over the leading space.
	 * 
 	 * 4/4/99 
	 * This behavior has proven inconsistent between revisions
	 * of DOS, so until a non-DOS boot loader can be developed
	 * I've removed the command line arguments capabilities.
	 *
	 */
/*
	p = (char*)0x80080080;
	if(p[0]){
		p[p[0]+1] = 0;
		p += 2;
	}
	else
*/
		p = 0;

	/*
	 * Look for special options on the command line,
	 * e.g. -scsi0=0x334
	 */
	if(p){
		while(*p){
			while(*p == ' ' || *p == '\t')
				p++;
			if(*p != '-')
				break;

			if(cistrncmp(p, "-ini=", 5) == 0){
				if(nini < Nini){
					p += 5;
					inis[nini] = p;
					while(*p && *p != ' ' && *p != '\t')
						p++;
					*p++ = 0;
					nini++;
					continue;
				}
			}

			while(*p && *p != ' ' && *p != '\t')
				p++;
		}
		if(*p == 0)
			p = 0;
	}

	if(nini == 0){
		inis[0] = "inferno/inferno.ini";
		inis[1] = "inferno.ini";
		inis[2] = "plan9.ini";
		nini = 3;
	}

	for(tp = types; tp->type != Tnone; tp++){
		if(tp->type == Tether)
			continue;
		if((mp = probe(tp->type, Fini, Dany)) && (mp->flag & Fini)){
			dotini(mp);
			break;
		}
	}

	consinit();

	tried = 0;
	mode = Mauto;
	if(p == 0)
		p = getconf("bootfile");

	if(p != 0) {
		mode = Manual;
		for(i = 0; i < NMode; i++){
			if(strcmp(p, modes[i].name) == 0){
				mode = modes[i].mode;
				goto done;
			}
		}
		if(parse(p, &type, &flag, &dev, file) == 0) {
			print("Bad bootfile syntax: %s\n", p);
			goto done;
		}
		mp = probe(type, flag, dev);
		if(mp == 0) {
			print("Cannot access device: %s\n", p);
			goto done;
		}
		tried = boot(mp, flag, file);
	}
done:
	if(tried == 0 && mode != Manual){
		flag = Fany;
		if(mode == Mlocal)
			flag &= ~Fbootp;
		if((mp = probe(Tany, flag, Dany)) && mp->type->type != Tfloppy)
			boot(mp, flag & mp->flag, 0);
	}

	def[0] = 0;
	probe(Tany, Fnone, Dany);
	if(mode != Manual){
		if((mp = probe(Tfloppy, Fdos, Dany)))
			sprint(def, "%s!%d!ipc", mp->type->name[Ndos], mp->dev);
	}
	else if(p = getconf("bootdef"))
		strcpy(def, p);
	flag = 0;
	for(tp = types; tp->type != Tnone; tp++){
		for(mp = tp->media; mp; mp = mp->next){
			if(flag == 0){
				flag = 1;
				print("Boot devices:");
			}
	
			if(mp->flag & Fbootp)
				print(" %s!%d", mp->type->name[Nbootp], mp->dev);
			if(mp->flag & Fdos)
				print(" %s!%d", mp->type->name[Ndos], mp->dev);
		}
	}
	if(flag)
		print("\n");

	for(;;){
		if(getstr("boot from", line, sizeof(line), def, mode != Manual) >= 0){
			if(parse(line, &type, &flag, &dev, file)){
				if(mp = probe(type, flag, dev))
					boot(mp, flag, file);
			}
		}
		def[0] = 0;
	}
}

int
getfields(char *lp, char **fields, int n, char sep)
{
	int i;

	for(i = 0; lp && *lp && i < n; i++){
		while(*lp == sep)
			*lp++ = 0;
		if(*lp == 0)
			break;
		fields[i] = lp;
		while(*lp && *lp != sep){
			if(*lp == '\\' && *(lp+1) == '\n')
				*lp++ = ' ';
			lp++;
		}
	}
	return i;
}

int
cistrcmp(char *a, char *b)
{
	int ac, bc;

	for(;;){
		ac = *a++;
		bc = *b++;
	
		if(ac >= 'A' && ac <= 'Z')
			ac = 'a' + (ac - 'A');
		if(bc >= 'A' && bc <= 'Z')
			bc = 'a' + (bc - 'A');
		ac -= bc;
		if(ac)
			return ac;
		if(bc == 0)
			break;
	}
	return 0;
}

int
cistrncmp(char *a, char *b, int n)
{
	unsigned ac, bc;

	while(n > 0){
		ac = *a++;
		bc = *b++;
		n--;

		if(ac >= 'A' && ac <= 'Z')
			ac = 'a' + (ac - 'A');
		if(bc >= 'A' && bc <= 'Z')
			bc = 'a' + (bc - 'A');

		ac -= bc;
		if(ac)
			return ac;
		if(bc == 0)
			break;
	}

	return 0;
}

void*
ialloc(ulong n, int align)
{

	static ulong palloc;
	ulong p;
	int a;

	if(palloc == 0)
		palloc = 2*1024*1024;

	p = palloc;
	if(align <= 0)
		align = 4;
	if(a = n % align)
		n += align - a;
	if(a = p % align)
		p += align - a;

	palloc = p+n;

	return memset((void*)(p|KZERO), 0, n);
}

static Block *allocbp;

Block*
allocb(int size)
{
	Block *bp, **lbp;
	ulong addr;

	lbp = &allocbp;
	for(bp = *lbp; bp; bp = bp->next){
		if((bp->lim - bp->base) >= size){
			*lbp = bp->next;
			break;
		}
		lbp = &bp->next;
	}
	if(bp == 0){
		bp = ialloc(sizeof(Block)+size+64, 0);
		addr = (ulong)bp;
		addr = ROUNDUP(addr + sizeof(Block), 8);
		bp->base = (uchar*)addr;
		bp->lim = ((uchar*)bp) + sizeof(Block)+size+64;
	}

	if(bp->flag)
		panic("allocb reuse\n");

	bp->rp = bp->base;
	bp->wp = bp->rp;
	bp->next = 0;
	bp->flag = 1;

	return bp;
}

void
freeb(Block* bp)
{
	bp->next = allocbp;
	allocbp = bp;

	bp->flag = 0;
}

enum {
	Paddr=		0x70,	/* address port */
	Pdata=		0x71,	/* data port */
};

uchar
nvramread(int offset)
{
	outb(Paddr, offset);
	return inb(Pdata);
}
