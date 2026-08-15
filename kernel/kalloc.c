// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "platform.h"
void freerange(void *pa_start, void *pa_end);
extern char etext[];  // kernel.ld sets this to end of kernel code.

extern char trampoline[];
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.
extern struct  platform platform;
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

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
// void
// kfree(void *pa)
// {
//   struct run *r;

//   if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
//     panic("kfree");

//   // Fill with junk to catch dangling refs.
//   memset(pa, 1, PGSIZE);

//   r = (struct run*)pa;

//   acquire(&kmem.lock);
//   r->next = kmem.freelist;
//   kmem.freelist = r;
//   release(&kmem.lock);
// }

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

  if(r)
  {
    memset((char*)r, 5, PGSIZE); // fill with junk
    kref_set((uint64)r, 1);
  }
  return (void*)r;
}

void
kfree(void *pa)
{
  struct run *r;
  uint64 a = (uint64)pa;
  /* 
   * Removed PHYSTOP check. We now trust the dynamic DTB boundaries 
   * established in platform_init_physical_pages.
   */
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end)
    panic("kfree");
    
  if(!kref_decrement(a))
    return;
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

static int
is_page_reserved(uint64 page_addr)
{
    uint64 page_end = page_addr + PGSIZE;

    for (int i = 0; i < platform.reserved.count; i++)
    {
        uint64 res_base = platform.reserved.region[i].base;
        uint64 res_end = res_base + platform.reserved.region[i].size;

        /* Check for any overlap between the 4K page and the reserved region */
        if (page_addr < res_end && page_end > res_base)
        {
            return 1;
        }
    }
    return 0;
}

void
platform_init_physical_pages(void)
{
    /* Step 1: Manually add the Kernel footprint to the reserved list */
    if (platform.reserved.count >= MAX_RESERVED_REGIONS) {
        panic("No space to reserve kernel!");
    }
    
    
    struct region *kres = &platform.reserved.region[platform.reserved.count];
    kres->base = (uint64)KERNBASE;
    kres->size = (uint64)end - (uint64)KERNBASE;
    kres->no_map = 0; /* The kernel text/data must be mapped */
    kres->reusable = 0;
    strncpy(kres->name, "kernel-image", sizeof(kres->name) - 1);
    
    platform.reserved.count++;

    /* Step 2: Page-by-page carving */
    for (int i = 0; i < platform.memory.count; i++)
    {
        uint64 mem_base = platform.memory.region[i].base;
        uint64 mem_size = platform.memory.region[i].size;

        /* Strictly align boundaries to prevent unaligned page faults */
        uint64 start_page = PGROUNDUP(mem_base);
        uint64 end_page = PGROUNDDOWN(mem_base + mem_size);

        for (uint64 p = start_page; p < end_page; p += PGSIZE)
        {
            /* If the page is clean, hand it to your physical allocator */
            if (!is_page_reserved(p))
            {
                kfree((void*)p);
            }
        }
    }
}