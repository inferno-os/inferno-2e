/* adapter operations */

int  pcmprobe(void);
int  pcmattach(void);

/* socket operations */

void pcm_dump_regs(int socket);
void pcm_ready(int socket);

int  pcm_check_vers1_2(int socket, char *candidate);
int  pcm_set_iomode(int socket);
void pcm_set_conf_regs(int socket, uchar cor, uchar csr);

/* window operations */

struct pcm_win *pcm_attr_map(int socket, int where, int size);
struct pcm_win *pcm_mem_map(int socket, int where, int size, int readonly);
struct pcm_win *pcm_port_map(int socket, int start, int end, int offset,
			     int flags);

void pcm_attr_unmap(struct pcm_win *wp);
void pcm_mem_unmap(struct pcm_win *wp);
void pcm_port_unmap(struct pcm_win *wp);

/* valid irq number are 3-5 ISA, mapping 1-3 (IT schematics) or 0-2 (IT doc) */
int  pcm_irq_map(int socket, int irq);
void pcm_irq_unmap(int socket);

