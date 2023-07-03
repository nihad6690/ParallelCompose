#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

#define NUM_OF_ALPHA 26
#define BUFFSIZE 4096
#define NUM_OF_PIPES 100

int pipes[NUM_OF_PIPES][2];
int index_pipes = 0;

void sendFileNameToChild(char *file_name, int write_pipe, int i);
void SIGCHLD_handler(int sigNum);

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        pid_t pids[argc];

        int i;

        /* Creates all of the required pipes*/
        for (i = 0; i < argc - 1; i++)
        {

            if (pipe(pipes[i]) == -1)
            {
                perror("childToParent pipe");
                exit(1);
            }
        }

        signal(SIGCHLD, SIGCHLD_handler);

        for (i = 0; i < argc - 1; i++)
        {

            pids[i] = fork();

            if (pids[i] == 0)
            {
                /* Checks for the special input and kills the child with SIGINT*/
                if (strcmp(argv[i + 1], "SIG") == 0)
                {
                    sleep(10 + 2 * i);
                    close(pipes[i][0]);
                    close(pipes[i][1]);

                    kill(getpid(), SIGINT);
                }
                /* Child process closes up read side of its write pipe */
                close(pipes[i][0]);

                /* The name of the file is sent to the function to create an array representing the
                   alphabet count and that is written to the pipe*/
                sendFileNameToChild(argv[i + 1], pipes[i][1], i);

                close(pipes[i][1]);

                exit(0);
            }
            else if (pids[i] > 0)
            {
                /* Parent process closes write side of its read pipe */
                close(pipes[i][1]);

                /* waiting to catch the signal and write to the file if the exit code is 0
                   Also avoiding  busy waits! */
                pause();

                close(pipes[i][0]);

                /*Breaks out of the loop to prevent an infinite loop*/
                if (index_pipes == argc - 1)
                {
                    break;
                }
            }
        }
    }
    else
    {
        printf("There are no files to read from\n");
    }

    /* Closing all the file descriptors */
    for (int i = 0; i < argc - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    return 0;
}

void sendFileNameToChild(char *file_name, int write_pipe, int i)
{
    char *buf = malloc(sizeof(char) * BUFFSIZE + 1);
    memset(buf, 0, sizeof(char) * BUFFSIZE + 1);
    int arr[NUM_OF_ALPHA] = {0};

    int fd = -1;
    int n;

    fd = open(file_name, O_RDONLY);
    /* Checks if the file name exists*/
    if (fd < 0)
    {
        free(buf);
        close(write_pipe);
        exit(1);
    }
    else
    {
        /* Reads the data and computes the histogram
           and then writes the histogram into the pipe */
        while ((n = read(fd, buf, BUFFSIZE)) > 0)
        {
            for (int i = 0; i < strlen(buf); i++)
            {
                int index = ((int)tolower(buf[i])) - 97;
                if (index >= 0 && index <= 25)
                {
                    arr[index]++;
                }
            }
            memset(buf, 0, sizeof(char) * BUFFSIZE + 1);
        }
        free(buf);
        close(fd);
        write(write_pipe, arr, sizeof(int) * NUM_OF_ALPHA);

        exit(0);
    }
}

void SIGCHLD_handler(int sigNum)
{
    signal(SIGCHLD, SIGCHLD_handler);

    int child_status;
    int arr[NUM_OF_ALPHA];
    pid_t child_pid = waitpid(-1, &child_status, WNOHANG);

    /*Checks if a signal was killed by SIGINT*/
    if (WIFSIGNALED(child_status))
    {
        printf("Child was killed by signal %d. Signal name %s.\n", WTERMSIG(child_status), strsignal(WTERMSIG(child_status)));
    }
    /* All of the children had exit code if 0 if everything terminated properly*/
    /* As a result the the information is read from the pipe and saved into a file*/
    else if (WEXITSTATUS(child_status) == 0)
    {
        /*Reads from the pipes and gets the array and then using that information writes into the file*/
        read(pipes[index_pipes][0], arr, sizeof(int) * NUM_OF_ALPHA);
        close(pipes[index_pipes][0]);

        char string_num[10000];
        char filename[10000] = "file";

        sprintf(string_num, "%d", (int)child_pid);

        strcat(filename, string_num);
        strcat(filename, ".hist");

        int fd = open(filename, O_CREAT | O_WRONLY, 0755);

        char alphabets[26] = "abcdefghijklmnopqrstuvwxyz";
        memset(string_num, 0, sizeof(char) * strlen(string_num));

        for (int i = 0; i < NUM_OF_ALPHA; i++)
        {
            char space = ' ';
            char newline = '\n';
            sprintf(string_num, "%d", arr[i]);

            write(fd, &alphabets[i], sizeof(char));
            write(fd, &space, sizeof(char));
            write(fd, string_num, sizeof(char) * strlen(string_num));
            if (i < NUM_OF_ALPHA - 1)
            {
                write(fd, &newline, sizeof(char));
            }
            memset(string_num, 0, sizeof(char) * strlen(string_num));
        }
        close(fd);
        printf("SIGCHLD was caught. Child %d terminated normally with exit code 0. The data is written into the file called %s.\n", child_pid, filename);
    }
    else
    {
        printf("SIGCHLD was caught. Child %d terminated abnormally with exit code 1. Nothing was written into any file.\n", child_pid);
    }
    close(pipes[index_pipes][0]);
    close(pipes[index_pipes][1]);

    index_pipes++;
}