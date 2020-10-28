#include "types.h"
#include "user.h"
#include "stat.h"

int main(int argc, char *argv[]) {
    int wtime, rtime;
    int pid=fork();
    if(pid == -1) {
        printf(1, "Failed to fork!\n");
        exit();
    }
    if(pid == 0) {
        if(argc > 1) {
            printf(1,"Starting to time %s\n", argv[1]);
            if(exec(argv[1], argv + 1) == -1) {
                printf(1, "Command not found!\n");
                exit();
            }
        }
        else {
            printf(1, "Running test program\n");
            for(long long int i=0; i < 1000000; i++) {
                i = i*1;
            }
            exit();
        }
    }
    else if (pid > 0) {
        int status = waitx(&wtime, &rtime);
        printf(1, "Time taken by the program\n Wait Time - %d\n Run Time - %d\n Status - %d\n\n", wtime, rtime, status);
        exit();
    }
}
