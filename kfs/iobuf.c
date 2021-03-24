#include	"all.h"

#define	DEBUG	0
#define	safefs	0

long	niob;
long	nhiob;
Hiob*	hiob;

Iobuf*
getbuf(Device dev, long addr, int flag)
{
	Iobuf *p, *s;
	Hiob *hp;
	long h;

	if(DEBUG)
		print("getbuf %Z(%ld) f=%x\n", dev, addr, flag);
	h = addr +
		dev.type*1009L +
		dev.ctrl*10007L +
		dev.unit*100003L +
		dev.part*1000003L;
	if(h < 0)
		h = ~h;
	h %= nhiob;
	hp = &hiob[h];

loop:
	lock(&(hp->lock));

/*
 * look for it in the active list
 */
	s = hp->link;
	for(p=s;;) {
		if(p->addr == addr && !devcmp(p->dev, dev)) {
			if(p != s) {
				p->back->fore = p->fore;
				p->fore->back = p->back;
				p->fore = s;
				p->back = s->back;
				s->back = p;
				p->back->fore = p;
				hp->link = p;
			}
			unlock(&(hp->lock));
			qlock(&(p->qlock));
			if(p->addr != addr || devcmp(p->dev, dev)) {
				qunlock(&(p->qlock));
				goto loop;
			}
			p->flags |= flag;
			fscons.bhit.count++;
			p->iobuf = p->xiobuf;
			return p;
		}
		p = p->fore;
		if(p == s)
			break;
	}
	if(flag & Bprobe) {
		unlock(&(hp->lock));
		return 0;
	}

/*
 * not found
 * take oldest unlocked entry in this queue
 */
xloop:
	p = s->back;
	if(!canqlock(&(p->qlock))) {
		if(p == hp->link) {
			unlock(&(hp->lock));
			print("iobuf all locked\n");
			goto loop;
		}
		s = p;
		goto xloop;
	}
	/*
	 * its dangerous to flush the pseudo
	 * devices since they recursively call
	 * getbuf/putbuf. deadlock!
	 */
	if(p->flags & Bres) {
		qunlock(&(p->qlock));
		if(p == hp->link) {
			unlock(&(hp->lock));
			print("iobuf all resed\n");
			goto loop;
		}
		s = p;
		goto xloop;
	}
	if(p->flags & Bmod) {
		unlock(&(hp->lock));
		if(isro(p->dev) || !devwrite(p->dev, p->addr, p->xiobuf))
				p->flags &= ~(Bimm|Bmod);
		qunlock(&(p->qlock));
		goto loop;
	}
	hp->link = p;
	p->addr = addr;
	p->dev = dev;
	p->flags = flag;
	unlock(&(hp->lock));
	p->iobuf = p->xiobuf;
	if(flag & Bread) {
		if(devread(p->dev, p->addr, p->iobuf)) {
			p->flags = 0;
			p->dev = devnone;
			p->addr = -1;
			p->iobuf = (char*)-1;
			qunlock(&(p->qlock));
			return 0;
		}
		fscons.bread.count++;
		return p;
	}
	fscons.binit.count++;
	return p;
}

/*
 * syncblock tries to put out a block per hashline
 * returns 0 all done,
 * returns 1 if it missed something
 */
int
syncblock(void)
{
	Iobuf *p, *s, *q;
	Hiob *hp;
	long h;
	int flag;

	flag = 0;
	for(h=0; h<nhiob; h++) {
		q = 0;
		hp = &hiob[h];
		lock(&(hp->lock));
		s = hp->link;
		for(p=s;;) {
			if(p->flags & Bmod) {
				if(q)
					flag = 1;	/* more than 1 mod/line */
				q = p;
			}
			p = p->fore;
			if(p == s)
				break;
		}
		unlock(&(hp->lock));
		if(q) {
			if(!canqlock(&(q->qlock))) {
				flag = 1;		/* missed -- was locked */
				continue;
			}
			if(!(q->flags & Bmod)) {
				qunlock(&(q->qlock));
				continue;
			}
			if(isro(q->dev)||!devwrite(q->dev, q->addr, q->xiobuf))
				q->flags &= ~(Bmod|Bimm);
			qunlock(&(q->qlock));
		}
	}
	return flag;
}

void
sync(char *reason)
{
	long i;

/*	print("sync: %s\n", reason); */
	for(i=10*nhiob; i>0; i--)
		if(!syncblock())
			return;
/*	print("sync shorted\n"); */
}

void
putbuf(Iobuf *p)
{
	if(canqlock(&(p->qlock)))
		print("buffer not locked %Z(%ld)\n", p->dev, p->addr);
	if(p->flags & Bimm || p->flags & Bmod && safefs) {
		if(!(p->flags & Bmod))
			print("imm and no mod %Z(%ld)\n", p->dev, p->addr);
		if(isro(q->dev)||!devwrite(p->dev, p->addr, p->iobuf))
			p->flags &= ~(Bmod|Bimm);
	}
	p->iobuf = (char*)-1;
	qunlock(&(p->qlock));
}

int
checktag(Iobuf *p, int tag, long qpath)
{
	Tag *t;

	t = (Tag*)(p->iobuf+BUFSIZE);

	if(t->tag != tag) {
		if(1 || CHAT(0))
			print("	tag = %T; expected %T\n",
				t->tag, tag);
		return 2;
	}
	if(qpath != QPNONE) {
		qpath &= ~QPDIR;
		if(qpath != t->path) {
			if(qpath == (t->path&~QPDIR))	/* old bug */
				return 0;
			if(1 || CHAT(0))
				print("	tag/path = %lux; expected %T/%lux\n",
					t->path, tag, qpath);
			return 1;
		}
	}
	return 0;
}

void
settag(Iobuf *p, int tag, long qpath)
{
	Tag *t;

	t = (Tag*)(p->iobuf+BUFSIZE);
	t->tag = tag;
	if(qpath != QPNONE) {
		t->path = qpath & ~QPDIR;
	}
	p->flags |= Bmod;
}
