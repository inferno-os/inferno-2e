enum
{
	SINADDRSZ	= 12,	/* Good until IP V6 */
	S_TCP		= 0,
	S_UDP
};

int		so_socket(int type);
void		so_connect(int, unsigned long, unsigned short);
void		so_getsockname(int, unsigned long*, unsigned short*);
void		so_bind(int, int, unsigned short);
void		so_listen(int);
int		so_accept(int, unsigned long*, unsigned short*);
int		so_getservbyname(char*, char*, char*);
int		so_gethostbyname(char*, char**, int);
int		so_gethostbyaddr(char*, char**, int);
int		so_recv(int, void*, int, void*, int);
int		so_send(int, void*, int, void*, int);
void		so_close(int);
int		so_hangup(int, int);
void		so_setsockopt(int, int, int);
void		so_poll(int, long);
void		so_noblock(int, int);


void		hnputl(void *p, unsigned long v);
void		hnputs(void *p, unsigned short v);
unsigned long	nhgetl(void *p);
unsigned short	nhgets(void *p);
unsigned long	parseip(char *to, char *from);
int		bipipe(int[]);
