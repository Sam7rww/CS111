#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/wait.h>

#define arg_size 32
#define pipe_size 32
#define DELIMITER " \t\r\n\a"

int exec_program(char **args)
{
    int rc = fork();
    if (rc < 0)
    {
        // fork failed; exit
        fprintf(stderr, "fork failed\n");
        exit(1);
    }
    else if (rc == 0)
    {
        // child (new process)
        execvp(args[0], args);
        printf("the command is wrong, please check!\n");
        exit(127);
    }
    else
    {
        // parent goes down this path (original process)
        sleep(1);
        int statloc;
        wait(&statloc);
        // printf("jsh state: %d\n", statloc);
        printf("jsh state: %d\n", WEXITSTATUS(statloc));
    }

    return 1;
}

// this is used to certify whether it is exit command
int exec_command(char **args)
{
    char *exit_command = strdup("exit");

    if (args[0] == NULL || strlen(args[0]) == 0)
    {
        // the command is null;
        return 1;
    }

    if (strcmp(args[0], exit_command) == 0)
    {
        // the command is exit;
        return 0;
    }

    return exec_program(args);
}

char **split_line(char *line, int type)
{
    int args_size = arg_size;
    int position = 0;
    char **args = malloc(args_size * sizeof(char *));
    char *token;

    // assert(line !=NULL);

    // error check
    if (!args)
    {
        fprintf(stderr, "allocation space for args error\n");
        return NULL;
    }

    if (type == 1)
    {
        // split pipe |
        while ((token = strsep(&line, "|")) != NULL)
        {
            // printf("token is %s\n",token);
            // strsep will split empty string that we need to ignore
            if (strlen(token) == 0)
                continue;

            args[position] = token;
            position++;

            if (position >= args_size)
            {
                args_size += arg_size;
                args = realloc(args, args_size * sizeof(char *));
                if (!args)
                {
                    fprintf(stderr, "allocation space for args error\n");
                    return NULL;
                }
            }
        }
    }
    else if (type == 2)
    {
        // split args
        while ((token = strsep(&line, DELIMITER)) != NULL)
        {
            // printf("token is %s\n",token);
            // strsep will split empty string that we need to ignore
            if (strlen(token) == 0)
                continue;

            args[position] = token;
            position++;

            if (position >= args_size)
            {
                args_size += arg_size;
                args = realloc(args, args_size * sizeof(char *));
                if (!args)
                {
                    fprintf(stderr, "allocation space for args error\n");
                    return NULL;
                }
            }
        }
    }

    args[position] = NULL;
    return args;
}

char *jsh_readline()
{
    char *line = NULL;
    size_t linecap = 0; // the capacity of the buffer
    ssize_t linelen = getline(&line, &linecap, stdin);
    if (linelen < 0)
    {
        fprintf(stderr, "an error occurs in getline func\n");
    }
    return line;
}

// this function is used to calculate the number of pipes, at least 1;
int pipe_num(char **pipes)
{
    int i = 0;
    while (pipes[i] != NULL)
    {
        i++;
    }
    return i;
}

// designed for pipeline process, need to handle dup2()
int spawn_proc(int in, int out, char *line, pid_t *pipes, int pipeNum)
{
    int rc;
    // split the line to get command args.
    char **args = split_line(line, 2);

    rc = fork();
    if (rc < 0)
    {
        // fork failed; exit
        fprintf(stderr, "fork failed\n");
        exit(1);
    }
    else if (rc == 0)
    {
        // child process
        if (in != 0)
        {
            int fd = dup2(in, 0);
            if (fd == -1)
            {
                printf("dup2 error\n");
                exit(127);
            }
            close(in);
        }

        if (out != 1)
        {
            int fd = dup2(out, 1);
            if (fd == -1)
            {
                printf("dup2 error\n");
                exit(127);
            }
            close(out);
        }

        // set current pid_t
        pipes[pipeNum] = getpid();

        execvp(args[0], args);
        printf("the command between pipeline is wrong, please check!\n");
        exit(127);
    }
    else if (rc > 0)
    {
        // parent wait
        // wait(NULL);
    }

    return 1;
}

void jsh_loop()
{
    char *line;
    char **pipes;
    int status = 1;

    // variable for pipe
    int i;
    int in, fd[2];

    do
    {
        printf("jsh$ ");
        line = jsh_readline();
        if (line == NULL)
        {
            continue;
        }
        // first split all the pipes
        pipes = split_line(line, 1);

        int num = pipe_num(pipes);
        if (num <= 1)
        {
            char **args = split_line(pipes[0], 2);
            if (args == NULL)
            {
                continue;
            }
            int result = exec_command(args);
            // command is exit, change status
            if (result == 0)
            {
                status = 0;
                break;
            }
        }
        else
        {
            // set first process input
            in = 0;
            pid_t pipePid[num];

            // produce new process to execute command and pipe
            for (i = 0; i < num - 1; ++i)
            {
                int pp_res = pipe(fd);
                if (pp_res < 0)
                {
                    printf("pipe function error\n");
                    exit(127);
                }
                sleep(0.5);
                spawn_proc(in, fd[1], pipes[i], &pipePid[0], i);
                close(fd[1]);
                in = fd[0];
            }

            int rc;
            char **args = split_line(pipes[num - 1], 2);

            rc = fork();
            if (rc < 0)
            {
                // fork failed; exit
                fprintf(stderr, "fork failed\n");
                exit(1);
            }
            else if (rc == 0)
            {
                /* Last part of the pipeline - set stdin to read end of the previous pipe
                and output to the original file descriptor 1. */
                if (in != 0)
                {
                    int fd = dup2(in, 0);
                    if (fd == -1)
                    {
                        printf("dup2 error\n");
                        exit(127);
                    }
                }

                // set current pid_t
                pipePid[num - 1] = getpid();

                // Execute the last command.
                execvp(args[0], args);
                printf("the last command of pipeline is wrong, please check!\n");
                exit(127);
            }
            else if (rc > 0)
            {

                for (i = 0; i < num - 1; i++)
                {
                    int stat;
                    // wait for all pipe commands
                    waitpid(pipePid[i], &stat, 0);
                    if (WEXITSTATUS(stat) == 127)
                    {
                        printf("the command between pipeline is wrong, please check!\n");
                    }
                }
                sleep(1);
                // wait for last pipe command and print status
                int statloc;
                waitpid(pipePid[num - 1], &statloc, 0);
                printf("jsh state: %d\n", WEXITSTATUS(statloc));
            }
        }

        free(line);
        free(pipes);

    } while (status);
}

int main(int argc, char *argv[])
{
    // printf("hello world (pid:%d)\n", (int) getpid());

    jsh_loop();

    return 0;
}