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
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/select.h>
#include "lib/utility.h"
#include "lib/timer.h"

#define COMMAND_EXIT "exit"
#define COMMAND_RUN "run"

#define MAXARGS 3
#define BUFFER_SIZE 100

vector_t *stopTimes;
vector_t *startTimes;
vector_t *children;

int runningChildren = 0;

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
struct timestruct
{
    pid_t pid;
    TIMER_T time;
};

void readTime(vector_t *times, pid_t pid)
{
    struct timestruct *timer = malloc(sizeof(struct timestruct));
    if (timer == NULL)
    {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }
    TIMER_READ(timer->time);
    timer->pid = pid;
    vector_pushBack(times, timer);
}

void freeTime(vector_t *times)
{
    for (int i = 0; i < vector_getSize(times); ++i)
    {
        free(vector_at(times, i));
    }
    vector_free(times);
}

float printTime(pid_t pid)
{
    TIMER_T timestarted;
    TIMER_T timestopped;

    for (int i = 0; i < vector_getSize(startTimes); ++i)
    {
        struct timestruct *startTime = (struct timestruct *)vector_at(startTimes, i);
        if (startTime->pid == pid)
        {
            timestarted = startTime->time;
        }
    }
    for (int i = 0; i < vector_getSize(stopTimes); ++i)
    {
        struct timestruct *stopTime = (struct timestruct *)vector_at(stopTimes, i);
        if (stopTime->pid == pid)
        {
            timestopped = stopTime->time;
        }
    }
    return TIMER_DIFF_SECONDS(timestarted, timestopped);
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
        readTime(stopTimes, child->pid);
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
            printf("CHILD EXITED: (PID=%d; return %s; %d s)\n", pid, ret, (int)printTime(pid));
        }
    }
    puts("END.");
}

void sigchld_handler(int sig, siginfo_t *siginfo, void *x)
{
    runningChildren--;
    waitForChild(children);
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

void readFIFO(int *faux, char *buffer, char *clientPipe)
{
    int i, j;

    if (read(*faux, buffer, BUFFER_SIZE) == -1)
    {
        perror("FAILED READING FIFO.\n");
        exit(EXIT_FAILURE);
    }

    strcpy(clientPipe, "");
    char tempBuffer[BUFFER_SIZE];
    for (i = 0; buffer[i] != '\n'; i++)
    {
        clientPipe[i] = buffer[i];
    }
    i++;
    for (j = 0; i < BUFFER_SIZE; i++, j++)
    {
        tempBuffer[j] = buffer[i];
    }
    strcpy(buffer, tempBuffer);
}

int main(int argc, char **argv)
{
    char fServPath[BUFFER_SIZE];
    char *args[MAXARGS + 1];
    char bufferClient[BUFFER_SIZE], bufferShell[BUFFER_SIZE], clientPipe[BUFFER_SIZE];
    int MAXCHILDREN = -1;
    int fserv, selectRet;
    fd_set readfds;

    if (argv[1] != NULL)
    {
        MAXCHILDREN = atoi(argv[1]);
    }

    children = vector_alloc(MAXCHILDREN);
    startTimes = vector_alloc(MAXCHILDREN);
    stopTimes = vector_alloc(MAXCHILDREN);
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
                if (clientFlag(fserv, &readfds))
                {
                    readFIFO(&fserv, bufferClient, clientPipe);
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
                //runningChildren e atualizado pela captura de SIGCHILD
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
            if (MAXCHILDREN != -1)
            {
                while (runningChildren >= MAXCHILDREN)
                {
                    //runningChildren e atualizado pela captura de SIGCHILD
                }
            }

            struct sigaction act;

            memset(&act, 0, sizeof(act));
            act.sa_sigaction = &sigchld_handler;
            act.sa_flags = SA_RESTART;
            sigaction(SIGCHLD, &act, 0);
            pid = fork();
            if (pid < 0)
            {
                perror("Failed to create new process.");
                exit(EXIT_FAILURE);
            }

            if (pid > 0)
            {
                pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; //Mutex implementado para prevenir erros com o malloc do readTime caso ocorra o signal sigchild
                if (pthread_mutex_lock(&mutex) != 0)
                {
                    perror("Mutex locking error.");
                    exit(EXIT_FAILURE);
                }
                readTime(startTimes, pid);
                if (pthread_mutex_unlock(&mutex) != 0)
                {
                    perror("Mutex unlocking error.");
                    exit(EXIT_FAILURE);
                }
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
    freeTime(startTimes);
    freeTime(stopTimes);

    return EXIT_SUCCESS;
}
