#ifndef CIRCUITROUTER_SHELL_H
#define CIRCUITROUTER_SHELL_H

#include "lib/vector.h"
#include <sys/types.h>

typedef struct
{
    pid_t pid;
    int status;
} child_t;

void waitForChild(vector_t *children);
void printChildren(vector_t *children);
void deleteExistentPipes();
void writeToFIFO(char *path, char *buffer, int size);
int clientRequest(char *buffer);
void openFIFO(char *filePath, char *arg, int *fserv);
void resetFlags(int fdFIFO, fd_set *readfds);
int clientFlag(int faux, fd_set *readfds);
void readFIFO(int *faux, char *buffer, char *clientPipe);

#endif /* CIRCUITROUTER_SHELL_H */