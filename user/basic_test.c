#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    fprintf(1, "Basic test for fork and exit...\n");
    
    int pid = fork();
    if (pid == 0) {
        fprintf(1, "Child process\n");
        exit(0);
    } else {
        fprintf(1, "Parent waiting for child...\n");
        wait(0);
        fprintf(1, "Child exited\n");
    }
    
    fprintf(1, "Test passed!\n");
    exit(0);
}
