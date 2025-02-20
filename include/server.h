#include <arpa/inet.h>

int initialize_socket(void);

int accept_clients(int server_sock, struct sockaddr_in host_addr, socklen_t host_addrlen);

void handle_sigint(int signum);

char **tokenize_commands(char *buffer);

int handle_client(int clientfd);

int read_and_tokenize(int clientfd);

int launch_processs(int client_fd, char **tokens);

int excecute_shell(int fd, char **args);

char *get_full_path(char **args);
