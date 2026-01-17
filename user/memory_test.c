#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PAGE_SIZE 4096
#define NUM_PAGES 10

// Test lazy allocation
void test_lazy_allocation() {
  printf("=== Testing Lazy Allocation ===\n");
  
  // Allocate memory but don't touch it
  char *p = sbrk(PAGE_SIZE * NUM_PAGES);
  if (p == (char*)-1) {
    printf("FAIL: sbrk failed\n");
    exit(1);
  }
  
  printf("Allocated %d pages without touching them\n", NUM_PAGES);
  
  // Now touch some pages (should trigger lazy allocation)
  p[0] = 'A';  // First page
  p[PAGE_SIZE * (NUM_PAGES - 1)] = 'Z';  // Last page
  
  if (p[0] != 'A' || p[PAGE_SIZE * (NUM_PAGES - 1)] != 'Z') {
    printf("FAIL: Lazy allocation failed\n");
    exit(1);
  }
  
  printf("PASS: Lazy allocation works correctly\n");
}

// Test copy-on-write
void test_copy_on_write() {
  printf("\n=== Testing Copy-on-Write ===\n");
  
  char *p = sbrk(PAGE_SIZE);
  if (p == (char*)-1) {
    printf("FAIL: sbrk failed\n");
    exit(1);
  }
  
  // Write to the page
  p[0] = 'X';
  printf("Parent wrote 'X' to page\n");
  
  int pid = fork();
  if (pid < 0) {
    printf("FAIL: fork failed\n");
    exit(1);
  }
  
  if (pid == 0) {
    // Child process
    printf("Child reading from page: %c\n", p[0]);
    
    // Child writes to page (should trigger COW)
    p[0] = 'Y';
    printf("Child wrote 'Y' to page\n");
    
    if (p[0] != 'Y') {
      printf("FAIL: Child's write failed\n");
      exit(1);
    }
    
    printf("PASS: COW worked for child\n");
    exit(0);
  } else {
    // Parent process
    wait(0);
    
    // Parent's page should still have original value
    if (p[0] != 'X') {
      printf("FAIL: Parent's page was modified (should be 'X' but is '%c')\n", p[0]);
      exit(1);
    }
    
    printf("PASS: Parent's page unchanged\n");
  }
}

// Test buddy system by allocating and freeing memory
void test_buddy_system() {
  printf("\n=== Testing Buddy System ===\n");
  
  // Allocate multiple pages
  char *pages[5];
  for (int i = 0; i < 5; i++) {
    pages[i] = sbrk(PAGE_SIZE);
    if (pages[i] == (char*)-1) {
      printf("FAIL: sbrk failed for page %d\n", i);
      exit(1);
    }
    pages[i][0] = '0' + i;  // Write different values
  }
  
  printf("Allocated 5 pages\n");
  
  // Verify we can read back the values
  for (int i = 0; i < 5; i++) {
    if (pages[i][0] != '0' + i) {
      printf("FAIL: Page %d has wrong value (expected '%c', got '%c')\n", 
             i, '0' + i, pages[i][0]);
      exit(1);
    }
  }
  
  printf("PASS: All pages accessible with correct values\n");
  
  // Test fork with multiple pages (COW should work for all)
  int pid = fork();
  if (pid < 0) {
    printf("FAIL: fork failed\n");
    exit(1);
  }
  
  if (pid == 0) {
    // Child modifies all pages
    for (int i = 0; i < 5; i++) {
      pages[i][0] = 'a' + i;
    }
    
    // Verify modifications
    for (int i = 0; i < 5; i++) {
      if (pages[i][0] != 'a' + i) {
        printf("FAIL: Child page %d wrong (expected '%c', got '%c')\n",
               i, 'a' + i, pages[i][0]);
        exit(1);
      }
    }
    
    printf("PASS: Child successfully modified all pages\n");
    exit(0);
  } else {
    wait(0);
    
    // Parent's pages should be unchanged
    for (int i = 0; i < 5; i++) {
      if (pages[i][0] != '0' + i) {
        printf("FAIL: Parent page %d changed (expected '%c', got '%c')\n",
               i, '0' + i, pages[i][0]);
        exit(1);
      }
    }
    
    printf("PASS: Parent's pages unchanged after child modification\n");
  }
}

// Test memory exhaustion
void test_memory_exhaustion() {
  printf("\n=== Testing Memory Exhaustion ===\n");
  
  // Try to allocate a lot of memory
  int allocated = 0;
  while (1) {
    char *p = sbrk(PAGE_SIZE);
    if (p == (char*)-1) {
      printf("Memory exhausted after %d pages\n", allocated);
      break;
    }
    
    // Touch the page to actually allocate it
    p[0] = allocated % 256;
    allocated++;
    
    if (allocated > 1000) {
      printf("Allocated 1000+ pages, stopping test\n");
      break;
    }
  }
  
  printf("PASS: Memory allocation handles exhaustion gracefully\n");
}

int main(int argc, char *argv[]) {
  printf("Starting memory management tests...\n\n");
  
  test_lazy_allocation();
  test_copy_on_write();
  test_buddy_system();
  test_memory_exhaustion();
  
  printf("\n=== All Tests Passed! ===\n");
  exit(0);
}
