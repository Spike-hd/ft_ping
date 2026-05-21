#include "../includes/ft_ping.h"

void build_icmp_packet(struct icmp_packet *packet)
{
	memset(packet, 0, sizeof(struct icmp_packet));
	
	// On remplit le payload avec des données
	for (int i = 0; i < PAYLOAD_SIZE; i++) {
		packet->data[i] = i; 
	}
	
	packet->type = ICMP_ECHO;                   // Type: Echo Request (8)
	packet->code = 0;                           // Code: 0
	packet->id = htons(getpid() & 0xFFFF);      // ID: utilise le PID du processus
	packet->seq = htons(0);                     // Numéro de séquence
	packet->checksum = icmp_checksum(packet, sizeof(struct icmp_packet)); // Calcul du checksum
}

int icmp_error_matches(char *icmp_msg, ssize_t icmp_len, uint16_t *seq)
{
	struct ip *inner_ip;
	struct icmp_packet *inner_packet;
	int inner_ip_len;

	if (icmp_len < ICMP_MINLEN + (ssize_t)sizeof(struct ip))
		return 0;
	inner_ip = (struct ip *)(icmp_msg + ICMP_MINLEN);
	inner_ip_len = inner_ip->ip_hl << 2;
	if (inner_ip_len < (int)sizeof(struct ip))
		return 0;
	if (icmp_len < ICMP_MINLEN + inner_ip_len + (ssize_t)ICMP_MINLEN)
		return 0;
	if (inner_ip->ip_p != IPPROTO_ICMP)
		return 0;
	inner_packet = (struct icmp_packet *)(icmp_msg + ICMP_MINLEN + inner_ip_len);
	if (inner_packet->type != ICMP_ECHO || inner_packet->id != htons(getpid() & 0xFFFF))
		return 0;
	*seq = ntohs(inner_packet->seq);
	return 1;
}

uint16_t icmp_checksum(void *data, int len)
{
	uint32_t checksum32 = 0;
	uint16_t *ptr = (uint16_t *)data;

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
