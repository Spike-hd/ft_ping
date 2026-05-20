#ifndef FT_PING_H
# define FT_PING_H

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <netdb.h>
# include <netinet/ip_icmp.h>
# include <arpa/inet.h>
# include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <math.h>

typedef struct s_stats {
	long packets_transmitted;
	long packets_received;
	double min_rtt;
	double max_rtt;
	double sum_rtt;
	double sum_sq_rtt;
	struct timeval start_time;
	char *dest_name;
} t_stats;

extern t_stats g_stats;

#define PAYLOAD_SIZE 56

struct icmp_packet {
    struct icmp header;
    char data[PAYLOAD_SIZE]; 
};

u_short	icmp_checksum(struct icmp *icmp_pkt, int len);
int		get_dest(char *av, struct sockaddr_in *dest);
int		error_msg(char *str);
int     ping_loop(struct icmp_packet *packet, int sockfd, struct sockaddr_in *dest, int verbose);

#endif
