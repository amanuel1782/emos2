#include "types.h"
#include "riscv.h"
#include "platform.h"
#include "clock.h"

extern struct platform platform;
uint64 timer_freq = 0;
uint64 timer_interval = 0;
/* 
 * timer_init()
 * Called during early boot AFTER platform_read_timer() has parsed the DTB.
 */
void
timer_init(void)
{
    /* 1. Read the frequency provided by the platform parser */
    timer_freq = platform.timer.frequency;

    /* 2. Guard against missing DTB properties (Zero-Divide Trap) */
    if (timer_freq == 0) {
        timer_freq = 10000000; /* Fallback to standard 10 MHz */
    }

    /* 3. Calculate how many CPU cycles occur between each OS tick */
    timer_interval = timer_freq / HZ;
}
static inline uint64
clock_ticks(void)
{
    return r_time();
}

/*
 * Read the CPU cycle counter.
 */
static inline uint64
cpu_cycles(void)
{
    return r_cycle();
}
/*
 * Read the retired instruction counter.
 */
static inline uint64
instructions_retired(void)
{
    return r_instret();
}

static inline uint64
clock_frequency(void)
{
    return timer_freq;
}

void
timer_init(void)
{
    /* 1. Read the frequency provided by the platform parser */
    timer_freq = platform.timer.frequency;

    /* 2. Guard against missing DTB properties (Zero-Divide Trap) */
    if (timer_freq == 0) {
        timer_freq = 10000000; /* Fallback to standard 10 MHz */
    }

    /* 3. Calculate how many CPU cycles occur between each OS tick */
    timer_interval = timer_freq / HZ;
}

/* Converts hardware time ticks to nanoseconds without 64-bit overflow */
uint64
clock_now_ns(void)
{
    uint64 ticks = clock_ticks();

    if (timer_freq == 0)
        return 0;

    uint64 sec = ticks / timer_freq;
    uint64 remainder = ticks % timer_freq;

    return (sec * 1000000000ULL) + ((remainder * 1000000000ULL) / timer_freq);
}