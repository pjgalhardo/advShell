
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
#include "lib/utility.h"

#define COMMAND_EXIT "exit"
#define COMMAND_RUN "run"

#define MAXARGS 3
#define BUFFER_SIZE 100

int clientRequest(char *buffer)
{

    return !(strcmp(buffer, "") == 0);
}

void writeToFIFO(char *path, char *buffer, int size)
{
    int faux;
    if (((faux = open(path, O_WRONLY)) == -1))
    {
        exit(-1);
    }
    if (write(faux, buffer, size) == -1)
    {
        perror("FAILED WRITING TO FIFO.\n");
        exit(EXIT_FAILURE);
    }
    if (close(faux) == -1)
    {
        perror("FAILED CLOSING FIFO.\n");
        exit(EXIT_FAILURE);
    }
}

void deleteExistentPipes()
{
    //FIXME: nao sei se gosto de como esta
    int i;
    char fileReturn[BUFFER_SIZE];
    strcpy(fileReturn, "1.pipe");
    for (i = 2; access(fileReturn, F_OK) != -1; i++)
    {
        if (unlink(fileReturn) == -1)
        {
            perror("FAILED UNLINKING FILE.\n");
            exit(EXIT_FAILURE);
        }
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

void openFIFO(char *filePath, char *arg, int *fserv)
{

    char *faux = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    strcpy(faux, arg);
    strcat(faux, ".pipe");
    faux += 2;
    strcpy(filePath, faux);
    faux -= 2;
    free(faux);
    unlink(filePath);

    if (mkfifo(filePath, 0777) < 0)
    {
        exit(-1);
    }

    if (((*fserv = open(filePath, O_RDWR)) == -1))
    {
        exit(-1);
    }
}

void resetFlags(int fdFIFO, fd_set *readfds)
{
    FD_ZERO(readfds);
    FD_SET(fdFIFO, readfds);
    FD_SET(fileno(stdin), readfds);
}

int clientFlag(int faux, fd_set *readfds)
{
    return FD_ISSET(faux, readfds);
}

int main(int argc, char **argv)
{
    char fServPath[BUFFER_SIZE];
    char *args[MAXARGS + 1];
    char bufferClient[BUFFER_SIZE], bufferShell[BUFFER_SIZE], clientPipe[BUFFER_SIZE];
    int MAXCHILDREN = -1;
    vector_t *children;
    int runningChildren = 0;
    int fserv, selectRet, j, i;
    fd_set readfds;
    struct timeval timeout;

    if (argv[1] != NULL)
    {
        MAXCHILDREN = atoi(argv[1]);
    }

    children = vector_alloc(MAXCHILDREN);
    deleteExistentPipes();
    printf("Welcome to CircuitRouter-AdvShell\n\n");

    /*  CRIACAO DO FIFO   */
    openFIFO(fServPath, argv[0], &fserv);

    while (1)
    {
        selectRet = 0;
        memset(bufferClient, 0, sizeof(bufferClient));
        memset(bufferShell, 0, sizeof(bufferShell));

        while (selectRet == 0)
        {
            resetFlags(fserv, &readfds);
            if ((selectRet = select(MAX(fileno(stdin), fserv) + 1, &readfds, NULL, NULL, NULL)) == 1)
            {
                //FIXME: fazer numa funcao
                if (clientFlag(fserv, &readfds))
                {
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
            writeToFIFO(clientPipe, "Command not supported.\n", BUFFER_SIZE);
        }
        else
            printf("Unknown command. Try again.\n");
    }

    close(fserv);
    unlink(fServPath);
    for (int i = 0; i < vector_getSize(children); i++)
    {
        free(vector_at(children, i));
    }
    vector_free(children);
    deleteExistentPipes();

    return EXIT_SUCCESS;
}
