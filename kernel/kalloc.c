// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

// <用于访问物理页引用计数数组>
// <物理页引用计数加1只会在fork中执行，单纯的映射并不会加1>
#define PA2PGREF_ID(P) (((P)-KERNBASE)/PGSIZE)
#define PGREF_MAX_ENTRIES PA2PGREF_ID(PHYSTOP)
struct spinlock pgreflock;  // 用于pgref数组的锁，防止竞争条件引起内存泄露
int pageref[PGREF_MAX_ENTRIES];
// <通过物理地址获得引用计数>
// 这几个宏有点东西，要会熟悉地址加减操作
#define PA2PGREF(P) pageref[PA2PGREF_ID((uint64)(P))]


void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  
  initlock(&kmem.lock, "kmem");
  // <初始化锁>
  initlock(&pgreflock, "pgref");

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

  // <lab>
  acquire(&pgreflock);
  if (--PA2PGREF(pa) <= 0)
  { // <当页面引用计数小于等于0的时候，释放>
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    r = (struct run*)pa;
    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  release(&pgreflock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
    PA2PGREF(r) = 1;  // <新分配的物理页ref为1，不需要加锁>
  }
  return (void*)r;
}


// <如果物理页面ref大于1个，那么该物理页ref减1>
// <然后拷贝给一个新的物理页，新物理页ref=1>
// <如果引用小于等于1，不创建和复制新的物理页，直接返回该页本身>
void *kcopy_n_deref(void *pa)
{
  acquire(&pgreflock);
  if (PA2PGREF(pa) <= 1)
  {
    release(&pgreflock);
    return pa;
  }
  // <分配新的内存页，并复制旧页中的数据到新页
  uint64 newpa;
  if( (newpa = (uint64)kalloc()) == 0 )
  {
    release(&pgreflock);
    return 0; // <超出内存>
  }
  memmove((void*)newpa, (void*)pa, PGSIZE);

  // <旧的页ref-1>
  --PA2PGREF(pa);

  release(&pgreflock);
  return (void*)newpa;
}

// <pa的引用计数加1>
void krefpage(void *pa)
{
  acquire(&pgreflock);
  ++PA2PGREF(pa);
  release(&pgreflock);
}