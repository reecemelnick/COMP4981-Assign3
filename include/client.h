static void parse_command_line(int argc, char *argv[], char *server_ip, long *server_port);

void usage(void);

int setup_client_socket(char *server_ip, long server_port);

void start_shell(int client_fd);
