// Physical memory allocator using Buddy System
// for user processes, kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

// Buddy system constants
#define MAX_ORDER 10  // 2^10 * 4096 = 4MB max block size
#define MIN_ORDER 0   // 2^0 * 4096 = 4KB min block size
#define PAGE_SIZE PGSIZE

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist[MAX_ORDER + 1];  // Free lists for each order
  uint8 refcount[(PHYSTOP - KERNBASE) / PGSIZE]; // Reference count for each physical page
  uint8 order_map[(PHYSTOP - KERNBASE) / PGSIZE]; // Order of each allocated block
} kmem;

// Helper functions for reference counting
static inline int pa2index(void *pa) {
  return ((uint64)pa - KERNBASE) / PGSIZE;
}

static inline void* index2pa(int index) {
  return (void*)(KERNBASE + index * PGSIZE);
}

// Buddy system helper functions
static inline int get_order(int size) {
  int order = 0;
  int block_size = PAGE_SIZE;
  while (block_size < size && order < MAX_ORDER) {
    order++;
    block_size <<= 1;
  }
  return order;
}

static inline void* get_buddy(void *pa, int order) {
  uint64 addr = (uint64)pa;
  uint64 buddy_addr = addr ^ (PAGE_SIZE << order);
  return (void*)buddy_addr;
}

static inline int is_aligned(void *pa, int order) {
  return ((uint64)pa & ((PAGE_SIZE << order) - 1)) == 0;
}

// Increment reference count for a physical page
void incref(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("incref: invalid pa");
  
  int index = pa2index(pa);
  acquire(&kmem.lock);
  if(kmem.refcount[index] < 255)
    kmem.refcount[index]++;
  release(&kmem.lock);
}

// Decrement reference count for a physical page
// Returns 1 if the page should be freed (refcount reached 0)
// Returns 0 if the page is still in use
int decref(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("decref: invalid pa");
  
  int index = pa2index(pa);
  acquire(&kmem.lock);
  if(kmem.refcount[index] == 0) {
    release(&kmem.lock);
    panic("decref: refcount underflow");
  }
  kmem.refcount[index]--;
  int should_free = (kmem.refcount[index] == 0);
  release(&kmem.lock);
  return should_free;
}

// Get reference count for a physical page
int getref(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("getref: invalid pa");
  
  int index = pa2index(pa);
  acquire(&kmem.lock);
  int count = kmem.refcount[index];
  release(&kmem.lock);
  return count;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  // Initialize free lists
  for(int i = 0; i <= MAX_ORDER; i++) {
    kmem.freelist[i] = 0;
  }
  // Initialize reference counts to 0
  for(int i = 0; i < (PHYSTOP - KERNBASE) / PGSIZE; i++) {
    kmem.refcount[i] = 0;
    kmem.order_map[i] = 0;
  }
  freerange(end, (void*)PHYSTOP);
}

// Initialize buddy system with all available memory
void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  
  // Add all pages to the highest possible order
  uint64 total_pages = ((uint64)pa_end - (uint64)p) / PGSIZE;
  
  // Find the maximum order that can fit the memory region
  int max_order = 0;
  uint64 max_block_size = PGSIZE;
  while(max_order < MAX_ORDER && max_block_size * 2 <= total_pages * PGSIZE) {
    max_order++;
    max_block_size <<= 1;
  }
  
  // Add blocks to free list
  uint64 current = (uint64)p;
  while(current + max_block_size <= (uint64)pa_end) {
    struct run *r = (struct run*)current;
    r->next = kmem.freelist[max_order];
    kmem.freelist[max_order] = r;
    current += max_block_size;
  }
  
  // Handle remaining memory with smaller blocks
  while(current + PGSIZE <= (uint64)pa_end) {
    struct run *r = (struct run*)current;
    r->next = kmem.freelist[0];
    kmem.freelist[0] = r;
    current += PGSIZE;
  }
}

// Buddy system allocation
static void* buddy_alloc(int order) {
  // Try to find a block of the requested order
  int current_order = order;
  while(current_order <= MAX_ORDER) {
    if(kmem.freelist[current_order] != 0) {
      // Found a block
      struct run *r = kmem.freelist[current_order];
      kmem.freelist[current_order] = r->next;
      
      // Split the block if it's larger than needed
      while(current_order > order) {
        current_order--;
        // Get buddy address
        void *buddy = get_buddy(r, current_order);
        // Add buddy to free list
        struct run *buddy_run = (struct run*)buddy;
        buddy_run->next = kmem.freelist[current_order];
        kmem.freelist[current_order] = buddy_run;
      }
      
      // Mark the allocated block's order
      int index = pa2index(r);
      kmem.order_map[index] = order;
      return (void*)r;
    }
    current_order++;
  }
  return 0;
}

// Buddy system free
static void buddy_free(void *pa, int order) {
  void *current_pa = pa;
  int current_order = order;
  
  while(current_order < MAX_ORDER) {
    // Get buddy address
    void *buddy = get_buddy(current_pa, current_order);
    
    // Check if buddy is in the free list
    int buddy_free = 0;
    struct run **prev = &kmem.freelist[current_order];
    struct run *curr = kmem.freelist[current_order];
    while(curr) {
      if((void*)curr == buddy) {
        // Remove buddy from free list
        *prev = curr->next;
        buddy_free = 1;
        break;
      }
      prev = &curr->next;
      curr = curr->next;
    }
    
    if(buddy_free) {
      // Merge with buddy
      if(current_pa > buddy) {
        current_pa = buddy;
      }
      current_order++;
    } else {
      break;
    }
  }
  
  // Add the (possibly merged) block to free list
  struct run *r = (struct run*)current_pa;
  r->next = kmem.freelist[current_order];
  kmem.freelist[current_order] = r;
  
  // Clear order map for the freed pages
  int block_size = PAGE_SIZE << current_order;
  for(int i = 0; i < block_size / PAGE_SIZE; i++) {
    int index = pa2index(current_pa) + i;
    kmem.order_map[index] = 0;
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Check reference count
  if(getref(pa) > 0) {
    // Page is still referenced, don't free it
    return;
  }

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  // Get the order of this block
  int index = pa2index(pa);
  int order = kmem.order_map[index];
  
  acquire(&kmem.lock);
  buddy_free(pa, order);
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  void *pa;
  
  acquire(&kmem.lock);
  pa = buddy_alloc(0); // Allocate a single page (order 0)
  release(&kmem.lock);

  if(pa) {
    memset((char*)pa, 5, PGSIZE); // fill with junk
    // Initialize reference count to 1
    int index = pa2index(pa);
    acquire(&kmem.lock);
    kmem.refcount[index] = 1;
    release(&kmem.lock);
  }
  return pa;
}
