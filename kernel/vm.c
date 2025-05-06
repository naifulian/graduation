#include "types.h"
#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[]; // trampoline.S

// Make a direct-map page table for the kernel.

/**
 * @brief 创建并配置内核页表的映射
 * 
 * @note 直接映射的意义，物理地址 == 虚拟地址
 * 1. 简化早期的启动：在内核启用分页机制前，地址访问的都是物理地址
 * 2. 方便内存管理：kalloc 返回的是物理地址，方便内核操作
 * 3. etext 和 trampoline pagetable 在这里是虚拟地址，但同样可以
 * 当作物理地址使用
 * 
 * @param void
 * 
 * @return 初始化完成的内核页表指针
 * 
 * @details
 * 此函数用于创建页表，并完成下面映射
 * 1. 硬件设备寄存器（UART、VIRTIO、PLIC）
 * 2. 内核代码段（只读可执行）、数据段（可读写）
 * 3. 物理内存的等量映射（direct-map）
 * 4. 跳板页（Trampoline）用于陷入处理
 * 5. 每个进程的内核栈空间
 */
pagetable_t
kvmmake(void)
{
    pagetable_t kpgtbl;

    // 分配一个物理页作为内核页表的 L2 级页表
    kpgtbl = (pagetable_t)kalloc();
    // 清空页表，确保初始无映射
    memset(kpgtbl, 0, PGSIZE);

    // 映射 UART 设备寄存器（串口通信）
    // 虚拟地址 UART0 直接映射到物理地址 UART0，大小为 PGSIZE，权限为可读可写
    kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

    // 映射 VIRTIO 磁盘设备寄存器
    // 虚拟地址 VIRTIO0 直接映射到物理地址 VIRTIO0，权限可读可写
    kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

    // 映射 PLIC（Platform-Level Interrupt Controller）中断控制器
    // PLIC 的 MMIO 区域大小为 0x4000000（64MB），权限可读可写
    kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

    // 映射内核代码段（.text）：从 KERNBASE 到 etext
    // 权限为可读、可执行（禁止写入保护代码）
    kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

    // 映射内核数据段（.data、.bss）及可用物理内存（up to PHYSTOP）
    // 权限为可读可写（数据段需要修改）
    kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

    // 映射跳板页（Trampoline）到内核地址空间顶端
    // 虚拟地址 TRAMPOLINE 映射到物理地址 trampoline 的代码页，权限可读、可执行
    kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

    // 为每个进程分配并映射内核栈（位于内核地址空间的高地址区域）
    proc_mapstacks(kpgtbl);

    return kpgtbl;
}

/**
 * @brief 对 kvmmake 的封装，kvmmake 创建页表并配置内核页表的映射
 */
void kvminit(void)
{
    kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
/**
 * @brief 启用分页机制并切换至内核页表，刷新 TLB 缓存
 * @note 
 * 内存屏障问题
 * 启用分页之后，在内核态除了内核栈和 trampoline 页面
 * 大部分页面执行的还是直接映射，物理地址还是等于虚拟地址
 * @param  void
 * @return void
 */
void kvminithart()
{   
    // 封装了内联汇编指令 sfence.vma，该指令用于同步虚拟内存访问
    // 它会确保在此屏障之前的所有页表更新对所有后续指令可见
    sfence_vma();

    // 设置 satp 寄存器，该操作会启用分页机制，使用指定的内核页表
    // 进行地址转换
    w_satp(MAKE_SATP(kernel_pagetable));

    // 刷新 TLB (Translation Lookaside Buffer) 
    // 在切换页表后，必须确保旧的 TLB 条目被清除，以避免使用过时的地址转换
    sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.

/**
* @brief 用软件的方式模拟 MMU 遍历页表，查找或创建虚拟地址(va)对应的页表项(PTE)
* - L2：顶级页表，使用虚拟地址高 9 位(30-38)索引
* - L1：中间页表：使用虚拟地址中间 9 位(21-29)索引
* - L0：叶页表，使用虚拟地址低 9 位(12-20)索引
* 
* @note walk 函数正常工作的前提是在进行内核地址空间的映射时，物理内存和
        虚拟内存之间采用的是直接映射的方法
*
* walk 函数遍历 Sv39 的三级页表结构，查找给定的虚拟地址对应的页表项。
* 如果中间层页表不存在且允许分配，也会动态创建所需的页表
*
* @param pagetable 顶级页表 L2 的内核虚拟地址，必须已建立内核映射 
* @param va 需要查询的虚拟地址，必须是 39 位有效地址(va < MAXVA)
* @param alloc 是否进行页表分配的标志：
*                           - 1 = 自动分配缺失页表
*                           - 0 = 遇到缺失页表返回空
* @return 成功时返回指向 PTE 的内核虚拟地址指针，
          失败返回 0，当 alloc=0 时页表缺失或 alloc=1 时内存不足
*/
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
    // 确保虚地址不超过 Sv39 架构定义的 39 位地址空间
    if (va >= MAXVA)
        panic("walk");

    // 逐级处理 L2 和 L1 页表，最后单独处理 L0 页表
    for (int level = 2; level > 0; level--) {
        // 根据当前层级和虚地址计算页表项索引
        pte_t *pte = &pagetable[PX(level, va)];
        
        // 根据 PTE_V 标志判断页表项是否有效
        if (*pte & PTE_V) {     
            // 有效，提取物理页号(PPN)并转换为内核虚拟地址
            // PPN 是物理地址，将其赋值给 pagetable 只有在
            // 虚拟地址和物理地址相等时才能使用
            // 即采用直接映射或关闭分页机制
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {                
            // 当前 PTE 地址无效，说明对应的页表没有分配
            // 根据 alloc 标记位决定是否申请新的页表
            // alloc 为不分配或 kalloc 分配失败时返回空指针
            if (!alloc || (pagetable = (pde_t *)kalloc()) == 0)
                return 0;
            
            // 将新页表页初始化为 0，以避免因为残留数据被误判为有效 PTE
            memset(pagetable, 0, PGSIZE);
            // 将新页表的物理地址写入到当前 PTE，并设置有效标志
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    // 处理 L0 级页表：返回最终页表项指针，此时不检查 PTE_V 标志
    // 调用者需自行设置 *pte 的内容（如 PTE_V | PTE_U | PTE_W 等）
    return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
/**
 * @brief walkaddr函数是walk函数的一层封装，专门
 * 用来查找用户页表中特定虚拟地址 va 所对应的物理地址
 * @note
 * 1. walkaddr 函数只用来查找用户页表
 * 2. 返回的是物理地址，而不是像 walk 一样返回最终的 PTE 
 * @param pagetable 用户页表
 * @param va 要查找的虚拟地址
 * @return 成功返回对应的物理地址，失败返回 0
 */
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
    pte_t *pte;
    uint64 pa;
    // 检查虚拟地址是否超出最大允许值
    if (va >= MAXVA)
        return 0;
    // 使用 walk 函数查找页表项
    pte = walk(pagetable, va, 0);
    // 页表项不存在，返回 0
    if (pte == 0)
        return 0;
    // 检查页表项是否有效
    if ((*pte & PTE_V) == 0)
        return 0;
    // 检查页表项用户是否可访问
    if ((*pte & PTE_U) == 0)
        return 0;
    // 从页表项中提取物理地址
    pa = PTE2PA(*pte);
    return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
/**
 * @brief 此函数是对 mappages 的封装，专门用于内核初始化阶段的内存映射
 * 
 * @param kpgtbl    内核页表指针
 * @param va        要映射的虚拟地址，必须按页对齐
 * @param pa        对应的物理地址，必须按页对齐
 * @param sz        映射区域的总大小，必须按页对齐
 * @param perm      权限标志
 */
void
kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa.
// va and size MUST be page-aligned.
// Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.

/** 
* @brief 将虚拟地址 [va, va+size) 映射到物理地址 [pa, pa+size) 并设置权限 perm
*
* @param pagetable 目标的 L2 级页表的内核虚拟地址
* @param va 待映射的起始虚拟地址，必须按页对齐
* @param size 映射区域总大小，必须按页对齐
* @param pa 起始物理地址，必须页对齐
* @param perm 权限标志

* @return 成功返回 0，失败返回 -1
*/
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
    uint64 a, last;
    pte_t *pte;

    // 检查虚拟地址 va 是否按页对齐（PGSIZE 的倍数）
    if ((va % PGSIZE) != 0)
        panic("mappages: va not aligned");

    // 检查 size 是否按页对齐
    if ((size % PGSIZE) != 0)
        panic("mappages: size not aligned");

    // 不允许映射长度为 0 的空间
    if (size == 0)
        panic("mappages: size");

    a = va;
    // 计算最后一个需要映射的页的起始地址
    last = va + size - PGSIZE;
    for (;;) {
        // 使用 walk 函数查找 PTE
        if ((pte = walk(pagetable, a, 1)) == 0)
            return -1;
        // 检查该 PTE 是否已被映射，避免重复映射同一虚拟页
        if (*pte & PTE_V)
            panic("mappages: remap");
        // 将物理地址 pa 转换为 PTE 格式，并设置权限和有效标志
        *pte = PA2PTE(pa) | perm | PTE_V;

        // 最后一页则退出循环
        if (a == last)
            break;
        // 移动到下一页：虚拟地址和物理地址各增加一页大小
        a += PGSIZE;
        pa += PGSIZE;
    }
    return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
/**
 * @brief
 * @param pagetable
 * @param va
 * @param npages
 * @param do_free
 */
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
/**
 * @brief 创建一个新的用户页表
 * @return 成功返回创建的页表指针，内存不足而失败时返回 0
 */
pagetable_t
uvmcreate()
{
    pagetable_t pagetable;
    // 分配一个物理页作为页表，kalloc() 返回的是物理地址，
    // 直接转换为页表指针类型
    pagetable = (pagetable_t)kalloc();
    
    // 内存不足
    if (pagetable == 0)
        return 0;

    // 初始化页表为 0，清零操作将所有PTE的V位设为0，表示无效
    memset(pagetable, 0, PGSIZE);
    return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
/**
 * @brief 将第一个用户进程 initcode 加载到页表的 0 地址
 * @param pagetable 目标用户页表
 * @param src 源数据地址(initcode)
 * @param sz 拷贝的字节数
 * @note 仅被第一个进程 initcode 使用，代码大小必须小于一页 
 */
void uvmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
    char *mem;

    // 如果要求分配的大小大于一个页面，则陷入panic
    if (sz >= PGSIZE)
        panic("uvmfirst: more than a page");
    
    // 分配一页物理内存作为 initcode 的存放处，memset 用来将当前页清空
    mem = kalloc();
    memset(mem, 0, PGSIZE);

    // 在页表中加入一条虚拟地址0 到物理地址 mem的映射，相当于将 initcode 映射到虚拟地址 0 
    mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W | PTE_R | PTE_X | PTE_U);
    // 拷贝 initcode 字节码到分配的物理页 mem 处
    memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
/**
 * @brief 递归释放整个页表树及占用的物理内存
 * @note 所有叶映射必须已经被解除了
 * @param pagetable 要释放的页表指针
 */
void freewalk(pagetable_t pagetable)
{
    // RISC-V 的一个页表有 2^9 = 512 个页表项(PTE)
    for (int i = 0; i < 512; i++) {
        // 获取当前的页表项
        pte_t pte = pagetable[i];

        // 如果有效位为 1，且读位、写位、可执行位都是0
        // 说明这是一个非叶级页表项，且此项未被释放，应该去递归地释放
        if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0) {
            // 获取下级页表的物理地址
            uint64 child = PTE2PA(pte);
            // 递归释放下级页表
            freewalk((pagetable_t)child);
            // 清空当前页表项
            pagetable[i] = 0;
        } else if (pte & PTE_V) {
            // 有效的页表项且映射到物理页面(L0 级页表)
            // 这表示页表正在使用中，不应该被释放
            panic("freewalk: leaf");
        }
        // PTE_V == 0，表示当前 PTE 已经被释放过了，不用再释放
    }
    // 释放当前页表占用的物理内存
    kfree((void *)pagetable);
}

// Free user memory pages,
// then free page-table pages.
void
uvmfree(pagetable_t pagetable, uint64 sz)
{
  if(sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char*)pa, PGSIZE);
    if(mappages(new, i, PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(va0 >= MAXVA)
      return -1;
    pte = walk(pagetable, va0, 0);
    if(pte == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_U) == 0 ||
       (*pte & PTE_W) == 0)
      return -1;
    pa0 = PTE2PA(*pte);
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}
