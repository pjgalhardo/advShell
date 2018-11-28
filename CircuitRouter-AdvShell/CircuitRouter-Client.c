#include <stdio.h> 
#include <string.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <stdlib.h>

#define BUFFER_SIZE 100

int main(int argc, char* argv[]) {
    
    int fclient;
    if (argc != 2) {
        perror("Wrong number of arguments.\n");
        exit(-1);
    }
    else {
        char* filePath = (char*)malloc(sizeof(char) * BUFFER_SIZE);
        strcpy(filePath, argv[1]);
        strcat(filePath, ".pipe");

        char* command = (char*)malloc(sizeof(char) * BUFFER_SIZE);
        while (1) {
            fclient = open(filePath, O_WRONLY);
            if (fclient == -1){
                perror("FAILED CREATING FIFO.\n");
            }
            fgets(command, BUFFER_SIZE, stdin);
            write(fclient, command, BUFFER_SIZE);
            close(fclient);


        }

        
        //end of program
        free(command);
        free(filePath);
    }
    
    return 0;
}