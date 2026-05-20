#include "../includes/ft_ping.h"

t_stats g_stats = {0};

void sigint_handler(int sig)
{
	(void)sig;
	struct timeval end_time;
	gettimeofday(&end_time, NULL);
	
	// Temps total d'exécution du programme
	double total_time = (end_time.tv_sec - g_stats.start_time.tv_sec) * 1000.0 +
						(end_time.tv_usec - g_stats.start_time.tv_usec) / 1000.0;
	
	long loss = 0;
	if (g_stats.packets_transmitted > 0) {
		loss = ((g_stats.packets_transmitted - g_stats.packets_received) * 100) / g_stats.packets_transmitted;
	}

	printf("\n--- %s ping statistics ---\n", g_stats.dest_name);
	printf("%ld packets transmitted, %ld received, %ld%% packet loss, time %.0fms\n",
		   g_stats.packets_transmitted, g_stats.packets_received, loss, total_time);
		   
	if (g_stats.packets_received > 0) {
		double avg = g_stats.sum_rtt / g_stats.packets_received;
		double variance = (g_stats.sum_sq_rtt / g_stats.packets_received) - (avg * avg);
		double stddev = sqrt(variance > 0 ? variance : 0);
		
		printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
			   g_stats.min_rtt, avg, g_stats.max_rtt, stddev);
	}
	exit(0);
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

	// [DEBUG]
	// Pour voir la conversion de l'adresse en string vers l'adresse IP
	// char ip_str[INET_ADDRSTRLEN];
	// inet_ntop(AF_INET, &(dest->sin_addr), ip_str, sizeof(ip_str));
	// printf("address = %s\n", ip_str);

	freeaddrinfo(res);
	return 0;
}

void build_icmp_packet(struct icmp_packet *packet)
{
	memset(packet, 0, sizeof(struct icmp_packet));
	
	// On remplit le payload avec des données
	for (int i = 0; i < PAYLOAD_SIZE; i++) {
		packet->data[i] = i; 
	}
	
	packet->header.icmp_type = ICMP_ECHO;           // Type: Echo Request (8)
	packet->header.icmp_code = 0;                   // Code: 0
	packet->header.icmp_id = htons(getpid());       // ID: utilise le PID du processus
	packet->header.icmp_seq = htons(0);             // Numéro de séquence
	packet->header.icmp_cksum = icmp_checksum((struct icmp *)packet, sizeof(struct icmp_packet)); // Calcul du checksum
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
int main(int ac, char **av)
{
	int sockfd;
	struct sockaddr_in dest;
	struct icmp_packet packet;
	char *dest_str;
	int verbose = 0;

	// TODO : Modifier le premier if, ac peut etre different de 2 si options sont utilisées, il faudra les gérer
	// TODO : Ajouter la gestion des options (ex: -v pour verbose)
	if (ac != 2 && ac != 3)
	{
		printf("Error, invalid number of arguments\nUsage: ./ft_ping [options] <destination>\n");
		return 1;
	}
	if (ac == 3)
	{
		if (strcmp(av[1], "-?") == 0 || strcmp(av[1], "--help") == 0)
			return error_msg("Usage: ./ft_ping [-v ] <destination>\n");
		else if (strcmp(av[1], "-v") == 0)
		{
			dest_str = av[2];
			verbose = 1;
			// Gestion de l'option -v
		}
		else
		{
			return error_msg("Error, invalid option\nUsage: ./ft_ping [options] <destination>\n");
		}
	} else {
		dest_str = av[1];
	}

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
	signal(SIGINT, sigint_handler);

	// construction de icmp
	build_icmp_packet(&packet);
	ping_loop(&packet, sockfd, &dest, verbose);

	close(sockfd);
	return 0;
}

int ping_loop(struct icmp_packet *packet, int sockfd, struct sockaddr_in *dest, int verbose)
{
	struct sockaddr_in sender;
	socklen_t sender_len = sizeof(sender);
	char buffer[1024];
	struct timeval start, end;
	uint16_t seq = 0; // On va gérer la séquence directement ici

	while (1) {
		// 1. On inscrit le temps actuel dans la payload (data)
		gettimeofday(&start, NULL);
		memcpy(packet->data, &start, sizeof(struct timeval));

		// 2. On finalise l'en-tête (séquence et checksum) avant d'envoyer
		packet->header.icmp_seq = htons(seq++);
		packet->header.icmp_cksum = 0;
		packet->header.icmp_cksum = icmp_checksum((struct icmp *)packet, sizeof(*packet));

		ssize_t bytes_sent = sendto(sockfd, packet, sizeof(*packet), 0, (struct sockaddr *)dest, sizeof(*dest));
		if (bytes_sent < 0) { 
			close(sockfd);
			return error_msg("Error during data sent\n");
		}
		g_stats.packets_transmitted++;
		
		// BOUCLE INTERNE : On lit jusqu'à avoir notre ECHOREPLY ou un timeout
		while (1) {
			ssize_t bytes_recv = recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&sender, &sender_len);
			if (bytes_recv < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					if (verbose) {
						printf("Request timeout for icmp_seq=%u\n", ntohs(packet->header.icmp_seq));
					}
					break; 
				} else {
					close(sockfd);
					return error_msg("Error for data recieved\n");
				}
			}

			gettimeofday(&end, NULL);
			struct ip *ip_hdr_recv = (struct ip *)buffer;
			int ip_hdr_len = ip_hdr_recv->ip_hl << 2;

			if (bytes_recv < ip_hdr_len + (ssize_t)ICMP_MINLEN)
				continue;

			// On "calque" notre structure icmp_packet sur la réception
			struct icmp_packet *recv_packet = (struct icmp_packet *)(buffer + ip_hdr_len);
			struct timeval *sent_time = (struct timeval *)recv_packet->data;

			// Filtre : est-ce que c'est bien NOTRE réponse ?
			if (recv_packet->header.icmp_type == ICMP_ECHOREPLY && recv_packet->header.icmp_id == htons(getpid())) {
				double time_ms = (end.tv_sec - sent_time->tv_sec) * 1000.0
					+ (end.tv_usec - sent_time->tv_usec) / 1000.0;

				g_stats.packets_received++;
				g_stats.sum_rtt += time_ms;
				g_stats.sum_sq_rtt += (time_ms * time_ms);
				if (g_stats.packets_received == 1 || time_ms < g_stats.min_rtt)
					g_stats.min_rtt = time_ms;
				if (time_ms > g_stats.max_rtt)
					g_stats.max_rtt = time_ms;

				printf("%zd bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n",
					bytes_recv - ip_hdr_len,
					inet_ntoa(sender.sin_addr),
					ntohs(recv_packet->header.icmp_seq),
					ip_hdr_recv->ip_ttl,
					time_ms);
				
				// On sort de la boucle de lecture puisqu'on a notre réponse
				break;
			} else if (verbose && recv_packet->header.icmp_type != ICMP_ECHO) {
				// En verbose, on affiche aussi les paquets bizarres que le système aurait pu nous envoyer, mais on ignore nos propres ECHOs
				double time_ms_verb = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
				printf("%zd bytes from %s: icmp_type=%d icmp_code=%d ttl=%d time=%.3f ms\n",
					bytes_recv - ip_hdr_len,
					inet_ntoa(sender.sin_addr),
					recv_packet->header.icmp_type,
					recv_packet->header.icmp_code,
					ip_hdr_recv->ip_ttl,
					time_ms_verb);
			}
		}

		sleep(1);
	}
	return 0;
}


u_short icmp_checksum(struct icmp *icmp_pkt, int len)
{
	uint32_t checksum32 = 0;
	uint16_t *ptr = (uint16_t *)icmp_pkt;

	// 1. Additionner tous les mots de 16 bits
	while (len > 1) {
		checksum32 += *ptr++;
		len -= 2;
	}
	// 2. Si taille impaire, ajouter le dernier octet
	if (len == 1) {
		checksum32 += *(uint8_t *)ptr;
	}
	// 3. Replier les dépassements
	while (checksum32 >> 16) {
		checksum32 = (checksum32 & 0xFFFF) + (checksum32 >> 16);
	}
	// 4. Complément à un
	return (uint16_t)(~checksum32);
}
 
