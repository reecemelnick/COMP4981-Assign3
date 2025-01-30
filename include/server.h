#include <arpa/inet.h>

int initialize_socket(void);

int accept_clients(int server_sock, struct sockaddr_in host_addr, socklen_t host_addrlen);

void handle_sigint(int signum);
