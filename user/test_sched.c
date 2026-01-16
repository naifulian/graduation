#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
print_stats(int pid, int retime, int rutime, int stime)
{
    fprintf(1, "PID: %d, Ready: %d, Running: %d, Sleeping: %d\n", 
           pid, retime, rutime, stime);
}

int
main(int argc, char *argv[])
{
    int pid1, pid2;
    int retime1, rutime1, stime1;
    int retime2, rutime2, stime2;
    
    fprintf(1, "Testing scheduling algorithms...\n");
    
    // Fork first child
    pid1 = fork();
    if (pid1 == 0) {
        // Child 1: do some work and sleep
        fprintf(1, "Child 1 starting\n");
        for (int i = 0; i < 1000000; i++) {
            // Some computation
        }
        sleep(5); // Sleep for 5 ticks
        fprintf(1, "Child 1 exiting\n");
        exit(0);
    }
    
    // Fork second child
    pid2 = fork();
    if (pid2 == 0) {
        // Child 2: do some work and sleep
        fprintf(1, "Child 2 starting\n");
        for (int i = 0; i < 1000000; i++) {
            // Some computation
        }
        sleep(3); // Sleep for 3 ticks
        fprintf(1, "Child 2 exiting\n");
        exit(0);
    }
    
    // Parent waits for children using wait2 to get statistics
    if (pid1 > 0 && pid2 > 0) {
        fprintf(1, "Waiting for first child...\n");
        int result1 = wait2(&retime1, &rutime1, &stime1);
        fprintf(1, "wait2 returned: %d\n", result1);
        if (result1 > 0) {
            print_stats(pid1, retime1, rutime1, stime1);
        }
        
        fprintf(1, "Waiting for second child...\n");
        int result2 = wait2(&retime2, &rutime2, &stime2);
        fprintf(1, "wait2 returned: %d\n", result2);
        if (result2 > 0) {
            print_stats(pid2, retime2, rutime2, stime2);
        }
        
        fprintf(1, "Test completed.\n");
    }
    
    exit(0);
}
