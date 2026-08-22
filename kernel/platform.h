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
struct irq_resource
{
    uint32 irq;
    uint32 flags;

    struct irq_controller *controller;
};

struct gpio_resource
{
    uint32 pin;
    uint32 flags;

    struct gpio_controller *controller;
};

struct clock_resource
{
    uint32 id;
    void *provider;
};

struct reset_resource
{
    uint32 id;
    void *provider;
};

struct dma_resource
{
    uint32 channel;
    void *controller;
};


struct device_info
{
    /* Device identity */
    enum device_type type;

    dtb_node *node;

    char name[64];
    char compatible[64];

    /* Device mapping */
    int map_kernel;

    /* MMIO resources */
    int region_count;
    struct region region[MAX_DEVICE_REGIONS];

    /* Interrupt resources */
    int irq_count;
    struct irq_resource irq[MAX_INTERRUPTS];

    /* Clock resources */
    int clock_count;
    struct clock_resource clock[MAX_CLOCKS];

    /* Reset resources */
    int reset_count;
    struct reset_resource reset[MAX_RESETS];

    /* DMA resources */
    int dma_count;
    struct dma_resource dma[MAX_DMA_CHANNELS];

    /* GPIO resources */
    int gpio_count;
    struct gpio_resource gpio[MAX_GPIO];

    /* Driver private data */
    void *driver_data;
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
void device_iomap()
void device_get_irq()
void device_request_irq()
void device_clock_enable()
void device_clock_get_rate()
void device_reset_assert()
void device_reset_deassert()
void device_dma_request()
void device_get_gpio()
void device_property_read_u32()
void platform_read_reg()
void platform_read_interrupts()
void platform_read_clocks()
void platform_read_resets()
void platform_read_dmas()
void platform_read_gpios()
void kref_set(uint64 pa, uint64 value);
int kref_decrement(uint64 pa);
int cow_break(pagetable_t pagetable, uint64 va);
#endif