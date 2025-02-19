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
#define PORT 8000

sig_atomic_t static volatile g_running = 1;    // NOLINT

int launch_processs(char **tokens);

int main(void)
{
    int res;

    res = initialize_socket();

    printf("\nGoodbye...\n");

    return res;
}

// handle CTRL-C
void handle_sigint(int signum)
{
    if(signum == SIGINT)
    {
        g_running = 0;
    }
}

// call basic functions to setup ipv4 socket
int initialize_socket(void)
{
    struct sockaddr_in host_addr;
    socklen_t          host_addrlen;

    // create ipv4 socket
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

// accpet clients
int accept_clients(int server_sock, struct sockaddr_in host_addr, socklen_t host_addrlen)
{
    // poll server_socket for incoming data
    struct pollfd pfd = {server_sock, POLLIN, 0};

    signal(SIGINT, &handle_sigint);

    while(g_running)
    {
        // poll to see if server has data to read
        int ret = poll(&pfd, 1, TIMEOUT);

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

        // if poll output is POLLIN, accpect
        if(pfd.revents & POLLIN)
        {
            int newsockfd = accept(server_sock, (struct sockaddr *)&host_addr, &host_addrlen);

            if(newsockfd < 0)
            {
                perror("accept");
                break;
            }

            printf("Connection made\n");
            // connection made, handle client tasks
            handle_client(newsockfd);
        }
    }

    return server_sock;
}

// wait for stuff to read. Tokenize input.
int handle_client(int clientfd)
{
    int res;

    res = read_and_tokenize(clientfd);

    return res;
}

int launch_processs(char **tokens)
{
    pid_t pid;
    int   status;

    pid = fork();
    if(pid == 0)
    {
        // child
        if(execv("/bin/ls", tokens) == -1)
        {
            perror("exec");
        }
        exit(EXIT_FAILURE);
    }
    else if(pid < 0)
    {
        // fork error
        perror("fork");
    }
    else
    {
        // parent process
        do
        {
            pid_t wpid;
            wpid = waitpid(pid, &status, WUNTRACED);
            printf("wpid: %d\n", (int)wpid);
        } while(!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}

// read input from socket and call tokenize
int read_and_tokenize(int clientfd)
{
    char   buffer[BUFFER_SIZE + 1];
    char **tokens;
    int    itr;

    struct pollfd pfd = {clientfd, POLLIN, 0};

    signal(SIGINT, &handle_sigint);

    while(g_running)
    {
        // wait for client connection to have incoming data
        int ret = poll(&pfd, 1, TIMEOUT);

        // error polling
        if(ret < 0)
        {
            break;
        }
        // Timeout expired, continue
        if(ret == 0)
        {
            continue;
        }

        if(pfd.revents & POLLIN)
        {
            ssize_t bytes_read;

            bytes_read = read(clientfd, buffer, (size_t)BUFFER_SIZE);
            if(bytes_read < 0)
            {
                perror("read");
                continue;
            }

            // null terminate buffer
            buffer[bytes_read] = '\0';

            // tokenize buffer
            tokens = tokenize_commands(buffer);

            itr = 0;
            if(tokens != NULL)
            {
                while(tokens[itr] != NULL)
                {
                    launch_processs(tokens);
                    printf("token: %s\n", tokens[itr]);
                    itr++;
                }

                // free all tokens and array of tokens
                for(int i = 0; tokens[i] != NULL; i++)
                {
                    free(tokens[i]);
                }
                free((void *)tokens);
            }
        }
    }

    // close client connection
    close(clientfd);
    printf("closing client connection\n");
    return 0;
}

// tokenize the input
char **tokenize_commands(char *buffer)
{
    char      **tokens;                    // array of tokens
    char      **temp;                      // temp array to store reallocated array
    const char *token;                     // token buffer
    char       *rest = buffer;             // pointer to char * to keep context of where we are in the buffer
    int         index;                     // index of tokens array
    int         bufsize = MAX_COMMANDS;    // initial amount of tokens allowed

    // alloacte space for tokens
    tokens = (char **)malloc((size_t)bufsize * sizeof(char *));
    if(!tokens)
    {
        perror("malloc fail");
        return NULL;
    }

    index = 0;
    // tokenize buffer until null is reached
    while((token = strtok_r(rest, " ", &rest)))
    {
        // malloc space for token
        tokens[index] = (char *)malloc((strlen(token) + 1) * sizeof(char));
        if(tokens[index] == NULL)
        {
            perror("malloc");
            free((void *)tokens);
            return NULL;
        }
        // copy token into tokens array
        strcpy(tokens[index], token);
        index++;

        // if max tokens is reached. realloc to add more space
        if(index >= bufsize)
        {
            bufsize += MAX_COMMANDS;
            temp = (char **)realloc((void *)tokens, (size_t)bufsize * sizeof(char *));
            if(!tokens)
            {
                perror("realloc fail");
                free((void *)tokens);
                return NULL;
            }

            tokens = temp;
        }
    }

    // add null to end of tokens array
    tokens[index] = NULL;

    return tokens;
}
