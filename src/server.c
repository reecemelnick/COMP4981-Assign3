#include "server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TIMEOUT 1000
#define BUFFER_SIZE 1024
#define MAX_COMMANDS 3

sig_atomic_t static volatile g_running = 1;    // NOLINT

int main(void)
{
    int res;

    res = initialize_socket();

    return res;
}

void handle_sigint(int signum)
{
    if(signum == SIGINT)
    {
        g_running = 0;
    }
}

int initialize_socket(void)
{
    const int          PORT = 8080;
    struct sockaddr_in host_addr;
    socklen_t          host_addrlen;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);    // NOLINT
    if(sockfd == -1)
    {
        perror("socket");
        return -1;
    }

    printf("Socket created successfully.\n");

    host_addrlen = sizeof(host_addr);

    host_addr.sin_family      = AF_INET;
    host_addr.sin_port        = htons(PORT);
    host_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(sockfd, (struct sockaddr *)&host_addr, host_addrlen) != 0)
    {
        perror("bind");
        close(sockfd);
        return -1;
    }

    printf("socket was bound successfully\n");

    if(listen(sockfd, SOMAXCONN) != 0)
    {
        perror("listen");
        close(sockfd);
        return -1;
    }

    printf("server listening for connections\n");

    if(accept_clients(sockfd, host_addr, host_addrlen) < 0)
    {
        perror("Accept clients");
        return -1;
    }

    return sockfd;
}

int accept_clients(int server_sock, struct sockaddr_in host_addr, socklen_t host_addrlen)
{
    struct sockaddr_in client_addr;
    socklen_t          client_addrlen;
    int                sockn;
    struct pollfd      pfd = {server_sock, POLLIN, 0};
    char               buffer[BUFFER_SIZE + 1];
    ssize_t            bytes_read;
    char             **tokens;
    int                itr;

    client_addrlen = sizeof(client_addr);

    signal(SIGINT, &handle_sigint);

    while(g_running)
    {
        int ret = poll(&pfd, 1, TIMEOUT);

        if(ret == 0)
        {
            continue;
        }

        if(ret < 0)
        {
            break;
        }

        if(pfd.revents & POLLIN)
        {
            int newsockfd = accept(server_sock, (struct sockaddr *)&host_addr, &host_addrlen);

            if(newsockfd < 0)
            {
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    if(!g_running)
                    {    // Check g_running again
                        break;
                    }
                    continue;
                }
                perror("accept");
                break;
            }

            sockn = getsockname(newsockfd, (struct sockaddr *)&client_addr, &client_addrlen);
            if(sockn < 0)
            {
                perror("sockname");
                continue;
            }

            printf("Connection made\n");

            bytes_read = read(newsockfd, buffer, (size_t)BUFFER_SIZE);
            if(bytes_read < 0)
            {
                if(errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    continue;
                }
                perror("read");
                continue;
            }

            buffer[bytes_read] = '\0';

            printf("%s\n", buffer);

            tokens = tokenize_commands(buffer);

            itr = 0;
            if(tokens != NULL)
            {
                while(tokens[itr] != NULL)
                {
                    printf("token: %s\n", tokens[itr]);
                    printf("length: %zu\n", strlen(tokens[itr]));
                    itr++;
                }

                for(int i = 0; tokens[i] != NULL; i++)
                {
                    free(tokens[i]);
                }
                free((void *)tokens);
            }

            close(newsockfd);
            printf("closing connection\n");
        }
    }

    printf("done in loop\n");

    return server_sock;
}

char **tokenize_commands(char *buffer)
{
    char      **tokens;
    const char *token;
    char       *rest = buffer;
    int         index;

    tokens = (char **)malloc(MAX_COMMANDS * sizeof(char *));

    index = 0;
    while((token = strtok_r(rest, " ", &rest)))
    {
        tokens[index] = (char *)malloc((strlen(token) + 1) * sizeof(char));
        if(tokens[index] == NULL)
        {
            perror("malloc");
            free((void *)tokens);
            return NULL;
        }
        strcpy(tokens[index], token);
        index++;
    }

    tokens[index] = NULL;

    return tokens;
}

// reece huy
// 0123456789
