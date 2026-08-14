#ifndef _CLOCK_H_
#define _CLOCK_H_

#include "types.h"
/* 
 * Define how many timer interrupts you want per second.
 * 100 HZ = 10ms per tick (standard for xv6/Linux).
 */
#define HZ 100

uint64 timer_freq;
uint64 timer_interval;

/*
 * Raw hardware clock ticks since boot.
 */
static inline uint64 clock_ticks(void);

/*
 * Current CPU cycles.
 */
static inline uint64 cpu_cycles(void);

/*
 * Clock frequency in Hz.
 */
static inline uint64 clock_frequency(void);

/*
 * Convert ticks to nanoseconds.
 */
uint64 clock_now_ns(void);

#endif