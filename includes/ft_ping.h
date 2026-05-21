#ifndef FT_PING_H
# define FT_PING_H

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <netdb.h>
# include <netinet/ip.h>
# include <netinet/ip_icmp.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <sys/time.h>
# include <errno.h>
# include <signal.h>
# include <math.h>
# include <stdint.h>

#define PAYLOAD_SIZE 56

typedef struct s_stats {
	long packets_transmitted;
	long packets_received;
	double rtt_min;
	double rtt_max;
	double sum_time;
	double sum_sq_rtt;
	struct timeval start_time;
	char *dest_name;
	volatile sig_atomic_t stop;
} t_stats;

extern t_stats g_stats;

struct icmp_packet {
	uint8_t		type;
	uint8_t		code;
	uint16_t	checksum;
	uint16_t	id;
	uint16_t	seq;
	char		data[PAYLOAD_SIZE];
};

uint16_t	icmp_checksum(void *data, int len);
int		get_dest(char *av, struct sockaddr_in *dest);
int		error_msg(char *str);
int     ping_loop(struct icmp_packet *packet, int sockfd, struct sockaddr_in *dest, int verbose);
int     is_icmp_error(uint8_t type);
void    print_stats(void);
void    build_icmp_packet(struct icmp_packet *packet);
void    save_stats_info(double time_ms);
void    sigint_handler(int signum);
int     create_socket(void);
int     parse_args(int ac, char **av, char **dest_str, int *verbose);
int     icmp_error_matches(char *icmp_msg, ssize_t icmp_len, uint16_t *seq);
void    print_usage(void);

#endif
