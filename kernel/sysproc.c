#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Change Process priority
extern int chpr(int, int);
extern int chtickets(int, int);
extern int wait2(uint64, uint64, uint64);

uint64
sys_chpr(void)
{
  int pid, pr;
  argint(0, &pid);
  argint(1, &pr);

  return chpr(pid, pr);
}

uint64
sys_yield(void) {
  yield();
  return 0;
}

uint64
sys_chtickets(void)
{
  int pid, tickets;
  argint(0, &pid);
  argint(1, &tickets);

  return chtickets(pid, tickets);
}

uint64
sys_getppid(void)
{
  return myproc()->parent->pid;
}

// copy elements from the kernel ptable to the user space
extern struct proc * getptable_proc(void);

uint64
sys_getptable(void){
  int size;
  uint64 buf_addr;
  char *buf;
  char *s;
  struct proc *p = '\0';

  argint(0, &size);
  argaddr(1, &buf_addr);
  buf = (char*)buf_addr;

  s = buf;
  p = getptable_proc();

  while(buf + size > s && p->state != UNUSED){
    *(int *)s = p->state;
    s+=4;
    *(int *)s = p->pid;
    s+=4;
    *(int *)s = p->parent->pid;
    s+=4;
    *(int *)s = p->priority;
    s+=4;
    *(int *)s = p->tickets;
    s+=4;
    *(int *)s = p->ctime;
    s+=4;
    memmove(s,p->name,16);
    s+=16;
    p++;
  }
  return 0;
}

uint64
sys_wait2(void) {
  uint64 retime_addr, rutime_addr, stime_addr;
  
  argaddr(0, &retime_addr);
  argaddr(1, &rutime_addr);
  argaddr(2, &stime_addr);
  
  return wait2(retime_addr, rutime_addr, stime_addr);
}
