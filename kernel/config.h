#ifndef _CONFIG_H_
#define _CONFIG_H_

// we use two HART (cpu) in challenge3
#define NCPU 2

//interval of timer interrupt. added @lab1_3
#define TIMER_INTERVAL 1000000

#define DRAM_BASE 0x80000000

#define USER_BASE 0x81000000
#define USER_SIZE 0x4000000
#define STACK_SIZE 0x100000

/* we use fixed physical (also logical) addresses for the stacks and trap frames as in
 Bare memory-mapping mode */
// user stack top
#define USER_STACK(x) (USER_BASE + USER_SIZE * x + STACK_SIZE)

// the stack used by PKE kernel when a syscall happens
#define USER_KSTACK(x) (USER_BASE + USER_SIZE * x + STACK_SIZE * 2)

// the trap frame used to assemble the user "process"
#define USER_TRAP_FRAME(x) (USER_BASE + USER_SIZE * x + STACK_SIZE * 3)

#endif
