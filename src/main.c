#include "../includes/ft_ping.h"

t_stats g_stats = {0};

void sigint_handler(int sig)
{
	(void)sig;
	g_stats.stop = 1;
}

int parse_args(int ac, char **av, char **dest_str, int *verbose)
{
	*dest_str = NULL;
	*verbose = 0;

	for (int i = 1; i < ac; i++) {
		if (strcmp(av[i], "-?") == 0 || strcmp(av[i], "--help") == 0) {
			print_usage();
			return 1;
		} else if (strcmp(av[i], "-v") == 0) {
			*verbose = 1;
		} else if (av[i][0] == '-') {
			printf("ft_ping: invalid option -- '%s'\n", av[i]);
			print_usage();
			return 1;
		} else if (!*dest_str) {
			*dest_str = av[i];
		} else {
			printf("ft_ping: too many destinations\n");
			print_usage();
			return 1;
		}
	}
	
	if (!*dest_str) {
		printf("ft_ping: destination required\n");
		print_usage();
		return 1;
	}
	return 0;
}

int get_dest(char *av, struct sockaddr_in *dest)
{
	// hint = les indices que je donne
	// res = la resolution renvoyée à partir des indices
	struct addrinfo hints;
	struct addrinfo *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_RAW; // socket
	hints.ai_protocol = IPPROTO_ICMP; // protocole

	if (getaddrinfo(av, NULL, &hints, &res) != 0) {
		return 1;
	}
	memcpy(dest, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	return 0;
}



int create_socket()
{
	int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd < 0) {
		return -1;
	}

	struct timeval timeout;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
		close(sockfd);
		return -1;
	}

	return sockfd;
}

void save_stats_info(double time_ms)
{
	g_stats.packets_received++;
	g_stats.sum_time += time_ms;
	g_stats.sum_sq_rtt += (time_ms * time_ms);
	if (g_stats.packets_received == 1 || time_ms < g_stats.rtt_min)
		g_stats.rtt_min = time_ms;
	if (time_ms > g_stats.rtt_max)
		g_stats.rtt_max = time_ms;
}

int ping_loop(struct icmp_packet *packet, int sockfd, struct sockaddr_in *dest, int verbose)
{
	struct sockaddr_in sender;
	socklen_t sender_len;
	char buffer[1024];
	struct timeval start, end;
	uint16_t seq = 0; // On va gérer la séquence directement ici

	while (!g_stats.stop) {
		uint16_t current_seq = seq++;

		// 1. On inscrit le temps actuel dans la payload (data)
		gettimeofday(&start, NULL);
		memcpy(packet->data, &start, sizeof(struct timeval));

		// 2. On finalise l'en-tête (séquence et checksum) avant d'envoyer
		packet->seq = htons(current_seq);
		packet->checksum = 0;
		packet->checksum = icmp_checksum(packet, sizeof(*packet));

		ssize_t bytes_sent = sendto(sockfd, packet, sizeof(*packet), 0, (struct sockaddr *)dest, sizeof(*dest));
		if (bytes_sent < 0) {
			return error_msg("Error during data sent\n");
		}
		g_stats.packets_transmitted++;
		
		// BOUCLE INTERNE : On lit jusqu'à avoir notre ECHOREPLY ou un timeout
		while (!g_stats.stop) {
			sender_len = sizeof(sender);
			ssize_t bytes_recv = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender, &sender_len);
			if (bytes_recv < 0) {
				if (errno == EINTR)
					break;
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					if (verbose) {
						printf("Request timeout for icmp_seq=%u\n", current_seq);
					}
					break;
				} else {
					return error_msg("Error for data recieved\n");
				}
			}

			gettimeofday(&end, NULL);
			if (bytes_recv < (ssize_t)sizeof(struct ip))
				continue;
			struct ip *ip_hdr_recv = (struct ip *)buffer;
			int ip_hdr_len = ip_hdr_recv->ip_hl << 2;

			if (ip_hdr_len < (int)sizeof(struct ip)
				|| bytes_recv < ip_hdr_len + (ssize_t)ICMP_MINLEN)
				continue;
			ssize_t icmp_len = bytes_recv - ip_hdr_len;

			// On "calque" notre structure icmp_packet sur la réception
			struct icmp_packet *recv_packet = (struct icmp_packet *)(buffer + ip_hdr_len);

			// Filtre : est-ce que c'est bien NOTRE réponse ?
			if (recv_packet->type == ICMP_ECHOREPLY && recv_packet->id == htons(getpid() & 0xFFFF)) {
				struct timeval sent_time;
				double time_ms;

				if (bytes_recv < ip_hdr_len + (ssize_t)(ICMP_MINLEN + sizeof(sent_time)))
					continue;
				memcpy(&sent_time, recv_packet->data, sizeof(sent_time));
				time_ms = (end.tv_sec - sent_time.tv_sec) * 1000.0
					+ (end.tv_usec - sent_time.tv_usec) / 1000.0;

				save_stats_info(time_ms);

				printf("%zd bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n",
					icmp_len,
					inet_ntoa(sender.sin_addr),
					ntohs(recv_packet->seq),
					ip_hdr_recv->ip_ttl,
					time_ms);
				
				// On sort de la boucle de lecture puisqu'on a notre réponse
				break;
			} else if (verbose && is_icmp_error(recv_packet->type)) {
				uint16_t error_seq;
				double time_ms_verb;

				if (!icmp_error_matches((char *)recv_packet, icmp_len, &error_seq))
					continue;
				time_ms_verb = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
				printf("%zd bytes from %s: icmp_seq=%u icmp_type=%d icmp_code=%d ttl=%d time=%.3f ms\n",
					icmp_len,
					inet_ntoa(sender.sin_addr),
					error_seq,
					recv_packet->type,
					recv_packet->code,
					ip_hdr_recv->ip_ttl,
					time_ms_verb);
				if (error_seq == current_seq)
					break;
			}
		}

		if (!g_stats.stop)
			sleep(1);
	}
	return 0;
}

int main(int ac, char **av)
{
	int sockfd;
	struct sockaddr_in dest;
	struct icmp_packet packet;
	char *dest_str;
	int verbose = 0;
	struct sigaction sa;

	if (parse_args(ac, av, &dest_str, &verbose))
		return 0;

	// Calcul de dest
	if (get_dest(dest_str, &dest)) {
		 printf("ping: %s: Name or service not known\n", dest_str);
		 return 1;
	}
	printf("PING %s (%s): %d data bytes\n", dest_str, inet_ntoa(dest.sin_addr), PAYLOAD_SIZE);

	// Calcul du sockfd
	sockfd = create_socket();
	if (sockfd < 0) { return error_msg("problem while creating the socket\n");}

	// On initialise les infos globales juste avant de commencer la boucle
	g_stats.dest_name = dest_str;
	gettimeofday(&g_stats.start_time, NULL);
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) < 0) {
		close(sockfd);
		return error_msg("problem while setting signal handler\n");
	}

	// construction de icmp
	build_icmp_packet(&packet);
	// boucle de ping
	ping_loop(&packet, sockfd, &dest, verbose);

	close(sockfd);
	if (g_stats.stop)
		print_stats();
	return 0;
}