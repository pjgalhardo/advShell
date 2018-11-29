
/*
// Projeto SO - exercise 1, version 1
// Sistemas Operativos, DEI/IST/ULisboa 2018-19
*/

#include "lib/commandlinereader.h"
#include "lib/vector.h"
#include "CircuitRouter-AdvShell.h"
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>

#define COMMAND_EXIT "exit"
#define COMMAND_RUN "run"

#define MAXARGS 3
#define BUFFER_SIZE 100

int clientRequest(char *buffer)
{

    return !(strcmp(buffer, "") == 0);
}

void deleteExistentPipes()
{
    //FIXME: nao sei se gosto de como esta
    int i;
    char fileReturn[BUFFER_SIZE];
    strcpy(fileReturn, "1.pipe");
    for (i = 2; access(fileReturn, F_OK) != -1; i++)
    {
        remove(fileReturn);
        sprintf(fileReturn, "%d.pipe", i);
    }
}

void waitForChild(vector_t *children)
{
    while (1)
    {
        child_t *child = malloc(sizeof(child_t));
        if (child == NULL)
        {
            perror("Error allocating memory");
            exit(EXIT_FAILURE);
        }
        child->pid = wait(&(child->status));
        if (child->pid < 0)
        {
            if (errno == EINTR)
            {
                /* Este codigo de erro significa que chegou signal que interrompeu a espera
                   pela terminacao de filho; logo voltamos a esperar */
                free(child);
                continue;
            }
            else
            {
                perror("Unexpected error while waiting for child.");
                exit(EXIT_FAILURE);
            }
        }
        vector_pushBack(children, child);
        return;
    }
}

void printChildren(vector_t *children)
{
    for (int i = 0; i < vector_getSize(children); ++i)
    {
        child_t *child = vector_at(children, i);
        int status = child->status;
        pid_t pid = child->pid;
        if (pid != -1)
        {
            const char *ret = "NOK";
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
            {
                ret = "OK";
            }
            printf("CHILD EXITED: (PID=%d; return %s)\n", pid, ret);
        }
    }
    puts("END.");
}

int main(int argc, char **argv)
{

    char *args[MAXARGS + 1];
    char bufferClient[BUFFER_SIZE], bufferShell[BUFFER_SIZE], clientPipe[BUFFER_SIZE];
    int MAXCHILDREN = -1;
    vector_t *children;
    int runningChildren = 0;
    int fserv, sret, j, i, fanswer;
    fd_set readfds;
    fd_set s_rd;
    struct timeval timeout;

    if (argv[1] != NULL)
    {
        MAXCHILDREN = atoi(argv[1]);
    }

    children = vector_alloc(MAXCHILDREN);
    deleteExistentPipes();
    printf("Welcome to CircuitRouter-AdvShell\n\n");

    /*  CRIACAO DO FIFO   */

    char *fServPath = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    strcpy(fServPath, argv[0]);
    strcat(fServPath, ".pipe");
    fServPath += 2;
    unlink(fServPath);

    if (mkfifo(fServPath, 0777) < 0)
    {
        exit(-1);
    }

    if (((fserv = open(fServPath, O_RDWR)) == -1))
    {
        exit(-1);
    }

    while (1)
    {
        sret = 0;
        memset(bufferClient, 0, sizeof(bufferClient));
        memset(bufferShell, 0, sizeof(bufferShell));

        while (sret == 0)
        {
            FD_ZERO(&readfds);
            FD_SET(fserv, &readfds);
            FD_ZERO(&s_rd);
            FD_SET(fileno(stdin), &s_rd);
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;
            if ((sret = select(8, &readfds, NULL, NULL, &timeout)) == 1)
            {
                //FIXME: fazer numa funcao
                read(fserv, bufferClient, BUFFER_SIZE);
                strcpy(clientPipe, "");
                char tempBuffer[BUFFER_SIZE];
                for (i = 0; bufferClient[i] != '\n'; i++)
                {
                    clientPipe[i] = bufferClient[i];
                }
                i++;
                for (j = 0; i < BUFFER_SIZE; i++, j++)
                {
                    tempBuffer[j] = bufferClient[i];
                }
                strcpy(bufferClient, tempBuffer);
            }

            else
            {
                timeout.tv_sec = 0;
                timeout.tv_usec = 100000;
                if ((sret = select(fileno(stdin) + 1, &s_rd, NULL, NULL, &timeout)) == 1)
                {
                    fgets(bufferShell, BUFFER_SIZE, stdin);
                }
            }
        }

        int numArgs;

        if (!clientRequest(bufferClient))
        {
            numArgs = readLineArguments(args, MAXARGS + 1, bufferShell, BUFFER_SIZE);
        }
        else
        {
            numArgs = readLineArguments(args, MAXARGS + 1, bufferClient, BUFFER_SIZE);
        }
        /* EOF (end of file) do stdin ou comando "sair" */
        if (!clientRequest(bufferClient) && (numArgs < 0 || (numArgs > 0 && (strcmp(args[0], COMMAND_EXIT) == 0))))
        {
            printf("CircuitRouter-AdvShell will exit.\n--\n");

            /* Espera pela terminacao de cada filho */
            while (runningChildren > 0)
            {
                waitForChild(children);
                runningChildren--;
            }

            printChildren(children);
            printf("--\nCircuitRouter-AdvShell ended.\n");
            break;
        }

        else if (numArgs > 0 && strcmp(args[0], COMMAND_RUN) == 0)
        {
            int pid;
            if (numArgs < 2)
            {
                printf("%s: invalid syntax. Try again.\n", COMMAND_RUN);
                continue;
            }
            if (MAXCHILDREN != -1 && runningChildren >= MAXCHILDREN)
            {
                waitForChild(children);
                runningChildren--;
            }

            pid = fork();
            if (pid < 0)
            {
                perror("Failed to create new process.");
                exit(EXIT_FAILURE);
            }

            if (pid > 0)
            {
                runningChildren++;
                printf("%s: background child started with PID %d.\n\n", COMMAND_RUN, pid);
                continue;
            }
            else
            {
                char seqsolver[] = "../CircuitRouter-SeqSolver/CircuitRouter-SeqSolver";
                if (clientRequest(bufferClient))
                {
                    char *newArgs[4] = {seqsolver, args[1], clientPipe, NULL};
                    execv(seqsolver, newArgs);
                }
                else
                {
                    char *newArgs[3] = {seqsolver, args[1], NULL};
                    execv(seqsolver, newArgs);
                }

                perror("Error while executing child process"); // Nao deveria chegar aqui
                exit(EXIT_FAILURE);
            }
        }

        else if (numArgs == 0)
        {
            /* Nenhum argumento; ignora e volta a pedir */
            continue;
        }
        else if (clientRequest(bufferClient))
        {
            fanswer = open(clientPipe, O_WRONLY);
            write(fanswer, "Command not supported.\n", BUFFER_SIZE);
            close(fanswer);
        }
        else
            printf("Unknown command. Try again.\n");
    }

    close(fserv);
    unlink(fServPath);
    fServPath -= 2;
    free(fServPath);

    for (int i = 0; i < vector_getSize(children); i++)
    {
        free(vector_at(children, i));
    }
    vector_free(children);
    deleteExistentPipes();

    return EXIT_SUCCESS;
}
