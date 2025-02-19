#include "client.h"
#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_INPUT 1024
#define RESPONSE_SIZE 4096
#define BS 10
#define TIMEOUT 1000

int main(int argc, char *argv[])
{
    char *server_ip   = NULL;
    long  server_port = 0;
    int   client_fd;

    parse_command_line(argc, argv, &server_ip, &server_port);

    client_fd = setup_client_socket(server_ip, server_port);

    start_shell(client_fd);

    free(server_ip);

    return EXIT_SUCCESS;
}

void start_shell(int client_fd)
{
    char          input[MAX_INPUT];    // Buffer to store user input
    char          output[RESPONSE_SIZE];
    struct pollfd pfd    = {client_fd, POLLIN, 0};
    int           status = 0;

    while(1)
    {
        size_t  len;
        ssize_t bytes_read;
        int     ret;

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

        bytes_read = write(client_fd, input, sizeof(input));
        if(bytes_read == -1)
        {
            perror("write");
            continue;
        }

        memset(output, 0, RESPONSE_SIZE);

        ret = poll(&pfd, 1, TIMEOUT);
        // if timeout expired, continue polling fd
        if(ret == 0)
        {
            continue;
        }

        // if error occured break loop
        if(ret < 0)
        {
            break;
        }
        if(pfd.revents & POLLIN)
        {
            bytes_read = read(client_fd, output, RESPONSE_SIZE);
            if(bytes_read == -1)
            {
                perror("read");
                continue;
            }
        }

        output[bytes_read] = '\0';
        printf("%s\n", output);
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
    host_addr.sin_port   = htons((uint16_t)server_port);

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

static void parse_command_line(int argc, char *argv[], char **server_ip, long *server_port)
{
    int   arg;
    int   argcount = 0;
    char *endptr;

    while((arg = getopt(argc, argv, "a:p:")) != -1)
    {
        switch(arg)
        {
            case 'a':
                free(*server_ip);
                *server_ip = (char *)malloc((strlen(optarg) * sizeof(char)) + 1);
                if(!*server_ip)
                {
                    perror("malloc");
                    exit(EXIT_FAILURE);
                }
                memcpy(*server_ip, optarg, strlen(optarg) + 1);
                argcount++;
                break;
            case 'p':
                *server_port = strtol(optarg, &endptr, BS);
                if(*endptr != '\0')
                {
                    fprintf(stderr, "Invalid port number: %s\n", optarg);
                    free(*server_ip);
                    exit(EXIT_FAILURE);
                }
                argcount++;
                break;
            case '?':
                usage();
                free(*server_ip);
                exit(EXIT_FAILURE);
            default:
                printf("default");
                break;
        }
    }

    if(argcount != 2)
    {
        usage();
        free(*server_ip);
        exit(EXIT_FAILURE);
    }
}

void usage(void)
{
    puts("Usage: ./client [-m] [-f]\nOptions:\n\t-m Message\n\t-f Transform function to be called (upper, lower, null)\n");
}
