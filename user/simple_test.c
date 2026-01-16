#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    int pid;
    int retime, rutime, stime;
    
    fprintf(1, "Simple test for wait2 system call...\n");
    
    pid = fork();
    if (pid == 0) {
        // Child process
        fprintf(1, "Child process started\n");
        sleep(10); // Sleep for a bit
        fprintf(1, "Child process exiting\n");
        exit(0);
    } else {
        // Parent process
        fprintf(1, "Parent waiting for child...\n");
        int result = wait2(&retime, &rutime, &stime);
        fprintf(1, "wait2 returned: %d\n", result);
        fprintf(1, "Statistics: ready=%d, running=%d, sleeping=%d\n", 
                retime, rutime, stime);
    }
    
    exit(0);
}
