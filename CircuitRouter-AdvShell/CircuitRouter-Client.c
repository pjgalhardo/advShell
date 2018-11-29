#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#define BUFFER_SIZE 100

int main(int argc, char *argv[])
{

    int fclient, i, fAnswer;
    if (argc != 2)
    {
        perror("Wrong number of arguments.\n");
        exit(-1);
    }
    else
    {
        char *filePath = (char *)malloc(sizeof(char) * BUFFER_SIZE);
        strcpy(filePath, argv[1]);
        strcat(filePath, ".pipe");
        //ver tamanho do fileReturn: tou a por um tamanho fixo mas se calhar devia fazer malloc
        char fileReturn[BUFFER_SIZE];
        char answer[BUFFER_SIZE];
        strcpy(fileReturn, "1.pipe");
        for (i = 1; access(fileReturn, F_OK) == 0; i++)
        {
            sprintf(fileReturn, "%d.pipe", i);
        }
        mkfifo(fileReturn, 0777);

        char *command = (char *)malloc(sizeof(char) * BUFFER_SIZE);
        char *completeCommand = (char *)malloc(sizeof(char) * BUFFER_SIZE);
        while (1)
        {
            fclient = open(filePath, O_WRONLY);
            if (fclient == -1)
            {
                perror("FAILED CREATING FIFO.\n");
            }

            fgets(command, BUFFER_SIZE, stdin);
            strcpy(completeCommand, fileReturn);
            strcat(completeCommand, "\n");
            strcat(completeCommand, command);
            write(fclient, completeCommand, BUFFER_SIZE);
            close(fclient);
            fAnswer = open(fileReturn, O_RDONLY);
            read(fAnswer, answer, BUFFER_SIZE);
            printf("%s", answer);
            close(fAnswer);
        }

        //end of program
        free(completeCommand);
        free(command);
        free(filePath);
    }

    return 0;
}