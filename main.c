#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>

u_short	icmp_checksum(struct icmp *icmp_pkt);
struct	sockaddr_in	*get_dest(char *av);
int		error_msg(char *str);

int main(int ac, char **av)
{
	int sockfd;
	struct sockaddr_in *dest, sender;
	socklen_t sender_len = sizeof(sender);
	char buffer[1024];

	if (ac != 2) { return error_msg("Error with the number of argument");}
	if (strcmp(av[1], "help") == 0 || strcmp(av[1], "-h") == 0)  { return error_msg("Ici mettre les commandes"); }

	// Calcul de dest
	dest = get_dest(av[1]);
	if (dest == NULL) {error_msg("getaddrinfo error\n");}

	// Calcul du sockfd
	sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sockfd < 0) { return error_msg("problem while creating the socket\n");}

	// construction de icmp
	struct icmp icmp_pkt;
	memset(&icmp_pkt, 0, sizeof(icmp_pkt));
	icmp_pkt.icmp_type = ICMP_ECHO;           // Type: Echo Request (8)
	icmp_pkt.icmp_code = 0;                   // Code: 0
	icmp_pkt.icmp_id = getpid();              // ID: utilise le PID du processus
	icmp_pkt.icmp_seq = 0;                    // Numéro de séquence
	icmp_pkt.icmp_cksum = 0;                  // Sera calculé après

	// Calcul du checksum
	icmp_pkt.icmp_cksum = icmp_checksum(&icmp_pkt);

	ssize_t bytes_sent = sendto(sockfd, &icmp_pkt, sizeof(icmp_pkt), 0, (struct sockaddr *)dest, sizeof(*dest));
	if (bytes_sent < 0) { return error_msg("Error during data sent\n");}

	ssize_t bytes_recv = recvfrom(sockfd, &buffer, sizeof(buffer),  0, (struct sockaddr *)&sender, &sender_len);
	if (bytes_sent < 0) { return error_msg("Error for data recieved\n");}



	close(sockfd);
	return 0;
}


struct sockaddr_in	*get_dest(char *av)
{
	// hint = les indices que je donne
	// res = la resolution renvoyée à partir des indices
	struct addrinfo hints;
	struct addrinfo *res;

	memset(&hints, 0, sizeof(hints)); // pour éviter des problemes on le met à zéro avant de l'utiliser
	hints.ai_family = AF_INET; // IPv4
	hints.ai_socktype = SOCK_RAW; // socket
	hints.ai_protocol = IPPROTO_ICMP; // protocole

	// getaddrinfo va recuperer les elements de hints pour les mettre dans res
	int status = getaddrinfo(av, NULL, &hints, &res);
	if (status != 0) {
		return NULL;
	}

	struct sockaddr_in *dest = (struct sockaddr_in *)res->ai_addr;
	char ip_str[INET_ADDRSTRLEN]; // ne sert a rien
	inet_ntop(AF_INET, &(dest->sin_addr), ip_str, sizeof(ip_str));
	printf("address = %s\n", ip_str);

	return dest;

}



u_short icmp_checksum(struct icmp *icmp_pkt)
{
	// normalement devrait renvoyer 8 mais mieux de ne pas hardcode i guess
	unsigned long len = sizeof(icmp_pkt->icmp_type)
						+ sizeof(icmp_pkt->icmp_code)
						+ sizeof(icmp_pkt->icmp_id)
						+ sizeof(icmp_pkt->icmp_seq)
						+ sizeof(icmp_pkt->icmp_cksum);
	unsigned long test = sizeof(icmp_pkt);
	printf("La taille totale calculée est de %lu\n", len);
	printf("Test pour vois si il y a eu du padding : %lu\n", test);

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


int	error_msg(char *str) {
	printf(str);
	return 1;
}
