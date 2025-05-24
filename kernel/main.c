#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void main()
{
    if (cpuid() == 0) {
        consoleinit();          // 设置控制台设备（如 UART），为后续 printf 提供输出支持
        printfinit();           // 初始化内核日志系统，允许使用 printf 打印调试信息。
        printf("\n");           // 打印内核正在启动的信息
        printf("xv6 kernel is booting\n");
        printf("\n");
        kinit();                // 初始化物理页分配器 Kmem 管理的空闲链表
        kvminit();              // 分配一页物理页作为内核页表并设置内核的物理地址空间到虚拟地址空间的映射
        kvminithart();          // 启用分页
        procinit();             // 初始化进程表
        trapinit();             // 设置陷阱处理程序入口
        trapinithart();         // 为当前 CPU 配置陷阱处理寄存器
        plicinit();             // 初始化 RISC-V 平台级中断控制器（PLIC），管理外设中断
        plicinithart();         // 为当前 CPU 启用 PLIC 中断。
        binit();                // 初始化缓冲区缓存（用于磁盘块读写）。
        iinit();                // 初始化 inode 缓存
        fileinit();             // 初始化文件表（管理打开的文件）
        virtio_disk_init();     // 模拟硬盘
        userinit();             // 创建第一个用户进程
        __sync_synchronize();   // hart0 向其他核心发送同步信号，唤醒其他核心
        started = 1;
    } else {
        while (started == 0)
            ;
        __sync_synchronize();
        printf("hart %d starting\n", cpuid());
        kvminithart();  // turn on paging
        trapinithart(); // install kernel trap vector
        plicinithart(); // ask PLIC for device interrupts
    }

    scheduler();                // 所有 hart 都开始调度程序
}
