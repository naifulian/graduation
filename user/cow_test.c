#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE 4096

int
main(int argc, char *argv[])
{
  int pid;
  char *p;
  
  printf("COW test starting...\n");
  
  // Allocate a page
  p = sbrk(PAGE_SIZE);
  if (p == (char*)-1) {
    printf("sbrk failed\n");
    exit(1);
  }
  
  // Write to the page
  p[0] = 'A';
  p[PAGE_SIZE - 1] = 'Z';
  
  printf("Parent: wrote to page, p[0]=%c, p[%d]=%c\n", p[0], PAGE_SIZE-1, p[PAGE_SIZE-1]);
  
  // Fork a child
  pid = fork();
  if (pid < 0) {
    printf("fork failed\n");
    exit(1);
  }
  
  if (pid == 0) {
    // Child process
    printf("Child: reading from page, p[0]=%c, p[%d]=%c\n", p[0], PAGE_SIZE-1, p[PAGE_SIZE-1]);
    
    // Child writes to the page (should trigger COW)
    p[0] = 'B';
    printf("Child: wrote to page, p[0]=%c\n", p[0]);
    
    // Check that parent's page is unchanged
    if (p[0] != 'B') {
      printf("Child: ERROR: p[0] should be 'B' but is '%c'\n", p[0]);
      exit(1);
    }
    
    printf("Child: COW test passed\n");
    exit(0);
  } else {
    // Parent process
    wait(0);
    
    // Check that parent's page is unchanged
    if (p[0] != 'A') {
      printf("Parent: ERROR: p[0] should be 'A' but is '%c'\n", p[0]);
      exit(1);
    }
    
    printf("Parent: COW test passed\n");
  }
  
  // Test lazy allocation
  printf("\nLazy allocation test...\n");
  
  // Allocate more memory but don't touch it
  char *q = sbrk(PAGE_SIZE * 10);
  if (q == (char*)-1) {
    printf("sbrk failed\n");
    exit(1);
  }
  
  // Write to the last page (should trigger lazy allocation)
  q[PAGE_SIZE * 9] = 'X';
  if (q[PAGE_SIZE * 9] != 'X') {
    printf("Lazy allocation test failed\n");
    exit(1);
  }
  
  printf("Lazy allocation test passed\n");
  
  printf("All tests passed!\n");
  exit(0);
}
