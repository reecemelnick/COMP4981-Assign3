#include "server.h"
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
#define BUFFER_SIZE 1024
#define MAX_COMMANDS 3
#define PORT 8000
#define BUILTINS 6
#define PATH_MAX 4096

sig_atomic_t static volatile g_running = 1;    // NOLINT

int built_cd(char *message, char **args);
int built_help(char *message, char **args);
int built_exit(char *message, char **args);
int built_echo(char *message, char **args);
int built_pwd(char *message, char **args);
int built_type(char *message, char **args);

int   launch_processs(int client_fd, char **tokens);
int   excecute_shell(int fd, char **args);
char *get_full_path(char **args);

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

int built_cd(char *message, char **args)
{
    if(args[1] == NULL)
    {
        fprintf(stderr, "lsh: expected argument to \"cd\"\n");
        strcpy(message, "lsh: expected argument to \"cd\"\n");
    }
    else
    {
        if(chdir(args[1]) != 0)
        {
            perror("lsh");
        }
    }
    return 1;
}

int built_echo(char *message, char **args)
{
    int i = 1;

    while(args[i] != NULL)
    {
        printf("%s ", args[i]);
        i++;
    }
    printf("\n");

    strcpy(message, "echo");

    return 1;
}

int built_pwd(char *message, char **args)
{
    char cwd[PATH_MAX];

    printf("%s\n", args[0]);

    if(getcwd(cwd, sizeof(cwd)) == NULL)
    {
        perror("pwd");
        return 1;
    }

    printf("%s\n", cwd);
    strcpy(message, "pwd");

    return 1;
}

int built_help(char *message, char **args)
{
    int i;

    const char *builtin_str[] = {"cd", "help", "exit", "pwd", "echo", "type"};

    printf("%s\n", args[0]);

    for(i = 0; i < BUILTINS; i++)
    {
        printf("  %s\n", builtin_str[i]);
    }

    printf("Use the man command for information on other programs.\n");
    strcpy(message, "Use the man command for information on other programs.\n");
    return 1;
}

int built_exit(char *message, char **args)
{
    if(message == NULL)
    {
        perror("Message pointer is NULL");
        return -1;
    }

    strcpy(message, "exiting");
    printf("%s %s\n", args[0], message);
    printf("Exiting........\n");
    return 1;
}

int built_type(char *message, char **args)
{
    const char *builtin_str[] = {"cd", "help", "exit", "pwd", "echo", "type"};

    if(args[1] == NULL)
    {
        fprintf(stderr, "type: expected argument\n");
        strcpy(message, "type: expected argument\n");
        return 1;
    }

    for(int i = 0; i < (int)(sizeof(builtin_str) / sizeof(builtin_str[0])); i++)
    {
        if(strcmp(args[1], builtin_str[i]) == 0)
        {
            printf("%s is a shell built-in command\n", args[1]);
            return 1;
        }
    }

    printf("%s is an external command\n", args[1]);
    return 1;
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
            if(res != 1)
            {
                perror("built in");
                return -1;
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
        if(path == NULL)
        {
            fprintf(stderr, "%s: command not found\n", tokens[0]);
            exit(EXIT_FAILURE);
        }

        if(dup2(client_fd, STDOUT_FILENO) == -1)
        {
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }

        // child
        if(execv(path, tokens) == -1)
        {
            perror("exec");
        }

        write(client_fd, "\n", 1);

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
