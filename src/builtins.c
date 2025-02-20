#include "../include/builtins.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUILTINS 6

int built_cd(char *message, char **args)
{
    if(args[1] == NULL)
    {
        strcpy(message, "shell: expected argument to \"cd\"\n");
    }
    else
    {
        if(chdir(args[1]) != 0)
        {
            perror("cd");
        }
    }
    return 1;
}

int built_echo(char *message, char **args)
{
    int i = 1;

    while(args[i] != NULL)
    {
        strcat(message, args[i]);
        strcat(message, " ");
        i++;
    }

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

    strcpy(message, cwd);

    return 1;
}

int built_help(char *message, char **args)
{
    int i;

    const char *builtin_str[] = {"cd", "help", "exit", "pwd", "echo", "type"};

    printf("%s\n", args[0]);

    strcpy(message, "Built in commands:\n");

    for(i = 0; i < BUILTINS; i++)
    {
        strcat(message, builtin_str[i]);
        strcat(message, ", ");
    }
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
    return 2;
}

int built_type(char *message, char **args)
{
    const char *builtin_str[] = {"cd", "help", "exit", "pwd", "echo", "type"};

    if(args[1] == NULL)
    {
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
