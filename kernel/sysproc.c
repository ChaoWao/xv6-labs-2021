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
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  uint64 base;
  int len;
  uint64 mask;
  if (argaddr(0, &base) < 0 || argint(1, &len) < 0 || argaddr(2, &mask) < 0) {
    return -1;
  }

  int MAX_PAGES = 32;
  len = MAX_PAGES > len ? len : MAX_PAGES;
  unsigned int mask_kernel = 0;
  unsigned int page_bit = 1;
  uint64 va = PGROUNDDOWN(base);
  for (int i = 0; i < len; i++) {
    pte_t *pte = walk(myproc()->pagetable, va + i*PGSIZE, 0);
    if (*pte & PTE_A) {
      mask_kernel |= page_bit;
      *pte &= ~PTE_A;
    }
    page_bit <<= 1;
  }
  if (copyout(myproc()->pagetable, mask, (char *)&mask_kernel, (len + 7) >> 3) < 0) {
    return -1;
  }

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
