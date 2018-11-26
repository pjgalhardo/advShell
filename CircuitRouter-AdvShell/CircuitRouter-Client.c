#include <stdio.h> 
#include <string.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <stdlib.h>

#define MAXFILEPATH 256

int main(int argc, char* argv[]) {
    
    int fclient;
    
    if (argc != 2) {
        perror("Wrong number of arguments.\n");
        exit(-1);
    }
    else {
        char* filePath = (char*)malloc(MAXFILEPATH);
        strcpy(filePath, argv[1]);
        strcat(filePath, ".pipe");
        
        mkfifo(filePath, 0777);

        char* command = (char*)malloc(MAXFILEPATH);
        

        while (1) {
            fclient = open(filePath, O_WRONLY);
            fgets(command, MAXFILEPATH, stdin);
            
            write(fclient, command, strlen(command) + 1);
            close(fclient);


        }

        
        //end of program
        free(command);
        free(filePath);
    }
    
    return 0;
}