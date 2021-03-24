// monconf: SA1100 monitor configuration and examination

#include <lib9.h>
#include <bio.h>
#include "monconf.h"

#define DFLT_OSC_HZ	3686400
#define DRAMLIST_OFS	0x50

typedef struct OldMonData OldMonData;
struct OldMonData {
	MonMisc	misc;
	ulong	dramptr0;
	ulong	dramnull;
	ulong	mmapnull;
};

typedef struct Mon Mon;
struct Mon {
	char	*fname;
	uchar 	*b;		/* buffer for full image */
	int	n;		/* length of buffer */
	ulong	base;		/* base address for image (link addr) */
	int	ctab;		/* ofs to config table */
	MonMisc *misc;
	ulong	*dramconf;
	ulong	*mmapconf;
	OldMonData old;
};


ulong
getulong(void *a)
{
	uchar *m = (uchar*)a;
	return m[0]|(m[1]<<8)|(m[2]<<16)|(m[3]<<24);
}

ushort
getushort(void *a)
{
	uchar *m = (uchar*)a;
	return m[0]|(m[1]<<8);
}

void
putulong(void *a, ulong v)
{
	uchar *m = (uchar*)a;
	m[0]=v&0xff;
	m[1]=(v>>8)&0xff;
	m[2]=(v>>16)&0xff;
	m[3]=(v>>24)&0xff;
}

void
putushort(void *a, ushort v)
{
	uchar *m = (uchar*)a;
	m[0]=v&0xff;
	m[1]=(v>>8)&0xff;
}

ulong*
monptr(Mon *m, ulong *p)
{
	ulong v = getulong(p);
	if(!v)
		return (ulong*)0;
	else
		return (ulong*)(m->b+v-m->base);
}

int
moninit_old(Mon *m, int skipdram)
{
	int i, z;
	memset(&m->old.misc, 0, sizeof m->old.misc);
	m->misc = &m->old.misc;
	if(skipdram)
		goto gotdram;
	m->dramconf = &m->old.dramptr0;
	m->mmapconf = &m->old.mmapnull;
	putulong(&m->old.dramptr0, 0);
	putulong(&m->old.dramnull, 0);
	putulong(&m->old.mmapnull, 0);
	for(i=0; i<0x1000; i += 4)
		if(getulong(m->b+i) == 0x1e) {
			z = i-6*4;
			z += m->base;
			print("Looks like DRAM config at 0x%ux...\n", z);
			putulong(&m->old.dramptr0, z);
			goto gotdram;
		}
	print("Couldn't find DRAM table\n");
	return -1;
    gotdram:
	for(i=0; i<0x4000; i += 4) {
		ulong v = getulong(m->b+i);
		if((v&0xfffff000) == 0xe59f0000) {
			int n, s;
			v = getulong(m->b+i+(v&0xfff)+8);
			if(v != 0x90020014 && v != 0x90020000)
				continue;
			n = getulong(m->b+i+4) & 0xff;
			s = ((n*4)+16)*DFLT_OSC_HZ;
			putulong(&m->misc->cpuspeed_ppcr, n);
			putulong(&m->misc->cpuspeed_hz, s);
			goto gotcpu;
		}
	}
	print("Can't find CPU speed (pretending it's 100MHz)\n");
	putulong(&m->misc->cpuspeed_hz, 100000000);
    gotcpu:
	return 0;
}


int
moninit(Mon *m, void *b, int n)
{
	ulong di;
	ulong *p;
	ulong v;

	USED(n);
	m->b = b;
	m->ctab = 0;
	m->base = 0x08000000;	/* default link base */
	for(di=0; di<0x100; di += 4)
		if(getulong(m->b+di) == MONCTAB_MAGIC) 
			m->ctab = di;
	if(!m->ctab) {
		print("No DRAM config list found (old monitor?)\n");
		return moninit_old(m, 0);
	}
	p = (ulong*)(m->b+m->ctab);
	v = getulong(&p[-2])&0xfffff000;
	if(v == 0xcd1fa000 || v == 0xe51ff000) {
		moninit_old(m, 1);
	} else {
		m->base = getulong(&p[-1]);
		m->misc = (MonMisc*)monptr(m, &p[-2]);
	}
	m->dramconf = (ulong*)(m->b+m->ctab+4);
	p = m->dramconf;
	while(*p++)
		;
	m->mmapconf = p;
	return 0;
}


void
mmap_dump(Mon *m, MmapConf *mc)
{
	ulong va, vs, pa, ps, fl;
	USED(m);
	print("virtual addr      physical addr     flags   description\n");
	print("----------------- ----------------- ------- -----------\n");
	while((vs = getulong(&mc->vs))) {
		char fs[5];
		char ds[40];
		ps = vs;
		va = getulong(&mc->va);
		pa = getulong(&mc->pa);
		fl = getulong(&mc->fl);
		if(fl&0x80000000)
			ps = 0xe0000000-pa;
		strcpy(fs, "");
		if(fl&(1<<4))
			strcat(fs, "U");
		if(fl&(1<<3))
			strcat(fs, "C");
		if(fl&(1<<2))
			strcat(fs, "B");
		sprint(fs+strlen(fs), "%d", (fl>>10)&3);
		if(pa < 0x20000000)
			sprint(ds, "static bank %d", pa/0x08000000);
		else if(pa < 0xc0000000)
			sprint(ds, "I/O space");
		else if(pa < 0xe0000000)
			sprint(ds, "DRAM");
		else if(pa < 0xf0000000)
			sprint(ds, "zero bank");
		else
			sprint(ds, "?");
		if((fl&~3) == 0)
			sprint(ds, "invalid");
		print("%8.8ux-%8.8ux %8.8ux-%8.8ux %2d,%-4s %s\n",
			va, va+vs-1, pa, pa+ps-1,
			(fl>>5)&0xf, fs, ds);
		++mc;
	}
}

void
dram_dump(Mon *m, DramConf *dc)
{
	char *romtype[] = {"Non-burst ROM or FLASH EPROM",
			   "Non-burst ROM or SRAM",
			   "Burst-of-four ROM",
			   "Burst-of-eight ROM"};

	ulong md = getulong(&dc->mdcnfg);
	ulong m0, m1, m2;
	char b[96];
	int x;
	int cdiv;
	int cpuclk=getulong(&m->misc->cpuspeed_hz); 
	int memclk=cpuclk/2;
	double r;
	int ra;
	int rows;
	int i;

	if(!md)
		return;

	print("MemClk=%d (~%dMHz, %8.3fns/cycle)\n",
		memclk, (memclk+500000)/1000000,
		1.0/(double)memclk*1000000000.0);
	print("MDCNFG=0x%8.8ux\n", md);
	print("\tDE: 0x%ux \t(banks %s%s%s%s)\n", md&0xf,
		(md&1) ? "0 " : "",
		(md&2) ? "1 " : "",
		(md&4) ? "2 " : "",
		(md&8) ? "3 " : "");
	x = (md>>4)&3;
	ra=9+x;
	rows = (1<<ra);
	print("\tDRAC: 0x%ux \t(%dx? rows=%d)\n", x, ra, rows);
	cdiv = (md>>6)&1;
	print("\tCDB2: 0x%ux \t(%s)\n", cdiv,
		cdiv ? "CAS shift by memclk" : "CAS shift by cpuclk");
	x = (md>>7)&0xf;
	print("\tTRP: 0x%ux \t(RAS precharge=%8.3fns)\n",
		x, (double)(x+1)/(double)memclk*1000000000.0);
	x = (md>>11)&0xf;
	print("\tTRASR: 0x%ux \t(RAS assertion during CBR=%8.3fns)\n",
		x, (double)(x+1)/(double)memclk*1000000000.0);
	x = (md>>15)&0x3;
	print("\tTDL: 0x%ux \t(input latch after CAS deassert=%8.3fns)\n",
		x, (double)x/(double)memclk*1000000000.0);
	x = (md>>17)&0x7fff;
	r = (double)(x*4)/(double)memclk;
	print("\tDRI: 0x%ux \t(refresh per row=%8.3fus, full=%8.3fms)\n",
		x, r*1000000.0, rows*r*1000.0);

	m0 = getulong(&dc->mdcas[0]);
	m1 = getulong(&dc->mdcas[1]);
	m2 = getulong(&dc->mdcas[2]);
	print("MDCAS0=0x%8.8ux MDCAS1=0x%8.8ux MDCAS2=0x%8.8ux (%8.3fns per bit)\n",
		m0, m1, m2, 1.0/(cpuclk/(1+cdiv))*1000000000.0);
	for(i=0; i<32; i++) {
		b[i] = (m0&(1<<i)) != 0;
		b[32+i] = (m1&(1<<i)) != 0;
		b[64+i] = (m2&(1<<i)) != 0;
	}
	print("\t");
	for(i=0; i<96; i++)
		print("%c", b[i] ? '1' : '.');
	print("\n");

	for(i=0; i<4; i++) {
		ushort s = getushort(&dc->msc[i]);
		print("Static Bank %d: 0x%4.4ux\n", i, s);
		x = s&3;
		print("\tRT: 0x%ux (%s)\n", x, romtype[x]);
		x = (s>>2)&1;
		print("\tRBW: 0x%ux (%d bits wide)\n", x, 16*(x+1));
		x = (s>>3)&0x1f;
		print("\tRDF: 0x%ux (first access delay=%8.3fns)\n", x,
			(double)(x+1)/(double)memclk*1000000000.0);
		x = (s>>8)&0x1f;
		print("\tRDN: 0x%ux (next access delay=%8.3fns)\n", x,
			(double)(x+1)/(double)memclk*1000000000.0);
		x = (s>>13)&0x7;
		print("\tRRR: 0x%ux (recovery time=%8.3fns)\n", x,
			(double)(x+1)/(double)memclk*1000000000.0);
	}
	print("\n\n");
}


char*
montypename(int n)
{
	switch(n) {
	case 1:	return "demon";
	case 3: return "angel";
	case 5: return "styxmon";
	default: return "unknown";
	}
}

void
monview(Mon *m)
{
	int s;
	ulong *p;
	DramConf *dc;
	MmapConf *mc;
	char *n;

	s = getulong(&m->misc->cpuspeed_hz);
	print("Monitor type: %d (%s)\n", m->misc->montype, montypename(m->misc->montype));
	if(n = (char*)monptr(m, &m->misc->monname))
		print("Monitor name: %s v%d.%d%c\n",
			n,
			m->misc->monver_major, 
			m->misc->monver_minor,
			m->misc->monver_patch);
	print("Code base: %8.8ux\n", m->base);
	if(m->misc->flashbase)
		print("Flash base: %8.8ux\n", getulong(&m->misc->flashbase));
	print("\n");
	print("PPCR=%d CpuClk=%d (~%dMHz, %8.3fns/cycle)\n",
		getulong(&m->misc->cpuspeed_ppcr),
		s, (s+500000)/1000000,
		1.0/(double)s*1000000000.0); 
	p = m->dramconf;
	while((dc = (DramConf*)monptr(m, p++))) {
		print("DRAM config at: %ux\n", (uchar*)dc-m->b+m->base);
		dram_dump(m, dc);
	}
	print("\n");
	p = m->mmapconf;
	while((mc = (MmapConf*)monptr(m, p++))) {
		print("Mmap config at: %ux\n", (uchar*)mc-m->b+m->base);
		mmap_dump(m, mc);
	}
}


char *
getenv_err(char *s)
{
	char *v = getenv(s);
	if(!v) {
		print("Error: %s not set\n", s);
		exits("fail");
	}
	return v;
}

char *
getenv2_err(char *p, char *s)
{
	char buf[100];
	char *v;
	sprint(buf, "%s%s", p, s);
	v = getenv(buf);
	if(!v) {
		print("Error: %s not set\n", buf);
		exits("fail");
	}
	return v;
}

double
uconv(char *s, double m, double a)
{
	double n = strtoll(s, &s, 0);
	if(*s == '.') {
		char c;
		double d = 1.0;
		while((c = *++s) >= '0' && c <= '9') 
			n += (c-'0')*(d /= 10.0);
	}
	switch(*s) {
	case 'G': n *= 1000000000.0; break;
	case 'M': n *= 1000000.0; break;
	case 'K': n *= 1000.0; break;
	case 'm': n /= 1000.0; break;
	case 'u': n /= 1000000.0; break;
	case 'n': n /= 1000000000.0; break;
	case 0:
		return n;
	}
	return (double)m*n+a;
}


void
dram_memcfg(Mon *m, int n)
{
	int oschz = uconv(getenv_err("OSCHZ"), 1, 0);
	int ppcr = uconv(getenv_err("PPCR"), 1, 0);
	int cpuclk = ((ppcr*4)+16)*oschz;
	int memclk = cpuclk/2;
	ulong mdcnfg;
	ulong mdcas[3];
	ushort sbank[4];
	char buf[40];
	char *s;
	int i, sb;
	DramConf *dc;

	if((s = getenv("MDCNFG")))
		mdcnfg = strtoll(s, 0, 0);
	else {
		int de = strtoll(getenv_err("MDCNFG_DE"), 0, 0);
		int rows = strtoll(getenv_err("MDCNFG_ROWS"), 0, 0);
		int cdb2 = strtoll(getenv_err("MDCNFG_CDB2"), 0, 0);
		int trp = uconv(getenv_err("MDCNFG_TRP"), memclk, -0.0001);
		int trasr = uconv(getenv_err("MDCNFG_TRASR"), memclk, -0.0001);
		int tdl = uconv(getenv_err("MDCNFG_TDL"), memclk, 0.9999);
		int drac = rows-9;
		int dri;
		if((s = getenv("MDCNFG_DRI")))
			dri = uconv(s, memclk/4, 0);
		else {
			double tdri = uconv(getenv_err("MDCNFG_TDRI"), 1, 0);
			dri = tdri/(1<<rows)*memclk/4.0;
		}
		mdcnfg = (de&0xf)|((drac&0x3)<<4)
			|((cdb2&1)<<6)|((trp&0xf)<<7)
			|((trasr&0xf)<<11)|((tdl&0x3)<<15)
			|((dri&0x7fff)<<17);
	}

	if((s = getenv("MDCAS0"))) {
		mdcas[0] = strtoll(s, 0, 0);
		mdcas[1] = strtoll(getenv_err("MDCAS1"), 0, 0);
		mdcas[2] = strtoll(getenv_err("MDCAS2"), 0, 0);
	} else {
		int n, b;
		s = getenv("MDCAS");
		for(n=0; n<3; n++)
			for(b=0; b<32; b++) {
				ulong v = 1;
				if(*s) {
					if(*s != '1')
						v = 0;
					s++;
				}
				mdcas[n] = (mdcas[n]&~(1UL<<b))|(v<<b);
			}
	}

	for(sb=0; sb<4; sb++) {
		char ps[40];
		sprint(buf, "SBANK%d", sb);
		s = getenv_err(buf);
		if(*s >= '0' && *s <= '9') {
			sbank[sb] = strtoll(s, 0, 0);
			continue;
		}
		sprint(ps, "SBANK_%s", s);
		if((s = getenv(ps))) {
			sbank[sb] = strtoll(s, 0, 0);
		} else {
			int rt = strtoll(getenv2_err(ps, "_RT"), 0, 0);
			int rbw = strtoll(getenv2_err(ps, "_RBW"), 0, 0);
			int rdf = uconv(getenv2_err(ps, "_RDF"),
						memclk, -0.0001);
			int rdn = uconv(getenv2_err(ps, "_RDN"),
						memclk, -0.0001);
			int rrr = uconv(getenv2_err(ps, "_RRR"),
						memclk/2, 0.9999);
			sbank[sb] = (rt&3)|((rbw&1)<<2)
					|((rdf&0x1f)<<3)
					|((rdn&0x1f)<<8)
					|((rrr&7)<<13);
		}
	}

	dc = (DramConf*)monptr(m, &m->dramconf[n]);
	if(!dc) {
		print("error: no place to store memcfg bank %d\n", n);
		return;
	}
	putulong(&dc->mdcnfg, mdcnfg);
	for(i=0; i<3; i++)
		putulong(&dc->mdcas[i], mdcas[i]);
	for(i=0; i<4; i++)
		putushort(&dc->msc[i], sbank[i]);

	putulong(&m->misc->cpuspeed_ppcr, ppcr);
	putulong(&m->misc->cpuspeed_hz, cpuclk);
}


void
readcmds(Mon *m)
{
	char *p;
	MmapConf *mc;
	char buf[256];
	char orig[256];
	Biobuf *bin;
	enum { NONE, MEMCFG, MEMMAP } type = NONE;
	int memcfgbank = 0;
	int memmapbank = 0;
	mc = (MmapConf*)monptr(m, m->mmapconf);
	if(!mc) {
		print("error: no place for mmap table\n");
		return;
	}
	bin = malloc(sizeof *bin);
	if(Binit(bin, 0, OREAD)) {
		free(bin);
		print("cannot open stdin\n");
		return;
	}
	while((p = Brdline(bin, '\n'))) {
		char *v;
		int n = BLINELEN(bin);
		memcpy(buf, p, n);
		buf[n] = '\0';
		p = strchr(buf, '#');
		strcpy(orig, buf);
		if(p)
			*p = '\0';
		p = v = buf;
		while(*v) {
			if(*v <= ' ')
				memmove(v, v+1, strlen(v));
			else
				++v;
		}
		v = p+strlen(buf)-1;
		while(v >= p && *v <= ' ')
			v--;
		v[1] = '\0';

		if(strcmp(p, "memcfg{") == 0) {
			type = MEMCFG;
			continue;
		}
		if(strcmp(p, "memmap{") == 0) {
			type = MEMMAP;
			continue;
		}
		if(buf[0] == '}') {
			switch(type) {
			case MEMCFG:
				dram_memcfg(m, memcfgbank++);
				break;
			case MEMMAP:
				memset(mc, 0, sizeof *mc);
				break;
			case NONE:
				print("syntax error: `}' without `}'\n");
			}
			type = NONE;
			continue;
		}
		if(type == MEMMAP) {
			ulong va, vs, pa, ps;
			char fs[8];
			int dom;
			ulong fl; 
			char *f;
			int err = 0;
			if(!*p)
				continue;

			v = orig;
			va = strtoll((f=v), &v, 16);
			err |= (f == v);
			while(strchr(" \t-", *v))
				++v;
			vs = strtoll((f=v), &v, 16);
			err |= (f == v);
			while(strchr(" \t", *v))
				++v;

			pa = strtoll((f=v), &v, 16);
			err |= (f == v);
			while(strchr(" \t-", *v))
				++v;
			ps = strtoll((f=v), &v, 16);
			err |= (f == v);
			while(strchr(" \t", *v))
				++v;
			
			dom = strtoll((f=v), &v, 0);
			err |= (f == v);
			while(strchr(" \t,", *v))
				++v;
			
			f = fs;	
			while(*v > 32)
				*f++ = *v++;

			if(err) {
				print("syntax error: %s\n", orig);
				continue;
			}
			fl = (dom<<5)|0x2;
			if(pa == 0xc0000000 && ps == 0xdfffffff) {
				fl |= 0x80000000;
				ps = pa+(vs-va);
			}
			if(ps-pa != vs-va) 
				print("error: (%ux-%ux) != (%ux-%ux)\n",
					vs, va, ps, pa);
			vs = (vs-va+1);
//			ps = (ps-pa+1);
			for(f = fs; *f; f++)
				switch(*f) {
				case 'U': fl |= (1<<4); break;
				case 'C': fl |= (1<<3); break;
				case 'B': fl |= (1<<2); break;
				case '0': case '1': case '2': case '3':
					fl |= (*f-'0')<<10;
				}
			putulong(&mc->va, va);
			putulong(&mc->vs, vs);
			putulong(&mc->pa, pa);
			putulong(&mc->fl, fl);
			++mc;
			++memmapbank;
			continue;
		}
		v = strchr(p, '=');
		if(v) 
			putenv(strdup(p));
		else if(*p)
			print("syntax error: %s\n", p);
	}

	print("wrote %d memcfg banks\n", memcfgbank);
	print("wrote %d memmap banks\n", memmapbank);
	if(p = getenv("FLASHBASE")) 
		putulong(&m->misc->flashbase, strtoll(p, 0, 0));
	if(p = getenv("GPDR")) 
		putulong(&m->misc->gpdr, strtoll(p, 0, 0));
	if(p = getenv("GAFR")) 
		putulong(&m->misc->gafr, strtoll(p, 0, 0));
	if(p = getenv("GPSR"))
		putulong(&m->misc->gpsr, strtoll(p, 0, 0));
	if(p = getenv("NOAUTO_ADDR"))
		putulong(&m->misc->noauto_addr, strtoll(p, 0, 0));
	if(p = getenv("NOAUTO_MASK"))
		putulong(&m->misc->noauto_mask, strtoll(p, 0, 0));
	if(p = getenv("NOAUTO_VAL"))
		putulong(&m->misc->noauto_val, strtoll(p, 0, 0));
}


int
fconv(va_list *arg, Fconv *f1)
{
	char buf[40];
	double d;
	d = va_arg(*arg, double) + 0.0005;
	sprint(buf, "%d.%3.3d", (int)d, ((int)(d*100))%100);
	f1->f2 = -1000;
	strconv(buf, f1);
	return 0;
}


int
main(int argc, char **argv)
{
	char buf[16384];
	int fd;
	Mon m;
	int n;
	int view = 0;

	fmtinstall('f', fconv);

	ARGBEGIN {
	default:
		print("%c: unknown option\n", ARGC());
		return -1;
	case 'v':
		view = 1;
		break;
	} ARGEND

	if(*argv == 0) {
		print("usage: monconf [-v] monitor\n");
		return -1;
	}
	if((fd = open(*argv, OREAD)) < 0) {
		perror(*argv);
		return -1;
	}
	n = read(fd, buf, sizeof buf);
	close(fd);

	if(moninit(&m, buf, n) < 0)
		return -1;

	if(view) {
		monview(&m);
	} else {
		readcmds(&m);

		if((fd = open(*argv, OWRITE)) < 0) {
			perror(*argv);
			return -1;
		}
		write(fd, buf, n);
		close(fd);
	}
	return 0;
}

