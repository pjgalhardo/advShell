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
#include "lib/timer.h"

#define COMMAND_EXIT "exit"
#define COMMAND_RUN "run"

#define MAXARGS 3
#define BUFFER_SIZE 100

vector_t *stopTimes;
vector_t *startTimes;
vector_t *children;

int runningChildren = 0;

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

int main(int argc, char **argv)
{

    char *args[MAXARGS + 1];
    char bufferClient[BUFFER_SIZE], bufferShell[BUFFER_SIZE];
    int MAXCHILDREN = -1;
    int fserv, sret;
    fd_set readfds;
    fd_set s_rd;
    struct timeval timeout;

    if (argv[1] != NULL)
    {
        MAXCHILDREN = atoi(argv[1]);
    }

    children = vector_alloc(MAXCHILDREN);
    startTimes = vector_alloc(MAXCHILDREN);
    stopTimes = vector_alloc(MAXCHILDREN);

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

    if (((fserv = open(fServPath, O_RDONLY)) == -1))
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
                read(fserv, bufferClient, BUFFER_SIZE);
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

        if (strcmp(bufferClient, "") == 0)
        {
            numArgs = readLineArguments(args, MAXARGS + 1, bufferShell, BUFFER_SIZE);
        }
        else
        {
            numArgs = readLineArguments(args, MAXARGS + 1, bufferClient, BUFFER_SIZE);
        }
        /* EOF (end of file) do stdin ou comando "sair" */
        if ((strcmp(bufferClient, "") == 0) && (numArgs < 0 || (numArgs > 0 && (strcmp(args[0], COMMAND_EXIT) == 0))))
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
                readTime(startTimes, pid);
                runningChildren++;
                printf("%s: background child started with PID %d.\n\n", COMMAND_RUN, pid);
                continue;
            }
            else
            {
                char seqsolver[] = "../CircuitRouter-SeqSolver/CircuitRouter-SeqSolver";
                char *newArgs[3] = {seqsolver, args[1], NULL};

                execv(seqsolver, newArgs);
                perror("Error while executing child process"); // Nao deveria chegar aqui
                exit(EXIT_FAILURE);
            }
        }

        else if (numArgs == 0)
        {
            /* Nenhum argumento; ignora e volta a pedir */
            continue;
        }
        else if ((strcmp(bufferClient, "") != 0))
        {
            printf("Command not supported.\n");
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
    freeTime(startTimes);
    freeTime(stopTimes);

    return EXIT_SUCCESS;
}
