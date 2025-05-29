#include "vm.h"

/**
 * @brief 初始化内核页表
 * @note 调用 kvmmake() 创建内核页表并建立内核地址空间的映射关系。
 */
void kvminit(void)
{
    kernel_pagetable = kvmmake();
}

/**
 * @brief 创建并配置内核页表
 * // Make a direct-map page table for the kernel.
 * @return 初始化完成的内核页表指针
 * @note  
 * 1.内核页表采用直接映射的方式映射 KERNBASE-PHYSTOP 的物理内存到内核虚拟地址空间
 * 2.trampoline页被映射到虚拟地址空间的顶端
 * 3.将每个进程的内核栈映射到内核虚拟地址空间的高地址
 */
pagetable_t kvmmake(void)
{
    pagetable_t kpgtbl;

    // 分配一页物理页作为内核页表的 L2 级页表并将这一页物理内存初始化为0(确保刚开始没映射关系?)
    kpgtbl = (pagetable_t)kalloc();
    memset(kpgtbl, 0, PGSIZE);

    /*1.将 0x80000000 地址以下的 IO 设备直接映射到内核的虚拟地址空间*/
    // 将 UART0 的物理地址直接映射到内核虚拟地址空间，设置为可读可写，用于串口设备通信
    kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

    // 将 VIRTIO0 的物理地址直接映射到内核虚拟地址空间，设置为可读可写，用于虚拟块设备访问
    kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

    // 将 PLIC 的物理地址直接映射到内核虚拟地址空间，设置为可读可写
    kvmmap(kpgtbl, PLIC, PLIC, 0x4000000, PTE_R | PTE_W);

    //直映射内核代码段（.text 段），起始地址为 KERNBASE，终止地址为 etext，权限为只读和可执行（防止内核代码段被修改）
    kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

    // 直接映射内核数据段（.data 和 .bss），权限为可读可写（数据段需要修改）
    kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

    // 将 trampoline 页映射到内核虚拟地址空间的高端（TRAMPOLINE 处），用于用户态与内核态之间切换的入口，权限为只读和可执行
    kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

    // 为所有进程预留并映射内核栈空间（每个进程一个栈），用于处理内核态上下文（如系统调用或中断）
    proc_mapstacks(kpgtbl);

    return kpgtbl;
}


/**
 * @brief 设置 SATP 寄存器为内核页表的物理地址并启用分页
 * @note 在 RISCV 上，只要把 satp 寄存器设为合法值就会自动分页
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


/**
* @brief 用软件的方式模拟 MMU 遍历页表，查找或创建虚拟地址(va)的页表项(PTE)
* @note walk 函数正常工作的前提是在进行内核地址空间的映射时，物理内存和
* 虚拟内存之间的映射采用的是直接映射或关闭分页机制
* @param pagetable 顶级页表 L2 的内核虚拟地址，必须已建立内核映射 
* @param va 需要查询的虚拟地址，必须是 39 位有效地址(va < MAXVA)
* @param alloc 页表项无效时是否进行页表分配的标志，0 为不分配，1 为分配
* @return 成功时返回指向 PTE 的内核虚拟地址指针，失败返回 0(当页表项无效且 alloc = 0 或物理内存不足时)
* @example 使用示例 
* 当前进程虚拟地址 va 对应的物理地址pa = PTE2PA(walk(myproc()->pagetable, va, 0))
*/
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc) {
    // 确保虚地址不超过虚拟地址空间的最高有效地址
    if (va >= MAXVA)
        panic("walk");

    // 模拟三级页表的访问过程
    for (int level = 2; level > 0; level--) {
        // PX(level, va) 得到 level 级页表的页表索引
        // 根据页表索引在页表中的偏移量得到页表项
        pte_t *pte = &pagetable[PX(level, va)];
        
        // 根据 PTE_V 标志判断页表项是否有效
        if (*pte & PTE_V) {     
            // 页表项有效，则从页表项中提取下一级页表的物理地址 PA
            // 将 PA 强转为 pagetable_t 指针(虚拟地址)
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {                
            // 页表项无效，说明对应的页表没有分配
            // 标记位 alloc = 1 且物理内存足够时申请新的页表
            if (!alloc || (pagetable = (pde_t *)kalloc()) == 0)
                // alloc = 0 或 kalloc 分配失败时返回 0，表示查找失败
                return 0;
            
            // 将新分配的页表初始化为 0
            memset(pagetable, 0, PGSIZE);
            // 将新分配的物理地址存放在上一级页表的页表项中，设置有效页表标志位
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    // 返回最后一级页表的页表项
    return &pagetable[PX(0, va)];
}


/**
 * @brief 查找用户页表中的虚拟地址 va 所映射对应的物理地址，虚拟地址未被映射时返回 0
 * @param pagetable 用户页表
 * @param va 要查找的虚拟地址
 * @return 成功时返回虚拟地址对应的的物理地址，失败返回 0
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


/**
 * @brief 向内核页表中添加地址映射，仅在内核启动时使用，不刷新 TLB 或启用分页
 * 
 * @param kpgtbl    内核页表指针
 * @param va        要映射的虚拟地址，必须按页对齐
 * @param pa        对应的物理地址，必须按页对齐
 * @param sz        映射区域的总大小，必须按页对齐
 * @param perm      权限标志
 */
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
    if (mappages(kpgtbl, va, sz, pa, perm) != 0)
        panic("kvmmap");
}


/** 
* @brief 在页表中创建页表项，建立虚拟地址 [va, va+size) 到物理地址 [pa, pa+size)的映射
* @note va 和 size 必须是按页对齐的
* @param pagetable 要映射的页表
* @param va 待映射的起始虚拟地址，必须按页对齐
* @param size 映射区域总大小，必须按页对齐
* @param pa 起始物理地址，必须按页对齐
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


/**
 * @brief 取消从虚拟地址 va 开始的 npages 个页面的映射关系
 * @note va 必须是页对齐的，可以选择是否释放虚拟地址对应的物理内存
 * @param pagetable 要操作的页表的指针
 * @param va 需要取消映射的起始虚拟地址
 * @param npages 要取消映射的页数
 * @param do_free 标志位，决定是否释放对应的物理内存，1 = 释放，0 = 不释放
 */
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
    uint64 a;
    pte_t *pte;

    // 确保虚拟地址 va 页对齐的
    if ((va % PGSIZE) != 0)
        panic("uvmunmap: not aligned");
    
    for (a = va; a < va + npages * PGSIZE; a += PGSIZE) {
        // 使用 walk 函数查找虚拟地址 va 对应的页表项，返回 0 表示页表项缺失
        if ((pte = walk(pagetable, a, 0)) == 0)
            panic("uvmunmap: walk");
        // 检查页表项是否有效，PTE_V 为 0 表示该页表项未被映射
        if ((*pte & PTE_V) == 0)
            panic("uvmunmap: not mapped");
        
        // 检查页表项是否是叶页表(是否映射到物理页)
        if (PTE_FLAGS(*pte) == PTE_V)
            panic("uvmunmap: not a leaf");
        // 如果需要释放物理内存
        if (do_free) {
            // 从页表项中提取物理地址
            uint64 pa = PTE2PA(*pte);
            // 调用 kfree 释放物理内存
            kfree((void *)pa);
        }
        // 将页表项清零，取消映射
        *pte = 0;
    }
}


/**
 * @brief 创建一个空的用户页表
 * @return 成功则返回创建的页表指针，内存不足而失败时返回 0
 */
pagetable_t uvmcreate()
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


/**
 * @brief 将第一个用户进程 initcode 加载到页表的 0 地址处
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

/**
 * @brief 分配页表项和物理内存，将进程的内存空间从 oldsz 扩展大 newsz
 * @note newsz 无序按页对齐
 * @return 成功返回新大小，失败返回 0
 */
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
    char *mem;
    uint64 a;

    if (newsz < oldsz)
        return oldsz;

    oldsz = PGROUNDUP(oldsz);
    for (a = oldsz; a < newsz; a += PGSIZE) {
        mem = kalloc();
        if (mem == 0) {
            uvmdealloc(pagetable, a, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R | PTE_U | xperm) != 0) {
            kfree(mem);
            uvmdealloc(pagetable, a, oldsz);
            return 0;
        }
    }
    return newsz;
}


/**
 * @brief 释放用户页，将进程内存大小从 oldsz 收缩到 newsz
 * @param pagetable 需要调整的页表
 * @param oldsz 当前的内存大小
 * @param newsz 进程新的内存大小
 * @return 调整后的进程内存大小
 * @note
 * 1. oldsz 和 newsz 不需要页对齐
 * 2. newsz 不一定要小于 oldsz
 * 3. oldsz 可以大于实际进程的内存大小
 */
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
    // 如果不需要收缩内存，则直接返回当前大小
    if (newsz >= oldsz)
        return oldsz;

    // 对 newsz 和 oldsz 进行页对齐，在 newsz < oldzs 的情况下才操作
    if (PGROUNDUP(newsz) < PGROUNDUP(oldsz)) {
        int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
        uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
    }

    return newsz;
}


/**
 * @brief 递归的释放页表所占用的物理页
 * @note 必须先解除 L0 页表项的映射关系
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

/**
 * @brief 取消用户页表映射关系，然后释放用户页表的物理内存，
 */
void uvmfree(pagetable_t pagetable, uint64 sz)
{
    if (sz > 0)
        uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);
    freewalk(pagetable);
}

/**
 * @brief 复制父进程页表及物理内u你到子进程页表
 * @
 * @return 成功返回 0 失败返回 -1
 */
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
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
