#include <stdio.h> 
#include <string.h> 
#include <fcntl.h> 
#include <sys/stat.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    
    if (argc != 2) {
        perror("Wrong number of arguments.\n");
        exit(-1);
    }
    else {
        const char* filePath = argv[1];

        mkfifo(filePath, 0777);
    }

    return 0;
}