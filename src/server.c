#include "server.h"
#include "../include/builtins.h"
#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define TIMEOUT 1000
#define BUFFER_SIZE 4096
#define MAX_COMMANDS 3
#define PORT 8000
#define BUILTINS 6

sig_atomic_t static volatile g_running = 1;    // NOLINT

void sigchld_handler(int signo);

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

void sigchld_handler(int signo)
{
    (void)signo;
    while(waitpid(-1, NULL, WNOHANG) > 0)
    {
    }
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
            pid_t pid;
            int   newsockfd = accept(server_sock, (struct sockaddr *)&host_addr, &host_addrlen);
            if(newsockfd < 0)
            {
                perror("accept");
                break;
            }

            printf("Connection made\n");

            pid = fork();
            if(pid == 0)
            {
                close(server_sock);
                handle_client(newsockfd);
                close(newsockfd);
                exit(0);
            }
            else if(pid > 0)
            {
                close(newsockfd);
            }
            else
            {
                perror("fork fail");
            }
        }
    }

    signal(SIGCHLD, sigchld_handler);

    return server_sock;
}

// wait for stuff to read. Tokenize input.
int handle_client(int clientfd)
{
    int res;

    res = read_and_tokenize(clientfd);
    if(res == -1)
    {
        perror("tokenize");
        exit(EXIT_FAILURE);
    }

    return res;
}

int excecute_shell(int fd, char **args)
{
    const char *builtin_str[] = {"cd", "help", "exit", "pwd", "echo", "type"};

    int (*builtin_func[])(char *, char **) = {&built_cd, &built_help, &built_exit, &built_pwd, &built_echo, &built_type};

    char    result_message[BUFFER_SIZE];
    ssize_t bytes_wrote;
    int     res;

    if(args == NULL || args[0] == NULL)
    {
        return -1;
    }

    for(int i = 0; i < BUILTINS; i++)
    {
        if(strcmp(args[0], builtin_str[i]) == 0)
        {
            res = (*builtin_func[i])(result_message, args);
            if(res == -1)
            {
                perror("built in");
                return -1;
            }

            // exit code
            if(res == 2)
            {
                return 2;
            }

            printf("about to write: %s\n", result_message);

            bytes_wrote = write(fd, result_message, strlen(result_message));
            if(bytes_wrote < 0)
            {
                perror("write");
            }

            return 1;
        }
    }

    printf("launching command\n");

    return launch_processs(fd, args);
}

char *get_full_path(char **args)
{
    char *cmd_path = NULL;
    char *path_env = getenv("PATH");

    if(path_env != NULL)
    {
        const char *path    = NULL;
        char       *saveptr = NULL;

        path = strtok_r(path_env, ":", &saveptr);

        while(path != NULL)
        {
            size_t cmd_len = strlen(path) + strlen(args[0]) + 2;

            cmd_path = (char *)malloc(cmd_len);
            if(cmd_path == NULL)
            {
                perror("malloc");
                exit(EXIT_FAILURE);
            }

            snprintf(cmd_path, cmd_len, "%s/%s", path, args[0]);

            if(access(cmd_path, X_OK) == 0)
            {
                return cmd_path;
            }

            free(cmd_path);
            cmd_path = NULL;
            path     = strtok_r(NULL, ":", &saveptr);
        }
    }

    return NULL;
}

int launch_processs(int client_fd, char **tokens)
{
    pid_t pid;
    int   status;

    pid = fork();
    if(pid == 0)
    {
        char *path = get_full_path(tokens);

        if(dup2(client_fd, STDOUT_FILENO) == -1 || dup2(client_fd, STDERR_FILENO) == -1)
        {
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }

        if(path == NULL || access(path, X_OK) == -1)
        {
            fprintf(stderr, "%s: command not found\n", tokens[0]);
            exit(EXIT_FAILURE);
        }

        // child
        if(execv(path, tokens) == -1)
        {
            perror("exec");
        }

        free(path);
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
    char **tokens = NULL;
    char   buffer[BUFFER_SIZE + 1];

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
            int     itr;
            int     res;

            bytes_read = read(clientfd, buffer, (size_t)BUFFER_SIZE);
            if(bytes_read < 0)
            {
                perror("read");
                return -1;
            }

            // null terminate buffer
            buffer[bytes_read] = '\0';

            // tokenize buffer
            tokens = tokenize_commands(buffer);

            res = excecute_shell(clientfd, tokens);
            if(res == -1)
            {
                perror("execute");
                return -1;
            }

            itr = 0;
            if(tokens != NULL)
            {
                while(tokens[itr] != NULL)
                {
                    printf("token: %s\n", tokens[itr]);
                    itr++;
                }

                for(int i = 0; tokens[i] != NULL; i++)
                {
                    free(tokens[i]);
                }
                free((void *)tokens);
            }

            printf("%d\n", res);

            // exit code
            if(res == 2)
            {
                break;
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
