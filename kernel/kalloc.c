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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int n = cpuid();
  pop_off();
  acquire(&kmem[n].lock);
  r->next = kmem[n].freelist;
  kmem[n].freelist = r;
  release(&kmem[n].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int me = cpuid();
  pop_off();

  acquire(&kmem[me].lock);
  r = kmem[me].freelist;
  if(r)
    kmem[me].freelist = r->next;
  release(&kmem[me].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  else {
    // try to steal from other cpu's freelist
    // may lead to dead lock, so be careful
    
    // since null is not included
    struct run * stolen = r;

    for (int i = 0; i < NCPU; i++) {
      if (i != me) {
        acquire(&kmem[i].lock);
        if (kmem[i].freelist) {
          // steal half, to do so, use quick-slow pointers
          struct run * quick = kmem[i].freelist;
          struct run * slow = kmem[i].freelist;
          while (quick->next && quick->next->next) {
            quick = quick->next->next;
            slow = slow->next;
          }
          stolen = slow->next;
          slow->next = r;
        }
        release(&kmem[i].lock);
        if (stolen)
          break;
      }
    }

    if (stolen) {
      acquire(&kmem[me].lock);
      kmem[me].freelist = stolen;
      r = kmem[me].freelist;
      kmem[me].freelist = r->next;
      release(&kmem[me].lock);
      memset((char*)r, 5, PGSIZE); // fill with junk
    }
  }
  return (void*)r;
}
