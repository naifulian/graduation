// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
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

/**
 * @brief 空闲的物理内存，一页大小(4096 Byte)
 * next 存放下一个空闲物理页的地址
 */
struct run {
    struct run *next;
};

/**
 * @brief 管理空闲链表
 */
struct {
    struct spinlock lock; // 保护空闲链表的自旋锁
    struct run *freelist; // 空闲页组成的链表的头指针
} kmem;

/**
 * @brief  xv6 管理的物理内存都加入空闲链表
 * @note xv6 管理的物理内存为 [end, PHYSTOP]
 */
void kinit()
{
    initlock(&kmem.lock, "kmem");
    // 初始化从 end 到 PHYSTOP(KERNBASE + 128KB) 的内存
    // 遍历这块内存，对每个 4KB 大小的内存使用 kfree 函数
    freerange(end, (void *)PHYSTOP);
}

/**
 * @brief 将物理内存 pa_start 到 pa_end 范围内的所有内存页都加入空闲链表
 * @param pa_start 起始的物理内存地址
 * @param pa_end 结束的物理内存地址
 * @note pa_start 会被向上对齐到 PGSIZE 的最近整数倍
 */
void freerange(void *pa_start, void *pa_end)
{
    char *p;
    // ​​将开始的物理地址向上对齐到页边界，原因是
    // MMU 要求物理内存以页为单位管理，在 kfree
    // 中会检查 p 是否是页对齐的
    p = (char *)PGROUNDUP((uint64)pa_start);
    for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
        kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)

/**
 * @brief 释放由 pa 指向的物理内存页(被添加到空闲链表)
 * @param pa 要被释放的物理内存页，必须是页对齐的
 * @note 被释放的物理内存页会被填满垃圾值 1
 */
void kfree(void *pa)
{
    struct run *r;

    // pa 不是页对齐的或者 pa 不在 xv6 管理的物理内存范围[end, PHYSTOP)内
    if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
        panic("kfree");

    // 用垃圾数据填充页面，防止悬垂引用
    memset(pa, 1, PGSIZE);

    r = (struct run *)pa;

    // 将 pa 指向的物理页加入空闲链表(头插法)
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
/**
 * @brief 从空闲链表 kmem.freelist 中取出第一个空闲页，
 * 然后将这个页面使用垃圾值 5 填满
 * 
 */
void *
kalloc(void)
{
    struct run *r;

    acquire(&kmem.lock);
    r = kmem.freelist;
    if (r)
        kmem.freelist = r->next;
    release(&kmem.lock);

    if (r)
        memset((char *)r, 5, PGSIZE); // fill with junk
    return (void *)r;
}
