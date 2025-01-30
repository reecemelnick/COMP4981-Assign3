#include "client.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_IP_LENGTH 16
#define MAX_INPUT 1024
#define BS 10
#define UNRECONIZED_STATUS 127

int main(int argc, char *argv[])
{
    char server_ip[MAX_IP_LENGTH];
    long server_port = 0;
    int  client_fd;

    parse_command_line(argc, argv, server_ip, &server_port);

    client_fd = setup_client_socket(server_ip, server_port);

    start_shell(client_fd);

    return EXIT_SUCCESS;
}

void start_shell(int client_fd)
{
    char input[MAX_INPUT];    // Buffer to store user input
    int  status = 0;

    while(1)
    {
        size_t  len;
        ssize_t bytes_read;

        printf("myshell$ ");
        fflush(stdout);

        if(fgets(input, MAX_INPUT, stdin) == NULL)
        {
            perror("fgets");
            break;
        }

        len = strlen(input);
        if(len > 0 && input[len - 1] == '\n')
        {
            input[len - 1] = '\0';
        }

        if(strcmp(input, "exit") == 0)
        {
            printf("Exiting shell...\n");
            break;
        }

        for(char *p = input; (p = strstr(p, "$?"));)
        {
            char   temp[MAX_INPUT];
            size_t prefix_len;
            snprintf(temp, sizeof(temp), "%d", status);
            prefix_len = (size_t)(p - input);    // Text before "$?"
            snprintf(temp, sizeof(temp), "%.*s%d%s", (int)prefix_len, input, status, p + 2);
            strcpy(input, temp);
        }

        // Simulate an exit status for demonstration
        if(strcmp(input, "simulate_success") == 0)
        {
            status = 0;    // Success
            printf("Simulating success: $? = %d\n", status);
        }
        else if(strcmp(input, "simulate_failure") == 0)
        {
            status = 1;    // Failure
            printf("Simulating failure: $? = %d\n", status);
        }
        else
        {
            // Default behaviour for unrecognized input
            printf("Unrecognized input: \"%s\"\n", input);
            status = UNRECONIZED_STATUS;    // Indicate command not found
        }

        bytes_read = write(client_fd, input, sizeof(input));
        if(bytes_read == -1)
        {
            perror("write");
            return;
        }
    }
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
        // close(sockfd);
    }

    // close(sockfd);

    return sockfd;
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
