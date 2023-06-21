#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "date.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
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
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  
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


  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
extern pte_t * walk(pagetable_t pagetable, uint64 va, int alloc);

int sys_pgaccess(void)
{
  // 声明一个变量接住那一位
  uint64 bitmask = 0;
  
  // 接收用户态传下来的参数
  uint64 startVA;
  int NumberOfPages;
  uint64 bitmaskva;

  // 获取第n个32位系统调用的参数
  if (argint(1, &NumberOfPages) < 0)
    return -1;
  if (NumberOfPages > MAXSCAN)
    return -1;

  // myproc获取当前进程的指针
  if (argaddr(0, &startVA) < 0)
    return -1;
  if (argaddr(2, &bitmaskva) < 0)
    return -1;
  
  int i;
  pte_t *pte;

  // 从起始地址开始，判断PTE_A是否被设置
  for (i = 0; i < NumberOfPages; startVA += PGSIZE, ++i)
  {
      if ((pte = walk(myproc()->pagetable, startVA, 0)) == 0)
        panic("pgaccess : walk failed");
      if (*pte & PTE_A)
      {
        bitmask |= 1 << i;  // 指示第i个页面被访问
        *pte &= ~PTE_A; // 将页表项的PAT_A标志位清0
      }
  }
  // 最后使用copyout将内核态下的BitMask拷贝到用户态
  copyout(myproc()->pagetable, bitmaskva, (char*)&bitmask, sizeof(bitmask));
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
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


