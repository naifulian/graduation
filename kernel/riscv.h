#ifndef __ASSEMBLER__

/*----------------------------------------------------------
 * 处理器核心相关寄存器操作
 *---------------------------------------------------------*/
// 获取当前硬件线程(hart)ID
/* 读取机器模式硬件线程ID寄存器 */
static inline uint64
r_mhartid()
{
    uint64 x;
    asm volatile("csrr %0, mhartid" : "=r"(x));
    return x;
}

/*----------------------------------------------------------
 * 机器模式状态寄存器(mstatus)操作
 *---------------------------------------------------------*/

/* mstatus寄存器位定义 */
#define MSTATUS_MPP_MASK (3L << 11) // 前一个特权模式掩码
#define MSTATUS_MPP_M (3L << 11)    // 前一个模式是机器模式
#define MSTATUS_MPP_S (1L << 11)    // 前一个模式是监管模式  
#define MSTATUS_MPP_U (0L << 11)    // 前一个模式是用户模式
#define MSTATUS_MIE (1L << 3)       // 机器模式中断使能位

/* 读取机器模式状态寄存器 */
static inline uint64
r_mstatus()
{
    uint64 x;
    asm volatile("csrr %0, mstatus" : "=r"(x));
    return x;
}

/* 写入机器模式状态寄存器 */
static inline void 
w_mstatus(uint64 x)
{
  asm volatile("csrw mstatus, %0" : : "r" (x));
}

/*----------------------------------------------------------
 * 异常处理相关寄存器
 *---------------------------------------------------------*/
/* 机器模式异常程序计数器(mepc) - 保存异常返回地址 */
static inline void 
w_mepc(uint64 x)
{
  asm volatile("csrw mepc, %0" : : "r" (x));
}

/*----------------------------------------------------------
 * 监管模式状态寄存器(sstatus)操作
 *---------------------------------------------------------*/

/* sstatus寄存器位定义 */
#define SSTATUS_SPP (1L << 8)  // 前一个模式(1=监管模式, 0=用户模式)
#define SSTATUS_SPIE (1L << 5) // 监管模式前一个中断使能状态
#define SSTATUS_UPIE (1L << 4) // 用户模式前一个中断使能状态  
#define SSTATUS_SIE (1L << 1)  // 监管模式中断使能
#define SSTATUS_UIE (1L << 0)  // 用户模式中断使能

/* 读取监管模式状态寄存器(sstatus) - 包含特权模式、中断使能等状态信息 */
static inline uint64
r_sstatus()
{
  uint64 x;
  asm volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

/* 写入监管模式状态寄存器 - 用于设置特权模式、中断使能等状态 */
static inline void 
w_sstatus(uint64 x)
{
  asm volatile("csrw sstatus, %0" : : "r" (x));
}

/*----------------------------------------------------------
 * 中断处理相关寄存器
 *---------------------------------------------------------*/
/* 监管模式中断待处理寄存器(sip) */
static inline uint64
r_sip()
{
  uint64 x;
  asm volatile("csrr %0, sip" : "=r" (x) );
  return x;
}

/* 写入监管模式中断待处理寄存器 */
static inline void 
w_sip(uint64 x)
{
  asm volatile("csrw sip, %0" : : "r" (x));
}

/*----------------------------------------------------------
 * 中断使能寄存器
 *---------------------------------------------------------*/
/* 监管模式中断使能寄存器(sie)位定义 */
#define SIE_SEIE (1L << 9) // 外部中断使能
#define SIE_STIE (1L << 5) // 定时器中断使能  
#define SIE_SSIE (1L << 1) // 软件中断使能
/* 读取监管模式中断使能寄存器 */
static inline uint64
r_sie()
{
  uint64 x;
  asm volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

/* 写入监管模式中断使能寄存器 */
static inline void 
w_sie(uint64 x)
{
  asm volatile("csrw sie, %0" : : "r" (x));
}

/*----------------------------------------------------------
 * 机器模式中断使能
 *---------------------------------------------------------*/
#define MIE_STIE (1L << 5)  // 监管模式定时器中断使能
/* 读取机器模式中断使能寄存器 */
static inline uint64
r_mie()
{
  uint64 x;
  asm volatile("csrr %0, mie" : "=r" (x) );
  return x;
}

/* 写入机器模式中断使能寄存器 */
static inline void 
w_mie(uint64 x)
{
  asm volatile("csrw mie, %0" : : "r" (x));
}

/*----------------------------------------------------------
 * 异常处理相关寄存器
 *---------------------------------------------------------*/
/* 监管模式异常程序计数器(sepc) - 保存异常返回地址 */
static inline void 
w_sepc(uint64 x)
{
  asm volatile("csrw sepc, %0" : : "r" (x));
}

/* 读取监管模式异常程序计数器(sepc) - 获取异常返回地址 */
static inline uint64
r_sepc()
{
  uint64 x;
  asm volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}

/*----------------------------------------------------------
 * 异常和中断委托寄存器
 *---------------------------------------------------------*/
/* 机器模式异常委托寄存器(medeleg) */
static inline uint64
r_medeleg()
{
  uint64 x;
  asm volatile("csrr %0, medeleg" : "=r" (x) );
  return x;
}

/* 写入机器模式异常委托寄存器 */
static inline void 
w_medeleg(uint64 x)
{
  asm volatile("csrw medeleg, %0" : : "r" (x));
}

/* 机器模式中断委托寄存器(mideleg) */
static inline uint64
r_mideleg()
{
  uint64 x;
  asm volatile("csrr %0, mideleg" : "=r" (x) );
  return x;
}

/* 写入机器模式中断委托寄存器 */
static inline void 
w_mideleg(uint64 x)
{
  asm volatile("csrw mideleg, %0" : : "r" (x));
}

/*----------------------------------------------------------
 * 陷阱处理相关寄存器  
 *---------------------------------------------------------*/
/* 监管模式陷阱向量基地址寄存器(stvec) - 低2位表示模式 */
static inline void 
w_stvec(uint64 x)
{
  asm volatile("csrw stvec, %0" : : "r" (x));
}

/* 读取监管模式陷阱向量基地址寄存器 */
static inline uint64
r_stvec()
{
  uint64 x;
  asm volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}

/*----------------------------------------------------------
 * 定时器相关寄存器
 *---------------------------------------------------------*/
/* 监管模式定时器比较寄存器(stimecmp) */
/* 读取监管模式定时器比较寄存器(stimecmp) - 通过CSR地址0x14d访问 */
static inline uint64
r_stimecmp()
{
  uint64 x;
  // asm volatile("csrr %0, stimecmp" : "=r" (x) );
  asm volatile("csrr %0, 0x14d" : "=r" (x) );
  return x;
}

/* 写入监管模式定时器比较寄存器 - 通过CSR地址0x14d访问 */
static inline void 
w_stimecmp(uint64 x)
{
  // asm volatile("csrw stimecmp, %0" : : "r" (x));
  asm volatile("csrw 0x14d, %0" : : "r" (x));
}

/*----------------------------------------------------------
 * 环境配置寄存器
 *---------------------------------------------------------*/
/* 机器模式环境配置寄存器(menvcfg) */
static inline uint64
r_menvcfg()
{
  uint64 x;
  // asm volatile("csrr %0, menvcfg" : "=r" (x) );
  asm volatile("csrr %0, 0x30a" : "=r" (x) );
  return x;
}

/* 写入机器模式环境配置寄存器 */
static inline void 
w_menvcfg(uint64 x)
{
  // asm volatile("csrw menvcfg, %0" : : "r" (x));
  asm volatile("csrw 0x30a, %0" : : "r" (x));
}

/*----------------------------------------------------------
 * 物理内存保护(PMP)相关寄存器
 *---------------------------------------------------------*/
/* 写入PMP配置寄存器0 */
static inline void
w_pmpcfg0(uint64 x)
{
  asm volatile("csrw pmpcfg0, %0" : : "r" (x));
}

/* 写入PMP地址寄存器0 */ 
static inline void
w_pmpaddr0(uint64 x)
{
  asm volatile("csrw pmpaddr0, %0" : : "r" (x));
}

/*----------------------------------------------------------
 * 页表相关定义
 *---------------------------------------------------------*/
/* 使用RISC-V的Sv39页表方案 */
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

/* 监管模式地址转换和保护寄存器(satp) - 保存页表地址 */
static inline void 
w_satp(uint64 x)
{
  asm volatile("csrw satp, %0" : : "r" (x));
}

/* 读取监管模式地址转换和保护寄存器 */
static inline uint64
r_satp()
{
  uint64 x;
  asm volatile("csrr %0, satp" : "=r" (x) );
  return x;
}

/*----------------------------------------------------------
 * 陷阱处理相关寄存器
 *---------------------------------------------------------*/
/* 监管模式陷阱原因寄存器(scause) */
static inline uint64
r_scause()
{
  uint64 x;
  asm volatile("csrr %0, scause" : "=r" (x) );
  return x;
}

/* 监管模式陷阱值寄存器(stval) - 保存陷阱相关信息 */
static inline uint64
r_stval()
{
  uint64 x;
  asm volatile("csrr %0, stval" : "=r" (x) );
  return x;
}

/*----------------------------------------------------------
 * 计数器相关寄存器
 *---------------------------------------------------------*/
/* 机器模式计数器使能寄存器(mcounteren) */
static inline void 
w_mcounteren(uint64 x)
{
  asm volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline uint64
r_mcounteren()
{
  uint64 x;
  asm volatile("csrr %0, mcounteren" : "=r" (x) );
  return x;
}

/* 机器模式周期计数器(time) */
static inline uint64
r_time()
{
  uint64 x;
  asm volatile("csrr %0, time" : "=r" (x) );
  return x;
}

/*----------------------------------------------------------
 * 中断控制函数
 *---------------------------------------------------------*/
/* 启用设备中断 */
static inline void
intr_on()
{
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

/* 禁用设备中断 */
static inline void
intr_off()
{
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

/* 检查设备中断是否启用 */
static inline int
intr_get()
{
  uint64 x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

/*----------------------------------------------------------
 * 通用寄存器操作
 *---------------------------------------------------------*/
/* 读取栈指针寄存器(sp) */
static inline uint64
r_sp()
{
  uint64 x;
  asm volatile("mv %0, sp" : "=r" (x) );
  return x;
}

/* 线程指针寄存器(tp)操作 - xv6用它保存当前核心的hartid(核心编号)，即cpus[]数组的索引 */
static inline uint64
r_tp()
{
  uint64 x;
  asm volatile("mv %0, tp" : "=r" (x) );
  return x;
}

static inline void 
w_tp(uint64 x)
{
  asm volatile("mv tp, %0" : : "r" (x));
}

/* 读取返回地址寄存器(ra) - 保存函数返回地址 */
static inline uint64
r_ra()
{
  uint64 x;
  asm volatile("mv %0, ra" : "=r" (x) );
  return x;
}

/*----------------------------------------------------------
 * TLB操作
 *---------------------------------------------------------*/
/* 刷新TLB(转换后备缓冲器) */
static inline void
sfence_vma()
{
  // 参数zero, zero表示刷新所有TLB条目
  asm volatile("sfence.vma zero, zero");
}

/*----------------------------------------------------------
 * 页表类型定义
 *---------------------------------------------------------*/
/* 页表项类型定义(pte_t) - 基于uint64类型 */ 
typedef uint64 pte_t;
/* 页表指针类型定义(pagetable_t) - 指向512个页表项的指针 */
typedef uint64 *pagetable_t; // 512 PTEs

#endif // __ASSEMBLER__

/*----------------------------------------------------------
 * 页表相关常量
 *---------------------------------------------------------*/
#define PGSIZE 4096 // 每个页面的大小为4KB
#define PGSHIFT 12  // 页内偏移量(12位)

/* 页面对齐宏 */
/* 向上对齐(地址或大小到下一页边界):
 * 1.对地址：返回 ≥sz 的第一个页起始地址（跳过不完整的部分页）
 * 2.对大小：返回 ≥sz 的最小页整数倍（确保完全覆盖所需内存）
 */
#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1)) 

/* 向下对齐(地址或大小到当前页边界):
 * 1.对地址：返回 ≤a 的当前页起始地址
 * 2.对大小：返回 ≤a 的最大页整数倍
 */
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1)) 

/* 页表项标志位定义 */
#define PTE_V (1L << 0) // 有效位(valid)
#define PTE_R (1L << 1) // 可读页面(Readable)
#define PTE_W (1L << 2) // 可写页面(Writeable)
#define PTE_X (1L << 3) // 可执行页面(executable)
#define PTE_U (1L << 4) // 用户模式可以访问(user-accessible)
#define PTE_A (1L << 6) // 处理器访问过(accessed)

/*----------------------------------------------------------
 * 页表项转换宏
 *---------------------------------------------------------*/
/* 将物理地址转为页表项：
 * 1. 右移12位获取物理页号(PPN)
 * 2. 左移10位按Sv39格式放置到PTE的[53:10]位
 */
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10) 

/* 将页表项转为物理地址：
 * 1. 右移10位获取PPN
 * 2. 左移12位恢复为物理地址
 */
#define PTE2PA(pte) (((pte) >> 10) << 12) 

/* 提取PTE的低10位标志位 */
#define PTE_FLAGS(pte) ((pte) & 0x3FF) 

/*----------------------------------------------------------
 * 页表索引相关宏
 *---------------------------------------------------------*/
#define PXMASK 0x1FF    // 页表索引掩码(9 bits)
#define PXSHIFT(level) (PGSHIFT+(9*(level))) // 计算指定层级索引的起始位

/* 从虚拟地址va中提取指定level的9位页表索引
 * level=0: VPN[2] (根页表索引)
 * level=1: VPN[1] 
 * level=2: VPN[0] (最后一级页表索引)
 */
#define PX(level, va) ((((uint64)(va)) >> PXSHIFT(level)) & PXMASK)

/*----------------------------------------------------------
 * 虚拟地址空间限制
 *---------------------------------------------------------*/
/* MAXVA是最高可能的虚拟地址加1
 * 实际上比Sv39允许的最大值少1位，以避免对有高位设置的虚拟地址进行符号扩展
 *
 * MAXVA表示虚拟地址空间的最高有效地址，十六进制表示为0x40 0000 0000
 * xv6内核的地址空间和进程空间都是使用低位的虚拟地址空间[0x0, 0x0000 003F FFFF FFFF]
 */
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
