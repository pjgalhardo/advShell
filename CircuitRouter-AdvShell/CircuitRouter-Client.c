#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define BUFFER_SIZE 100

void clientRequest(int faux, char *filePath)
{
    char command[BUFFER_SIZE], completeCommand[BUFFER_SIZE];
    if (fgets(command, BUFFER_SIZE, stdin) == NULL)
    {
        perror("FAILED RECEIVING INPUT FROM STDIN\n");
        exit(EXIT_FAILURE);
    }
    strcpy(completeCommand, filePath);
    strcat(completeCommand, "\n");
    strcat(completeCommand, command);
    if (write(faux, completeCommand, BUFFER_SIZE) == -1)
    {
        perror("FAILED WRITING TO FIFO.\n");
        exit(EXIT_FAILURE);
    }
}

void checkNumberOfArguments(int args)
{
    if (args != 2)
    {
        perror("Wrong number of arguments.\n");
        exit(EXIT_FAILURE);
    }
}

void createClientPipe(char *fileReturn)
{
    int i;
    strcpy(fileReturn, "1.pipe");
    for (i = 1; access(fileReturn, F_OK) == 0; i++)
    {
        sprintf(fileReturn, "%d.pipe", i);
    }

    if (mkfifo(fileReturn, 0777) < 0)
    {
        perror("FAILED CREATING FIFO.\n");
        exit(EXIT_FAILURE);
    }
}

void openFIFO(char *filePath, int *faux, int flag)
{
    *faux = open(filePath, flag);
    if (*faux == -1)
    {
        perror("FAILED OPENING FIFO.\n");
        exit(EXIT_FAILURE);
    }
}

void printResult(int faux)
{
    char answer[BUFFER_SIZE];
    if (read(faux, answer, BUFFER_SIZE) == -1)
    {
        perror("FAILED READING FIFO.\n");
        exit(EXIT_FAILURE);
    }
    printf("%s", answer);
}

void closeFIFO(int faux)
{
    if (close(faux) == -1)
    {
        perror("FAILED CLOSING FIFO\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{

    int fclient, fAnswer;
    char filePath[BUFFER_SIZE], fileReturn[BUFFER_SIZE];

    strcpy(filePath, argv[1]);
    createClientPipe(fileReturn);
    while (1)
    {
        openFIFO(filePath, &fclient, O_WRONLY);
        clientRequest(fclient, fileReturn);
        closeFIFO(fclient);
        openFIFO(fileReturn, &fAnswer, O_RDONLY);
        printResult(fAnswer);
        closeFIFO(fAnswer);
    }

    //end of program
    return 0;
}