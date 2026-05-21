
#include "../includes/ft_ping.h"

int	error_msg(char *str) {
	fprintf(stderr, "%s", str);
	return 1;
}

void print_usage(void)
{
	printf("Usage: ./ft_ping [options] <destination>\n");
	printf("Options:\n");
	printf("  -v\t\tVerbose output\n");
	printf("  -?\t\tShow this help message\n");
}

int is_icmp_error(uint8_t type)
{
	return (type == 3 || type == 4 || type == 5 || type == 11 || type == 12);
}

void print_stats(void)
{
	struct timeval end_time;

	gettimeofday(&end_time, NULL);
	
	// Temps total d'exécution du programme
	double total_time = (end_time.tv_sec - g_stats.start_time.tv_sec) * 1000.0 + (end_time.tv_usec - g_stats.start_time.tv_usec) / 1000.0;
	
	// Calcul du pourcentage de perte					
	long loss = 0;
	if (g_stats.packets_transmitted > 0)
		loss = ((g_stats.packets_transmitted - g_stats.packets_received) * 100) / g_stats.packets_transmitted;

	printf("\n--- %s ping statistics ---\n", g_stats.dest_name);
	printf("%ld packets transmitted, %ld received, %ld%% packet loss, time %.0fms\n", g_stats.packets_transmitted, g_stats.packets_received, loss, total_time);
		   
	if (g_stats.packets_received > 0) {
		double avg = g_stats.sum_time / g_stats.packets_received;
		double variance = (g_stats.sum_sq_rtt / g_stats.packets_received) - (avg * avg);
		double mdev = sqrt(variance > 0 ? variance : 0);
		printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n", g_stats.rtt_min, avg, g_stats.rtt_max, mdev);
	}
}
