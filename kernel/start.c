#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
// Counter enable bits (mcounteren/scounteren)
#define COUNTEREN_CYCLE    (1UL << 0)
#define COUNTEREN_TIME     (1UL << 1)
#define COUNTEREN_INSTRET  (1UL << 2)

#define COUNTEREN_ALL (COUNTEREN_CYCLE | \
                        COUNTEREN_TIME | \
                        COUNTEREN_INSTRET)
void main();
void timerinit();

// entry.S needs one stack per CPU.
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// entry.S jumps here in machine mode on stack0.
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;
  x |= MSTATUS_MPP_S;
  w_mstatus(x);
  // set M Exception Program Counter to main, for mret.
  // requires gcc -mcmodel=medany
  w_mepc((uint64)main);

  // disable paging for now.
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE);

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  // ask for clock interrupts.
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  int id = r_mhartid();
  w_tp(id);

  // switch to supervisor mode and jump to main().
  asm volatile("mret");
}
// ask each hart to generate timer interrupts.
// Ask each hart to generate timer interrupts.
void
timerinit(void)
{
  // Enable Supervisor-mode timer interrupts.
  w_mie(r_mie() | MIE_STIE);

  // Enable the Sstc extension (Supervisor Timer Compare).
  w_menvcfg(r_menvcfg() | (1UL << 63));

  // Allow Supervisor mode to read cycle, time, and instret.
  w_mcounteren(r_mcounteren() | COUNTEREN_ALL);

  // Allow User mode to read cycle, time, and instret directly.
  w_scounteren(r_scounteren() | COUNTEREN_ALL);

  // Schedule the first timer interrupt.
  w_stimecmp(r_time() + 1000000);
}
// // ask each hart to generate timer interrupts.
// void
// timerinit()
// {
//   // enable supervisor-mode timer interrupts.
//   w_mie(r_mie() | MIE_STIE);
  
//   // enable the sstc extension (i.e. stimecmp).
//   w_menvcfg(r_menvcfg() | (1L << 63)); 
  
//   // allow supervisor to use stimecmp and time.
//   w_mcounteren(r_mcounteren() | 2);
  
//   // ask for the very first timer interrupt.
//   w_stimecmp(r_time() + 1000000);
// }
