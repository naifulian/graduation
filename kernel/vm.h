#ifndef __VM__
#define __VM__

#include "types.h"
#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"

// 内核页表
pagetable_t kernel_pagetable;

// 链接器脚本 kernel.ld 定义符号 etext ，指向内核 .text 段的结尾
extern char etext[];  

// trampoline.S 中定义的全局符号
extern char trampoline[]; 

/* 内核虚拟内存初始化相关操作 (Kernel VM Initialization) */
void kvminit(void);
pagetable_t kvmmake(void);
void kvminithart(void);

/* 页表核心操作 (Page Table Core Operations) */
pte_t* walk(pagetable_t pagetable, uint64 va, int alloc);
uint64 walkaddr(pagetable_t pagetable, uint64 va);
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free);

/* 内存映射操作 (Memory Mapping) */
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm);


/* 用户地址空间管理 (User Address Space) */
pagetable_t uvmcreate();
void uvmfirst(pagetable_t pagetable, uchar *src, uint sz);
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm);
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz);
void uvmfree(pagetable_t pagetable, uint64 sz);
int uvmcopy(pagetable_t old_pagetable, pagetable_t new_pagetable, uint64 sz);
void uvmclear(pagetable_t pagetable, uint64 va);
void freewalk(pagetable_t pagetable);

/* 内核空间和用户空间之间的数据拷贝 (Cross-space Data Transfer) */
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len);
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len);
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max);
#endif
