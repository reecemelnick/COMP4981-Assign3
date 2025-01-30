#include "client.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_IP_LENGTH 16
#define BS 10

int main(int argc, char *argv[])
{
    char server_ip[MAX_IP_LENGTH];
    long server_port = 0;

    parse_command_line(argc, argv, server_ip, &server_port);

    setup_client_socket(server_ip, server_port);

    return EXIT_SUCCESS;
}

int setup_client_socket(char *server_ip, long server_port)
{
    struct sockaddr_in host_addr;
    socklen_t          host_addrlen;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);    // NOLINT
    if(sockfd == -1)
    {
        perror("socket");
        return -1;
    }

    host_addrlen = sizeof(host_addr);

    host_addr.sin_family = AF_INET;
    host_addr.sin_port   = htons(server_port);

    if(inet_pton(AF_INET, server_ip, &host_addr.sin_addr) <= 0)
    {
        perror("inet_pton failed");
        exit(EXIT_FAILURE);
    }

    if(connect(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0)
    {
        perror("connecting\n");
        close(sockfd);
    }

    // close(sockfd);

    return 0;
}

static void parse_command_line(int argc, char *argv[], char *server_ip, long *server_port)
{
    int   arg;
    int   argcount = 0;
    char *endptr;

    while((arg = getopt(argc, argv, "a:p:")) != -1)
    {
        switch(arg)
        {
            case 'a':
                snprintf(server_ip, MAX_IP_LENGTH, "%s", optarg);
                argcount++;
                break;
            case 'p':
                *server_port = strtol(optarg, &endptr, BS);
                if(*endptr != '\0')
                {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    exit(EXIT_FAILURE);
                }
                argcount++;
                break;
            case '?':
                usage();
                exit(EXIT_FAILURE);
            default:
                printf("default");
                break;
        }
    }

    if(argcount != 2)
    {
        usage();
        exit(EXIT_FAILURE);
    }
}

void usage(void)
{
    puts("Usage: ./client [-m] [-f]\nOptions:\n\t-m Message\n\t-f Transform function to be called (upper, lower, null)\n");
}
