#include "u.h"
#include "lib.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"

#include "etherif.h"

static Ether ether[MaxEther];

extern int ether21140reset(Ether*);
extern int elnk3reset(Ether*);
extern int i82557reset(Ether*);

static struct {
	char	*type;
	int	(*reset)(Ether*);
} cards[10];

void
addethercard(char* t, int (*r)(Ether*))
{
	static int ncard;

	if(ncard == MaxEther)
		panic("too many ether cards");
	cards[ncard].type = t;
	cards[ncard].reset = r;
	ncard++;
}


int
etherinit(void)
{
	Ether *ctlr;
	int ctlrno, i, mask, n, x;

	mask = 0;
	x = splhi();
	for(ctlrno = 0; ctlrno < MaxEther; ctlrno++){
		ctlr = &ether[ctlrno];
		memset(ctlr, 0, sizeof(Ether));
		if(isaconfig("ether", ctlrno, ctlr) == 0)
			continue;
		for(n = 0; cards[n].type; n++){
			if(strcmp(cards[n].type, ctlr->type))
				continue;
			ctlr->ctlrno = ctlrno;
			if((*cards[n].reset)(ctlr))
				break;

			ctlr->state = 1;
			mask |= 1<<ctlrno;
			if(ctlr->irq == 2)
				ctlr->irq = 9;
			setvec(Int0vec + ctlr->irq, ctlr->interrupt, ctlr);

			print("ether#%d: %s: port 0x%luX irq %d",
				ctlr->ctlrno, ctlr->type, ctlr->port, ctlr->irq);
			if(ctlr->mem)
				print(" addr 0x%luX", ctlr->mem & ~KZERO);
			if(ctlr->size)
				print(" size 0x%luX", ctlr->size);
			print(":");
			for(i = 0; i < sizeof(ctlr->ea); i++)
				print(" %2.2uX", ctlr->ea[i]);
			print("\n");
		
			if(ctlr->nrb == 0)
				ctlr->nrb = Nrb;
			ctlr->rb = ialloc(sizeof(RingBuf)*ctlr->nrb, 0);
			if(ctlr->ntb == 0)
				ctlr->ntb = Ntb;
			ctlr->tb = ialloc(sizeof(RingBuf)*ctlr->ntb, 0);

			ctlr->rh = 0;
			ctlr->ri = 0;
			for(i = 0; i < ctlr->nrb; i++)
				ctlr->rb[i].owner = Interface;
		
			ctlr->th = 0;
			ctlr->ti = 0;
			for(i = 0; i < ctlr->ntb; i++)
				ctlr->tb[i].owner = Host;
		
			break;
		}
	}
	splx(x);

	return mask;
}

static Ether*
attach(int ctlrno)
{
	Ether *ctlr;

	if(ctlrno >= MaxEther || ether[ctlrno].state == 0)
		return 0;

	ctlr = &ether[ctlrno];
	if(ctlr->state == 1){
		ctlr->state = 2;
		(*ctlr->attach)(ctlr);
	}

	return ctlr;
}

uchar*
etheraddr(int ctlrno)
{
	Ether *ctlr;

	if((ctlr = attach(ctlrno)) == 0)
		return 0;

	return ctlr->ea;
}

static int
wait(RingBuf* ring, uchar owner, int timo)
{
	ulong start;

	start = m->ticks;
	while(TK2MS(m->ticks - start) < timo){
		if(ring->owner != owner)
			return 1;
	}

	return 0;
}

int
etherrxpkt(int ctlrno, Etherpkt* pkt, int timo)
{
	int n;
	Ether *ctlr;
	RingBuf *ring;

	if((ctlr = attach(ctlrno)) == 0)
		return 0;

	ring = &ctlr->rb[ctlr->rh];
	if(wait(ring, Interface, timo) == 0){
		/*
		print("ether%d: rx timeout\n", ctlrno);
		 */
		return 0;
	}

	n = ring->len;
	memmove(pkt, ring->pkt, n);
	ring->owner = Interface;
	ctlr->rh = NEXT(ctlr->rh, ctlr->nrb);

	return n;
}

int
ethertxpkt(int ctlrno, Etherpkt* pkt, int len, int)
{
	Ether *ctlr;
	RingBuf *ring;
	int s;

	if((ctlr = attach(ctlrno)) == 0)
		return 0;

	ring = &ctlr->tb[ctlr->th];
	if(wait(ring, Interface, 1000) == 0){
		print("ether%d: tx buffer timeout\n", ctlrno);
		return 0;
	}

	memmove(ring->pkt, pkt, len);
	if(len < ETHERMINTU){
		memset(ring->pkt+len, 0, ETHERMINTU-len);
		len = ETHERMINTU;
	}
	ring->len = len;
	ring->owner = Interface;
	ctlr->th = NEXT(ctlr->th, ctlr->ntb);
	s = splhi();
	(*ctlr->transmit)(ctlr);
	splx(s);

	return 1;
}

Type Ethertype = 	
	{	Tether,
		Fbootp,
		etherinit, 0, 0, bootp, 0,
		{ 0, "e", },
	};

void
devetherlink(void)
{
	addboottype(&Ethertype);
}
