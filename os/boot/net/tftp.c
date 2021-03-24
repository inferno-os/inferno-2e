#include <lib9.h>
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "etherif.h"
#include "netboot.h"
#include "udp.h"



extern int tftptimeout;


struct tftpstate {
	uchar	*addr;
	int	len;
	int	maxlen;
	ushort	block;
};


static int
tftp_input(void *vt, struct mbuf *m)
{
	struct udphdr  *udp;
	struct tftp_t  *tr;
	ushort         len;
	struct tftp_t  *tp;
	struct tftpstate *t = (struct tftpstate*)vt;
	uchar pkt[UDPHDRSIZE+TFTP_ACK_MSG_SIZE];
	int opcode;

	memset(pkt, 0, sizeof pkt);
	tr = mtod(m, struct tftp_t*);
	udp = (struct udphdr *)((uchar*)tr - sizeof(struct udphdr));
	opcode = nhgets(tr->opcode);
	if (opcode == TFTP_ERROR) {
		char estr[ERRLEN];
		sprint(estr, "TFTP error %d (%s)\n", nhgets(tr->err.errcode), tr->err.errmsg);
		error(estr);
		t->maxlen = -1;
		return -1;
	}			/* ACK PACKET */
	if (opcode != TFTP_DATA) {
		status("?", t->len, t->maxlen);
		return 0;
	}
	/* Check block number */
	if (nhgets(tr->data.block) != t->block) {
		status("!", t->len, t->maxlen);
		return 0;
	}
	t->block++;

	tp = (struct tftp_t*)&pkt[UDPHDRSIZE];
	hnputs(tp->opcode, TFTP_ACK);
	memcpy2(tp->ack.block, tr->data.block);
	if(netdebug & NETDBG_TFTP_STATUS)
		print("<tftp:xmit:%d>", TFTP_ACK_MSG_SIZE);
	udp_transmit(arptable[ARP_SERVER].ipaddr, nhgets(udp->uh_dport),
			nhgets(udp->uh_sport), tp, TFTP_ACK_MSG_SIZE);
	len = nhgets(udp->uh_ulen) - sizeof(struct udphdr) - 4;
	if(len > 512) {
		status(">", t->len, t->maxlen);
		return 0;
	}
	if(t->len + len > t->maxlen) {
		status(">>", t->len, t->maxlen);
		return 0;
	}
	memcpy(t->addr, (uchar*)tr->data.download, len);
	t->addr += len;
	t->len += len;
	if(len < 512)
		t->maxlen = t->len;
	if((t->block&0x1f) == 2 || len < 512)
		status("tftp", t->len, t->maxlen);
	return 1;
}

static void
tftp_nak( in_addr destip, ushort srcport, ushort destport, int /*blocknum*/)
{
	struct tftp_t  *tp;
	uchar pkt[UDPHDRSIZE+15];

	memset(pkt, 0, sizeof pkt);
	tp = (struct tftp_t*)&pkt[UDPHDRSIZE];
	hnputs(tp->opcode, TFTP_ERROR);
	hnputs(tp->err.errcode, 1);
	sprint(tp->err.errmsg,"block error");
	if(netdebug & NETDBG_TFTP_STATUS)
		print("<tftp:xmit:15>");
	udp_transmit(destip,srcport,destport, tp, 15);
}


int
tftp(const char *name, uchar *addr, int maxlen)
{
	int retry = MAX_TFTP_RETRIES;
	static ushort iport = 2000;
	ushort len;
	struct tftp_t *tp;
	struct tftpstate t;
	int i;
	uchar pkt[UDPHDRSIZE+sizeof(struct tftp_t)];

	memset(pkt, 0, sizeof pkt);
	iport++;
	tp = (struct tftp_t*)&pkt[UDPHDRSIZE];
	hnputs(tp->opcode, TFTP_RRQ);
	len = sprint(tp->rrq, "%s", name);
	tp->rrq[len++] = '\0';
	len += sprint(&tp->rrq[len], "octet") +1;
	len = (int)(&tp->rrq[len]) - (int)tp + 1;
	t.maxlen = maxlen;
	t.len = 0;
	t.addr = addr;
	t.block = 1;

	while (retry--) {
		if(netdebug & NETDBG_TFTP_STATUS)
			print("<tftp:xmit:%d>", len);
		if (!udp_transmit(arptable[ARP_SERVER].ipaddr, iport,
				TFTP_PORT, tp, sizeof(struct tftp_t)))
			return -1;
		for(;;) {
			int nakretry = (t.len == 0) ? 3 : 30;
			if(t.len == t.maxlen)
				return t.len;
			microdelay(500);
			while(!udp_receive(iport, tftp_input, &t, tftptimeout)
					&& --nakretry) {
				if(t.maxlen < 0)
					return -1;
				tftp_nak(arptable[ARP_SERVER].ipaddr, iport, TFTP_PORT, t.block);
				if(interrupt()) {
					error("tftp aborted");
					return -1;
				}
				status("tftp: NAK", t.len, t.maxlen);
			}
			if(t.maxlen < 0)
				return -1;
			if(netdebug & NETDBG_TFTP_STATUS)
				print("<tftp:recv:%d>", t.len);
			if(nakretry == 0)
				break;
		}
		delay(200);
		for(i=0; i<80; i++)
			print("\b \b");
	}
	error("tftp: retry limit reached");
	return -1;
}

