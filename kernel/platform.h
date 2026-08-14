#include "types.h"
#ifndef PLATFORM_H
#define PLATFORM_H
#define PLATFORM_MAX_MEMORY_REGIONS 8
#define PLATFORM_MAX_DEVICES        64
#define PLATFORM_MAX_CPUS            8
#define MAX_REGIONS 16
#define MAX_RANGES_CELLS 64
#define MAX_RESERVED_REGIONS 128
#define MAX_RESERVED_NAME        64
#define MAX_COMPATIBLE_STRING    64
#define REFCOUNTS_PER_PAGE (PGSIZE / sizeof(uint64))
#define REF_COUNT_MAX_PAGES 10

void kref_set(uint64 pa, uint64 value);
int kref_decrement(uint64 pa);
int
cow_break(pagetable_t pagetable, uint64 va);

struct {
    struct spinlock lock;
    uint64 *pages[REF_COUNT_MAX_PAGES];
    int npages;
    int count_pages;
} ref_counts;

struct timer_info {

    uint64 frequency;

    uint64 ticks_per_us;

    uint64 ticks_per_ms;

    uint64 ticks_per_sec;

    int valid;

};
struct region
{
    uint64 base;
    uint64 size;

    bool no_map;
    bool reusable;

    char name[MAX_RESERVED_NAME];
    char compatible[MAX_COMPATIBLE_STRING];
};

struct reg_region {
    uint64 base;
    uint64 size;
    enum type {
    MMIO,
    IRQ,
    MEMORY
};
    int array_start_index;
};

struct cpu_info {
    uint32 hartid;
};

enum device_type {
    DEV_NONE,
    DEV_UART,
    DEV_PLIC,
    DEV_VIRTIO,
    DEV_TIMER,
    DEV_CLINT,
    DEV_PCIE,
    DEV_SD,
    DEV_CLINT,
};
struct device_info
{
    enum device_type type;

    dtb_node *node;

    char name[64];
    char compatible[64];

    int map_kernel;

    /* MMIO resources */
    int region_count;
    struct region region[MAX_DEVICE_REGIONS];

    /* Interrupt resources */
    int irq_count;
    uint32 irq[MAX_INTERRUPTS];

    /* Clock resources */
    int clock_count;
    uint32 clock[MAX_CLOCKS];

    /* Reset resources */
    int reset_count;
    uint32 reset[MAX_RESETS];

    /* DMA resources */
    int dma_count;
    uint32 dma[MAX_DMA_CHANNELS];
};

struct cpu_info {

    uint32 hartid;

    uint64 frequency;

    const char *isa;

    const char *mmu;

    int online;

    int present;

    dtb_node *node;

};

struct platform {
    struct {
        int count;
        struct reg_region region[PLATFORM_MAX_MEMORY_REGIONS];
    } memory;

    struct {
        int count;
        struct cpu_info cpu[PLATFORM_MAX_CPUS];
    } cpus;

    struct {

        uint64 frequency;

        uint64 ticks_per_us;

        uint64 ticks_per_ms;

        uint64 ticks_per_sec;

        int valid;

    } timer;
    
    struct reserved_memory
        {
            int count;
            struct region region[MAX_RESERVED_REGIONS];
        } reserved;

    struct {
        int count;
        struct device_info device[PLATFORM_MAX_DEVICES];
    } devices;
};

#endif