#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  // Test lazy allocation
  char *p = sbrk(4096);  // Allocate one page
  if (p == (char*)-1) {
    printf("sbrk failed\n");
    exit(1);
  }
  
  // Write to the allocated memory (should trigger page fault)
  p[0] = 'A';
  p[4095] = 'Z';
  
  // Read back
  if (p[0] != 'A' || p[4095] != 'Z') {
    printf("lazy allocation test failed\n");
    exit(1);
  }
  
  printf("lazy allocation test passed\n");
  exit(0);
}
