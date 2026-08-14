#include "defs.h"
#include "platform.h"
#include "smoldtb.h"
#include "types.h"
struct platform platform;

static void platform_read_cpus(void);
static void platform_read_memory(void);
static void platform_read_timer(void);
static void platform_read_devices(void);


void
platform_init(void)
{
    memset(&platform,0,sizeof(platform));


    platform_read_cpus();

    platform_read_memory();

    platform_read_timer();

    platform_read_devices();
}

struct reg_list
{
    int count;
    reg_region region[MAX_REGIONS];
};
/* 
 * Reconstructs a 64-bit integer from 'n' native 32-bit cells.
 * Since the library already handled the endianness, we just shift and OR.
 */
static int
get_cells(
    dtb_node *node,
    const char *name,
    int default_value
)
{

    dtb_prop *prop;


    prop = dtb_find_prop(node,name);


    if(prop == 0)
        return default_value;



    uint32 value;


    dtb_read_prop_1(
        prop,
        1,
        &value
    );


    return value;
}
static int
platform_get_reg_layout(dtb_node *node,
                        int *addr_cells,
                        int *size_cells)
{
    dtb_node *parent;

    if (node == NULL ||
        addr_cells == NULL ||
        size_cells == NULL)
        return -1;

    parent = dtb_get_parent(node);

    if (parent == NULL) {
        *addr_cells = 2;
        *size_cells = 1;
        return 0;
    }

    *addr_cells = get_cells(parent,
                            "#address-cells",
                            2);

    *size_cells = get_cells(parent,
                            "#size-cells",
                            1);

    if (*addr_cells <= 0 || *addr_cells > 2)
        return -1;

    if (*size_cells <= 0 || *size_cells > 2)
        return -1;

    return 0;
}

static uint64
read_native_int(const uint32 **data, int cells)
{
    uint64 val = 0;
    for (int i = 0; i < cells; i++) {
        val = (val << 32) | **data;
        (*data)++;
    }
    return val;
}

/*
 * Translates an address and determines the maximum contiguous physical 
 * size available through this translation path.
 */
static int
platform_translate_address(dtb_node *node, uint64 in_addr, uint64 *out_addr, uint64 *max_size)
{
    dtb_node *parent;
    uint64 current_addr = in_addr;
    uint64 allowed_size = (uint64)-1; /* Start with unlimited size */

    parent = dtb_get_parent(node);

    while (parent != NULL) {
        dtb_prop *ranges = dtb_find_prop(parent, "ranges");

        if (ranges == NULL) {
            return -1;
        }

        if (ranges->len == 0) {
            /* 1:1 identity mapping, size is unimpeded. Move up. */
            node = parent;
            parent = dtb_get_parent(parent);
            continue;
        }

        dtb_node *grandparent = dtb_get_parent(parent);

        int child_addr_cells  = get_cells(parent, "#address-cells", 2);
        int size_cells        = get_cells(parent, "#size-cells", 1);
        int parent_addr_cells = grandparent ? get_cells(grandparent, "#address-cells", 2) : 2;

        int tuple_cells = child_addr_cells + parent_addr_cells + size_cells;
        
        if (tuple_cells == 0) return -1;

        int total_cells = ranges->len / sizeof(uint32);
        if (total_cells % tuple_cells != 0) return -1; 
        if (total_cells > MAX_RANGES_CELLS) return -1;

        uint32 ranges_data[MAX_RANGES_CELLS];
        
        if (dtb_read_prop_1(ranges, total_cells, ranges_data) != total_cells) {
            return -1;
        }

        int num_ranges = total_cells / tuple_cells;
        const uint32 *cells_ptr = ranges_data;
        
        int matched = 0;
        uint64 translated = 0;

        for (int r = 0; r < num_ranges; r++) {
            uint64 c_addr     = read_native_int(&cells_ptr, child_addr_cells);
            uint64 p_addr     = read_native_int(&cells_ptr, parent_addr_cells);
            uint64 range_size = read_native_int(&cells_ptr, size_cells);

            if (current_addr >= c_addr && current_addr < (c_addr + range_size)) {
                translated = p_addr + (current_addr - c_addr);
                
                /* Calculate how much contiguous space is left in this specific range window */
                uint64 available_in_range = (c_addr + range_size) - current_addr;
                
                /* Shrink the allowed size if this bus limits it further */
                if (available_in_range < allowed_size) {
                    allowed_size = available_in_range;
                }
                
                matched = 1;
                break;
            }
        }

        if (!matched) {
            return -1;
        }

        current_addr = translated;
        node = parent;
        parent = grandparent;
    }

    *out_addr = current_addr;
    if (max_size) {
        *max_size = allowed_size;
    }
    return 0;
}

int
platform_read_reg(dtb_node *node,
                  struct reg_list *regs)
{
    dtb_prop *prop;
    dtb_pair layout;
    dtb_pair values[MAX_REGIONS];
    size_t dtb_count;

    if (node == NULL || regs == NULL)
        return -1;

    memset(regs, 0, sizeof(*regs));

    prop = dtb_find_prop(node, "reg");
    if (prop == NULL) return -1;

    if (platform_get_reg_layout(node, &layout.a, &layout.b) != 0)
        return -1;

    dtb_count = dtb_read_prop_2(prop, layout, NULL);
    if (dtb_count == 0) return -1;

    if (dtb_count > MAX_REGIONS)
        dtb_count = MAX_REGIONS;

    if (dtb_read_prop_2(prop, layout, values) != dtb_count)
        return -1;

    regs->count = 0;

    /* Loop through the logical regions defined in the DTB */
    for (size_t i = 0; i < dtb_count; i++) {
        uint64 raw_base = values[i].a;
        uint64 remaining_size = values[i].b;

        /* 
         * Translate and chunk the region. If the logical region crosses 
         * a physical bus mapping boundary, it will be split into multiple 
         * physical windows.
         */
        while (remaining_size > 0) {
            if (regs->count >= MAX_REGIONS) {
                /* Array full, prevent overflow */
                break; 
            }

            uint64 phys_base;
            uint64 max_chunk = (uint64)-1;

            if (platform_translate_address(node, raw_base, &phys_base, &max_chunk) == 0) {
                /* Take either the rest of the region, or the max allowed by the bus window */
                uint64 chunk_size = (remaining_size < max_chunk) ? remaining_size : max_chunk;
                
                regs->region[regs->count].base = phys_base;
                regs->region[regs->count].size = chunk_size;
                regs->count++;

                /* Advance the pointers to process the remaining part of the region */
                raw_base += chunk_size;
                remaining_size -= chunk_size;
            } else {
                /* 
                 * Fallback: If translation fails or isn't defined, 
                 * use a 1:1 flat mapping for the remaining size. 
                 */
                regs->region[regs->count].base = raw_base;
                regs->region[regs->count].size = remaining_size;
                regs->count++;
                break;
            }
        }
    }

    return regs->count;
}

static void
platform_read_timer(void)
{
    dtb_node *cpus;
    dtb_prop *prop;
    uint32 freq;

    /* Default to "unknown". */
    platform.timer.frequency = 0;
    platform.timer.valid = 0;

    cpus = dtb_find("/cpus");
    if (cpus == NULL)
        return;

    prop = dtb_find_prop(cpus, "timebase-frequency");
    if (prop == NULL)
        return;

    if (dtb_read_prop_1(prop, 1, &freq) != 1)
        return;

    if (freq == 0)
        return;

    platform.timer.frequency = (uint64)freq;
    platform.timer.valid = 1;

    platform.timer.ticks_per_sec = platform.timer.frequency;
    platform.timer.ticks_per_ms  = platform.timer.frequency / 1000;
    platform.timer.ticks_per_us  = platform.timer.frequency / 1000000;
}
static void
platform_read_memory(void)
{
    dtb_node *root = dtb_find("/");
    if (root == NULL)
        return;

    dtb_node *node = dtb_get_child(root);

    /*
     * Index into the tightly packed ref_count[] array.
     *
     * Example:
     *   RAM bank 0: 0x80000000 - 0x81000000 -> pages [0 ... 4095]
     *   RAM bank 1: 0x90000000 - 0x91000000 -> pages [4096 ... 8191]
     *
     * Physical addresses are NOT used as indices.
     */
    int current_index = 0;

    while (node)
    {
        dtb_prop *type = dtb_find_prop(node, "device_type");

        if (type)
        {
            const char *name = dtb_read_string(type, 0);

            if (name && strcmp(name, "memory") == 0)
            {
                struct reg_list reg;
                int count = platform_read_reg(node, &reg);

                if (count > 0)
                {
                    for (int i = 0; i < count; i++)
                    {
                        uint64 base = reg.region[i].base;
                        uint64 size = reg.region[i].size;

                        /* Ignore empty memory regions. */
                        if (size == 0)
                            continue;

                        /* Prevent platform.memory.region[] overflow. */
                        if (platform.memory.count >= MAX_MEMORY_REGIONS)
                            break;

                        /*
                         * Make sure the region represents whole pages.
                         * You can replace this with a panic() if required.
                         */
                        if (base % PGSIZE != 0 || size % PGSIZE != 0)
                            continue;

                        int dest_idx = platform.memory.count;

                        platform.memory.region[dest_idx].base =
                            base;

                        platform.memory.region[dest_idx].size =
                            size;

                        platform.memory.region[dest_idx].type =
                            MEMORY;

                        /*
                         * First page of this physical RAM region in
                         * the packed reference-count array.
                         */
                        platform.memory.region[dest_idx].array_start_index =
                            current_index;

                        /*
                         * Advance by the number of physical pages in
                         * this RAM bank.
                         */
                        current_index += size / PGSIZE;

                        platform.memory.count++;
                    }
                }
            }
        }

        node = dtb_get_sibling(node);
    }

    /*
     * current_index is now the total number of RAM pages across
     * all non-contiguous memory regions.
     *
     * Example:
     *
     * region[0]:
     *   base              = 0x80000000
     *   size              = 16 MB
     *   array_start_index = 0
     *
     * region[1]:
     *   base              = 0x90000000
     *   size              = 32 MB
     *   array_start_index = 4096
     *
     * Total packed ref-count entries = 12288 pages.
     */

    /*
     * Optional:
     *
     * if (current_index > MAX_RAM_PAGES)
     *     panic("too much physical RAM");
     */
}

static void
platform_read_reserved_memory(void)
{
    dtb_node *reserved;

    reserved = dtb_find("/reserved-memory");

    if (reserved == NULL)
        return;

    for (dtb_node *node = dtb_get_child(reserved);
         node != NULL;
         node = dtb_get_sibling(node))
    {
        /* Ignore disabled reserved-memory nodes */
        dtb_prop *status = dtb_find_prop(node, "status");
        if (status)
        {
            const char *status_str = dtb_read_string(status, 0);
            if (status_str && strcmp(status_str, "okay") != 0)
                continue;
        }

        struct reg_list regs;

        int count = platform_read_reg(node, &regs);

        if (count <= 0)
            continue;

        bool no_map = (dtb_find_prop(node, "no-map") != NULL);
        bool reusable = (dtb_find_prop(node, "reusable") != NULL);

        dtb_prop *compat = dtb_find_prop(node, "compatible");
        const char *compat_string = "";
        if (compat)
        {
            const char *parsed = dtb_read_string(compat, 0);
            if (parsed)
                compat_string = parsed;
        }

        const char *name = dtb_node_name(node);
        if (!name)
            name = "unknown";

        for (int i = 0; i < count; i++)
        {
            if (regs.region[i].size == 0)
                continue;

            if (platform.reserved.count >= MAX_RESERVED_REGIONS)
            {
                panic("reserved-memory table full");
            }

            struct region *dst =
                &platform.reserved.region[platform.reserved.count];

            memset(dst, 0, sizeof(*dst));

            dst->base = regs.region[i].base;
            dst->size = regs.region[i].size;

            dst->no_map = no_map;
            dst->reusable = reusable;

            strncpy(dst->name,
                    name,
                    sizeof(dst->name) - 1);

            strncpy(dst->compatible,
                    compat_string,
                    sizeof(dst->compatible) - 1);

            platform.reserved.count++;
        }
    }
}

static int
device_is_enabled(dtb_node *node)
{
    dtb_prop *status;

    status = dtb_find_prop(node, "status");

    /* Missing status means enabled */
    if (status == NULL)
        return 1;

    const char *str = dtb_read_string(status, 0);

    if (str == NULL)
        return 0;

    if (!strcmp(str, "okay"))
        return 1;

    if (!strcmp(str, "ok"))
        return 1;

    return 0;
}
static void
add_device(enum device_type type,
           dtb_node *node,
           int map)
{
    struct reg_list regs;
    int reg_count;

    if (!device_is_enabled(node))
        return;

    reg_count = platform_read_reg(node, &regs);

    if (reg_count <= 0)
        return;

    if (platform.devices.count >= MAX_DEVICES) {
        /* 
         * Warning: If this panics before the UART is initialized, 
         * you will get a silent hang. 
         */
        panic("device table full");
    }

    struct device_info *dev = 
        &platform.devices.device[platform.devices.count];

    memset(dev, 0, sizeof(*dev));

    dev->type = type;
    dev->map_kernel = map;
    dev->node = node;

    const char *name = dtb_node_name(node);
    if (name)
        strlcpy(dev->name,
                name,
                sizeof(dev->name));

    dtb_prop *compat = dtb_find_prop(node, "compatible");

    if (compat) {
        const char *str = dtb_read_string(compat, 0);
        if (str)
            strlcpy(dev->compatible,
                    str,
                    sizeof(dev->compatible));
    }

    if (reg_count > MAX_DEVICE_REGIONS)
        reg_count = MAX_DEVICE_REGIONS;

    /* Pack valid regions tightly to avoid array holes */
    int valid_regions = 0;
    for (int i = 0; i < reg_count; i++) {
        if (regs.region[i].size == 0)
            continue;
            
        dev->region[valid_regions] = regs.region[i];
        valid_regions++;
    }

    /* If no valid regions were found after filtering, abort registration */
    if (valid_regions == 0) {
        return; 
    }

    dev->region_count = valid_regions;
    
    /* Only increment count now that we know the device is fully valid */
    platform.devices.count++;
}

struct early_device_id
{
    const char *compatible;
    enum device_type type;
};

static const struct early_device_id target_devices[] =
{
    { "ns16550a", DEV_UART },
    { "sifive,uart0", DEV_UART },

    { "riscv,plic0", DEV_PLIC },
    { "riscv,clint0", DEV_CLINT },

    { "pci-host-ecam-generic", DEV_PCIE },
    { "snps,dw-mshc", DEV_SD },

    { NULL, 0 }
};
static void
platform_read_devices(void)
{
    dtb_node *node;

    for (int i = 0; target_devices[i].compatible != NULL; i++) {

        node = dtb_find_compatible(NULL,
                                   target_devices[i].compatible);

        while (node != NULL) {

            add_device(target_devices[i].type,
                       node,
                       1);

            node = dtb_find_compatible(node,
                                       target_devices[i].compatible);
        }
    }
}
void
platform_read_cpus(void)
{
    dtb_node *cpus;
    dtb_node *cpu;

    platform.cpus.count = 0;

    cpus = dtb_find("/cpus");
    if (cpus == NULL)
        panic("platform: missing /cpus node");

    cpu = dtb_get_child(cpus);

    while (cpu != NULL) {

        dtb_prop *prop;
        const char *str;

        /* Must be a CPU node. */
        prop = dtb_find_prop(cpu, "device_type");
        str = prop ? dtb_read_string(prop, 0) : NULL;

        if (str == NULL || strcmp(str, "cpu") != 0) {
            cpu = dtb_get_sibling(cpu);
            continue;
        }

        /* Skip disabled CPUs. */
        prop = dtb_find_prop(cpu, "status");
        str = prop ? dtb_read_string(prop, 0) : NULL;

        if (str &&
            strcmp(str, "okay") != 0 &&
            strcmp(str, "ok") != 0) {
            cpu = dtb_get_sibling(cpu);
            continue;
        }

        if (platform.cpus.count >= NCPU)
            panic("platform: too many CPUs");

        struct cpu_info *info =
            &platform.cpus.cpu[platform.cpus.count];

        memset(info, 0, sizeof(*info));

        /* Read hart ID. */
        prop = dtb_find_prop(cpu, "reg");
        if (prop == NULL)
            panic("platform: CPU missing reg");

        if (dtb_read_prop_values(prop, 1, &info->hartid) != 1)
            panic("platform: invalid CPU reg");

        info->present = 1;
        info->enabled = 1;
        info->node = cpu;

        platform.cpus.count++;

        cpu = dtb_get_sibling(cpu);
    }

    if (platform.cpus.count == 0)
        panic("platform: no usable CPUs");
}
int
pa_to_index(uint64 pa)
{
  for(int i = 0; i < platform.memory.count; i++){
    struct reg_region *r = &platform.memory.region[i];

    if(r->type != MEMORY)
      continue;

    if(pa >= r->base &&
       pa - r->base < r->size){

      if(pa % PGSIZE != 0)
        return -1;

      return r->array_start_index +
             (pa - r->base) / PGSIZE;
    }
  }

  return -1;
}
static void
ref_counts_init(void)
{
    int total_pages = 0;

    for (int i = 0; i < platform.memory.count; i++) {
        struct reg_region *r = &platform.memory.region[i];

        if (r->type != MEMORY)
            continue;

        total_pages += r->size / PGSIZE;
    }

    if (total_pages == 0)
        panic("ref_counts_init: no RAM");

    ref_counts.npages = total_pages;

    ref_counts.count_pages =
        (total_pages + REFCOUNTS_PER_PAGE - 1)
        / REFCOUNTS_PER_PAGE;

    if (ref_counts.count_pages > REF_COUNT_MAX_PAGES)
        panic("ref_counts_init: too many pages");

    for (int i = 0; i < ref_counts.count_pages; i++) {
        ref_counts.pages[i] = (uint64 *)kalloc();

        if (ref_counts.pages[i] == 0)
            panic("ref_counts_init: kalloc failed");

        /*
         * kalloc() fills the page with 5, so initialize
         * all reference counters to zero.
         */
        memset(ref_counts.pages[i], 0, PGSIZE);
    }

    initlock(&ref_counts.lock, "ref_counts");
}
static inline uint64 *
ref_count_ptr(int index)
{
    if (index < 0 || index >= ref_counts.npages)
        panic("ref_count_ptr: invalid index");

    int page = index / REFCOUNTS_PER_PAGE;
    int offset = index % REFCOUNTS_PER_PAGE;

    return &ref_counts.pages[page][offset];
}
static uint64 *
kref_ptr(int index)
{
  if(index < 0 || index >= ref_counts.npages)
    panic("kref_ptr");

  uint64 page = index / REFCOUNTS_PER_PAGE;
  uint64 off  = index % REFCOUNTS_PER_PAGE;

  return &ref_counts.pages[page][off];
}
static uint64
kref_get(uint64 pa)
{
  int index = pa_to_index(pa);

  if(index < 0)
    panic("kref_get: invalid pa");

  acquire(&ref_counts.lock);

  uint64 n = *kref_ptr(index);

  release(&ref_counts.lock);

  return n;
}
static void
kref_increment(uint64 pa)
{
  int index = pa_to_index(pa);

  if(index < 0)
    panic("kref_increment: invalid pa");

  acquire(&ref_counts.lock);

  uint64 *count = kref_ptr(index);

  (*count)++;

  release(&ref_counts.lock);
}
int
kref_decrement(uint64 pa)
{
  int index = pa_to_index(pa);

  if(index < 0)
    panic("kref_decrement: invalid pa");

  acquire(&ref_counts.lock);

  uint64 *count = kref_ptr(index);

  if(*count == 0)
    panic("kref_decrement: underflow");

  (*count)--;

  int free = (*count == 0);

  release(&ref_counts.lock);

  return free;
}
void
kref_set(uint64 pa, uint64 value)
{
  int index = pa_to_index(pa);

  if(index < 0)
    panic("kref_set: invalid pa");

  acquire(&ref_counts.lock);
  *kref_ptr(index) = value;
  release(&ref_counts.lock);
}
int
cow_break(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;
  uint flags;

  if(va % PGSIZE != 0)
    return -1;

  pte = walk(pagetable, va, 0);

  if(pte == 0)
    return -1;

  if((*pte & PTE_V) == 0)
    return -1;

  if((*pte & PTE_COW) == 0)
    return -1;

  pa = PTE2PA(*pte);

  /*
   * If this is the last reference, nobody else can be using
   * the page. We can simply turn this mapping back into writable.
   */
  if(kref_get(pa) == 1){
    flags = PTE_FLAGS(*pte);

    flags |= PTE_W;
    flags &= ~PTE_COW;

    *pte = PA2PTE(pa) | flags;

    sfence_vma();

    return 0;
  }

  /*
   * Shared page: create a private copy.
   */
  char *mem = kalloc();

  if(mem == 0)
    return -1;

  memmove(mem, (void *)pa, PGSIZE);

  flags = PTE_FLAGS(*pte);

  flags |= PTE_W;
  flags &= ~PTE_COW;

  *pte = PA2PTE((uint64)mem) | flags;

  /*
   * The new page already has refcount = 1 because kalloc()
   * established it.
   *
   * This process no longer references the old page.
   */
  kref_decrement(pa);

  sfence_vma();

  return 0;
}