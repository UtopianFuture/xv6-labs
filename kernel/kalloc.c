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
} kmem;

struct COUNT{
  uint count[(PHYSTOP - KERNBASE) / PGSIZE];
  struct spinlock lock;
}count;

// uint rcount(uint64 pa, char func, int n)
// {
//   print();
//   acquire(&count.lock);
//   uint64 idx = (pa - KERNBASE) / PGSIZE;
//   if(func == '+')
//     count.count[idx]++;
//   else if(func == '-')
//     count.count[idx]--;
//   else if(func == 'n')
//     count.count[idx] = n;
//   else if(func == 'v'){
//     release(&count.lock);
//    return count.count[idx]; // get value
//   }else{
//     printf("wrong func: %c", func);
//   }
//   release(&count.lock);
//   return 0;
// }

uint64 index(uint64 pa){
  return (pa - KERNBASE) / PGSIZE;
}

void lock(){
  acquire(&count.lock);
}

void unlock(){
  release(&count.lock);
}

void set(uint64 pa, int n){
  count.count[index(pa)] = n;
}

uint get(uint64 pa){
  return count.count[index(pa)];
}

void increase(uint64 pa, int n){
  // lock();
  count.count[index(pa)] += n;
  // unlock();
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
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

  // lock();
  if(get((uint64)pa) > 1){
    count.count[index((uint64)pa)] -= 1;
    // increase((uint64)pa, -1); // still have processes use this page
    // unlock();
    return;
  }

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  set((uint64)pa, 0);
  // unlock();
  // printf("kfree: rcount: %d\n", rcount((uint64)pa, 'v', 0));
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
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

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    set((uint64)r, 1); // set the page count to 1
  }
  return (void*)r;
}
